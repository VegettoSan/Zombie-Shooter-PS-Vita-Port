#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc="${VITASDK:-/usr/local/vitasdk}/bin/arm-vita-eabi-gcc"
python3 - "$tmp" <<'PY'
from pathlib import Path
import sys
text=Path('source/utils/light_pipeline.c').read_text()
a=text.index('static uint32_t divide_light');b=text.index('static void division_install',a)
wrapper=text[a:b].replace('static uint32_t divide_light','uint32_t division_hook')
Path(sys.argv[1]+'/production.c').write_text('#include "utils/light_division.h"\nLightDivCache division_cache;\nuintptr_t native_division=0x988b7c68;\n'+wrapper+'\nint branch_test(uint16_t o[2],uintptr_t s,uintptr_t t) { return light_thumb_bl(o,s,t); }\n')
PY
for opt in 0 3; do
 gcc -O"$opt" -Wall -Wextra -Werror -pthread -Isource tests/light_division_host.c -o "$tmp/host"
 "$tmp/host"
 "$cc" -O"$opt" -Wall -Wextra -Werror -mfloat-abi=softfp -mfpu=neon -ffreestanding -fno-builtin -nostdlib -Isource "$tmp/production.c" -Wl,-Ttext=0x10000,-e,division_hook -lgcc -o "$tmp/division.elf"
 python3 tests/light_division_arm_regression.py --repo "$PWD" --neon "$tmp/division.elf"
done
