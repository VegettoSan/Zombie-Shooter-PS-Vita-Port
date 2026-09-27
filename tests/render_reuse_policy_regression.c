#include <assert.h>
#include <stdio.h>
#include "../source/utils/render_reuse_policy.h"
int main(void) {
 RenderReusePolicy p={0};
 for(int i=0;i<100;i++) { assert(!render_reuse_choose(&p,1,123,0));render_reuse_complete(&p,2500); }
 assert(!p.expensive);
 assert(!render_reuse_choose(&p,1,123,0));render_reuse_complete(&p,1000000);assert(!p.expensive);
 render_reuse_complete(&p,2500);
 for(int i=0;i<4;i++) { render_reuse_choose(&p,1,123,0);render_reuse_complete(&p,12000); }
 assert(p.expensive);
 for(int i=0;i<20;i++) { assert(render_reuse_choose(&p,1,123,0)==(i&1));if(!(i&1)) render_reuse_complete(&p,12000); }
 for(int i=0;i<4;i++)render_reuse_complete(&p,3000);
 assert(!p.expensive && !render_reuse_choose(&p,1,123,0));
 assert(!render_reuse_choose(&p,2,124,0));render_reuse_complete(&p,1000);
 assert(!render_reuse_choose(&p,2,124,0));assert(render_reuse_choose(&p,2,124,0));
 assert(!render_reuse_choose(&p,2,124,1));assert(!p.has_frame);
 assert(!render_reuse_choose(&p,0,124,1));
 puts("Render reuse policy PASS: cheap logo renders, spike rejection, hysteresis, 2:1 expensive/forced, owner/argument invalidation");
}
