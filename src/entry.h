#ifndef ENTRY_H
#define ENTRY_H

#include <pango/pangocairo.h>
#include "color.h"
#include "image.h"
#include "surface.h"

#define MAX_PASSWORD_LENGTH 64

struct entry {
	struct surface surface;
	struct wl_subsurface *wl_subsurface;
	struct image image;
	struct {
		PangoContext *context;
		PangoLayout *layout;
	} pango;
	struct {
		cairo_surface_t *surface;
		cairo_t *cr;
	} cairo;
	PangoRectangle text_bounds;

	wchar_t password[MAX_PASSWORD_LENGTH];
	/* Assume maximum of 4 bytes per wchar_t (for UTF-8) */
	char password_mb[4*MAX_PASSWORD_LENGTH];
	char password_mb_print[4*MAX_PASSWORD_LENGTH];
	uint32_t password_length;
	bool password_visible;

	/* Options */
	bool use_pango;
	uint32_t dot_radius;
	uint32_t font_size;
	const char *font_name;
	uint32_t padding;
	bool tight_layout;
	wchar_t password_character;
	uint32_t num_characters;
	struct color foreground_color;
	struct color background_color;
	struct {
		struct color color;
		struct color outline_color;
		int32_t width;
		int32_t outline_width;
	} border;
};

void entry_init(struct entry *entry, uint32_t scale);
void entry_destroy(struct entry *entry);
void entry_update(struct entry *entry);
void entry_set_scale(struct entry *entry, uint32_t scale);

#endif /* ENTRY_H */
