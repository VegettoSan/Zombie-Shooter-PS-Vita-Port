#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for opt in 0 3; do
  cc -std=gnu11 -O"$opt" -Isource tests/gl_buffer_regression.c -o "$tmp/gl-$opt"
  "$tmp/gl-$opt"
done
python3 - <<'PY'
from pathlib import Path
s=Path('source/dynlib.c').read_text()
assert '"glBindBuffer", (uintptr_t)&glBindBuffer_soloader' in s
assert '"eglGetProcAddress", (uintptr_t)&eglGetProcAddress_soloader' in s
bridge=Path('source/utils/gl_buffers.inc').read_text()
draw=bridge.split('void glDrawElements_soloader')[1]
query=bridge.split('void glGetIntegerv_soloader')[1].split('void glDrawElements_soloader')[0]
assert 'glGetIntegerv(' not in draw and 'lock_guest_buffers(' not in draw
assert 'for (' not in query
assert '__sync_fetch_and_add' not in bridge
print('GL static checks passed: import/resolver bridges, no per-draw query/lock or linear binding search')
PY
