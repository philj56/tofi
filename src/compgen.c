#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "history.h"
#include "log.h"
#include "string_vec.h"
#include "xmalloc.h"

struct string_vec compgen(struct history *history)
{
	log_debug("Retrieving PATH.\n");
	const char *env_path = getenv("PATH");
	if (env_path == NULL) {
		log_error("Couldn't retrieve PATH from environment.\n");
		exit(EXIT_FAILURE);
	}

	struct string_vec programs = string_vec_create();
	char *path = xstrdup(env_path);
	char *saveptr = NULL;
	char *path_entry = strtok_r(path, ":", &saveptr);

	log_debug("Scanning PATH for binaries.\n");
	while (path_entry != NULL) {
		DIR *dir = opendir(path_entry);
		if (dir != NULL) {
			int fd = dirfd(dir);
			struct dirent *d;
			while ((d = readdir(dir)) != NULL) {
				struct stat sb;
				if (fstatat(fd, d->d_name, &sb, 0) == -1) {
					continue;
				}
				if (!(sb.st_mode & S_IXUSR)) {
					continue;
				}
				if (!S_ISREG(sb.st_mode)) {
					continue;
				}
				string_vec_add(&programs, d->d_name);
			}
			closedir(dir);
		}
		path_entry = strtok_r(NULL, ":", &saveptr);
	}
	free(path);

	log_debug("Sorting results.\n");
	string_vec_sort(&programs);

	log_debug("Making unique.\n");
	string_vec_uniq(&programs);

	log_debug("Moving already known programs to the front.\n");
	/*
	 * Remove any programs in our history from the generated list, and
	 * store which ones we found in to_add.
	 * Removal is done without changing the count, as we're about to re-add
	 * them at the front.
	 */
	struct string_vec to_add = string_vec_create();
	for (size_t i = 0; i < history->count; i++) {
		char **res = string_vec_find(&programs, history->buf[i].name);
		if (res == NULL) {
			continue;
		}
		free(*res);
		*res = NULL;
		string_vec_add(&to_add, history->buf[i].name);
	}

	/* Sort the vector to push the removed entries to the end. */
	string_vec_sort(&programs);

	/*
	 * Move the results down by the number of items we want to add. There's
	 * guaranteed to be enough space to do this, as we just removed that
	 * many items.
	 */
	memmove(
		&programs.buf[to_add.count],
		programs.buf,
		(programs.count - to_add.count) * sizeof(programs.buf[0]));

	/* Add our history to the front in order. */
	for (size_t i = 0; i < to_add.count; i++) {
		programs.buf[i] = xstrdup(to_add.buf[i]);
	}
	string_vec_destroy(&to_add);
	log_debug("Done.\n");
	return programs;
}
