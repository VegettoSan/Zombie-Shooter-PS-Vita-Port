/* MIT. Exact signed-depth light mask, derived from canonical SO 0x510b00. */
#ifndef ZOMBIE_RASTER_LIGHT_H
#define ZOMBIE_RASTER_LIGHT_H
#include <stdint.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
static inline uint16_t light_pixel(int16_t source_z,uint16_t source,int16_t scene_z,int16_t depth) {
    int delta=(int)source_z+(int)depth-(int)scene_z;
    if(delta<0) return 0;
    return delta>127?(uint16_t)(source|15u):(uint16_t)(source|((unsigned)(delta>>3)<<12));
}
static inline int light_overlap(uintptr_t a,uintptr_t b,uintptr_t bytes) {
    return a<b?b-a<bytes:a-b<bytes;
}
static inline int light_try(const int16_t *source_z,const uint16_t *source,const int16_t *scene_z,
                            uint16_t *dst,int count,uint16_t depth) {
    if(count<16 || count>16384 || !source_z || !source || !scene_z || !dst ||
       (((uintptr_t)source_z|(uintptr_t)source|(uintptr_t)scene_z|(uintptr_t)dst)&1)) return 0;
    uintptr_t bytes=(uintptr_t)count*2;
    if(light_overlap((uintptr_t)dst,(uintptr_t)source_z,bytes) ||
       light_overlap((uintptr_t)dst,(uintptr_t)source,bytes) ||
       light_overlap((uintptr_t)dst,(uintptr_t)scene_z,bytes)) return 0;
    int i=0;
#ifdef __ARM_NEON
    int32x4_t d=vdupq_n_s32((int16_t)depth),zero=vdupq_n_s32(0),limit=vdupq_n_s32(127);
    for(;i+8<=count;i+=8) {
        int16x8_t a=vld1q_s16(source_z+i),z=vld1q_s16(scene_z+i);
        int32x4_t lo=vsubq_s32(vaddq_s32(vmovl_s16(vget_low_s16(a)),d),vmovl_s16(vget_low_s16(z)));
        int32x4_t hi=vsubq_s32(vaddq_s32(vmovl_s16(vget_high_s16(a)),d),vmovl_s16(vget_high_s16(z)));
        uint16x8_t valid=vcombine_u16(vmovn_u32(vcgeq_s32(lo,zero)),vmovn_u32(vcgeq_s32(hi,zero)));
        uint64x2_t blocks=vreinterpretq_u64_u16(valid);
        if(!(vgetq_lane_u64(blocks,0)|vgetq_lane_u64(blocks,1))) {
            vst1q_u16(dst+i,vdupq_n_u16(0));continue;
        }
        uint32x4_t ml=vbslq_u32(vcgtq_s32(lo,limit),vdupq_n_u32(15),vshlq_n_u32(vreinterpretq_u32_s32(vshrq_n_s32(lo,3)),12));
        uint32x4_t mh=vbslq_u32(vcgtq_s32(hi,limit),vdupq_n_u32(15),vshlq_n_u32(vreinterpretq_u32_s32(vshrq_n_s32(hi,3)),12));
        uint16x8_t mask=vcombine_u16(vmovn_u32(ml),vmovn_u32(mh));
        vst1q_u16(dst+i,vandq_u16(vorrq_u16(vld1q_u16(source+i),mask),valid));
    }
#endif
    for(;i<count;++i) {
        int delta=(int)source_z[i]+(int)(int16_t)depth-(int)scene_z[i];
        dst[i]=delta<0?0:light_pixel(source_z[i],source[i],scene_z[i],(int16_t)depth);
    }
    return 1;
}
void raster_light_install(void);
void raster_light_report(void);
#endif
