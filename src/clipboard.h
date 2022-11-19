#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <wayland-client.h>

struct clipboard {
	struct wl_data_offer *wl_data_offer;
	const char *mime_type;
	int fd;
};

void clipboard_finish_paste(struct clipboard *clipboard);
void clipboard_reset(struct clipboard *clipboard);

#endif /* CLIPBOARD_H */
