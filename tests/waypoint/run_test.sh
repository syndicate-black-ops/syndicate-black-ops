#!/usr/bin/env bash
# Offline test of the info_waypoint client core (src/cl_dll/waypoint_core.cpp: the "Waypoint" message, the marker's
# place on the screen and on its edge, the distance text; docs/INFO_WAYPOINT.md) with ASan/UBSan. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/waypoint_test" \
  "$ROOT/tests/waypoint/waypoint_test.cpp" "$ROOT/src/cl_dll/waypoint_core.cpp"
"$T/waypoint_test"
