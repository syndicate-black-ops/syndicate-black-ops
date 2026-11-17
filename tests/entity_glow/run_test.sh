#!/usr/bin/env bash
# Offline test of env_entity_glow's brush model reader (src/cl_dll/entity_glow_bsp.cpp, docs/ENV_ENTITY_GLOW.md) with
# ASan/UBSan: a small BSP 30 file built in memory (two brush models), every truncation of it, hostile counts, offsets
# and indexes, and random byte changes. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -o "$T/bsp_test" "$ROOT/tests/entity_glow/bsp_test.cpp" "$ROOT/src/cl_dll/entity_glow_bsp.cpp"
"$T/bsp_test"
