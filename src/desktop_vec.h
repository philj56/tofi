#ifndef DESKTOP_VEC_H
#define DESKTOP_VEC_H

#include <stddef.h>
#include <stdio.h>

struct desktop_entry {
	char *id;
	char *name;
	char *path;
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
		const char *restrict path);
void desktop_vec_add_file(struct desktop_vec *desktop, const char *id, const char *path);

void desktop_vec_sort(struct desktop_vec *restrict vec);
struct desktop_entry *desktop_vec_find(struct desktop_vec *restrict vec, const char *name);

struct desktop_vec desktop_vec_load(FILE *file);
void desktop_vec_save(struct desktop_vec *restrict vec, FILE *restrict file);


#endif /* DESKTOP_VEC_H */
