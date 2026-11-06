#!/usr/bin/env bash
# Release checks: static analysis, sanitizer builds and fuzz harnesses of the game code, in one command.
#
#   scripts/check_sanitizers.sh [all|static|asan|fuzz|tests] ...   (default: all = static asan fuzz tests)
#
#   static  clang-tidy (bugprone-*, clang-analyzer-*, a few cert-*), cppcheck and a set of bug-finding compiler
#           warnings over src/ (not the SDK), with the real compile flags: the analysis build tree is configured
#           from the same CMake files and toolchain file as the sniper build, with clang -m32 (docker/analysis).
#           Findings that are not in tests/static/baseline.tsv (the triaged ones) are listed; exit status 1 if any.
#   asan    hl.so + client.so built with -fsanitize=address,undefined and the test layer (-DSBO_TEST_COMMANDS=ON, for
#           run_entity_tests.sh --asan) in the sniper image (build/sanitize/asan/).
#           For the fuzz harnesses and for a soak on a dedicated server (LD_PRELOAD the image's 32-bit libasan, see
#           TOOLING-NOTES.md).
#   fuzz    tests/fuzz/*/run_test.sh (each builds its own harness; the server harness loads build/sanitize/asan/hl.so).
#           FUZZ_TIME (seconds per harness, default 120).
#   tests   the existing tests: tests/*/run_test.sh (not tests/fuzz), scripts/fgd.py check, scripts/check_eol.sh,
#           scripts/check_lowercase.sh.
#
# Output: build/sanitize/<step>/ (logs), a summary at the end. Heavy steps run in docker with --memory/--cpus caps and
# at low priority; JOBS (default 2) is the parallelism inside them. One step at a time, never in parallel.
# Env: SNIPER_IMAGE (as scripts/build.sh), ANALYSIS_IMAGE (default sbo/analysis:local, built from docker/analysis if
# missing), DOCKER_MEMORY (default 4g), DOCKER_CPUS (default 2), JOBS (default 2), FUZZ_TIME (default 120).
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/sanitize
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}
ANALYSIS_IMAGE=${ANALYSIS_IMAGE:-sbo/analysis:local}
DOCKER_MEMORY=${DOCKER_MEMORY:-4g}
DOCKER_CPUS=${DOCKER_CPUS:-2}
export JOBS=${JOBS:-2}
export FUZZ_TIME=${FUZZ_TIME:-120}
mkdir -p "$OUT"

# same mounts as scripts/build.sh: a worktree's symlinked SDK and the main checkout's .git (version header)
EXTRA_MOUNTS=()
if [ -L "$ROOT/external/hl1_source_sdk" ]; then
  SDK_TARGET=$(readlink -f "$ROOT/external/hl1_source_sdk")
  EXTRA_MOUNTS=(-v "$SDK_TARGET:$SDK_TARGET:ro")
