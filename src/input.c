#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <unistd.h>
#include "input.h"
#include "log.h"
#include "nelem.h"
#include "tofi.h"
#include "unicode.h"


static uint32_t keysym_to_key(xkb_keysym_t sym);
static void add_character(struct tofi *tofi, xkb_keycode_t keycode);
static void delete_character(struct tofi *tofi);
static void delete_word(struct tofi *tofi);
static void clear_input(struct tofi *tofi);
static void paste(struct tofi *tofi);
static void select_previous_result(struct tofi *tofi);
static void select_next_result(struct tofi *tofi);
static void select_previous_page(struct tofi *tofi);
static void select_next_page(struct tofi *tofi);
static void next_cursor_or_result(struct tofi *tofi);
static void previous_cursor_or_result(struct tofi *tofi);
static void reset_selection(struct tofi *tofi);

void input_handle_mouse(struct tofi *tofi, mouse_event_t event)
{
	if (event == MOUSE_EVENT_WHEEL_UP) select_previous_result(tofi);
	else if (event == MOUSE_EVENT_WHEEL_DOWN) select_next_result(tofi);
	else if (event == MOUSE_EVENT_RIGHT_CLICK) {
		tofi->closed = true;
		return;
	} else if (event == MOUSE_EVENT_LEFT_CLICK) {
		tofi->submit = true;
		return;
	}
	tofi->window.surface.redraw = true;
}

void input_handle_keypress(struct tofi *tofi, xkb_keycode_t keycode)
{
	if (tofi->xkb_state == NULL) {
		return;
	}

	bool ctrl = xkb_state_mod_name_is_active(
			tofi->xkb_state,
			XKB_MOD_NAME_CTRL,
			XKB_STATE_MODS_EFFECTIVE);
	bool alt = xkb_state_mod_name_is_active(
			tofi->xkb_state,
			XKB_MOD_NAME_ALT,
			XKB_STATE_MODS_EFFECTIVE);
	bool shift = xkb_state_mod_name_is_active(
			tofi->xkb_state,
			XKB_MOD_NAME_SHIFT,
			XKB_STATE_MODS_EFFECTIVE);

	uint32_t ch = xkb_state_key_get_utf32(tofi->xkb_state, keycode);

	/*
	 * Use physical key code for shortcuts by default, ignoring layout
	 * changes. Linux keycodes are 8 less than XKB keycodes.
	 */
	uint32_t key = keycode - 8;
	if (!tofi->physical_keybindings) {
		xkb_keysym_t sym = xkb_state_key_get_one_sym(tofi->xkb_state, keycode);
		key = keysym_to_key(sym);
	}

	/*
	 * Alt does not affect which character is selected, so we have to check
	 * for it explicitly.
	 */
	if (utf32_isprint(ch) && !ctrl && !alt) {
		add_character(tofi, keycode);
	} else if ((key == KEY_BACKSPACE || key == KEY_W) && ctrl) {
		delete_word(tofi);
	} else if (key == KEY_BACKSPACE
			|| (key == KEY_H && ctrl)) {
		delete_character(tofi);
	} else if (key == KEY_U && ctrl) {
		clear_input(tofi);
	} else if (key == KEY_V && ctrl) {
		paste(tofi);
	} else if (key == KEY_LEFT) {
		previous_cursor_or_result(tofi);
	} else if (key == KEY_RIGHT) {
		next_cursor_or_result(tofi);
	} else if (key == KEY_UP
			|| key == KEY_LEFT
			|| (key == KEY_TAB && shift)
			|| (key == KEY_H && alt)
			|| ((key == KEY_K || key == KEY_P || key == KEY_B) && (ctrl || alt))) {
		select_previous_result(tofi);
	} else if (key == KEY_DOWN
			|| key == KEY_RIGHT
			|| key == KEY_TAB
			|| (key == KEY_L && alt)
			|| ((key == KEY_J || key == KEY_N || key == KEY_F) && (ctrl || alt))) {
		select_next_result(tofi);
	} else if (key == KEY_HOME) {
		reset_selection(tofi);
	} else if (key == KEY_PAGEUP) {
		select_previous_page(tofi);
	} else if (key == KEY_PAGEDOWN) {
		select_next_page(tofi);
	} else if (key == KEY_ESC
			|| ((key == KEY_C || key == KEY_LEFTBRACE || key == KEY_G) && ctrl)) {
		tofi->closed = true;
		return;
	} else if (key == KEY_ENTER
			|| key == KEY_KPENTER
			|| (key == KEY_M && ctrl)) {
		tofi->submit = true;
		return;
	}

	if (tofi->auto_accept_single && tofi->window.entry.results.count == 1) {
		tofi->submit = true;
	}

	tofi->window.surface.redraw = true;
}

