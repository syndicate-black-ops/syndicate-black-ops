#!/usr/bin/env bash
# Material registry test (src/pm_shared/pm_materials.c, docs/MATERIALS.md): builds the registry alone with
# tests/materials/materials_test.c as its host, with AddressSanitizer and UBSan, and runs
#   - unit tests (HL's built-in sounds, texture lists, whole names and HL's 12 characters, types files, refused
#     files, BSP texture lumps, sound paths, map files with inline blocks, who wins),
#   - the game's own materials/textures.txt and materials/types.txt (no message allowed),
#   - a fuzz run: mutated copies of those files and of a BSP texture lump into every parser (FUZZ_TIME seconds,
#     default 20).
# Needs gcc.
#
# The 1.38 check (what sounds different from 1.38, and why): for every texture of the given maps, HL SDK's lookup
# of 1.38 on Half-Life's materials.txt against the registry on SBO's files. A texture that sounds different must be
# decided by SBO's data (a tag of materials/textures.txt or of the map's file, or W's steps), else the check fails:
#   tests/materials/run_test.sh --maps <half-life materials.txt> <bsp or folder>...
#   e.g. tests/materials/run_test.sh --maps ../sbo-1.38/materials.txt \
#          gamedir/syndicate/maps ../sbo-maps
# Precache count per map (the sounds, the files added to the download list, the letters used, the map, the files):
#   tests/materials/run_test.sh --precache gamedir/syndicate gamedir/syndicate/maps
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
gcc -std=gnu99 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -fno-sanitize-recover=undefined -I"$ROOT/src/pm_shared" -o "$T/materials_test" \
  "$ROOT/tests/materials/materials_test.c" "$ROOT/src/pm_shared/pm_materials.c"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:halt_on_error=1
if [ "${1:-}" = "--precache" ]; then
  # sounds precached per map (TEXTURETYPE_PrecacheLevel): tests/materials/run_test.sh --precache <gamedir> <bsp or folder>...
  GD=$2; shift 2
  mapfile -t BSPS < <(for a in "$@"; do if [ -d "$a" ]; then find "$a" -iname '*.bsp' | sort; else echo "$a"; fi; done)
  exec "$T/materials_test" --precache "$GD" "${BSPS[@]}"
fi
if [ "${1:-}" = "--maps" ]; then
  HLFILE=$2; shift 2
  mapfile -t BSPS < <(for a in "$@"; do if [ -d "$a" ]; then find "$a" -iname '*.bsp' | sort; else echo "$a"; fi; done)
  exec "$T/materials_test" --maps "$HLFILE" "$ROOT/gamedir/syndicate" "${BSPS[@]}"
fi
timeout $(( ${FUZZ_TIME:-20} + 60 )) "$T/materials_test" "$ROOT/gamedir/syndicate" --fuzz "${FUZZ_TIME:-20}"
