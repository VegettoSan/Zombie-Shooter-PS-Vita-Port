#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_root=${1:-"$root"}
testdir=$(mktemp -d)
trap 'rm -rf -- "$testdir"' EXIT
cp -a "$source_root/lib/falso_jni" "$testdir/falso_jni"
# ARM-only _AtoV is not used by these V/variadic JNI tests. Retain all real
# vtable/lookup/array/dispatch functions; abort if a test accidentally uses A.
python3 - "$testdir/falso_jni" <<'PY'
from pathlib import Path
import sys
r=Path(sys.argv[1])
p=r/'FalsoJNI_ImplBridge.h'; p.write_text(p.read_text().replace('va_list _AtoV(int dummy, ...);','void *_AtoV(int dummy, ...);'))
p=r/'FalsoJNI_ImplBridge.c'; s=p.read_text(); pos=s.index('va_list _AtoV(int dummy, ...) {'); p.write_text(s[:pos]+'void *_AtoV(int dummy, ...) { __builtin_abort(); }\n')
PY
for optimization in 0 3; do
  mode=-DZOMBIE_RELEASE_BUILD=1
  if [[ "$optimization" == 0 ]]; then mode=-DZOMBIE_DEBUG_BUILD=1; fi
  gcc -std=gnu11 -O"$optimization" -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -I"$root/tests/input_host" -I"$source_root/source" -c "$source_root/source/utils/gamepad.c" -o "$testdir/profile.o"
  g++ -std=gnu++20 "$mode" -O"$optimization" -ffunction-sections -fdata-sections \
    -I"$root/tests/input_host" -I"$source_root/lib/falso_ndk" -I"$source_root/source" \
    "$root/tests/gamepad_regression.cpp" "$source_root/lib/falso_ndk/shim/fndk_controls.cpp" \
    "$source_root/lib/falso_ndk/android/AInput.cpp" "$testdir/profile.o" -Wl,--gc-sections -o "$testdir/pad"
  "$testdir/pad"
  gcc -std=gnu11 -O"$optimization" -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    "$mode" -I"$testdir" -I"$source_root/lib" "$root/tests/input_device_regression.c" "$source_root/source/java.c" \
    "$testdir/falso_jni/FalsoJNI.c" "$testdir/falso_jni/FalsoJNI_ImplBridge.c" "$testdir/falso_jni/converter.c" \
    -Wl,--gc-sections -o "$testdir/device"
  "$testdir/device"
done
