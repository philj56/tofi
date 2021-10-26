#ifndef GL_H
#define GL_H

#include <epoxy/gl.h>

struct client_state;
struct gl {
	GLuint vbo;
	GLuint vao;
	GLuint ebo;
	GLuint texture;
	GLuint shader;
};

void gl_initialise(struct client_state *state);
void gl_draw(struct client_state *state);

#endif /* GL_H */
