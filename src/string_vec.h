#ifndef STRING_VEC_H
#define STRING_VEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "history.h"
#include "matching.h"

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

void string_vec_add(struct string_vec *restrict vec, const char *restrict str);

void string_vec_sort(struct string_vec *restrict vec);

struct scored_string *string_vec_find_sorted(struct string_vec *restrict vec, const char *str);


/*
 * Like a string_vec, but only store a reference to the corresponding string
 * rather than copying it. Although compatible with the string_vec struct, we
 * create a new struct to make the compiler complain if we mix them up.
 */
struct scored_string_ref {
	char *string;
	int32_t search_score;
	int32_t history_score;
};

struct string_ref_vec {
	size_t count;
	size_t size;
	struct scored_string_ref *buf;
};

/*
 * Although some of these functions are identical to the corresponding
 * string_vec ones, we create new functions to avoid potentially mixing up
 * the two.
 */
[[nodiscard("memory leaked")]]
struct string_ref_vec string_ref_vec_create(void);

void string_ref_vec_destroy(struct string_ref_vec *restrict vec);

[[nodiscard("memory leaked")]]
struct string_ref_vec string_ref_vec_copy(const struct string_ref_vec *restrict vec);

void string_ref_vec_add(struct string_ref_vec *restrict vec, char *restrict str);

void string_ref_vec_history_sort(struct string_ref_vec *restrict vec, struct history *history);

void string_vec_uniq(struct string_vec *restrict vec);

struct scored_string_ref *string_ref_vec_find_sorted(struct string_ref_vec *restrict vec, const char *str);

[[nodiscard("memory leaked")]]
struct string_ref_vec string_ref_vec_filter(
		const struct string_ref_vec *restrict vec,
		const char *restrict substr,
		enum matching_algorithm algorithm);

[[nodiscard("memory leaked")]]
struct string_ref_vec string_ref_vec_from_buffer(char *buffer);

#endif /* STRING_VEC_H */
