#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "shm.h"

/* These two functions aren't used on linux. */
#ifndef __linux__
static void randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}
#endif

int shm_allocate_file(size_t size)
{
#ifdef __linux__
	/*
	 * On linux, we can just use memfd_create(). This is both simpler and
	 * potentially allows usage of Transparent HugePages, which speed up
	 * the first paint of a large screen buffer.
	 *
	 * This isn't available on *BSD, which we could conceivably be running
	 * on.
	 */
	int fd = memfd_create("wl_shm", 0);
#else
	int fd = create_shm_file();
#endif
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}
