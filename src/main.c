#include <assert.h>
#include <wayland-egl.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "client.h"
#include "egl.h"
#include "xdg-shell-client-protocol.h"

void draw_frame(struct client_state *state)
{
	glClearColor(rand() / (double)RAND_MAX, rand() / (double)RAND_MAX, rand() / (double)RAND_MAX, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(state->egl.display, state->egl.surface);
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

	if (width != state->width || height != state->height) {
		state->width = width;
		state->height = height;
		printf("Resize: %d x %d\n", width, height);
		wl_egl_window_resize(state->egl.window, width, height, 0, 0);
		draw_frame(state);
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
       struct client_state *client_state = data;
       fprintf(stderr, "keyboard enter; keys pressed are:\n");
       uint32_t *key;
       wl_array_for_each(key, keys) {
               char buf[128];
               xkb_keysym_t sym = xkb_state_key_get_one_sym(
                               client_state->xkb_state, *key + 8);
               xkb_keysym_get_name(sym, buf, sizeof(buf));
               fprintf(stderr, "sym: %-12s (%d), ", buf, sym);
               xkb_state_key_get_utf8(client_state->xkb_state,
                               *key + 8, buf, sizeof(buf));
               fprintf(stderr, "utf8: '%s'\n", buf);
       }
}

static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
       struct client_state *client_state = data;
       char buf[128];
       uint32_t keycode = key + 8;
       xkb_keysym_t sym = xkb_state_key_get_one_sym(
                       client_state->xkb_state, keycode);
       xkb_keysym_get_name(sym, buf, sizeof(buf));
       const char *action =
               state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release";
       fprintf(stderr, "key %s: sym: %-12s (%d), ", action, buf, sym);
       xkb_state_key_get_utf8(client_state->xkb_state, keycode,
                       buf, sizeof(buf));
       fprintf(stderr, "utf8: '%s'\n", buf);
       if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
	       draw_frame(client_state);
       }
}

static void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface)
{
       fprintf(stderr, "keyboard leave\n");
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
       /* Left as an exercise for the reader */
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

static const struct wl_callback_listener wl_surface_frame_listener;

static void
wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);

	/* Request another frame */
	struct client_state *state = data;
	cb = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

	/* Update scroll amount at 24 pixels per second */
	if (state->last_frame != 0) {
		int elapsed = time - state->last_frame;
		state->offset += elapsed / 1000.0 * 24;
	}

	/* Submit a frame for this event */
	wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(state->wl_surface);

	state->last_frame = time;
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

static void
registry_global(void *data, struct wl_registry *wl_registry,
        uint32_t name, const char *interface, uint32_t version)
{
    struct client_state *state = data;
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(
                wl_registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = wl_registry_bind(
                wl_registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->wl_seat = wl_registry_bind(
                wl_registry, name, &wl_seat_interface, 7);
	wl_seat_add_listener(state->wl_seat,
			&wl_seat_listener, state);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(
                wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base,
                &xdg_wm_base_listener, state);
    }
}

static void
registry_global_remove(void *data,
        struct wl_registry *wl_registry, uint32_t name)
{
    /* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int main(int argc, char *argv[])
{
    struct client_state state = { 0 };
    state.width = 640;
    state.height = 480;
    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
            state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel,
		    &xdg_toplevel_listener, &state);
    xdg_toplevel_set_title(state.xdg_toplevel, "Greetd mini wayland greeter");

    wl_surface_commit(state.wl_surface);

    egl_create_window(&state);
    egl_create_context(&state);

    struct wl_callback *cb = wl_surface_frame(state.wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);

    wl_display_roundtrip(state.wl_display);
    draw_frame(&state);
    while (!state.closed) {
	    wl_display_dispatch(state.wl_display);
    }

    wl_display_disconnect(state.wl_display);

    return 0;
}
