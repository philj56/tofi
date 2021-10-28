#include <assert.h>
#include <wayland-egl.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>
#include <locale.h>
#include "client.h"
#include "egl.h"
#include "gl.h"
#include "background.h"
#include "xdg-shell-client-protocol.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);
static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

void draw_frame(struct surface *surface)
{
	printf("DRAW\n");
	surface->redraw = true;
}

static void
xdg_toplevel_configure(void *data,
	struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
	struct wl_array *states)
{
	struct client_state *state = data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us */
		return;
	}

	int32_t scaled_width = width * state->window.scale;
	int32_t scaled_height = height * state->window.scale;

	if (scaled_width != state->window.surface.width || scaled_height != state->window.surface.height) {
		state->window.surface.width = scaled_width;
		state->window.surface.height = scaled_height;
		wl_egl_window_resize(state->window.surface.egl.window, scaled_width, scaled_height, 0, 0);

		printf("%d, %d\n",
				(width - state->window.entry.surface.width / state->window.scale) / 2,
				(height - state->window.entry.surface.height / state->window.scale) / 2
				);
		wl_subsurface_set_position(state->window.entry.wl_subsurface,
				(width - state->window.entry.surface.width / state->window.scale) / 2,
				(height - state->window.entry.surface.height / state->window.scale) / 2
				);
		wl_surface_commit(state->window.entry.surface.wl_surface);
		draw_frame(&state->window.surface);
	}
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct client_state *state = data;
	state->closed = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close
};

static void
xdg_surface_configure(void *data,
        struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t format, int32_t fd, uint32_t size)
{
       struct client_state *client_state = data;
       assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

       char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
       assert(map_shm != MAP_FAILED);

       struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
                       client_state->xkb_context, map_shm,
                       XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
       munmap(map_shm, size);
       close(fd);

       struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
       xkb_keymap_unref(client_state->xkb_keymap);
       xkb_state_unref(client_state->xkb_state);
       client_state->xkb_keymap = xkb_keymap;
       client_state->xkb_state = xkb_state;
}

static void
wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface,
               struct wl_array *keys)
{
}

static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
       struct client_state *client_state = data;
       char buf[128];
       uint32_t keycode = key + 8;
       xkb_keysym_t sym = xkb_state_key_get_one_sym(client_state->xkb_state, keycode);
       xkb_keysym_get_name(sym, buf, sizeof(buf));
       if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
	       int len = xkb_state_key_get_utf8(client_state->xkb_state, keycode, buf, sizeof(buf));
	       wchar_t ch;
	       mbtowc(&ch, buf, sizeof(buf));
	       if (len > 0 && iswprint(ch)) {
		       fprintf(stderr, "%s\n", buf);
	       }
       }
}

static void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface)
{
}

static void
wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t mods_depressed,
               uint32_t mods_latched, uint32_t mods_locked,
               uint32_t group)
{
       struct client_state *client_state = data;
       xkb_state_update_mask(client_state->xkb_state,
               mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
               int32_t rate, int32_t delay)
{
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
       .keymap = wl_keyboard_keymap,
       .enter = wl_keyboard_enter,
       .leave = wl_keyboard_leave,
       .key = wl_keyboard_key,
       .modifiers = wl_keyboard_modifiers,
       .repeat_info = wl_keyboard_repeat_info,
};

static void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
       struct client_state *state = data;

       bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

       if (have_keyboard && state->wl_keyboard == NULL) {
	       state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
	       wl_keyboard_add_listener(state->wl_keyboard,
			       &wl_keyboard_listener, state);
       } else if (!have_keyboard && state->wl_keyboard != NULL) {
	       wl_keyboard_release(state->wl_keyboard);
	       state->wl_keyboard = NULL;
       }
}

static void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	fprintf(stderr, "seat name: %s\n", name);
}

static const struct wl_seat_listener wl_seat_listener = {
       .capabilities = wl_seat_capabilities,
       .name = wl_seat_name,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void
wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);
	fprintf(stderr, "callback\n");

	/* Request another frame */
	struct client_state *state = data;
	cb = wl_surface_frame(state->window.surface.wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

	/* Submit a frame for this event */
	wl_surface_commit(state->window.surface.wl_surface);
}

static void output_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
		int32_t subpixel, const char *make, const char *model, int32_t transform)
{
	/* Deliberately left blank */
}

