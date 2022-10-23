#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fuzzy_match.h"
#include "tap.h"

void is_simple_match(const char *pattern, const char *str, const char *message)
{
	int32_t res = fuzzy_match_simple_words(pattern, str);
	tap_isnt(res, INT32_MIN, message);
}

void isnt_simple_match(const char *pattern, const char *str, const char *message)
{
	int32_t res = fuzzy_match_simple_words(pattern, str);
	tap_is(res, INT32_MIN, message);
}

void is_fuzzy_match(const char *pattern, const char *str, const char *message)
{
	int32_t res = fuzzy_match_words(pattern, str);
	tap_isnt(res, INT32_MIN, message);
}

void isnt_fuzzy_match(const char *pattern, const char *str, const char *message)
{
	int32_t res = fuzzy_match_words(pattern, str);
	tap_is(res, INT32_MIN, message);
}

void is_match(const char *pattern, const char *str, const char *message)
{
	is_simple_match(pattern, str, message);
	is_fuzzy_match(pattern, str, message);
}

void isnt_match(const char *pattern, const char *str, const char *message)
{
	isnt_simple_match(pattern, str, message);
	isnt_fuzzy_match(pattern, str, message);
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
	isnt_simple_match("ạ", "aọ", "Decomposed diacritics, character mismatch");
	tap_todo("Needs composed character comparison");
	isnt_fuzzy_match("ạ", "aọ", "Decomposed diacritics, character mismatch");

	tap_plan();

	return EXIT_SUCCESS;
}
