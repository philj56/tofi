#ifndef TOFI_H
#define TOFI_H

#include <stdbool.h>
#include <stdint.h>
#include "color.h"
#include "entry.h"
#include "image.h"
#include "surface.h"
#include "wlr-layer-shell-unstable-v1.h"

#define MAX_OUTPUT_NAME_LEN 256

struct output_list_element {
	struct wl_list link;
	struct wl_output *wl_output;
	char *name;
	uint32_t width;
	uint32_t height;
	int32_t scale;
	int32_t transform;
};

struct tofi {
	/* Wayland globals */
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_seat *wl_seat;
	struct wl_shm *wl_shm;
	struct zwlr_layer_shell_v1 *zwlr_layer_shell;
	struct wl_list output_list;
	struct output_list_element *default_output;

	/* Wayland objects */
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;

	/* Keyboard objects */
	char *xkb_keymap_string;
	struct xkb_state *xkb_state;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;

	/* State */
	bool submit;
	bool closed;
	int32_t output_width;
	int32_t output_height;
	struct {
		struct surface surface;
		struct zwlr_layer_surface_v1 *zwlr_layer_surface;
		struct entry entry;
		uint32_t width;
		uint32_t height;
		uint32_t scale;
		int32_t transform;
		int32_t margin_top;
		int32_t margin_bottom;
		int32_t margin_left;
		int32_t margin_right;
	} window;

	/* Options */
	uint32_t anchor;
	bool hide_cursor;
	bool use_history;
	bool late_keyboard_init;
	bool drun_launch;
	char target_output_name[MAX_OUTPUT_NAME_LEN];
};

#endif /* TOFI_H */
