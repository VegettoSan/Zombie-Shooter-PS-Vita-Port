/* Exact 3.6.1 / 1161 Thumb upload contract; fail closed on source drift. */
#ifndef ZOMBIE_SOFTWARE_FRAMEBUFFER_CONTRACT_H
#define ZOMBIE_SOFTWARE_FRAMEBUFFER_CONTRACT_H
#include <stdint.h>
#include <string.h>
#define SOFTWARE_UNLOCK_ENTRY 0x0047EE38u
#define SOFTWARE_UNLOCK_TEXSUB_RETURN 0x0047EECDu
static inline int software_framebuffer_contract(const void *entry) {
    const uint32_t prologue[]={0xaf03b5f0u,0x0700e92du};
    /* OpenGLES::UnLock(bool)+0x60: dimensions from +0x218/+0x21c,
     * RGBA/U8, producer +0x210, target 2D, level/x/y zero, BLX TexSub.
     * Instructions contain no relocations; PLT resolution changes no bytes. */
    const uint32_t upload[]={
        0x4101f241u,0x1a87ed94u,0x1208f641u,0x0ac0eebdu,
        0x0210f8d4u,0x1ac1eebdu,0x2102e9cdu,0xf6409004u,
        0x210050e1u,0x23002200u,0x0a00ed8du,0x1a01ed8du,
        0xe252f06bu
    };
    return entry && !memcmp(entry,prologue,sizeof(prologue)) &&
        !memcmp((const uint8_t *)entry+0x60,upload,sizeof(upload));
}
#endif
