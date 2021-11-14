#include <assert.h>
#include <wayland-egl.h>
#include <epoxy/gl.h>
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
#include <xdg-shell.h>
#include <xkbcommon/xkbcommon.h>
#include "tofi.h"
#include "compgen.h"
#include "egl.h"
#include "entry.h"
#include "image.h"
#include "gl.h"
#include "log.h"
#include "nelem.h"
#include "string_vec.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))


static void resize(struct tofi *tofi)
{
	struct surface *surface = &tofi->window.surface;
	struct surface *entry_surface = &tofi->window.entry.surface;

	/*
	 * Resize the main window.
	 * EGL wants actual pixel width / height, so we have to scale the
	 * values provided by Wayland.
	 */
	surface->width = tofi->window.width * tofi->window.scale;
	surface->height = tofi->window.height * tofi->window.scale;
	wl_egl_window_resize(
			surface->egl.window,
			surface->width,
			surface->height,
			0,
			0);

	/*
	 * Need to redraw the background at the new size. This entails a
	 * wl_surface_commit, so no need to do so explicitly here.
	 */
	tofi->window.surface.redraw = true;

	/*
	 * Center the entry.
	 * Wayland wants "surface-local" width / height, so we have to divide
	 * the entry's pixel size by the scale factor.
	 */
	int32_t x = (
			tofi->window.width
			- entry_surface->width / tofi->window.scale
		    ) / 2;
	int32_t y = (
			tofi->window.height
			- entry_surface->height / tofi->window.scale
		    ) / 2;
	wl_subsurface_set_position(tofi->window.entry.wl_subsurface, x, y);
	wl_surface_commit(tofi->window.entry.surface.wl_surface);
}

static void xdg_toplevel_configure(
		void *data,
		struct xdg_toplevel *xdg_toplevel,
		int32_t width,
		int32_t height,
		struct wl_array *states)
{
	struct tofi *tofi = data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us, so don't do anything. */
		log_debug("XDG toplevel configure with no width or height.\n");
		return;
	}
	log_debug("XDG toplevel configure, %d x %d.\n", width, height);
	if (width != tofi->window.width || height != tofi->window.height) {
		tofi->window.width = width;
		tofi->window.height = height;
		tofi->window.resize = true;
	}
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct tofi *tofi = data;
	tofi->closed = true;
	log_debug("XDG toplevel close.\n");
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close
};

static void xdg_surface_configure(
		void *data,
		struct xdg_surface *xdg_surface,
		uint32_t serial)
{
	xdg_surface_ack_configure(xdg_surface, serial);
	log_debug("XDG surface configured.\n");
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
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

	char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
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
	if (len > 0 && iswprint(ch)) {
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
	} else if (entry->input_length > 0
			&& (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter)) {
		tofi->submit = true;
		return;
	}
	entry_update(&tofi->window.entry);
	tofi->window.entry.surface.redraw = true;
	
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

static void xdg_wm_base_ping(
		void *data,
		struct xdg_wm_base *xdg_wm_base,
		uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
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

static void output_done(void *data, struct wl_output *wl_output)
{
	log_debug("Output configuration done.\n");
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static void registry_global(
		void *data,
		struct wl_registry *wl_registry,
		uint32_t name,
		const char *interface,
		uint32_t version)
{
	struct tofi *tofi = data;
	log_debug("Registry %s %u.\n", interface, name);
	if (!strcmp(interface, wl_compositor_interface.name)) {
		tofi->wl_compositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_compositor_interface,
				4);
		log_debug("Bound to compositor %u.\n", name);
	} else if (!strcmp(interface, wl_subcompositor_interface.name)) {
		tofi->wl_subcompositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_subcompositor_interface,
				1);
		log_debug("Bound to subcompositor %u.\n", name);
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
				3);
		wl_output_add_listener(
				tofi->wl_output,
				&wl_output_listener,
				tofi);
		log_debug("Bound to output %u.\n", name);
	} else if (!strcmp(interface, xdg_wm_base_interface.name)) {
		tofi->xdg_wm_base = wl_registry_bind(
				wl_registry,
				name,
				&xdg_wm_base_interface,
				1);
		xdg_wm_base_add_listener(
				tofi->xdg_wm_base,
				&xdg_wm_base_listener,
				tofi);
		log_debug("Bound to xdg_wm_base %u.\n", name);
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
	log_debug("TODO: surface entered output.\n");
}

static void surface_leave(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	log_debug("TODO: surface left output.\n");
}

static const struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave
};

