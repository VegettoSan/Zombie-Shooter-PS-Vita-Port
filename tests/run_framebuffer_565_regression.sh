#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for opt in 0 3; do
    cc -std=gnu11 -O"$opt" -Wall -Wextra -Ilib/vitagl/source tests/framebuffer_565_regression.c -o "$tmp/framebuffer565-$opt"
    "$tmp/framebuffer565-$opt"
done

# Pass11 keeps the corrected converter only as an explicit diagnostic path.
# Real Vita testing proved conventional RGB565 layout: R high, B low.
grep -q 'vld4_u8' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'r = vshlq_n_u16(vshrq_n_u16(r, 3), 11)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'g = vshlq_n_u16(vshrq_n_u16(g, 2), 5)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'b = vshrq_n_u16(b, 3)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'SOFTWARE_UNLOCK_TEXSUB_RETURN 0x0046BF59u' source/utils/glutil.c
# Release default is native RGBA after Pass10 measured ~12-13 ms conversion
# versus ~6 ms for the original RGBA upload. Legacy value 1 migrates to 0.
grep -q 'setting_framebuffer_565 = 0' source/utils/settings.c
grep -q 'setting_framebuffer_565 = value == 2 ? 2 : 0' source/utils/settings.c
grep -q 'setting_software_frameskip = 1' source/utils/settings.c
