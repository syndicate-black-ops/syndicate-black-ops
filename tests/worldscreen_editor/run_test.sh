#!/usr/bin/env bash
# Offline test of the world screen editor's layout document (src/cl_dll/worldscreen/worldscreen_layout_doc.cpp) with
# ASan/UBSan, on every shipped layout (gamedir/syndicate/ui/scripts/screens/*.res), hand-made cases and random text.
# Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/worldscreen" -o "$T/layout_doc_test" \
  "$ROOT/tests/worldscreen_editor/layout_doc_test.cpp" "$ROOT/src/cl_dll/worldscreen/worldscreen_layout_doc.cpp"
"$T/layout_doc_test" "$ROOT"/gamedir/syndicate/ui/scripts/screens/*.res
