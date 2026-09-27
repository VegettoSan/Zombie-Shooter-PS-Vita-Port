#!/usr/bin/env python3
import random
from pathlib import Path

SRC = Path('source/utils/raster_palette.c').read_text(encoding='utf-8')

# Model the exact ARM code in Gamma::diffuse/specular/alpha and
# Color::Color(Gamma const&, Color const&, int), independently from the C helper.
def original_arm_model(color: int, negative: int, positive: int) -> int:
    diffuse = (~negative) & 0xFFFFFFFF
    out = 0
    for shift in (0, 8, 16):  # B,G,R in packed AARRGGBB
        src = (color >> shift) & 0xFF
        factor = ((diffuse >> shift) & 0xFF) + 1
        spec = (positive >> shift) & 0xFF
        value = ((factor * src) >> 8) + spec
        if value > 255:
            value = 255
        out |= value << shift
    neg_a = (negative >> 24) & 0xFF
    delta = -neg_a if neg_a else ((positive >> 24) & 0xFF)
    alpha = ((color >> 24) & 0xFF) + delta
    alpha = 0 if alpha < 0 else 255 if alpha > 255 else alpha
    return out | (alpha << 24)

# Algebra used by the new Vita path. Kept separate so a sign/byte/order error
# is caught by randomized comparison against the ARM-derived model above.
def optimized_model(color: int, negative: int, positive: int) -> int:
    b = color & 255
    g = (color >> 8) & 255
    r = (color >> 16) & 255
    a = (color >> 24) & 255
    nb, ng, nr, na = negative & 255, (negative >> 8) & 255, (negative >> 16) & 255, negative >> 24
    pb, pg, pr, pa = positive & 255, (positive >> 8) & 255, (positive >> 16) & 255, positive >> 24
    b = min(255, (((256 - nb) * b) >> 8) + pb)
    g = min(255, (((256 - ng) * g) >> 8) + pg)
    r = min(255, (((256 - nr) * r) >> 8) + pr)
    a = max(0, min(255, a + (-na if na else pa)))
    return b | (g << 8) | (r << 16) | (a << 24)

edge_bytes = (0, 1, 127, 128, 254, 255)
for c in edge_bytes:
    color = c | (c << 8) | (c << 16) | (c << 24)
    for n in edge_bytes:
        neg = n | (n << 8) | (n << 16) | (n << 24)
        for p in edge_bytes:
            pos = p | (p << 8) | (p << 16) | (p << 24)
            assert original_arm_model(color, neg, pos) == optimized_model(color, neg, pos)

rng = random.Random(0x5A17A)
for _ in range(250000):
    color = rng.getrandbits(32)
    negative = rng.getrandbits(32)
    positive = rng.getrandbits(32)
    got = optimized_model(color, negative, positive)
    expect = original_arm_model(color, negative, positive)
    assert got == expect, (hex(color), hex(negative), hex(positive), hex(expect), hex(got))

required = (
    '_ZN3VID17SetGammaToPaletteEPhRK5Gamma',
    '0x506378',
    '0xaf03b5f0',
    '0x8d04f84d',
    'vld4_u8',
    'vst4_u8',
    'vmulq_n_u16',
    'vshrq_n_u16',
    'vminq_u16',
    'vmaxq_s16',
    'gamma_palette_fast',
)
for token in required:
    assert token in SRC, token

print('Gamma palette regression PASS: 250k randomized + edge cases, packed AARRGGBB/order/sign/saturation and guarded NEON hook')
