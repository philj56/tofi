#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "string_vec.h"
#include "xmalloc.h"

static int cmpstringp(const void *restrict a, const void *restrict b)
{
	/*
	 * For qsort we receive pointers to the array elements (which are
	 * pointers to char), so convert and dereference them for comparison.
	 */
	const char *restrict str1 = *(const char **)a;
	const char *restrict str2 = *(const char **)b;

	/*
	 * Ensure any NULL strings are shoved to the end.
	 */
	if (str1 == NULL) {
		return 1;
	}
	if (str2 == NULL) {
		return -1;
	}
	return strcmp(str1, str2);
}

struct string_vec string_vec_create(void)
{
	struct string_vec vec = {
		.count = 0,
		.size = 128,
		.buf = xcalloc(128, sizeof(char *))
	};
	return vec;
}

void string_vec_destroy(struct string_vec *restrict vec)
{
	for (size_t i = 0; i < vec->count; i++) {
		free(vec->buf[i]);
	}
	free(vec->buf);
}

struct string_vec string_vec_copy(struct string_vec *restrict vec)
{
	struct string_vec copy = {
		.count = vec->count,
		.size = vec->size,
		.buf = xcalloc(vec->size, sizeof(char *))
	};

	for (size_t i = 0; i < vec->count; i++) {
		copy.buf[i] = xstrdup(vec->buf[i]);
	}

	return copy;
}

void string_vec_add(struct string_vec *restrict vec, const char *restrict str)
{
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = xrealloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count] = xstrdup(str);
	vec->count++;
}

void string_vec_sort(struct string_vec *restrict vec)
{
	qsort(vec->buf, vec->count, sizeof(vec->buf[0]), cmpstringp);
}

void string_vec_uniq(struct string_vec *restrict vec)
{
	size_t count = vec->count;
	for (size_t i = 1; i < vec->count; i++) {
		if (!strcmp(vec->buf[i], vec->buf[i-1])) {
			free(vec->buf[i-1]);
			vec->buf[i-1] = NULL;
			count--;
		}
	}
	string_vec_sort(vec);
	vec->count = count;
}

char **string_vec_find(struct string_vec *restrict vec, const char * str)
{
	return bsearch(&str, vec->buf, vec->count, sizeof(vec->buf[0]), cmpstringp);
}

struct string_vec string_vec_filter(
		const struct string_vec *restrict vec,
		const char *restrict substr)
{
	struct string_vec filt = string_vec_create();
	for (size_t i = 0; i < vec->count; i++) {
		if (strcasestr(vec->buf[i], substr) != NULL) {
			string_vec_add(&filt, vec->buf[i]);
		}
	}
	return filt;
}

struct string_vec string_vec_load(FILE *file)
{
	struct string_vec vec = string_vec_create();
	if (file == NULL) {
		return vec;
	}

	ssize_t bytes_read;
	char *line = NULL;
	size_t len;
	while ((bytes_read = getline(&line, &len, file)) != -1) {
		if (line[bytes_read - 1] == '\n') {
			line[bytes_read - 1] = '\0';
		}
		string_vec_add(&vec, line);
	}
	free(line);

	return vec;
}

void string_vec_save(struct string_vec *restrict vec, FILE *restrict file)
{
	for (size_t i = 0; i < vec->count; i++) {
		fputs(vec->buf[i], file);
		fputc('\n', file);
	}
}
