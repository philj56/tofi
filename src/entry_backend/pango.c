#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"
#include "../unicode.h"
#include "../xmalloc.h"

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

static void render_text_themed(
		cairo_t *cr,
		PangoLayout *layout,
		const char *text,
		const struct text_theme *theme,
		PangoRectangle *ink_rect,
		PangoRectangle *logical_rect)
{
	struct color color = theme->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	pango_layout_set_text(layout, text, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	pango_layout_get_pixel_extents(layout, ink_rect, logical_rect);

	if (theme->background_color.a == 0) {
		/* No background to draw, we're done. */
		return;
	}

	struct directional padding = theme->padding;

	cairo_save(cr);
	color = theme->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_translate(
			cr,
			floor(-padding.left + ink_rect->x),
			-padding.top);
	rounded_rectangle(
			cr,
			ceil(ink_rect->width + padding.left + padding.right),
			ceil(logical_rect->height + padding.top + padding.bottom),
			theme->background_corner_radius
			);
	cairo_fill(cr);
	cairo_restore(cr);

	color = theme->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	pango_cairo_show_layout(cr, layout);
}

void entry_backend_pango_init(struct entry *entry, uint32_t *width, uint32_t *height)
{
	cairo_t *cr = entry->cairo[0].cr;

	/* Setup Pango. */
	log_debug("Creating Pango context.\n");
	PangoContext *context = pango_cairo_create_context(cr);

	log_debug("Creating Pango font description.\n");
	PangoFontDescription *font_description =
		pango_font_description_from_string(entry->font_name);
	pango_font_description_set_size(
			font_description,
			entry->font_size * PANGO_SCALE);
	if (entry->font_variations[0] != 0) {
		pango_font_description_set_variations(
				font_description,
				entry->font_variations);
	}
	pango_context_set_font_description(context, font_description);
	pango_font_description_free(font_description);

	entry->pango.layout = pango_layout_new(context);

	if (entry->font_features[0] != 0) {
		log_debug("Setting font features.\n");
		PangoAttribute *attr = pango_attr_font_features_new(entry->font_features);
		PangoAttrList *attr_list = pango_attr_list_new();
		pango_attr_list_insert(attr_list, attr);
		pango_layout_set_attributes(entry->pango.layout, attr_list);
	}


	entry->pango.context = context;
}

void entry_backend_pango_destroy(struct entry *entry)
{
	g_object_unref(entry->pango.layout);
	g_object_unref(entry->pango.context);
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

/*
 * This is pretty much a direct translation of the corresponding function in
 * the harfbuzz backend. As that's the one that I care about most, there are
 * more explanatory comments than there are here, so go look at that if you
 * want to understand how tofi's text rendering works.
 */
void entry_backend_pango_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	PangoLayout *layout = entry->pango.layout;

	cairo_save(cr);
	struct color color = entry->foreground_color;

	/* Render the prompt */
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;
	render_text_themed(cr, layout, entry->prompt_text, &entry->prompt_theme, &ink_rect, &logical_rect);

	cairo_translate(cr, logical_rect.width + logical_rect.x, 0);
	cairo_translate(cr, entry->prompt_padding, 0);

	/* Render the entry text */
	if (entry->input_utf8_length == 0) {
		render_text_themed(cr, layout, entry->placeholder_text, &entry->placeholder_theme, &ink_rect, &logical_rect);
	} else if (entry->hide_input) {
		size_t nchars = entry->input_utf32_length;
		size_t char_size = entry->hidden_character_utf8_length;
		char *buf = xmalloc(1 + nchars * char_size);
		for (size_t i = 0; i < nchars; i++) {
			for (size_t j = 0; j < char_size; j++) {
				buf[i * char_size + j] = entry->hidden_character_utf8[j];
			}
		}
		buf[char_size * nchars] = '\0';

		render_text_themed(cr, layout, buf, &entry->placeholder_theme, &ink_rect, &logical_rect);
		free(buf);
	} else {
		render_text_themed(cr, layout, entry->input_utf8, &entry->input_theme, &ink_rect, &logical_rect);
	}
	logical_rect.width = MAX(logical_rect.width, (int)entry->input_width);

	color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

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
			cairo_translate(cr, logical_rect.x + logical_rect.width + entry->result_spacing, 0);
		} else {
			cairo_translate(cr, 0, logical_rect.height + entry->result_spacing);
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

		const char *str;
		if (i < entry->results.count) {
			str = entry->results.buf[index].string;
		} else {
			str = "";
		}
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
				render_text_themed(cr, layout, str, theme, &ink_rect, &logical_rect);
			} else if (!entry->horizontal) {
				if (size_overflows(entry, 0, logical_rect.height)) {
					entry->num_results_drawn = i;
					break;
				} else {
					render_text_themed(cr, layout, str, theme, &ink_rect, &logical_rect);
				}
			} else {
				cairo_push_group(cr);
				render_text_themed(cr, layout, str, theme, &ink_rect, &logical_rect);

				cairo_pattern_t *group = cairo_pop_group(cr);
				if (size_overflows(entry, logical_rect.width, 0)) {
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
			ssize_t prematch_len = -1;
			ssize_t postmatch_len = -1;
			size_t match_len = entry->input_utf8_length;
			PangoRectangle ink_subrect;
			PangoRectangle logical_subrect;
			if (entry->input_utf8_length > 0 && entry->selection_highlight_color.a != 0) {
				char *match_pos = utf8_strcasestr(str, entry->input_utf8);
				if (match_pos != NULL) {
					prematch_len = (match_pos - str);
					postmatch_len = strlen(str) - prematch_len - match_len;
					if (postmatch_len <= 0) {
						postmatch_len = -1;
					}
				}
			}

			for (int pass = 0; pass < 2; pass++) {
				cairo_save(cr);
				color = entry->selection_theme.foreground_color;
				cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

				pango_layout_set_text(layout, str, prematch_len);
				pango_cairo_update_layout(cr, layout);
				pango_cairo_show_layout(cr, layout);
				pango_layout_get_pixel_extents(entry->pango.layout, &ink_subrect, &logical_subrect);
				ink_rect = ink_subrect;
				logical_rect = logical_subrect;

				if (prematch_len != -1) {
					cairo_translate(cr, logical_subrect.x + logical_subrect.width, 0);
					color = entry->selection_highlight_color;
					cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
					pango_layout_set_text(layout, &str[prematch_len], match_len);
					pango_cairo_update_layout(cr, layout);
					pango_cairo_show_layout(cr, layout);
					pango_layout_get_pixel_extents(entry->pango.layout, &ink_subrect, &logical_subrect);
					if (prematch_len == 0) {
						ink_rect = ink_subrect;
						logical_rect = logical_subrect;
					} else {
						ink_rect.width = logical_rect.width
							- ink_rect.x
							+ ink_subrect.x
							+ ink_subrect.width;
						logical_rect.width += logical_subrect.x + logical_subrect.width;
					}
				}

				if (postmatch_len != -1) {
					cairo_translate(cr, logical_subrect.x + logical_subrect.width, 0);
					color = entry->selection_theme.foreground_color;
					cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
					pango_layout_set_text(layout, &str[prematch_len + match_len], -1);
					pango_cairo_update_layout(cr, layout);
					pango_cairo_show_layout(cr, layout);
					pango_layout_get_pixel_extents(entry->pango.layout, &ink_subrect, &logical_subrect);
					ink_rect.width = logical_rect.width
						- ink_rect.x
						+ ink_subrect.x
						+ ink_subrect.width;
					logical_rect.width += logical_subrect.x + logical_subrect.width;

				}

				cairo_restore(cr);

				if (entry->selection_theme.background_color.a == 0) {
					break;
				} else if (pass == 0) {
					struct directional padding = entry->selection_theme.padding;
					cairo_save(cr);
					color = entry->selection_theme.background_color;
					cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
					cairo_translate(
							cr,
							floor(-padding.left + ink_rect.x),
							-padding.top);
					rounded_rectangle(
							cr,
							ceil(ink_rect.width + padding.left + padding.right),
							ceil(logical_rect.height + padding.top + padding.bottom),
							entry->selection_theme.background_corner_radius
							);
					cairo_fill(cr);
					cairo_restore(cr);
				}
			}
		}
	}
	entry->num_results_drawn = i;
	log_debug("Drew %zu results.\n", i);

	cairo_restore(cr);
}