fi
if [ -f "$ROOT/.git" ]; then
  GIT_DIR_PATH=$(sed -n 's/^gitdir: *//p' "$ROOT/.git")
  case $GIT_DIR_PATH in /*) ;; *) GIT_DIR_PATH=$ROOT/$GIT_DIR_PATH;; esac
  if [ -f "$GIT_DIR_PATH/commondir" ]; then
    GIT_COMMON=$(cd "$GIT_DIR_PATH" && cd "$(cat commondir)" && pwd)
    EXTRA_MOUNTS+=(-v "$GIT_COMMON:$GIT_COMMON:ro")
  fi
fi
run() { local img=$1; shift
  nice -n 15 docker run --rm --memory="$DOCKER_MEMORY" --memory-swap="$DOCKER_MEMORY" --cpus="$DOCKER_CPUS" -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -e JOBS -e FUZZ_TIME -v "$ROOT:$ROOT" "${EXTRA_MOUNTS[@]}" -w "$ROOT" "$img" "$@"; }

declare -A RESULT
step_static() {
  local S=$OUT/static; mkdir -p "$S"
  if ! docker image inspect "$ANALYSIS_IMAGE" >/dev/null 2>&1; then
    echo "building $ANALYSIS_IMAGE (log: $S/docker.log)"
    nice -n 15 docker build --memory="$DOCKER_MEMORY" -t "$ANALYSIS_IMAGE" "$ROOT/docker/analysis" > "$S/docker.log" 2>&1 \
      || { tail -20 "$S/docker.log"; return 1; }
  fi
  # the client's SDL2 headers come from the sniper image (find_path in cmake/sbo_client.cmake)
  if [ ! -f "$S/include/SDL2/SDL_mouse.h" ]; then
    mkdir -p "$S/include"
    docker run --rm -u "$(id -u):$(id -g)" -v "$S/include:/out" "$SNIPER_IMAGE" cp -r /usr/include/SDL2 /out/ || return 1
  fi
  echo "==== static: configure (clang -m32, compile_commands.json) ===="
  run "$ANALYSIS_IMAGE" bash -c "CC=clang CXX=clang++ cmake -G Ninja -S . -B '$S/cmake' -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-i686.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DSBO_SDL2_INCLUDE_DIR='$S/include' -DSBO_SDL2_LIBRARY=SDL2 -DSBO_TEST_COMMANDS=ON > '$S/configure.log' 2>&1 \
      && ninja -C '$S/cmake' sbo_version_gen >> '$S/configure.log' 2>&1 \
      && python3 -I tests/static/compdb.py '$S/cmake/compile_commands.json' '$S/compile_commands.json'" \
    || { tail -20 "$S/configure.log"; return 1; }
  echo "==== static: clang-tidy (log: $S/clang-tidy.log) ===="
  run "$ANALYSIS_IMAGE" timeout 5400 run-clang-tidy -j "$JOBS" -quiet -p "$S" -config-file=tests/static/clang-tidy.yaml \
    > "$S/clang-tidy.log" 2>&1
  echo "==== static: cppcheck (log: $S/cppcheck.log) ===="
  mkdir -p "$S/cppcheck-build"
  run "$ANALYSIS_IMAGE" timeout 5400 cppcheck --project="$S/compile_commands.json" -j "$JOBS" --enable=warning,portability \
    --inline-suppr --suppress=missingIncludeSystem --suppress=unmatchedSuppression --suppress='*:*/hl1_source_sdk/*' \
    --cppcheck-build-dir="$S/cppcheck-build" --template='{file}:{line}:{column}: {severity}: {message} [{id}]' --quiet \
    > "$S/cppcheck.log" 2>&1
  echo "==== static: compiler warnings (log: $S/warnings.log) ===="
  run "$ANALYSIS_IMAGE" timeout 3600 python3 -I tests/static/compiler_warnings.py "$S/compile_commands.json" "$JOBS" \
    > "$S/warnings.log" 2>&1
  python3 -I "$ROOT/tests/static/report.py" "$ROOT" "$S" --tidy "$S/clang-tidy.log" --cppcheck "$S/cppcheck.log" \
    --warnings "$S/warnings.log" | tee "$S/report.txt"
  return "${PIPESTATUS[0]}"
}

step_asan() {
  local A=$OUT/asan; mkdir -p "$A"
  echo "==== asan: hl.so + client.so, -fsanitize=address,undefined (log: $A/logs/) ===="
  # -m32 again: these replace the toolchain file's CMAKE_<LANG>_FLAGS_INIT
  local SAN="-m32 -fsanitize=address,undefined -fno-omit-frame-pointer"
  run "$SNIPER_IMAGE" bash scripts/cmake_build.sh linux "$A" RelWithDebInfo \
    "-DCMAKE_C_FLAGS=$SAN" "-DCMAKE_CXX_FLAGS=$SAN" "-DCMAKE_SHARED_LINKER_FLAGS=-m32 -fsanitize=address,undefined" \
    "-DCMAKE_EXE_LINKER_FLAGS=-m32 -fsanitize=address,undefined" -DSBO_TEST_COMMANDS=ON
}

step_fuzz() {
  local F=$OUT/fuzz rc=0; mkdir -p "$F"
  for t in "$ROOT"/tests/fuzz/*/run_test.sh; do
    local name; name=$(basename "$(dirname "$t")")
    echo "==== fuzz: $name (log: $F/$name.log) ===="
    if bash "$t" > "$F/$name.log" 2>&1; then echo "  ok"; else echo "  FAILED"; tail -30 "$F/$name.log"; rc=1; fi
  done
  return $rc
}

step_tests() {
  local T=$OUT/tests rc=0; mkdir -p "$T"
  for t in "$ROOT"/tests/*/run_test.sh; do
    case $t in */tests/fuzz/*) continue;; esac
    local name; name=$(basename "$(dirname "$t")")
    echo "==== test: $name (log: $T/$name.log) ===="
    if timeout 1800 nice -n 15 bash "$t" > "$T/$name.log" 2>&1; then echo "  ok"; else echo "  FAILED"; tail -20 "$T/$name.log"; rc=1; fi
  done
  echo "==== test: fgd.py check, check_eol.sh, check_lowercase.sh ===="
  (cd "$ROOT" && python3 scripts/fgd.py check) > "$T/fgd.log" 2>&1 || { echo "  fgd.py check FAILED"; rc=1; }
  (cd "$ROOT" && bash scripts/check_eol.sh) > "$T/eol.log" 2>&1 || { echo "  check_eol.sh FAILED"; rc=1; }
  (cd "$ROOT" && bash scripts/check_lowercase.sh) > "$T/lowercase.log" 2>&1 || { echo "  check_lowercase.sh FAILED"; rc=1; }
  return $rc
}

STEPS=("$@"); [ ${#STEPS[@]} -eq 0 ] && STEPS=(all)
[ "${STEPS[0]}" = all ] && STEPS=(static asan fuzz tests)
status=0
for s in "${STEPS[@]}"; do
  case $s in static|asan|fuzz|tests) ;; *) echo "usage: $0 [all|static|asan|fuzz|tests]..." >&2; exit 2;; esac
  if "step_$s"; then RESULT[$s]=ok; else RESULT[$s]=FAILED; status=1; fi
done
echo; echo "==== summary ===="
for s in "${STEPS[@]}"; do printf '  %-7s %s\n' "$s" "${RESULT[$s]}"; done
exit $status
