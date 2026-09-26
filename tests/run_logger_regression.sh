#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/psp2/kernel" "$tmp/psp2/io"
cat > "$tmp/psp2/kernel/threadmgr.h" <<'H'
typedef int SceKernelLwMutexWork;
H
cat > "$tmp/psp2/io/fcntl.h" <<'H'
typedef int SceUID;
#define SCE_O_WRONLY 1
#define SCE_O_CREAT 2
#define SCE_O_EXCL 4
#define SCE_O_APPEND 8
H
printf 'typedef int SceIoStat;\n' > "$tmp/psp2/io/stat.h"
touch "$tmp/psp2/kernel/clib.h" "$tmp/psp2/kernel/processmgr.h"
for opt in 0 3; do
  cc -std=gnu11 -O"$opt" -Isource -I"$tmp" -DZOMBIE_RELEASE_BUILD=1 \
    '-DZOMBIE_BUILD_VARIANT="Release"' '-DZOMBIE_BUILD_ID="test"' \
    tests/logger_regression.c -o "$tmp/logger-$opt"
  "$tmp/logger-$opt"
done
