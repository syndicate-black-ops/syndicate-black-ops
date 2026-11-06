#!/usr/bin/env bash
# Offline test of the Locus value parsing (src/dlls/locus.cpp, docs/LOCUS.md) with ASan/UBSan: the parser built
# alone on stub headers (tests/locus/stubs; the file is copied so its own directory's SDK headers are not used).
# Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
cp "$ROOT/src/dlls/locus.cpp" "$ROOT/src/dlls/locus.h" "$T/"
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -g -O1 -fsanitize=address,undefined \
  -fno-sanitize-recover=all -DLOCUS_FUZZ -I"$ROOT/tests/locus/stubs" -I"$T" -o "$T/locus_test" "$ROOT/tests/locus/locus_test.cpp"
"$T/locus_test"
