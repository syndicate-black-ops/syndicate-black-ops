#!/usr/bin/env bash
# Server fuzz harness: an AddressSanitizer / UBSan hl.so (scripts/check_sanitizers.sh asan) on a stub engine
# (stub_engine.cpp), fed hostile client commands, chat, names, impulses, disconnects, and every entity class with
# hostile key values (fuzz_server.cpp). Runs in the sniper image (32-bit libasan).
#
#   tests/fuzz/server/run_test.sh [hl.so] [-- fuzz_server options]
#     hl.so  default build/sanitize/asan/hl.so
#     FUZZ_TIME  seconds (default 120), split between client and entity runs (--mode all)
#
# The command, key and class lists are read from the source each time, so new commands and keys are fuzzed without
# touching the harness. values.txt / map_keys.txt: values and keys of the SBO maps (a scan of the maps).
# First the fixed reproducers, cases/*.txt (fuzz_server.cpp "cases"; FUZZ_CASES_ONLY=1: only those), then FUZZ_TIME of
# fuzzing. Exit status 1 on an AddressSanitizer / UBSan error, a hang, or an engine-error condition (stub_engine.h).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
H=$ROOT/tests/fuzz/server
HL=$ROOT/build/sanitize/asan/hl.so
if [ $# -gt 0 ] && [ "$1" != "--" ]; then HL=$1; shift; fi
[ "${1:-}" = "--" ] && shift
[ -f "$HL" ] || { echo "no $HL: run scripts/check_sanitizers.sh asan first" >&2; exit 2; }
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}
OUT=$ROOT/build/sanitize/fuzz/server; mkdir -p "$OUT"

# the commands the server reads (ClientCommand, the game rules, voice, message boxes), the keys KeyValue reads, the
# entity classes: from the source
cat "$ROOT"/src/dlls/*.cpp "$ROOT"/src/game_shared/*.cpp | tr -d '\r' \
  | grep -ohE '(FStrEq|strcmp|stricmp|strcasecmp|_stricmp)\( *(pcmd|cmd|pszCommand) *, *"[^"]+"' \
  | sed -E 's/.*"([^"]+)"$/\1/' > "$OUT/commands.raw"
{ sort -u "$OUT/commands.raw"; echo use; echo lastinv; echo menuselect;
  cat "$ROOT"/src/dlls/*.cpp | tr -d '\r' | grep -ohE 'LINK_ENTITY_TO_CLASS\( *weapon_[a-z0-9_]+' | sed -E 's/.*\( *//'; } \
  | sort -u > "$OUT/commands.txt"
{ cat "$ROOT"/src/dlls/*.cpp | tr -d '\r' | grep -ohE 'FStrEq\( *pkvd->szKeyName *, *"[^"]+"' | sed -E 's/.*"([^"]+)"$/\1/';
  printf '%s\n' classname origin angles targetname target model rendermode renderamt rendercolor renderfx spawnflags \
    health speed dmg message netname killtarget globalname skin body scale frags team delay wait sounds style;
  grep -v '^#' "$H/map_keys.txt"; } | sort -u > "$OUT/keys.txt"
cat "$ROOT"/src/dlls/*.cpp | tr -d '\r' | grep -ohE 'LINK_ENTITY_TO_CLASS\( *[A-Za-z0-9_]+' | sed -E 's/.*\( *//' \
  | sort -u > "$OUT/classes.txt"
echo "commands $(wc -l < "$OUT/commands.txt"), keys $(wc -l < "$OUT/keys.txt"), classes $(wc -l < "$OUT/classes.txt")"

SDK_MOUNT=()
[ -L "$ROOT/external/hl1_source_sdk" ] && SDK_MOUNT=(-v "$(readlink -f "$ROOT/external/hl1_source_sdk"):$(readlink -f "$ROOT/external/hl1_source_sdk"):ro")
HL_DIR=$(cd "$(dirname "$HL")" && pwd)
nice -n 15 docker run --rm --memory="${DOCKER_MEMORY:-4g}" --cpus="${DOCKER_CPUS:-2}" -u "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$ROOT:$ROOT" -v "$HL_DIR:$HL_DIR:ro" "${SDK_MOUNT[@]}" -w "$ROOT" \
  -e FUZZ_CASES_ONLY -e ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:halt_on_error=1:handle_abort=1:allocator_may_return_null=1:detect_odr_violation=0 \
  -e UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:report_error_type=1:suppressions=$ROOT/tests/fuzz/ubsan.supp \
  "$SNIPER_IMAGE" bash -c "
    set -e
    g++ -m32 -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fpermissive -w \
      -Dstricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -DVALVE_DLL -DCLIENT_WEAPONS \
      -I src/dlls -I src/engine -I src/common -I src/pm_shared -I src/game_shared \
      -o '$OUT/fuzz_server' '$H/fuzz_server.cpp' '$H/stub_engine.cpp' -ldl
    # the fixed reproducers first (cases/*.txt: every bug the fuzzing found, as a regression test)
    set +e
    '$OUT/fuzz_server' '$HL' '$ROOT/gamedir/syndicate' '$OUT/commands.txt' '$OUT/keys.txt' '$OUT/classes.txt' \
      '$H/values.txt' --mode cases '$H'/cases/*.txt > '$OUT/cases.log' 2>&1
    cases=\$?
    grep -E '^(case |ENGINE:|\*\*\*|SUMMARY|.*runtime error|fuzz_server)' '$OUT/cases.log'
    [ \"\${FUZZ_CASES_ONLY:-}\" = 1 ] && exit \$cases
    '$OUT/fuzz_server' '$HL' '$ROOT/gamedir/syndicate' '$OUT/commands.txt' '$OUT/keys.txt' '$OUT/classes.txt' \
      '$H/values.txt' --seconds '${FUZZ_TIME:-120}' $* 2>&1 | tee '$OUT/run.log' | grep -E '^(ENGINE:|\*\*\*|---|fuzz_server|SUMMARY|.*runtime error)' | sort | uniq -c | sort -rn | head -200
    fuzz=\${PIPESTATUS[0]}
    [ \$cases -eq 0 ] && [ \$fuzz -eq 0 ]
  "
