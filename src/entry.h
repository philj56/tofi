#ifndef ENTRY_H
#define ENTRY_H

#include "entry_backend/pango.h"
#include "entry_backend/harfbuzz.h"

#include <cairo/cairo.h>
#include <uchar.h>
#include "color.h"
#include "desktop_vec.h"
#include "history.h"
#include "surface.h"
#include "string_vec.h"

#define MAX_INPUT_LENGTH 256
#define MAX_PROMPT_LENGTH 256
#define MAX_FONT_NAME_LENGTH 256
#define MAX_FONT_FEATURES_LENGTH 128
#define MAX_FONT_VARIATIONS_LENGTH 128

enum tofi_mode {
	TOFI_MODE_PLAIN,
	TOFI_MODE_RUN,
	TOFI_MODE_DRUN
};

enum cursor_style {
	CURSOR_STYLE_BAR,
	CURSOR_STYLE_BLOCK,
	CURSOR_STYLE_UNDERSCORE
};

struct directional {
	int32_t top;
	int32_t right;
	int32_t bottom;
	int32_t left;
};

struct text_theme {
	struct color foreground_color;
	struct color background_color;
	struct directional padding;
	uint32_t background_corner_radius;

	bool foreground_specified;
	bool background_specified;
	bool padding_specified;
	bool radius_specified;
};

struct cursor_theme {
	struct color color;
	struct color text_color;
	enum cursor_style style;
	uint32_t corner_radius;
	uint32_t thickness;

	double underline_depth;
	double em_width;

	bool color_specified;
	bool text_color_specified;
	bool thickness_specified;

	bool show;
};

struct entry {
	struct entry_backend_harfbuzz harfbuzz;
	struct entry_backend_pango pango;
	struct {
		cairo_surface_t *surface;
		cairo_t *cr;
	} cairo[2];
	int index;

	uint32_t input_utf32[MAX_INPUT_LENGTH];
	char input_utf8[4*MAX_INPUT_LENGTH];
	uint32_t input_utf32_length;
	uint32_t input_utf8_length;
	uint32_t cursor_position;

	uint32_t selection;
	uint32_t first_result;
	char *command_buffer;
	struct string_ref_vec results;
	struct string_ref_vec commands;
	struct desktop_vec apps;
	struct history history;
	bool use_pango;

	uint32_t clip_x;
	uint32_t clip_y;
	uint32_t clip_width;
	uint32_t clip_height;

	/* Options */
	enum tofi_mode mode;
	bool horizontal;
	bool hide_input;
	char hidden_character_utf8[6];
	uint8_t hidden_character_utf8_length;
	uint32_t num_results;
	uint32_t num_results_drawn;
	uint32_t last_num_results_drawn;
	int32_t result_spacing;
	int32_t result_vert_spacing;
	int32_t result_hori_spacing;
	uint32_t font_size;
	char font_name[MAX_FONT_NAME_LENGTH];
	char font_features[MAX_FONT_FEATURES_LENGTH];
	char font_variations[MAX_FONT_VARIATIONS_LENGTH];
	char prompt_text[MAX_PROMPT_LENGTH];
	char placeholder_text[MAX_PROMPT_LENGTH];
	uint32_t prompt_padding;
	uint32_t corner_radius;
	uint32_t padding_top;
	uint32_t padding_bottom;
	uint32_t padding_left;
	uint32_t padding_right;
	bool padding_top_is_percent;
	bool padding_bottom_is_percent;
	bool padding_left_is_percent;
	bool padding_right_is_percent;
	bool clip_to_padding;
	uint32_t input_width;
	uint32_t border_width;
	uint32_t outline_width;
	struct color foreground_color;
	struct color background_color;
	struct color selection_highlight_color;
	struct color border_color;
	struct color outline_color;

	struct cursor_theme cursor_theme;
	struct text_theme prompt_theme;
	struct text_theme input_theme;
	struct text_theme placeholder_theme;
	struct text_theme default_result_theme;
	struct text_theme alternate_result_theme;
	struct text_theme selection_theme;
};

void entry_init(struct entry *entry, uint8_t *restrict buffer, uint32_t width, uint32_t height, uint32_t fractional_scale_numerator);
void entry_destroy(struct entry *entry);
void entry_update(struct entry *entry);

#endif /* ENTRY_H */
