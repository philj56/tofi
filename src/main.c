#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>
#include "tofi.h"
#include "compgen.h"
#include "drun.h"
#include "config.h"
#include "entry.h"
#include "image.h"
#include "input.h"
#include "log.h"
#include "nelem.h"
#include "lock.h"
#include "shm.h"
#include "string_vec.h"
#include "string_vec.h"
#include "unicode.h"
#include "xmalloc.h"

#undef MAX
#undef MIN
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *mime_type_text_plain = "text/plain";
static const char *mime_type_text_plain_utf8 = "text/plain;charset=utf-8";

static uint32_t gettime_ms() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	uint32_t ms = t.tv_sec * 1000;
	ms += t.tv_nsec / 1000000;
	return ms;
}

static void zwlr_layer_surface_configure(
		void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface,
		uint32_t serial,
		uint32_t width,
		uint32_t height)
{
	struct tofi *tofi = data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us, so don't do anything. */
		log_debug("Layer surface configure with no width or height.\n");
		return;
	}
	log_debug("Layer surface configure, %d x %d.\n", width, height);

	/*
	 * Resize the main window.
	 * We want actual pixel width / height, so we have to scale the
	 * values provided by Wayland.
	 */
	tofi->window.width = width * tofi->window.scale;
	tofi->window.height = height * tofi->window.scale;

	tofi->window.surface.width = tofi->window.width;
	tofi->window.surface.height = tofi->window.height;

	zwlr_layer_surface_v1_ack_configure(
			tofi->window.zwlr_layer_surface,
			serial);
}

static void zwlr_layer_surface_close(
		void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface)
{
	struct tofi *tofi = data;
	tofi->closed = true;
	log_debug("Layer surface close.\n");
}

static const struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
	.configure = zwlr_layer_surface_configure,
	.closed = zwlr_layer_surface_close
};

static void wl_keyboard_keymap(
		void *data,
		struct wl_keyboard *wl_keyboard,
		uint32_t format,
		int32_t fd,
		uint32_t size)
{
	struct tofi *tofi = data;
	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
	log_debug("Configuring keyboard.\n");

	char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(map_shm != MAP_FAILED);

	if (tofi->late_keyboard_init) {
		tofi->xkb_keymap_string = xstrdup(map_shm);
	} else {
		struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
				tofi->xkb_context,
				map_shm,
				XKB_KEYMAP_FORMAT_TEXT_V1,
				XKB_KEYMAP_COMPILE_NO_FLAGS);
		munmap(map_shm, size);
		close(fd);

		struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
		xkb_keymap_unref(tofi->xkb_keymap);
		xkb_state_unref(tofi->xkb_state);
		tofi->xkb_keymap = xkb_keymap;
		tofi->xkb_state = xkb_state;
	}
	munmap(map_shm, size);
	close(fd);

	log_debug("Keyboard configured.\n");
}

static void wl_keyboard_enter(
		void *data,
		struct wl_keyboard *wl_keyboard,
		uint32_t serial,
		struct wl_surface *surface,
		struct wl_array *keys)
{
	/* Deliberately left blank */
}

static void wl_keyboard_leave(
		void *data,
		struct wl_keyboard *wl_keyboard,
		uint32_t serial,
		struct wl_surface *surface)
{
	/* Deliberately left blank */
}

static void wl_keyboard_key(
		void *data,
		struct wl_keyboard *wl_keyboard,
		uint32_t serial,
		uint32_t time,
		uint32_t key,
		uint32_t state)
{
	struct tofi *tofi = data;

	/*
	 * If this wasn't a keypress (i.e. was a key release), just update key
	 * repeat info and return.
	 */
	uint32_t keycode = key + 8;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		if (keycode == tofi->repeat.keycode) {
			tofi->repeat.active = false;
		} else {
			tofi->repeat.next = gettime_ms() + tofi->repeat.delay;
		}
		return;
	}

	/* A rate of 0 disables key repeat */
	if (xkb_keymap_key_repeats(tofi->xkb_keymap, keycode) && tofi->repeat.rate != 0) {
		tofi->repeat.active = true;
		tofi->repeat.keycode = keycode;
		tofi->repeat.next = gettime_ms() + tofi->repeat.delay;
	}
	input_handle_keypress(tofi, keycode);
}

static void wl_keyboard_modifiers(
		void *data,
		struct wl_keyboard *wl_keyboard,
		uint32_t serial,
		uint32_t mods_depressed,
		uint32_t mods_latched,
		uint32_t mods_locked,
		uint32_t group)
{
	struct tofi *tofi = data;
	if (tofi->xkb_state == NULL) {
		return;
	}
	xkb_state_update_mask(
			tofi->xkb_state,
			mods_depressed,
			mods_latched,
			mods_locked,
			0,
			0,
			group);
}

static void wl_keyboard_repeat_info(
		void *data,
		struct wl_keyboard *wl_keyboard,
		int32_t rate,
		int32_t delay)
{
	struct tofi *tofi = data;
	tofi->repeat.rate = rate;
	tofi->repeat.delay = delay;
	if (rate > 0) {
		log_debug("Key repeat every %u ms after %u ms.\n",
				1000 / rate,
				delay);
	} else {
		log_debug("Key repeat disabled.\n");
	}
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap = wl_keyboard_keymap,
	.enter = wl_keyboard_enter,
	.leave = wl_keyboard_leave,
	.key = wl_keyboard_key,
	.modifiers = wl_keyboard_modifiers,
	.repeat_info = wl_keyboard_repeat_info,
};

static void wl_pointer_enter(
		void *data,
		struct wl_pointer *pointer,
		uint32_t serial,
		struct wl_surface *surface,
		wl_fixed_t surface_x,
		wl_fixed_t surface_y)
{
	struct tofi *tofi = data;
	if (tofi->hide_cursor) {
		/* Hide the cursor by setting its surface to NULL. */
		wl_pointer_set_cursor(tofi->wl_pointer, serial, NULL, 0, 0);
	}
}

