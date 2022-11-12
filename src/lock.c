#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "log.h"
#include "xmalloc.h"

static const char *default_cache_dir = ".cache/";
static const char *lock_filename = "tofi.lock";

[[nodiscard("memory leaked")]]
static char *get_lock_path() {
	char *lock_name = NULL;
	const char *runtime_path = getenv("XDG_RUNTIME_DIR");
	if (runtime_path == NULL) {
		runtime_path = getenv("XDG_CACHE_HOME");
	}
	if (runtime_path == NULL) {
		const char *home = getenv("HOME");
		if (home == NULL) {
			log_error("Couldn't retrieve HOME from environment.\n");
			return NULL;
		}
		size_t len = strlen(home) + 1
			+ strlen(default_cache_dir) + 1
			+ strlen(lock_filename) + 1;
		lock_name = xmalloc(len);
		snprintf(
			lock_name,
			len,
			"%s/%s/%s",
			home,
			default_cache_dir,
			lock_filename);
	} else {
		size_t len = strlen(runtime_path) + 1
			+ strlen(lock_filename) + 1;
		lock_name = xmalloc(len);
		snprintf(
			lock_name,
			len,
			"%s/%s",
			runtime_path,
			lock_filename);
	}
	return lock_name;
}

bool lock_check(void)
{
	bool ret = false;
	char *filename = get_lock_path();
	errno = 0;
	int fd = open(filename, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		log_error("Failed to open lock file %s: %s.\n", filename, strerror(errno));
	} else if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK) {
			/*
			 * We can't lock the file because another tofi process
			 * already has.
			 */
			ret = true;
		}
        }

	free(filename);
	return ret;
}
