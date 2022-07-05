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
#include <wayland-util.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>
#include "tofi.h"
#include "compgen.h"
#include "config.h"
#include "entry.h"
#include "image.h"
#include "log.h"
#include "nelem.h"
#include "shm.h"
#include "string_vec.h"
#include "xmalloc.h"

#undef MAX
#undef MIN
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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
	uint32_t keycode = key + 8;
	if (tofi->xkb_state == NULL) {
		return;
	}
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

	if (sym == XKB_KEY_Up || sym == XKB_KEY_Left
			|| (sym == XKB_KEY_k
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
			) {
		uint32_t nsel = MAX(MIN(tofi->window.entry.num_results, tofi->window.entry.results.count), 1);
		tofi->window.entry.selection += nsel;
		tofi->window.entry.selection--;
		tofi->window.entry.selection %= nsel;
	} else if (sym == XKB_KEY_Down || sym == XKB_KEY_Right || sym == XKB_KEY_Tab
			|| (sym == XKB_KEY_j
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
			) {
		uint32_t nsel = MAX(MIN(tofi->window.entry.num_results, tofi->window.entry.results.count), 1);
		tofi->window.entry.selection++;
		tofi->window.entry.selection %= nsel;
	} else {
		tofi->window.entry.selection = 0;
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
		struct output_list_element *el = xmalloc(sizeof(*el));
		el->wl_output = wl_registry_bind(
				wl_registry,
				name,
				&wl_output_interface,
				4);
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


static void usage()
{
	fprintf(stderr, "%s",
"Usage: tofi [options]\n"
"\n"
"  -h, --help                           Print this message and exit.\n"
"  -c, --config                         Specify a config file.\n"
"      --font <name|path>               Font to use.\n"
"      --font-size <pt>                 Point size of text.\n"
"      --background-color <color>       Color of the background.\n"
"      --outline-width <px>             Width of the border outlines.\n"
"      --outline-color <color>          Color of the border outlines.\n"
"      --border-width <px>              Width of the border.\n"
"      --border-color <color>           Color of the border.\n"
"      --text-color <color>             Color of text.\n"
"      --prompt-text <string>           Prompt text.\n"
"      --num-results <n>                Maximum number of results to display.\n"
"      --selection-color <color>        Color of selected result.\n"
"      --selection-background <color>   Color of selected result background.\n"
"      --result-spacing <px>            Spacing between results.\n"
"      --min-input-width <px>           Minimum input width in horizontal mode.\n"
"      --width <px|%>                   Width of the window.\n"
"      --height <px|%>                  Height of the window.\n"
"      --corner-radius <px>             Radius of window corners.\n"
"      --output <name>                  Name of output to display window on.\n"
"      --anchor <position>              Location on screen to anchor window.\n"
"      --margin-top <px|%>              Offset from top of screen.\n"
"      --margin-bottom <px|%>           Offset from bottom of screen.\n"
"      --margin-left <px|%>             Offset from left of screen.\n"
"      --margin-right <px|%>            Offset from right of screen.\n"
"      --padding-top <px|%>             Padding between top border and text.\n"
"      --padding-bottom <px|%>          Padding between bottom border and text.\n"
"      --padding-left <px|%>            Padding between left border and text.\n"
"      --padding-right <px|%>           Padding between right border and text.\n"
"      --hide-cursor <true|false>       Hide the cursor.\n"
"      --horizontal <true|false>        List results horizontally.\n"
"      --history <true|false>           Sort results by number of usages.\n"
"      --hint-font <true|false>         Perform font hinting.\n"
"      --late-keyboard-init             (EXPERIMENTAL) Delay keyboard\n"
"                                       initialisation until after the first\n"
"                                       draw to screen.\n"
	);
}

/* Option parsing with getopt. */
const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", required_argument, NULL, 'c'},
	{"anchor", required_argument, NULL, 0},
	{"background-color", required_argument, NULL, 0},
	{"corner-radius", required_argument, NULL, 0},
	{"font", required_argument, NULL, 0},
	{"font-size", required_argument, NULL, 0},
	{"num-results", required_argument, NULL, 0},
	{"selection-color", required_argument, NULL, 0},
	{"selection-background", required_argument, NULL, 0},
	{"outline-width", required_argument, NULL, 0},
	{"outline-color", required_argument, NULL, 0},
	{"prompt-text", required_argument, NULL, 0},
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
	{"hint-font", required_argument, NULL, 0},
	{"output", required_argument, NULL, 'o'},
	{"late-keyboard-init", no_argument, NULL, 'k'},
	{NULL, 0, NULL, 0}
};
const char *short_options = ":hc:";

static void parse_early_args(struct tofi *tofi, int argc, char *argv[])
{
	/* Handle errors ourselves (i.e. ignore them for now). */
	opterr = 0;

	/* Just check for help, late-keyboard-init and output */
	optind = 1;
	int opt = getopt_long(argc, argv, short_options, long_options, NULL);
	while (opt != -1) {
		if (opt == 'h') {
			usage();
			exit(EXIT_SUCCESS);
		} else if (opt == 'k') {
			tofi->late_keyboard_init = true;
		} else if (opt == 'o') {
			snprintf(tofi->target_output_name, N_ELEM(tofi->target_output_name), "%s", optarg);
		}
		opt = getopt_long(argc, argv, short_options, long_options, NULL);
	}
}

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
			usage();
			exit(EXIT_SUCCESS);
		} else if (opt == 'c') {
			config_load(tofi, optarg);
			load_default_config = false;
		} else if (opt == ':') {
			log_error("Option %s requires an argument.\n", argv[optind - 1]);
			usage();
			exit(EXIT_FAILURE);
		} else if (opt == '?') {
			if (optopt) {
				log_error("Unknown option -%c.\n", optopt);
			} else {
				log_error("Unknown option %s.\n", argv[optind - 1]);
			}
			usage();
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
			apply_option(tofi, long_options[option_index].name, optarg);
		}
		opt = getopt_long(argc, argv, short_options, long_options, &option_index);
	}

	if (optind < argc) {
		log_error("Unexpected non-option argument '%s'.\n", argv[optind]);
		usage();
		exit(EXIT_FAILURE);
	}
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
			.entry = {
				.font_name = "Sans",
				.font_size = 24,
				.prompt_text = "run: ",
				.num_results = 5,
				.padding_top = 8,
				.padding_bottom = 8,
				.padding_left = 8,
				.padding_right = 8,
				.border_width = 12,
				.outline_width = 4,
				.background_color = {0.106f, 0.114f, 0.118f, 1.0f},
				.foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
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
	};
	wl_list_init(&tofi.output_list);

	parse_early_args(&tofi, argc, argv);

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
		tofi.output_width = el->width;
		tofi.output_height = el->height;
		tofi.window.scale = el->scale;
		log_debug("Selected output %s.\n", el->name);
	}

	/*
	 * We can now parse our arguments and config file, as we know the
	 * output size required for specifying window sizes in percent.
	 */
	parse_args(&tofi, argc, argv);

	/* Scale fonts to the correct size. */
	tofi.window.entry.font_size *= tofi.window.scale;

	/*
	 * If we were invoked as tofi-run, generate the command list.
	 * Otherwise, just read standard input.
	 */
	if (strstr(argv[0], "-run")) {
		log_debug("Generating command list.\n");
		log_indent();
		tofi.window.entry.commands = compgen_cached();
		log_unindent();
		log_debug("Command list generated.\n");
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
		tofi.use_history = false;
	}
	if (tofi.use_history) {
		tofi.window.entry.history = history_load();
		compgen_history_sort(&tofi.window.entry.commands, &tofi.window.entry.history);
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
			ZWLR_LAYER_SHELL_V1_LAYER_TOP,
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
			tofi.window.width / tofi.window.scale,
			tofi.window.height / tofi.window.scale);
	zwlr_layer_surface_v1_set_margin(
			tofi.window.zwlr_layer_surface,
			tofi.window.margin_top,
			tofi.window.margin_right,
			tofi.window.margin_bottom,
			tofi.window.margin_left);
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

	while (wl_display_dispatch(tofi.wl_display) != -1) {
		if (tofi.closed) {
			break;
		}
		if (tofi.window.surface.redraw) {
			surface_draw(&tofi.window.surface);
			tofi.window.surface.redraw = false;
		}
		if (tofi.submit) {
			tofi.submit = false;
			if (tofi.window.entry.results.count > 0) {
				uint32_t selection = tofi.window.entry.selection;
				printf("%s\n", tofi.window.entry.results.buf[selection].string);
				if (tofi.use_history) {
					history_add(
							&tofi.window.entry.history,
							tofi.window.entry.results.buf[selection].string);
					history_save(&tofi.window.entry.history);
				}
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
