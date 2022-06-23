#ifndef ENTRY_BACKEND_PANGO_H
#define ENTRY_BACKEND_PANGO_H

#include <pango/pangocairo.h>

struct entry;

struct entry_backend_pango {
	PangoContext *context;
	PangoLayout *layout;
};

void entry_backend_pango_init(struct entry *entry, uint32_t *width, uint32_t *height);
void entry_backend_pango_destroy(struct entry *entry);
void entry_backend_pango_update(struct entry *entry);

#endif /* ENTRY_BACKEND_PANGO_H */
