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
	log_debug("%s\n", entry->font_name);
	PangoFontDescription *font_description =
		pango_font_description_from_string(entry->font_name);
	pango_font_description_set_size(
			font_description,
			entry->font_size * PANGO_SCALE);
	pango_context_set_font_description(context, font_description);
	pango_font_description_free(font_description);

	entry->pango.layout = pango_layout_new(context);
	log_debug("Setting Pango text.\n");
	pango_layout_set_text(entry->pango.layout, entry->prompt_text, -1);
	log_debug("First Pango draw.\n");
	pango_cairo_update_layout(cr, entry->pango.layout);

	/* Draw the prompt now, as this only needs to be done once */
	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	pango_cairo_show_layout(cr, entry->pango.layout);

	/* Move and clip so we don't draw over the prompt */
	int prompt_width;
	pango_layout_get_pixel_size(entry->pango.layout, &prompt_width, NULL);
	cairo_translate(cr, prompt_width, 0);
	*width -= prompt_width;
	cairo_rectangle(cr, 0, 0, *width, *height);
	cairo_clip(cr);

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

	pango_layout_set_text(layout, entry->input_mb, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	int width;
	int height;
	pango_layout_get_size(layout, &width, &height);
	width = MAX(width, (int)entry->input_width * PANGO_SCALE);

	for (size_t i = 0; i < entry->num_results && i < entry->results.count; i++) {
		if (entry->horizontal) {
			cairo_translate(cr, (int)(width / PANGO_SCALE) + entry->result_spacing, 0);
		} else {
			cairo_translate(cr, 0, (int)(height / PANGO_SCALE) + entry->result_spacing);
		}
		const char *str;
		if (i < entry->results.count) {
			str = entry->results.buf[i];
		} else {
			str = "";
		}
		pango_layout_set_text(layout, str, -1);
		pango_cairo_update_layout(cr, layout);
		if (i == entry->selection) {
			cairo_save(cr);
			struct color color = entry->selection_color;
			cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
		}
		pango_cairo_show_layout(cr, layout);
		if (i == entry->selection) {
			cairo_restore(cr);
		}
		pango_layout_get_size(layout, &width, &height);
	}
}
