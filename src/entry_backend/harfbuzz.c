#include <cairo/cairo.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-ot.h>
#include <math.h>
#include "harfbuzz.h"
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"
#include "../unicode.h"
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

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void rounded_rectangle(cairo_t *cr, uint32_t width, uint32_t height, uint32_t r)
{
	cairo_new_path(cr);

	/* Top-left */
	cairo_arc(cr, r, r, r, -M_PI, -M_PI_2);

	/* Top-right */
	cairo_arc(cr, width - r, r, r, -M_PI_2, 0);

	/* Bottom-right */
	cairo_arc(cr, width - r, height - r, r, 0, M_PI_2);

	/* Bottom-left */
	cairo_arc(cr, r, height - r, r, M_PI_2, M_PI);

	cairo_close_path(cr);
}

static const char *get_ft_error_string(int err_code)
{
	for (size_t i = 0; i < N_ELEM(ft_errors); i++) {
		if (ft_errors[i].err_code == err_code) {
			return ft_errors[i].err_msg;
		}
	}

	return "Unknown FT error";
}

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
	hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
}

/*
 * Render a hb_buffer with Cairo, and return the extents of the rendered text
 * in Cairo units.
 */
static cairo_text_extents_t render_hb_buffer(cairo_t *cr, hb_font_extents_t *font_extents, hb_buffer_t *buffer, double scale)
{
	cairo_save(cr);

	/*
	 * Cairo uses y-down coordinates, but HarfBuzz uses y-up, so we
	 * shift the text down by its ascent height to compensate.
	 */
	cairo_translate(cr, 0, font_extents->ascender / 64.0);

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
		 *
		 * For whatever reason, the coordinates are also scaled by
		 * Cairo's scale factor, so we have to also divide by the scale
		 * factor to account for this.
		 */
		cairo_glyphs[i].index = glyph_info[i].codepoint;
		cairo_glyphs[i].x = x + glyph_pos[i].x_offset / 64.0 / scale;
		cairo_glyphs[i].y = y - glyph_pos[i].y_offset / 64.0 / scale;

		x += glyph_pos[i].x_advance / 64.0 / scale;
		y -= glyph_pos[i].y_advance / 64.0 / scale;
	}

	cairo_show_glyphs(cr, cairo_glyphs, glyph_count);

	cairo_text_extents_t extents;
	cairo_glyph_extents(cr, cairo_glyphs, glyph_count, &extents);

	/* Account for the shifted baseline in our returned text extents. */
	extents.y_bearing += font_extents->ascender / 64.0;

	free(cairo_glyphs);

	cairo_restore(cr);

	return extents;
}

/*
 * Clear the harfbuzz buffer, shape some text and render it with Cairo,
 * returning the extents of the rendered text in Cairo units.
 */
static cairo_text_extents_t render_text(
		cairo_t *cr,
		struct entry_backend_harfbuzz *hb,
		const char *text)
{
	hb_buffer_clear_contents(hb->hb_buffer);
	setup_hb_buffer(hb->hb_buffer);
	hb_buffer_add_utf8(hb->hb_buffer, text, -1, 0, -1);
	hb_shape(hb->hb_font, hb->hb_buffer, hb->hb_features, hb->num_features);
	return render_hb_buffer(cr, &hb->hb_font_extents, hb->hb_buffer, hb->scale);
}

/*
 * Render the background box for a piece of text with the given theme and text
 * extents.
 */
static void render_text_background(
		cairo_t *cr,
		struct entry *entry,
		cairo_text_extents_t extents,
		const struct text_theme *theme)
{
	struct directional padding = theme->padding;
	cairo_font_extents_t font_extents = entry->harfbuzz.cairo_font_extents;

	/*
	 * If any of the padding values are negative, make them just big enough
	 * to fit the available area. This is needed over just making them
	 * bigger than the clip area in order to make rounded corners line up
	 * with the edges nicely.
	 */
	cairo_matrix_t mat;
	cairo_get_matrix(cr, &mat);
	double base_x = mat.x0 - entry->clip_x + extents.x_bearing;
	double base_y = mat.y0 - entry->clip_y;

	/* Need to use doubles to end up on nice pixel boundaries. */
	double padding_left = padding.left;
	double padding_right = padding.right;
	double padding_top = padding.top;
	double padding_bottom = padding.bottom;

	if (padding.left < 0) {
		padding_left = base_x;
	}
	if (padding.right < 0) {
		padding_right = entry->clip_width - extents.width - base_x;
	}
	if (padding.top < 0) {
		padding_top = base_y;
	}
	if (padding.bottom < 0) {
		padding_bottom = entry->clip_height - font_extents.height - base_y;
	}

	cairo_save(cr);
	struct color color = theme->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_translate(
			cr,
			-padding_left + extents.x_bearing,
			-padding_top);
	rounded_rectangle(
			cr,
			ceil(extents.width + padding_left + padding_right),
			ceil(font_extents.height + padding_top + padding_bottom),
			theme->background_corner_radius
			);
	cairo_fill(cr);
	cairo_restore(cr);
}


