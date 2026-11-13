#!/usr/bin/env bash
# Offline test of the client info answer parsing (src/dlls/client_info.cpp, docs/dev/CLIENT_INFO.md) with
# ASan/UBSan: the answers of each engine and of cl_sbo_os, missing answers, the userinfo version, random / hostile answers. Needs g++.
#   tests/client_info/run_test.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -g -O1 -fsanitize=address,undefined \
  -fno-sanitize-recover=all -I"$ROOT/src/dlls" -I"$ROOT/src/game_shared" -o "$T/parser_test" "$ROOT/tests/client_info/parser_test.cpp"
"$T/parser_test"
