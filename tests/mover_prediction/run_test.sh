#!/usr/bin/env bash
# Offline test of mover prediction with movewith children (src/cl_dll/mover_prediction.cpp with
# src/cl_dll/movewith_client.cpp, docs/MOVER_PREDICTION.md "Merging with movewith") with ASan/UBSan on a stub engine
# (tests/mover_prediction/stub/): a player riding a composed movewith child of a predicted parent. The real source
# files are compiled from copies so "hud.h" / "cl_util.h" / "pm_shared.h" resolve to the stubs. Needs g++.
#   tests/mover_prediction/run_test.sh [mover_prediction.cpp [mover_prediction.h]]
# MP_OLD_API=1: the source has the single MoverPrediction_PreMove (before the movewith merge), called before
# MoveWith_PlayerMove as it was.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=${1:-$ROOT/src/cl_dll/mover_prediction.cpp}
HDR=${2:-$ROOT/src/cl_dll/mover_prediction.h}
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cp "$SRC" "$T/mover_prediction.cpp"
cp "$HDR" "$T/mover_prediction.h"
cp "$ROOT/src/cl_dll/movewith_client.cpp" "$ROOT/src/cl_dll/movewith_client.h" "$T/"
cp "$ROOT/tests/mover_prediction/stub/"*.h "$T/"
cp "$ROOT/src/cl_dll/util_vector.h" "$ROOT/src/cl_dll/studio_attachments.h" "$T/"
DEF=""
[ "${MP_OLD_API:-0}" = 1 ] && DEF="-DMP_OLD_API"
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-class-memaccess -g -O1 -fsanitize=address,undefined \
  -fno-sanitize-recover=all $DEF -I"$T" -I"$ROOT/src/common" -I"$ROOT/src/pm_shared" -I"$ROOT/src/game_shared" \
  -I"$ROOT/src/engine" -o "$T/mover_prediction_test" \
  "$ROOT/tests/mover_prediction/mover_prediction_test.cpp" "$T/mover_prediction.cpp" "$T/movewith_client.cpp"
"$T/mover_prediction_test"