/*
 * Render some text with an optional background box, using settings from the
 * given theme.
 */
static cairo_text_extents_t render_text_themed(
		cairo_t *cr,
		struct entry *entry,
		const char *text,
		const struct text_theme *theme)
{
	struct entry_backend_harfbuzz *hb = &entry->harfbuzz;

	/*
	 * I previously thought rendering the text to a group, measuring it,
	 * drawing the box on the main canvas and then drawing the group would
	 * be the most efficient way of doing this. I was wrong.
	 *
	 * It turns out to be much quicker to just draw the text to the canvas,
	 * paint over it with the box, and then draw the text again. This is
	 * fine, as long as the box is always bigger than the text (which it is
	 * unless the user sets some extreme values for the corner radius).
	 */
	struct color color = theme->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_text_extents_t extents = render_text(cr, hb, text);

	if (theme->background_color.a == 0) {
		/* No background to draw, we're done. */
		return extents;
	}

	render_text_background(cr, entry, extents, theme);

	color = theme->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	render_text(cr, hb, text);
	return extents;
}

/*
 * Rendering the input is more complicated when a cursor is involved.
 *
 * Firstly, we need to use UTF-32 strings in order to properly position the
 * cursor when ligatures / combining diacritics are involved.
 *
 * Next, we need to do some calculations on the shaped hb_buffer, to work out
 * where to draw the cursor and how wide it needs to be. We may also need to
 * make the background wider to account for the cursor.
 *
 * Finally, if we're drawing a block-style cursor, we may need to render the
 * text again to draw the highlighted character in a different colour.
 */
