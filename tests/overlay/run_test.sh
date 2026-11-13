#!/usr/bin/env bash
# Offline test of the env_overlay client core (src/cl_dll/overlay_core.cpp: the "Overlay" message, the fades and the
# placement; docs/ENV_OVERLAY.md) with ASan/UBSan. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/overlay_test" \
  "$ROOT/tests/overlay/overlay_test.cpp" "$ROOT/src/cl_dll/overlay_core.cpp"
"$T/overlay_test"
