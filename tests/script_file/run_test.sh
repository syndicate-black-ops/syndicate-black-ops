#!/usr/bin/env bash
# Offline test of the option script reader / writer (src/cl_dll/vgui2/sbo_script_file.cpp: settings.scr and user.scr,
# used by SBO's Options and Create server windows) with ASan/UBSan. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I"$ROOT/src/cl_dll/vgui2" \
  -o "$T/script_test" "$ROOT/tests/script_file/script_test.cpp" "$ROOT/src/cl_dll/vgui2/sbo_script_file.cpp"
"$T/script_test" "$ROOT/gamedir/syndicate/settings.scr" "$ROOT/gamedir/syndicate/user.scr"