static cairo_text_extents_t render_input(
		cairo_t *cr,
		struct entry_backend_harfbuzz *hb,
		const uint32_t *text,
		uint32_t text_length,
		const struct text_theme *theme,
		uint32_t cursor_position,
		const struct cursor_theme *cursor_theme)
{
	cairo_font_extents_t font_extents;
	cairo_font_extents(cr, &font_extents);
	struct directional padding = theme->padding;

	struct color color = theme->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	hb_buffer_clear_contents(hb->hb_buffer);
	setup_hb_buffer(hb->hb_buffer);
	hb_buffer_add_utf32(hb->hb_buffer, text, -1, 0, -1);
	hb_shape(hb->hb_font, hb->hb_buffer, hb->hb_features, hb->num_features);
	cairo_text_extents_t extents = render_hb_buffer(cr, &hb->hb_font_extents, hb->hb_buffer, hb->scale);

	/*
	 * If the cursor is at the end of text, we need to account for it in
	 * both the size of the background and the returned extents.x_advance.
	 */
	double extra_cursor_advance = 0;
	if (cursor_position == text_length && cursor_theme->show) {
		switch (cursor_theme->style) {
			case CURSOR_STYLE_BAR:
				extra_cursor_advance = cursor_theme->thickness;
				break;
			case CURSOR_STYLE_BLOCK:
				extra_cursor_advance = cursor_theme->em_width;
				break;
			case CURSOR_STYLE_UNDERSCORE:
				extra_cursor_advance = cursor_theme->em_width;
				break;
		}
		extra_cursor_advance += extents.x_advance
			- extents.x_bearing
			- extents.width;
	}

	/* Draw the background if required. */
	if (theme->background_color.a != 0) {
		cairo_save(cr);
		color = theme->background_color;
		cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
		cairo_translate(
				cr,
				floor(-padding.left + extents.x_bearing),
				-padding.top);
		rounded_rectangle(
				cr,
				ceil(extents.width + extra_cursor_advance + padding.left + padding.right),
				ceil(font_extents.height + padding.top + padding.bottom),
				theme->background_corner_radius
				);
		cairo_fill(cr);
		cairo_restore(cr);

		color = theme->foreground_color;
		cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

		render_hb_buffer(cr, &hb->hb_font_extents, hb->hb_buffer, hb->scale);
	}

	if (!cursor_theme->show) {
		/* No cursor to draw, we're done. */
		return extents;
	}

	double cursor_x;
	double cursor_width;

	if (cursor_position == text_length) {
		/* Cursor is at the end of text, no calculations to be done. */
		cursor_x = extents.x_advance;
		cursor_width = cursor_theme->em_width;
	} else {
		/*
		 * If the cursor is within the text, there's a bit more to do.
		 *
		 * We need to walk through the drawn glyphs, advancing the
		 * cursor by the appropriate amount as we go. This is
		 * complicated by the potential presence of ligatures, which
		 * mean the cursor could be located part way through a single
		 * glyph.
		 *
		 * To determine the appropriate width for the block and
		 * underscore cursors, we then do the same thing again, this
		 * time stopping at the character after the cursor. The width
		 * is then the difference between these two positions.
		 */
		unsigned int glyph_count;
		hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(hb->hb_buffer, &glyph_count);
		hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(hb->hb_buffer, &glyph_count);
		int32_t cursor_start = 0;
		int32_t cursor_end = 0;
		for (size_t i = 0; i < glyph_count; i++) {
			uint32_t cluster = glyph_info[i].cluster;
			int32_t x_advance = glyph_pos[i].x_advance;
			uint32_t next_cluster = text_length;
			for (size_t j = i + 1; j < glyph_count; j++) {
				/*
				 * This needs to be a loop to account for
				 * multiple glyphs sharing the same cluster
				 * (e.g. diacritics).
				 */
				if (glyph_info[j].cluster > cluster) {
					next_cluster = glyph_info[j].cluster;
					break;
				}
			}
			if (next_cluster > cursor_position) {
				size_t glyph_clusters = next_cluster - cluster;
				if (glyph_clusters > 1) {
					uint32_t diff = cursor_position - cluster;
					cursor_start += diff * x_advance / glyph_clusters;
				}
				break;
			}
			cursor_start += x_advance;
		}
		for (size_t i = 0; i < glyph_count; i++) {
			uint32_t cluster = glyph_info[i].cluster;
			int32_t x_advance = glyph_pos[i].x_advance;
			uint32_t next_cluster = text_length;
			for (size_t j = i + 1; j < glyph_count; j++) {
				if (glyph_info[j].cluster > cluster) {
					next_cluster = glyph_info[j].cluster;
					break;
				}
			}
			if (next_cluster > cursor_position + 1) {
				size_t glyph_clusters = next_cluster - cluster;
				if (glyph_clusters > 1) {
					uint32_t diff = cursor_position + 1 - cluster;
					cursor_end += diff * x_advance / glyph_clusters;
				}
				break;
			}
			cursor_end += x_advance;
		}
		/* Convert from HarfBuzz 26.6 fixed-point to float. */
		cursor_x = cursor_start / 64.0 / hb->scale;
		cursor_width = (cursor_end - cursor_start) / 64.0 / hb->scale;
	}

	cairo_save(cr);
	color = cursor_theme->color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_translate(cr, cursor_x, 0);
	switch (cursor_theme->style) {
		case CURSOR_STYLE_BAR:
			rounded_rectangle(cr, cursor_theme->thickness, font_extents.height, cursor_theme->corner_radius);
			cairo_fill(cr);
			break;
		case CURSOR_STYLE_BLOCK:
			rounded_rectangle(cr, cursor_width, font_extents.height, cursor_theme->corner_radius);
			cairo_fill_preserve(cr);
			cairo_clip(cr);
			cairo_translate(cr, -cursor_x, 0);
			color = cursor_theme->text_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
			render_hb_buffer(cr, &hb->hb_font_extents, hb->hb_buffer, hb->scale);
			break;
		case CURSOR_STYLE_UNDERSCORE:
			cairo_translate(cr, 0, cursor_theme->underline_depth);
			rounded_rectangle(cr, cursor_width, cursor_theme->thickness, cursor_theme->corner_radius);
			cairo_fill(cr);
			break;
	}
	cairo_restore(cr);

	extents.x_advance += extra_cursor_advance;
	return extents;
}

