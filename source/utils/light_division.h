/* MIT. Exact ARM signed divmod with bounded, coherent reciprocal cache.
 * No approximation: a final remainder check corrects the floor reciprocal.
 * Inputs/outputs are register bit patterns; unsigned negation avoids INT_MIN UB. */
#ifndef ZOMBIE_LIGHT_DIVISION_H
#define ZOMBIE_LIGHT_DIVISION_H
#include <stdint.h>
typedef struct { uint32_t denominator,reciprocal; } LightDivEntry;
typedef struct { LightDivEntry entry[64]; } LightDivCache;
static inline uint32_t light_abs_bits(uint32_t value) { return value>>31?0u-value:value; }
/* Entries publish exactly once: key0 empty, key1 being initialized, key>=2
 * immutable. A collision/contended entry falls back to original, no spinning
 * or reciprocal recomputation per pixel. Acquire key protects immutable m. */
static inline int light_div_try(LightDivCache *cache,uint32_t numerator,uint32_t denominator,uint32_t *out,int *hit) {
    uint32_t n=light_abs_bits(numerator),d=light_abs_bits(denominator),q,m;
    *hit=0;
    if(!d) return 0;
    if(d==1) q=n;
    else {
        unsigned slot=(d^(d>>8)^(d>>16))&63u;
        LightDivEntry *entry=&cache->entry[slot];
        uint32_t key=__atomic_load_n(&entry->denominator,__ATOMIC_ACQUIRE);
        if(key==d) { m=entry->reciprocal;*hit=1; }
        else {
            if(key) return 0;
            uint32_t expected=0;
            if(!__atomic_compare_exchange_n(&entry->denominator,&expected,1,0,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)) return 0;
            m=(uint32_t)((UINT64_C(1)<<32)/d);
            entry->reciprocal=m;
            __atomic_store_n(&entry->denominator,d,__ATOMIC_RELEASE);
        }
        q=(uint32_t)(((uint64_t)n*m)>>32);
        if(n-q*d>=d) ++q;
    }
    if((numerator^denominator)>>31) q=0u-q;
    *out=q;
    return 1;
}
/* ARM Thumb-2 BL to a nearby Thumb veneer; reject rather than truncate range. */
static inline int light_thumb_bl(uint16_t out[2],uintptr_t site,uintptr_t target) {
    int64_t delta=(int64_t)target-(int64_t)(site+4);
    if((site|target)&1 || delta<-(INT64_C(1)<<24) || delta>=(INT64_C(1)<<24)) return 0;
    uint32_t bits=(uint32_t)delta;
    unsigned s=(bits>>24)&1,i1=(bits>>23)&1,i2=(bits>>22)&1;
    out[0]=(uint16_t)(0xf000|(s<<10)|((bits>>12)&1023));
    out[1]=(uint16_t)(0xd000|((!(i1^s))<<13)|((!(i2^s))<<11)|((bits>>1)&2047));
    return 1;
}
#endif