static void wl_pointer_leave(
		void *data,
		struct wl_pointer *pointer,
		uint32_t serial,
		struct wl_surface *surface)
{
	/* Deliberately left blank */
}

static void wl_pointer_motion(
		void *data,
		struct wl_pointer *pointer,
		uint32_t time,
		wl_fixed_t surface_x,
		wl_fixed_t surface_y)
{
	/* Deliberately left blank */
}

static void wl_pointer_button(
		void *data,
		struct wl_pointer *pointer,
		uint32_t serial,
		uint32_t time,
		uint32_t button,
		enum wl_pointer_button_state state)
{
	/* Deliberately left blank */
}

static void wl_pointer_axis(
		void *data,
		struct wl_pointer *pointer,
		uint32_t time,
		enum wl_pointer_axis axis,
		wl_fixed_t value)
{
	/* Deliberately left blank */
}

static void wl_pointer_frame(void *data, struct wl_pointer *pointer)
{
	/* Deliberately left blank */
}

static void wl_pointer_axis_source(
		void *data,
		struct wl_pointer *pointer,
		enum wl_pointer_axis_source axis_source)
{
	/* Deliberately left blank */
}

static void wl_pointer_axis_stop(
		void *data,
		struct wl_pointer *pointer,
		uint32_t time,
		enum wl_pointer_axis axis)
{
	/* Deliberately left blank */
}

static void wl_pointer_axis_discrete(
		void *data,
		struct wl_pointer *pointer,
		enum wl_pointer_axis axis,
		int32_t discrete)
{
	/* Deliberately left blank */
}

static const struct wl_pointer_listener wl_pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete
};

static void wl_seat_capabilities(
		void *data,
		struct wl_seat *wl_seat,
		uint32_t capabilities)
{
	struct tofi *tofi = data;

	bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
	bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

	if (have_keyboard && tofi->wl_keyboard == NULL) {
		tofi->wl_keyboard = wl_seat_get_keyboard(tofi->wl_seat);
		wl_keyboard_add_listener(
				tofi->wl_keyboard,
				&wl_keyboard_listener,
				tofi);
		log_debug("Got keyboard from seat.\n");
	} else if (!have_keyboard && tofi->wl_keyboard != NULL) {
		wl_keyboard_release(tofi->wl_keyboard);
		tofi->wl_keyboard = NULL;
		log_debug("Released keyboard.\n");
	}

	if (have_pointer && tofi->wl_pointer == NULL) {
		tofi->wl_pointer = wl_seat_get_pointer(tofi->wl_seat);
		wl_pointer_add_listener(
				tofi->wl_pointer,
				&wl_pointer_listener,
				tofi);
		log_debug("Got pointer from seat.\n");
	} else if (!have_pointer && tofi->wl_pointer != NULL) {
		wl_pointer_release(tofi->wl_pointer);
		tofi->wl_pointer = NULL;
		log_debug("Released pointer.\n");
	}
}

static void wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	/* Deliberately left blank */
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = wl_seat_name,
};

static void wl_data_offer_offer(
		void *data,
		struct wl_data_offer *wl_data_offer,
		const char *mime_type)
{
	struct clipboard *clipboard = data;

	/* Only accept plain text, and prefer utf-8. */
	if (!strcmp(mime_type, mime_type_text_plain)) {
		if (clipboard->mime_type != NULL) {
			clipboard->mime_type = mime_type_text_plain;
		}
	} else if (!strcmp(mime_type, mime_type_text_plain_utf8)) {
		clipboard->mime_type = mime_type_text_plain_utf8;
	}
}

static void wl_data_offer_source_actions(
		void *data,
		struct wl_data_offer *wl_data_offer,
		uint32_t source_actions)
{
	/* Deliberately left blank */
}

static void wl_data_offer_action(
		void *data,
		struct wl_data_offer *wl_data_offer,
		uint32_t action)
{
	/* Deliberately left blank */
}

static const struct wl_data_offer_listener wl_data_offer_listener = {
	.offer = wl_data_offer_offer,
	.source_actions = wl_data_offer_source_actions,
	.action = wl_data_offer_action
};

static void wl_data_device_data_offer(
		void *data,
		struct wl_data_device *wl_data_device,
		struct wl_data_offer *wl_data_offer)
{
	struct clipboard *clipboard = data;
	clipboard_reset(clipboard);
	clipboard->wl_data_offer = wl_data_offer;
	wl_data_offer_add_listener(
			wl_data_offer,
			&wl_data_offer_listener,
			clipboard);
}

static void wl_data_device_enter(
		void *data,
		struct wl_data_device *wl_data_device,
		uint32_t serial,
		struct wl_surface *wl_surface,
		int32_t x,
		int32_t y,
		struct wl_data_offer *wl_data_offer)
{
	/* Drag-and-drop is just ignored for now. */
	wl_data_offer_accept(
			wl_data_offer,
			serial,
			NULL);
	wl_data_offer_set_actions(
			wl_data_offer,
			WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE,
			WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE);
}

static void wl_data_device_leave(
		void *data,
		struct wl_data_device *wl_data_device)
{
	/* Deliberately left blank */
}

static void wl_data_device_motion(
		void *data,
		struct wl_data_device *wl_data_device,
		uint32_t time,
		int32_t x,
		int32_t y)
{
	/* Deliberately left blank */
}

static void wl_data_device_drop(
		void *data,
		struct wl_data_device *wl_data_device)
{
	/* Deliberately left blank */
}

static void wl_data_device_selection(
		void *data,
		struct wl_data_device *wl_data_device,
		struct wl_data_offer *wl_data_offer)
{
	struct clipboard *clipboard = data;
	if (wl_data_offer == NULL) {
		clipboard_reset(clipboard);
	}
}

static const struct wl_data_device_listener wl_data_device_listener = {
	.data_offer = wl_data_device_data_offer,
	.enter = wl_data_device_enter,
	.leave = wl_data_device_leave,
	.motion = wl_data_device_motion,
	.drop = wl_data_device_drop,
	.selection = wl_data_device_selection
};

