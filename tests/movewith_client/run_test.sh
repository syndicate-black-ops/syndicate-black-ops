#!/usr/bin/env bash
# Offline test of the movewith client composition (src/cl_dll/movewith_client.cpp, docs/MOVEWITH.md) with
# ASan/UBSan on a stub engine (tests/movewith_client/stub/): drawing, prediction physents, event traces, and the
# studio renderer's step fraction (src/cl_dll/studio_interp.h). The real
# source file is compiled from a copy so its "hud.h" / "cl_util.h" resolve to the stubs. Needs g++.
#   tests/movewith_client/run_test.sh [path/to/movewith_client.cpp]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=${1:-$ROOT/src/cl_dll/movewith_client.cpp}
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cp "$SRC" "$T/movewith_client.cpp"
cp "$ROOT/src/cl_dll/movewith_client.h" "$T/"
cp "$ROOT/src/cl_dll/studio_interp.h" "$ROOT/src/cl_dll/studio_attachments.h" "$T/"
cp "$ROOT/tests/movewith_client/stub/"*.h "$T/"
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -DSBO_TEST_COMMANDS -I"$T" -I"$ROOT/src/common" -I"$ROOT/src/pm_shared" -I"$ROOT/src/game_shared" \
  -I"$ROOT/src/engine" -o "$T/movewith_client_test" \
  "$ROOT/tests/movewith_client/movewith_client_test.cpp" "$T/movewith_client.cpp"
"$T/movewith_client_test"
