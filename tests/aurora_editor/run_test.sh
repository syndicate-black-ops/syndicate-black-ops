#!/usr/bin/env bash
# Offline test of the particle editor's script document (src/cl_dll/aurora/aurora_doc.cpp) with ASan/UBSan, on every
# shipped script (gamedir/syndicate/aurora/*.aur), hand-made cases and random text; and the key table
# (src/cl_dll/aurora/aurora_keys.h) is current with tools/aurora-editor/core/aurora-keys.json. Needs g++, python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
python3 "$ROOT/scripts/gen_aurora_keys.py" --check
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/aurora" -o "$T/aurora_doc_test" \
  "$ROOT/tests/aurora_editor/aurora_doc_test.cpp" "$ROOT/src/cl_dll/aurora/aurora_doc.cpp"
"$T/aurora_doc_test" "$ROOT"/gamedir/syndicate/aurora/*.aur
# the sprite list's thumbnails (src/cl_dll/vgui2/sbo_image_decode.cpp DecodeSprScaled) on every shipped sprite
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/vgui2" -o "$T/spr_thumb_test" \
  "$ROOT/tests/aurora_editor/spr_thumb_test.cpp" "$ROOT/src/cl_dll/vgui2/sbo_image_decode.cpp"
mapfile -t SPRITES < <(find "$ROOT/gamedir/syndicate/sprites" -name '*.spr' | sort)
"$T/spr_thumb_test" "${SPRITES[@]}"
