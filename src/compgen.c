#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "log.h"
#include "string_vec.h"
#include "xmalloc.h"

static const char *default_state_dir = ".local/state";
static const char *histfile_basename = "tofi-history";

static char *get_histfile_path() {
	char *histfile_name;
	const char *state_path = getenv("XDG_STATE_HOME");
	if (state_path == NULL) {
		const char *home = getenv("HOME");
		if (home == NULL) {
			log_error("Couldn't retrieve HOME from environment.\n");
			exit(EXIT_FAILURE);
		}
		size_t len = strlen(home) + 1
			+ strlen(default_state_dir) + 1
			+ strlen(histfile_basename) + 1;
		histfile_name = xmalloc(len);
		snprintf(
			histfile_name,
			len,
			"%s/%s/%s",
			home,
			default_state_dir,
			histfile_basename);
	} else {
		size_t len = strlen(state_path) + 1
			+ strlen(histfile_basename) + 1;
		histfile_name = xmalloc(len);
		snprintf(
			histfile_name,
			len,
			"%s/%s",
			state_path,
			histfile_basename);
	}
	return histfile_name;

}

static void load_history()
{
	char *name = get_histfile_path();
	FILE *histfile = fopen(name, "rb");

	free(name);

	if (histfile == NULL) {
		return;
	}
	
}

struct string_vec compgen()
{
	load_history();
	log_debug("Retrieving PATH.\n");
	const char *env_path = getenv("PATH");
	if (env_path == NULL) {
		log_error("Couldn't retrieve PATH from environment.\n");
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
