#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
	pango_context_set_font_description(context, font_description);
	pango_font_description_free(font_description);

	entry->pango.layout = pango_layout_new(context);

	entry->pango.context = context;
}

void entry_backend_pango_destroy(struct entry *entry)
{
	g_object_unref(entry->pango.layout);
	g_object_unref(entry->pango.context);
}

void entry_backend_pango_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	PangoLayout *layout = entry->pango.layout;

	cairo_save(cr);
	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	/* Render the prompt */
	pango_layout_set_text(layout, entry->prompt_text, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	int width;
	int height;
	pango_layout_get_pixel_size(entry->pango.layout, &width, NULL);
	cairo_translate(cr, width, 0);


	/* Render the entry text */
	pango_layout_set_text(layout, entry->input_mb, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
	pango_layout_get_size(layout, &width, &height);
	width = MAX(width, (int)entry->input_width * PANGO_SCALE);

	cairo_matrix_t mat;
	/* Render our results */
	for (size_t i = 0; i < entry->results.count; i++) {
		if (entry->horizontal) {
			cairo_translate(cr, (int)(width / PANGO_SCALE) + entry->result_spacing, 0);
		} else {
			cairo_translate(cr, 0, (int)(height / PANGO_SCALE) + entry->result_spacing);
		}
		if (entry->num_results == 0) {
			cairo_get_matrix(cr, &mat);
			if (entry->horizontal) {
				if (mat.x0 > entry->clip_x + entry->clip_width) {
					entry->num_results_drawn = i;
					log_debug("Drew %zu results.\n", i);
					break;
				}
			} else {
				if (mat.y0 > entry->clip_y + entry->clip_height) {
					entry->num_results_drawn = i;
					log_debug("Drew %zu results.\n", i);
					break;
				}
			}
		} else if (i >= entry->num_results) {
			break;
		}
		const char *str;
		if (i < entry->results.count) {
			str = entry->results.buf[i].string;
		} else {
			str = "";
		}
		pango_layout_set_text(layout, str, -1);
		pango_cairo_update_layout(cr, layout);
		if (i == entry->selection) {
			cairo_push_group(cr);
			color = entry->selection_foreground_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
		}
		pango_cairo_show_layout(cr, layout);
		pango_layout_get_size(layout, &width, &height);
		if (i == entry->selection) {
			cairo_pop_group_to_source(cr);
			cairo_save(cr);
			color = entry->selection_background_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
			uint32_t pad = entry->selection_background_padding;
			cairo_translate(cr, -pad, 0);
			cairo_rectangle(cr, 0, 0, (int)(width / PANGO_SCALE) + pad * 2, (int)(height / PANGO_SCALE));
			cairo_translate(cr, pad, 0);
			cairo_fill(cr);
			cairo_restore(cr);
			cairo_paint(cr);
			color = entry->foreground_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
		}
	}

	cairo_restore(cr);
}
