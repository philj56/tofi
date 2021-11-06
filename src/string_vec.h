#ifndef STRING_VEC_H
#define STRING_VEC_H

#include <stddef.h>

struct string_vec {
	size_t count;
	size_t size;
	char **buf;
};

[[nodiscard]]
struct string_vec string_vec_create();

void string_vec_destroy(struct string_vec *restrict vec);

void string_vec_add(struct string_vec *restrict vec, const char *restrict str);

void string_vec_sort(struct string_vec *restrict vec);

void string_vec_uniq(struct string_vec *restrict vec);

[[nodiscard]] [[gnu::nonnull]]
struct string_vec string_vec_filter(
		struct string_vec *restrict vec,
		const char *restrict substr);

#endif /* STRING_VEC_H */
