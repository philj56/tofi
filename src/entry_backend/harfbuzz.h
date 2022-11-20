#ifndef ENTRY_BACKEND_HARFBUZZ_H
#define ENTRY_BACKEND_HARFBUZZ_H

#include <stdbool.h>
#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <harfbuzz/hb.h>

#define MAX_FONT_FEATURES 16

struct entry;

struct entry_backend_harfbuzz {
	FT_Library ft_library;
	FT_Face ft_face;

	cairo_font_face_t *cairo_face;

	hb_font_t *hb_font;
	hb_buffer_t *hb_buffer;
	hb_feature_t hb_features[MAX_FONT_FEATURES];
	uint8_t num_features;

	bool disable_hinting;
};

void entry_backend_harfbuzz_init(struct entry *entry, uint32_t *width, uint32_t *height);
void entry_backend_harfbuzz_destroy(struct entry *entry);
void entry_backend_harfbuzz_update(struct entry *entry);

#endif /* ENTRY_BACKEND_HARFBUZZ_H */
