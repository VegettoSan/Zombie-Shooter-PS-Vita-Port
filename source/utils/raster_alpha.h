/* MIT. Exact fixed-depth alpha scanlines from the canonical Android SO.
 * Z is read-only. Reject aliasing before batching; unsupported rows use ARM. */
#ifndef ZOMBIE_RASTER_ALPHA_H
#define ZOMBIE_RASTER_ALPHA_H
#include <stdint.h>
#include <stddef.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
typedef struct { unsigned vector_pixels, skipped_pixels; } AlphaWork;
static inline void alpha_dispatch_emit(uint32_t out[5],uint32_t original,uint32_t replacement) {
    out[0]=0xe3530020; /* cmp r3,#32, no stack/register changes */
    out[1]=0xb59ff000;out[2]=0xe59ff000;out[3]=original;out[4]=replacement;
}
static inline int alpha_overlap(uintptr_t a,size_t n,uintptr_t b,size_t m) {
    if(n>UINTPTR_MAX-a || m>UINTPTR_MAX-b) return 1;
    return a<b+m && b<a+n;
}
static inline uint32_t alpha_blend(uint32_t s,uint32_t d) {
    unsigned a=s>>24,inv=255-a,da=d>>24;
    /* R/B products fit in two separate 16-bit lanes: weights sum255,
     * each sum <=65025, so no cross-lane carry or 32-bit overflow. */
    uint32_t rb=((((s&0x00ff00ffu)*a+(d&0x00ff00ffu)*inv)>>8)&0x00ff00ffu);
    unsigned g=(((s>>8)&255)*a+((d>>8)&255)*inv)>>8;
    unsigned out_a=da+(((255-da)*a)>>8);
    return (out_a<<24)|(g<<8)|rb;
}
/* indexed=0: source is RGBA; indexed=1: source bytes index 256 RGBA colors. */
static inline int alpha_try(const uint8_t *source,const uint16_t *z,uint32_t *dst,
        int count,uint16_t depth,const uint32_t *palette,unsigned indexed,AlphaWork *work) {
    if(count<32 || count>16384 || indexed>1 || !source || !z || !dst ||
       ((uintptr_t)z&1) || ((uintptr_t)dst&3) ||
       (indexed?(!palette || ((uintptr_t)palette&3)):((uintptr_t)source&3))) return 0;
    size_t colors=(size_t)count*4,depths=(size_t)count*2,src_size=indexed?(size_t)count:colors;
    if(alpha_overlap((uintptr_t)dst,colors,(uintptr_t)source,src_size) ||
       alpha_overlap((uintptr_t)dst,colors,(uintptr_t)z,depths) ||
       (indexed && alpha_overlap((uintptr_t)dst,colors,(uintptr_t)palette,1024))) return 0;
    unsigned vector=0,skipped=0;int i=0;
#ifdef __ARM_NEON
    for(;i+8<=count;i+=8) {
        uint16x8_t mask=vcgeq_u16(vdupq_n_u16(depth),vld1q_u16(z+i));
        uint32x2_t any=vreinterpret_u32_u16(vorr_u16(vget_low_u16(mask),vget_high_u16(mask)));
        uint32x2_t all=vreinterpret_u32_u16(vand_u16(vget_low_u16(mask),vget_high_u16(mask)));
        if(!(vget_lane_u32(any,0)|vget_lane_u32(any,1))) { skipped+=8;continue; }
        if((vget_lane_u32(all,0)&vget_lane_u32(all,1))==UINT32_MAX) {
            uint32_t gathered[8];const uint8_t *src=source+4*i;
            if(indexed) { for(unsigned k=0;k<8;++k) gathered[k]=palette[source[i+k]];src=(const uint8_t *)gathered; }
            uint8x8x4_t s=vld4_u8(src),d=vld4_u8((const uint8_t *)(dst+i));
            uint16x8_t a=vmovl_u8(s.val[3]),inv=vsubq_u16(vdupq_n_u16(255),a);
            for(unsigned channel=0;channel<3;++channel)
                d.val[channel]=vshrn_n_u16(vmlaq_u16(vmulq_u16(vmovl_u8(s.val[channel]),a),vmovl_u8(d.val[channel]),inv),8);
            d.val[3]=vadd_u8(d.val[3],vshrn_n_u16(vmulq_u16(vsubq_u16(vdupq_n_u16(255),vmovl_u8(d.val[3])),a),8));
            vst4_u8((uint8_t *)(dst+i),d);vector+=8;continue;
        }
        for(unsigned k=0;k<8;++k) {
            unsigned j=(unsigned)i+k;
            if(depth>=z[j]) dst[j]=alpha_blend(indexed?palette[source[j]]:((const uint32_t *)source)[j],dst[j]);
        }
    }
#endif
    for(;i<count;++i) if(depth>=z[i]) dst[i]=alpha_blend(indexed?palette[source[i]]:((const uint32_t *)source)[i],dst[i]);
    work->vector_pixels=vector;work->skipped_pixels=skipped;return 1;
}
#endif
