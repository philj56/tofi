#include <assert.h>
#include <wayland-egl.h>
#include <epoxy/gl.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wctype.h>
#include <wchar.h>
#include <xkbcommon/xkbcommon.h>
#include <xdg-shell.h>
#include <locale.h>
#include "client.h"
#include "egl.h"
#include "entry.h"
#include "image.h"
#include "gl.h"
#include "greetd.h"
#include "log.h"
#include "nelem.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))


static void handle_response(
		struct client_state *state,
		struct json_object *response,
		enum greetd_request_type request);
static void create_session(struct client_state *state);
static void start_session(struct client_state *state);
static void post_auth_message_response(struct client_state *state);
static void cancel_session(struct client_state *state);
static void restart_session(struct client_state *state);

static void resize(struct client_state *state)
{
	struct surface *surface = &state->window.surface;
	struct surface *entry_surface = &state->window.entry.surface;

	/*
	 * Resize the main window.
	 * EGL wants actual pixel width / height, so we have to scale the
	 * values provided by Wayland.
	 */
	surface->width = state->window.width * state->window.scale;
	surface->height = state->window.height * state->window.scale;
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
	state->window.surface.redraw = true;

	/*
	 * Center the password entry.
	 * Wayland wants "surface-local" width / height, so we have to divide
	 * the entry's pixel size by the scale factor.
	 */
	int32_t x = (
			state->window.width
			- entry_surface->width / state->window.scale
		    ) / 2;
	int32_t y = (
			state->window.height
			- entry_surface->height / state->window.scale
		    ) / 2;
	wl_subsurface_set_position( state->window.entry.wl_subsurface, x, y);
	wl_surface_commit(state->window.entry.surface.wl_surface);
}

