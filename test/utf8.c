#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matching.h"
#include "tap.h"

void is_single_match(enum matching_algorithm algorithm, const char *pattern, const char *str, const char *message)
{
	int32_t res = match_words(algorithm, pattern, str);
	tap_isnt(res, INT32_MIN, message);
}

void isnt_single_match(enum matching_algorithm algorithm, const char *pattern, const char *str, const char *message)
{
	int32_t res = match_words(algorithm, pattern, str);
	tap_is(res, INT32_MIN, message);
}

void is_match(const char *pattern, const char *str, const char *message)
{
	is_single_match(MATCHING_ALGORITHM_NORMAL, pattern, str, message);
	is_single_match(MATCHING_ALGORITHM_PREFIX, pattern, str, message);
	is_single_match(MATCHING_ALGORITHM_FUZZY, pattern, str, message);
}

void isnt_match(const char *pattern, const char *str, const char *message)
{
	isnt_single_match(MATCHING_ALGORITHM_NORMAL, pattern, str, message);
	isnt_single_match(MATCHING_ALGORITHM_PREFIX, pattern, str, message);
	isnt_single_match(MATCHING_ALGORITHM_FUZZY, pattern, str, message);
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	tap_version(14);

	/* Case insensitivity. */
	is_match("o", "O", "Single Latin character, different case");
	is_match("д", "Д", "Single Cyrillic character, different case");
	is_match("ξ", "Ξ", "Single Greek character, different case");
	is_match("o", "ọ", "Single character with decomposed diacritic");

	/* Combining diacritics. */
	isnt_match("o", "ọ", "Single character with composed diacritic");
	isnt_single_match(MATCHING_ALGORITHM_NORMAL, "ạ", "aọ", "Decomposed diacritics, character mismatch");
	tap_todo("Needs composed character comparison");
	isnt_single_match(MATCHING_ALGORITHM_FUZZY, "ạ", "aọ", "Decomposed diacritics, character mismatch");

	tap_plan();

	return EXIT_SUCCESS;
}
