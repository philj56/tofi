#ifndef DESKTOP_VEC_H
#define DESKTOP_VEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "matching.h"

struct desktop_entry {
	char *id;
	char *name;
	char *path;
	char *keywords;
	uint32_t search_score;
	uint32_t history_score;
};

struct desktop_vec {
	size_t count;
	size_t size;
	struct desktop_entry *buf;
};

[[nodiscard("memory leaked")]]
struct desktop_vec desktop_vec_create(void);
void desktop_vec_destroy(struct desktop_vec *restrict vec);
void desktop_vec_add(
		struct desktop_vec *restrict vec,
		const char *restrict id,
		const char *restrict name,
		const char *restrict path,
		const char *restrict keywords);
void desktop_vec_add_file(struct desktop_vec *desktop, const char *id, const char *path);

void desktop_vec_sort(struct desktop_vec *restrict vec);
struct desktop_entry *desktop_vec_find_sorted(struct desktop_vec *restrict vec, const char *name);
struct string_ref_vec desktop_vec_filter(
		const struct desktop_vec *restrict vec,
		const char *restrict substr,
		enum matching_algorithm algorithm);

struct desktop_vec desktop_vec_load(FILE *file);
void desktop_vec_save(struct desktop_vec *restrict vec, FILE *restrict file);


#endif /* DESKTOP_VEC_H */
