/* MIT. Boot-guarded light kernel and inclusive per-layer measurements. */
#include "utils/raster_light.h"
#include "utils/engine_probe.h"
#include "utils/logger.h"
#include <string.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
extern so_module so_mod;
static uintptr_t originals[2];
static unsigned installed;
static struct { unsigned calls,fast,pixels,samples,us,max; } rows;
static struct { unsigned calls,us,max; } layers[17],light_draw;
#define ADD(p,v) __atomic_fetch_add((p),(v),__ATOMIC_RELAXED)
#define LOAD(p) __atomic_load_n((p),__ATOMIC_RELAXED)
static void maximum(unsigned *p,unsigned v) {
    unsigned old=LOAD(p);
    while(v>old && !__atomic_compare_exchange_n(p,&old,v,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
}
static void light_row(const int16_t *a,const uint16_t *s,const int16_t *z,uint16_t *dst,int n,uint16_t depth) {
    unsigned seq=ADD(&rows.calls,1);
    uint64_t start=(seq&255u)==0?sceKernelGetProcessTimeWide():0;
    if(light_try(a,s,z,dst,n,depth)) { ADD(&rows.fast,1);ADD(&rows.pixels,(unsigned)n); }
    else ((void (*)(const int16_t *,const uint16_t *,const int16_t *,uint16_t *,int,uint16_t))originals[0])(a,s,z,dst,n,depth);
    if(start) { unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);ADD(&rows.samples,1);ADD(&rows.us,us);maximum(&rows.max,us); }
}
void raster_light_layer_record(int layer,unsigned us) {
    unsigned index=layer>=0 && layer<16?(unsigned)layer:16;
    ADD(&layers[index].calls,1);ADD(&layers[index].us,us);maximum(&layers[index].max,us);
}
static uintptr_t draw_light(void *self,const void *sprite) {
    ADD(&light_draw.calls,1);uint64_t start=sceKernelGetProcessTimeWide();
    uintptr_t ret=((uintptr_t (*)(void *,const void *))originals[1])(self,sprite);
    unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);ADD(&light_draw.us,us);maximum(&light_draw.max,us);return ret;
}
void raster_light_install(void) {
    static const struct { const char *symbol;unsigned offset;uint32_t prologue[2];uintptr_t replacement; } hooks[]={
        {"_Z17AsmDrawLightWithZPhS_PtS0_it",0x510b00,{0xaf03b5f0,0x0b00e92d},(uintptr_t)light_row},
        {"_ZN9VID_LIGHT4DrawEPK6SPRITE",0x511878,{0xaf03b5f0,0x0f00e92d},(uintptr_t)draw_light}
    };
    for(unsigned i=0;i<2;++i) {
        uintptr_t address=(uintptr_t)so_symbol(&so_mod,hooks[i].symbol),entry=address&~(uintptr_t)1;
        uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;uint32_t code[4];
        if(!(address&1) || entry!=so_mod.load_addr+hooks[i].offset ||
           arena<so_mod.patch_base || arena>so_mod.patch_base+so_mod.patch_size ||
           so_mod.patch_base+so_mod.patch_size-arena<sizeof(code) ||
           !engine_probe_trampoline(code,(void *)entry,hooks[i].prologue,(uint32_t)(entry+8))) {
            l_warn("[PATCH] light hook %u disabled: address/arena/prologue mismatch",i);continue;
        }
        sceClibMemcpy((void *)arena,code,sizeof(code));originals[i]=arena|1;so_mod.patch_head=arena+sizeof(code);
        hook_addr(address,hooks[i].replacement);kuKernelFlushCaches((void *)arena,sizeof(code));kuKernelFlushCaches((void *)entry,8);installed|=1u<<i;
    }
    l_perf("light_hooks installed_mask=%u expected_mask=3 min_count=16 vector_width=8",installed);
}
void raster_light_report(void) {
    static unsigned old[5],previous[18][2];
    unsigned now[]={LOAD(&rows.calls),LOAD(&rows.fast),LOAD(&rows.pixels),LOAD(&rows.samples),LOAD(&rows.us)};
    l_perf("light_rows installed=%u calls=%u fast_rows=%u fast_pixels=%u sampled_calls=%u sampled_us=%u max_sample_us_lifetime=%u sample_period=256",
        installed&1,now[0]-old[0],now[1]-old[1],now[2]-old[2],now[3]-old[3],now[4]-old[4],LOAD(&rows.max));memcpy(old,now,sizeof(now));
    for(unsigned i=0;i<18;++i) {
        unsigned calls=i==17?LOAD(&light_draw.calls):LOAD(&layers[i].calls);
        unsigned us=i==17?LOAD(&light_draw.us):LOAD(&layers[i].us);
        unsigned max=i==17?LOAD(&light_draw.max):LOAD(&layers[i].max);
        unsigned dc=calls-previous[i][0],du=us-previous[i][1];
        if(dc) l_perf("scene_draw kind=%s layer=%d calls=%u inclusive_us=%u avg_us=%u max_us_lifetime=%u nested=1",
            i==17?"VID_LIGHT":"DrawLayer",i==17?-1:(i==16?-2:(int)i),dc,du,du/dc,max);
        previous[i][0]=calls;previous[i][1]=us;
    }
}
