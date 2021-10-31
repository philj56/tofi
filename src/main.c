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
#include <locale.h>
#include "client.h"
#include "egl.h"
#include "entry.h"
#include "image.h"
#include "gl.h"
#include "log.h"
#include "nelem.h"
#include "xdg-shell-client-protocol.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
			fprintf(stderr, "%ls\n", entry->password);
		} else if (entry->password_length > 0 && sym == XKB_KEY_BackSpace) {
			entry->password[entry->password_length - 1] = '\0';
			entry->password_length--;
		} else if (sym == XKB_KEY_Escape) {
			entry->password[0] = '\0';
			entry->password_length = 0;
		} else if (entry->password_length > 0
				&& (sym == XKB_KEY_Return
					|| sym == XKB_KEY_KP_Enter)) {
			entry->password[0] = '\0';
			entry->password_length = 0;
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

static void wl_seat_capabilities(
		void *data,
		struct wl_seat *wl_seat,
		uint32_t capabilities)
{
	struct client_state *state = data;

	bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

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
	//struct client_state *state = data;
	/* TODO */
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
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_compositor_interface,
				4);
		log_debug("Bound to compositor %u.\n", name);
	} else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		state->wl_subcompositor = wl_registry_bind(
				wl_registry,
				name,
				&wl_subcompositor_interface,
				1);
		log_debug("Bound to subcompositor %u.\n", name);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
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
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
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
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
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
	/* TODO */
	fprintf(stderr, "TODO: enter\n");
}

static void surface_leave(
		void *data,
		struct wl_surface *wl_surface,
		struct wl_output *wl_output)
{
	/* TODO */
	fprintf(stderr, "TODO: leave\n");
}

static const struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave
};

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	struct client_state state = {
		.window = {
			.background_color = {0.89, 0.8, 0.824, 1.0},
			.scale = 1,
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
				.password_character = L'·',
				.num_characters = 12,
				.background_color = {0.106, 0.114, 0.118, 1.0},
				.foreground_color = {1.0, 1.0, 1.0, 1.0}
			}
		}
	};


	struct option long_options[] = {
		{"background_image", required_argument, NULL, 'b'},
		{"background_color", required_argument, NULL, 'B'},
		{"border_width", required_argument, NULL, 'r'},
		{"border_color", required_argument, NULL, 'R'},
		{"outline_width", required_argument, NULL, 'o'},
		{"outline_color", required_argument, NULL, 'O'},
		{"entry_padding", required_argument, NULL, 'e'},
		{"entry_color", required_argument, NULL, 'E'},
		{"text_color", required_argument, NULL, 'T'},
		{"font_name", required_argument, NULL, 'f'},
		{"font_size", required_argument, NULL, 'F'},
		{"password_character", required_argument, NULL, 'c'},
		{"width_characters", required_argument, NULL, 'n'},
		{"wide_layout", no_argument, NULL, 'w'},
		{NULL, 0, NULL, 0}
	};
	const char *short_options = "b:B:e:E:f:F:r:R:n:o:O:c:T:w";

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
				break;
			case 'F':
				state.window.entry.font_size =
					strtol(optarg, NULL, 0);
				break;
			case 'c':
				mbrtowc(
					&state.window.entry.password_character,
					optarg,
					4,
					NULL);
				break;
			case 'n':
				state.window.entry.num_characters =
					strtol(optarg, NULL, 0);
			case 'w':
				state.window.entry.tight_layout = false;
				break;
			case '?':
				break;
		}
		opt = getopt_long(argc, argv, short_options, long_options, NULL);
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
	}

	log_info("Window closed, performing cleanup.\n");
	wl_display_disconnect(state.wl_display);

	log_info("Finished, exiting.\n");
	return 0;
}
