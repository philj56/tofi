#include <unistd.h>
#include "clipboard.h"

void clipboard_finish_paste(struct clipboard *clipboard)
{
	if (clipboard->fd > 0) {
		close(clipboard->fd);
		clipboard->fd = 0;
	}
}

void clipboard_reset(struct clipboard *clipboard)
{
	if (clipboard->wl_data_offer != NULL) {
		wl_data_offer_destroy(clipboard->wl_data_offer);
		clipboard->wl_data_offer = NULL;
	}
	if (clipboard->fd > 0) {
		close(clipboard->fd);
		clipboard->fd = 0;
	}
	clipboard->mime_type = NULL;
}
