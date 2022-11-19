#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "fuzzy_match.h"
#include "history.h"
#include "string_vec.h"
#include "unicode.h"
#include "xmalloc.h"

static int cmpstringp(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;

	/*
	 * Ensure any NULL strings are shoved to the end.
	 */
	if (str1->string == NULL) {
		return 1;
	}
	if (str2->string == NULL) {
		return -1;
	}
	return strcmp(str1->string, str2->string);
}

static int cmpscorep(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;

	int hist_diff = str2->history_score - str1->history_score;
	int search_diff = str2->search_score - str1->search_score;
	return hist_diff + search_diff;
}

static int cmphistoryp(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;

	return str2->history_score - str1->history_score;
}

struct string_vec string_vec_create(void)
{
	struct string_vec vec = {
		.count = 0,
		.size = 128,
		.buf = xcalloc(128, sizeof(*vec.buf)),
	};
	return vec;
}

void string_vec_destroy(struct string_vec *restrict vec)
{
	for (size_t i = 0; i < vec->count; i++) {
		free(vec->buf[i].string);
	}
	free(vec->buf);
}

struct string_vec string_vec_copy(const struct string_vec *restrict vec)
{
	struct string_vec copy = {
		.count = vec->count,
		.size = vec->size,
		.buf = xcalloc(vec->size, sizeof(*copy.buf)),
	};

	for (size_t i = 0; i < vec->count; i++) {
		copy.buf[i].string = xstrdup(vec->buf[i].string);
		copy.buf[i].search_score = vec->buf[i].search_score;
		copy.buf[i].history_score = vec->buf[i].history_score;
	}

	return copy;
}

void string_vec_add(struct string_vec *restrict vec, const char *restrict str)
{
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = xrealloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count].string = utf8_normalize(str);
	if (vec->buf[vec->count].string == NULL) {
		vec->buf[vec->count].string = xstrdup(str);
	}
	vec->buf[vec->count].search_score = 0;
	vec->buf[vec->count].history_score = 0;
	vec->count++;
}

void string_vec_sort(struct string_vec *restrict vec)
{
	qsort(vec->buf, vec->count, sizeof(vec->buf[0]), cmpstringp);
}

void string_vec_history_sort(struct string_vec *restrict vec, struct history *history)
{
	/*
	 * To find elements without assuming the vector is pre-sorted, we use a
	 * hash table, which results in O(N+M) work (rather than O(N*M) for
	 * linear search.
	 */
	GHashTable *hash = g_hash_table_new(g_str_hash, g_str_equal);
	for (size_t i = 0; i < vec->count; i++) {
		g_hash_table_insert(hash, vec->buf[i].string, &vec->buf[i]);
	}
	for (size_t i = 0; i < history->count; i++) {
		struct scored_string *res = g_hash_table_lookup(hash, history->buf[i].name);
		if (res == NULL) {
			continue;
		}
		res->history_score = history->buf[i].run_count;
	}
	g_hash_table_unref(hash);

	qsort(vec->buf, vec->count, sizeof(vec->buf[0]), cmphistoryp);
}

void string_vec_uniq(struct string_vec *restrict vec)
{
	size_t count = vec->count;
	for (size_t i = 1; i < vec->count; i++) {
		if (!strcmp(vec->buf[i].string, vec->buf[i-1].string)) {
			free(vec->buf[i-1].string);
			vec->buf[i-1].string = NULL;
			count--;
		}
	}
	string_vec_sort(vec);
	vec->count = count;
}

struct scored_string *string_vec_find_sorted(struct string_vec *restrict vec, const char * str)
{
	return bsearch(&str, vec->buf, vec->count, sizeof(vec->buf[0]), cmpstringp);
}

struct string_vec string_vec_filter(
		const struct string_vec *restrict vec,
		const char *restrict substr,
		bool fuzzy)
{
	if (substr[0] == '\0') {
		return string_vec_copy(vec);
	}
	struct string_vec filt = string_vec_create();
	for (size_t i = 0; i < vec->count; i++) {
		int32_t search_score;
		if (fuzzy) {
			search_score = fuzzy_match_words(substr, vec->buf[i].string);
		} else {
			search_score = fuzzy_match_simple_words(substr, vec->buf[i].string);
		}
		if (search_score != INT32_MIN) {
			string_vec_add(&filt, vec->buf[i].string);
			/*
			 * Store the position of the match in the string as
			 * its search_score, for later sorting.
			 */
			filt.buf[filt.count - 1].search_score = search_score;
			filt.buf[filt.count - 1].history_score = vec->buf[i].history_score;
		}
	}
	/*
	 * Sort the results by this search_score. This moves matches at the beginnings
	 * of words to the front of the result list.
	 */
	qsort(filt.buf, filt.count, sizeof(filt.buf[0]), cmpscorep);
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
		fputs(vec->buf[i].string, file);
		fputc('\n', file);
	}
}
