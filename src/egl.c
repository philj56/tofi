#include <assert.h>
#include <string.h>
#include <wayland-egl.h>
#include "egl.h"
#include "log.h"

static const char *egl_error_string();

void egl_create_window(
		struct egl *egl,
		struct wl_surface *wl_surface,
		uint32_t width,
		uint32_t height)
{
	egl->window = wl_egl_window_create(wl_surface, width, height);

	if (egl->window == EGL_NO_SURFACE) {
		egl_log_error("Couldn't create EGL window");
	}
}

void egl_create_context(struct egl *egl, struct wl_display *wl_display)
{
	egl->display = eglGetDisplay(wl_display);
	if (egl->display == EGL_NO_DISPLAY) {
		egl_log_error("Couldn't get EGL display");
		return;
	}

	if (!eglInitialize(egl->display, NULL, NULL)) {
		egl_log_error("Couldn't initialise EGL");
		return;
	}

	EGLBoolean result;
	EGLConfig config;
	EGLint num_configs;
	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_NONE
	};

	result = eglGetConfigs(egl->display, NULL, 0, &num_configs);
	if ((result != EGL_TRUE) || (num_configs == 0)) {
		egl_log_error("No EGL configs available");
		return;
	}

	result = eglChooseConfig(
			egl->display,
			config_attribs,
			&config,
			1,
			&num_configs);
	if ((result != EGL_TRUE) || (num_configs != 1)) {
		egl_log_error("Failed to choose EGL config");
		return;
	}

	egl->surface = eglCreateWindowSurface(
			egl->display,
			config,
			egl->window,
			NULL);
	if (egl->surface == EGL_NO_SURFACE) {
		egl_log_error("Couldn't create EGL window surface");
		return;
	}

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 2,
		EGL_NONE
	};

	egl->context = eglCreateContext(
			egl->display,
			config,
			EGL_NO_CONTEXT,
			context_attribs);
	if (egl->context == EGL_NO_CONTEXT) {
		egl_log_error("Couldn't create EGL context");
		return;
	}

	result = eglMakeCurrent(
			egl->display,
			egl->surface,
			egl->surface,
			egl->context);
	if (!result) {
		egl_log_error("Couldn't make EGL context current");
		return;
	}
}

void egl_destroy(struct egl *egl)
{
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(egl->display, egl->surface);
	wl_egl_window_destroy(egl->window);
}

void egl_log_error(const char *msg)
{
	log_error("%s: %s\n", msg, egl_error_string());
}

void egl_make_current(struct egl *egl)
{
	bool result = eglMakeCurrent(
			egl->display,
			egl->surface,
			egl->surface,
			egl->context);
	if (!result) {
		egl_log_error("Couldn't make EGL context current");
		return;
	}
}

void egl_swap_buffers(struct egl *egl)
{
	eglSwapBuffers(egl->display, egl->surface);
}

const char *egl_error_string()
{
	switch(eglGetError()) {
		case EGL_SUCCESS:
			return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:
			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:
			return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:
			return "EGL_CONTEXT_LOST";
	}
	return "Unknown EGL error";
}
