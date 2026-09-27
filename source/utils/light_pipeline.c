/* MIT. log_0003: legacy scanline unused, VID_LIGHT and layer11 costly.
 * Patch only two original signed divisions inside GRAPH::DrawLightSource.
 * The original division helper and every other caller remain unchanged. */
#include "utils/light_division.h"
#include "utils/engine_probe.h"
#include "utils/logger.h"
#include <string.h>
#include <stdbool.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
extern so_module so_mod;
static LightDivCache division_cache;
static uintptr_t native_division,originals[3];
static unsigned division_installed,installed;
static struct { unsigned calls,us,max; } phases[3];
#define ADD(p,v) __atomic_fetch_add((p),(v),__ATOMIC_RELAXED)
#define LOAD(p) __atomic_load_n((p),__ATOMIC_RELAXED)
static uint32_t divide_light(uint32_t n,uint32_t d) {
    int hit;uint32_t value;
    return light_div_try(&division_cache,n,d,&value,&hit)?value:
        ((uint32_t (*)(uint32_t,uint32_t))native_division)(n,d);
}
static void division_install(void) {
#ifndef ZOMBIE_LIGHT_DIVISION_EXPERIMENT
    l_perf("light_division installed_mask=0 expected_mask=0 experiment=off");
    return;
#else
    const char *symbol="_ZN5GRAPH15DrawLightSourceERK6VECTORff5ColorPK6SPRITEb";
    uintptr_t address=(uintptr_t)so_symbol(&so_mod,symbol),base=so_mod.load_addr;
    const uint32_t prologue[]={0xaf03b5f0,0x0f00e92d};
    const uint32_t divcode[]={0xe92d4090,0xe28d7004,0xe0204001,0xe0202fc0,0xe0213fc1,0xe0420fc0,0xe0431fc1,0xeb000002};
    const uint32_t first[]={0x9916bf24,0xe61ef0a4,0x7080f5c0};
    const uint32_t second[]={0x31e7f240,0xd392458e,0x46749916,0xe528f0a4,0xc05cf8dd};
    uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;
    uint16_t branches[2][2];
    if(address!=base+0x41299d || memcmp((void *)(address&~1u),prologue,8) ||
       memcmp((void *)(base+0x8b7c68),divcode,sizeof(divcode)) ||
       memcmp((void *)(base+0x413026),first,sizeof(first)) ||
       memcmp((void *)(base+0x41320a),second,sizeof(second)) ||
       arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
       so_mod.patch_base+so_mod.patch_size-arena<8 ||
       !light_thumb_bl(branches[0],base+0x41302a,arena) || !light_thumb_bl(branches[1],base+0x413216,arena)) {
        l_warn("[PATCH] light division disabled: anchor/calls/helper/range mismatch");return;
    }
    native_division=base+0x8b7c68; /* Original is ARM, returns signed quotient r0; caller discards r1. */
    uint32_t veneer[]={0xf000f8df,(uint32_t)(uintptr_t)divide_light};
    sceClibMemcpy((void *)arena,veneer,8);so_mod.patch_head=arena+8;
    kuKernelFlushCaches((void *)arena,8);
    sceClibMemcpy((void *)(base+0x41302a),branches[0],4);
    sceClibMemcpy((void *)(base+0x413216),branches[1],4);
    kuKernelFlushCaches((void *)(base+0x41302a),4);kuKernelFlushCaches((void *)(base+0x413216),4);
    division_installed=3;
    l_perf("light_division installed_mask=3 expected_mask=3 cache_entries=64 exact_signed_quotient=1 calls_only=GRAPH_DrawLightSource experiment=on");
#endif
}
static void record(unsigned i,uint64_t start) {
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);ADD(&phases[i].calls,1);ADD(&phases[i].us,us);
    unsigned old=LOAD(&phases[i].max);
    while(us>old && !__atomic_compare_exchange_n(&phases[i].max,&old,us,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
}
/* Opaque 32-bit float/Color words deliberately preserve SoftFP register/stack
 * layout without float arithmetic in the wrapper. bool remains the native bool. */
static uintptr_t draw_source(void *self,const void *v,uint32_t x,uint32_t y,uint32_t color,const void *sprite,bool shadow) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,const void *,uint32_t,uint32_t,uint32_t,const void *,bool))originals[0])(self,v,x,y,color,sprite,shadow);
    record(0,start);return ret;
}
static uintptr_t draw_as2(void *self,const void *v,uint32_t x,uint32_t y,uint32_t z,uint32_t w,uint32_t h,uint32_t color,const void *sprite,bool shadow) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,const void *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,const void *,bool))originals[1])(self,v,x,y,z,w,h,color,sprite,shadow);
    record(1,start);return ret;
}
static uintptr_t draw_stencil(void *self,const void *v,const void *size,bool shadow) {
    uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,const void *,const void *,bool))originals[2])(self,v,size,shadow);
    record(2,start);return ret;
}
void light_pipeline_install(void) {
    division_install();
    static const struct { const char *name;unsigned offset;uint32_t prologue[2];uintptr_t replacement; } hooks[]={
        {"_ZN5GRAPH15DrawLightSourceERK6VECTORff5ColorPK6SPRITEb",0x41299c,{0xaf03b5f0,0x0f00e92d},(uintptr_t)draw_source},
        {"_ZN5GRAPH19DrawLightSource_AS2ERK6VECTORfffff5ColorPK6SPRITEb",0x4132d0,{0xaf03b5f0,0x8d04f84d},(uintptr_t)draw_as2},
        {"_ZN5GRAPH19DrawShadowToStencilERK6VECTORRK7VECTOR2b",0x4134c4,{0xaf03b5f0,0x0f00e92d},(uintptr_t)draw_stencil}
    };
    for(unsigned i=0;i<3;++i) {
        uintptr_t address=(uintptr_t)so_symbol(&so_mod,hooks[i].name),entry=address&~1u;
        uintptr_t arena=(so_mod.patch_head+3)&~3u;uint32_t code[4];
        if(address!=so_mod.load_addr+hooks[i].offset+1 || arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
           so_mod.patch_base+so_mod.patch_size-arena<16 || !engine_probe_trampoline(code,(void *)entry,hooks[i].prologue,(uint32_t)(entry+8))) continue;
        sceClibMemcpy((void *)arena,code,16);originals[i]=arena|1;so_mod.patch_head=arena+16;
        hook_addr(address,hooks[i].replacement);kuKernelFlushCaches((void *)arena,16);kuKernelFlushCaches((void *)entry,8);installed|=1u<<i;
    }
    l_perf("light_pipeline installed_mask=%u expected_mask=7 inclusive=1",installed);
}
void light_pipeline_report(void) {
    static unsigned previous[3][2];
    unsigned populated=0,busy=0;
    for(unsigned i=0;i<64;++i) {
        unsigned key=__atomic_load_n(&division_cache.entry[i].denominator,__ATOMIC_ACQUIRE);
        populated+=key>=2;busy+=key==1;
    }
    l_perf("light_division installed_mask=%u cache_populated_entries_lifetime=%u cache_busy_entries=%u cache_entries=64 exact_signed_quotient=1 per_pixel_logging=0",
        division_installed,populated,busy);
    static const char *names[]={"source","AS2","stencil"};
    for(unsigned i=0;i<3;++i) {
        unsigned calls=LOAD(&phases[i].calls),us=LOAD(&phases[i].us),dc=calls-previous[i][0],du=us-previous[i][1];
        l_perf("light_pipeline name=%s installed=%u calls=%u inclusive_us=%u avg_us=%u max_us_lifetime=%u nested=1",
            names[i],(installed>>i)&1,dc,du,dc?du/dc:0,LOAD(&phases[i].max));previous[i][0]=calls;previous[i][1]=us;
    }
}
