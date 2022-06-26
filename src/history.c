#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "history.h"
#include "log.h"
#include "mkdirp.h"
#include "xmalloc.h"

static const char *default_state_dir = ".local/state";
static const char *histfile_basename = "tofi-history";

[[nodiscard("memory leaked")]]
static struct history history_create(void);

static char *get_histfile_path() {
	char *histfile_name = NULL;
	const char *state_path = getenv("XDG_STATE_HOME");
	if (state_path == NULL) {
		const char *home = getenv("HOME");
		if (home == NULL) {
			log_error("Couldn't retrieve HOME from environment.\n");
			return NULL;
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

struct history history_load()
{
	struct history vec = history_create();
	char *histfile_name = get_histfile_path();
	if (histfile_name == NULL) {
		return vec;
	}

	FILE *histfile = fopen(histfile_name, "rb");
	free(histfile_name);

	if (histfile == NULL) {
		return vec;
	}

	errno = 0;
	if (fseek(histfile, 0, SEEK_END) != 0) {
		log_error("Error seeking in history file: %s.\n", strerror(errno));
		fclose(histfile);
		return vec;
	}

	errno = 0;
	size_t len = ftell(histfile);
	if (fseek(histfile, 0, SEEK_SET) != 0) {
		log_error("Error seeking in history file: %s.\n", strerror(errno));
		fclose(histfile);
		return vec;
	}

	errno = 0;
	char *buf = xmalloc(len + 1);
	if (fread(buf, 1, len, histfile) != len) {
		log_error("Error reading history file: %s.\n", strerror(errno));
		fclose(histfile);
		return vec;
	}
	fclose(histfile);
	buf[len] = '\0';

	char *saveptr = NULL;
	char *tok = strtok_r(buf, " ", &saveptr);
	while (tok != NULL) {
		size_t run_count = strtoull(tok, NULL, 10);
		tok = strtok_r(NULL, "\n", &saveptr);
		if (tok == NULL) {
			break;
		}
		history_add(&vec, tok);
		vec.buf[vec.count - 1].run_count = run_count;
		tok = strtok_r(NULL, " ", &saveptr);
	}

	free(buf);
	return vec;
}

void history_save(struct history *history)
{
	char *histfile_name = get_histfile_path();
	if (histfile_name == NULL) {
		return;
	}

	/* Create the path if necessary. */
	if (!mkdirp(histfile_name)) {
		return;
	}

	/* Use open rather than fopen to ensure the proper permissions. */
	int histfd = open(histfile_name, O_WRONLY | O_CREAT, 0600);
	FILE *histfile = fdopen(histfd, "wb");
	if (histfile == NULL) {
		return;
	}

	for (size_t i = 0; i < history->count; i++) {
		fprintf(histfile, "%zu %s\n", history->buf[i].run_count, history->buf[i].name);
	}

	fclose(histfile);
	free(histfile_name);
}

struct history history_create(void)
{
	struct history vec = {
		.count = 0,
		.size = 16,
		.buf = xcalloc(16, sizeof(struct program))
	};
	return vec;
}

void history_destroy(struct history *restrict vec)
{
	for (size_t i = 0; i < vec->count; i++) {
		free(vec->buf[i].name);
	}
	free(vec->buf);
}

void history_add(struct history *restrict vec, const char *restrict str)
{
	/*
	 * If the program's already in our vector, just increment the count and
	 * move the program up if needed.
	 */
	for (size_t i = 0; i < vec->count; i++) {
		if (!strcmp(vec->buf[i].name, str)) {
			vec->buf[i].run_count++;
			size_t count = vec->buf[i].run_count;
			if (i > 0 && count <= vec->buf[i-1].run_count) {
				return;
			}
			/* We need to move the program up the list */
			size_t j = i;
			while (j > 0 && count > vec->buf[j-1].run_count) {
				j--;
			}
			struct program tmp = vec->buf[i];
			memmove(&vec->buf[j+1], &vec->buf[j], (i - j) * sizeof(struct program));
			vec->buf[j] = tmp;
			return;
		}
	}

	/* Otherwise add it to the end with a run count of 1 */
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = xrealloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count].name = xstrdup(str);
	vec->buf[vec->count].run_count = 1;
	vec->count++;
}

void history_remove(struct history *restrict vec, const char *restrict str)
{
	for (size_t i = 0; i < vec->count; i++) {
		if (!strcmp(vec->buf[i].name, str)) {
			free(vec->buf[i].name);
			if (i < vec->count - 1) {
				memmove(&vec->buf[i], &vec->buf[i+1], (vec->count - i) * sizeof(struct program));
			}
			vec->count--;
			return;
		}
	}
}
