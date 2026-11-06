#!/usr/bin/env bash
# Parser test for the overview files (src/cl_dll/hud_spectator.cpp, docs/SPECTATOR.md "Multi-layer overviews"):
# cuts ParseOverviewFile, LoadMapSprites and IsLayerBelowView out of the client's source, builds them alone with
# tests/overview/parser_test.cpp as their host (the engine's file functions, exactly sized buffers) and runs them
# under AddressSanitizer on every gamedir/syndicate/overviews/*.txt and on tests/overview/cases/ (several layers,
# layers out of order, nine layers, a missing image, over-long names, files that end early). Needs g++.
#
# More game folders can be given (each with an overviews/ folder); every overview file in them must parse cleanly:
#   tests/overview/run_test.sh ../overview-test/syndicate
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
SRC="$ROOT/src/cl_dll"

# the sources are Latin-1 with CRLF
iconv -f latin1 -t utf8 "$SRC/hud_spectator.h" | tr -d '\r' > "$T/h"
iconv -f latin1 -t utf8 "$SRC/hud_spectator.cpp" | tr -d '\r' > "$T/cpp"
{ grep -E '^#define[[:space:]]+OVERVIEW_' "$T/h"
  awk '/^typedef struct overviewInfo_s/{p=1} p{print} /^} overviewInfo_t;/{p=0}' "$T/h"; } > "$T/overview_defs.inc"
for f in 'bool CHudSpectator::ParseOverviewFile' 'void CHudSpectator::LoadMapSprites' 'bool CHudSpectator::IsLayerBelowView'; do
  awk -v f="$f" 'index($0,f)==1{p=1} p{print} p&&/^}/{p=0}' "$T/cpp"
done > "$T/overview_code.inc"
[ "$(grep -c '^}' "$T/overview_code.inc")" = 3 ] || { echo "could not cut the three functions out of hud_spectator.cpp" >&2; exit 1; }

g++ -std=c++17 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-overflow -g -fsanitize=address,undefined \
  -fno-sanitize-recover=all -I"$T" -o "$T/parser_test" "$ROOT/tests/overview/parser_test.cpp"

# a file with a syntax error keeps its buffer, as in the SDK: leaks are not what this test is for
export ASAN_OPTIONS="detect_leaks=0:hard_rss_limit_mb=2000${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
fail=0
run() { timeout 60 "$T/parser_test" "$@" || { echo "FAIL $2 (exit $?)"; fail=1; }; }

for dir in "$ROOT/gamedir/syndicate" "$@"; do
  for txt in "$dir"/overviews/*.txt; do
    run "$dir" "$(basename "$txt" .txt)"
  done
done
# cases: <map> <layers that must be left>
C="$ROOT/tests/overview/cases"
while read -r map layers; do
  run "$C" "$map" "$layers"
done < "$C/expect.txt"
run "$C" no_such_map 0

[ $fail = 0 ] && echo "overview parser test: all passed" || { echo "overview parser test: FAILED"; exit 1; }