static void xdg_toplevel_configure(
		void *data,
		struct xdg_toplevel *xdg_toplevel,
		int32_t width,
		int32_t height,
		struct wl_array *states)
{
	struct client_state *state = data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us, so don't do anything. */
		log_debug("XDG toplevel configure with no width or height.\n");
		return;
	}
	log_debug("XDG toplevel configure, %d x %d.\n", width, height);
	if (width != state->window.width || height != state->window.height) {
		state->window.width = width;
		state->window.height = height;
		state->window.resize = true;
	}
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct client_state *state = data;
	state->closed = true;
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
	struct client_state *client_state = data;
	assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

	char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	assert(map_shm != MAP_FAILED);

	struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
			client_state->xkb_context,
			map_shm,
			XKB_KEYMAP_FORMAT_TEXT_V1,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map_shm, size);
	close(fd);

	struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
	xkb_keymap_unref(client_state->xkb_keymap);
	xkb_state_unref(client_state->xkb_state);
	client_state->xkb_keymap = xkb_keymap;
	client_state->xkb_state = xkb_state;
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
	struct client_state *client_state = data;
	char buf[128];
	uint32_t keycode = key + 8;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(
			client_state->xkb_state,
			keycode);
	xkb_keysym_get_name(sym, buf, sizeof(buf));
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		struct entry *entry = &client_state->window.entry;
		int len = xkb_state_key_get_utf8(
				client_state->xkb_state,
				keycode,
				buf,
				sizeof(buf));
		wchar_t ch;
		mbtowc(&ch, buf, sizeof(buf));
		if (len > 0 && iswprint(ch)) {
			if (entry->password_length < N_ELEM(entry->password) - 1) {
				entry->password[entry->password_length] = ch;
				entry->password_length++;
				entry->password[entry->password_length] = L'\0';
			}
		} else if (entry->password_length > 0 && sym == XKB_KEY_BackSpace) {
			entry->password[entry->password_length - 1] = '\0';
			entry->password_length--;
		} else if (sym == XKB_KEY_Escape
				|| (sym == XKB_KEY_c
					&& xkb_state_mod_name_is_active(
						client_state->xkb_state,
						XKB_MOD_NAME_CTRL,
						XKB_STATE_MODS_EFFECTIVE)
				   )
			  )
		{
			entry->password[0] = '\0';
			entry->password_length = 0;
		} else if (entry->password_length > 0
				&& (sym == XKB_KEY_Return
					|| sym == XKB_KEY_KP_Enter)) {
			const wchar_t *src = entry->password;
			wcsrtombs(
					entry->password_mb,
					&src,
					N_ELEM(entry->password_mb),
					NULL);
			entry->password[0] = '\0';
			entry->password_length = 0;
			client_state->submit = true;
		}
		entry_update(&client_state->window.entry);
		client_state->window.entry.surface.redraw = true;
	}
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
	struct client_state *client_state = data;
	xkb_state_update_mask(
			client_state->xkb_state,
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
	struct client_state *state = data;
	/* Hide the cursor by setting its surface to NULL. */
	wl_pointer_set_cursor(state->wl_pointer, serial, NULL, 0, 0);
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
	struct client_state *state = data;

	bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
	bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

	if (have_keyboard && state->wl_keyboard == NULL) {
		state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
		wl_keyboard_add_listener(
				state->wl_keyboard,
				&wl_keyboard_listener,
				state);
		log_debug("Got keyboard from seat.\n");
	} else if (!have_keyboard && state->wl_keyboard != NULL) {
		wl_keyboard_release(state->wl_keyboard);
		state->wl_keyboard = NULL;
		log_debug("Released keyboard.\n");
	}

	if (have_pointer && state->wl_pointer == NULL) {
		/*
		 * We only need to listen to the cursor if we're going to hide
		 * it.
		 */
		if (state->hide_cursor) {
			state->wl_pointer = wl_seat_get_pointer(state->wl_seat);
			wl_pointer_add_listener(
					state->wl_pointer,
					&wl_pointer_listener,
					state);
			log_debug("Got pointer from seat.\n");
		}
	} else if (!have_pointer && state->wl_pointer != NULL) {
		wl_pointer_release(state->wl_pointer);
		state->wl_pointer = NULL;
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
	struct client_state *state = data;
	state->window.scale = MAX(factor, (int32_t)state->window.scale);
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
	struct client_state *state = data;
	if (!strcmp(interface, wl_compositor_interface.name)) {
		state->wl_compositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_compositor_interface,
				4);
		log_debug("Bound to compositor %u.\n", name);
	} else if (!strcmp(interface, wl_subcompositor_interface.name)) {
		state->wl_subcompositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_subcompositor_interface,
				1);
		log_debug("Bound to subcompositor %u.\n", name);
	} else if (!strcmp(interface, wl_seat_interface.name)) {
		state->wl_seat = wl_registry_bind(
				wl_registry,
				name,
				&wl_seat_interface,
				7);
		wl_seat_add_listener(
				state->wl_seat,
				&wl_seat_listener,
				state);
		log_debug("Bound to seat %u.\n", name);
	} else if (!strcmp(interface, wl_output_interface.name)) {
		state->wl_output = wl_registry_bind(
				wl_registry,
				name,
				&wl_output_interface,
				3);
		wl_output_add_listener(
				state->wl_output,
				&wl_output_listener,
				state);
		log_debug("Bound to output %u.\n", name);
	} else if (!strcmp(interface, xdg_wm_base_interface.name)) {
		state->xdg_wm_base = wl_registry_bind(
				wl_registry,
				name,
				&xdg_wm_base_interface,
				1);
		xdg_wm_base_add_listener(
				state->xdg_wm_base,
				&xdg_wm_base_listener,
				state);
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
"Usage: greetd-mini-wl-greeter -u username -c command [options]\n"
"  -u, --user=NAME                The user to login as.\n"
"  -c, --command=COMMAND          The command to run on login.\n"
"  -b, --background-image=PATH    An image to use as the background.\n"
"  -B, --background-color=COLOR   Color of the background.\n"
"  -o, --outline-width=VALUE      Width of the border outlines in pixels.\n"
"  -O, --outline-color=COLOR      Color of the border outlines.\n"
"  -r, --border-width=VALUE       Width of the border in pixels.\n"
"  -R, --border-color=COLOR       Color of the border.\n"
"  -e, --entry-padding=VALUE      Padding around the password text in pixels.\n"
"  -E, --entry-color=COLOR        Color of the password entry box.\n"
"  -f, --font-name=NAME           Font to use for the password entry.\n"
"  -F, --font-size=VALUE          Point size of the password text.\n"
"  -T, --text-color=COLOR         Color of the password text.\n"
"  -C, --password-character=CHAR  Character to use to hide the password.\n"
"  -n, --width-characters=VALUE   Width of the password entry box in characters.\n"
"  -w, --wide-layout              Make the password entry box full height.\n"
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
	struct client_state state = {
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
				.tight_layout = true,
				.password_character = '.',
				.num_characters = 12,
				.background_color = {0.106, 0.114, 0.118, 1.0},
				.foreground_color = {1.0, 1.0, 1.0, 1.0}
			}
		}
	};


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
		{"password-character", required_argument, NULL, 'C'},
		{"command", required_argument, NULL, 'c'},
		{"user", required_argument, NULL, 'u'},
		{"width-characters", required_argument, NULL, 'n'},
		{"wide-layout", no_argument, NULL, 'w'},
		{"hide-cursor", no_argument, NULL, 'H'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	const char *short_options = ":b:B:c:C:e:E:f:F:hHr:R:n:o:O:T:u:w";

	int opt = getopt_long(argc, argv, short_options, long_options, NULL);
	while (opt != -1) {
		switch (opt) {
			case 'b':
				image_load(
					&state.window.background_image,
					optarg);
				break;
			case 'B':
				state.window.background_color =
					hex_to_color(optarg);
				break;
			case 'r':
				state.window.entry.border.width =
					strtol(optarg, NULL, 0);
				break;
			case 'R':
				state.window.entry.border.color =
					hex_to_color(optarg);
				break;
			case 'o':
				state.window.entry.border.outline_width =
					strtol(optarg, NULL, 0);
				break;
			case 'O':
				state.window.entry.border.outline_color =
					hex_to_color(optarg);
				break;
			case 'e':
				state.window.entry.padding =
					strtol(optarg, NULL, 0);
				break;
			case 'E':
				state.window.entry.background_color =
					hex_to_color(optarg);
				break;
			case 'T':
				state.window.entry.foreground_color =
					hex_to_color(optarg);
				break;
			case 'f':
				state.window.entry.font_name = optarg;
				state.window.entry.use_pango = true;
				break;
			case 'F':
				state.window.entry.font_size =
					strtol(optarg, NULL, 0);
				break;
			case 'C':
				mbrtowc(
					&state.window.entry.password_character,
					optarg,
					4,
					NULL);
				state.window.entry.use_pango = true;
				break;
			case 'c':
				state.command = optarg;
				break;
			case 'u':
				state.username = optarg;
				break;
			case 'n':
				state.window.entry.num_characters =
					strtol(optarg, NULL, 0);
				break;
			case 'w':
				state.window.entry.tight_layout = false;
				state.window.entry.use_pango = true;
				break;
			case 'H':
				state.hide_cursor = true;
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
	state.wl_display = wl_display_connect(NULL);
	if (state.wl_display == NULL) {
		log_error("Couldn't connect to Wayland display.\n");
		exit(EXIT_FAILURE);
	}
	state.wl_registry = wl_display_get_registry(state.wl_display);
	state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (state.xkb_context == NULL) {
		log_error("Couldn't create an XKB context.\n");
		exit(EXIT_FAILURE);
	}
	wl_registry_add_listener(
			state.wl_registry,
			&wl_registry_listener,
			&state);

	/*
	 * After this first roundtrip, the only thing that should have happened
	 * is our registry_global() function being called and setting up the
	 * various global object bindings.
	 */
	log_debug("First roundtrip start.\n");
	wl_display_roundtrip(state.wl_display);
	log_debug("First roundtrip done.\n");

	/*
	 * The next roundtrip causes the listeners we set up in
	 * registry_global() to be called. Notably, the output should be
	 * configured, telling us the scale factor.
	 */
	log_debug("Second roundtrip start.\n");
	wl_display_roundtrip(state.wl_display);
	log_debug("Second roundtrip done.\n");

	/*
	 * Next, we create the Wayland surfaces that we need - one for
	 * the whole window, and another for the password entry box.
	 */
	/*
	 * The main window surface takes on the xdg_surface and xdg_toplevel
	 * roles, in order to receive configure events to change its size.
	 */
	log_debug("Creating main window surface.\n");
	state.window.surface.wl_surface =
		wl_compositor_create_surface(state.wl_compositor);
	wl_surface_add_listener(
			state.window.surface.wl_surface,
			&wl_surface_listener,
			&state);
	wl_surface_set_buffer_scale(
			state.window.surface.wl_surface,
			state.window.scale);

	state.window.xdg_surface = xdg_wm_base_get_xdg_surface(
			state.xdg_wm_base,
			state.window.surface.wl_surface);
	xdg_surface_add_listener(
			state.window.xdg_surface,
			&xdg_surface_listener,
			&state);

	state.window.xdg_toplevel =
		xdg_surface_get_toplevel(state.window.xdg_surface);
	xdg_toplevel_add_listener(
			state.window.xdg_toplevel,
			&xdg_toplevel_listener,
			&state);
	xdg_toplevel_set_title(
			state.window.xdg_toplevel,
			"Greetd mini wayland greeter");

	wl_surface_commit(state.window.surface.wl_surface);

	/*
	 * The password entry surface takes on a subsurface role and is set to
	 * be desynchronised, so that we can update it independently from the
	 * main window.
	 */
	log_debug("Creating password entry surface.\n");
	state.window.entry.surface.wl_surface =
		wl_compositor_create_surface(state.wl_compositor);
	wl_surface_set_buffer_scale(
			state.window.entry.surface.wl_surface,
			state.window.scale);

	state.window.entry.wl_subsurface = wl_subcompositor_get_subsurface(
			state.wl_subcompositor,
			state.window.entry.surface.wl_surface,
			state.window.surface.wl_surface);
	wl_subsurface_set_desync(state.window.entry.wl_subsurface);

	wl_surface_commit(state.window.entry.surface.wl_surface);

	/*
	 * Initialise the Pango & Cairo structures for rendering the password
	 * entry. Cairo needs to know the size of the surface it's creating,
	 * and there's no way to resize it aside from tearing everything down
	 * and starting again, so we make sure to do this after we've
	 * determined our output's scale factor. This stops us being able to
	 * change the scale factor after startup, but this is just a greeter,
	 * which shouldn't be moving between outputs while running.
	 */
	log_debug("Initialising Pango / Cairo.\n");
	entry_init(&state.window.entry, state.window.scale);
	log_debug("Pango / Cairo initialised.\n");

	/*
	 * Tell the compositor not to make our window smaller than the password
	 * entry.
	 */
	xdg_toplevel_set_min_size(
			state.window.xdg_toplevel,
			state.window.entry.surface.width / state.window.scale,
			state.window.entry.surface.height / state.window.scale);

	/*
	 * Now that we've done all our Wayland-related setup, we do another
	 * roundtrip. This should cause the XDG toplevel window to be
	 * configured, after which we're ready to start drawing to the screen.
	 */
	log_debug("Third roundtrip start.\n");
	wl_display_roundtrip(state.wl_display);
	log_debug("Third roundtrip done.\n");

	/*
	 * Create the various EGL and GL structures for each surface, and
	 * perform an initial render of everything.
	 */
	log_debug("Initialising main window surface.\n");
	surface_initialise(
			&state.window.surface,
			state.wl_display,
			&state.window.background_image);
	surface_draw(
			&state.window.surface,
			&state.window.background_color,
			&state.window.background_image);

	log_debug("Initialising entry window surface.\n");
	surface_initialise(
			&state.window.entry.surface,
			state.wl_display,
			&state.window.entry.image);
	surface_draw(
			&state.window.entry.surface,
			&state.window.background_color,
			&state.window.entry.image);

	/* Call resize() just to center the password entry properly. */
	resize(&state);

	/*
	 * We've just rendered everything and resized, so we don't need to do
	 * it again right now.
	 */
	state.window.resize = false;
	state.window.surface.redraw = false;
	state.window.entry.surface.redraw = false;

	/* Create the greetd session. */
	create_session(&state);

	while (wl_display_dispatch(state.wl_display) != -1) {
		if (state.closed) {
			break;
		}
		if (state.window.resize) {
			resize(&state);
			state.window.resize = false;
		}
		if (state.window.surface.redraw) {
			surface_draw(
					&state.window.surface,
					&state.window.background_color,
					&state.window.background_image);
			state.window.surface.redraw = false;
		}
		if (state.window.entry.surface.redraw) {
			surface_draw(
					&state.window.entry.surface,
					&state.window.background_color,
					&state.window.entry.image);
			state.window.entry.surface.redraw = false;
		}
		if (state.submit) {
			post_auth_message_response(&state);
			state.submit = false;
		}
	}

	log_debug("Window closed, performing cleanup.\n");
#ifdef DEBUG
	/*
	 * For debug builds, try to cleanup as much as possible, to make using
	 * e.g. Valgrind easier. There's still a few unavoidable leaks though,
	 * mostly from OpenGL libs and Pango.
	 */
	entry_destroy(&state.window.entry);
	surface_destroy(&state.window.entry.surface);
	surface_destroy(&state.window.surface);
	if (state.window.background_image.buffer != NULL) {
		free(state.window.background_image.buffer);
		state.window.background_image.buffer = NULL;
	}
	eglTerminate(state.window.surface.egl.display);
	wl_subsurface_destroy(state.window.entry.wl_subsurface);
	wl_surface_destroy(state.window.entry.surface.wl_surface);
	xdg_toplevel_destroy(state.window.xdg_toplevel);
	xdg_surface_destroy(state.window.xdg_surface);
	wl_surface_destroy(state.window.surface.wl_surface);
	if (state.wl_keyboard != NULL) {
		wl_keyboard_release(state.wl_keyboard);
	}
	if (state.wl_pointer != NULL) {
		wl_pointer_release(state.wl_pointer);
	}
	wl_compositor_destroy(state.wl_compositor);
	wl_subcompositor_destroy(state.wl_subcompositor);
	wl_seat_release(state.wl_seat);
	wl_output_release(state.wl_output);
	xdg_wm_base_destroy(state.xdg_wm_base);
	xkb_state_unref(state.xkb_state);
	xkb_keymap_unref(state.xkb_keymap);
	xkb_context_unref(state.xkb_context);
	wl_registry_destroy(state.wl_registry);
#endif
	/*
	 * For release builds, skip straight to display disconnection and quit.
	 */
	wl_display_disconnect(state.wl_display);

	log_debug("Finished, exiting.\n");
	return EXIT_SUCCESS;
}

void handle_response(
		struct client_state *state,
		struct json_object *response,
		enum greetd_request_type request)
{
	if (response == NULL) {
		return;
	}
	enum greetd_response_type type = greetd_parse_response_type(response);

	switch (type) {
		case GREETD_RESPONSE_SUCCESS:
			switch (request) {
				case GREETD_REQUEST_CREATE_SESSION:
				case GREETD_REQUEST_POST_AUTH_MESSAGE_RESPONSE:
					start_session(state);
					break;
				case GREETD_REQUEST_START_SESSION:
					state->closed = true;
					break;
				case GREETD_REQUEST_CANCEL_SESSION:
					break;
			}
			break;
		case GREETD_RESPONSE_ERROR:
			switch (request) {
				case GREETD_REQUEST_POST_AUTH_MESSAGE_RESPONSE:
				case GREETD_REQUEST_START_SESSION:
					log_error(
						"Failed to create greetd session: %s\n",
						json_object_get_string(
							json_object_object_get(
								response,
								"description")
							)
						);
					restart_session(state);
					break;
				case GREETD_REQUEST_CREATE_SESSION:
					log_error("Failed to connect to greetd session.\n");
					break;
				case GREETD_REQUEST_CANCEL_SESSION:
					break;
			}
			break;
		case GREETD_RESPONSE_AUTH_MESSAGE:
			switch (greetd_parse_auth_message_type(response)) {
				case GREETD_AUTH_MESSAGE_VISIBLE:
					state->window.entry.password_visible = true;
					break;
				case GREETD_AUTH_MESSAGE_SECRET:
					state->window.entry.password_visible = false;
					break;
				case GREETD_AUTH_MESSAGE_INFO:
				case GREETD_AUTH_MESSAGE_ERROR:
					/* TODO */
					restart_session(state);
					break;
				case GREETD_AUTH_MESSAGE_INVALID:
					break;
			}
			break;
		case GREETD_RESPONSE_INVALID:
			break;
	}
	json_object_put(response);
}

void create_session(struct client_state *state)
{
	log_debug("Creating greetd session for user '%s'.\n", state->username);
	handle_response(
			state,
			greetd_create_session(state->username),
			GREETD_REQUEST_CREATE_SESSION);
}

void start_session(struct client_state *state)
{
	log_debug("Starting session with command '%s'.\n", state->command);
	handle_response(
			state,
			greetd_start_session(state->command),
			GREETD_REQUEST_START_SESSION);
}

void post_auth_message_response(struct client_state *state)
{
	log_debug("Posting auth message response.\n");
	handle_response(
			state,
			greetd_post_auth_message_response(state->window.entry.password_mb),
			GREETD_REQUEST_POST_AUTH_MESSAGE_RESPONSE);
}

void cancel_session(struct client_state *state)
{
	log_debug("Cancelling session.\n");
	handle_response(
			state,
			greetd_cancel_session(),
			GREETD_REQUEST_CANCEL_SESSION);
}

void restart_session(struct client_state *state)
{
	cancel_session(state);
	create_session(state);
}
