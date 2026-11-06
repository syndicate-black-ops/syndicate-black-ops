#!/usr/bin/env bash
# Offline test of the console color text splitter (src/cl_dll/vgui2/sbo_console_text.h, docs/CONSOLE_COLOR.md) with
# ASan/UBSan: runs between color codes, UTF-8-safe console pieces, random and hostile input. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/vgui2" -o "$T/text_test" "$ROOT/tests/console_color/text_test.cpp"
"$T/text_test"
