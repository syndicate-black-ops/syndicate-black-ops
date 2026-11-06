#!/usr/bin/env bash
# Configure + build SBO with CMake inside a toolchain image (CI runs this directly; scripts/build.sh wraps it
# in docker for local builds).
#
#   scripts/cmake_build.sh <windows|linux> <out dir> [config] [extra cmake -D args...]
#     windows  client.dll + hl.dll   (docker/clang-cl image: clang-cl + lld-link + xwin MSVC CRT/SDK)
#     linux    client.so + hl.so  (Steam Runtime 3 "sniper" SDK image: GCC -m32, glibc 2.31)
#   config: RelWithDebInfo (default), Release, Debug, MinSizeRel
# Output: <out>/ client.dll client.pdb client.map hl.dll hl.pdb hl.map | client.so client.map hl.so hl.map,
#         <out>/logs/{configure,build}.log, build tree <out>/cmake/
# GITHUB_ACTIONS=true (set by GitHub Actions; scripts/build.sh passes it on): the configure and build output also goes
# to the job log, in collapsible groups. Otherwise it goes to the log files only.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PLATFORM=${1:?usage: cmake_build.sh <windows|linux> <out dir> [config] [cmake args...]}
mkdir -p "${2:?usage: cmake_build.sh <windows|linux> <out dir> [config] [cmake args...]}"; OUT=$(cd "$2" && pwd)
CONFIG=${3:-RelWithDebInfo}; shift 2; [ $# -gt 0 ] && shift
case $PLATFORM in
  windows) TOOLCHAIN=$ROOT/cmake/toolchains/clang-cl-i686.cmake; FILES=(client.dll client.pdb client.map hl.dll hl.pdb hl.map);;
  linux)   TOOLCHAIN=$ROOT/cmake/toolchains/linux-i686.cmake;   FILES=(client.so client.map hl.so hl.map);;
  *) echo "unknown platform: $PLATFORM" >&2; exit 2;;
esac
BLD=$OUT/cmake; LOGS=$OUT/logs; mkdir -p "$BLD" "$LOGS"
run_logged() {  # <title> <log file> <command...>
  if [ "${GITHUB_ACTIONS:-}" = true ]; then
    local rc; echo "::group::$1"; "${@:3}" 2>&1 | tee "$2"; rc=${PIPESTATUS[0]}; echo "::endgroup::"; return "$rc"
  fi
  "${@:3}" > "$2" 2>&1
}

echo "==== configure ($PLATFORM, $CONFIG) ===="
if ! run_logged "configure ($PLATFORM, $CONFIG)" "$LOGS/configure.log" \
     cmake -G Ninja -S "$ROOT" -B "$BLD" -DCMAKE_BUILD_TYPE="$CONFIG" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" "$@"; then
  tail -30 "$LOGS/configure.log" >&2; echo "configure failed (see $LOGS/configure.log)" >&2; exit 1
fi
echo "==== build ===="
if ! run_logged "build ($PLATFORM)" "$LOGS/build.log" cmake --build "$BLD" -j "${JOBS:-$(nproc)}"; then
  grep -E 'error:|error [A-Z]+[0-9]+|undefined (symbol|reference)|FAILED' "$LOGS/build.log" | head -40 >&2
  echo "build failed (see $LOGS/build.log)" >&2; exit 1
fi
for f in "${FILES[@]}"; do cp "$BLD/$f" "$OUT/"; done
echo "warnings: $(grep -c 'warning' "$LOGS/build.log" || true)"
if [ "${GITHUB_ACTIONS:-}" = true ]; then
  echo "::group::warnings ($PLATFORM)"; grep -E 'warning' "$LOGS/build.log" | sort -u || true; echo "::endgroup::"
fi
cd "$OUT" && for f in "${FILES[@]}"; do case $f in *.dll|*.so) ls -la "$f"; md5sum "$f"; sha256sum "$f";; esac; done
