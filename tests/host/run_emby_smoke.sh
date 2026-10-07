#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/emby5-client-smoke"
clang++ -std=c++17 -O2 \
  -I"${ROOT}/app/src" -I"${ROOT}/app/engine/addons/include" \
  "${ROOT}/tests/host/emby_client_smoke.cpp" \
  "${ROOT}/app/src/jf/jf_client.cpp" \
  "${ROOT}/app/engine/addons/src/cJSON.c" \
  -o "${OUT}"
"${OUT}"
rm -f "${OUT}"
