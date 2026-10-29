#!/usr/bin/env bash
set -euo pipefail
cd /home/vegettosandev/Zombie-Shooter-PS-Vita-Port
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build-fps-20261028-release -DCMAKE_BUILD_TYPE=Release > artifacts/fps-2026-10-28/configure-release.log 2>&1
cmake --build build-fps-20261028-release -j8 > artifacts/fps-2026-10-28/build-release.log 2>&1
