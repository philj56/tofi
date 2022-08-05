#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "log.h"
#include "shm.h"
#include "surface.h"

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void surface_init(
		struct surface *surface,
		struct wl_shm *wl_shm)
{
	const int height = surface->height;
	const int width = surface->width;

	/* Assume 4 bytes per pixel for WL_SHM_FORMAT_ARGB8888 */
	const int stride = width * 4;
	surface->stride = stride;

	/* Double-buffered pool, so allocate space for two windows */
	surface->shm_pool_size =
		height
		* stride
		* 2;
	surface->shm_pool_fd = shm_allocate_file(surface->shm_pool_size);
	surface->shm_pool_data = mmap(
			NULL,
			surface->shm_pool_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			surface->shm_pool_fd,
			0);
	surface->wl_shm_pool = wl_shm_create_pool(
			wl_shm,
			surface->shm_pool_fd,
			surface->shm_pool_size);

	for (int i = 0; i < 2; i++) {
		int offset = height * stride * i;
		surface->buffers[i] = wl_shm_pool_create_buffer(
				surface->wl_shm_pool,
				offset,
				width,
				height,
				stride,
				WL_SHM_FORMAT_ARGB8888);
	}

	log_debug("Created shm file with size %d KiB.\n",
			surface->shm_pool_size / 1024);
}

void surface_destroy(struct surface *surface)
{
	wl_shm_pool_destroy(surface->wl_shm_pool);
	munmap(surface->shm_pool_data, surface->shm_pool_size);
	surface->shm_pool_data = NULL;
	close(surface->shm_pool_fd);
	wl_buffer_destroy(surface->buffers[0]);
	wl_buffer_destroy(surface->buffers[1]);
}

void surface_draw(struct surface *surface, struct rect_vec *damage_lists[2])
{
	wl_surface_attach(surface->wl_surface, surface->buffers[surface->index], 0, 0);
	if (damage_lists == NULL) {
		wl_surface_damage_buffer(surface->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
	} else {
		for (size_t i = 0; i < damage_lists[0]->count; i++) {
			struct rectangle rect = damage_lists[0]->buf[i];
			wl_surface_damage_buffer(surface->wl_surface,
					rect.x,
					rect.y,
					rect.width,
					rect.height);
		}
		for (size_t i = 0; i < damage_lists[1]->count; i++) {
			struct rectangle rect = damage_lists[1]->buf[i];
			wl_surface_damage_buffer(surface->wl_surface,
					rect.x,
					rect.y,
					rect.width,
					rect.height);
		}
	}
	wl_surface_commit(surface->wl_surface);

	surface->index = !surface->index;
}
