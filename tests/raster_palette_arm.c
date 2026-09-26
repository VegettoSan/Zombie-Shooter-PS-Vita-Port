#include "utils/raster_palette.h"
volatile unsigned test_mode;
int palette_test(void *self,uint32_t **pixels,const uint32_t *palette,uint16_t **z,
                 uint16_t **source_z,uint8_t **source,int step,int count) {
    PaletteWork work;
    const int32_t *flat=(test_mode&PALETTE_FLAT)?(const int32_t *)((uintptr_t)self+1188):0;
    return palette_try(pixels,palette,z,source_z,source,step,count,test_mode,flat,&work);
}

void dispatch_test(uint32_t out[6],uint32_t original,uint32_t replacement) {
    palette_dispatch_emit(out,original,replacement);
}
