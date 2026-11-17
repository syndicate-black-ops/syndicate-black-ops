#!/usr/bin/env bash
# Offline test of the Aurora runtime (src/cl_dll/aurora/particlesys.cpp, particlemgr.cpp) with ASan/UBSan, built
# with the particle editor's engine shim (tools/aurora-editor/core): the top-level "attachment" and "lifetime" of
# XashXT / PrimeXT scripts as defaults (docs/AURORA.md "XashXT and PrimeXT scripts"). Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CORE="$ROOT/tools/aurora-editor/core"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
sh "$CORE/build.sh" stage "$ROOT" "$T/stage"
g++ -std=c++14 -g -O1 -Wall -Wno-unused-function -Wno-class-memaccess -Wno-sign-compare \
  -fsanitize=address,undefined -fno-sanitize-recover=all -I"$CORE/include" -I"$T/stage" \
  "$T"/stage/particlesys.cpp "$T"/stage/particlemgr.cpp "$T"/stage/parsemsg.cpp "$CORE/shim.cpp" \
  "$ROOT/tests/aurora/script_keys_test.cpp" -o "$T/script_keys_test" -lm
"$T/script_keys_test"
