#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzzy_match.h"
#include "unicode.h"
#include "xmalloc.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int32_t compute_score(
		int32_t jump,
		bool first_char,
		const char *restrict match);

static int32_t fuzzy_match_recurse(
		const char *restrict pattern,
		const char *restrict str,
		int32_t score,
		bool first_match_only,
		bool first_char);

/*
 * Split patterns into words, and perform simple matching against str for each.
 * Returns the sum of substring distances from the start of str.
 * If a word is not found, returns INT32_MIN.
 */
int32_t fuzzy_match_simple_words(const char *restrict patterns, const char *restrict str)
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
			score += str - c;
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

int32_t levenshtein(const char *restrict pattern, const char *restrict str)
{
	const size_t slen = strlen(str);
	const size_t plen = strlen(pattern);

	if (*pattern == '\0') {
		return INT32_MIN;
	}
	//if (slen < plen) {
	//	return INT32_MIN;
	//}

	uint32_t *v0 = xcalloc(slen + 1, sizeof(*v0));
	uint32_t *v1 = xcalloc(slen + 1, sizeof(*v1));

	for (uint32_t i = 0; i < slen + 1; i++) {
		v0[i] = i;
	}

	for (uint32_t i = 0; i < plen; i++) {
		v1[0] = i + 1;

		for (uint32_t j = 0; j < slen; j++) {
			uint32_t deletion_cost = v0[j + 1] + 1;
			uint32_t insertion_cost = v1[j] + 1;
			uint32_t substitution_cost;
			if (pattern[i] == str[j]) {
				substitution_cost = v0[j];
			} else {
				substitution_cost = v0[j] + 1;
			}

			v1[j + 1] = MIN(deletion_cost, MIN(insertion_cost, substitution_cost));
		}

		uint32_t *tmp = v0;
		v0 = v1;
		v1 = tmp;
	}

	uint32_t result = v0[slen];

	free(v0);
	free(v1);

	return result;
}

int32_t levenshtein_osa(const char *restrict pattern, const char *restrict str)
{
	const size_t slen = strlen(str);
	const size_t plen = strlen(pattern);

	const uint32_t deletion_price = 1;
	const uint32_t insertion_price = 1;
	const uint32_t substitution_price = 1;
	const uint32_t transposition_price = 1;

	if (*pattern == '\0') {
		return INT32_MIN;
	}
	if (slen < plen - 1) {
		return INT32_MIN;
	}

	// vn: n = difference from current row
	uint32_t *v0 = xcalloc(slen + 1, sizeof(*v0));
	uint32_t *v1 = xcalloc(slen + 1, sizeof(*v1));
	uint32_t *v2 = xcalloc(slen + 1, sizeof(*v2));

	for (uint32_t i = 0; i < slen + 1; i++) {
		v1[i] = i * deletion_price;
	}

	for (uint32_t i = 0; i < plen; i++) {
		v0[0] = (i + 1) * deletion_price;

		for (uint32_t j = 0; j < slen; j++) {
			uint32_t deletion_cost = v1[j + 1] + deletion_price;
			uint32_t insertion_cost = v0[j] + insertion_price;
			uint32_t substitution_cost;
			if (pattern[i] == str[j]) {
				substitution_cost = v1[j];
			} else {
				substitution_cost = v1[j] + substitution_price;
			}

			v0[j + 1] = MIN(deletion_cost, MIN(insertion_cost, substitution_cost));

			if (i > 0 && j > 0 && pattern[i] == str[j - 1] && pattern[i - 1] == str[j]) {
				uint32_t transposition_cost = v2[j - 1] + transposition_price;
				v0[j + 1] = MIN(v0[j + 1], transposition_cost);
			}
		}

		uint32_t *tmp = v2;
		v2 = v1;
		v1 = v0;
		v0 = tmp;
	}

	uint32_t result = v1[slen];

	free(v0);
	free(v1);
	free(v2);

	return result;
}