static bool size_overflows(struct entry *entry, uint32_t width, uint32_t height)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	cairo_matrix_t mat;
	cairo_get_matrix(cr, &mat);
	if (entry->horizontal) {
		if (mat.x0 + width > entry->clip_x + entry->clip_width) {
			return true;
		}
	} else {
		if (mat.y0 + height > entry->clip_y + entry->clip_height) {
			return true;
		}
	}
	return false;
}

void entry_backend_harfbuzz_init(
		struct entry *entry,
		uint32_t *width,
		uint32_t *height)
{
	struct entry_backend_harfbuzz *hb = &entry->harfbuzz;
	cairo_t *cr = entry->cairo[0].cr;
	uint32_t font_size = floor(entry->font_size * PT_TO_DPI);
	cairo_surface_get_device_scale(entry->cairo[0].surface, &hb->scale, NULL);

	/*
	 * Setting up our font has three main steps:
	 *
	 * 1. Load the font face with FreeType.
	 * 2. Create a HarfBuzz font referencing the FreeType font.
	 * 3. Create a Cairo font referencing the FreeType font.
	 *
	 * The simultaneous interaction of Cairo and HarfBuzz with FreeType is
	 * a little finicky, so the order of the last two steps is important.
	 * We use HarfBuzz to set font variation settings (such as weight), if
	 * any. This modifies the underlying FreeType font, so we must create
	 * the Cairo font *after* this point for the changes to take effect.
	 *
	 * This doesn't seem like it should be necessary, as both HarfBuzz and
	 * Cairo reference the same FreeType font, but it is.
	 */

	/* Setup FreeType. */
	log_debug("Creating FreeType library.\n");
	int err;
	err = FT_Init_FreeType(&hb->ft_library);
	if (err) {
		log_error("Error initialising FreeType: %s\n",
				get_ft_error_string(err));
		exit(EXIT_FAILURE);
	}

	log_debug("Loading FreeType font.\n");
	err = FT_New_Face(
			hb->ft_library,
			entry->font_name,
			0,
			&hb->ft_face);
	if (err) {
		log_error("Error loading font: %s\n", get_ft_error_string(err));
		exit(EXIT_FAILURE);
	}

	err = FT_Set_Char_Size(
			hb->ft_face,
			font_size * 64,
			font_size * 64,
			0,
			0);
	if (err) {
		log_error("Error setting font size: %s\n",
				get_ft_error_string(err));
	}

	log_debug("Creating Harfbuzz font.\n");
	hb->hb_font = hb_ft_font_create_referenced(hb->ft_face);

	if (entry->font_variations[0] != 0) {
		log_debug("Parsing font variations.\n");
	}
	char *saveptr = NULL;
	char *variation = strtok_r(entry->font_variations, ",", &saveptr);
	while (variation != NULL && hb->num_variations < N_ELEM(hb->hb_variations)) {
		if (hb_variation_from_string(variation, -1, &hb->hb_variations[hb->num_variations])) {
			hb->num_variations++;
		} else {
			log_error("Failed to parse font variation \"%s\".\n", variation);
		}
		variation = strtok_r(NULL, ",", &saveptr);
	}

	/*
	 * We need to set variations now and update the underlying FreeType
	 * font, as Cairo will then use the FreeType font for drawing.
	 */
	hb_font_set_variations(hb->hb_font, hb->hb_variations, hb->num_variations);
#ifndef NO_HARFBUZZ_FONT_CHANGED
	hb_ft_hb_font_changed(hb->hb_font);
#endif

	if (entry->font_features[0] != 0) {
		log_debug("Parsing font features.\n");
	}
	saveptr = NULL;
	char *feature = strtok_r(entry->font_features, ",", &saveptr);
	while (feature != NULL && hb->num_features < N_ELEM(hb->hb_features)) {
		if (hb_feature_from_string(feature, -1, &hb->hb_features[hb->num_features])) {
			hb->num_features++;
		} else {
			log_error("Failed to parse font feature \"%s\".\n", feature);
		}
		feature = strtok_r(NULL, ",", &saveptr);
	}

	/* Get some font metrics used for rendering the cursor. */
	uint32_t m_codepoint;
	if (hb_font_get_glyph_from_name(hb->hb_font, "m", -1, &m_codepoint)) {
		entry->cursor_theme.em_width = hb_font_get_glyph_h_advance(hb->hb_font, m_codepoint) / 64.0;
	} else {
		/* If we somehow fail to get an m from the font, just guess. */
		entry->cursor_theme.em_width = font_size * 5.0 / 8.0;
	}
	hb_font_get_h_extents(hb->hb_font, &hb->hb_font_extents);
	int32_t underline_depth;
#ifdef NO_HARFBUZZ_METRIC_FALLBACK
	if (!hb_ot_metrics_get_position(
			hb->hb_font,
			HB_OT_METRICS_TAG_UNDERLINE_OFFSET,
			&underline_depth)) {
		underline_depth = -font_size * 64.0 / 18;
	}
#else
	hb_ot_metrics_get_position_with_fallback(
			hb->hb_font,
			HB_OT_METRICS_TAG_UNDERLINE_OFFSET,
			&underline_depth);
#endif
	entry->cursor_theme.underline_depth = (hb->hb_font_extents.ascender - underline_depth) / 64.0;

	if (entry->cursor_theme.style == CURSOR_STYLE_UNDERSCORE && !entry->cursor_theme.thickness_specified) {
		int32_t thickness;
#ifdef NO_HARFBUZZ_METRIC_FALLBACK
		if (!hb_ot_metrics_get_position(
				hb->hb_font,
				HB_OT_METRICS_TAG_UNDERLINE_SIZE,
				&thickness)) {
			thickness = font_size * 64.0 / 18;
		}
#else
		hb_ot_metrics_get_position_with_fallback(
				hb->hb_font,
				HB_OT_METRICS_TAG_UNDERLINE_SIZE,
				&thickness);
#endif
		entry->cursor_theme.thickness = thickness / 64.0;

	}

	hb->line_spacing =
		hb->hb_font_extents.ascender
		- hb->hb_font_extents.descender
		+ hb->hb_font_extents.line_gap;

	log_debug("Creating Harfbuzz buffer.\n");
	hb->hb_buffer = hb_buffer_create();

	log_debug("Creating Cairo font.\n");
	hb->cairo_face = cairo_ft_font_face_create_for_ft_face(hb->ft_face, 0);

	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_set_font_face(cr, hb->cairo_face);
	cairo_set_font_size(cr, font_size);
	cairo_font_options_t *opts = cairo_font_options_create();
	if (hb->disable_hinting) {
		cairo_font_options_set_hint_style(opts, CAIRO_HINT_STYLE_NONE);
	} else {
		cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_ON);
	}
	cairo_set_font_options(cr, opts);

	/* We also need to set up the font for our other Cairo context. */
	cairo_set_font_face(entry->cairo[1].cr, hb->cairo_face);
	cairo_set_font_size(entry->cairo[1].cr, font_size);
	cairo_set_font_options(entry->cairo[1].cr, opts);

	cairo_font_options_destroy(opts);

	/*
	 * This is really dumb, but if we don't call cairo_font_extents (or
	 * presumably some similar function) before rendering the text for the
	 * first time, font spacing is all messed up.
	 *
	 * This whole Harfbuzz - Cairo - FreeType thing is a bit finicky to
	 * set up.
	 */
	cairo_font_extents(cr, &hb->cairo_font_extents);

	/*
	 * Cairo changes the size of the font, which sometimes causes rendering
	 * of 'm' characters to mess up (as Harfbuzz has already it when we
	 * measured it). We therefore have to notify Harfbuzz of any potential
	 * changes here.
	 *
	 * In future, the recently-added hb-cairo interface would probably
	 * solve this issue.
	 */
	hb_ft_font_changed(hb->hb_font);
}

