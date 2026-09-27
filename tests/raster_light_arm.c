#include "utils/raster_light.h"
int light_test(const int16_t *a,const uint16_t *s,const int16_t *z,uint16_t *d,int n,uint16_t depth) {
    return light_try(a,s,z,d,n,depth);
}
volatile uintptr_t test_original;
void hook_test(const int16_t *a,const uint16_t *s,const int16_t *z,uint16_t *d,int n,uint16_t depth) {
    if(!light_try(a,s,z,d,n,depth)) ((void (*)(const int16_t *,const uint16_t *,const int16_t *,uint16_t *,int,uint16_t))test_original)(a,s,z,d,n,depth);
}