static void output_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	/* Deliberately left blank */
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
	struct client_state *state = data;
	state->window.scale = MAX(factor, (int32_t)state->window.scale);
	wl_surface_set_buffer_scale(state->window.surface.wl_surface, state->window.scale);
	wl_surface_set_buffer_scale(state->window.entry.surface.wl_surface, state->window.scale);
}

static void output_done(void *data, struct wl_output *wl_output)
{
	/* TODO */
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static void registry_global(void *data, struct wl_registry *wl_registry,
		uint32_t name, const char *interface, uint32_t version)
{
	struct client_state *state = data;
	//fprintf(stderr, "%s\n", interface);
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(
				wl_registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		state->wl_subcompositor = wl_registry_bind(
				wl_registry, name, &wl_subcompositor_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(
				wl_registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat,
				&wl_seat_listener, state);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		state->wl_output = wl_registry_bind(
				wl_registry, name, &wl_output_interface, 3);
		wl_output_add_listener(state->wl_output,
				&wl_output_listener, state);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		state->xdg_wm_base = wl_registry_bind(
				wl_registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(state->xdg_wm_base,
				&xdg_wm_base_listener, state);
	}
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry,
		uint32_t name)
{
	/* Deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void surface_enter(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output)
{
	fprintf(stderr, "enter\n");
}

static void surface_leave(void *data, struct wl_surface *wl_surface, struct wl_output *wl_output)
{
	fprintf(stderr, "leave\n");
}

static const struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter,
	.leave = surface_leave
};

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	struct client_state state = { 0 };
	load_background(&state, "Night-1800.png");
	state.window.background_color = (struct color){ 0.2, 0.8, 0.8, 1.0 };
	state.window.surface.width = 640;
	state.window.surface.height = 480;
	state.window.entry.surface.width = 80;
	state.window.entry.surface.height = 40;
	state.wl_display = wl_display_connect(NULL);
	state.wl_registry = wl_display_get_registry(state.wl_display);
	state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);

	state.window.surface.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	state.window.scale = 1;
	wl_surface_add_listener(state.window.surface.wl_surface, &wl_surface_listener, &state);
	state.window.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.window.surface.wl_surface);
	xdg_surface_add_listener(state.window.xdg_surface, &xdg_surface_listener, &state);
	state.window.xdg_toplevel = xdg_surface_get_toplevel(state.window.xdg_surface);
	xdg_toplevel_add_listener(state.window.xdg_toplevel,
			&xdg_toplevel_listener, &state);
	xdg_toplevel_set_title(state.window.xdg_toplevel, "Greetd mini wayland greeter");
	wl_surface_commit(state.window.surface.wl_surface);

	state.window.entry.surface.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	state.window.entry.wl_subsurface = wl_subcompositor_get_subsurface(state.wl_subcompositor, state.window.entry.surface.wl_surface, state.window.surface.wl_surface);
	wl_subsurface_set_desync(state.window.entry.wl_subsurface);
	wl_surface_commit(state.window.entry.surface.wl_surface);


	struct surface *surface = &state.window.surface;
	egl_create_window(&surface->egl, surface->wl_surface, surface->width, surface->height);
	egl_create_context(&surface->egl, state.wl_display);
	gl_initialise(&state);

	surface = &state.window.entry.surface;
	egl_create_window(&surface->egl, surface->wl_surface, surface->width, surface->height);
	egl_create_context(&surface->egl, state.wl_display);


	wl_display_roundtrip(state.wl_display);
	egl_make_current(&state.window.surface.egl);
	eglSwapBuffers(state.window.surface.egl.display, state.window.surface.egl.surface);
	egl_make_current(&state.window.entry.surface.egl);
	eglSwapBuffers(state.window.entry.surface.egl.display, state.window.entry.surface.egl.surface);
	draw_frame(&state.window.surface);
	while (wl_display_dispatch(state.wl_display) != -1) {
		if (state.closed) {
			break;
		}
		if (state.window.surface.redraw) {
			egl_make_current(&state.window.surface.egl);
			gl_draw(&state);
			egl_swap_buffers(&state.window.surface.egl);
			state.window.surface.redraw = false;
		}
		if (state.window.entry.surface.redraw) {
			egl_make_current(&state.window.entry.surface.egl);
			egl_swap_buffers(&state.window.entry.surface.egl);
			state.window.entry.surface.redraw = false;
		}
	}

	wl_display_disconnect(state.wl_display);

	return 0;
}
