#include <errno.h>
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
	int64_t tmp;
	size_t len = strlen(hex);

	errno = 0;
	if (len == 3) {
		char str[] = {
			hex[0], hex[0],
			hex[1], hex[1],
			hex[2], hex[2],
			'\0'};
		char *endptr;
		tmp = strtoll(str, &endptr, 16);
		if (errno || *endptr != '\0' || tmp < 0) {
			return (struct color) { -1, -1, -1, -1 };
		}
		val = tmp;
		val <<= 8;
		val |= 0xFFu;
	} else if (len == 4) {
		char str[] = {
			hex[0], hex[0],
			hex[1], hex[1],
			hex[2], hex[2],
			hex[3], hex[3],
			'\0'};
		char *endptr;
		tmp = strtoll(str, &endptr, 16);
		if (errno || *endptr != '\0' || tmp < 0) {
			return (struct color) { -1, -1, -1, -1 };
		}
		val = tmp;
	} else if (len == 6) {
		char *endptr;
		tmp = strtoll(hex, &endptr, 16);
		if (errno || *endptr != '\0' || tmp < 0) {
			return (struct color) { -1, -1, -1, -1 };
		}
		val = tmp;
		val <<= 8;
		val |= 0xFFu;
	} else if (len == 8) {
		char *endptr;
		tmp = strtoll(hex, &endptr, 16);
		if (errno || *endptr != '\0' || tmp < 0) {
			return (struct color) { -1, -1, -1, -1 };
		}
		val = tmp;
	} else {
		return (struct color) { -1, -1, -1, -1 };
	}

	return (struct color) {
		.r = (float)((val & 0xFF000000u) >> 24) / 255.0f,
		.g = (float)((val & 0x00FF0000u) >> 16) / 255.0f,
		.b = (float)((val & 0x0000FF00u) >> 8)  / 255.0f,
		.a = (float)((val & 0x000000FFu) >> 0)  / 255.0f,
	};
}
