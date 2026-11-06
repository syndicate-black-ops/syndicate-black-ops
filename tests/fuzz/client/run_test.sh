#!/usr/bin/env bash
# Client fuzz harness: an AddressSanitizer / UBSan client.so (scripts/check_sanitizers.sh asan) on a stub client engine
# (fuzz_client.cpp), every hooked user message fed truncated, full-size and hostile payloads, HUD_Redraw in between.
# Runs in the sniper image (32-bit libasan).
#
#   tests/fuzz/client/run_test.sh [client.so] [-- fuzz_client options]
#     client.so  default build/sanitize/asan/client.so;  FUZZ_TIME seconds (default 120)
# HL_LIB_DIR: a folder with the engine's 32-bit libsteam_api.so (client.so links it), e.g. a Half-Life or HLDS install.
# SKIP: messages not fuzzed (handlers that need VGUI2 panels, which the stub doesn't start), comma-separated.
# Exit status 1 on an AddressSanitizer / UBSan error or a hang.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
H=$ROOT/tests/fuzz/client
SO=$ROOT/build/sanitize/asan/client.so
if [ $# -gt 0 ] && [ "$1" != "--" ]; then SO=$1; shift; fi
[ "${1:-}" = "--" ] && shift
[ -f "$SO" ] || { echo "no $SO: run scripts/check_sanitizers.sh asan first" >&2; exit 2; }
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}
OUT=$ROOT/build/sanitize/fuzz/client; mkdir -p "$OUT"
SKIP=${SKIP:-}
HL_LIB_DIR=${HL_LIB_DIR:?set HL_LIB_DIR to a folder with the 32-bit libsteam_api.so (Half-Life or HLDS install)}
HL_LIB_DIR=$(cd "$HL_LIB_DIR" && pwd)
SDK_MOUNT=()
[ -L "$ROOT/external/hl1_source_sdk" ] && SDK_MOUNT=(-v "$(readlink -f "$ROOT/external/hl1_source_sdk"):$(readlink -f "$ROOT/external/hl1_source_sdk"):ro")
SO_DIR=$(cd "$(dirname "$SO")" && pwd)
nice -n 15 docker run --rm --memory="${DOCKER_MEMORY:-4g}" --cpus="${DOCKER_CPUS:-2}" -u "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$ROOT:$ROOT" -v "$SO_DIR:$SO_DIR:ro" -v "$HL_LIB_DIR:$HL_LIB_DIR:ro" "${SDK_MOUNT[@]}" -w "$ROOT" \
  -e LD_LIBRARY_PATH="$HL_LIB_DIR" \
  -e ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:handle_abort=1:allocator_may_return_null=1:detect_odr_violation=0 \
  -e UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:report_error_type=1:suppressions=$ROOT/tests/fuzz/ubsan.supp \
  "$SNIPER_IMAGE" bash -c "
    set -e
    g++ -m32 -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fpermissive -w \
      -include cmake/compat/sbo_compat.h -DCLIENT_DLL -DLINUX -D_LINUX -DPOSIX -D_POSIX -DGNUC -DDISABLE_VEC_ORIGIN \
      -Dstricmp=strcasecmp -D_stricmp=strcasecmp -Dstrnicmp=strncasecmp -D_strnicmp=strncasecmp \
      -I src/cl_dll -I src/engine -I src/common -I src/dlls -I src/pm_shared -I src/game_shared \
      -o '$OUT/fuzz_client' '$H/fuzz_client.cpp' -ldl
    '$OUT/fuzz_client' '$SO' '$ROOT/gamedir/syndicate' --seconds '${FUZZ_TIME:-120}' ${SKIP:+--skip '$SKIP'} $* 2>&1 \
      | tee '$OUT/run.log' | grep -E '^(\*\*\*|fuzz_client|SUMMARY|.*runtime error)' | sort | uniq -c | sort -rn | head -200
    exit \${PIPESTATUS[0]}
  "
