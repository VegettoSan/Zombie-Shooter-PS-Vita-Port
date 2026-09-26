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
#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
#include "utils/perf.h"
#include <vitagl/source/utils/zombie_texture_update.h>

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

    vglInitExtended(0, 960, 544, 6 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    gl_initialized = 1;
    l_perf("render width=960 height=544 msaa=none");
}

void gl_swap() {
    vglSwapBuffers(GL_FALSE);
}

/* GLES is driven by the game render thread. Single-writer counters avoid
 * atomic RMW and clock reads on draws/binds; publication is on that thread. */
typedef struct { unsigned calls, total_us, max_us; } GLTiming;
static struct {
    unsigned draws, arrays, binds, rejected;
    GLTiming buffer_data, buffer_sub, tex_image, tex_sub, finish, flush, compile, link, draw_sample;
} gl_perf;
static void gl_time(GLTiming *t, uint64_t start) {
    unsigned us = (unsigned)(sceKernelGetProcessTimeWide()-start);
    t->calls++; t->total_us += us;
    if (us > t->max_us) t->max_us = us;
}
static unsigned draw_sample_sequence;
static uint64_t draw_sample_begin(void) {
    return (draw_sample_sequence++ & 15u)==0?sceKernelGetProcessTimeWide():0;
}
static void draw_sample_end(uint64_t start) { if(start) gl_time(&gl_perf.draw_sample,start); }
/* The SDK reports runClocks in native kernel units. Log the raw delta rather
 * than assuming a unit or summing another thread's blocked time into render. */
static void report_render_thread(void) {
    static SceUID previous_tid;
    static uint64_t previous_clocks;
    static unsigned previous_preempts;
    SceUID tid=sceKernelGetThreadId();
    SceKernelThreadInfo info={.size=sizeof(info)};
    int result=sceKernelGetThreadInfo(tid,&info);
    int valid=result==0 && previous_tid==tid;
    l_perf("render_thread id=0x%08X result=%d delta_valid=%d run_clocks_delta=%llu preempt_delta=%u affinity=0x%X priority=%d",
        (unsigned)tid,result,valid,(unsigned long long)(valid?info.runClocks-previous_clocks:0),
        valid?info.threadPreemptCount-previous_preempts:0,(unsigned)info.currentCpuAffinityMask,info.currentPriority);
    if(result==0) { previous_tid=tid;previous_clocks=info.runClocks;previous_preempts=info.threadPreemptCount; }
    else previous_tid=0;
}
static struct { uintptr_t caller; unsigned calls, us, max_us; int w,h; GLenum format,type; } upload_groups[8];
static void report_texture_costs(void) {
    VglZombieTextureStats cow; vglZombieTextureStatsGet(&cow);
    l_perf("tex_cow calls=%u optimized=%u rgb565=%u full_replacements=%u alloc_failures=%u alloc_us=%u preserve_us=%u old_bytes=%llu preserved_bytes=%llu max_w=%u max_h=%u",
        cow.cow_calls,cow.optimized_calls,cow.rgb565_calls,cow.full_replacements,cow.alloc_failures,cow.alloc_us,cow.preserve_us,
        (unsigned long long)cow.old_bytes,(unsigned long long)cow.preserved_bytes,cow.max_texture_w,cow.max_texture_h);
    for (unsigned i=0;i<8;++i) if (upload_groups[i].calls) {
        l_perf("tex_upload caller=0x%08X calls=%u total_us=%u max_us=%u max_shape=%dx%d format=0x%X type=0x%X",
            (unsigned)upload_groups[i].caller,upload_groups[i].calls,upload_groups[i].us,upload_groups[i].max_us,
            upload_groups[i].w,upload_groups[i].h,upload_groups[i].format,upload_groups[i].type);
    }
    memset(upload_groups,0,sizeof(upload_groups));
    SceKernelFreeMemorySizeInfo mem = {.size=sizeof(mem)};
    int result=sceKernelGetFreeMemorySize(&mem);
    l_perf("mem system_result=%d user_free_kib=%u cdram_free_kib=%u phy_free_kib=%u vgl_ram_free_kib=%u vgl_ram_total_kib=%u vgl_vram_free_kib=%u vgl_vram_total_kib=%u vgl_phy_free_kib=%u vgl_phy_total_kib=%u",
        result,mem.size_user/1024,mem.size_cdram/1024,mem.size_phycont/1024,
        (unsigned)(vglMemFree(VGL_MEM_RAM)/1024),(unsigned)(vglMemTotal(VGL_MEM_RAM)/1024),
        (unsigned)(vglMemFree(VGL_MEM_VRAM)/1024),(unsigned)(vglMemTotal(VGL_MEM_VRAM)/1024),
        (unsigned)(vglMemFree(VGL_MEM_SLOW)/1024),(unsigned)(vglMemTotal(VGL_MEM_SLOW)/1024));
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
        l_perf("frame frames=%u elapsed_ms=%u fps_x10=%u swap_avg_us=%u swap_max_us=%u frame_max_us=%u",
               window_frames, (unsigned)(elapsed_us / 1000),
               (unsigned)(((uint64_t)window_frames * 10000000) / elapsed_us),
               (unsigned)(swap_total_us / window_frames), max_swap_us,
               max_frame_us);
        l_perf("gl draws=%u draw_arrays=%u binds=%u rejected=%u", gl_perf.draws, gl_perf.arrays, gl_perf.binds, gl_perf.rejected);
#define REPORT_GL(name, member) l_perf("gl " name "_calls=%u " name "_total_us=%u " name "_max_us=%u", gl_perf.member.calls, gl_perf.member.total_us, gl_perf.member.max_us)
        REPORT_GL("buffer_data", buffer_data); REPORT_GL("buffer_sub", buffer_sub);
        REPORT_GL("tex_image", tex_image); REPORT_GL("tex_sub", tex_sub);
        REPORT_GL("finish", finish); REPORT_GL("flush", flush);
        REPORT_GL("compile", compile); REPORT_GL("link", link);
        REPORT_GL("draw_sample", draw_sample);
        l_perf("draw_sampling period=16");
        report_render_thread();
        report_texture_costs();
#undef REPORT_GL
        memset(&gl_perf, 0, sizeof(gl_perf));
        perf_report();
        logger_force_sync();
        /* The next interval includes report/sync overhead in its frame time. */
        window_start_us = end_us;
        window_frames = 0;
        swap_total_us = 0;
        max_swap_us = 0;
        max_frame_us = 0;
    }
    return result;
}

