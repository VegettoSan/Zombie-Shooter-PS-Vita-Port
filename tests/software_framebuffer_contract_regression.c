#include <assert.h>
#include <stdio.h>
#include "../source/utils/software_framebuffer_contract.h"
#include "../source/utils/upload_reuse_guard.h"
#include "../source/utils/render_reuse_policy.h"
int main(int argc,char **argv) {
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char bytes[0x94];assert(fread(bytes,1,sizeof(bytes),f)==sizeof(bytes));fclose(f);
    assert(software_framebuffer_contract(bytes));
    assert(!software_framebuffer_contract(NULL));
    for(unsigned i=0;i<sizeof(bytes);i++) if(i<8 || i>=0x60) {
        bytes[i]^=1;assert(!software_framebuffer_contract(bytes));bytes[i]^=1;
    }
    UploadReuseGuard upload={0};RenderReusePolicy policy={0};
    for(unsigned i=0;i<4;i++) {
        assert(!render_reuse_choose(&policy,upload.valid?1:0,123,4));
        render_reuse_complete(&policy,10000);
        upload_reuse_complete(&upload,7,456,768,435);
    }
    assert(!render_reuse_choose(&policy,1,123,4));
    render_reuse_complete(&policy,10000);
    assert(render_reuse_choose(&policy,1,123,4));
    assert(upload_reuse_matches(&upload,7,456,768,435));
    upload_reuse_invalidate(&upload);
    assert(!render_reuse_choose(&policy,upload.valid?1:0,123,4));
    assert(!upload_reuse_matches(&upload,7,456,768,435));
    puts("3.6.1 framebuffer contract + adaptive reuse + invalidation: PASS");
}
