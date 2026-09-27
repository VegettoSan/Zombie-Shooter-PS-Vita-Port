/* MIT. Only expensive software rendering is reused; simulation never skipped.
 * Four consecutive measured renders enter/exit the mode, avoiding a decision
 * from a single loading spike. Owner/argument changes always prime a new frame. */
#ifndef ZOMBIE_RENDER_REUSE_POLICY_H
#define ZOMBIE_RENDER_REUSE_POLICY_H
#include <stdint.h>
typedef struct { uintptr_t owner;int argument,has_frame,expensive;unsigned slow,fast,phase; } RenderReusePolicy;
static inline int render_reuse_choose(RenderReusePolicy *p,int mode,uintptr_t owner,int argument) {
    if(p->owner!=owner || p->argument!=argument) {
        *p=(RenderReusePolicy){.owner=owner,.argument=argument};
    }
    if(!mode || !p->has_frame || (mode==1 && !p->expensive)) { p->phase=0;return 0; }
    return (p->phase++ & 1u)!=0;
}
static inline void render_reuse_complete(RenderReusePolicy *p,unsigned us) {
    p->has_frame=1;
    if(us>=8000) { p->fast=0;if(p->slow<4) p->slow++;if(p->slow==4) p->expensive=1; }
    else if(us<=4000) { p->slow=0;if(p->fast<4) p->fast++;if(p->fast==4) { p->expensive=0;p->phase=0; } }
    else p->slow=p->fast=0;
}
#endif
