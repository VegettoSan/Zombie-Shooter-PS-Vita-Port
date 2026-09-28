#include <assert.h>
#include <stdio.h>
#include "../source/utils/upload_reuse_guard.h"
#include "../source/utils/render_reuse_policy.h"
int main(void) {
 UploadReuseGuard g={0};
 assert(!upload_reuse_matches(&g,7,0x1234,864,489));
 upload_reuse_complete(&g,7,0x1234,864,489);
 assert(upload_reuse_matches(&g,7,0x1234,864,489));
 assert(!upload_reuse_matches(&g,8,0x1234,864,489));
 assert(!upload_reuse_matches(&g,7,0x5678,864,489));
 assert(!upload_reuse_matches(&g,7,0x1234,960,544));
 upload_reuse_invalidate(&g);
 assert(!upload_reuse_matches(&g,7,0x1234,864,489)); // recycled ID
 upload_reuse_complete(&g,0,0x1234,864,489);
 assert(!upload_reuse_matches(&g,0,0x1234,864,489));
 RenderReusePolicy policy={0};
 upload_reuse_complete(&g,7,0x1234,864,489);
 for(unsigned i=0;i<4;i++) {
   assert(!render_reuse_choose(&policy,g.valid?1:0,9,0));render_reuse_complete(&policy,9000);
 }
 assert(!render_reuse_choose(&policy,1,9,0));assert(render_reuse_choose(&policy,1,9,0));
 upload_reuse_invalidate(&g);
 assert(!render_reuse_choose(&policy,g.valid?1:0,9,0)); // force a new producer frame
 puts("Upload reuse lifetime guard PASS");
}
