#ifndef COLOR_H
#define COLOR_H

#include <stdbool.h>
#include <stdint.h>

struct color {
	float r;
	float g;
	float b;
	float a;
};

struct color hex_to_color(const char *hex);

#endif /* COLOR_H */