void entry_backend_harfbuzz_destroy(struct entry *entry)
{
	hb_buffer_destroy(entry->harfbuzz.hb_buffer);
	hb_font_destroy(entry->harfbuzz.hb_font);
	cairo_font_face_destroy(entry->harfbuzz.cairo_face);
	FT_Done_Face(entry->harfbuzz.ft_face);
	FT_Done_FreeType(entry->harfbuzz.ft_library);
}

void entry_backend_harfbuzz_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	cairo_text_extents_t extents;

	cairo_save(cr);

	/* Render the prompt */
	extents = render_text_themed(cr, entry, entry->prompt_text, &entry->prompt_theme);
	{
		struct entry_backend_harfbuzz *hb = &entry->harfbuzz;
		hb_buffer_clear_contents(hb->hb_buffer);
		setup_hb_buffer(hb->hb_buffer);
		hb_buffer_add_utf8(hb->hb_buffer, "test", -1, 0, -1);
		hb_shape(hb->hb_font, hb->hb_buffer, hb->hb_features, hb->num_features);
	}

	cairo_translate(cr, extents.x_advance, 0);
	cairo_translate(cr, entry->prompt_padding, 0);

	/* Render the entry text */
	if (entry->input_utf32_length == 0) {
		uint32_t *tmp = utf8_string_to_utf32_string(entry->placeholder_text);
		extents = render_input(
				cr,
				&entry->harfbuzz,
				tmp,
				utf32_strlen(tmp),
				&entry->placeholder_theme,
				0,
				&entry->cursor_theme);
		free(tmp);
	} else if (entry->hide_input) {
		size_t nchars = entry->input_utf32_length;
		uint32_t *buf = xcalloc(nchars + 1, sizeof(*entry->input_utf32));
		uint32_t ch = utf8_to_utf32(entry->hidden_character_utf8);
		for (size_t i = 0; i < nchars; i++) {
			buf[i] = ch;
		}
		buf[nchars] = U'\0';
		extents = render_input(
				cr,
				&entry->harfbuzz,
				buf,
				entry->input_utf32_length,
				&entry->input_theme,
				entry->cursor_position,
				&entry->cursor_theme);
		free(buf);
	} else {
		extents = render_input(
				cr,
				&entry->harfbuzz,
				entry->input_utf32,
				entry->input_utf32_length,
				&entry->input_theme,
				entry->cursor_position,
				&entry->cursor_theme);
	}
	extents.x_advance = MAX(extents.x_advance, entry->input_width);

	uint32_t num_results;
	if (entry->num_results == 0) {
		num_results = entry->results.count;
	} else {
		num_results = MIN(entry->num_results, entry->results.count);
	}
	/* Render our results */
	size_t i;
	int32_t vert_spacing = entry->result_vert_spacing;
	int32_t hori_spacing = entry->result_hori_spacing;
	for (i = 0; i < num_results; i++) {

		if (entry->horizontal) {
			cairo_translate(cr, extents.x_advance + hori_spacing, vert_spacing);
			if (i == 0){ hori_spacing = entry->result_spacing; vert_spacing = 0; }
		} else {
			cairo_translate(cr, hori_spacing, entry->harfbuzz.line_spacing / 64.0 + vert_spacing);
			if (i == 0){ hori_spacing = 0; vert_spacing = entry->result_spacing; }
		}
		if (entry->num_results == 0) {
			if (size_overflows(entry, 0, 0)) {
				break;
			}
		} else if (i >= entry->num_results) {
			break;
		}


		size_t index = i + entry->first_result;
		/*
		 * We may be on the last page, which could have fewer results
		 * than expected, so check and break if necessary.
		 */
		if (index >= entry->results.count) {
			break;
		}

		const char *result = entry->results.buf[index].string;
		/*
		 * If this isn't the selected result, or it is but we're not
		 * doing any fancy match-highlighting, just print as normal.
		 */
		if (i != entry->selection || (entry->selection_highlight_color.a == 0)) {
			const struct text_theme *theme;
			if (i == entry->selection) {
				theme = &entry->selection_theme;
			} else if (index % 2) {
				theme = &entry->alternate_result_theme;;
			} else {
				theme = &entry->default_result_theme;;
			}

			if (entry->num_results > 0) {
				/*
				 * We're not auto-detecting how many results we
				 * can fit, so just render the text.
				 */
				extents = render_text_themed(cr, entry, result, theme);
			} else if (!entry->horizontal) {
				/*
				 * The height of the text doesn't change, so
				 * we don't need to re-measure it each time.
				 */
				if (size_overflows(entry, 0, entry->harfbuzz.line_spacing / 64.0)) {
					break;
				} else {
					extents = render_text_themed(cr, entry, result, theme);
				}
			} else {
				/*
				 * The difficult case: we're auto-detecting how
				 * many results to draw, but we can't know
				 * whether this result will fit without
				 * drawing it! To solve this, draw to a
				 * temporary group, measure that, then copy it
				 * to the main canvas only if it will fit.
				 */
				cairo_push_group(cr);
				extents = render_text_themed(cr, entry, result, theme);

				cairo_pattern_t *group = cairo_pop_group(cr);
				if (size_overflows(entry, extents.x_advance, 0)) {
					cairo_pattern_destroy(group);
					break;
				} else {
					cairo_save(cr);
					cairo_set_source(cr, group);
					cairo_paint(cr);
					cairo_restore(cr);
					cairo_pattern_destroy(group);
				}
			}
		} else {
			/*
			 * For match highlighting, there's a bit more to do.
			 *
			 * We need to split the text into prematch, match and
			 * postmatch chunks, and draw each separately.
			 *
			 * However, we only want one background box around them
			 * all (if we're drawing one). To do this, we have to
			 * do the rendering part of render_text_themed()
			 * manually, with the same method of:
			 * - Draw the text and measure it
			 * - Draw the box
			 * - Draw the text again
			 *
			 * N.B. The size_overflows check isn't necessary here,
			 * as it's currently not possible for the selection to
			 * do so.
			 */
			size_t prematch_len;
			size_t postmatch_len;
			char *prematch = xstrdup(result);
			char *match = NULL;
			char *postmatch = NULL;
			if (entry->input_utf8_length > 0 && entry->selection_highlight_color.a != 0) {
				char *match_pos = utf8_strcasestr(prematch, entry->input_utf8);
				if (match_pos != NULL) {
					match = xstrdup(result);
					prematch_len = (match_pos - prematch);
					prematch[prematch_len] = '\0';
					postmatch_len = strlen(result) - prematch_len - entry->input_utf8_length;
					if (postmatch_len > 0) {
						postmatch = xstrdup(result);
					}
					match[entry->input_utf8_length + prematch_len] = '\0';
				}
			}

			for (int pass = 0; pass < 2; pass++) {
				cairo_save(cr);
				struct color color = entry->selection_theme.foreground_color;
				cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

				cairo_text_extents_t subextents = render_text(cr, &entry->harfbuzz, prematch);
				extents = subextents;

				if (match != NULL) {
					cairo_translate(cr, subextents.x_advance, 0);
					color = entry->selection_highlight_color;
					cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

					subextents = render_text(cr, &entry->harfbuzz, &match[prematch_len]);

					if (prematch_len == 0) {
						extents = subextents;
					} else {
						/*
						 * This calculation is a little
						 * complex, but it's basically:
						 *
						 * (distance from leftmost pixel of
						 * prematch to logical end of prematch)
						 * 
						 * +
						 *
						 * (distance from logical start of match
						 * to rightmost pixel of match).
						 */
						extents.width = extents.x_advance
							- extents.x_bearing
							+ subextents.x_bearing
							+ subextents.width;
						extents.x_advance += subextents.x_advance;
					}
				}

				if (postmatch != NULL) {
					cairo_translate(cr, subextents.x_advance, 0);
					color = entry->selection_theme.foreground_color;
					cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
					subextents = render_text(
							cr,
							&entry->harfbuzz,
							&postmatch[entry->input_utf8_length + prematch_len]);

					extents.width = extents.x_advance
						- extents.x_bearing
						+ subextents.x_bearing
						+ subextents.width;
					extents.x_advance += subextents.x_advance;
				}

				cairo_restore(cr);

				if (entry->selection_theme.background_color.a == 0) {
					/* No background box, we're done. */
					break;
				} else if (pass == 0) {
					/* 
					 * First pass, paint over the text with
					 * our background box.
					 */
					render_text_background(cr, entry, extents, &entry->selection_theme);
				}
			}

			free(prematch);
			if (match != NULL) {
				free(match);
			}
			if (postmatch != NULL) {
				free(postmatch);
			}
		}
	}
	entry->num_results_drawn = i;
	log_debug("Drew %zu results.\n", i);

	cairo_restore(cr);
}
