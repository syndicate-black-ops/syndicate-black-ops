#!/usr/bin/env bash
# Offline tests of the server browser's parsers (docs/SERVER_BROWSER.md) with ASan/UBSan. Needs g++.
#   address_test: src/cl_dll/vgui2/sbo_server_address.cpp (the Add server dialog): unit cases and random / hostile
#                 text. Name lookups are not run (no network): only Split and ParseIPv4, and Resolve on IPv4 text.
#   a2s_test:     src/cl_dll/vgui2/sbo_a2s_rules.cpp (the rules query): GoldSrc and Source split replies out of order,
#                 cut and damaged packets, random data.
#   image_test:     src/cl_dll/vgui2/sbo_image_decode.cpp (map pictures, overviews): every gamedir/syndicate/overviews/*.tga
#                 and gfx/mapimages/*.jpg, a progressive copy of one JPEG (the CDN sends progressive JPEGs; made with
#                 ImageMagick when it is installed), then each cut short and with random bytes changed.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
FLAGS=(-std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I"$ROOT/src/cl_dll/vgui2")
g++ "${FLAGS[@]}" -o "$T/address_test" "$ROOT/tests/server_browser/address_test.cpp" \
  "$ROOT/src/cl_dll/vgui2/sbo_server_address.cpp"
g++ "${FLAGS[@]}" -o "$T/a2s_test" "$ROOT/tests/server_browser/a2s_test.cpp" "$ROOT/src/cl_dll/vgui2/sbo_a2s_rules.cpp"
g++ "${FLAGS[@]}" -o "$T/image_test" "$ROOT/tests/server_browser/image_test.cpp" "$ROOT/src/cl_dll/vgui2/sbo_image_decode.cpp"
"$T/address_test"
"$T/a2s_test"
PROG=()
first=$(ls "$ROOT"/gamedir/syndicate/gfx/mapimages/*.jpg | head -1)
if command -v convert >/dev/null && convert "$first" -interlace Plane "$T/progressive.jpg" 2>/dev/null; then PROG=("$T/progressive.jpg"); fi
"$T/image_test" "$ROOT"/gamedir/syndicate/overviews/*.tga "$ROOT"/gamedir/syndicate/gfx/mapimages/*.jpg "${PROG[@]}"
