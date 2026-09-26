#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "utils/engine_probe.h"
int main(void) {
    const uint32_t prologues[][2]={{0xaf03b5f0,0x0f00e92d},{0xaf03b5f0,0xbd04f84d},{0xaf03b5f0,0x8d04f84d}};
    for(unsigned i=0;i<3;++i) {
        uint32_t output[4]={0};
        assert(engine_probe_trampoline(output,prologues[i],prologues[i],0x98410a38));
        assert(!memcmp(output,prologues[i],8));
        assert(output[2]==0xf000f8df && output[3]==0x98410a39);
        uint32_t changed[2]={prologues[i][0]^1,prologues[i][1]},before[4];
        memcpy(before,output,sizeof(before));
        assert(!engine_probe_trampoline(output,changed,prologues[i],0));
        assert(!memcmp(output,before,sizeof(output)));
    }
    puts("Engine probe emission regression passed: exact prologues, Thumb resume, mismatch leaves output intact");
}
