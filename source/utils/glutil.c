/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/glutil.h"

#include "utils/utils.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>

// Helpers for our handling of shaders
GLboolean skip_next_compile = GL_FALSE;
char next_shader_fname[256];
void load_shader(GLuint shader, const char * string, size_t length);
static int gl_initialized = 0;

void gl_preload() {
    if (!file_exists("ur0:/data/libshacccg.suprx")
        && !file_exists("ur0:/data/external/libshacccg.suprx")) {
        fatal_error("Error: libshacccg.suprx is not installed. "
                    "Google \"ShaRKBR33D\" for quick installation.");
    }

#ifdef USE_GLSL_SHADERS
    vglSetSemanticBindingMode(VGL_MODE_POSTPONED);
#endif
}

void gl_init() {
    /*
     * main() initializes VitaGL before handing control to NativeActivity, and
     * Android then calls eglInitialize() from its render thread.  VitaGL must
     * only be initialized once; a second vglInitExtended() corrupts its global
     * state and GPU allocations.
     */
    if (gl_initialized) {
        l_debug("VitaGL already initialized; skipping duplicate gl_init()");
        return;
    }

    vglInitExtended(0, 960, 544, 6 * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X);
    gl_initialized = 1;
}

void gl_swap() {
    vglSwapBuffers(GL_FALSE);
}

static volatile unsigned present_count;
static volatile unsigned last_present_ms;

unsigned egl_present_count(void) {
    return __atomic_load_n(&present_count, __ATOMIC_RELAXED);
}

unsigned egl_present_age_ms(void) {
    unsigned last = __atomic_load_n(&last_present_ms, __ATOMIC_RELAXED);
    if (!last) return 0;
    return (unsigned)(sceKernelGetProcessTimeWide() / 1000) - last;
}

/* Measure the game's present cadence and VitaGL time without a per-frame log.
 * The lifecycle thread reads the counters even if rendering stops. */
EGLBoolean eglSwapBuffers_soloader(EGLDisplay dpy, EGLSurface surface) {
    static uint64_t window_start_us;
    static uint64_t last_end_us;
    static uint64_t swap_total_us;
    static unsigned window_frames;
    static unsigned max_swap_us;
    static unsigned max_frame_us;

    uint64_t start_us = sceKernelGetProcessTimeWide();
    EGLBoolean result = eglSwapBuffers(dpy, surface);
    uint64_t end_us = sceKernelGetProcessTimeWide();
    unsigned swap_us = (unsigned)(end_us - start_us);
    unsigned frame_us = last_end_us ? (unsigned)(end_us - last_end_us) : 0;
    if (!window_start_us) window_start_us = end_us;
    last_end_us = end_us;
    swap_total_us += swap_us;
    window_frames++;
    if (swap_us > max_swap_us) max_swap_us = swap_us;
    if (frame_us > max_frame_us) max_frame_us = frame_us;
    __atomic_store_n(&last_present_ms, (unsigned)(end_us / 1000), __ATOMIC_RELAXED);
    __atomic_add_fetch(&present_count, 1, __ATOMIC_RELAXED);

    uint64_t elapsed_us = end_us - window_start_us;
    if (elapsed_us >= 5000000) {
        l_info("[PERF] present frames=%u elapsed_ms=%u fps_x10=%u swap_avg_us=%u swap_max_us=%u frame_max_us=%u",
               window_frames, (unsigned)(elapsed_us / 1000),
               (unsigned)(((uint64_t)window_frames * 10000000) / elapsed_us),
               (unsigned)(swap_total_us / window_frames), max_swap_us,
               max_frame_us);
        window_start_us = end_us;
        window_frames = 0;
        swap_total_us = 0;
        max_swap_us = 0;
        max_frame_us = 0;
    }
    return result;
}

/* Android GLES exposes numeric buffer names. This VitaGL build exposes its
 * internal VBO pointer as GLuint, while the game's drawBatch_nogamma_VBO
 * stores the element-buffer name in an unsigned short. Return compact names
 * to the SO and translate them at every buffer-name API boundary. */
#define GUEST_BUFFER_COUNT 65536u
static GLuint guest_buffers[GUEST_BUFFER_COUNT];
static unsigned next_guest_buffer = 1;
static volatile int guest_buffers_lock;

