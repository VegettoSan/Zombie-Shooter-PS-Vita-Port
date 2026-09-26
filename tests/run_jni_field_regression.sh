#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_root=${1:-"$root"}
testdir=$(mktemp -d)
trap 'rm -rf -- "$testdir"' EXIT
cp -a "$source_root/lib/falso_jni" "$testdir/falso_jni"
# x86_64 va_list is an array; ARM's unrelated _AtoV returns a value. Omit only
# that unused helper from the host translation. Field/array code stays intact.
python3 - "$testdir/falso_jni" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
p=root/'FalsoJNI_ImplBridge.h'
s=p.read_text().replace('va_list _AtoV(int dummy, ...);', '')
p.write_text(s)
p=root/'FalsoJNI_ImplBridge.c'
s=p.read_text()
pos=s.index('va_list _AtoV(int dummy, ...) {')
assert s[pos:].rstrip().endswith('}')
p.write_text(s[:pos])
PY
for optimization in 0 3; do
  gcc -std=gnu11 -O"$optimization" -ffunction-sections -fdata-sections \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -I"$testdir" -I"$source_root/lib" "$root/tests/jni_field_regression.c" "$source_root/source/java.c" \
    "$testdir/falso_jni/FalsoJNI_ImplBridge.c" "$testdir/falso_jni/converter.c" \
    -Wl,--gc-sections -o "$testdir/jni-O$optimization"
  "$testdir/jni-O$optimization"
done
