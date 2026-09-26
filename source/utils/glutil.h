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
EGLBoolean eglSwapBuffers_soloader(EGLDisplay dpy, EGLSurface surface);
unsigned egl_present_count(void);
unsigned egl_present_age_ms(void);

void glGenBuffers_soloader(GLsizei n, GLuint *buffers);
void glBindBuffer_soloader(GLenum target, GLuint buffer);
void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers);
void glGetIntegerv_soloader(GLenum pname, GLint *data);
void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

void glDrawArrays_soloader(GLenum mode, GLint first, GLsizei count);
void glFinish_soloader(void);
void glFlush_soloader(void);
void glBufferData_soloader(GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
void glBufferSubData_soloader(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
void glTexImage2D_soloader(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data);
void glTexSubImage2D_soloader(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data);

void glCompileShader_soloader(GLuint shader);

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_GLUTIL_H
