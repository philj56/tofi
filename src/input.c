#include <linux/input-event-codes.h>
#include "input.h"
#include "nelem.h"
#include "tofi.h"
#include "unicode.h"


static void add_character(struct tofi *tofi, xkb_keycode_t keycode);
static void delete_character(struct tofi *tofi);
static void delete_word(struct tofi *tofi);
static void clear_input(struct tofi *tofi);
static void select_previous_result(struct tofi *tofi);
static void select_next_result(struct tofi *tofi);
static void reset_selection(struct tofi *tofi);
static void refresh_results(struct tofi *tofi);

void input_handle_keypress(struct tofi *tofi, xkb_keycode_t keycode)
{
	if (tofi->xkb_state == NULL) {
		return;
	}

	/*
	 * Use physical key code for shortcuts, ignoring layout changes.
	 * Linux keycodes are 8 less than XKB keycodes.
	 */
	const uint32_t key = keycode - 8;

	xkb_keysym_t sym = xkb_state_key_get_one_sym(tofi->xkb_state, keycode);

	uint32_t ch = xkb_state_key_get_utf32(
			tofi->xkb_state,
			keycode);
	if (utf32_isprint(ch)) {
		add_character(tofi, keycode);
	} else if ((sym == XKB_KEY_BackSpace || key == KEY_W)
			&& xkb_state_mod_name_is_active(
				tofi->xkb_state,
				XKB_MOD_NAME_CTRL,
				XKB_STATE_MODS_EFFECTIVE))
	{
		delete_word(tofi);
	} else if (sym == XKB_KEY_BackSpace) {
		delete_character(tofi);
	} else if (key == KEY_U
			&& xkb_state_mod_name_is_active(
				tofi->xkb_state,
				XKB_MOD_NAME_CTRL,
				XKB_STATE_MODS_EFFECTIVE)
		   )
	{
		clear_input(tofi);
	} else if (sym == XKB_KEY_Up || sym == XKB_KEY_Left || sym == XKB_KEY_ISO_Left_Tab
			|| (key == KEY_K
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
	   ) {
		select_previous_result(tofi);
	} else if (sym == XKB_KEY_Down || sym == XKB_KEY_Right || sym == XKB_KEY_Tab
			|| (key == KEY_J
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
		  ) {
		select_next_result(tofi);
	} else if (sym == XKB_KEY_Home) {
		reset_selection(tofi);
	} else if (sym == XKB_KEY_Escape
			|| (key == KEY_C
				&& xkb_state_mod_name_is_active(
					tofi->xkb_state,
					XKB_MOD_NAME_CTRL,
					XKB_STATE_MODS_EFFECTIVE)
			   )
		  )
	{
		tofi->closed = true;
		return;
	} else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
		tofi->submit = true;
		return;
	}

	tofi->window.surface.redraw = true;
}

void reset_selection(struct tofi *tofi) {
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
	entry->input_utf32[entry->input_utf32_length] = utf8_to_utf32(buf);
	entry->input_utf32_length++;
	entry->input_utf32[entry->input_utf32_length] = U'\0';
	memcpy(&entry->input_utf8[entry->input_utf8_length],
			buf,
			N_ELEM(buf));
	entry->input_utf8_length += len;
	if (entry->drun) {
		struct string_vec results = desktop_vec_filter(&entry->apps, entry->input_utf8, tofi->fuzzy_match);
		string_vec_destroy(&entry->results);
		entry->results = results;
	} else {
		struct string_vec tmp = entry->results;
		entry->results = string_vec_filter(&entry->results, entry->input_utf8, tofi->fuzzy_match);
		string_vec_destroy(&tmp);
	}

	reset_selection(tofi);
}

void refresh_results(struct tofi *tofi)
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
	string_vec_destroy(&entry->results);
	if (entry->drun) {
		entry->results = desktop_vec_filter(&entry->apps, entry->input_utf8, tofi->fuzzy_match);
	} else {
		entry->results = string_vec_filter(&entry->commands, entry->input_utf8, tofi->fuzzy_match);
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

	entry->input_utf32_length--;
	entry->input_utf32[entry->input_utf32_length] = U'\0';

	refresh_results(tofi);
}

void delete_word(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	if (entry->input_utf32_length == 0) {
		/* No input to delete. */
		return;
	}

	while (entry->input_utf32_length > 0 && utf32_isspace(entry->input_utf32[entry->input_utf32_length - 1])) {
		entry->input_utf32_length--;
	}
	while (entry->input_utf32_length > 0 && !utf32_isspace(entry->input_utf32[entry->input_utf32_length - 1])) {
		entry->input_utf32_length--;
	}
	entry->input_utf32[entry->input_utf32_length] = U'\0';

	refresh_results(tofi);
}

void clear_input(struct tofi *tofi)
{
	struct entry *entry = &tofi->window.entry;

	entry->input_utf32_length = 0;
	entry->input_utf32[0] = U'\0';

	refresh_results(tofi);
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
