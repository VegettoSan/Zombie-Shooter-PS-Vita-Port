/* MIT. Palette software raster acceleration, no scene/quality changes. */
#include "utils/raster_palette.h"
#include <string.h>
#include "utils/logger.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
extern so_module so_mod;
typedef void (*PaletteOriginal)(void *,uint32_t **,const uint32_t *,uint16_t **,uint16_t **,uint8_t **,int,int);
static PaletteOriginal originals[8];
static unsigned installed;
static struct { unsigned calls,fast,pixels,vector,skipped,samples,us,max; } counters[8];
#define ADD(p,v) __atomic_fetch_add((p),(v),__ATOMIC_RELAXED)
#define LOAD(p) __atomic_load_n((p),__ATOMIC_RELAXED)
static void palette_row(unsigned mode,void *self,uint32_t **pixels,const uint32_t *palette,
        uint16_t **z,uint16_t **source_z,uint8_t **source,int step,int count) {
    unsigned sequence=ADD(&counters[mode].calls,1);
    uint64_t start=(sequence&255)==0?sceKernelGetProcessTimeWide():0;
    const int32_t *depth=(mode&PALETTE_FLAT)?(const int32_t *)((uintptr_t)self+1188):NULL;
    PaletteWork work;
    if(palette_try(pixels,palette,z,source_z,source,step,count,mode,depth,&work)) {
        ADD(&counters[mode].fast,1);ADD(&counters[mode].pixels,(unsigned)count);
        if(work.vector_pixels) ADD(&counters[mode].vector,work.vector_pixels);
        if(work.skipped_pixels) ADD(&counters[mode].skipped,work.skipped_pixels);
    } else originals[mode](self,pixels,palette,z,source_z,source,step,count);
    if(start) {
        unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
        ADD(&counters[mode].samples,1);ADD(&counters[mode].us,us);
        unsigned old=LOAD(&counters[mode].max);
        while(us>old && !__atomic_compare_exchange_n(&counters[mode].max,&old,us,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
    }
}
#define ROW(number) static void row_##number(void *self,uint32_t **pixels,const uint32_t *palette,uint16_t **z,uint16_t **source_z,uint8_t **source,int step,int count) { palette_row(number,self,pixels,palette,z,source_z,source,step,count); }
ROW(0) ROW(1) ROW(2) ROW(3) ROW(4) ROW(5) ROW(6) ROW(7)
void raster_palette_install(void) {
    static const struct { const char *name;unsigned offset,length;uint32_t prologue[3];uintptr_t replacement; } hooks[]={
        {"_ZN11VID_SURFACE17pallete_SOFT_DRAWERPjP5ColorRPtS5_RPhii",0x51c3f2,12,{0xaf03b5f0,0x0f00e92d,0x6978b084},(uintptr_t)row_0},
        {"_ZN11VID_SURFACE22pallete_FLAT_SOFT_DRAWERPjP5ColorRPtS5_RPhii",0x51c334,8,{0xaf03b5f0,0x0f00e92d,0},(uintptr_t)row_1},
        {"_ZN11VID_SURFACE34pallete_SOFT_DRAW_NO_Z_INTEGRATIONERPjP5ColorRPtS5_RPhii",0x51c588,8,{0xaf03b5f0,0x0f00e92d,0},(uintptr_t)row_2},
        {"_ZN11VID_SURFACE39pallete_FLAT_SOFT_DRAW_NO_Z_INTEGRATIONERPjP5ColorRPtS5_RPhii",0x51c4d4,8,{0xaf03b5f0,0x0f00e92d,0},(uintptr_t)row_3},
        {"_ZN11VID_SURFACE24pallete_SOFT_DRAW_OPAQUEERPjP5ColorRPtS5_RPhii",0x51c6ac,8,{0xaf03b5f0,0x0b00e92d,0},(uintptr_t)row_4},
        {"_ZN11VID_SURFACE29pallete_FLAT_SOFT_DRAW_OPAQUEERPjP5ColorRPtS5_RPhii",0x51c64e,12,{0xaf03b5f0,0x0b00e92d,0xc014f8d7},(uintptr_t)row_5},
        {"_ZN11VID_SURFACE41pallete_SOFT_DRAW_NO_Z_INTEGRATION_OPAQUEERPjP5ColorRPtS5_RPhii",0x51c76c,8,{0xaf03b5f0,0x8d04f84d,0},(uintptr_t)row_6},
        {"_ZN11VID_SURFACE46pallete_FLAT_SOFT_DRAW_NO_Z_INTEGRATION_OPAQUEERPjP5ColorRPtS5_RPhii",0x51c716,12,{0xaf03b5f0,0x8d04f84d,0xc014f8d7},(uintptr_t)row_7}
    };
    for(unsigned i=0;i<8;++i) {
        uintptr_t addr=(uintptr_t)so_symbol(&so_mod,hooks[i].name),entry=addr&~(uintptr_t)1;
        uintptr_t arena=(so_mod.patch_head+3)&~(uintptr_t)3;unsigned size=hooks[i].length+8,allocation=size+24;
        if(!(addr&1) || entry!=so_mod.load_addr+hooks[i].offset || arena<so_mod.patch_base ||
           arena>so_mod.patch_base+so_mod.patch_size || so_mod.patch_base+so_mod.patch_size-arena<allocation ||
           memcmp((void *)entry,hooks[i].prologue,hooks[i].length)) {
            l_warn("[PATCH] palette row %u disabled: address/arena/prologue mismatch",i);continue;
        }
        uint32_t code[5];memcpy(code,hooks[i].prologue,hooks[i].length);
        code[hooks[i].length/4]=0xf000f8df;
        code[hooks[i].length/4+1]=(uint32_t)(entry+hooks[i].length)|1;
        sceClibMemcpy((void *)arena,code,size);originals[i]=(PaletteOriginal)(arena|1);
        uint32_t dispatch[6];palette_dispatch_emit(dispatch,(uint32_t)(arena|1),(uint32_t)hooks[i].replacement);
        sceClibMemcpy((void *)(arena+size),dispatch,sizeof(dispatch));
        so_mod.patch_head=arena+allocation;hook_addr(addr,arena+size);
        kuKernelFlushCaches((void *)arena,allocation);kuKernelFlushCaches((void *)entry,(entry&2)?10:8);
        installed|=1u<<i;
        l_info("[PATCH] palette row %u installed: so+0x%X",i,hooks[i].offset);
    }
}
void raster_palette_report(void) {
    l_perf("palette_hooks installed_mask=%u expected_mask=255 min_count=32",installed);
    static unsigned previous[8][7];
    for(unsigned i=0;i<8;++i) {
        unsigned now[]={LOAD(&counters[i].calls),LOAD(&counters[i].fast),LOAD(&counters[i].pixels),LOAD(&counters[i].vector),LOAD(&counters[i].skipped),LOAD(&counters[i].samples),LOAD(&counters[i].us)};
        if(now[0]!=previous[i][0]) l_perf("palette mode=%u calls=%u fast_rows=%u fast_pixels=%u vector_pixels=%u rejected_block_pixels=%u sampled_calls=%u sampled_us=%u max_us_lifetime=%u sample_period=256 min_count=32",i,now[0]-previous[i][0],now[1]-previous[i][1],now[2]-previous[i][2],now[3]-previous[i][3],now[4]-previous[i][4],now[5]-previous[i][5],now[6]-previous[i][6],LOAD(&counters[i].max));
        memcpy(previous[i],now,sizeof(now));
    }
}
