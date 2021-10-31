#include <arpa/inet.h>
#include <cairo/cairo.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <pango/pango.h>
#include <wchar.h>
#include "client.h"
#include "entry.h"
#include "log.h"
#include "nelem.h"

static void calculate_font_extents(struct entry *entry, uint32_t scale);

void entry_init(struct entry *entry, uint32_t scale)
{
	calculate_font_extents(entry, scale);
	struct color color;

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
			entry->surface.width,
			entry->surface.height
			);
	cairo_surface_set_device_scale(surface, scale, scale);
	cairo_t *cr = cairo_create(surface);

	/* Running size of current drawing area. */
	int32_t width = entry->surface.width / scale;
	int32_t height = entry->surface.height / scale;

	/* Draw the outer outline */
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

	/*
	 * Move the cursor back up, so that Pango draws in the correct place if
	 * we're doing a tight layout.
	 */
	cairo_translate(cr, -entry->text_bounds.x, -entry->text_bounds.y);

	/* Setup Pango. */
	PangoContext *context = pango_cairo_create_context(cr);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_text(layout, "", -1);

	PangoFontDescription *font_description =
		pango_font_description_from_string(entry->font_name);
	pango_font_description_set_size(
			font_description,
			entry->font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, font_description);
	pango_font_description_free(font_description);

	entry->pangocairo.surface = surface;
	entry->pangocairo.cr = cr;
	entry->pangocairo.layout = layout;
	entry->image.width = entry->surface.width;
	entry->image.height = entry->surface.height;
	entry->image.buffer = cairo_image_surface_get_data(surface);
}

void entry_update(struct entry *entry)
{
	cairo_t *cr = entry->pangocairo.cr;
	PangoLayout *layout = entry->pangocairo.layout;

	/* Redraw the background. */
	struct color color = entry->background_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	cairo_paint(cr);

	/* Draw our text with Pango. */
	color = entry->foreground_color;
	cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
	size_t len = 0;
	for (unsigned int i = 0; i < entry->password_length; i++) {
		len += wcrtomb(entry->password_mb_print + len, entry->password_character, NULL);
	}
	entry->password_mb_print[len] = '\0';
	pango_layout_set_text(layout, entry->password_mb_print, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	entry->image.redraw = true;
}

void entry_set_scale(struct entry *entry, uint32_t scale)
{
	cairo_surface_set_device_scale(entry->pangocairo.surface, scale, scale);
}

void calculate_font_extents(struct entry *entry, uint32_t scale)
{
	/*
	 * To calculate the size of the password box, we do the following:
	 * 	1. Load the font we're going to use.
	 * 	2. Create a string of the desired length using the specified
	 * 	   password character, e.g. "·······".
	 * 	3. Render the string with Pango in some abstract layout.
	 * 	4. Measure the bounding box of the layout.
	 * 	5. Add on the size of the border / outline.
	 */
	PangoFontMap *font_map = pango_cairo_font_map_get_default();
	PangoContext *context = pango_font_map_create_context(font_map);

	PangoFontDescription *font_description =
		pango_font_description_from_string(entry->font_name);
	pango_font_description_set_size(
			font_description,
			entry->font_size * PANGO_SCALE);
	pango_context_set_font_description(context, font_description);

#ifdef DEBUG
	{
		PangoFont *font =
			pango_context_load_font(context, font_description);
		PangoFontDescription *desc = pango_font_describe(font);
		log_debug("Using font: %s\n",
			pango_font_description_to_string(desc));

		pango_font_description_free(desc);
		g_object_unref(font);
	}
#endif
	pango_font_description_free(font_description);

	PangoLayout *layout = pango_layout_new(context);
	char *buf = calloc(MAX_PASSWORD_LENGTH, 4);
	size_t len = 0;
	for (unsigned int i = 0; i < entry->num_characters; i++) {
		len += wcrtomb(buf + len, entry->password_character, NULL);
	}
	buf[len] = '\0';
	pango_layout_set_text(layout, buf, -1);
	free(buf);

	PangoRectangle rect;
	if (entry->tight_layout) {
		pango_layout_get_pixel_extents(layout, &rect, NULL);
	} else {
		pango_layout_get_pixel_extents(layout, NULL, &rect);
	}
	/*
	 * TODO: This extra 1px padding is needed for certain fonts, why?
	 */
	rect.width += 2;
	rect.height += 2;
	rect.x -= 1;
	rect.y -= 1;

	int width = rect.width;
	int height = rect.height;
	width += 2 * (
			entry->border.width
			+ 2 * entry->border.outline_width
			+ entry->padding
		     );
	height += 2 * (
			entry->border.width
			+ 2 * entry->border.outline_width
			+ entry->padding
		     );
	entry->surface.width = width * scale;
	entry->surface.height = height * scale;
	entry->text_bounds = rect;

	g_object_unref(layout);
	g_object_unref(context);
}
