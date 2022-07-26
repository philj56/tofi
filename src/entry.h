#ifndef ENTRY_H
#define ENTRY_H

#include "entry_backend/pango.h"
#include "entry_backend/harfbuzz.h"

#include <cairo/cairo.h>
#include <wchar.h>
#include "color.h"
#include "desktop_vec.h"
#include "history.h"
#include "image.h"
#include "surface.h"
#include "string_vec.h"

#define MAX_INPUT_LENGTH 256
#define MAX_PROMPT_LENGTH 256
#define MAX_FONT_NAME_LENGTH 256

struct entry {
	struct image image;
	struct entry_backend_harfbuzz harfbuzz;
	struct entry_backend_pango pango;
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

	uint32_t selection;
	struct string_vec results;
	struct string_vec commands;
	struct desktop_vec apps;
	struct history history;
	bool use_pango;

	uint32_t clip_x;
	uint32_t clip_y;
	uint32_t clip_width;
	uint32_t clip_height;

	/* Options */
	bool drun;
	bool horizontal;
	uint32_t num_results;
	uint32_t num_results_drawn;
	int32_t result_spacing;
	uint32_t font_size;
	char font_name[MAX_FONT_NAME_LENGTH];
	char prompt_text[MAX_PROMPT_LENGTH];
	uint32_t corner_radius;
	uint32_t padding_top;
	uint32_t padding_bottom;
	uint32_t padding_left;
	uint32_t padding_right;
	uint32_t input_width;
	uint32_t border_width;
	uint32_t outline_width;
	struct color foreground_color;
	struct color background_color;
	struct color selection_foreground_color;
	struct color selection_background_color;
	struct color border_color;
	struct color outline_color;
};

void entry_init(struct entry *entry, uint8_t *restrict buffer, uint32_t width, uint32_t height);
void entry_destroy(struct entry *entry);
void entry_update(struct entry *entry);

#endif /* ENTRY_H */
