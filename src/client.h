#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "egl.h"

struct client_state {
	/* Globals */
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_shm *wl_shm;
	struct wl_compositor *wl_compositor;
	struct wl_seat *wl_seat;
	struct xdg_wm_base *xdg_wm_base;
	/* Objects */
	struct wl_surface *wl_surface;
	struct wl_keyboard *wl_keyboard;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	/* State */
	float offset;
	uint32_t last_frame;
	int width;
	int height;
	bool closed;
	struct egl egl;
	/* Keyboard state */
	struct xkb_state *xkb_state;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
};

#endif /* CLIENT_H */
