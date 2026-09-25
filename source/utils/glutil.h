/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  glutil.h
 * @brief OpenGL API initializer, related functions.
 */

#ifndef SOLOADER_GLUTIL_H
#define SOLOADER_GLUTIL_H

#include <vitaGL.h>

#ifdef __cplusplus
extern "C" {
#endif

void gl_init();

void gl_preload();

void gl_swap();

void glGenBuffers_soloader(GLsizei n, GLuint *buffers);
void glBindBuffer_soloader(GLenum target, GLuint buffer);
void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers);
void glGetIntegerv_soloader(GLenum pname, GLint *data);
void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

void glCompileShader_soloader(GLuint shader);

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_GLUTIL_H
