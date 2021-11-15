#include <arpa/inet.h>
#include <cairo/cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <wchar.h>
#include "entry.h"
#include "log.h"
#include "nelem.h"

void entry_init(struct entry *entry, uint32_t width, uint32_t height, uint32_t scale)
{
	entry->image.width = width;
	entry->image.height = height;

	width /= scale;
	height /= scale;

	/*
	 * Cairo uses native 32 bit integers for pixels, so if this processor
	 * is little endian we have to tell OpenGL to swizzle the texture.
	 */
	if (htonl(0xFFu) != 0xFFu) {
		entry->image.swizzle = true;
	}

	/*
	 * Create the cairo surface and context we'll be using.
	 */
	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32,
			width * scale,
			height * scale
			);
	cairo_surface_set_device_scale(surface, scale, scale);
	cairo_t *cr = cairo_create(surface);

	/* Draw the outer outline */
	struct color color = entry->border.outline_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Move and clip following draws to be within this outline */
	cairo_translate(
			cr,
			entry->border.outline_width,
			entry->border.outline_width);
	width -= 2 * entry->border.outline_width;
	height -= 2 * entry->border.outline_width;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	/* Draw the border */
	color = entry->border.color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Move and clip following draws to be within the border */
	cairo_translate(cr, entry->border.width, entry->border.width);
	width -= 2 * entry->border.width;
	height -= 2 * entry->border.width;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	/* Draw the inner outline */
	color = entry->border.outline_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Move and clip following draws to be within this outline */
	cairo_translate(
			cr,
			entry->border.outline_width,
			entry->border.outline_width);
	width -= 2 * entry->border.outline_width;
	height -= 2 * entry->border.outline_width;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	/* Draw the entry background */
	color = entry->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Move and clip following draws to be within the specified padding */
	cairo_translate(cr, entry->padding, entry->padding);
	width -= 2 * entry->padding;
	height -= 2 * entry->padding;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	/* Setup Pango. */
	PangoContext *context = pango_cairo_create_context(cr);

	PangoFontDescription *font_description =
		pango_font_description_from_string(entry->font_name);
	pango_font_description_set_size(
			font_description,
			entry->font_size * PANGO_SCALE);
	pango_context_set_font_description(context, font_description);
	pango_font_description_free(font_description);

	entry->pango.prompt_layout = pango_layout_new(context);
	pango_layout_set_text(entry->pango.prompt_layout, "run: ", -1);
	int prompt_width;
	int prompt_height;
	pango_layout_get_pixel_size(entry->pango.prompt_layout, &prompt_width, &prompt_height);
	pango_cairo_update_layout(cr, entry->pango.prompt_layout);

	/* Draw the prompt now, as this only needs to be done once */
	color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	pango_cairo_show_layout(cr, entry->pango.prompt_layout);

	/* Move and clip so we don't draw over the prompt */
	cairo_translate(cr, prompt_width, 0);
	width -= prompt_width;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	entry->pango.entry_layout = pango_layout_new(context);
	pango_layout_set_text(entry->pango.entry_layout, "", -1);

	for (size_t i = 0; i < 5; i++) {
		PangoLayout *layout = pango_layout_new(context);
		pango_layout_set_text(layout, "", -1);
		entry->pango.result_layouts[i] = layout;
	}

	entry->pango.context = context;
	
	entry->cairo.surface = surface;
	entry->cairo.cr = cr;

	entry->image.buffer = cairo_image_surface_get_data(surface);
}

void entry_destroy(struct entry *entry)
{
	for (size_t i = 0; i < 5; i++) {
		g_object_unref(entry->pango.result_layouts[i]);
	}
	g_object_unref(entry->pango.entry_layout);
	g_object_unref(entry->pango.prompt_layout);
	g_object_unref(entry->pango.context);
	cairo_destroy(entry->cairo.cr);
	cairo_surface_destroy(entry->cairo.surface);
}

void entry_update(struct entry *entry)
{
	cairo_t *cr = entry->cairo.cr;
	cairo_save(cr);

	entry->image.redraw = true;

	/* Clear the image. */
	struct color color = entry->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Draw our text. */
	color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	pango_layout_set_text(entry->pango.entry_layout, entry->input_mb, -1);
	pango_cairo_update_layout(cr, entry->pango.entry_layout);
	pango_cairo_show_layout(cr, entry->pango.entry_layout);

	for (size_t i = 0; i < 5; i++) {
		cairo_translate(cr, 0, 50);
		PangoLayout *layout = entry->pango.result_layouts[i];
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
	cairo_restore(cr);
}

void entry_set_scale(struct entry *entry, uint32_t scale)
{
	cairo_surface_set_device_scale(entry->cairo.surface, scale, scale);
}
