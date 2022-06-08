#include <cairo/cairo.h>
#include <glib.h>
#include <wchar.h>
#include "entry.h"
#include "log.h"
#include "nelem.h"

void entry_init(struct entry *entry, uint32_t width, uint32_t height, uint32_t scale)
{
	entry->image.width = width;
	entry->image.height = height;
	entry->image.scale = scale;

	width /= scale;
	height /= scale;

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
	
	entry->cairo.surface = surface;
	entry->cairo.cr = cr;

	/* Setup the backend. */
	entry_backend_init(entry, width, height, scale);

	entry->image.buffer = cairo_image_surface_get_data(surface);
}

void entry_destroy(struct entry *entry)
{
	entry_backend_destroy(entry);
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

	entry_backend_update(entry);

	cairo_restore(cr);
}

void entry_set_scale(struct entry *entry, uint32_t scale)
{
	entry->image.scale = scale;
	cairo_surface_set_device_scale(entry->cairo.surface, scale, scale);
}
