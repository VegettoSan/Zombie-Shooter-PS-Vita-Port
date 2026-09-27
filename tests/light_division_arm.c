#include "utils/light_division.h"
LightDivCache test_cache;
int test_hit;
uint32_t division_hook(uint32_t n,uint32_t d) {
    uint32_t value;int fast=light_div_try(&test_cache,n,d,&value,&test_hit);
    if(fast) return value;
    test_hit=-1;
    return ((uint32_t (*)(uint32_t,uint32_t))(uintptr_t)0x988b7c68)(n,d);
}
int branch_test(uint16_t out[2],uintptr_t site,uintptr_t target) { return light_thumb_bl(out,site,target); }
