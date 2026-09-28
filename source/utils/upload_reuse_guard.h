/* MIT. One completed software upload is reusable only for the same producer,
 * texture object and dimensions. Any image redefinition or texture deletion
 * invalidates it, including numeric texture-ID recycling. */
#ifndef ZOMBIE_UPLOAD_REUSE_GUARD_H
#define ZOMBIE_UPLOAD_REUSE_GUARD_H
#include <stdint.h>
typedef struct { unsigned texture;uintptr_t producer;int w,h,valid; } UploadReuseGuard;
static inline int upload_reuse_matches(const UploadReuseGuard *g,unsigned t,uintptr_t p,int w,int h) {
 return g->valid && t && g->texture==t && g->producer==p && g->w==w && g->h==h;
}
static inline void upload_reuse_complete(UploadReuseGuard *g,unsigned t,uintptr_t p,int w,int h) {
 *g=(UploadReuseGuard){t,p,w,h,t!=0};
}
static inline void upload_reuse_invalidate(UploadReuseGuard *g) {g->valid=0;}
#endif