static void output_geometry(
		void *data,
		struct wl_output *wl_output,
		int32_t x,
		int32_t y,
		int32_t physical_width,
		int32_t physical_height,
		int32_t subpixel,
		const char *make,
		const char *model,
		int32_t transform)
{
	struct tofi *tofi = data;
	struct output_list_element *el;
	wl_list_for_each(el, &tofi->output_list, link) {
		if (el->wl_output == wl_output) {
			el->transform = transform;
		}
	}
}

static void output_mode(
		void *data,
		struct wl_output *wl_output,
		uint32_t flags,
		int32_t width,
		int32_t height,
		int32_t refresh)
{
	struct tofi *tofi = data;
	struct output_list_element *el;
	wl_list_for_each(el, &tofi->output_list, link) {
		if (el->wl_output == wl_output) {
			if (flags & WL_OUTPUT_MODE_CURRENT) {
				el->width = width;
				el->height = height;
			}
		}
	}
}

static void output_scale(
		void *data,
		struct wl_output *wl_output,
		int32_t factor)
{
	struct tofi *tofi = data;
	struct output_list_element *el;
	wl_list_for_each(el, &tofi->output_list, link) {
		if (el->wl_output == wl_output) {
			el->scale = factor;
		}
	}
}

static void output_name(
		void *data,
		struct wl_output *wl_output,
		const char *name)
{
	struct tofi *tofi = data;
	struct output_list_element *el;
	wl_list_for_each(el, &tofi->output_list, link) {
		if (el->wl_output == wl_output) {
			el->name = xstrdup(name);
		}
	}
}

static void output_description(
		void *data,
		struct wl_output *wl_output,
		const char *description)
{
	/* Deliberately left blank */
}

static void output_done(void *data, struct wl_output *wl_output)
{
	log_debug("Output configuration done.\n");
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
#ifndef NO_WL_OUTPUT_NAME
	.name = output_name,
	.description = output_description,
#endif
};

static void registry_global(
		void *data,
		struct wl_registry *wl_registry,
		uint32_t name,
		const char *interface,
		uint32_t version)
{
	struct tofi *tofi = data;
	//log_debug("Registry %u: %s v%u.\n", name, interface, version);
	if (!strcmp(interface, wl_compositor_interface.name)) {
		tofi->wl_compositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_compositor_interface,
				4);
		log_debug("Bound to compositor %u.\n", name);
	} else if (!strcmp(interface, wl_seat_interface.name)) {
		tofi->wl_seat = wl_registry_bind(
				wl_registry,
				name,
				&wl_seat_interface,
				7);
		wl_seat_add_listener(
				tofi->wl_seat,
				&wl_seat_listener,
				tofi);
		log_debug("Bound to seat %u.\n", name);
	} else if (!strcmp(interface, wl_output_interface.name)) {
		struct output_list_element *el = xmalloc(sizeof(*el));
		if (version < 4) {
			el->name = xstrdup("");
			log_warning("Using an outdated compositor, "
					"output selection will not work.\n");
		} else {
			version = 4;
		}
		el->wl_output = wl_registry_bind(
				wl_registry,
				name,
				&wl_output_interface,
				version);
		wl_output_add_listener(
				el->wl_output,
				&wl_output_listener,
				tofi);
		wl_list_insert(&tofi->output_list, &el->link);
		log_debug("Bound to output %u.\n", name);
	} else if (!strcmp(interface, wl_shm_interface.name)) {
		tofi->wl_shm = wl_registry_bind(
				wl_registry,
				name,
				&wl_shm_interface,
				1);
		log_debug("Bound to shm %u.\n", name);
	} else if (!strcmp(interface, wl_data_device_manager_interface.name)) {
		tofi->wl_data_device_manager = wl_registry_bind(
				wl_registry,
				name,
				&wl_data_device_manager_interface,
				3);
		log_debug("Bound to data device manager  %u.\n", name);
	} else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
		if (version < 3) {
			log_warning("Using an outdated compositor, "
					"screen anchoring may not work.\n");
		} else {
			version = 3;
		}
		tofi->zwlr_layer_shell = wl_registry_bind(
				wl_registry,
				name,
				&zwlr_layer_shell_v1_interface,
				version);
		log_debug("Bound to zwlr_layer_shell_v1 %u.\n", name);
	}
}

static void registry_global_remove(
		void *data,
		struct wl_registry *wl_registry,
		uint32_t name)
{
	/* Deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void surface_enter(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	log_debug("Surface entered output.\n");
}

static void surface_leave(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	/* Deliberately left blank */
}

static const struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave
};

/*
 * These "dummy_*" functions are callbacks just for the dummy surface used to
 * select the default output if there's more than one.
 */
static void dummy_layer_surface_configure(
		void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface,
		uint32_t serial,
		uint32_t width,
		uint32_t height)
{
	zwlr_layer_surface_v1_ack_configure(
			zwlr_layer_surface,
			serial);
}

static void dummy_layer_surface_close(
		void *data,
		struct zwlr_layer_surface_v1 *zwlr_layer_surface)
{
}

static const struct zwlr_layer_surface_v1_listener dummy_layer_surface_listener = {
	.configure = dummy_layer_surface_configure,
	.closed = dummy_layer_surface_close
};

static void dummy_surface_enter(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	struct tofi *tofi = data;
	struct output_list_element *el;
	wl_list_for_each(el, &tofi->output_list, link) {
		if (el->wl_output == wl_output) {
			tofi->default_output = el;
			break;
		}
	}
}

static void dummy_surface_leave(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	/* Deliberately left blank */
}

static const struct wl_surface_listener dummy_surface_listener = {
	.enter = dummy_surface_enter,
	.leave = dummy_surface_leave
};


