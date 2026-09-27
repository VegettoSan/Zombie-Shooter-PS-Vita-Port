/* MIT. Palette software raster acceleration, no scene/quality changes. */
#include "utils/raster_palette.h"
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
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
static void max_relaxed(unsigned *p,unsigned v) {
    unsigned old=LOAD(p);
    while(v>old && !__atomic_compare_exchange_n(p,&old,v,1,__ATOMIC_RELAXED,__ATOMIC_RELAXED)) {}
}
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
        ADD(&counters[mode].samples,1);ADD(&counters[mode].us,us);max_relaxed(&counters[mode].max,us);
    }
}
#define ROW(number) static void row_##number(void *self,uint32_t **pixels,const uint32_t *palette,uint16_t **z,uint16_t **source_z,uint8_t **source,int step,int count) { palette_row(number,self,pixels,palette,z,source_z,source,step,count); }
ROW(0) ROW(1) ROW(2) ROW(3) ROW(4) ROW(5) ROW(6) ROW(7)

/*
 * Pass 9: the Pass 8 hardware log showed VID_SOFTWARE::preparePalette taking
 * 4.8-9.4 ms/frame in heavy gameplay while draw_impl was only 1.5-3.5 ms.
 * Reverse engineering of the immutable Android SO shows the hot path is
 * VID::SetGammaToPalette: it invokes Color(Gamma, Color, alpha) 256 times.
 *
 * Gamma stores negative magnitudes in word 0 and positive magnitudes in word 1.
 * The original constructor computes, per RGB channel:
 *   min(255, (((256 - negative) * source) >> 8) + positive)
 * Alpha is clamp(source_alpha + Gamma::alpha(), 0, 255).
 * This implementation is bit-exact for every byte combination but performs
 * eight colors at once with NEON and avoids 256 C++ calls per palette.
 */
typedef struct { uint32_t negative,positive; } GammaRaw;
static unsigned gamma_installed;
static struct {
    unsigned calls,transformed,default_skips,null_skips,colors,vector_colors,samples,us,max;
} gamma_perf;

static inline unsigned gamma_byte(uint32_t word,unsigned shift) {
    return (word>>shift)&255u;
}
static inline int gamma_alpha_delta(const GammaRaw *g) {
    unsigned n=gamma_byte(g->negative,24);
    return n?-(int)n:(int)gamma_byte(g->positive,24);
}
static inline uint8x8_t gamma_rgb8(uint8x8_t source,unsigned negative,unsigned positive) {
    uint16x8_t value=vmovl_u8(source);
    value=vmulq_n_u16(value,(uint16_t)(256u-negative));
    value=vshrq_n_u16(value,8);
    value=vaddq_u16(value,vdupq_n_u16((uint16_t)positive));
    value=vminq_u16(value,vdupq_n_u16(255));
    return vmovn_u16(value);
}
static inline uint8x8_t gamma_alpha8(uint8x8_t source,int delta) {
    int16x8_t value=vreinterpretq_s16_u16(vmovl_u8(source));
    value=vaddq_s16(value,vdupq_n_s16((int16_t)delta));
    value=vmaxq_s16(value,vdupq_n_s16(0));
    value=vminq_s16(value,vdupq_n_s16(255));
    return vmovn_u16(vreinterpretq_u16_s16(value));
}
/* Portable scalar oracle used by regression tests/documentation. Packed Color
 * layout is AARRGGBB, matching the original ARM constructors. */
