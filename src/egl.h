#ifndef EGL_H
#define EGL_H

#include <wayland-egl.h>
#include <epoxy/egl.h>

struct egl {
	EGLNativeWindowType window;
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
};

void egl_create_window(
		struct egl *egl,
		struct wl_surface *wl_surface,
		uint32_t width,
		uint32_t height);
void egl_create_context(struct egl *egl, struct wl_display *wl_display);
void egl_destroy(struct egl *egl);
void egl_log_error(const char *msg);
void egl_make_current(struct egl *egl);
void egl_swap_buffers(struct egl *egl);

#endif /* EGL_H */
