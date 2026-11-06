#!/usr/bin/env bash
# point_worldtext test (docs/POINT_WORLDTEXT.md): builds the client's message parser, font metrics reader, text
# decoder and layout (src/cl_dll/worldtext_core.cpp) alone, without the SDK, with AddressSanitizer and
# UndefinedBehaviorSanitizer, and feeds it the shipped fonts' metrics, the server's messages and hostile input
# (tests/worldtext/worldtext_test.cpp). Then checks the shipped font sprites (scripts/gen_worldtext_fonts.py verify).
# Needs g++ and python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# no ulimit -v: AddressSanitizer reserves terabytes of address space (it uses a few hundred MB)
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll" -I"$ROOT/src/game_shared" -o "$T/worldtext_test" \
  "$ROOT/tests/worldtext/worldtext_test.cpp" "$ROOT/src/cl_dll/worldtext_core.cpp"
timeout 300 "$T/worldtext_test" "$ROOT/gamedir/syndicate/sprites/worldtext/sans.txt" \
  "$ROOT/gamedir/syndicate/sprites/worldtext/mono.txt"
python3 "$ROOT/scripts/gen_worldtext_fonts.py" verify
