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

static void calculate_font_extents(struct entry *entry);

void entry_init(struct entry *entry)
{
	calculate_font_extents(entry);

	/* 
	 * Cairo uses native 32 bit integers for pixels, so if this computer is
	 * little endian we have to tell OpenGL to swizzle the texture.
	 */
	if (htonl(0xFFu) != 0xFFu) {
		entry->image.swizzle = true;
	}

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32,
			entry->surface.width,
			entry->surface.height
			);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgb(cr, 0.031, 0.031, 0);
	cairo_paint(cr);

	cairo_translate(cr, entry->border.outline_width, entry->border.outline_width);
	cairo_rectangle(cr,
		0,
		0,
		entry->surface.width - 2*entry->border.outline_width,
		entry->surface.height - 2*entry->border.outline_width
		);
	cairo_clip(cr);
	cairo_set_source_rgb(cr, 0.976, 0.149, 0.447);
	cairo_paint(cr);

	cairo_translate(cr, entry->border.width, entry->border.width);
	cairo_rectangle(cr,
		0,
		0,
		entry->surface.width - 2*(entry->border.outline_width + entry->border.width),
		entry->surface.height - 2*(entry->border.outline_width + entry->border.width)
		);
	cairo_clip(cr);
	cairo_set_source_rgb(cr, 0.106, 0.114, 0.118);
	cairo_paint(cr);

	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, "", -1);

	PangoFontDescription *font_description = pango_font_description_from_string("Rubik Bold 48");
	pango_layout_set_font_description(layout, font_description);
	pango_font_description_free(font_description);

	cairo_set_source_rgb(cr, 0.973, 0.973, 0.941);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

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
	cairo_set_source_rgb(cr, 0.106, 0.114, 0.118);
	cairo_paint(cr);
	cairo_set_source_rgb(cr, 0.973, 0.973, 0.941);
	//const wchar_t *src = entry->password;
	//wcsrtombs(entry->password_mb, &src, N_ELEM(entry->password_mb), NULL);
	for (unsigned int i = 0; i < entry->password_length; i++) {
		entry->password_mb[2 * i] = '\xC2';
		entry->password_mb[2 * i + 1] = '\xB7';
	}
	entry->password_mb[2 * entry->password_length] = '\0';
	fprintf(stderr, "%s\n", entry->password_mb);
	pango_layout_set_text(layout, entry->password_mb, -1);
	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
	entry->image.redraw = true;
}

void calculate_font_extents(struct entry *entry)
{
	PangoFontMap *font_map = pango_cairo_font_map_get_default();
	PangoContext *context = pango_font_map_create_context(font_map);
	PangoLayout *layout = pango_layout_new(context);
	{
		PangoFontDescription *font_description = pango_font_description_from_string("Rubik Bold 48");
		pango_layout_set_font_description(layout, font_description);
		PangoFont *font = pango_font_map_load_font(font_map, context, font_description);
		g_object_unref(font);
		pango_font_description_free(font_description);

		font_description = pango_context_get_font_description(context);
		log_info("Using family: %s\n", pango_font_description_get_family(font_description));
	}
	pango_layout_set_text(layout, "············", -1);

	int width;
	int height;
	pango_layout_get_pixel_size(layout, &width, &height);
	fprintf(stderr, "%d x %d\n", width, height);
	fprintf(stderr, "%d, %d\n", entry->border.width, entry->border.outline_width);
	width += 2 * (entry->border.width + entry->border.outline_width);
	height += 2 * (entry->border.width + entry->border.outline_width);
	entry->surface.width = width;
	entry->surface.height = height;

	g_object_unref(layout);
	g_object_unref(context);
}
