#include <epoxy/gl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "gl.h"
#include "log.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

static void load_shader(GLuint shader, const char *filename);
static GLuint create_shader_program(const char *vert, const char *frag);

void gl_initialise(struct gl *gl, struct image *texture)
{
	if (texture == NULL) {
		return;
	}

	/* Compile and link the shader programs */
	gl->shader = create_shader_program(
			SHADER_PATH "vert.vert",
			SHADER_PATH "frag.frag"
			);
	glUseProgram(gl->shader);
	glUniform1i(glGetUniformLocation(gl->shader, "tex"), 0);

	/* Create a vertex buffer for a quad filling the screen */
	float vertices[] = {
		//  Position      Texcoords
		-1.0f,  1.0f, 0.0f, 0.0f, // Top-left
		1.0f,  1.0f,  1.0f, 0.0f, // Top-right
		1.0f, -1.0f,  1.0f, 1.0f, // Bottom-right
		-1.0f, -1.0f, 0.0f, 1.0f  // Bottom-left
	};

	glGenBuffers(1, &gl->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	/* Create a vertex array and enable vertex attributes for the shaders. */
	glGenVertexArrays(1, &gl->vao);
	glBindVertexArray(gl->vao);

	GLint posAttrib = glGetAttribLocation(gl->shader, "position");
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
	glEnableVertexAttribArray(posAttrib);

	GLint texAttrib = glGetAttribLocation(gl->shader, "texcoord");
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)(2*sizeof(float)));
	glEnableVertexAttribArray(texAttrib);

	glBindVertexArray(0);

	/* Create the element buffer object that will actually be drawn via
	 * glDrawElements(). */
	GLuint elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenBuffers(1, &gl->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	/* Create the texture we'll draw to */
	glGenTextures(1, &gl->texture);
	glBindTexture(GL_TEXTURE_2D, gl->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, (GLvoid *)texture->buffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (texture->swizzle) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	}

	/* Bind the actual bits we'll be using to render */
	glBindVertexArray(gl->vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->ebo);
}

void gl_clear(struct gl *gl, struct color *color)
{
	glClearColor(color->r, color->g, color->b, color->a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void gl_draw_texture(struct gl *gl, struct image *texture, int32_t x, int32_t y, int32_t width, int32_t height)
{
	if (texture->redraw) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, GL_RGBA,
				GL_UNSIGNED_BYTE, (GLvoid *)texture->buffer);
		texture->redraw = false;
	}

	glViewport(x, y, width, height);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void load_shader(GLuint shader, const char *filename)
{
	errno = 0;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	long size = ftell(fp);
	if (size <= 0) {
		log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	unsigned long usize = (unsigned long) size;
	GLchar *source = malloc(usize + 1);
	rewind(fp);
	if (fread(source, 1, usize, fp) != usize) {
		log_error("Failed to load shader %s: %s.\n", filename, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	source[usize] = '\0';
	glShaderSource(shader, 1, (const GLchar *const *)&source, NULL);
	free(source);

	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		log_error("Failed to compile shader %s!\n", filename);

		GLint info_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);
		if (info_length > 1) {
			char *log = malloc((unsigned)info_length * sizeof(*log));
			glGetShaderInfoLog(shader, info_length, NULL, log);
			log_error("\t%s\n", log);
			free(log);
		}
		exit(EXIT_FAILURE);
	}
}

GLuint create_shader_program(const char *vert, const char *frag)
{
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	load_shader(vertex_shader, vert);

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	load_shader(fragment_shader, frag);

	GLuint shader = glCreateProgram();
	glAttachShader(shader, vertex_shader);
	glAttachShader(shader, fragment_shader);
	glLinkProgram(shader);
	return shader;
}