static void lock_guest_buffers(void) {
    while (__sync_lock_test_and_set(&guest_buffers_lock, 1)) {
        while (guest_buffers_lock) { }
    }
}

static void unlock_guest_buffers(void) {
    __sync_lock_release(&guest_buffers_lock);
}

static GLuint reserve_guest_buffer(GLuint vita_name) {
    GLuint guest = 0;
    lock_guest_buffers();
    for (unsigned i = 0; i < GUEST_BUFFER_COUNT - 1; ++i) {
        unsigned candidate = next_guest_buffer;
        next_guest_buffer = candidate + 1;
        if (next_guest_buffer == GUEST_BUFFER_COUNT)
            next_guest_buffer = 1;
        if (!guest_buffers[candidate]) {
            guest_buffers[candidate] = vita_name;
            guest = candidate;
            break;
        }
    }
    unlock_guest_buffers();
    return guest;
}

void glGenBuffers_soloader(GLsizei n, GLuint *buffers) {
    static unsigned calls;
    if (n <= 0 || !buffers) {
        glGenBuffers(n, buffers);
        return;
    }
    for (GLsizei i = 0; i < n; ++i) {
        GLuint vita_name = 0;
        glGenBuffers(1, &vita_name);
        GLuint guest = vita_name ? reserve_guest_buffer(vita_name) : 0;
        if (vita_name && !guest) {
            l_error("[GL] guest buffer names exhausted");
            glDeleteBuffers(1, &vita_name);
        }
        buffers[i] = guest;
        if (__sync_fetch_and_add(&calls, 1) < 48)
            l_info("[GL] glGenBuffers caller=%p guest=%u vita=0x%08X",
                   __builtin_return_address(0), (unsigned)guest,
                   (unsigned)vita_name);
    }
}

void glBindBuffer_soloader(GLenum target, GLuint buffer) {
    static unsigned element_calls;
    static unsigned unknown_names;
    GLuint vita_name = 0;
    if (buffer) {
        if (buffer >= GUEST_BUFFER_COUNT) {
            if (__sync_fetch_and_add(&unknown_names, 1) < 16)
                l_error("[GL] glBindBuffer guest name exceeds 16 bits: 0x%08X caller=%p",
                        (unsigned)buffer, __builtin_return_address(0));
            return;
        }
        lock_guest_buffers();
        vita_name = guest_buffers[buffer];
        unlock_guest_buffers();
        if (!vita_name) {
            /* GLES permits binding a previously unused nonzero name. */
            glGenBuffers(1, &vita_name);
            if (!vita_name)
                return;
            lock_guest_buffers();
            if (guest_buffers[buffer]) {
                GLuint existing = guest_buffers[buffer];
                unlock_guest_buffers();
                glDeleteBuffers(1, &vita_name);
                vita_name = existing;
            } else {
                guest_buffers[buffer] = vita_name;
                unlock_guest_buffers();
            }
            if (__sync_fetch_and_add(&unknown_names, 1) < 16)
                l_info("[GL] created implicit buffer guest=%u vita=0x%08X",
                       (unsigned)buffer, (unsigned)vita_name);
        }
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        unsigned call = __sync_fetch_and_add(&element_calls, 1);
        if (call < 64)
            l_info("[GL] glBindBuffer(ELEMENT) caller=%p guest=%u vita=0x%08X",
                   __builtin_return_address(0), (unsigned)buffer,
                   (unsigned)vita_name);
    }
    glBindBuffer(target, vita_name);
}

void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers) {
    if (n <= 0 || !buffers) {
        glDeleteBuffers(n, buffers);
        return;
    }
    for (GLsizei i = 0; i < n; ++i) {
        GLuint guest = buffers[i];
        GLuint vita_name = 0;
        if (guest && guest < GUEST_BUFFER_COUNT) {
            lock_guest_buffers();
            vita_name = guest_buffers[guest];
            guest_buffers[guest] = 0;
            unlock_guest_buffers();
        }
        if (vita_name)
            glDeleteBuffers(1, &vita_name);
    }
}

