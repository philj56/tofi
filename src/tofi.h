#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <xdg-shell.h>
#include "color.h"
#include "entry.h"
#include "image.h"
#include "surface.h"
#include "wlr-layer-shell-unstable-v1.h"

struct tofi {
	/* Globals */
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_seat *wl_seat;
	struct wl_output *wl_output;
	struct zwlr_layer_shell_v1 *zwlr_layer_shell;

	uint32_t wl_display_name;
	uint32_t wl_registry_name;
	uint32_t wl_compositor_name;
	uint32_t wl_seat_name;
	uint32_t wl_output_name;
	uint32_t zwlr_layer_shell_name;

	/* Objects */
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;

	/* State */
	bool closed;
	struct {
		struct surface surface;
		struct zwlr_layer_surface_v1 *zwlr_layer_surface;
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
