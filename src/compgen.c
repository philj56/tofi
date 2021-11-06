#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "log.h"
#include "string_vec.h"

struct string_vec compgen()
{
	log_debug("Retrieving PATH.\n");
	const char *env_path = getenv("PATH");
	if (env_path == NULL) {
		log_error("Couldn't retrieve PATH from environment.");
		exit(EXIT_FAILURE);
	}
	struct string_vec programs = string_vec_create();
	char *path = strdup(env_path);
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
	log_debug("Done.\n");
	return programs;
}