void glGetIntegerv_soloader(GLenum pname, GLint *data) {
    glGetIntegerv(pname, data);
    if (!data || (pname != GL_ARRAY_BUFFER_BINDING &&
                  pname != GL_ELEMENT_ARRAY_BUFFER_BINDING) || !*data)
        return;
    GLuint vita_name = (GLuint)*data;
    GLuint guest = 0;
    lock_guest_buffers();
    for (unsigned i = 1; i < GUEST_BUFFER_COUNT; ++i) {
        if (guest_buffers[i] == vita_name) {
            guest = i;
            break;
        }
    }
    unlock_guest_buffers();
    *data = (GLint)guest;
}

void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type,
                            const GLvoid *indices) {
    static unsigned draws;
    static unsigned rejected;
    GLint element_binding = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_binding);
    unsigned draw = __sync_fetch_and_add(&draws, 1);
    if (draw < 16)
        l_info("[GL] glDrawElements caller=%p mode=0x%X count=%d type=0x%X indices=%p element=0x%08X",
               __builtin_return_address(0), (unsigned)mode, (int)count,
               (unsigned)type, indices, (unsigned)element_binding);
    if (element_binding != 0 && (uint32_t)element_binding < 0x80000000u) {
        unsigned reject = __sync_fetch_and_add(&rejected, 1);
        if (reject < 16)
            l_error("[GL] skip draw: invalid VitaGL element-buffer pointer 0x%08X",
                    (unsigned)element_binding);
        return;
    }
    glDrawElements(mode, count, type, indices);
}

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glShaderSource<%p>(shader: %i, count: %i, string: %p, length: %p)\n", __builtin_return_address(0), shader, count, string, _length);
#endif
    if (!string) {
        l_error("<%p> Shader source string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    } else if (!*string) {
        l_error("<%p> Shader source *string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    }

    size_t total_length = 0;

    for (int i = 0; i < count; ++i) {
        if (!_length) {
            total_length += strlen(string[i]);
        } else {
            total_length += _length[i];
        }
    }

    char * str = malloc(total_length+1);
    size_t l = 0;

    for (int i = 0; i < count; ++i) {
        if (!_length) {
            memcpy(str + l, string[i], strlen(string[i]));
            l += strlen(string[i]);
        } else {
            memcpy(str + l, string[i], _length[i]);
            l += _length[i];
        }
    }
    str[total_length] = '\0';

    load_shader(shader, str, total_length);

    free(str);
}

void glCompileShader_soloader(GLuint shader) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glCompileShader<%p>(shader: %i)\n", __builtin_return_address(0), shader);
#endif

#ifndef USE_GXP_SHADERS
    if (!skip_next_compile) {
        glCompileShader(shader);
#ifdef DUMP_COMPILED_SHADERS
        void *bin = vglMalloc(32 * 1024);
        GLsizei len;
        vglGetShaderBinary(shader, 32 * 1024, &len, bin);
        file_save(next_shader_fname, bin, len);
        vglFree(bin);
#endif
    }
    skip_next_compile = GL_FALSE;
#endif
}

#if defined(USE_GLSL_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else {
        glShaderSource(shader, 1, &string, &length);
        strcpy(next_shader_fname, gxp_path);
    }

    free(sha_name);
}
#elif defined(USE_GLSL_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    glShaderSource(shader, 1, &string, &length);
}
#elif defined(USE_CG_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    char cg_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);
    snprintf(cg_path, sizeof(cg_path), DATA_PATH"cg/%s.cg", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else if (file_exists(cg_path)) {
        char *buffer;
        size_t size;

        file_load(cg_path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);
        strcpy(next_shader_fname, gxp_path);

        free(buffer);
        skip_next_compile = GL_FALSE;
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }

        skip_next_compile = GL_FALSE;
    }

    free(sha_name);
}
#elif defined(USE_CG_SHADERS) || defined(USE_GXP_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char path[256];
#ifdef USE_CG_SHADERS
    snprintf(path, sizeof(path), DATA_PATH"cg/%s.cg", sha_name);
#else
    snprintf(path, sizeof(path), DATA_PATH"gxp/%s.gxp", sha_name);
#endif

    if (file_exists(path)) {
#ifdef USE_CG_SHADERS
        char *buffer;
        size_t size;

        file_load(path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);

        free(buffer);
#else
        uint8_t *buffer;
        size_t size;

        file_load(path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
#endif
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }
    }

    free(sha_name);
}
#else
#error "Define one of (USE_GLSL_SHADERS, USE_CG_SHADERS, USE_GXP_SHADERS)"
#endif
