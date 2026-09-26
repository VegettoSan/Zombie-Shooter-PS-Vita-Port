#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "utils/zombie_shader_cache.h"
#include "shaders/precompiled_blit_v.h"
int main(void) {
    unsigned char payload[32]={0};ZscHeader h={0x43535a47,ZSC_VERSION,0x8b31,1,sizeof(payload),zsc_checksum(payload,sizeof(payload)),42};
    assert(zsc_validate(&h,payload,sizeof(payload),0x8b31,1,42));
    assert(!zsc_validate(&h,payload,sizeof(payload)-1,0x8b31,1,42));
    assert(!zsc_validate(&h,payload,sizeof(payload),0x8b30,1,42));
    assert(!zsc_validate(&h,payload,sizeof(payload),0x8b31,0,42));
    assert(!zsc_validate(&h,payload,sizeof(payload),0x8b31,1,43));
    for(unsigned i=0;i<sizeof(payload);++i){payload[i]^=1;assert(!zsc_validate(&h,payload,sizeof(payload),0x8b31,1,42));payload[i]^=1;}
    h.version++;assert(!zsc_validate(&h,payload,sizeof(payload),0x8b31,1,42));h.version--;
    h.magic=0;assert(!zsc_validate(&h,payload,sizeof(payload),0x8b31,1,42));
    assert(size_blit_v==sizeof(blit_v));
    assert(zsc_gxp_bounds(blit_v,sizeof(blit_v))); /* declared282, stored284 */
    assert(!zsc_gxp_bounds(blit_v,159));
    assert(!zsc_gxp_bounds(blit_v,281));
    assert(!zsc_gxp_bounds(blit_v,298)); /* more than15 padding bytes */
    unsigned char bad[sizeof(blit_v)];memcpy(bad,blit_v,sizeof(bad));
    memset(bad+36,255,4);assert(!zsc_gxp_bounds(bad,sizeof(bad)));
    memcpy(bad,blit_v,sizeof(bad));memset(bad+40,255,4);
    assert(!zsc_gxp_bounds(bad,sizeof(bad)));
    puts("Shader cache envelope and real GXP padding/bounds regression passed: type/pair/version, truncation and corruption rejection");
}
