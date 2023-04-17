#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "matching.h"
#include "unicode.h"
#include "xmalloc.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int32_t simple_match_words(
		const char *restrict patterns,
		const char *restrict str);

static int32_t prefix_match_words(
		const char *restrict patterns,
		const char *restrict str);

static int32_t fuzzy_match_words(
		const char *restrict patterns,
		const char *restrict str);

static int32_t fuzzy_match(
		const char *restrict pattern,
		const char *restrict str);

static int32_t fuzzy_match_recurse(
		const char *restrict pattern,
		const char *restrict str,
		int32_t score,
		bool first_match_only,
		bool first_char);

static int32_t compute_score(
		int32_t jump,
		bool first_char,
		const char *restrict match);

/*
 * Select the appropriate algorithm, and return its score.
 * Each algorithm returns larger scores for better matches,
 * and returns INT32_MIN if a word is not found.
 */
int32_t match_words(
		enum matching_algorithm algorithm,
		const char *restrict patterns,
		const char *restrict str)
{
	switch (algorithm) {
		case MATCHING_ALGORITHM_NORMAL:
			return simple_match_words(patterns, str);
		case MATCHING_ALGORITHM_PREFIX:
			return prefix_match_words(patterns, str);
		case MATCHING_ALGORITHM_FUZZY:
			return fuzzy_match_words(patterns, str);
		default:
			return INT32_MIN;
	}
}

/*
 * Split patterns into words, and perform simple matching against str for each.
 * Returns the negative sum of substring distances from the start of str.
 * If a word is not found, returns INT32_MIN.
 */
int32_t simple_match_words(const char *restrict patterns, const char *restrict str)
{
	int32_t score = 0;
	char *saveptr = NULL;
	char *tmp = utf8_normalize(patterns);
	char *pattern = strtok_r(tmp, " ", &saveptr);
	while (pattern != NULL) {
		char *c = utf8_strcasestr(str, pattern);
		if (c == NULL) {
			score = INT32_MIN;
			break;
		} else {
			score -= c - str;
		}
		pattern = strtok_r(NULL, " ", &saveptr);
	}
	free(tmp);
	return score;
}

/*
 * Split patterns into words, and perform prefix matching against str for each.
 * Returns the negative sum of remaining string suffix lengths.
 * If a word is not found, returns INT32_MIN.
 */
int32_t prefix_match_words(const char *restrict patterns, const char *restrict str)
{
	int32_t score = 0;
	char *saveptr = NULL;
	char *tmp = utf8_normalize(patterns);
	char *pattern = strtok_r(tmp, " ", &saveptr);
	while (pattern != NULL) {
		char *c = utf8_strcasestr(str, pattern);
		if (c != str) {
			score = INT32_MIN;
			break;
		} else {
			score -= utf8_strlen(str) - utf8_strlen(pattern);
		}
		pattern = strtok_r(NULL, " ", &saveptr);
	}
	free(tmp);
	return score;
}


/*
 * Split patterns into words, and return the sum of fuzzy_match(word, str).
 * If a word is not found, returns INT32_MIN.
 */
int32_t fuzzy_match_words(const char *restrict patterns, const char *restrict str)
{
	int32_t score = 0;
	char *saveptr = NULL;
	char *tmp = utf8_normalize(patterns);
	char *pattern = strtok_r(tmp, " ", &saveptr);
	while (pattern != NULL) {
		int32_t word_score = fuzzy_match(pattern, str);
		if (word_score == INT32_MIN) {
			score = INT32_MIN;
			break;
		} else {
			score += word_score;
		}
		pattern = strtok_r(NULL, " ", &saveptr);
	}
	free(tmp);
	return score;
}

/*
 * Returns score if each character in pattern is found sequentially within str.
 * Returns INT32_MIN otherwise.
 */
