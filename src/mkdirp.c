#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "log.h"
#include "mkdirp.h"
#include "xmalloc.h"

bool mkdirp(const char *path)
{
	struct stat statbuf;
	if (stat(path, &statbuf) == 0) {
		/* If the file exists, we don't need to do anything. */
		return true;
	}

	/*
	 * Walk down the path, creating directories as we go.
	 * This works by repeatedly finding the next / in path, then calling
	 * mkdir() on the string up to that point.
	 */
	char *tmp = xstrdup(path);
	char *cursor = tmp;
	while ((cursor = strchr(cursor + 1, '/')) != NULL) {
		*cursor = '\0';
		log_debug("Creating directory %s\n", tmp);
		if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
			log_error(
					"Error creating file path: %s.\n",
					strerror(errno));
			free(tmp);
			return false;
		}
		*cursor = '/';
	}
	free(tmp);
	return true;
}
