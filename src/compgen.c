#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "compgen.h"
#include "history.h"
#include "log.h"
#include "mkdirp.h"
#include "string_vec.h"
#include "xmalloc.h"

static const char *default_cache_dir = ".cache";
static const char *cache_basename = "tofi-compgen";

[[nodiscard("memory leaked")]]
static char *get_cache_path() {
	char *cache_name = NULL;
	const char *state_path = getenv("XDG_CACHE_HOME");
	if (state_path == NULL) {
		const char *home = getenv("HOME");
		if (home == NULL) {
			log_error("Couldn't retrieve HOME from environment.\n");
			return NULL;
		}
		size_t len = strlen(home) + 1
			+ strlen(default_cache_dir) + 1
			+ strlen(cache_basename) + 1;
		cache_name = xmalloc(len);
		snprintf(
			cache_name,
			len,
			"%s/%s/%s",
			home,
			default_cache_dir,
			cache_basename);
	} else {
		size_t len = strlen(state_path) + 1
			+ strlen(cache_basename) + 1;
		cache_name = xmalloc(len);
		snprintf(
			cache_name,
			len,
			"%s/%s",
			state_path,
			cache_basename);
	}
	return cache_name;
}

struct string_vec compgen_cached()
{
	log_debug("Retrieving PATH.\n");
	const char *env_path = getenv("PATH");
	if (env_path == NULL) {
		log_error("Couldn't retrieve PATH from environment.\n");
		exit(EXIT_FAILURE);
	}

	log_debug("Retrieving cache location.\n");
	char *cache_path = get_cache_path();

	struct stat sb;
	if (cache_path == NULL) {
		return compgen();
	}

	/* If the cache doesn't exist, create it and return */
	errno = 0;
	if (stat(cache_path, &sb) == -1) {
		if (errno == ENOENT) {
			struct string_vec commands = compgen();
			if (!mkdirp(cache_path)) {
				free(cache_path);
				return commands;
			}
			FILE *cache = fopen(cache_path, "wb");
			string_vec_save(&commands, cache);
			fclose(cache);
			free(cache_path);
			return commands;
		}
		free(cache_path);
		return compgen();
	}

	/* The cache exists, so check if it's still in date */
	char *path = xstrdup(env_path);
	char *saveptr = NULL;
	char *path_entry = strtok_r(path, ":", &saveptr);
	bool out_of_date = false;
	while (path_entry != NULL) {
		struct stat path_sb;
		if (stat(path_entry, &path_sb) == 0) {
			if (path_sb.st_mtim.tv_sec > sb.st_mtim.tv_sec) {
				out_of_date = true;
				break;
			}
		}
		path_entry = strtok_r(NULL, ":", &saveptr);
	}
	free(path);

	struct string_vec commands;
	if (out_of_date) {
		log_debug("Cache out of date, updating.\n");
		log_indent();
		commands = compgen();
		log_unindent();
		FILE *cache = fopen(cache_path, "wb");
		string_vec_save(&commands, cache);
		fclose(cache);
	} else {
		log_debug("Cache up to date, loading.\n");
		FILE *cache = fopen(cache_path, "rb");
		commands = string_vec_load(cache);
		fclose(cache);
	}
	free(cache_path);
	return commands;
}

struct string_vec compgen()
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

	return programs;
}

static int cmpscorep(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;
	return str2->history_score - str1->history_score;
}

void compgen_history_sort(struct string_vec *programs, struct history *history)
{
	log_debug("Moving already known programs to the front.\n");
	for (size_t i = 0; i < history->count; i++) {
		struct scored_string *res = string_vec_find(programs, history->buf[i].name);
		if (res == NULL) {
			log_debug("History entry \"%s\" not found.\n", history->buf[i].name);
			continue;
		}
		res->history_score = history->buf[i].run_count;
	}

	qsort(programs->buf, programs->count, sizeof(programs->buf[0]), cmpscorep);
}