static void usage(bool err)
{
	fprintf(err ? stderr : stdout, "%s",
"Usage: tofi [options]\n"
"\n"
"Basic options:\n"
"  -h, --help                           Print this message and exit.\n"
"  -c, --config <path>                  Specify a config file.\n"
"      --prompt-text <string>           Prompt text.\n"
"      --width <px|%>                   Width of the window.\n"
"      --height <px|%>                  Height of the window.\n"
"      --output <name>                  Name of output to display window on.\n"
"      --anchor <position>              Location on screen to anchor window.\n"
"      --horizontal <true|false>        List results horizontally.\n"
"      --fuzzy-match <true|false>       Use fuzzy matching for searching.\n"
"\n"
"All options listed in \"man 5 tofi\" are also accpted in the form \"--key=value\".\n"
	);
}

/* Option parsing with getopt. */
const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", required_argument, NULL, 'c'},
	{"include", required_argument, NULL, 0},
	{"anchor", required_argument, NULL, 0},
	{"exclusive-zone", required_argument, NULL, 0},
	{"background-color", required_argument, NULL, 0},
	{"corner-radius", required_argument, NULL, 0},
	{"font", required_argument, NULL, 0},
	{"font-size", required_argument, NULL, 0},
	{"font-features", required_argument, NULL, 0},
	{"font-variations", required_argument, NULL, 0},
	{"num-results", required_argument, NULL, 0},
	{"selection-color", required_argument, NULL, 0},
	{"selection-match-color", required_argument, NULL, 0},
	{"selection-padding", required_argument, NULL, 0},
	{"selection-background", required_argument, NULL, 0},
	{"outline-width", required_argument, NULL, 0},
	{"outline-color", required_argument, NULL, 0},
	{"prompt-text", required_argument, NULL, 0},
	{"prompt-padding", required_argument, NULL, 0},
	{"placeholder-text", required_argument, NULL, 0},
	{"placeholder-color", required_argument, NULL, 0},
	{"result-spacing", required_argument, NULL, 0},
	{"min-input-width", required_argument, NULL, 0},
	{"border-width", required_argument, NULL, 0},
	{"border-color", required_argument, NULL, 0},
	{"text-color", required_argument, NULL, 0},
	{"width", required_argument, NULL, 0},
	{"height", required_argument, NULL, 0},
	{"margin-top", required_argument, NULL, 0},
	{"margin-bottom", required_argument, NULL, 0},
	{"margin-left", required_argument, NULL, 0},
	{"margin-right", required_argument, NULL, 0},
	{"padding-top", required_argument, NULL, 0},
	{"padding-bottom", required_argument, NULL, 0},
	{"padding-left", required_argument, NULL, 0},
	{"padding-right", required_argument, NULL, 0},
	{"horizontal", required_argument, NULL, 0},
	{"hide-cursor", required_argument, NULL, 0},
	{"history", required_argument, NULL, 0},
	{"history-file", required_argument, NULL, 0},
	{"fuzzy-match", required_argument, NULL, 0},
	{"require-match", required_argument, NULL, 0},
	{"hide-input", required_argument, NULL, 0},
	{"hidden-character", required_argument, NULL, 0},
	{"drun-launch", required_argument, NULL, 0},
	{"drun-print-exec", required_argument, NULL, 0},
	{"terminal", required_argument, NULL, 0},
	{"hint-font", required_argument, NULL, 0},
	{"multi-instance", required_argument, NULL, 0},
	{"output", required_argument, NULL, 0},
	{"scale", required_argument, NULL, 0},
	{"late-keyboard-init", optional_argument, NULL, 'k'},
	{NULL, 0, NULL, 0}
};
const char *short_options = ":hc:";

static void parse_args(struct tofi *tofi, int argc, char *argv[])
{

	bool load_default_config = true;
	int option_index = 0;

	/* Handle errors ourselves. */
	opterr = 0;

	/* First pass, just check for config file, help, and errors. */
	optind = 1;
	int opt = getopt_long(argc, argv, short_options, long_options, &option_index);
	while (opt != -1) {
		if (opt == 'h') {
			usage(false);
			exit(EXIT_SUCCESS);
		} else if (opt == 'c') {
			config_load(tofi, optarg);
			load_default_config = false;
		} else if (opt == ':') {
			log_error("Option %s requires an argument.\n", argv[optind - 1]);
			usage(true);
			exit(EXIT_FAILURE);
		} else if (opt == '?') {
			if (optopt) {
				log_error("Unknown option -%c.\n", optopt);
			} else {
				log_error("Unknown option %s.\n", argv[optind - 1]);
			}
			usage(true);
			exit(EXIT_FAILURE);
		}
		opt = getopt_long(argc, argv, short_options, long_options, &option_index);
	}
	if (load_default_config) {
		config_load(tofi, NULL);
	}

	/* Second pass, parse everything else. */
	optind = 1;
	opt = getopt_long(argc, argv, short_options, long_options, &option_index);
	while (opt != -1) {
		if (opt == 0) {
			config_apply(tofi, long_options[option_index].name, optarg);
		} else if (opt == 'k') {
			/*
			 * Backwards compatibility for --late-keyboard-init not
			 * taking an argument.
			 */
			if (optarg) {
				config_apply(tofi, long_options[option_index].name, optarg);
			} else {
				tofi->late_keyboard_init = true;
			}
		}
		opt = getopt_long(argc, argv, short_options, long_options, &option_index);
	}

	if (optind < argc) {
		log_error("Unexpected non-option argument '%s'.\n", argv[optind]);
		usage(true);
		exit(EXIT_FAILURE);
	}
}

