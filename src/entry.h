#ifndef ENTRY_H
#define ENTRY_H

#include <pango/pangocairo.h>
#include "util.h"
#include "surface.h"

#define MAX_PASSWORD_LENGTH 256

struct entry {
	struct surface surface;
	struct wl_subsurface *wl_subsurface;
	struct image image;
	struct {
		PangoLayout *layout;
		cairo_surface_t *surface;
		cairo_t *cr;
	} pangocairo;
	struct {
		struct color color;
		struct color outline_color;
		int32_t width;
		int32_t outline_width;
	} border;
	wchar_t password[MAX_PASSWORD_LENGTH];
	char password_mb[4*MAX_PASSWORD_LENGTH];
	uint32_t password_length;
};

void entry_init(struct entry *entry);
void entry_update(struct entry *entry);

#endif /* ENTRY_H */
