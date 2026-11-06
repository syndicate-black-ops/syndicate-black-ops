#!/usr/bin/env bash
# Offline test of the version gate's version parser (src/dlls/version_gate.cpp, docs/VERSION_GATE.md) with
# ASan/UBSan: unit cases and random / hostile userinfo values. Needs g++.
#   tests/version_gate/run_test.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -g -O1 -fsanitize=address,undefined \
  -fno-sanitize-recover=all -I"$ROOT/src/dlls" -o "$T/parser_test" "$ROOT/tests/version_gate/parser_test.cpp"
"$T/parser_test"
