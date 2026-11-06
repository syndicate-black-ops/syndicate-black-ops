#!/usr/bin/env bash
# Offline test of the HTML MOTD checks (src/cl_dll/vgui2/sbo_html.h, docs/HTML_MOTD.md) with ASan/UBSan: which
# MOTD texts are a URL, HTML or plain text, the URL copy, and random / truncated input. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/vgui2" -o "$T/html_test" "$ROOT/tests/html_motd/html_test.cpp"
"$T/html_test"
