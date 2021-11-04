#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <xdg-shell.h>
#include "color.h"
#include "entry.h"
#include "image.h"
#include "surface.h"

struct client_state {
	/* Globals */
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_subcompositor *wl_subcompositor;
	struct wl_seat *wl_seat;
	struct wl_output *wl_output;
	struct xdg_wm_base *xdg_wm_base;

	uint32_t wl_display_name;
	uint32_t wl_registry_name;
	uint32_t wl_compositor_name;
	uint32_t wl_subcompositor_name;
	uint32_t wl_seat_name;
	uint32_t wl_output_name;
	uint32_t xdg_wm_base_name;

	/* Objects */
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;

	/* State */
	bool closed;
	struct {
		struct surface surface;
		struct xdg_surface *xdg_surface;
		struct xdg_toplevel *xdg_toplevel;
		struct image background_image;
		struct color background_color;
		struct entry entry;
		int32_t width;
		int32_t height;
		uint32_t scale;
		bool resize;
	} window;

	/* Keyboard state */
	struct xkb_state *xkb_state;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;

	/* greetd state */
	const char *username;
	const char *command;
	bool submit;

	/* Options */
	bool hide_cursor;
};

#endif /* CLIENT_H */