static uint32_t keysym_to_key(xkb_keysym_t sym)
{
	switch (sym) {
		case XKB_KEY_BackSpace:
			return KEY_BACKSPACE;
		case XKB_KEY_w:
			return KEY_W;
		case XKB_KEY_u:
			return KEY_U;
		case XKB_KEY_v:
			return KEY_V;
		case XKB_KEY_Left:
			return KEY_LEFT;
		case XKB_KEY_Right:
			return KEY_RIGHT;
		case XKB_KEY_Up:
			return KEY_UP;
		case XKB_KEY_ISO_Left_Tab:
			return KEY_TAB;
		case XKB_KEY_h:
			return KEY_H;
		case XKB_KEY_k:
			return KEY_K;
		case XKB_KEY_p:
			return KEY_P;
		case XKB_KEY_Down:
			return KEY_DOWN;
		case XKB_KEY_Tab:
			return KEY_TAB;
		case XKB_KEY_l:
			return KEY_L;
		case XKB_KEY_j:
			return KEY_J;
		case XKB_KEY_n:
			return KEY_N;
		case XKB_KEY_Home:
			return KEY_HOME;
		case XKB_KEY_Page_Up:
			return KEY_PAGEUP;
		case XKB_KEY_Page_Down:
			return KEY_PAGEDOWN;
		case XKB_KEY_Escape:
			return KEY_ESC;
		case XKB_KEY_c:
			return KEY_C;
		case XKB_KEY_bracketleft:
			return KEY_LEFTBRACE;
		case XKB_KEY_Return:
			return KEY_ENTER;
		case XKB_KEY_KP_Enter:
			return KEY_KPENTER;
		case XKB_KEY_m:
			return KEY_M;
	}
	return (uint32_t)-1;
}

void reset_selection(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;
	entry->selection = 0;
	entry->first_result = 0;
}

void add_character(struct tofi *tofi, xkb_keycode_t keycode)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->input_utf32_length >= N_ELEM(entry->input_utf32) - 1) {
		/* No more room for input */
		return;
	}

	char buf[5]; /* 4 UTF-8 bytes plus null terminator. */
	int len = xkb_state_key_get_utf8(
			tofi->xkb_state,
			keycode,
			buf,
			sizeof(buf));
	if (entry->cursor_position == entry->input_utf32_length) {
		entry->input_utf32[entry->input_utf32_length] = utf8_to_utf32(buf);
		entry->input_utf32_length++;
		entry->input_utf32[entry->input_utf32_length] = U'\0';
		memcpy(&entry->input_utf8[entry->input_utf8_length],
				buf,
				N_ELEM(buf));
		entry->input_utf8_length += len;

		if (entry->mode == TOFI_MODE_DRUN) {
			struct string_ref_vec results = desktop_vec_filter(&entry->apps, entry->input_utf8, tofi->matching_algorithm);
			string_ref_vec_destroy(&entry->results);
			entry->results = results;
		} else {
			struct string_ref_vec tmp = entry->results;
			entry->results = string_ref_vec_filter(&entry->results, entry->input_utf8, tofi->matching_algorithm);
			string_ref_vec_destroy(&tmp);
		}

		reset_selection(tofi);
	} else {
		for (size_t i = entry->input_utf32_length; i > entry->cursor_position; i--) {
			entry->input_utf32[i] = entry->input_utf32[i - 1];
		}
		entry->input_utf32[entry->cursor_position] = utf8_to_utf32(buf);
		entry->input_utf32_length++;
		entry->input_utf32[entry->input_utf32_length] = U'\0';

		input_refresh_results(tofi);
	}

	entry->cursor_position++;
}

void input_refresh_results(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	size_t bytes_written = 0;
	for (size_t i = 0; i < entry->input_utf32_length; i++) {
		bytes_written += utf32_to_utf8(
				entry->input_utf32[i],
				&entry->input_utf8[bytes_written]);
	}
	entry->input_utf8[bytes_written] = '\0';
	entry->input_utf8_length = bytes_written;
	string_ref_vec_destroy(&entry->results);
	if (entry->mode == TOFI_MODE_DRUN) {
		entry->results = desktop_vec_filter(&entry->apps, entry->input_utf8, tofi->matching_algorithm);
	} else {
		entry->results = string_ref_vec_filter(&entry->commands, entry->input_utf8, tofi->matching_algorithm);
	}

	reset_selection(tofi);
}

