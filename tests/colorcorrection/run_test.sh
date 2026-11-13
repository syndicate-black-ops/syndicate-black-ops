#!/usr/bin/env bash
# Offline test of the color correction client core (src/cl_dll/colorcorrection_core.cpp: the "ColorCorr" message, the
# .raw / .cube readers, the simple keys, weights, fades and the blend; docs/COLOR_CORRECTION.md) with ASan/UBSan, on
# good, cut and hostile input plus a seeded random fuzz pass, and the editor's operations
# (src/cl_dll/colorcorrection_ops.cpp: every operation, the .raw it writes, the curve math, look names, random
# settings). Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/colorcorrection_test" \
  "$ROOT/tests/colorcorrection/colorcorrection_test.cpp" "$ROOT/src/cl_dll/colorcorrection_core.cpp"
"$T/colorcorrection_test" "${@}"
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/colorcorrection_ops_test" \
  "$ROOT/tests/colorcorrection/colorcorrection_ops_test.cpp" "$ROOT/src/cl_dll/colorcorrection_ops.cpp" \
  "$ROOT/src/cl_dll/colorcorrection_core.cpp"
"$T/colorcorrection_ops_test" "${@}"
