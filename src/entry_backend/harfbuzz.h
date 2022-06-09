#ifndef ENTRY_BACKEND_HARFBUZZ_H
#define ENTRY_BACKEND_HARFBUZZ_H

#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <harfbuzz/hb.h>
#include <pango/pangocairo.h>

struct entry;

struct entry_backend {
	FT_Library ft_library;
	FT_Face ft_face;

	cairo_font_face_t *cairo_face;

	hb_font_t *hb_font;
	hb_buffer_t *hb_buffer;
};

void entry_backend_init(struct entry *entry, uint32_t *width, uint32_t *height, uint32_t scale);
void entry_backend_destroy(struct entry *entry);
void entry_backend_update(struct entry *entry);

#endif /* ENTRY_BACKEND_HARFBUZZ_H */
