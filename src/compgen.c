#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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

static void write_cache(const char *buffer, const char *filename)
{
	errno = 0;
	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		log_error("Failed to open cache file \"%s\": %s\n", filename, strerror(errno));
		return;
	}
	size_t len = strlen(buffer);
	errno = 0;
	if (fwrite(buffer, 1, len, fp) != len) {
		log_error("Error writing cache file \"%s\": %s\n", filename, strerror(errno));
	}
	fclose(fp);
}

static char *read_cache(const char *filename)
{
	errno = 0;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		log_error("Failed to open cache file \"%s\": %s\n", filename, strerror(errno));
		return NULL;
	}
	if (fseek(fp, 0, SEEK_END)) {
		log_error("Failed to seek in cache file: %s\n", strerror(errno));
		fclose(fp);
		return NULL;
	}
	size_t size;
	{
		long ssize = ftell(fp);
		if (ssize < 0) {
			log_error("Failed to determine cache file size: %s\n", strerror(errno));
			fclose(fp);
			return NULL;
		}
		size = (size_t)ssize;
	}
	char *cache = xmalloc(size + 1);
	rewind(fp);
	if (fread(cache, 1, size, fp) != size) {
		log_error("Failed to read cache file: %s\n", strerror(errno));
		free(cache);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	cache[size] = '\0';

	return cache;
}

char *compgen_cached()
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
			char *commands = compgen();
			if (mkdirp(cache_path)) {
				write_cache(commands, cache_path);
			}
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

	char *commands;
	if (out_of_date) {
		log_debug("Cache out of date, updating.\n");
		log_indent();
		commands = compgen();
		log_unindent();
		write_cache(commands, cache_path);
	} else {
		log_debug("Cache up to date, loading.\n");
		commands = read_cache(cache_path);
	}
	free(cache_path);
	return commands;
}

char *compgen()
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
				if (faccessat(fd, d->d_name, X_OK, 0) == -1) {
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

	size_t buf_len = 0;
	for (size_t i = 0; i < programs.count; i++) {
		buf_len += strlen(programs.buf[i].string) + 1;
	}
	char *buf = xmalloc(buf_len + 1);
	size_t bytes_written = 0;
	for (size_t i = 0; i < programs.count; i++) {
		bytes_written += sprintf(&buf[bytes_written], "%s\n", programs.buf[i].string);
	}
	buf[bytes_written] = '\0';

	string_vec_destroy(&programs);

	return buf;
}

static int cmpscorep(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;
	return str2->history_score - str1->history_score;
}

struct string_ref_vec compgen_history_sort(struct string_ref_vec *programs, struct history *history)
{
	log_debug("Moving already known programs to the front.\n");
	for (size_t i = 0; i < history->count; i++) {
		struct scored_string_ref *res = string_ref_vec_find_sorted(programs, history->buf[i].name);
		if (res == NULL) {
			log_debug("History entry \"%s\" not found.\n", history->buf[i].name);
			continue;
		}
		res->history_score = history->buf[i].run_count;
	}

	/*
	 * For compgen, we expect there to be many more commands than history
	 * entries. For speed, we therefore create a copy of the command
	 * vector with all of the non-zero history score items pushed to the
	 * front. We can then call qsort() on just the first few items of the
	 * new vector, rather than on the entire original vector.
	 */
	struct string_ref_vec vec = {
		.count = programs->count,
		.size = programs->size,
		.buf = xcalloc(programs->size, sizeof(*vec.buf))
	};

	size_t n_hist = 0;
	for (ssize_t i = programs->count - 1; i >= 0; i--) {
		if (programs->buf[i].history_score == 0) {
			vec.buf[i + n_hist] = programs->buf[i];
		} else {
			vec.buf[n_hist] = programs->buf[i];
			n_hist++;
		}
	}
	qsort(vec.buf, n_hist, sizeof(vec.buf[0]), cmpscorep);
	return vec;
}
