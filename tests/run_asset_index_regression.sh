#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for opt in 0 3; do
  cc -std=gnu11 -O"$opt" -Wall -Wextra -Werror -pthread -Isource tests/asset_index_regression.c -o "$tmp/asset-$opt"
  "$tmp/asset-$opt"
done
