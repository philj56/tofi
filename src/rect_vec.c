#include "rect_vec.h"
#include "xmalloc.h"

struct rect_vec rect_vec_create(void)
{
	struct rect_vec vec = {
		.count = 0,
		.size = 16,
		.buf = xcalloc(16, sizeof(*vec.buf)),
	};
	return vec;
}

void rect_vec_destroy(struct rect_vec *restrict vec)
{
	free(vec->buf);
}

void rect_vec_clear(struct rect_vec *restrict vec)
{
	vec->count = 0;
}

void rect_vec_add(struct rect_vec *restrict vec, struct rectangle rect)
{
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = xrealloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count] = rect;
	vec->count++;
}

struct rect_vec rect_vec_copy(struct rect_vec *restrict vec)
{
	struct rect_vec copy = {
		.count = vec->count,
		.size = vec->size,
		.buf = xcalloc(vec->size, sizeof(*copy.buf)),
	};

	for (size_t i = 0; i < vec->count; i++) {
		copy.buf[i] = vec->buf[i];
	}

	return copy;
}
