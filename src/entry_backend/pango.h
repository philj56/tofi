#ifndef ENTRY_BACKEND_PANGO_H
#define ENTRY_BACKEND_PANGO_H

#include <pango/pangocairo.h>

struct entry;

struct entry_backend {
	PangoContext *context;
	PangoLayout *prompt_layout;
	PangoLayout *entry_layout;
	PangoLayout *result_layouts[5];
};

void entry_backend_init(struct entry *entry, uint32_t width, uint32_t height, uint32_t scale);
void entry_backend_destroy(struct entry *entry);
void entry_backend_update(struct entry *entry);

#endif /* ENTRY_BACKEND_PANGO_H */
