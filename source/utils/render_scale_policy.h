/* MIT. Cap the engine work surface without changing physical DisplayMetrics. */
#ifndef ZOMBIE_RENDER_SCALE_POLICY_H
#define ZOMBIE_RENDER_SCALE_POLICY_H
#include <stdint.h>
#include <math.h>
static inline float render_scale_cap(uint32_t width,uint32_t height,
                                     float original,int requested) {
    if (!requested || width!=960 || height!=544 ||
        !(original>0.0f) || !isfinite(original)) return original;
    if(requested!=864 && requested!=960) return original;
    float cap=(float)requested/(float)width;
    return original>cap ? cap : original;
}
#endif