static bool do_submit(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;
	uint32_t selection = entry->selection + entry->first_result;
	char *res = entry->results.buf[selection].string;

	if (tofi->window.entry.results.count == 0) {
		/* Always require a match in drun mode. */
		if (tofi->require_match || entry->drun) {
			return false;
		} else {
			printf("%s\n", entry->input_utf8);
			return true;
		}
	}

	if (entry->drun) {
		/*
		 * At this point, the list of apps is history sorted rather
		 * than alphabetically sorted, so we can't use
		 * desktop_vec_find_sorted().
		 */
		struct desktop_entry *app = NULL;
		for (size_t i = 0; i < entry->apps.count; i++) {
			if (!strcmp(res, entry->apps.buf[i].name)) {
				app = &entry->apps.buf[i];
				break;
			}
		}
		if (app == NULL) {
			log_error("Couldn't find application file! This shouldn't happen.\n");
			return false;
		}
		char *path = app->path;
		if (tofi->drun_launch) {
			drun_launch(path);
		} else {
			drun_print(path, tofi->default_terminal);
		}
	} else {
		printf("%s\n", res);
	}
	if (tofi->use_history) {
		history_add(
				&entry->history,
				entry->results.buf[selection].string);
		if (tofi->history_file[0] == 0) {
			history_save_default_file(&entry->history, entry->drun);
		} else {
			history_save(&entry->history, tofi->history_file);
		}
	}
	return true;
}

static void read_clipboard(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;
	/* Buffer for 4 UTF-8 bytes plus a null terminator. */
	char buffer[5];
	memset(buffer, 0, N_ELEM(buffer));
	errno = 0;
	bool eof = false;
	while (entry->input_utf32_length < N_ELEM(entry->input_utf32)) {
		for (size_t i = 0; i < 4; i++) {
			/*
			 * Read input 1 byte at a time. This is slow, but easy,
			 * and speed of pasting shouldn't really matter.
			 */
			int res = read(tofi->clipboard.fd, &buffer[i], 1);
			if (res == 0) {
				eof = true;
				break;
			} else if (res == -1) {
				if (errno == EAGAIN) {
					/*
					 * There was no more data to be read,
					 * but EOF hasn't been reached yet.
					 *
					 * This could mean more than a pipe's
					 * capacity (64k) of data was sent, in
					 * which case we'd potentially skip
					 * a character, but we should hit the
					 * input length limit long before that.
					 */
					input_refresh_results(tofi);
					tofi->window.surface.redraw = true;
					return;
				}
				log_error("Failed to read clipboard: %s\n", strerror(errno));
				clipboard_finish_paste(&tofi->clipboard);
				return;
			}
			uint32_t unichar = utf8_to_utf32_validate(buffer);
			if (unichar == (uint32_t)-2) {
				/* The current character isn't complete yet. */
				continue;
			} else if (unichar == (uint32_t)-1) {
				log_error("Invalid UTF-8 character in clipboard: %s\n", buffer);
				break;
			} else {
				entry->input_utf32[entry->input_utf32_length] = unichar;
				entry->input_utf32_length++;
				break;
			}
		}
		memset(buffer, 0, N_ELEM(buffer));
		if (eof) {
			break;
		}
	}
	entry->input_utf32[MIN(entry->input_utf32_length, N_ELEM(entry->input_utf32) - 1)] = U'\0';

	clipboard_finish_paste(&tofi->clipboard);

	input_refresh_results(tofi);
	tofi->window.surface.redraw = true;
}

