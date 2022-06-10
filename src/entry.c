#include <cairo/cairo.h>
#include <wchar.h>
#include "entry.h"
#include "log.h"
#include "nelem.h"

void entry_init(struct entry *entry, uint8_t *restrict buffer, uint32_t width, uint32_t height, uint32_t scale)
{
	entry->image.width = width;
	entry->image.height = height;
	entry->image.scale = scale;

	/*
	 * Create the cairo surfaces and contexts we'll be using.
	 *
	 * In order to avoid an unnecessary copy when passing the image to the
	 * Wayland server, we accept a pointer to the mmap-ed file that our
	 * Wayland buffers are created from. This is assumed to be
	 * (width * height * (sizeof(uint32_t) == 4) * 2) bytes,
	 * to allow for double buffering.
	 */
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
			buffer,
			CAIRO_FORMAT_ARGB32,
			width,
			height,
			width * sizeof(uint32_t)
			);
	cairo_surface_set_device_scale(surface, scale, scale);
	cairo_t *cr = cairo_create(surface);

	entry->cairo[0].surface = surface;
	entry->cairo[0].cr = cr;

	entry->cairo[1].surface = cairo_image_surface_create_for_data(
			&buffer[width * height * sizeof(uint32_t)],
			CAIRO_FORMAT_ARGB32,
			width,
			height,
			width * sizeof(uint32_t)
			);
	cairo_surface_set_device_scale(entry->cairo[1].surface, scale, scale);
	entry->cairo[1].cr = cairo_create(entry->cairo[1].surface);

	/*
	 * Cairo drawing operations take the scale into account,
	 * so we account for that here.
	 */
	width /= scale;
	height /= scale;

	/* Draw the outer outline */
	struct color color = entry->border.outline_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_set_line_width(cr, 2 * entry->border.outline_width);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_stroke(cr);


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
	cairo_set_line_width(cr, 2 * entry->border.width);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_stroke(cr);

	/* Move and clip following draws to be within the border */
	cairo_translate(cr, entry->border.width, entry->border.width);
	width -= 2 * entry->border.width;
	height -= 2 * entry->border.width;
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_clip(cr);

	/* Draw the inner outline */
	color = entry->border.outline_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_set_line_width(cr, 2 * entry->border.outline_width);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_stroke(cr);

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

	/* Setup the backend. */
	entry_backend_init(entry, &width, &height, scale);

	/*
	 * To avoid performing all this drawing twice, we take a small
	 * shortcut. After performing all the drawing as normal on our first
	 * Cairo context, we can copy over just the important state (the
	 * transformation matrix and clip rectangle) and perform a memcpy()
	 * to initialise the other context.
	 *
	 * This memcpy can pretty expensive however, and isn't needed until
	 * we need to draw our second buffer (i.e. when the user presses a
	 * key). In order to minimise startup time, the memcpy() isn't
	 * performed here, but instead happens later, just after the first
	 * frame has been displayed on screen (and while the user is unlikely
	 * to press another key for the <10ms it takes to memcpy).
	 */
	cairo_matrix_t mat;
	cairo_get_matrix(cr, &mat);
	cairo_set_matrix(entry->cairo[1].cr, &mat);
	cairo_rectangle(entry->cairo[1].cr, 0, 0, width, height);
	cairo_clip(entry->cairo[1].cr);
}

void entry_destroy(struct entry *entry)
{
	entry_backend_destroy(entry);
	cairo_destroy(entry->cairo[0].cr);
	cairo_destroy(entry->cairo[1].cr);
	cairo_surface_destroy(entry->cairo[0].surface);
	cairo_surface_destroy(entry->cairo[1].surface);
}

void entry_update(struct entry *entry)
{
	log_debug("Start rendering entry.\n");
	cairo_t *cr = entry->cairo[entry->index].cr;
	cairo_save(cr);

	/* Clear the image. */
	struct color color = entry->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Draw our text. */
	color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

	entry_backend_update(entry);

	cairo_restore(cr);
	log_debug("Finish rendering entry.\n");

	entry->index = !entry->index;
}

void entry_set_scale(struct entry *entry, uint32_t scale)
{
	entry->image.scale = scale;
	cairo_surface_set_device_scale(entry->cairo[0].surface, scale, scale);
	cairo_surface_set_device_scale(entry->cairo[1].surface, scale, scale);
}
