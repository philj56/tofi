#ifndef RECT_VEC_H
#define RECT_VEC_H

#include <stdint.h>
#include <stddef.h>

struct rectangle {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct rect_vec {
	size_t count;
	size_t size;
	struct rectangle *buf;
};

[[nodiscard("memory leaked")]]
struct rect_vec rect_vec_create(void);

void rect_vec_destroy(struct rect_vec *restrict vec);

void rect_vec_clear(struct rect_vec *restrict vec);

void rect_vec_add(struct rect_vec *restrict vec, struct rectangle rect);

struct rect_vec rect_vec_copy(struct rect_vec *restrict vec);

#endif /* RECT_VEC_H */
