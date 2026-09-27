#include "utils/light_division.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
static LightDivCache cache;
static unsigned fast_count;
static void *worker(void *arg) {
    uint32_t state=(uint32_t)(uintptr_t)arg;
    for(unsigned i=0;i<100000;++i) {
        state=state*1664525u+1013904223u;uint32_t n=state;
        state=state*1664525u+1013904223u;
        uint32_t d=(i&1)?state:(1000+(state&31));if(!d)d=1;
        if(i%3==0)d=0u-d;
        int hit;uint32_t actual=0xaaaaaaaa;
        int fast=light_div_try(&cache,n,d,&actual,&hit);
        int64_t a=(int32_t)n,b=(int32_t)d;
        if(fast) {assert(actual==(uint32_t)(a/b));__atomic_fetch_add(&fast_count,1,__ATOMIC_RELAXED);}
        else assert(actual==0xaaaaaaaa);
    }return 0;
}
int main(void) {
    pthread_t threads[8];for(unsigned i=0;i<8;++i)assert(!pthread_create(&threads[i],0,worker,(void *)(uintptr_t)(i+1)));
    for(unsigned i=0;i<8;++i)assert(!pthread_join(threads[i],0));
    assert(fast_count>1000);
    puts("Light signed division PASS: 800k concurrent attempts, immutable publication/collision fallback, exact successful quotients");
}
