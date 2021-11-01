#include "client.h"
#include "gl.h"
#include "surface.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void surface_initialise(
		struct surface *surface,
		struct wl_display *wl_display,
		struct image *texture)
{
	egl_create_window(
			&surface->egl,
			surface->wl_surface,
			surface->width,
			surface->height);
	egl_create_context(&surface->egl, wl_display);
	gl_initialise(&surface->gl, texture);
}

void surface_destroy(struct surface *surface)
{
	egl_make_current(&surface->egl);
	gl_destroy(&surface->gl);
	egl_destroy(&surface->egl);
}

void surface_draw(
		struct surface *surface,
		struct color *color,
		struct image *texture)
{
	egl_make_current(&surface->egl);
	gl_clear(&surface->gl, color);

	if (texture != NULL && texture->buffer != NULL) {
		double scale = MAX(
				(double)surface->width / texture->width,
				(double)surface->height / texture->height
				);
		uint32_t width = (uint32_t)(scale * texture->width);
		uint32_t height = (uint32_t)(scale * texture->height);
		int32_t x = -((int32_t)width - (int32_t)surface->width) / 2;
		int32_t y = -((int32_t)height - (int32_t)surface->height) / 2;

		gl_draw_texture(&surface->gl, texture, x, y, width, height);
	}

	egl_swap_buffers(&surface->egl);
}
