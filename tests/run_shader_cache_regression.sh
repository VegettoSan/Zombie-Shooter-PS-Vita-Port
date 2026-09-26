#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
for optimization in 0 3; do
  gcc -std=gnu11 -Wall -Wextra -Werror -O"$optimization" -I"$root/lib/vitagl/source" "$root/tests/shader_cache_regression.c" -o "$work/cache"
  "$work/cache"
done
