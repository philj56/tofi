#ifndef GL_H
#define GL_H

#include <epoxy/gl.h>
#include "color.h"
#include "image.h"

struct gl {
	GLuint vbo;
	GLuint vao;
	GLuint ebo;
	GLuint texture;
	GLuint shader;
};

void gl_initialise(struct gl *gl, struct image *texture);
void gl_destroy(struct gl *gl);
void gl_clear(struct gl *gl, struct color *color);
void gl_draw_texture(
		struct gl *gl,
		struct image *texture,
		int32_t x,
		int32_t y,
		int32_t width,
		int32_t height);

#endif /* GL_H */
