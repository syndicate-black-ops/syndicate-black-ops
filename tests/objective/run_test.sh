#!/usr/bin/env bash
# Offline test of the trigger_objective client core (src/cl_dll/objective_core.cpp: the "Objective" message, the list,
# the HUD rows, fades and notices; docs/OBJECTIVES.md) with ASan/UBSan: good, cut, hostile and random messages.
# Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/objective_test" \
  "$ROOT/tests/objective/objective_test.cpp" "$ROOT/src/cl_dll/objective_core.cpp"
"$T/objective_test"
