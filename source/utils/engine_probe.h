/* MIT. Only relocate the explicitly verified, PC-independent 8-byte
 * prologues of this canonical SO. This is not a general ARM relocator. */
#ifndef ZOMBIE_ENGINE_PROBE_H
#define ZOMBIE_ENGINE_PROBE_H
#include <stdint.h>
#include <string.h>
static inline int engine_probe_trampoline(uint32_t out[4],const void *original,
                                         const uint32_t expected[2],uint32_t resume) {
    if(memcmp(original,expected,8)) return 0;
    memcpy(out,original,8);
    out[2]=UINT32_C(0xf000f8df); /* Thumb-2 ldr.w pc,[pc,#0] */
    out[3]=resume|1u;
    return 1;
}
#endif
