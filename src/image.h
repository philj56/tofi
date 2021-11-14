#ifndef IMAGE_H
#define IMAGE_H

#include <stdbool.h>
#include <stdint.h>

struct image {
	uint8_t *buffer;
	uint32_t width;
	uint32_t height;
	bool swizzle;
	bool redraw;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t width;
		uint32_t height;
	} damage;
};

void image_load(struct image *image, const char *filename);

#endif /* IMAGE_H */