void delete_character(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->input_utf32_length == 0) {
		/* No input to delete. */
		return;
	}

	if (entry->cursor_position == 0) {
		return;
	} else if (entry->cursor_position == entry->input_utf32_length) {
		entry->cursor_position--;
		entry->input_utf32_length--;
		entry->input_utf32[entry->input_utf32_length] = U'\0';
	} else {
		for (size_t i = entry->cursor_position - 1; i < entry->input_utf32_length - 1; i++) {
			entry->input_utf32[i] = entry->input_utf32[i + 1];
		}
		entry->cursor_position--;
		entry->input_utf32_length--;
		entry->input_utf32[entry->input_utf32_length] = U'\0';
	}

	input_refresh_results(tofi);
}

void delete_word(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->cursor_position == 0) {
		/* No input to delete. */
		return;
	}

	uint32_t new_cursor_pos = entry->cursor_position;
	while (new_cursor_pos > 0 && utf32_isspace(entry->input_utf32[new_cursor_pos - 1])) {
		new_cursor_pos--;
	}
	while (new_cursor_pos > 0 && !utf32_isspace(entry->input_utf32[new_cursor_pos - 1])) {
		new_cursor_pos--;
	}
	uint32_t new_length = entry->input_utf32_length - (entry->cursor_position - new_cursor_pos);
	for (size_t i = 0; i < new_length; i++) {
		entry->input_utf32[new_cursor_pos + i] = entry->input_utf32[entry->cursor_position + i];
	}
	entry->input_utf32_length = new_length;
	entry->input_utf32[entry->input_utf32_length] = U'\0';

	entry->cursor_position = new_cursor_pos;
	input_refresh_results(tofi);
}

void clear_input(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	entry->cursor_position = 0;
	entry->input_utf32_length = 0;
	entry->input_utf32[0] = U'\0';

	input_refresh_results(tofi);
}

void paste(struct tofi *tofi)
{
	if (tofi->clipboard.wl_data_offer == NULL || tofi->clipboard.mime_type == NULL) {
		return;
	}

	/*
	 * Create a pipe, and give the write end to the compositor to give to
	 * the clipboard manager.
	 */
	errno = 0;
	int fildes[2];
	if (pipe2(fildes, O_CLOEXEC | O_NONBLOCK) == -1) {
		log_error("Failed to open pipe for clipboard: %s\n", strerror(errno));
		return;
	}
	wl_data_offer_receive(tofi->clipboard.wl_data_offer, tofi->clipboard.mime_type, fildes[1]);
	close(fildes[1]);

	/* Keep the read end for reading in the main loop. */
	tofi->clipboard.fd = fildes[0];
}

void select_previous_result(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->selection > 0) {
		entry->selection--;
		return;
	}

	uint32_t nsel = MAX(MIN(entry->num_results_drawn, entry->results.count), 1);

	if (entry->first_result > nsel) {
		entry->first_result -= entry->last_num_results_drawn;
		entry->selection = entry->last_num_results_drawn - 1;
	} else if (entry->first_result > 0) {
		entry->selection = entry->first_result - 1;
		entry->first_result = 0;
	}
}

void select_next_result(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	uint32_t nsel = MAX(MIN(entry->num_results_drawn, entry->results.count), 1);

	entry->selection++;
	if (entry->selection >= nsel) {
		entry->selection -= nsel;
		if (entry->results.count > 0) {
			entry->first_result += nsel;
			entry->first_result %= entry->results.count;
		} else {
			entry->first_result = 0;
		}
		entry->last_num_results_drawn = entry->num_results_drawn;
	}
}

void previous_cursor_or_result(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->cursor_theme.show
			&& entry->selection == 0
			&& entry->cursor_position > 0) {
		entry->cursor_position--;
	} else {
		select_previous_result(tofi);
	}
}

void next_cursor_or_result(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->cursor_theme.show
			&& entry->cursor_position < entry->input_utf32_length) {
		entry->cursor_position++;
	} else {
		select_next_result(tofi);
	}
}

void select_previous_page(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->first_result >= entry->last_num_results_drawn) {
		entry->first_result -= entry->last_num_results_drawn;
	} else {
		entry->first_result = 0;
	}
	entry->selection = 0;
	entry->last_num_results_drawn = entry->num_results_drawn;
}

void select_next_page(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	entry->first_result += entry->num_results_drawn;
	if (entry->first_result >= entry->results.count) {
		entry->first_result = 0;
	}
	entry->selection = 0;
	entry->last_num_results_drawn = entry->num_results_drawn;
}
