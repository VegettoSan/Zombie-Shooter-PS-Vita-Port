#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/zombie_texture_update.h"

static uint32_t rng_state = 0x5A17C0DEu;
static uint32_t rng32(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static uint16_t reference_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    /* VitaGL read_rgb565(): R bits 0..4, G 5..10, B 11..15. */
    return (uint16_t)(((uint16_t)r >> 3) |
                      ((uint16_t)(g >> 2) << 5) |
                      ((uint16_t)(b >> 3) << 11));
}

static void known_colors(void) {
    const uint8_t src[] = {
        255,   0,   0,   0,
          0, 255,   0,  17,
          0,   0, 255, 255,
        255, 255, 255, 123,
          0,   0,   0,  77,
    };
    uint16_t dst[5] = {0};
    zombie_texture_rgba8888_to_rgb565(dst, src, 5, 1, sizeof(dst), sizeof(src));
    assert(dst[0] == 0x001Fu);
    assert(dst[1] == 0x07E0u);
    assert(dst[2] == 0xF800u);
    assert(dst[3] == 0xFFFFu);
    assert(dst[4] == 0x0000u);
}

static void randomized_stride_case(unsigned width, unsigned height,
                                   unsigned src_pad, unsigned dst_pad) {
    const size_t src_stride = (size_t)width * 4 + src_pad;
    const size_t dst_stride = (size_t)width * 2 + dst_pad;
    const size_t src_bytes = src_stride * height;
    const size_t dst_bytes = dst_stride * height;
    uint8_t *src = malloc(src_bytes + 64);
    uint8_t *dst = malloc(dst_bytes + 64);
    uint8_t *expected = malloc(dst_bytes + 64);
    assert(src && dst && expected);

    for (size_t i = 0; i < src_bytes + 64; ++i) src[i] = (uint8_t)rng32();
    memset(dst, 0xA5, dst_bytes + 64);
    memset(expected, 0xA5, dst_bytes + 64);

    for (unsigned y = 0; y < height; ++y) {
        const uint8_t *s = src + (size_t)y * src_stride;
        uint16_t *d = (uint16_t *)(expected + (size_t)y * dst_stride);
        for (unsigned x = 0; x < width; ++x) {
            d[x] = reference_rgb565(s[x * 4 + 0], s[x * 4 + 1], s[x * 4 + 2]);
        }
    }

    zombie_texture_rgba8888_to_rgb565(dst, src, width, height,
                                      dst_stride, src_stride);
    assert(memcmp(dst, expected, dst_bytes + 64) == 0);

    /* Alpha must not influence this final opaque framebuffer texture. */
    if (width && height) {
        uint8_t *src2 = malloc(src_bytes + 64);
        uint8_t *dst2 = malloc(dst_bytes + 64);
        assert(src2 && dst2);
        memcpy(src2, src, src_bytes + 64);
        memset(dst2, 0xA5, dst_bytes + 64);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
                src2[(size_t)y * src_stride + x * 4 + 3] ^= 0xFFu;
        zombie_texture_rgba8888_to_rgb565(dst2, src2, width, height,
                                          dst_stride, src_stride);
        assert(memcmp(dst, dst2, dst_bytes + 64) == 0);
        free(src2);
        free(dst2);
    }

    free(src);
    free(dst);
    free(expected);
}

int main(void) {
    known_colors();

    /* Widths straddle the 8-pixel NEON block boundary; host builds exercise
     * the scalar reference implementation while Vita/ARM compilation checks
     * the same header's vector path. */
    const unsigned widths[] = {1, 2, 7, 8, 9, 15, 16, 17, 63, 64, 65, 863, 864};
    const unsigned heights[] = {1, 2, 7, 31, 489};
    for (unsigned wi = 0; wi < sizeof(widths)/sizeof(widths[0]); ++wi) {
        for (unsigned hi = 0; hi < sizeof(heights)/sizeof(heights[0]); ++hi) {
            unsigned w = widths[wi], h = heights[hi];
            randomized_stride_case(w, h, (wi * 3u) & 15u, (hi * 2u) & 14u);
        }
    }

    /* 250k independent color tuples catch channel-order/quantization mistakes. */
    for (unsigned i = 0; i < 250000; ++i) {
        uint8_t pixel[4] = {(uint8_t)rng32(), (uint8_t)rng32(),
                            (uint8_t)rng32(), (uint8_t)rng32()};
        uint16_t got = 0;
        zombie_texture_rgba8888_to_rgb565(&got, pixel, 1, 1, 2, 4);
        assert(got == reference_rgb565(pixel[0], pixel[1], pixel[2]));
    }

    puts("Framebuffer565 regression passed: VitaGL R-low RGB565 layout, 250k colors, alpha independence, strides, guards and 864x489 surface");
    return 0;
}
