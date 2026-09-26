#include "utils/raster_alpha.h"
volatile unsigned test_mode;
int alpha_test(const uint8_t *s,const uint16_t *z,uint32_t *d,int n,uint16_t depth,const uint32_t *p) {
    AlphaWork work;return alpha_try(s,z,d,n,depth,p,test_mode,&work);
}
void dispatch_test(uint32_t out[5],uint32_t original,uint32_t replacement) { alpha_dispatch_emit(out,original,replacement); }
volatile uintptr_t test_original;
void hook_test(const uint8_t *s,const uint16_t *z,uint32_t *d,int n,uint16_t depth,const uint32_t *p) {
    AlphaWork work;
    if(!alpha_try(s,z,d,n,depth,p,test_mode,&work))
        ((void (*)(const uint8_t *,const uint16_t *,uint32_t *,int,uint16_t,const uint32_t *))test_original)(s,z,d,n,depth,p);
}
