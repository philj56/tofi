#ifndef ENTRY_H
#define ENTRY_H

#ifdef USE_PANGO
#include "entry_backend/pango.h"
#else
#include "entry_backend/harfbuzz.h"
#endif

#include <cairo/cairo.h>
#include "color.h"
#include "history.h"
#include "image.h"
#include "surface.h"
#include "string_vec.h"

#define MAX_INPUT_LENGTH 128

struct entry {
	struct image image;
	struct entry_backend backend;
	struct {
		cairo_surface_t *surface;
		cairo_t *cr;
	} cairo[2];
	int index;

	wchar_t input[MAX_INPUT_LENGTH];
	/* Assume maximum of 4 bytes per wchar_t (for UTF-8) */
	char input_mb[4*MAX_INPUT_LENGTH];
	uint32_t input_length;
	uint32_t input_mb_length;

	struct string_vec results;
	struct string_vec commands;
	struct history history;

	/* Options */
	uint32_t font_size;
	const char *font_name;
	uint32_t padding;
	uint32_t num_characters;
	uint32_t num_lines;
	struct color foreground_color;
	struct color background_color;
	struct {
		struct color color;
		struct color outline_color;
		int32_t width;
		int32_t outline_width;
	} border;
};

void entry_init(struct entry *entry, uint8_t *restrict buffer, uint32_t width, uint32_t height, uint32_t scale);
void entry_destroy(struct entry *entry);
void entry_update(struct entry *entry);
void entry_set_scale(struct entry *entry, uint32_t scale);

#endif /* ENTRY_H */
