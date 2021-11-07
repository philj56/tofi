#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include "string_vec.h"

static int cmpstringp(const void *restrict a, const void *restrict b)
{
	/*
	 * We receive pointers to the array elements (which are pointers to
	 * char), so convert and dereference them for comparison.
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
	return strcasecmp(str1, str2);
}

struct string_vec string_vec_create(void)
{
	struct string_vec vec = {
		.count = 0,
		.size = 128,
		.buf = calloc(128, sizeof(char *))
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
		.buf = calloc(vec->size, sizeof(char *))
	};

	for (size_t i = 0; i < vec->count; i++) {
		copy.buf[i] = strdup(vec->buf[i]);
	}

	return copy;
}

void string_vec_add(struct string_vec *restrict vec, const char *restrict str)
{
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = realloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count] = strdup(str);
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
