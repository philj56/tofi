#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>
#include "tofi.h"
#include "compgen.h"
#include "entry.h"
#include "image.h"
#include "log.h"
#include "nelem.h"
#include "shm.h"
#include "string_vec.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int get_anchor(int anchor)
{
	switch (anchor) {
		case 1:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		case 2:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
		case 3:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		case 4:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		case 5:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		case 6:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
		case 7:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
		case 8:
			return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	}
	return 0;
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

	/* Assume 4 bytes per pixel for WL_SHM_FORMAT_ARGB8888 */
	tofi->window.surface.stride = tofi->window.surface.width * 4;

	/*
	 * Need to redraw the background at the new size. This entails
	 * a wl_surface_commit, so no need to do so explicitly here.
	 */
	tofi->window.surface.redraw = true;
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

	char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(map_shm != MAP_FAILED);

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
	uint32_t keycode = key + 8;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(
			tofi->xkb_state,
			keycode);
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	struct entry *entry = &tofi->window.entry;
	char buf[5]; /* 4 UTF-8 bytes plus null terminator. */
	int len = xkb_state_key_get_utf8(
			tofi->xkb_state,
			keycode,
			buf,
			sizeof(buf));
	wchar_t ch;
	mbtowc(&ch, buf, sizeof(buf));
	if (len > 0 && iswprint(ch) && !iswblank(ch)) {
		if (entry->input_length < N_ELEM(entry->input) - 1) {
			entry->input[entry->input_length] = ch;
			entry->input_length++;
			entry->input[entry->input_length] = L'\0';
			memcpy(&entry->input_mb[entry->input_mb_length],
					buf,
					N_ELEM(buf));
			entry->input_mb_length += len;
			struct string_vec tmp = entry->results;
			entry->results = string_vec_filter(&entry->results, entry->input_mb);
			string_vec_destroy(&tmp);
		}
	} else if (entry->input_length > 0 && sym == XKB_KEY_BackSpace) {
		entry->input_length--;
		entry->input[entry->input_length] = L'\0';
		const wchar_t *src = entry->input;
		size_t siz = wcsrtombs(
				entry->input_mb,
				&src,
				N_ELEM(entry->input_mb),
				NULL);
		entry->input_mb_length = siz;
		string_vec_destroy(&entry->results);
		entry->results = string_vec_filter(&entry->commands, entry->input_mb);
	} else if (sym == XKB_KEY_Escape
			|| (sym == XKB_KEY_c
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
		  )
	{
		tofi->closed = true;
	} else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
		tofi->submit = true;
		return;
	}
	entry_update(&tofi->window.entry);
	tofi->window.surface.redraw = true;
	
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
	/* Deliberately left blank */
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
	/* Hide the cursor by setting its surface to NULL. */
	wl_pointer_set_cursor(tofi->wl_pointer, serial, NULL, 0, 0);
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
		/*
		 * We only need to listen to the cursor if we're going to hide
		 * it.
		 */
		if (tofi->hide_cursor) {
			tofi->wl_pointer = wl_seat_get_pointer(tofi->wl_seat);
			wl_pointer_add_listener(
					tofi->wl_pointer,
					&wl_pointer_listener,
					tofi);
			log_debug("Got pointer from seat.\n");
		}
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
	/* Deliberately left blank */
}

static void output_mode(
		void *data,
		struct wl_output *wl_output,
		uint32_t flags,
		int32_t width,
		int32_t height,
		int32_t refresh)
{
	/* Deliberately left blank */
}

static void output_scale(
		void *data,
		struct wl_output *wl_output,
		int32_t factor)
{
	struct tofi *tofi = data;
	tofi->window.scale = MAX(factor, (int32_t)tofi->window.scale);
	log_debug("Output scale factor is %d.\n", factor);
}

static void output_name(
		void *data,
		struct wl_output *wl_output,
		const char *name)
{
	/* Deliberately left blank */
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
	.name = output_name,
	.description = output_description,
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
		tofi->wl_output = wl_registry_bind(
				wl_registry,
				name,
				&wl_output_interface,
				4);
		wl_output_add_listener(
				tofi->wl_output,
				&wl_output_listener,
				tofi);
		log_debug("Bound to output %u.\n", name);
	} else if (!strcmp(interface, wl_shm_interface.name)) {
		tofi->wl_shm = wl_registry_bind(
				wl_registry,
				name,
				&wl_shm_interface,
				1);
		log_debug("Bound to shm %u.\n", name);
	} else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
		tofi->zwlr_layer_shell = wl_registry_bind(
				wl_registry,
				name,
				&zwlr_layer_shell_v1_interface,
				4);
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

