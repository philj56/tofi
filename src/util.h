#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>

struct image {
	uint8_t *buffer;
	uint32_t width;
	uint32_t height;
	bool swizzle;
	bool redraw;
};

struct color {
	float r;
	float g;
	float b;
	float a;
};

#endif /* UTIL_H */
