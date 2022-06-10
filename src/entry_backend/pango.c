#include <cairo/cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <wchar.h>
#include "../entry.h"
#include "../log.h"
#include "../nelem.h"

void entry_backend_init(struct entry *entry, uint32_t *width, uint32_t *height, uint32_t scale)
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

	entry->backend.layout = pango_layout_new(context);
	log_debug("Setting Pango text.\n");
	pango_layout_set_text(entry->backend.layout, "run: ", -1);
	int prompt_width;
	int prompt_height;
	log_debug("Get Pango pixel size.\n");
	pango_layout_get_pixel_size(entry->backend.layout, &prompt_width, &prompt_height);
	log_debug("First Pango draw.\n");
	pango_cairo_update_layout(cr, entry->backend.layout);

	/* Draw the prompt now, as this only needs to be done once */
	struct color color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	pango_cairo_show_layout(cr, entry->backend.layout);

	/* Move and clip so we don't draw over the prompt */
	cairo_translate(cr, prompt_width, 0);
	*width -= prompt_width;
	cairo_rectangle(cr, 0, 0, *width, *height);
	cairo_clip(cr);

	entry->backend.context = context;
}

void entry_backend_destroy(struct entry *entry)
{
	g_object_unref(entry->backend.layout);
	g_object_unref(entry->backend.context);
}

void entry_backend_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo[entry->index].cr;
	PangoLayout *layout = entry->backend.layout;

	pango_layout_set_text(layout, entry->input_mb, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	int width;
	int height;
	pango_layout_get_size(entry->backend.layout, &width, &height);

	for (size_t i = 0; i < 5; i++) {
		cairo_translate(cr, 0, (int)(height / PANGO_SCALE));
		const char *str;
		if (i < entry->results.count) {
			str = entry->results.buf[i];
		} else {
			str = "";
		}
		pango_layout_set_text(layout, str, -1);
		pango_cairo_update_layout(cr, layout);
		pango_cairo_show_layout(cr, layout);
	}
}
