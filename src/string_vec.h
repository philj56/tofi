#ifndef STRING_VEC_H
#define STRING_VEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "history.h"

struct scored_string {
	char *string;
	int32_t search_score;
	int32_t history_score;
};

struct string_vec {
	size_t count;
	size_t size;
	struct scored_string *buf;
};

[[nodiscard("memory leaked")]]
struct string_vec string_vec_create(void);

void string_vec_destroy(struct string_vec *restrict vec);

[[nodiscard("memory leaked")]]
struct string_vec string_vec_copy(const struct string_vec *restrict vec);

void string_vec_add(struct string_vec *restrict vec, const char *restrict str);

void string_vec_sort(struct string_vec *restrict vec);

void string_vec_history_sort(struct string_vec *restrict vec, struct history *history);

void string_vec_uniq(struct string_vec *restrict vec);

struct scored_string *string_vec_find(struct string_vec *restrict vec, const char *str);

[[nodiscard("memory leaked")]]
struct string_vec string_vec_filter(
		const struct string_vec *restrict vec,
		const char *restrict substr,
		bool fuzzy);

[[nodiscard("memory leaked")]]
struct string_vec string_vec_load(FILE *file);
void string_vec_save(struct string_vec *restrict vec, FILE *restrict file);

#endif /* STRING_VEC_H */
