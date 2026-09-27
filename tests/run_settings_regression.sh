#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_variant() {
  local name="$1"
  shift
  local data="$tmp/$name"
  mkdir -p "$data"
  cc -std=gnu11 -O2 -Isource \
    "-DDATA_PATH=\"$data/\"" "$@" \
    source/utils/settings.c tests/settings_regression.c \
    -o "$tmp/settings-$name"
  "$tmp/settings-$name"
}

run_variant debug
run_variant release -DZOMBIE_RELEASE_BUILD=1
