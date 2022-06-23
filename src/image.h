#ifndef IMAGE_H
#define IMAGE_H

#include <stdbool.h>
#include <stdint.h>

struct image {
	uint32_t width;
	uint32_t height;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t width;
		uint32_t height;
	} damage;
};

#endif /* IMAGE_H */