static void usage()
{
	fprintf(stderr,
"Usage: tofi [options]\n"
"  -B, --background-color=COLOR   Color of the background.\n"
"  -o, --outline-width=VALUE      Width of the border outlines in pixels.\n"
"  -O, --outline-color=COLOR      Color of the border outlines.\n"
"  -r, --border-width=VALUE       Width of the border in pixels.\n"
"  -R, --border-color=COLOR       Color of the border.\n"
"  -e, --entry-padding=VALUE      Padding around the entry box in pixels.\n"
"  -E, --entry-color=COLOR        Color of the entry box.\n"
"  -f, --font-name=NAME           Font to use.\n"
"  -F, --font-size=VALUE          Point size of text.\n"
"  -T, --text-color=COLOR         Color of text.\n"
"  -H, --hide-cursor              Hide the cursor.\n"
"  -h, --help                     Print this message and exit.\n"
	);
}

int main(int argc, char *argv[])
{
	/*
	 * Set the locale to the user's default, so we can deal with non-ASCII
	 * characters.
	 */
	setlocale(LC_ALL, "");

	/* Default options. */
	struct tofi tofi = {
		.window = {
			.background_color = {0.89f, 0.8f, 0.824f, 1.0f},
			.scale = 1,
			.width = 640,
			.height = 320,
			.entry = {
				.border = {
					.width = 6,
					.outline_width = 2,
					.color = {0.976f, 0.149f, 0.447f, 1.0f},
					.outline_color = {0.031f, 0.031f, 0.0f, 1.0f},
				},
				.font_name = "Sans Bold",
				.font_size = 24,
				.prompt_text = "run: ",
				.num_results = 5,
				.padding = 8,
				.background_color = {0.106f, 0.114f, 0.118f, 1.0f},
				.foreground_color = {1.0f, 1.0f, 1.0f, 1.0f}
			}
		}
	};

	log_debug("Generating command list.\n");
	log_indent();
	tofi.window.entry.history = history_load();
	tofi.window.entry.commands = compgen_cached();
	compgen_history_sort(&tofi.window.entry.commands, &tofi.window.entry.history);
	tofi.window.entry.results = string_vec_copy(&tofi.window.entry.commands);
	log_unindent();
	log_debug("Command list generated.\n");


	/* Option parsing with getopt. */
	struct option long_options[] = {
		{"anchor", required_argument, NULL, 'a'},
		{"background-color", required_argument, NULL, 'B'},
		{"corner-radius", required_argument, NULL, 'c'},
		{"entry-padding", required_argument, NULL, 'e'},
		{"entry-color", required_argument, NULL, 'E'},
		{"font-name", required_argument, NULL, 'f'},
		{"font-size", required_argument, NULL, 'F'},
		{"num-results", required_argument, NULL, 'n'},
		{"outline-width", required_argument, NULL, 'o'},
		{"outline-color", required_argument, NULL, 'O'},
		{"prompt-text", required_argument, NULL, 'p'},
		{"result-padding", required_argument, NULL, 'P'},
		{"border-width", required_argument, NULL, 'r'},
		{"border-color", required_argument, NULL, 'R'},
		{"text-color", required_argument, NULL, 'T'},
		{"width", required_argument, NULL, 'X'},
		{"height", required_argument, NULL, 'Y'},
		{"x-offset", required_argument, NULL, 'x'},
		{"y-offset", required_argument, NULL, 'y'},
		{"layout-horizontal", no_argument, NULL, 'l'},
		{"hide-cursor", no_argument, NULL, 'H'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	const char *short_options = ":a:B:c:e:E:f:F:hHln:o:O:p:P:r:R:T:x:X:y:Y:";

	int opt = getopt_long(argc, argv, short_options, long_options, NULL);
	while (opt != -1) {
		switch (opt) {
			case 'a':
				tofi.anchor =
					get_anchor(strtol(optarg, NULL, 0));
				break;
			case 'B':
				tofi.window.background_color =
					hex_to_color(optarg);
				break;
			case 'c':
				tofi.window.entry.corner_radius =
					strtoul(optarg, NULL, 0);
				break;
			case 'r':
				tofi.window.entry.border.width =
					strtoul(optarg, NULL, 0);
				break;
			case 'R':
				tofi.window.entry.border.color =
					hex_to_color(optarg);
				break;
			case 'o':
				tofi.window.entry.border.outline_width =
					strtoul(optarg, NULL, 0);
				break;
			case 'O':
				tofi.window.entry.border.outline_color =
					hex_to_color(optarg);
				break;
			case 'e':
				tofi.window.entry.padding =
					strtoul(optarg, NULL, 0);
				break;
			case 'E':
				tofi.window.entry.background_color =
					hex_to_color(optarg);
				break;
			case 'T':
				tofi.window.entry.foreground_color =
					hex_to_color(optarg);
				break;
			case 'f':
				tofi.window.entry.font_name = optarg;
				break;
			case 'F':
				tofi.window.entry.font_size =
					strtoul(optarg, NULL, 0);
				break;
			case 'H':
				tofi.hide_cursor = true;
				break;
			case 'p':
				tofi.window.entry.prompt_text = optarg;
				break;
			case 'P':
				tofi.window.entry.result_padding =
					strtol(optarg, NULL, 0);
				break;
			case 'n':
				tofi.window.entry.num_results =
					strtoul(optarg, NULL, 0);
				break;
			case 'X':
				tofi.window.width =
					strtoul(optarg, NULL, 0);
				break;
			case 'x':
				tofi.window.x =
					strtol(optarg, NULL, 0);
				break;
			case 'Y':
				tofi.window.height =
					strtoul(optarg, NULL, 0);
				break;
			case 'y':
				tofi.window.y =
					strtol(optarg, NULL, 0);
				break;
			case 'l':
				tofi.window.entry.horizontal = true;
				break;
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
				break;
			case ':':
				log_error(
					"Option %s requires an argument.\n",
					argv[optind - 1]);
				usage();
				exit(EXIT_FAILURE);
				break;
			case '?':
				log_error(
					"Unknown option %s.\n",
					argv[optind - 1]);
				usage();
				exit(EXIT_FAILURE);
				break;
		}
		opt = getopt_long(argc, argv, short_options, long_options, NULL);
	}
	if (optind < argc) {
		log_error(
			"Unexpected non-option argument '%s'.\n",
			argv[optind]);
		usage();
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
	 * configured, telling us the scale factor.
	 */
	log_debug("Second roundtrip start.\n");
	log_indent();
	wl_display_roundtrip(tofi.wl_display);
	log_unindent();
	log_debug("Second roundtrip done.\n");

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

	tofi.window.zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			tofi.zwlr_layer_shell,
			tofi.window.surface.wl_surface,
			tofi.wl_output,
			ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
			"launcher");
	zwlr_layer_surface_v1_set_keyboard_interactivity(
			tofi.window.zwlr_layer_surface,
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
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
			-1);
	zwlr_layer_surface_v1_set_size(
			tofi.window.zwlr_layer_surface,
			tofi.window.width,
			tofi.window.height);
	zwlr_layer_surface_v1_set_margin(
			tofi.window.zwlr_layer_surface,
			tofi.window.x,
			tofi.window.y,
			tofi.window.x,
			tofi.window.y);
	wl_surface_commit(tofi.window.surface.wl_surface);

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
			tofi.window.surface.width,
			tofi.window.surface.height,
			tofi.window.scale);
	entry_update(&tofi.window.entry);
	log_unindent();
	log_debug("Renderer initialised.\n");

	/* Perform an initial render. */
	surface_draw(
			&tofi.window.surface,
			&tofi.window.entry.background_color,
			&tofi.window.entry.image);

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

	while (wl_display_dispatch(tofi.wl_display) != -1) {
		if (tofi.closed) {
			break;
		}
		if (tofi.window.surface.redraw) {
			surface_draw(
					&tofi.window.surface,
					&tofi.window.entry.background_color,
					&tofi.window.entry.image);
			tofi.window.surface.redraw = false;
		}
		if (tofi.submit) {
			tofi.submit = false;
			if (tofi.window.entry.results.count > 0) {
				printf("%s\n", tofi.window.entry.results.buf[0]);
				history_add(
						&tofi.window.entry.history,
						tofi.window.entry.results.buf[0]);
				history_save(&tofi.window.entry.history);
				break;
			}
		}
	}

	log_debug("Window closed, performing cleanup.\n");
#ifdef DEBUG
	/*
	 * For debug builds, try to cleanup as much as possible, to make using
	 * e.g. Valgrind easier. There's still a few unavoidable leaks though,
	 * mostly from Pango.
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
	wl_seat_release(tofi.wl_seat);
	wl_output_release(tofi.wl_output);
	wl_shm_destroy(tofi.wl_shm);
	zwlr_layer_shell_v1_destroy(tofi.zwlr_layer_shell);
	xkb_state_unref(tofi.xkb_state);
	xkb_keymap_unref(tofi.xkb_keymap);
	xkb_context_unref(tofi.xkb_context);
	wl_registry_destroy(tofi.wl_registry);
	string_vec_destroy(&tofi.window.entry.commands);
	string_vec_destroy(&tofi.window.entry.results);
	history_destroy(&tofi.window.entry.history);
#endif
	/*
	 * For release builds, skip straight to display disconnection and quit.
	 */
	wl_display_roundtrip(tofi.wl_display);
	wl_display_disconnect(tofi.wl_display);

	log_debug("Finished, exiting.\n");
	return EXIT_SUCCESS;
}
