/* MIT. Fixed-depth alpha family used by VID_SOFTWARE::draw_impl.
 * Boot-only guarded hooks. Hot rows have sampled timing, no logging or thread lookup. */
#include "utils/raster_alpha.h"
#include "utils/logger.h"
#include <string.h>
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
extern so_module so_mod;
static uintptr_t originals[2];static unsigned installed;
static struct { unsigned calls,fast,pixels,vector,skipped,samples,us,max; } counters[2];
#define ADD(p,v) __atomic_fetch_add((p),(v),__ATOMIC_RELAXED)
#define LOAD(p) __atomic_load_n((p),__ATOMIC_RELAXED)
static void alpha_row(unsigned mode,const uint8_t *source,const uint16_t *z,uint32_t *dst,int count,uint16_t depth,const uint32_t *palette) {
    unsigned sequence=ADD(&counters[mode].calls,1);
    uint64_t start=(sequence&255)==0?sceKernelGetProcessTimeWide():0;
    AlphaWork work;
    if(alpha_try(source,z,dst,count,depth,palette,mode,&work)) {
        ADD(&counters[mode].fast,1);ADD(&counters[mode].pixels,(unsigned)count);
        if(work.vector_pixels) ADD(&counters[mode].vector,work.vector_pixels);
        if(work.skipped_pixels) ADD(&counters[mode].skipped,work.skipped_pixels);
    } else if(mode) ((void (*)(const uint8_t *,const uint16_t *,uint32_t *,int,uint16_t,const uint32_t *))originals[1])(source,z,dst,count,depth,palette);
    else ((void (*)(const uint8_t *,const uint16_t *,uint32_t *,int,uint16_t))originals[0])(source,z,dst,count,depth);
    if(start) {
        unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);ADD(&counters[mode].samples,1);ADD(&counters[mode].us,us);
        unsigned old=LOAD(&counters[mode].max);
        while(us>old && !__atomic_compare_exchange_n(&counters[mode].max,&old,us,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
    }
}
static void row_rgba(const uint8_t *s,const uint16_t *z,uint32_t *d,int n,uint16_t depth) { alpha_row(0,s,z,d,n,depth,NULL); }
static void row_indexed(const uint8_t *s,const uint16_t *z,uint32_t *d,int n,uint16_t depth,const uint32_t *p) { alpha_row(1,s,z,d,n,depth,p); }
void raster_alpha_install(void) {
    /* The original count<1 test/return (first6 bytes) stays in place.
     * Hook at push, copying complete PC-independent prologue instructions. */
    static const struct { const char *name;unsigned offset,length;uint32_t prologue[3];uintptr_t replacement; } hooks[]={
        {"_Z19drawLineWithAlpha32PhPtP5Colorit",0x470642,8,{0xaf03b5f0,0x0f00e92d,0},(uintptr_t)row_rgba},
        {"_Z18AsmDrawWithAlpha32PhPtP5ColoritPv",0x4707b8,12,{0xaf03b5f0,0x0f00e92d,0xc008f8d7},(uintptr_t)row_indexed}
    };
    const uint16_t early[]={0x2b01,0xbfb8,0x4770};
    for(unsigned i=0;i<2;++i) {
        uintptr_t addr=(uintptr_t)so_symbol(&so_mod,hooks[i].name),base=addr&~(uintptr_t)1,entry=base+6;
        uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;unsigned size=hooks[i].length+8,allocation=size+20;
        if(!(addr&1) || base!=so_mod.load_addr+hooks[i].offset || arena<so_mod.patch_base ||
           arena>so_mod.patch_base+so_mod.patch_size || so_mod.patch_base+so_mod.patch_size-arena<allocation ||
           memcmp((void *)base,early,6) || memcmp((void *)entry,hooks[i].prologue,hooks[i].length)) {
            l_warn("[PATCH] alpha row %u disabled: address/arena/prologue mismatch",i);continue;
        }
        uint32_t code[5];memcpy(code,hooks[i].prologue,hooks[i].length);
        code[hooks[i].length/4]=0xf000f8df;code[hooks[i].length/4+1]=(uint32_t)(entry+hooks[i].length)|1;
        sceClibMemcpy((void *)arena,code,size);originals[i]=arena|1;
        uint32_t dispatch[5];alpha_dispatch_emit(dispatch,(uint32_t)(arena|1),(uint32_t)hooks[i].replacement);
        sceClibMemcpy((void *)(arena+size),dispatch,sizeof(dispatch));so_mod.patch_head=arena+allocation;
        hook_addr(entry|1,arena+size);kuKernelFlushCaches((void *)arena,allocation);kuKernelFlushCaches((void *)entry,(entry&2)?10:8);
        installed|=1u<<i;
    }
    l_perf("alpha_hooks installed_mask=%u expected_mask=3 min_count=32",installed);
}
void raster_alpha_report(void) {
    static unsigned previous[2][7];
    for(unsigned i=0;i<2;++i) {
        unsigned now[]={LOAD(&counters[i].calls),LOAD(&counters[i].fast),LOAD(&counters[i].pixels),LOAD(&counters[i].vector),LOAD(&counters[i].skipped),LOAD(&counters[i].samples),LOAD(&counters[i].us)};
        l_perf("alpha mode=%u installed=%u calls=%u fast_rows=%u fast_pixels=%u vector_pixels=%u rejected_block_pixels=%u sampled_calls=%u sampled_us=%u max_us_lifetime=%u sample_period=256 min_count=32",i,(installed>>i)&1,now[0]-previous[i][0],now[1]-previous[i][1],now[2]-previous[i][2],now[3]-previous[i][3],now[4]-previous[i][4],now[5]-previous[i][5],now[6]-previous[i][6],LOAD(&counters[i].max));
        memcpy(previous[i],now,sizeof(now));
    }
}
