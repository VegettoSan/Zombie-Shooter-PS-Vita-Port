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

static void shader_cache_report(void);
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

    #ifdef ZOMBIE_GC_EXPERIMENT
    vglSetupGarbageCollector(127,0x20000);
#endif
    vglInitExtended(0, 960, 544, 6 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    extern char vgl_shader_cache_path[256];
    if(file_mkpath(DATA_PATH "cache/shaders/.probe",0777))
        snprintf(vgl_shader_cache_path,256,DATA_PATH "cache/shaders");
    else vgl_shader_cache_path[0]=0; // Cache IO fails safely; normal compilation.
    gl_initialized = 1;
    l_perf("render width=960 height=544 msaa=none experiment=%s shader_cache=native_v2",ZOMBIE_VITAGL_EXPERIMENT);
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
        shader_cache_report();
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
    if(format==GL_RGBA && type==GL_UNSIGNED_BYTE && width>=720 && height>=400) {
        static int last_width,last_height;
        if(width!=last_width || height!=last_height) {
            l_perf("software_surface width=%d height=%d stride_bytes=%u bytes=%llu caller=0x%08X",width,height,(unsigned)width*4,(unsigned long long)width*height*4,(unsigned)(uintptr_t)__builtin_return_address(0));
            last_width=width;last_height=height;
        }
    }
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


/* Native cache/inventory. Source observation happens only on glShaderSource,
 * program combinations on link; a short lock never runs on sprite draws. */
static unsigned char shader_inventory_lock;
static struct { uint64_t hash;unsigned type,length,uses; } shader_inventory[256];
static struct {uint64_t vertex,fragment;unsigned uses;} program_inventory[256];
static unsigned unique_shaders,unique_programs,inventory_overflow;
void vglZombieShaderObserved(uint64_t hash,unsigned type,unsigned length) {
    while(__atomic_test_and_set(&shader_inventory_lock,__ATOMIC_ACQUIRE)) {}
    unsigned i;for(i=0;i<unique_shaders;++i) if(shader_inventory[i].hash==hash && shader_inventory[i].type==type)break;
    if(i==unique_shaders && i<256) {
        shader_inventory[i].hash=hash;shader_inventory[i].type=type;shader_inventory[i].length=length;++unique_shaders;
        l_perf("shader_unique hash=%016llX type=0x%X source_length=%u",(unsigned long long)hash,type,length);
    }
    if(i<256)shader_inventory[i].uses++;else inventory_overflow++;
    __atomic_clear(&shader_inventory_lock,__ATOMIC_RELEASE);
}
void vglZombieProgramObserved(uint64_t vertex,uint64_t fragment) {
    while(__atomic_test_and_set(&shader_inventory_lock,__ATOMIC_ACQUIRE)) {}
    unsigned i;for(i=0;i<unique_programs;++i)if(program_inventory[i].vertex==vertex && program_inventory[i].fragment==fragment)break;
    if(i==unique_programs && i<256){program_inventory[i].vertex=vertex;program_inventory[i].fragment=fragment;++unique_programs;}
    if(i<256)program_inventory[i].uses++;else inventory_overflow++;
    __atomic_clear(&shader_inventory_lock,__ATOMIC_RELEASE);
}
extern void vglZombieShaderCacheStatsGet(uint32_t *out);
static void shader_cache_report(void) {
    uint32_t stats[9];vglZombieShaderCacheStatsGet(stats);
    unsigned vertex=0,fragment=0,uses=0;
    while(__atomic_test_and_set(&shader_inventory_lock,__ATOMIC_ACQUIRE)) {}
    for(unsigned i=0;i<unique_shaders;++i){vertex+=shader_inventory[i].type==GL_VERTEX_SHADER;fragment+=shader_inventory[i].type==GL_FRAGMENT_SHADER;}
    for(unsigned i=0;i<unique_programs;++i)uses+=program_inventory[i].uses;
    l_perf("shader_inventory unique_vertex=%u unique_fragment=%u unique_program_combinations=%u program_uses=%u overflow=%u",vertex,fragment,unique_programs,uses,inventory_overflow);
    __atomic_clear(&shader_inventory_lock,__ATOMIC_RELEASE);
    l_perf("shader_cache hits=%u misses=%u invalid=%u bytes_loaded=%u compile_us=%u load_us=%u writes_failed=%u actual_compile_calls=%u writes=%u counters=lifetime",stats[0],stats[1],stats[2],stats[3],stats[4],stats[5],stats[6],stats[7],stats[8]);
}
void glShaderSource_soloader(GLuint shader,GLsizei count,const GLchar **strings,const GLint *lengths) {
    /* Native vitaGL already handles concatenation and negative lengths. */
    glShaderSource(shader,count,strings,lengths);
}
void glCompileShader_soloader(GLuint shader) {
    uint64_t start=sceKernelGetProcessTimeWide();glCompileShader(shader);gl_time(&gl_perf.compile,start);
}
