#!/usr/bin/env bash
# Offline test of the inventory client core (src/cl_dll/inventory_core.cpp: the "Inventory" message and the two
# item lists; docs/INVENTORY.md) with ASan/UBSan: good, cut and hostile messages and a random fuzz. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/inventory_test" \
  "$ROOT/tests/inventory/inventory_test.cpp" "$ROOT/src/cl_dll/inventory_core.cpp"
"$T/inventory_test"
