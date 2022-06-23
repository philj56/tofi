#ifndef ENTRY_BACKEND_PANGO_H
#define ENTRY_BACKEND_PANGO_H

#include <pango/pangocairo.h>

struct entry;

struct entry_backend {
	PangoContext *context;
	PangoLayout *layout;
};

void entry_backend_init(struct entry *entry, uint32_t *width, uint32_t *height);
void entry_backend_destroy(struct entry *entry);
void entry_backend_update(struct entry *entry);

#endif /* ENTRY_BACKEND_PANGO_H */
