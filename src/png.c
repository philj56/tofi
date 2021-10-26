#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <png.h>
#include "client.h"
#include "log.h"
#include "png.h"

#define HEADER_BYTES 8

void load_background(struct client_state *state, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	uint8_t header[HEADER_BYTES];
	if (!fp) {
		log_error("Couldn't open %s\n", filename);
		return;
	}
	if (fread(header, 1, HEADER_BYTES, fp) != HEADER_BYTES) {
		log_error("Failed to read camera data: %s\n", filename);
		fclose(fp);
		return;
	}
	if (png_sig_cmp(header, 0, HEADER_BYTES)) {
		log_error("Not a PNG file: %s\n", filename);
		fclose(fp);
		return;
	}

	png_structp png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr) {
		log_error("Couldn't create PNG read struct.\n");
		fclose(fp);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		log_error("Couldn't create PNG info struct.\n");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		log_error("Couldn't setjmp for libpng.\n");
		return;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, HEADER_BYTES);
	png_read_info(png_ptr, info_ptr);

	uint32_t width = png_get_image_width(png_ptr, info_ptr);
	uint32_t height = png_get_image_height(png_ptr, info_ptr);
	//uint8_t color_type = png_get_color_type(png_ptr, info_ptr);
	//uint8_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	png_set_expand(png_ptr);
	png_set_gray_to_rgb(png_ptr);
	png_set_filler(png_ptr, 0xFFu, PNG_FILLER_AFTER);
	
	png_read_update_info(png_ptr, info_ptr);

	uint32_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	/* Guard against integer overflow */
	if (height > PNG_SIZE_MAX / row_bytes) {
		png_error(png_ptr, "image_data buffer would be too large");
	}

	png_bytep buffer = malloc(height * row_bytes);
	png_bytepp row_pointers = calloc(height, sizeof(png_bytep));
	for (uint32_t y = 0; y < height; y++) {
		row_pointers[y] = &buffer[y * row_bytes];
	}

	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(row_pointers);
	fclose(fp);

	state->background_image.width = width;
	state->background_image.height = height;
	state->background_image.buffer = buffer;
}
