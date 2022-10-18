#include <cairo/cairo.h>
#include <harfbuzz/hb-ft.h>
#include <math.h>
#include "harfbuzz.h"
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"
#include "../utf8.h"
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
}


/*
 * Render a hb_buffer with Cairo, and return the width of the rendered area in
 * Cairo units.
 */
static cairo_text_extents_t render_hb_buffer(cairo_t *cr, hb_buffer_t *buffer)
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
		 */
		cairo_glyphs[i].index = glyph_info[i].codepoint;
		cairo_glyphs[i].x = x + glyph_pos[i].x_offset / 64.0;
		cairo_glyphs[i].y = y - glyph_pos[i].y_offset / 64.0;
		x += glyph_pos[i].x_advance / 64.0;
		y -= glyph_pos[i].y_advance / 64.0;
	}

	cairo_show_glyphs(cr, cairo_glyphs, glyph_count);

	cairo_text_extents_t extents;
	cairo_glyph_extents(cr, cairo_glyphs, glyph_count, &extents);

	/* Account for the shifted baseline in our returned text extents. */
	extents.y_bearing += font_extents.ascent;

	free(cairo_glyphs);

	cairo_restore(cr);

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
	cairo_t *cr = entry->cairo[0].cr;
	uint32_t font_size = floor(entry->font_size * PT_TO_DPI);

	/* Setup FreeType. */
	log_debug("Creating FreeType library.\n");
	int err;
	err = FT_Init_FreeType(&entry->harfbuzz.ft_library);
	if (err) {
		log_error("Error initialising FreeType: %s\n",
				get_ft_error_string(err));
		exit(EXIT_FAILURE);
	}

	log_debug("Loading FreeType font.\n");
	err = FT_New_Face(
			entry->harfbuzz.ft_library,
			entry->font_name,
			0,
			&entry->harfbuzz.ft_face);
	if (err) {
		log_error("Error loading font: %s\n", get_ft_error_string(err));
		exit(EXIT_FAILURE);
	}
	err = FT_Set_Char_Size(
			entry->harfbuzz.ft_face,
			font_size * 64,
			font_size * 64,
			0,
			0);
	if (err) {
		log_error("Error setting font size: %s\n",
				get_ft_error_string(err));
	}

	log_debug("Creating Cairo font.\n");
	entry->harfbuzz.cairo_face =
		cairo_ft_font_face_create_for_ft_face(entry->harfbuzz.ft_face, 0);

	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_set_font_face(cr, entry->harfbuzz.cairo_face);
	cairo_set_font_size(cr, font_size);
	cairo_font_options_t *opts = cairo_font_options_create();
	if (entry->harfbuzz.disable_hinting) {
		cairo_font_options_set_hint_style(opts, CAIRO_HINT_STYLE_NONE);
	} else {
		cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_ON);
	}
	cairo_set_font_options(cr, opts);


	/* We also need to set up the font for our other Cairo context. */
	cairo_set_font_face(entry->cairo[1].cr, entry->harfbuzz.cairo_face);
	cairo_set_font_size(entry->cairo[1].cr, font_size);
	cairo_set_font_options(entry->cairo[1].cr, opts);

	cairo_font_options_destroy(opts);

	log_debug("Creating Harfbuzz font.\n");
	entry->harfbuzz.hb_font =
		hb_ft_font_create_referenced(entry->harfbuzz.ft_face);

	log_debug("Creating Harfbuzz buffer.\n");
	hb_buffer_t *buffer = hb_buffer_create();
	entry->harfbuzz.hb_buffer = buffer;
	setup_hb_buffer(buffer);
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
	hb_buffer_t *buffer = entry->harfbuzz.hb_buffer;
	cairo_text_extents_t extents;

	cairo_save(cr);
	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	/* Render the prompt */
	hb_buffer_clear_contents(buffer);
	setup_hb_buffer(buffer);
	hb_buffer_add_utf8(entry->harfbuzz.hb_buffer, entry->prompt_text, -1, 0, -1);
	hb_shape(entry->harfbuzz.hb_font, entry->harfbuzz.hb_buffer, NULL, 0);
	extents = render_hb_buffer(cr, buffer);

	cairo_translate(cr, extents.x_advance, 0);
	cairo_translate(cr, entry->prompt_padding, 0);

	/* Render the entry text */
	hb_buffer_clear_contents(buffer);
	setup_hb_buffer(buffer);
	if (entry->hide_input) {
		size_t char_len = N_ELEM(entry->hidden_character_mb);
		for (size_t i = 0; i < entry->input_length; i++) {
			hb_buffer_add_utf8(buffer, entry->hidden_character_mb, char_len, 0, char_len);
		}
	} else {
		hb_buffer_add_utf8(buffer, entry->input_mb, -1, 0, -1);
	}
	hb_shape(entry->harfbuzz.hb_font, buffer, NULL, 0);
	extents = render_hb_buffer(cr, buffer);
	extents.x_advance = MAX(extents.x_advance, entry->input_width);

	cairo_font_extents_t font_extents;
	cairo_font_extents(cr, &font_extents);

	uint32_t num_results;
	if (entry->num_results == 0) {
		num_results = entry->results.count;
	} else {
		num_results = MIN(entry->num_results, entry->results.count);
	}
	/* Render our results */
	size_t i;
	for (i = 0; i < num_results; i++) {
		if (entry->horizontal) {
			cairo_translate(cr, extents.x_advance + entry->result_spacing, 0);
		} else {
			cairo_translate(cr, 0, font_extents.height + entry->result_spacing);
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
		 * doing any fancy match-highlighting or backgrounds, just
		 * print as normal.
		 */
		if (i != entry->selection
				|| (entry->selection_highlight_color.a == 0
					&& entry->selection_background_color.a == 0)) {
			if (i == entry->selection) {
				color = entry->selection_foreground_color;
			} else {
				color = entry->foreground_color;
			}
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

			hb_buffer_clear_contents(buffer);
			setup_hb_buffer(buffer);
			hb_buffer_add_utf8(buffer, result, -1, 0, -1);
			hb_shape(entry->harfbuzz.hb_font, buffer, NULL, 0);

			if (entry->num_results > 0) {
				/*
				 * We're not auto-detecting how many results we
				 * can fit, so just render the text.
				 */
				extents = render_hb_buffer(cr, buffer);
			} else if (!entry->horizontal) {
				/*
				 * The height of the text doesn't change, so
				 * we don't need to re-measure it each time.
				 */
				if (size_overflows(entry, 0, font_extents.height)) {
					entry->num_results_drawn = i;
					break;
				} else {
					extents = render_hb_buffer(cr, buffer);
				}
			} else {
				/*
				 * The difficult case: we're auto-detecting how
				 * many results to draw, but we can't know
				 * whether this results will fit without
				 * drawing it! To solve this, draw to a
				 * temporary group, measure that, then copy it
				 * to the main canvas only if it will fit.
				 */
				cairo_push_group(cr);
				extents = render_hb_buffer(cr, buffer);
				cairo_pattern_t *group = cairo_pop_group(cr);
				if (size_overflows(entry, extents.x_advance, 0)) {
					entry->num_results_drawn = i;
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
			 * For the selected result, there's a bit more to do.
			 *
			 * First, we need to use a different foreground color -
			 * simple enough.
			 *
			 * Next, we may need to draw a background box - this
			 * involves rendering to a cairo group, measuring the
			 * size of the text, drawing the background on the main
			 * canvas, then finally drawing the group on top of
			 * that.
			 *
			 * Finally, we may need to highlight the matching
			 * portion of text - this is achieved simply by
			 * splitting the text into prematch, match and
			 * postmatch chunks, and drawing each separately.
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
			cairo_text_extents_t subextents;
			if (entry->input_mb_length > 0 && entry->selection_highlight_color.a != 0) {
				char *match_pos = utf8_strcasestr(prematch, entry->input_mb);
				if (match_pos != NULL) {
					match = xstrdup(result);
					prematch_len = (match_pos - prematch);
					prematch[prematch_len] = '\0';
					postmatch_len = strlen(result) - prematch_len - entry->input_mb_length;
					if (postmatch_len > 0) {
						postmatch = xstrdup(result);
					}
					match[entry->input_mb_length + prematch_len] = '\0';
				}
			}

			cairo_push_group(cr);
			color = entry->selection_foreground_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

			hb_buffer_clear_contents(buffer);
			setup_hb_buffer(buffer);
			hb_buffer_add_utf8(buffer, prematch, -1, 0, -1);
			hb_shape(entry->harfbuzz.hb_font, buffer, NULL, 0);
			subextents = render_hb_buffer(cr, buffer);
			extents = subextents;

			free(prematch);
			prematch = NULL;

			if (match != NULL) {
				cairo_translate(cr, subextents.x_advance, 0);
				color = entry->selection_highlight_color;
				cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
				hb_buffer_clear_contents(buffer);
				setup_hb_buffer(buffer);
				hb_buffer_add_utf8(buffer, &match[prematch_len], -1, 0, -1);
				hb_shape(entry->harfbuzz.hb_font, buffer, NULL, 0);
				subextents = render_hb_buffer(cr, buffer);

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

				free(match);
				match = NULL;
			}

			if (postmatch != NULL) {
				cairo_translate(cr, subextents.x_advance, 0);
				color = entry->selection_foreground_color;
				cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
				hb_buffer_clear_contents(buffer);
				setup_hb_buffer(buffer);
				hb_buffer_add_utf8(buffer, &postmatch[entry->input_mb_length + prematch_len], -1, 0, -1);
				hb_shape(entry->harfbuzz.hb_font, buffer, NULL, 0);
				subextents = render_hb_buffer(cr, buffer);

				extents.width = extents.x_advance
					- extents.x_bearing
					+ subextents.x_bearing
					+ subextents.width;
				extents.x_advance += subextents.x_advance;


				free(postmatch);
				postmatch = NULL;
			}

			cairo_pop_group_to_source(cr);
			cairo_save(cr);
			color = entry->selection_background_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
			int32_t pad = entry->selection_background_padding;
			if (pad < 0) {
				pad = entry->clip_width;
			}
			cairo_translate(cr, floor(-pad + extents.x_bearing), 0);
			cairo_rectangle(cr, 0, 0, ceil(extents.width + pad * 2), ceil(font_extents.height));
			cairo_fill(cr);
			cairo_restore(cr);
			cairo_paint(cr);
		}
	}
	entry->num_results_drawn = i;
	log_debug("Drew %zu results.\n", i);

	cairo_restore(cr);
}