int32_t fuzzy_match(const char *restrict pattern, const char *restrict str)
{
	const int unmatched_letter_penalty = -1;
	const size_t slen = utf8_strlen(str);
	const size_t plen = utf8_strlen(pattern);
	int32_t score = 0;

	if (*pattern == '\0') {
		return score;
	}
	if (slen < plen) {
		return INT32_MIN;
	}

	/* We can already penalise any unused letters. */
	score += unmatched_letter_penalty * (int32_t)(slen - plen);

        /*
         * If the string is more than 100 characters, just find the first fuzzy
         * match rather than the best.
         *
         * This is required as the number of possible matches (for patterns and
         * strings all consisting of one letter) scales something like:
         *
         * slen! / (plen! (slen - plen)!)  ~  slen^plen for plen << slen
         *
         * This quickly grinds everything to a halt. 100 is chosen fairly
         * arbitrarily from the following logic:
         *
         * - e is the most common character in English, at around 13% of
         *   letters. Depending on the context, let's say this be up to 20%.
         * - 100 * 0.20 = 20 repeats of the same character.
         * - In the worst case here, 20! / (10! 10!) ~200,000 possible matches,
         *   which is "slow but not frozen" for my machine.
         *
         * In reality, this worst case shouldn't be hit, and finding the "best"
         * fuzzy match in lines of text > 100 characters isn't really in scope
         * for a dmenu clone.
         */
        bool first_match_only = slen > 100;

	/* Perform the match. */
	score = fuzzy_match_recurse(pattern, str, score, first_match_only, true);

	return score;
}

/*
 * Recursively match the whole of pattern against str.
 * The score parameter is the score of the previously matched character.
 *
 * This reaches a maximum recursion depth of strlen(pattern) + 1. However, the
 * stack usage is small (the maximum I've seen on x86_64 is 144 bytes with
 * gcc -O3), so this shouldn't matter unless pattern contains thousands of
 * characters.
 */
int32_t fuzzy_match_recurse(
		const char *restrict pattern,
		const char *restrict str,
		int32_t score,
		bool first_match_only,
		bool first_char)
{
	if (*pattern == '\0') {
		/* We've matched the full pattern. */
		return score;
	}

	const char *match = str;
	uint32_t search = utf8_to_utf32(pattern);

	int32_t best_score = INT32_MIN;

	/*
	 * Find all occurrences of the next pattern character in str, and
	 * recurse on them.
	 */
	while ((match = utf8_strcasechr(match, search)) != NULL) {
		int32_t jump = 0;
		for (const char *tmp = str; tmp != match; tmp = utf8_next_char(tmp)) {
			jump++;
		}
		int32_t subscore = fuzzy_match_recurse(
				utf8_next_char(pattern),
				utf8_next_char(match),
				compute_score(jump, first_char, match),
				first_match_only,
				false);
		best_score = MAX(best_score, subscore);
		match = utf8_next_char(match);

		if (first_match_only) {
			break;
		}
	}

	if (best_score == INT32_MIN) {
		/* We couldn't match the rest of the pattern. */
		return INT32_MIN;
	} else {
		return score + best_score;
	}
}

/*
 * Calculate the score for a single matching letter.
 * The scoring system is taken from fts_fuzzy_match v0.2.0 by Forrest Smith,
 * which is licensed to the public domain.
 *
 * The factors affecting score are:
 *   - Bonuses:
 *     - If there are multiple adjacent matches.
 *     - If a match occurs after a separator character.
 *     - If a match is uppercase, and the previous character is lowercase.
 *
 *   - Penalties:
 *     - If there are letters before the first match.
 *     - If there are superfluous characters in str (already accounted for).
 */
int32_t compute_score(int32_t jump, bool first_char, const char *restrict match)
{
	const int adjacency_bonus = 15;
	const int separator_bonus = 30;
	const int camel_bonus = 30;
	const int first_letter_bonus = 15;

	const int leading_letter_penalty = -5;
	const int max_leading_letter_penalty = -15;

	int32_t score = 0;

	const uint32_t cur = utf8_to_utf32(match);

	/* Apply bonuses. */
	if (!first_char && jump == 0) {
		score += adjacency_bonus;
	}
	if (!first_char || jump > 0) {
		const uint32_t prev = utf8_to_utf32(utf8_prev_char(match));
		if (utf32_isupper(cur) && utf32_islower(prev)) {
			score += camel_bonus;
		}
		if (utf32_isalnum(cur) && !utf32_isalnum(prev)) {
			score += separator_bonus;
		}
	}
	if (first_char && jump == 0) {
		/* Match at start of string gets separator bonus. */
		score += first_letter_bonus;
	}

	/* Apply penalties. */
	if (first_char) {
		score += MAX(leading_letter_penalty * jump,
				max_leading_letter_penalty);
	}

	return score;
}
