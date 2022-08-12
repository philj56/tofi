#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzzy_match.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*
 * Returns score if each character in pattern is found sequentially within str.
 * Returns INT32_MIN otherwise.
 *
 * The scoring system is taken from fts_fuzzy_match v0.1.0 by Forrest Smith,
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
 *     - If there are superfluous characters in str.
 */
int32_t fuzzy_match(const char *restrict pattern, const char *restrict str)
{
	if (*pattern == '\0') {
		return 0;
	}
	if (strlen(str) < strlen(pattern)) {
		return INT32_MIN;
	}

	const int adjacency_bonus = 5;
	const int separator_bonus = 10;
	const int camel_bonus = 10;

	const int leading_letter_penalty = -3;
	const int max_leading_letter_penalty = -9;
	const int unmatched_letter_penalty = -1;

	int32_t score = 0;
	const char *last_match = str;

	/*
	 * Iterate through pattern, jumping to the next matching character in
	 * str. This will only ever find the first fuzzy match, not the best,
	 * but it's good enough and very quick.
	 */
	for (size_t pidx = 0; pattern[pidx] != '\0'; pidx++) {
		char search[2] = { pattern[pidx], '\0' };
		char *match = strcasestr(last_match, search);
		if (match == NULL) {
			return INT32_MIN;
		}

		int32_t jump = match - last_match;

		/* Apply bonuses. */
		if (pidx > 0 && jump == 0) {
			score += adjacency_bonus;
		}
		if (pidx > 0 || jump > 0) {
			if (isupper(*match) && islower(*(match - 1))) {
				score += camel_bonus;
			}
			if (isalnum(*match) && !isalnum(*(match - 1))) {
				score += separator_bonus;
			}
		}
		if (pidx == 0 && jump == 0) {
			/* Match at start of string gets separator bonus. */
			score += separator_bonus;
		}

		/* Apply penalties. */
		if (pidx == 0) {
			score += MAX(leading_letter_penalty * jump,
					max_leading_letter_penalty);
		}

		last_match = match + 1;
	}

	score += unmatched_letter_penalty * (strlen(str) - strlen(pattern));

	return score;
}
