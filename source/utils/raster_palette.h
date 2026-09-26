/* MIT. Exact palette scanline math recovered from the canonical Android SO.
 * Safe row-local pointers replace repeated reference loads/stores. NEON only
 * batches eight visible pixels; partial and occluded blocks preserve accesses. */
#ifndef ZOMBIE_RASTER_PALETTE_H
#define ZOMBIE_RASTER_PALETTE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
enum { PALETTE_FLAT=1, PALETTE_NO_Z_WRITE=2, PALETTE_OPAQUE=4 };
typedef struct { unsigned vector_pixels, skipped_pixels; } PaletteWork;
/* ARM dispatch: short rows go straight to the original Thumb trampoline,
 * preserving all arguments/stack/LR and avoiding guards/counters entirely. */
static inline void palette_dispatch_emit(uint32_t out[6],uint32_t original,uint32_t replacement) {
    out[0]=0xe59dc00c; /* ldr ip,[sp,#12]: eighth argument, count */
    out[1]=0xe35c0020; /* cmp ip,#32 */
    out[2]=0xb59ff000; /* ldrlt pc,[pc,#0] -> original */
    out[3]=0xe59ff000; /* ldr pc,[pc,#0] -> replacement */
    out[4]=original;out[5]=replacement;
}
static inline uint32_t palette_blend(uint32_t s,uint32_t d) {
    unsigned a=s>>24,inv=256-a;
    unsigned rb=((s&0xff)*a+(d&0xff)*inv)>>8;
    unsigned g=(((s>>8)&0xff)*a+((d>>8)&0xff)*inv)>>8;
    unsigned b=(((s>>16)&0xff)*a+((d>>16)&0xff)*inv)>>8;
    return 0xff000000u|(b<<16)|(g<<8)|rb;
}
static inline int palette_overlap(uintptr_t a,size_t n,uintptr_t b,size_t m) {
    if(n>UINTPTR_MAX-a || m>UINTPTR_MAX-b) return 1;
    return a<b+m && b<a+n;
}
static inline int palette_try(uint32_t **pixels,const uint32_t *palette,
        uint16_t **z,uint16_t **source_z,uint8_t **source,int step,int count,
        unsigned mode,const int32_t *flat_depth,PaletteWork *work) {
    if(count<8 || count>16384 || step!=1 || mode>7 || !pixels || !z || !source ||
       !palette || (!(mode&PALETTE_FLAT) && !source_z)) return 0;
    if(((uintptr_t)pixels&(sizeof(void *)-1)) || ((uintptr_t)z&(sizeof(void *)-1)) ||
       ((uintptr_t)source&(sizeof(void *)-1)) ||
       (!(mode&PALETTE_FLAT) && ((uintptr_t)source_z&(sizeof(void *)-1)))) return 0;
    uint32_t *dst=*pixels;uint16_t *dz=*z;
    uint16_t *sz=(mode&PALETTE_FLAT)?NULL:*source_z;uint8_t *src=*source;
    if(!dst || !dz || !src || (!(mode&PALETTE_FLAT) && !sz) ||
       ((uintptr_t)dst&3) || ((uintptr_t)palette&3) || ((uintptr_t)dz&1) ||
       (!(mode&PALETTE_FLAT) && ((uintptr_t)sz&1))) return 0;
    int32_t flat=0;
    if(mode&PALETTE_FLAT) {
        if(!flat_depth || ((uintptr_t)flat_depth&3)) return 0;
        flat=*flat_depth;if(flat<0 || flat>65535) return 0;
    }
    /* References must be distinct, aligned and in a compact region. Prove
     * that whole region disjoint from arrays: conservative, cheaper than
     * comparing every individual cell with every buffer for every scanline. */
    uintptr_t refs[]={(uintptr_t)pixels,(uintptr_t)z,(uintptr_t)source,(uintptr_t)source_z};
    unsigned ref_count=(mode&PALETTE_FLAT)?3:4;
    uintptr_t low=refs[0],high=low;
    for(unsigned i=0;i<ref_count;++i) {
        if(refs[i]&(sizeof(void *)-1)) return 0;
        if(refs[i]<low) low=refs[i];
        if(refs[i]>high) high=refs[i];
        for(unsigned j=0;j<i;++j) if(refs[i]==refs[j]) return 0;
    }
    if(high-low>256) return 0;
    size_t refs_size=(size_t)(high-low)+sizeof(void *),colors=(size_t)count*4,depths=(size_t)count*2;
#define OVER(a,n,b,m) palette_overlap((uintptr_t)(a),(n),(uintptr_t)(b),(m))
    if(OVER(dst,colors,dz,depths) || OVER(dst,colors,src,count) || OVER(dst,colors,palette,1024) ||
       OVER(low,refs_size,dst,colors) || OVER(low,refs_size,dz,depths) ||
       OVER(low,refs_size,src,count) || OVER(low,refs_size,palette,1024)) return 0;
    if(!(mode&PALETTE_NO_Z_WRITE) && (OVER(dz,depths,src,count) || OVER(dz,depths,palette,1024))) return 0;
    if(mode&PALETTE_FLAT) {
        if(OVER(dst,colors,flat_depth,4) || OVER(low,refs_size,flat_depth,4) ||
           (!(mode&PALETTE_NO_Z_WRITE) && OVER(dz,depths,flat_depth,4))) return 0;
    } else if(OVER(dst,colors,sz,depths) || OVER(low,refs_size,sz,depths) ||
              (!(mode&PALETTE_NO_Z_WRITE) && OVER(dz,depths,sz,depths))) return 0;
#undef OVER
    unsigned vector_pixels=0,skipped_pixels=0;int offset=0;
#ifdef __ARM_NEON
    for(;offset+8<=count;offset+=8) {
        uint16x8_t depths=(mode&PALETTE_FLAT)?vdupq_n_u16((uint16_t)flat):vld1q_u16(sz+offset);
        uint16x8_t mask=vcgeq_u16(depths,vld1q_u16(dz+offset));
        uint32x2_t all=vreinterpret_u32_u16(vand_u16(vget_low_u16(mask),vget_high_u16(mask)));
        uint32x2_t any=vreinterpret_u32_u16(vorr_u16(vget_low_u16(mask),vget_high_u16(mask)));
        if(!(vget_lane_u32(any,0)|vget_lane_u32(any,1))) { skipped_pixels+=8;continue; }
        if((vget_lane_u32(all,0)&vget_lane_u32(all,1))==UINT32_MAX) {
            uint32_t colors[8];
            for(unsigned j=0;j<8;++j) colors[j]=palette[src[offset+j]];
            if(mode&PALETTE_OPAQUE) {
                vst1q_u32(dst+offset,vld1q_u32(colors));
                vst1q_u32(dst+offset+4,vld1q_u32(colors+4));
            } else {
                uint8x8x4_t s=vld4_u8((const uint8_t *)colors);
                uint8x8x4_t d=vld4_u8((const uint8_t *)(dst+offset));
                uint16x8_t alpha=vmovl_u8(s.val[3]),inv=vsubq_u16(vdupq_n_u16(256),alpha);
                for(unsigned channel=0;channel<3;++channel)
                    d.val[channel]=vshrn_n_u16(vmlaq_u16(vmulq_u16(vmovl_u8(s.val[channel]),alpha),vmovl_u8(d.val[channel]),inv),8);
                d.val[3]=vdup_n_u8(255);vst4_u8((uint8_t *)(dst+offset),d);
            }
            if(!(mode&PALETTE_NO_Z_WRITE)) vst1q_u16(dz+offset,depths);
            vector_pixels+=8;continue;
        }
        /* Mixed visibility: avoid gathering/reading/writing rejected pixels. */
        for(unsigned j=0;j<8;++j) {
            int k=offset+(int)j;unsigned depth=(mode&PALETTE_FLAT)?(unsigned)flat:sz[k];
            if(depth>=dz[k]) {
                uint32_t color=palette[src[k]];
                if(!(mode&PALETTE_NO_Z_WRITE)) dz[k]=(uint16_t)depth;
                dst[k]=(mode&PALETTE_OPAQUE)?color:palette_blend(color,dst[k]);
            }
        }
    }
#endif
    for(;offset<count;++offset) {
        unsigned depth=(mode&PALETTE_FLAT)?(unsigned)flat:sz[offset];
        if(depth>=dz[offset]) {
            uint32_t color=palette[src[offset]];
            if(!(mode&PALETTE_NO_Z_WRITE)) dz[offset]=(uint16_t)depth;
            dst[offset]=(mode&PALETTE_OPAQUE)?color:palette_blend(color,dst[offset]);
        }
    }
    *pixels=dst+count;*z=dz+count;*source=src+count;
    if(!(mode&PALETTE_FLAT)) *source_z=sz+count;
    work->vector_pixels=vector_pixels;work->skipped_pixels=skipped_pixels;
    return 1;
}
#endif
