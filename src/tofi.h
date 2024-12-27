#ifndef TOFI_H
#define TOFI_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "clipboard.h"
#include "color.h"
#include "entry.h"
#include "matching.h"
#include "surface.h"
#include "wlr-layer-shell-unstable-v1.h"
#include "fractional-scale-v1.h"

#define MAX_OUTPUT_NAME_LEN 256
#define MAX_TERMINAL_NAME_LEN 256
#define MAX_HISTORY_FILE_NAME_LEN 256

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
	struct wl_data_device_manager *wl_data_device_manager;
	struct wl_data_device *wl_data_device;
	struct wp_viewporter *wp_viewporter;
	struct wp_fractional_scale_manager_v1 *wp_fractional_scale_manager;
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
	struct clipboard clipboard;
	struct {
		struct surface surface;
		struct wp_viewport *wp_viewport;
		struct zwlr_layer_surface_v1 *zwlr_layer_surface;
		struct entry entry;
		uint32_t width;
		uint32_t height;
		uint32_t scale;
		uint32_t fractional_scale;
		int32_t transform;
		int32_t exclusive_zone;
		int32_t margin_top;
		int32_t margin_bottom;
		int32_t margin_left;
		int32_t margin_right;
		bool width_is_percent;
		bool height_is_percent;
		bool exclusive_zone_is_percent;
		bool margin_top_is_percent;
		bool margin_bottom_is_percent;
		bool margin_left_is_percent;
		bool margin_right_is_percent;
	} window;
	struct {
		uint32_t rate;
		uint32_t delay;
		uint32_t keycode;
		uint32_t next;
		bool active;
	} repeat;

	/* Options */
	uint32_t anchor;
	enum matching_algorithm matching_algorithm;
	bool ascii_input;
	bool hide_cursor;
	bool mouse_enable;
	bool use_history;
	bool use_scale;
	bool late_keyboard_init;
	bool drun_launch;
	bool drun_print_exec;
	bool require_match;
	bool auto_accept_single;
	bool print_index;
	bool multiple_instance;
	bool physical_keybindings;
	char target_output_name[MAX_OUTPUT_NAME_LEN];
	char default_terminal[MAX_TERMINAL_NAME_LEN];
	char history_file[MAX_HISTORY_FILE_NAME_LEN];
};

#endif /* TOFI_H */
