#!/usr/bin/env bash
# Offline test of the map compatibility file parsers (src/dlls/map_compat.cpp, docs/MAP_COMPAT.md), with
# ASan/UBSan: unit cases, the game's maps/map_compat.txt against the .bsp files given (their CRC-32 must match their
# entries), and random / damaged rules files, BSP headers and entity lumps. Needs g++.
#   tests/map_compat/run_test.sh [map.bsp ...]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/dlls" -o "$T/parser_test" "$ROOT/tests/map_compat/parser_test.cpp"
"$T/parser_test" "$ROOT/gamedir/syndicate/maps/map_compat.txt" "$@"
