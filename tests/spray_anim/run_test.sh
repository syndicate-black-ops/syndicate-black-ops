#!/usr/bin/env bash
# Animated spray test (docs/ANIMATED_SPRAYS.md): makes test GIFs and converts them with scripts/gif_to_spray.py
# (tests/spray_anim/make_case.py), checks the sprays with the converter's --check, then builds the client's reader
# (src/cl_dll/spray_anim_core.cpp) alone with AddressSanitizer and UndefinedBehaviorSanitizer and feeds it the sprays
# and hostile input (tests/spray_anim/spray_anim_test.cpp). Needs g++ and python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
timeout 300 python3 -I "$ROOT/tests/spray_anim/make_case.py" "$T"
for f in "$T"/test.wad "$T"/still.wad "$T"/big.wad; do python3 -I "$ROOT/scripts/gif_to_spray.py" --check "$f"; done
# no ulimit -v: AddressSanitizer reserves terabytes of address space (it uses a few hundred MB)
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -o "$T/spray_anim_test" \
  "$ROOT/tests/spray_anim/spray_anim_test.cpp" "$ROOT/src/cl_dll/spray_anim_core.cpp"
timeout 300 "$T/spray_anim_test" "$T"