static void usage()
{
	fprintf(stderr,
"Usage: tofi [options]\n"
"  -u, --user=NAME                The user to login as.\n"
"  -c, --command=COMMAND          The command to run on login.\n"
"  -b, --background-image=PATH    An image to use as the background.\n"
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
"  -n, --width-characters=VALUE   Width of the entry box in characters.\n"
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
		.username = "nobody",
		.command = "false",
		.window = {
			.background_color = {0.89, 0.8, 0.824, 1.0},
			.scale = 1,
			.width = 640,
			.height = 480,
			.surface = { .width = 640, .height = 480 },
			.entry = {
				.border = {
					.width = 6,
					.outline_width = 2,
					.color = {0.976, 0.149, 0.447, 1.0},
					.outline_color = {0.031, 0.031, 0.0, 1.0},
				},
				.font_name = "Sans Bold",
				.font_size = 24,
				.padding = 8,
				.num_characters = 12,
				.background_color = {0.106, 0.114, 0.118, 1.0},
				.foreground_color = {1.0, 1.0, 1.0, 1.0}
			}
		}
	};

	tofi.window.entry.commands = compgen();
	tofi.window.entry.results = string_vec_copy(&tofi.window.entry.commands);


	/* Option parsing with getopt. */
	struct option long_options[] = {
		{"background-image", required_argument, NULL, 'b'},
		{"background-color", required_argument, NULL, 'B'},
		{"border-width", required_argument, NULL, 'r'},
		{"border-color", required_argument, NULL, 'R'},
		{"outline-width", required_argument, NULL, 'o'},
		{"outline-color", required_argument, NULL, 'O'},
		{"entry-padding", required_argument, NULL, 'e'},
		{"entry-color", required_argument, NULL, 'E'},
		{"text-color", required_argument, NULL, 'T'},
		{"font-name", required_argument, NULL, 'f'},
		{"font-size", required_argument, NULL, 'F'},
		{"command", required_argument, NULL, 'c'},
		{"user", required_argument, NULL, 'u'},
		{"width-characters", required_argument, NULL, 'n'},
		{"hide-cursor", no_argument, NULL, 'H'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	const char *short_options = ":b:B:c:e:E:f:F:hHr:R:n:o:O:T:u:";

	int opt = getopt_long(argc, argv, short_options, long_options, NULL);
	while (opt != -1) {
		switch (opt) {
			case 'b':
				image_load(
					&tofi.window.background_image,
					optarg);
				break;
			case 'B':
				tofi.window.background_color =
					hex_to_color(optarg);
				break;
			case 'r':
				tofi.window.entry.border.width =
					strtol(optarg, NULL, 0);
				break;
			case 'R':
				tofi.window.entry.border.color =
					hex_to_color(optarg);
				break;
			case 'o':
				tofi.window.entry.border.outline_width =
					strtol(optarg, NULL, 0);
				break;
			case 'O':
				tofi.window.entry.border.outline_color =
					hex_to_color(optarg);
				break;
			case 'e':
				tofi.window.entry.padding =
					strtol(optarg, NULL, 0);
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
					strtol(optarg, NULL, 0);
				break;
			case 'c':
				tofi.command = optarg;
				break;
			case 'u':
				tofi.username = optarg;
				break;
			case 'n':
				tofi.window.entry.num_characters =
					strtol(optarg, NULL, 0);
				break;
			case 'H':
				tofi.hide_cursor = true;
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
	wl_display_roundtrip(tofi.wl_display);
	log_debug("First roundtrip done.\n");

	/*
	 * The next roundtrip causes the listeners we set up in
	 * registry_global() to be called. Notably, the output should be
	 * configured, telling us the scale factor.
	 */
	log_debug("Second roundtrip start.\n");
	wl_display_roundtrip(tofi.wl_display);
	log_debug("Second roundtrip done.\n");

	/*
	 * Next, we create the Wayland surfaces that we need - one for
	 * the whole window, and another for the entry box.
	 */
	/*
	 * The main window surface takes on the xdg_surface and xdg_toplevel
	 * roles, in order to receive configure events to change its size.
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

	tofi.window.xdg_surface = xdg_wm_base_get_xdg_surface(
			tofi.xdg_wm_base,
			tofi.window.surface.wl_surface);
	xdg_surface_add_listener(
			tofi.window.xdg_surface,
			&xdg_surface_listener,
			&tofi);

	tofi.window.xdg_toplevel =
		xdg_surface_get_toplevel(tofi.window.xdg_surface);
	xdg_toplevel_add_listener(
			tofi.window.xdg_toplevel,
			&xdg_toplevel_listener,
			&tofi);
	xdg_toplevel_set_title(
			tofi.window.xdg_toplevel,
			"Greetd mini wayland greeter");

	wl_surface_commit(tofi.window.surface.wl_surface);

	/*
	 * The entry surface takes on a subsurface role and is set to be
	 * desynchronised, so that we can update it independently from the main
	 * window.
	 */
	log_debug("Creating entry surface.\n");
	tofi.window.entry.surface.wl_surface =
		wl_compositor_create_surface(tofi.wl_compositor);
	wl_surface_set_buffer_scale(
			tofi.window.entry.surface.wl_surface,
			tofi.window.scale);

	tofi.window.entry.wl_subsurface = wl_subcompositor_get_subsurface(
			tofi.wl_subcompositor,
			tofi.window.entry.surface.wl_surface,
			tofi.window.surface.wl_surface);
	wl_subsurface_set_desync(tofi.window.entry.wl_subsurface);

	wl_surface_commit(tofi.window.entry.surface.wl_surface);

	/*
	 * Initialise the Pango & Cairo structures for rendering the entry.
	 * Cairo needs to know the size of the surface it's creating, and
	 * there's no way to resize it aside from tearing everything down and
	 * starting again, so we make sure to do this after we've determined
	 * our output's scale factor. This stops us being able to change the
	 * scale factor after startup, but this is just a launcher, which
	 * shouldn't be moving between outputs while running.
	 */
	log_debug("Initialising Pango / Cairo.\n");
	entry_init(&tofi.window.entry, tofi.window.scale);
	log_debug("Pango / Cairo initialised.\n");

	/* Tell the compositor not to make our window smaller than the entry. */
	xdg_toplevel_set_min_size(
			tofi.window.xdg_toplevel,
			tofi.window.entry.surface.width / tofi.window.scale,
			tofi.window.entry.surface.height / tofi.window.scale);

	/*
	 * Now that we've done all our Wayland-related setup, we do another
	 * roundtrip. This should cause the XDG toplevel window to be
	 * configured, after which we're ready to start drawing to the screen.
	 */
	log_debug("Third roundtrip start.\n");
	wl_display_roundtrip(tofi.wl_display);
	log_debug("Third roundtrip done.\n");

	/*
	 * Create the various EGL and GL structures for each surface, and
	 * perform an initial render of everything.
	 */
	log_debug("Initialising main window surface.\n");
	surface_initialise(
			&tofi.window.surface,
			tofi.wl_display,
			&tofi.window.background_image);
	surface_draw(
			&tofi.window.surface,
			&tofi.window.background_color,
			&tofi.window.background_image);

	log_debug("Initialising entry window surface.\n");
	surface_initialise(
			&tofi.window.entry.surface,
			tofi.wl_display,
			&tofi.window.entry.image);

	log_debug("Initial draw\n");
	entry_update(&tofi.window.entry);
	surface_draw(
			&tofi.window.entry.surface,
			&tofi.window.background_color,
			&tofi.window.entry.image);

	/* Call resize() just to center the entry properly. */
	resize(&tofi);

	/*
	 * We've just rendered everything and resized, so we don't need to do
	 * it again right now.
	 */
	tofi.window.resize = false;
	tofi.window.surface.redraw = false;
	tofi.window.entry.surface.redraw = false;

	while (wl_display_dispatch(tofi.wl_display) != -1) {
		if (tofi.closed) {
			break;
		}
		if (tofi.window.resize) {
			resize(&tofi);
			tofi.window.resize = false;
		}
		if (tofi.window.surface.redraw) {
			surface_draw(
					&tofi.window.surface,
					&tofi.window.background_color,
					&tofi.window.background_image);
			tofi.window.surface.redraw = false;
		}
		if (tofi.window.entry.surface.redraw) {
			surface_draw(
					&tofi.window.entry.surface,
					&tofi.window.background_color,
					&tofi.window.entry.image);
			tofi.window.entry.surface.redraw = false;
		}
		if (tofi.submit) {
			if (tofi.window.entry.results.count > 0) {
				printf("%s\n", tofi.window.entry.results.buf[0]);
			}
			break;
		}
	}

	log_debug("Window closed, performing cleanup.\n");
#ifdef DEBUG
	/*
	 * For debug builds, try to cleanup as much as possible, to make using
	 * e.g. Valgrind easier. There's still a few unavoidable leaks though,
	 * mostly from OpenGL libs and Pango.
	 */
	entry_destroy(&tofi.window.entry);
	surface_destroy(&tofi.window.entry.surface);
	surface_destroy(&tofi.window.surface);
	if (tofi.window.background_image.buffer != NULL) {
		free(tofi.window.background_image.buffer);
		tofi.window.background_image.buffer = NULL;
	}
	eglTerminate(tofi.window.surface.egl.display);
	wl_subsurface_destroy(tofi.window.entry.wl_subsurface);
	wl_surface_destroy(tofi.window.entry.surface.wl_surface);
	xdg_toplevel_destroy(tofi.window.xdg_toplevel);
	xdg_surface_destroy(tofi.window.xdg_surface);
	wl_surface_destroy(tofi.window.surface.wl_surface);
	if (tofi.wl_keyboard != NULL) {
		wl_keyboard_release(tofi.wl_keyboard);
	}
	if (tofi.wl_pointer != NULL) {
		wl_pointer_release(tofi.wl_pointer);
	}
	wl_compositor_destroy(tofi.wl_compositor);
	wl_subcompositor_destroy(tofi.wl_subcompositor);
	wl_seat_release(tofi.wl_seat);
	wl_output_release(tofi.wl_output);
	xdg_wm_base_destroy(tofi.xdg_wm_base);
	xkb_state_unref(tofi.xkb_state);
	xkb_keymap_unref(tofi.xkb_keymap);
	xkb_context_unref(tofi.xkb_context);
	wl_registry_destroy(tofi.wl_registry);
#endif
	/*
	 * For release builds, skip straight to display disconnection and quit.
	 */
	wl_display_disconnect(tofi.wl_display);

	log_debug("Finished, exiting.\n");
	return EXIT_SUCCESS;
}
