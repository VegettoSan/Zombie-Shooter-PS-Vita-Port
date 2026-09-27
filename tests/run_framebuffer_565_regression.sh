#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for opt in 0 3; do
    cc -std=gnu11 -O"$opt" -Wall -Wextra -Ilib/vitagl/source tests/framebuffer_565_regression.c -o "$tmp/framebuffer565-$opt"
    "$tmp/framebuffer565-$opt"
done

grep -q 'vld4_u8' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'r = vshrq_n_u16(r, 3)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'g = vshlq_n_u16(vshrq_n_u16(g, 2), 5)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'b = vshlq_n_u16(vshrq_n_u16(b, 3), 11)' lib/vitagl/source/utils/zombie_texture_update.h
grep -q 'SOFTWARE_UNLOCK_TEXSUB_RETURN 0x0046BF59u' source/utils/glutil.c
grep -q 'framebuffer_565 = 1' source/utils/settings.c
