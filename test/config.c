#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "tofi.h"
#include "tap.h"

void is_valid(const char *option, const char *value, const char *message)
{
	struct tofi tofi;
	bool res = config_apply(&tofi, option, value);
	tap_is(res, true, message);
}

void isnt_valid(const char *option, const char *value, const char *message)
{
	struct tofi tofi;
	bool res = config_apply(&tofi, option, value);
	tap_is(res, false, message);
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	tap_version(14);

	/* Anchors */
	is_valid("anchor", "top-left", "Anchor top-left");
	is_valid("anchor", "top", "Anchor top");
	is_valid("anchor", "top-right", "Anchor top-right");
	is_valid("anchor", "right", "Anchor right");
	is_valid("anchor", "bottom-right", "Anchor bottom-right");
	is_valid("anchor", "bottom", "Anchor bottom");
	is_valid("anchor", "bottom-left", "Anchor bottom-left");
	is_valid("anchor", "left", "Anchor left");
	is_valid("anchor", "center", "Anchor center");
	isnt_valid("anchor", "left-bottom", "Invalid anchor");

	/* Bools */
	is_valid("horizontal", "tRuE", "Boolean true");
	is_valid("horizontal", "fAlSe", "Boolean false");
	isnt_valid("horizontal", "truefalse", "Invalid boolean");

	/* Password characters */
	is_valid("hidden-character", "O", "Single Latin character");
	is_valid("hidden-character", "Д", "Single Cyrillic character");
	is_valid("hidden-character", "Ξ", "Single Greek character");
	is_valid("hidden-character", "ọ", "Single character with decomposed diacritic");
	is_valid("hidden-character", "漢", "Single CJK character");
	isnt_valid("hidden-character", "ae", "Multiple characters");

	/* Colours */
	is_valid("text-color", "46B", "Three character color without hash");
	is_valid("text-color", "#46B", "Three character color with hash");
	is_valid("text-color", "46BA", "Four character color without hash");
	is_valid("text-color", "#46BA", "Four character color with hash");
	is_valid("text-color", "4466BB", "Six character color without hash");
	is_valid("text-color", "#4466BB", "Six character color with hash");
	is_valid("text-color", "4466BBAA", "Eight character color without hash");
	is_valid("text-color", "#4466BBAA", "Eight character color with hash");
	isnt_valid("text-color", "4466BBA", "Five character color without hash");
	isnt_valid("text-color", "#4466BBA", "Five character color with hash");
	isnt_valid("text-color", "9GB", "Three character color with invalid characters");
	isnt_valid("text-color", "95GB", "Four character color with invalid characters");
	isnt_valid("text-color", "95XGUB", "Six character color with invalid characters");
	isnt_valid("text-color", "950-4GBY", "Eight character color with invalid characters");
	isnt_valid("text-color", "-99", "Negative two character color");
	isnt_valid("text-color", "-999", "Negative three character color");
	isnt_valid("text-color", "-9999", "Negative four character color");
	isnt_valid("text-color", "-99999", "Negative five character color");
	isnt_valid("text-color", "-999999", "Negative six character color");
	isnt_valid("text-color", "-9999999", "Negative seven character color");
	isnt_valid("text-color", "-99999999", "Negative eight character color");

	/* Signed values */
	is_valid("result-spacing", "-2147483648", "INT32 Min");
	is_valid("result-spacing", "2147483647", "INT32 Max");
	isnt_valid("result-spacing", "-2147483649", "INT32 Min - 1");
	isnt_valid("result-spacing", "2147483648", "INT32 Max + 1");
	isnt_valid("result-spacing", "6A", "INT32 invalid character");

	/* Unsigned values */
	is_valid("corner-radius", "0", "UINT32 0");
	is_valid("corner-radius", "4294967295", "UINT32 Max");
	isnt_valid("corner-radius", "4294967296", "UINT32 Max + 1");
	isnt_valid("corner-radius", "-1", "UINT32 -1");
	isnt_valid("corner-radius", "6A", "UINT32 invalid character");

	/* Unsigned percentages */
	is_valid("width", "0", "UINT32 0 percent without sign");
	is_valid("width", "0%", "UINT32 0 percent with sign");
	is_valid("width", "4294967295", "UINT32 Max percent without sign");
	is_valid("width", "4294967295%", "UINT32 Max percent with sign");
	isnt_valid("width", "4294967296", "UINT32 Max + 1 percent without sign");
	isnt_valid("width", "4294967296%", "UINT32 Max + 1 percent with sign");
	isnt_valid("width", "-1", "UINT32 -1 percent without sign");
	isnt_valid("width", "-1%", "UINT32 -1 percent with sign");

	/* Directional values */
	is_valid("prompt-background-padding", "0", "Single directional value");
	is_valid("prompt-background-padding", "0,1", "Two directional values");
	is_valid("prompt-background-padding", "0,1,-2", "Three directional values");
	is_valid("prompt-background-padding", "0,1,-2,3", "Four directional values");
	isnt_valid("prompt-background-padding", "0,1,-2,3,-4", "Five directional values");
	isnt_valid("prompt-background-padding", "0,1,-2,3,-4,5", "Six directional values");

	tap_plan();

	return EXIT_SUCCESS;
}
