#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc="${VITASDK:-/usr/local/vitasdk}/bin/arm-vita-eabi-gcc"
for opt in 0 3; do
  "$cc" -O"$opt" -Wall -Wextra -Werror -mfloat-abi=softfp -mfpu=neon -ffreestanding -fno-builtin -nostdlib -Isource tests/raster_alpha_arm.c -Wl,-Ttext=0x10000,-e,alpha_test -o "$tmp/neon.elf"
  "$cc" -O"$opt" -Wall -Wextra -Werror -mfloat-abi=softfp -mfpu=neon -U__ARM_NEON -ffreestanding -fno-builtin -nostdlib -Isource tests/raster_alpha_arm.c -Wl,-Ttext=0x10000,-e,alpha_test -o "$tmp/scalar.elf"
  python3 tests/raster_alpha_arm_regression.py --repo "$PWD" --scalar "$tmp/scalar.elf" --neon "$tmp/neon.elf"
done
