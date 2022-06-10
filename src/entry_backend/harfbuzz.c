#include <cairo/cairo.h>
#include <glib.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-glib.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <wchar.h>
#include "harfbuzz.h"
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"
#include "../xmalloc.h"

/*
 * FreeType is normally compiled without error strings, so we have to do this
 * funky macro trick to get them. See <freetype/fterrors.h> for more details.
 */
#undef FTERRORS_H_
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, NULL } };

const struct {
	int err_code;
	const char *err_msg;
} ft_errors[] =

#include <freetype/fterrors.h>

/*
 * Cairo / FreeType use 72 Pts per inch, but Pango uses 96 DPI, so we have to
 * rescale for consistency.
 */
#define PT_TO_DPI (96.0 / 72.0)

/*
 * hb_buffer_clear_contents also clears some basic script information, so group
 * them here for convenience.
 */
static void setup_hb_buffer(hb_buffer_t *buffer)
{
	hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
	hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
	hb_buffer_set_language(buffer, hb_language_from_string("en", -1));
}


/*
 * Render a hb_buffer with Cairo, and return the width of the rendered area in
 * Cairo units.
 */
static uint32_t render_hb_buffer(cairo_t *cr, hb_buffer_t *buffer, uint32_t scale)
{
	cairo_save(cr);

	/*
	 * Cairo uses y-down coordinates, but HarfBuzz uses y-up, so we
	 * shift the text down by its ascent height to compensate.
	 */
	cairo_font_extents_t font_extents;
	cairo_font_extents(cr, &font_extents);
	cairo_translate(cr, 0, font_extents.ascent);

	unsigned int glyph_count;
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buffer, &glyph_count);
	cairo_glyph_t *cairo_glyphs = xmalloc(sizeof(cairo_glyph_t) * glyph_count);

	double x = 0;
	double y = 0;
	for (unsigned int i=0; i < glyph_count; i++) {
		/*
		 * The coordinates returned by HarfBuzz are in 26.6 fixed-point
		 * format, so we divide by 64.0 (2^6) to get floats.
		 * We also divide by the display's scale factor, to counter
		 * Cairo's scaling of coordinates and sizes.
		 */
		cairo_glyphs[i].index = glyph_info[i].codepoint;
		cairo_glyphs[i].x = x + glyph_pos[i].x_offset / 64.0 / scale;
		cairo_glyphs[i].y = y - glyph_pos[i].y_offset / 64.0 / scale;
		x += glyph_pos[i].x_advance / 64.0 / scale;
		y -= glyph_pos[i].y_advance / 64.0 / scale;
	}

	cairo_show_glyphs(cr, cairo_glyphs, glyph_count);

	free(cairo_glyphs);

	cairo_restore(cr);

	double width = 0;
	for (unsigned int i=0; i < glyph_count; i++) {
		width += glyph_pos[i].x_advance / 64.0 / scale;
	}
	return ceil(width);
}

void entry_backend_init(
		struct entry *entry,
		uint32_t *width,
		uint32_t *height,
		uint32_t scale)
{
	cairo_t *cr = entry->cairo[0].cr;

	/* Setup FreeType. */
	log_debug("Creating FreeType library.\n");
	int err;
	err = FT_Init_FreeType(&entry->backend.ft_library);
	if (err) {
		log_error("Error initialising FreeType: %s\n",
				ft_errors[err].err_msg);
		exit(EXIT_FAILURE);
	}

	log_debug("Loading FreeType font.\n");
	err = FT_New_Face(
			entry->backend.ft_library, "font.ttf", 0,
			&entry->backend.ft_face);
	if (err) {
		log_error("Error loading font: %s\n", ft_errors[err].err_msg);
		exit(EXIT_FAILURE);
	}
	err = FT_Set_Char_Size(
			entry->backend.ft_face,
			entry->font_size * 64 * scale,
			entry->font_size * 64 * scale,
			96,
			96);
	if (err) {
		log_error("Error setting font size: %s\n",
				ft_errors[err].err_msg);
		exit(EXIT_FAILURE);
	}

	log_debug("Creating Cairo font.\n");
	entry->backend.cairo_face =
		cairo_ft_font_face_create_for_ft_face(entry->backend.ft_face, 0);

	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_set_font_face(cr, entry->backend.cairo_face);
	cairo_set_font_size(cr, entry->font_size * PT_TO_DPI);

	/* We also need to set up the font for our other Cairo context. */
	cairo_set_font_face(entry->cairo[1].cr, entry->backend.cairo_face);
	cairo_set_font_size(entry->cairo[1].cr, entry->font_size * PT_TO_DPI);

	log_debug("Creating Harfbuzz font.\n");
	entry->backend.hb_font =
		hb_ft_font_create_referenced(entry->backend.ft_face);

	log_debug("Creating Harfbuzz buffer.\n");
	hb_buffer_t *buffer = hb_buffer_create();
	entry->backend.hb_buffer = buffer;
	hb_buffer_set_unicode_funcs(buffer, hb_glib_get_unicode_funcs());
	setup_hb_buffer(buffer);

	/* Draw the prompt now, as this only needs to be done once */
	log_debug("Drawing prompt.\n");
	hb_buffer_add_utf8(entry->backend.hb_buffer, "run: ", -1, 0, -1);
	hb_shape(entry->backend.hb_font, entry->backend.hb_buffer, NULL, 0);
	uint32_t prompt_width = render_hb_buffer(cr, buffer, scale);

	/* Move and clip so we don't draw over the prompt later */
	cairo_translate(cr, prompt_width, 0);
	*width -= prompt_width;
	cairo_rectangle(cr, 0, 0, *width, *height);
	cairo_clip(cr);
}

void entry_backend_destroy(struct entry *entry)
{
	hb_buffer_destroy(entry->backend.hb_buffer);
	hb_font_destroy(entry->backend.hb_font);
	FT_Done_Face(entry->backend.ft_face);
	FT_Done_FreeType(entry->backend.ft_library);
}

void entry_backend_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	hb_buffer_t *buffer = entry->backend.hb_buffer;

	/* Render the entry text */
	hb_buffer_clear_contents(buffer);
	setup_hb_buffer(buffer);
	hb_buffer_add_utf8(buffer, entry->input_mb, -1, 0, -1);
	hb_shape(entry->backend.hb_font, buffer, NULL, 0);
	render_hb_buffer(cr, buffer, entry->image.scale);

	cairo_font_extents_t font_extents;
	cairo_font_extents(cr, &font_extents);

	/* Render our results entry text */
	for (size_t i = 0; i < 5 && i < entry->results.count; i++) {
		cairo_translate(cr, 0, font_extents.height);

		hb_buffer_clear_contents(buffer);
		setup_hb_buffer(buffer);
		hb_buffer_add_utf8(buffer, entry->results.buf[i], -1, 0, -1);
		hb_shape(entry->backend.hb_font, buffer, NULL, 0);
		render_hb_buffer(cr, buffer, entry->image.scale);
	}
}
