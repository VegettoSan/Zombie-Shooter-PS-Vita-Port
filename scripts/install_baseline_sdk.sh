#!/usr/bin/env bash
# Install only into an absent SDK path. Never overwrite a working SDK.
set -euo pipefail
sdk=/usr/local/vitasdk
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ -e "$sdk" ]]; then
  echo "$sdk already exists; verifying without modifying it"
  VITASDK="$sdk" python3 "$root/scripts/prepare_build.py"
  exit
fi
archive=$(mktemp --suffix=.tar.gz)
trap 'rm -f -- "$archive"' EXIT
curl --fail --location --retry 3 \
  https://github.com/VegettoSan/Zombie-Shooter-PS-Vita-Port/releases/download/sdk-functional-local-20260925/vitasdk-softfp-local-20260925.tar.gz \
  -o "$archive"
echo "25e1271b50aef4e0b4e7be837b6c2316507c47a0f56127a5b2d633f6a6aa0878  $archive" | sha256sum --check
if [[ -w /usr/local ]]; then
  tar --no-same-owner -xzf "$archive" -C /usr/local
else
  sudo tar --no-same-owner -xzf "$archive" -C /usr/local
fi
VITASDK="$sdk" python3 "$root/scripts/prepare_build.py"