uint32_t zombie_gamma_palette_color(uint32_t color,uint32_t negative,uint32_t positive) {
    unsigned b=color&255u,g=(color>>8)&255u,r=(color>>16)&255u,a=color>>24;
    unsigned nb=negative&255u,ng=(negative>>8)&255u,nr=(negative>>16)&255u,na=negative>>24;
    unsigned pb=positive&255u,pg=(positive>>8)&255u,pr=(positive>>16)&255u,pa=positive>>24;
    b=(((256u-nb)*b)>>8)+pb;if(b>255u)b=255u;
    g=(((256u-ng)*g)>>8)+pg;if(g>255u)g=255u;
    r=(((256u-nr)*r)>>8)+pr;if(r>255u)r=255u;
    int alpha=(int)a+(na?-(int)na:(int)pa);if(alpha<0)alpha=0;else if(alpha>255)alpha=255;
    return b|(g<<8)|(r<<16)|((uint32_t)alpha<<24);
}
static void gamma_palette_fast(void *self,uint8_t *palette,const GammaRaw *gamma) {
    (void)self;
    unsigned sequence=ADD(&gamma_perf.calls,1);
    uint64_t start=(sequence&255u)==0?sceKernelGetProcessTimeWide():0;
    if(!palette || !gamma) {
        ADD(&gamma_perf.null_skips,1);
        return;
    }
    uint32_t negative=gamma->negative,positive=gamma->positive;
    if((negative|positive)==0) {
        ADD(&gamma_perf.default_skips,1);
        return;
    }
    const unsigned nb=gamma_byte(negative,0),ng=gamma_byte(negative,8),nr=gamma_byte(negative,16);
    const unsigned pb=gamma_byte(positive,0),pg=gamma_byte(positive,8),pr=gamma_byte(positive,16);
    const int ad=gamma_alpha_delta(gamma);
    for(unsigned i=0;i<256;i+=8) {
        uint8x8x4_t c=vld4_u8(palette+i*4);
        c.val[0]=gamma_rgb8(c.val[0],nb,pb);
        c.val[1]=gamma_rgb8(c.val[1],ng,pg);
        c.val[2]=gamma_rgb8(c.val[2],nr,pr);
        c.val[3]=gamma_alpha8(c.val[3],ad);
        vst4_u8(palette+i*4,c);
    }
    ADD(&gamma_perf.transformed,1);ADD(&gamma_perf.colors,256);ADD(&gamma_perf.vector_colors,256);
    if(start) {
        unsigned us=(unsigned)(sceKernelGetProcessTimeWide()-start);
        ADD(&gamma_perf.samples,1);ADD(&gamma_perf.us,us);max_relaxed(&gamma_perf.max,us);
    }
}
static void gamma_palette_install(void) {
    const char *symbol="_ZN3VID17SetGammaToPaletteEPhRK5Gamma";
    const unsigned offset=0x506378;
    const uint32_t expected[2]={0xaf03b5f0,0x8d04f84d};
    uintptr_t address=(uintptr_t)so_symbol(&so_mod,symbol),entry=address&~(uintptr_t)1;
    if(!(address&1) || entry!=so_mod.load_addr+offset || memcmp((void *)entry,expected,8)) {
        l_warn("[PATCH] gamma palette acceleration disabled: address/prologue mismatch");
        return;
    }
    hook_addr(address,(uintptr_t)&gamma_palette_fast);
    kuKernelFlushCaches((void *)entry,8);
    gamma_installed=1;
    l_perf("gamma_palette_install installed=1 so+0x%X colors_per_palette=256 vector_width=8",offset);
}

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
    l_perf("palette_install installed_mask=0x%02X rejected_mask=0x%02X",installed,255u & ~installed);
    gamma_palette_install();
}
void raster_palette_report(void) {
    l_perf("palette_hooks installed_mask=%u expected_mask=255 min_count=32",installed);
    static unsigned previous[8][7];
    for(unsigned i=0;i<8;++i) {
        unsigned now[]={LOAD(&counters[i].calls),LOAD(&counters[i].fast),LOAD(&counters[i].pixels),LOAD(&counters[i].vector),LOAD(&counters[i].skipped),LOAD(&counters[i].samples),LOAD(&counters[i].us)};
        if(now[0]!=previous[i][0]) l_perf("palette mode=%u calls=%u fast_rows=%u fast_pixels=%u vector_pixels=%u rejected_block_pixels=%u sampled_calls=%u sampled_us=%u max_us_lifetime=%u sample_period=256 min_count=32",i,now[0]-previous[i][0],now[1]-previous[i][1],now[2]-previous[i][2],now[3]-previous[i][3],now[4]-previous[i][4],now[5]-previous[i][5],now[6]-previous[i][6],LOAD(&counters[i].max));
        memcpy(previous[i],now,sizeof(now));
    }
    static unsigned old[8];
    unsigned now[]={LOAD(&gamma_perf.calls),LOAD(&gamma_perf.transformed),LOAD(&gamma_perf.default_skips),LOAD(&gamma_perf.null_skips),LOAD(&gamma_perf.colors),LOAD(&gamma_perf.vector_colors),LOAD(&gamma_perf.samples),LOAD(&gamma_perf.us)};
    l_perf("gamma_palette installed=%u calls=%u transformed=%u default_skips=%u null_skips=%u colors=%u vector_colors=%u sampled_calls=%u sampled_us=%u max_us_lifetime=%u sample_period=256",
        gamma_installed,now[0]-old[0],now[1]-old[1],now[2]-old[2],now[3]-old[3],now[4]-old[4],now[5]-old[5],now[6]-old[6],now[7]-old[7],LOAD(&gamma_perf.max));
    memcpy(old,now,sizeof(now));
}
