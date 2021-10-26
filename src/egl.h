#ifndef EGL_H
#define EGL_H

#include <epoxy/egl.h>

struct client_state;
struct egl {
	EGLNativeWindowType window;
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
};

void egl_create_window(struct client_state *state);
void egl_create_context(struct client_state *state);

#endif /* EGL_H */
