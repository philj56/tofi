#include <stdlib.h>
#include <string.h>
#include "color.h"
#include "log.h"

struct color hex_to_color(const char *hex)
{
	if (hex[0] == '#') {
		hex++;
	}

	uint32_t val = 0;
	size_t len = strlen(hex);

	if (len == 3) {
		char str[] = {
			hex[0], hex[0],
			hex[1], hex[1],
			hex[2], hex[2],
			'\0'};
		val = strtol(str, NULL, 16);
		val <<= 8;
		val |= 0xFFu;
	} else if (len == 4) {
		char str[] = {
			hex[0], hex[0],
			hex[1], hex[1],
			hex[2], hex[2],
			hex[3], hex[3],
			'\0'};
		val = strtol(str, NULL, 16);
	} else if (len == 6) {
		val = strtol(hex, NULL, 16);
		val <<= 8;
		val |= 0xFFu;
	} else if (len == 8) {
		val = strtol(hex, NULL, 16);
	} else {
		log_error("Invalid hex color %s\n", hex);
	}

	return (struct color) {
		.r = ((val & 0xFF000000u) >> 24) / 255.0f,
		.g = ((val & 0x00FF0000u) >> 16) / 255.0f,
		.b = ((val & 0x0000FF00u) >> 8)  / 255.0f,
		.a = ((val & 0x000000FFu) >> 0)  / 255.0f,
	};
}