int main(int argc, char *argv[])
{
	/* Call log_debug to initialise the timers we use for perf checking. */
	log_debug("This is tofi.\n");

	/*
	 * Set the locale to the user's default, so we can deal with non-ASCII
	 * characters.
	 */
	setlocale(LC_ALL, "");

	/* Default options. */
	struct tofi tofi = {
		.window = {
			.scale = 1,
			.width = 1280,
			.height = 720,
			.exclusive_zone = -1,
			.entry = {
				.font_name = "Sans",
				.font_size = 24,
				.prompt_text = "run: ",
				.hidden_character_utf8 = u8"*",
				.padding_top = 8,
				.padding_bottom = 8,
				.padding_left = 8,
				.padding_right = 8,
				.border_width = 12,
				.outline_width = 4,
				.background_color = {0.106f, 0.114f, 0.118f, 1.0f},
				.foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
				.placeholder_color = {1.0f, 1.0f, 1.0f, 0.66f},
				.selection_foreground_color = {0.976f, 0.149f, 0.447f, 1.0f},
				.border_color = {0.976f, 0.149f, 0.447f, 1.0f},
				.outline_color = {0.031f, 0.031f, 0.0f, 1.0f},
			}
		},
		.anchor =  ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
		.use_history = true,
		.require_match = true,
		.use_scale = true,
	};
	wl_list_init(&tofi.output_list);
	if (getenv("TERMINAL") != NULL) {
		snprintf(
				tofi.default_terminal,
				N_ELEM(tofi.default_terminal),
				"%s",
				getenv("TERMINAL"));
	}

	parse_args(&tofi, argc, argv);

	if (!tofi.multiple_instance && lock_check()) {
		log_error("Another instance of tofi is already running.\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Initial Wayland & XKB setup.
	 * The first thing to do is connect a listener to the global registry,
	 * so that we can bind to the various global objects and start talking
	 * to Wayland.
	 */

	tofi.wl_display = wl_display_connect(NULL);
	if (tofi.wl_display == NULL) {
		log_error("Couldn't connect to Wayland display.\n");
		exit(EXIT_FAILURE);
	}
	tofi.wl_registry = wl_display_get_registry(tofi.wl_display);
	tofi.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (tofi.xkb_context == NULL) {
		log_error("Couldn't create an XKB context.\n");
		exit(EXIT_FAILURE);
	}
	wl_registry_add_listener(
			tofi.wl_registry,
			&wl_registry_listener,
			&tofi);

	/*
	 * After this first roundtrip, the only thing that should have happened
	 * is our registry_global() function being called and setting up the
	 * various global object bindings.
	 */
	log_debug("First roundtrip start.\n");
	log_indent();
	wl_display_roundtrip(tofi.wl_display);
	log_unindent();
	log_debug("First roundtrip done.\n");

	/*
	 * The next roundtrip causes the listeners we set up in
	 * registry_global() to be called. Notably, the output should be
	 * configured, telling us the scale factor and size.
	 */
	log_debug("Second roundtrip start.\n");
	log_indent();
	wl_display_roundtrip(tofi.wl_display);
	log_unindent();
	log_debug("Second roundtrip done.\n");

	/* If there's more than one output, we need to select one. */
	if (wl_list_length(&tofi.output_list) > 1) {
		/*
		 * Determine the default output
		 *
		 * This seems like an ugly solution, but as far as I know
		 * there's no way to determine the default output other than to
		 * call get_layer_surface with NULL as the output and see which
		 * output our surface turns up on.
		 *
		 * Here we set up a single pixel surface, perform the required
		 * two roundtrips, then tear it down. tofi.default_output
		 * should then contain the output our surface was assigned to.
		 */
		struct surface surface = {
			.width = 1,
			.height = 1
		};
		surface.wl_surface =
			wl_compositor_create_surface(tofi.wl_compositor);
		wl_surface_add_listener(
				surface.wl_surface,
				&dummy_surface_listener,
				&tofi);

		struct zwlr_layer_surface_v1 *zwlr_layer_surface =
			zwlr_layer_shell_v1_get_layer_surface(
					tofi.zwlr_layer_shell,
					surface.wl_surface,
					NULL,
					ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
					"dummy");
		/*
		 * Workaround for Hyprland, where if this is not set the dummy
		 * surface never enters an output for some reason.
		 */
		zwlr_layer_surface_v1_set_keyboard_interactivity(
				zwlr_layer_surface,
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
				);
		zwlr_layer_surface_v1_add_listener(
				zwlr_layer_surface,
				&dummy_layer_surface_listener,
				&tofi);
		zwlr_layer_surface_v1_set_size(
				zwlr_layer_surface,
				1,
				1);
		wl_surface_commit(surface.wl_surface);
		wl_display_roundtrip(tofi.wl_display);
		surface_init(&surface, tofi.wl_shm);
		surface_draw(&surface);
		wl_display_roundtrip(tofi.wl_display);
		surface_destroy(&surface);
		zwlr_layer_surface_v1_destroy(zwlr_layer_surface);
		wl_surface_destroy(surface.wl_surface);

		/*
		 * Walk through our output list and select the one we want if
		 * the user's asked for a specific one, otherwise just get the
		 * default one.
		 */
		bool found_target = false;
		struct output_list_element *head;
		head = wl_container_of(tofi.output_list.next, head, link);

		struct output_list_element *el;
		struct output_list_element *tmp;
		if (tofi.target_output_name[0] != 0) {
			log_debug("Looking for output %s.\n", tofi.target_output_name);
		} else if (tofi.default_output != NULL) {
			snprintf(
					tofi.target_output_name,
					N_ELEM(tofi.target_output_name),
					"%s",
					tofi.default_output->name);
			/* We don't need this anymore. */
			tofi.default_output = NULL;
		}
		wl_list_for_each_reverse_safe(el, tmp, &tofi.output_list, link) {
			if (!strcmp(tofi.target_output_name, el->name)) {
				found_target = true;
				continue;
			}
			/*
			 * If we've already found the output we're looking for
			 * or this isn't the first output in the list, remove
			 * it.
			 */
			if (found_target || el != head) {
				wl_list_remove(&el->link);
				wl_output_release(el->wl_output);
				free(el->name);
				free(el);
			}
		}
	}
	{
		/*
		 * The only output left should either be the one we want, or
		 * the first that was advertised.
		 */
		struct output_list_element *el;
		el = wl_container_of(tofi.output_list.next, el, link);

		/*
		 * If we're rotated 90 degrees, we need to swap width and
		 * height to calculate percentages.
		 */
		switch (el->transform) {
			case WL_OUTPUT_TRANSFORM_90:
			case WL_OUTPUT_TRANSFORM_270:
			case WL_OUTPUT_TRANSFORM_FLIPPED_90:
			case WL_OUTPUT_TRANSFORM_FLIPPED_270:
				tofi.output_width = el->height;
				tofi.output_height = el->width;
				break;
			default:
				tofi.output_width = el->width;
				tofi.output_height = el->height;
		}
		tofi.window.scale = el->scale;
		tofi.window.transform = el->transform;
		log_debug("Selected output %s.\n", el->name);
	}

	/*
	 * We can now scale values and calculate any percentages, as we know
	 * the output size and scale.
	 */
	config_fixup_values(&tofi);

	/*
	 * If we were invoked as tofi-run, generate the command list.
	 * If we were invoked as tofi-drun, generate the desktop app list.
	 * Otherwise, just read standard input.
	 */
	if (strstr(argv[0], "-run")) {
		log_debug("Generating command list.\n");
		log_indent();
		tofi.window.entry.commands = compgen_cached();
		if (tofi.use_history) {
			if (tofi.history_file[0] == 0) {
				tofi.window.entry.history = history_load_default_file(tofi.window.entry.drun);
			} else {
				tofi.window.entry.history = history_load(tofi.history_file);
			}
			compgen_history_sort(&tofi.window.entry.commands, &tofi.window.entry.history);
		}
		log_unindent();
		log_debug("Command list generated.\n");
	} else if (strstr(argv[0], "-drun")) {
		log_debug("Generating desktop app list.\n");
		log_indent();
		tofi.window.entry.drun = true;
		struct desktop_vec apps = drun_generate_cached();
		if (tofi.use_history) {
			if (tofi.history_file[0] == 0) {
				tofi.window.entry.history = history_load_default_file(tofi.window.entry.drun);
			} else {
				tofi.window.entry.history = history_load(tofi.history_file);
			}
			if (tofi.window.entry.drun) {
				drun_history_sort(&apps, &tofi.window.entry.history);
			}
		}
		struct string_vec commands = string_vec_create();
		for (size_t i = 0; i < apps.count; i++) {
			string_vec_add(&commands, apps.buf[i].name);
		}
		tofi.window.entry.commands = commands;
		tofi.window.entry.apps = apps;
		log_unindent();
		log_debug("App list generated.\n");
	} else {
		char *line = NULL;
		size_t n = 0;
		tofi.window.entry.commands = string_vec_create();
		while (getline(&line, &n, stdin) != -1) {
			char *c = strchr(line, '\n');
			if (c) {
				*c = '\0';
			}
			string_vec_add(&tofi.window.entry.commands, line);
		}
		free(line);
		if (tofi.use_history) {
			if (tofi.history_file[0] == 0) {
				tofi.use_history = false;
			} else {
				tofi.window.entry.history = history_load(tofi.history_file);
				string_vec_history_sort(&tofi.window.entry.commands, &tofi.window.entry.history);
			}
		}
	}
	tofi.window.entry.results = string_vec_copy(&tofi.window.entry.commands);

	/*
	 * Next, we create the Wayland surface, which takes on the
	 * layer shell role.
	 */
	log_debug("Creating main window surface.\n");
	tofi.window.surface.wl_surface =
		wl_compositor_create_surface(tofi.wl_compositor);
	wl_surface_add_listener(
			tofi.window.surface.wl_surface,
			&wl_surface_listener,
			&tofi);
	wl_surface_set_buffer_scale(
			tofi.window.surface.wl_surface,
			tofi.window.scale);

	/* Grab the first (and only remaining) output from our list. */
	struct wl_output *wl_output;
	{
		struct output_list_element *el;
		el = wl_container_of(tofi.output_list.next, el, link);
		wl_output = el->wl_output;
	}

	tofi.window.zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			tofi.zwlr_layer_shell,
			tofi.window.surface.wl_surface,
			wl_output,
			ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
			"launcher");
	zwlr_layer_surface_v1_set_keyboard_interactivity(
			tofi.window.zwlr_layer_surface,
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
			);
	zwlr_layer_surface_v1_add_listener(
			tofi.window.zwlr_layer_surface,
			&zwlr_layer_surface_listener,
			&tofi);
	zwlr_layer_surface_v1_set_anchor(
			tofi.window.zwlr_layer_surface,
			tofi.anchor);
	zwlr_layer_surface_v1_set_exclusive_zone(
			tofi.window.zwlr_layer_surface,
			tofi.window.exclusive_zone);
	zwlr_layer_surface_v1_set_size(
			tofi.window.zwlr_layer_surface,
			tofi.window.width / tofi.window.scale,
			tofi.window.height / tofi.window.scale);
	zwlr_layer_surface_v1_set_margin(
			tofi.window.zwlr_layer_surface,
			tofi.window.margin_top / tofi.window.scale,
			tofi.window.margin_right / tofi.window.scale,
			tofi.window.margin_bottom / tofi.window.scale,
			tofi.window.margin_left / tofi.window.scale);
	wl_surface_commit(tofi.window.surface.wl_surface);

	/*
	 * Create a data device and setup a listener for data offers. This is
	 * required for clipboard support.
	 */
	tofi.wl_data_device = wl_data_device_manager_get_data_device(
			tofi.wl_data_device_manager,
			tofi.wl_seat);
	wl_data_device_add_listener(
			tofi.wl_data_device,
			&wl_data_device_listener,
			&tofi.clipboard);


	/*
	 * Now that we've done all our Wayland-related setup, we do another
	 * roundtrip. This should cause the layer surface window to be
	 * configured, after which we're ready to start drawing to the screen.
	 */
	log_debug("Third roundtrip start.\n");
	log_indent();
	wl_display_roundtrip(tofi.wl_display);
	log_unindent();
	log_debug("Third roundtrip done.\n");


	/*
	 * Create the various structures for our window surface. This needs to
	 * be done before calling entry_init as that performs some initial
	 * drawing, and surface_init allocates the buffers we'll be drawing to.
	 */
	log_debug("Initialising window surface.\n");
	log_indent();
	surface_init(&tofi.window.surface, tofi.wl_shm);
	log_unindent();
	log_debug("Window surface initialised.\n");

	/*
	 * Initialise the structures for rendering the entry.
	 * Cairo needs to know the size of the surface it's creating, and
	 * there's no way to resize it aside from tearing everything down and
	 * starting again, so we make sure to do this after we've determined
	 * our output's scale factor. This stops us being able to change the
	 * scale factor after startup, but this is just a launcher, which
	 * shouldn't be moving between outputs while running.
	 */
	log_debug("Initialising renderer.\n");
	log_indent();
	entry_init(
			&tofi.window.entry,
			tofi.window.surface.shm_pool_data,
			tofi.window.width,
			tofi.window.height);
	log_unindent();
	log_debug("Renderer initialised.\n");

	/* Perform an initial render. */
	surface_draw(&tofi.window.surface);

	/*
	 * entry_init() left the second of the two buffers we use for
	 * double-buffering unpainted to lower startup time, as described
	 * there. Here, we flush our first, finished buffer to the screen, then
	 * copy over the image to the second buffer before we need to use it in
	 * the main loop. This ensures we paint to the screen as quickly as
	 * possible after startup.
	 */
	wl_display_roundtrip(tofi.wl_display);
	memcpy(
		cairo_image_surface_get_data(tofi.window.entry.cairo[1].surface),
		cairo_image_surface_get_data(tofi.window.entry.cairo[0].surface),
		tofi.window.entry.image.width * tofi.window.entry.image.height * sizeof(uint32_t)
	);

	/* We've just rendered, so we don't need to do it again right now. */
	tofi.window.surface.redraw = false;

	/* If we delayed keyboard initialisation, do it now */
	if (tofi.late_keyboard_init) {
		struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
				tofi.xkb_context,
				tofi.xkb_keymap_string,
				XKB_KEYMAP_FORMAT_TEXT_V1,
				XKB_KEYMAP_COMPILE_NO_FLAGS);

		struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
		xkb_keymap_unref(tofi.xkb_keymap);
		xkb_state_unref(tofi.xkb_state);
		tofi.xkb_keymap = xkb_keymap;
		tofi.xkb_state = xkb_state;
		free(tofi.xkb_keymap_string);
		tofi.late_keyboard_init = false;
	}

	/*
	 * Main event loop.
	 * See the wl_display(3) man page for an explanation of the
	 * order of the various functions called here.
	 */
	while (!tofi.closed) {
		struct pollfd pollfds[2] = {{0}, {0}};
		pollfds[0].fd = wl_display_get_fd(tofi.wl_display);

		/* Make sure we're ready to receive events on the main queue. */
		while (wl_display_prepare_read(tofi.wl_display) != 0) {
			wl_display_dispatch_pending(tofi.wl_display);
		}

		/* Make sure all our requests have been sent to the server. */
		while (wl_display_flush(tofi.wl_display) != 0) {
			pollfds[0].events = POLLOUT;
			poll(&pollfds[0], 1, -1);
		}

		/*
		 * Set time to wait for poll() to -1 (unlimited), unless
		 * there's some key repeating going on.
		 */
		int timeout = -1;
		if (tofi.repeat.active) {
			int64_t wait = (int64_t)tofi.repeat.next - (int64_t)gettime_ms();
			if (wait >= 0) {
				timeout = wait;
			} else {
				timeout = 0;
			}
		}

		pollfds[0].events = POLLIN | POLLPRI;
		int res;
		if (tofi.clipboard.fd == 0) {
			res = poll(&pollfds[0], 1, timeout);
		} else {
			/*
			 * We're trying to paste from the clipboard, which is
			 * done by reading from a pipe, so poll that file
			 * descriptor as well.
			 */
			pollfds[1].fd = tofi.clipboard.fd;
			pollfds[1].events = POLLIN | POLLPRI;
			res = poll(pollfds, 2, timeout);
		}
		if (res == 0) {
			/*
			 * No events to process and no error - we presumably
			 * have a key repeat to handle.
			 */
			wl_display_cancel_read(tofi.wl_display);
			if (tofi.repeat.active) {
				int64_t wait = (int64_t)tofi.repeat.next - (int64_t)gettime_ms();
				if (wait <= 0) {
					input_handle_keypress(&tofi, tofi.repeat.keycode);
					tofi.repeat.next += 1000 / tofi.repeat.rate;
				}
			}
		} else if (res < 0) {
			/* There was an error polling the display. */
			wl_display_cancel_read(tofi.wl_display);
		} else {
			if (pollfds[0].revents & (POLLIN | POLLPRI)) {
				/* Events to read, so put them on the queue. */
				wl_display_read_events(tofi.wl_display);
			} else {
				/*
				 * No events to read - we were woken up to
				 * handle clipboard data.
				 */
				wl_display_cancel_read(tofi.wl_display);
			}
			if (pollfds[1].revents & (POLLIN | POLLPRI)) {
				/* Read clipboard data. */
				if (tofi.clipboard.fd > 0) {
					read_clipboard(&tofi);
				}
			}
			if (pollfds[1].revents & POLLHUP) {
				/*
				 * The other end of the clipboard pipe has
				 * closed, cleanup.
				 */
				clipboard_finish_paste(&tofi.clipboard);
			}
		}

		/* Handle any events we read. */
		wl_display_dispatch_pending(tofi.wl_display);

		if (tofi.window.surface.redraw) {
			entry_update(&tofi.window.entry);
			surface_draw(&tofi.window.surface);
			tofi.window.surface.redraw = false;
		}
		if (tofi.submit) {
			tofi.submit = false;
			if (do_submit(&tofi)) {
				break;
			}
		}

	}

	log_debug("Window closed, performing cleanup.\n");
#ifdef DEBUG
	/*
	 * For debug builds, try to cleanup as much as possible, to make using
	 * e.g. Valgrind easier. There's still a few unavoidable leaks though,
	 * mostly from Pango, and Cairo holds onto quite a bit of cached data
	 * (without leaking it)
	 */
	surface_destroy(&tofi.window.surface);
	entry_destroy(&tofi.window.entry);
	zwlr_layer_surface_v1_destroy(tofi.window.zwlr_layer_surface);
	wl_surface_destroy(tofi.window.surface.wl_surface);
	if (tofi.wl_keyboard != NULL) {
		wl_keyboard_release(tofi.wl_keyboard);
	}
	if (tofi.wl_pointer != NULL) {
		wl_pointer_release(tofi.wl_pointer);
	}
	wl_compositor_destroy(tofi.wl_compositor);
	if (tofi.clipboard.wl_data_offer != NULL) {
		wl_data_offer_destroy(tofi.clipboard.wl_data_offer);
	}
	wl_data_device_release(tofi.wl_data_device);
	wl_data_device_manager_destroy(tofi.wl_data_device_manager);
	wl_seat_release(tofi.wl_seat);
	{
		struct output_list_element *el;
		struct output_list_element *tmp;
		wl_list_for_each_safe(el, tmp, &tofi.output_list, link) {
			wl_list_remove(&el->link);
			wl_output_release(el->wl_output);
			free(el->name);
			free(el);
		}
	}
	wl_shm_destroy(tofi.wl_shm);
	zwlr_layer_shell_v1_destroy(tofi.zwlr_layer_shell);
	xkb_state_unref(tofi.xkb_state);
	xkb_keymap_unref(tofi.xkb_keymap);
	xkb_context_unref(tofi.xkb_context);
	wl_registry_destroy(tofi.wl_registry);
	if (tofi.window.entry.drun) {
		desktop_vec_destroy(&tofi.window.entry.apps);
	}
	string_vec_destroy(&tofi.window.entry.commands);
	string_vec_destroy(&tofi.window.entry.results);
	if (tofi.use_history) {
		history_destroy(&tofi.window.entry.history);
	}
#endif
	/*
	 * For release builds, skip straight to display disconnection and quit.
	 */
	wl_display_roundtrip(tofi.wl_display);
	wl_display_disconnect(tofi.wl_display);

	log_debug("Finished, exiting.\n");
	return EXIT_SUCCESS;
}