#include "utils/gl_buffers.inc"


void glDrawArrays_soloader(GLenum mode, GLint first, GLsizei count) {
    gl_perf.arrays++;
    uint64_t start=draw_sample_begin();glDrawArrays(mode,first,count);draw_sample_end(start);
}
void glFinish_soloader(void) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glFinish(); gl_time(&gl_perf.finish, start);
}
void glLinkProgram_soloader(GLuint program) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glLinkProgram(program); gl_time(&gl_perf.link, start);
}
void glFlush_soloader(void) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glFlush(); gl_time(&gl_perf.flush, start);
}
void glBufferData_soloader(GLenum target, GLsizei size, const GLvoid *data, GLenum usage) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glBufferData(target, size, data, usage); gl_time(&gl_perf.buffer_data, start);
}
void glBufferSubData_soloader(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glBufferSubData(target, offset, size, data); gl_time(&gl_perf.buffer_sub, start);
}
void glTexImage2D_soloader(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glTexImage2D(target, level, internalFormat, width, height, border, format, type, data); gl_time(&gl_perf.tex_image, start);
}
void glTexSubImage2D_soloader(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data) {
    uint64_t start = sceKernelGetProcessTimeWide();
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data);
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
    gl_perf.tex_sub.calls++; gl_perf.tex_sub.total_us+=us;
    if (us>gl_perf.tex_sub.max_us) gl_perf.tex_sub.max_us=us;
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    unsigned i;
    for(i=0;i<7;++i) if(!upload_groups[i].calls || upload_groups[i].caller==caller) break;
    /* The eighth group catches overflow rather than hiding unrecognized callers. */
    upload_groups[i].caller=i==7?0:caller;
    upload_groups[i].calls++; upload_groups[i].us+=us;
    if(us>upload_groups[i].max_us) {
        upload_groups[i].max_us=us; upload_groups[i].w=width; upload_groups[i].h=height;
        upload_groups[i].format=format; upload_groups[i].type=type;
    }
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
        uint64_t start = sceKernelGetProcessTimeWide();
        glCompileShader(shader); gl_time(&gl_perf.compile, start);
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
