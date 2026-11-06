#!/usr/bin/env bash
# Parser test for the sequence files (src/dlls/sequence.cpp, docs/SEQUENCES.md): builds the parser alone, without
# the SDK, with tests/sequences/parser_test.cpp as its host, and checks what tests/sequences/sequences/*.seq and a
# set of faulty files become. Needs g++.
#
# Also checked: the game's own gamedir/syndicate/sequences/global.seq and the SDK's example
# (sdk/examples/sequences/co_example.seq), and how "repeat" runs.
#
# More files can be given, each as <gamedir>:<name> (reads <gamedir>/sequences/global.seq, or the game's own if
# there is none, and <name>.seq, which must load without an error; the entries are printed): a map's file, the
# test asset of docs/SEQUENCES.md, another game's files.
#   tests/sequences/run_test.sh ../sequence-test:co_appart
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -fsanitize=address,undefined -DSEQUENCE_STANDALONE \
  -I"$ROOT/src/dlls" -I"$ROOT/src/common" -I"$ROOT/src/game_shared" -o "$T/parser_test" \
  "$ROOT/tests/sequences/parser_test.cpp" "$ROOT/src/dlls/sequence.cpp"
"$T/parser_test" "$ROOT/tests/sequences" "$@"
