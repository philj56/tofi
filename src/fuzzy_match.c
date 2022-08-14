#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzzy_match.h"

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
		bool first_char);

/*
 * Returns score if each character in pattern is found sequentially within str.
 * Returns INT32_MIN otherwise.
 */
int32_t fuzzy_match(const char *restrict pattern, const char *restrict str)
{
	const int unmatched_letter_penalty = -1;
	const size_t slen = strlen(str);
	const size_t plen = strlen(pattern);
	int32_t score = 0;

	if (*pattern == '\0') {
		return score;
	}
	if (slen < plen) {
		return INT32_MIN;
	}

	/* We can already penalise any unused letters. */
	score += unmatched_letter_penalty * (int32_t)(slen - plen);

	/* Perform the match. */
	score = fuzzy_match_recurse(pattern, str, score, true);

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
		bool first_char)
{
	if (*pattern == '\0') {
		/* We've matched the full pattern. */
		return score;
	}

	const char *match = str;
	const char search[2] = { *pattern, '\0' };

	int32_t best_score = INT32_MIN;

	/*
	 * Find all occurrences of the next pattern character in str, and
	 * recurse on them.
	 */
	while ((match = strcasestr(match, search)) != NULL) {
		int32_t subscore = fuzzy_match_recurse(
				pattern + 1,
				match + 1,
				compute_score(match - str, first_char, match),
				false);
		best_score = MAX(best_score, subscore);
		match++;
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

	/* Apply bonuses. */
	if (!first_char && jump == 0) {
		score += adjacency_bonus;
	}
	if (!first_char || jump > 0) {
		if (isupper(*match) && islower(*(match - 1))) {
			score += camel_bonus;
		}
		if (isalnum(*match) && !isalnum(*(match - 1))) {
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
