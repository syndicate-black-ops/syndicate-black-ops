#!/usr/bin/env bash
# Local build in docker (same steps as CI).
#
#   scripts/build.sh [windows|linux|all] [config] [extra cmake -D args...]
#     -> build/windows/{client,hl}.{dll,pdb,map}   build/linux/{client.so,client.map,hl.so,hl.map}
#        (logs in build/<platform>/logs/)
# Env: CLANG_CL_IMAGE (default sbo/clang-cl:local; built from docker/clang-cl if missing),
#      SNIPER_IMAGE (default: the Steam Runtime 3 "sniper" SDK image CI uses), JOBS (default nproc).
#      CI_COMMIT_SHORT_SHA, CI_COMMIT_REF_NAME, CI_COMMIT_BRANCH are passed on when set: a CI build's version line
#      (cmake/sbo_version.cmake; GitHub Actions sets them, .github/workflows/build.yml).
#      GITHUB_ACTIONS is passed on too: scripts/cmake_build.sh then shows the configure and build output in the job log.
#      SBO_NO_BUILD_CACHE=1: compile without the shared compiler cache (below); the binaries are the same either way.
#
# The checkout is mounted at /sbo in the build container, whichever checkout or worktree it is, so every compile
# command is the same in every checkout: compiler messages, debug info (DWARF, PDB) and __FILE__ name /sbo/...
# (for gdb: set substitute-path /sbo <checkout>). The build tree is build/<platform>/cmake.
# Shared compiler cache, used when its binary is in <main checkout>/build/tools/ (static builds mounted into the
# container; without it the build runs uncached): Linux ccache (build/tools/ccache-<version>/ccache, cache
# build/build-cache/ccache), Windows sccache (build/tools/sccache-<version>/sccache, cache
# build/build-cache/sccache-windows; one Windows build uses it at a time, a second one waits for it). Each is
# capped at 5 GB.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WHAT=${1:-all}; shift || true
CLANG_CL_IMAGE=${CLANG_CL_IMAGE:-sbo/clang-cl:local}
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}

# A git worktree may have external/hl1_source_sdk as a symlink to another checkout's submodule: mount its target too
EXTRA_MOUNTS=()
if [ -L "$ROOT/external/hl1_source_sdk" ]; then
  SDK_TARGET=$(readlink -f "$ROOT/external/hl1_source_sdk")
  EXTRA_MOUNTS=(-v "$SDK_TARGET:$SDK_TARGET:ro")
fi
# A git worktree's .git is a file naming a git dir in the main checkout: mount that checkout's .git too, so
# cmake/sbo_version.cmake can read the commit and the branch (without it: "unknown")
if [ -f "$ROOT/.git" ]; then
  GIT_DIR_PATH=$(sed -n 's/^gitdir: *//p' "$ROOT/.git")
  case $GIT_DIR_PATH in /*) ;; *) GIT_DIR_PATH=$ROOT/$GIT_DIR_PATH;; esac
  if [ -f "$GIT_DIR_PATH/commondir" ]; then
    GIT_COMMON=$(cd "$GIT_DIR_PATH" && cd "$(cat commondir)" && pwd)
    EXTRA_MOUNTS+=(-v "$GIT_COMMON:$GIT_COMMON:ro")
  fi
fi
# The main checkout (its build/ holds the compiler cache and its tools): a worktree's common git dir is
# <main checkout>/.git; a plain checkout is its own main checkout
MAIN=$ROOT
if [ -n "${GIT_COMMON:-}" ] && [ "$(basename "$GIT_COMMON")" = .git ]; then MAIN=$(dirname "$GIT_COMMON"); fi
newest_tool() { ls -d "$MAIN"/build/tools/"$1"-*/"$1" 2>/dev/null | sort -V | tail -1 || true; }

# run <platform> <image>: scripts/cmake_build.sh <platform> in <image>, checkout at /sbo, output in build/<platform>
run() { local platform=$1 img=$2; shift 2
  local tree=$ROOT/build/$platform/cmake cache=(-e SBO_COMPILER_LAUNCHER=) lock=
  # a build tree configured before the /sbo mount names the host path: CMake refuses it, so it is set aside once
  if [ -f "$tree/CMakeCache.txt" ] && ! grep -q "^CMAKE_CACHEFILE_DIR:INTERNAL=/sbo/build/$platform/cmake\$" "$tree/CMakeCache.txt"; then
    local aside=$tree.host-path; [ ! -e "$aside" ] || aside=$aside.$(date +%s)
    mv "$tree" "$aside"
    echo "build/$platform/cmake was configured outside /sbo: moved to ${aside#"$ROOT"/} (unused, can be deleted)" >&2
  fi
  if [ "${SBO_NO_BUILD_CACHE:-}" != 1 ]; then
    local tool dir
    case $platform in linux) tool=$(newest_tool ccache); dir=$MAIN/build/build-cache/ccache;;
                      windows) tool=$(newest_tool sccache); dir=$MAIN/build/build-cache/sccache-windows;; esac
    if [ -n "$tool" ] && [ -x "$tool" ]; then
      mkdir -p "$dir"
      cache=(-v "$tool:/opt/sbo-build-cache/$(basename "$tool"):ro" -v "$dir:/build-cache"
             -e SBO_COMPILER_LAUNCHER=/opt/sbo-build-cache/"$(basename "$tool")")
      case $platform in
        # sloppiness: ccache's settings for precompiled headers (vgui_controls; -include) and no __DATE__/__TIME__ in
        # the sources
        linux) cache+=(-e CCACHE_DIR=/build-cache -e CCACHE_MAXSIZE=5G -e CCACHE_SLOPPINESS=pch_defines,time_macros);;
        # sccache: one server per cache directory (its docs: concurrent servers on one directory race)
        windows) cache+=(-e SCCACHE_DIR=/build-cache -e SCCACHE_CACHE_SIZE=5G -e SCCACHE_IDLE_TIMEOUT=0); lock=$dir.lock;;
      esac
    fi
  fi
  (
    if [ -n "$lock" ]; then
      exec 9>"$lock"
      flock -n 9 || { echo "waiting for the $platform compiler cache (another $platform build uses it)" >&2; flock 9; }
    fi
    docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e JOBS \
      -e CI_COMMIT_SHORT_SHA -e CI_COMMIT_REF_NAME -e CI_COMMIT_BRANCH -e GITHUB_ACTIONS "${cache[@]}" \
      -v "$ROOT:/sbo" "${EXTRA_MOUNTS[@]}" -w /sbo "$img" bash scripts/cmake_build.sh "$platform" "/sbo/build/$platform" "$@"
  ); }

windows() {
  if ! docker image inspect "$CLANG_CL_IMAGE" >/dev/null 2>&1; then
    mkdir -p "$ROOT/build"
    echo "building toolchain image $CLANG_CL_IMAGE (log: build/docker-clang-cl.log)" >&2
    docker build -t "$CLANG_CL_IMAGE" "$ROOT/docker/clang-cl" > "$ROOT/build/docker-clang-cl.log" 2>&1 \
      || { tail -20 "$ROOT/build/docker-clang-cl.log" >&2; exit 1; }
  fi
  run windows "$CLANG_CL_IMAGE" "$@"
}
linux() { run linux "$SNIPER_IMAGE" "$@"; }

case $WHAT in
  windows) windows "$@";;
  linux)   linux "$@";;
  all)     windows "$@"; linux "$@";;
  *) echo "usage: scripts/build.sh [windows|linux|all] [config] [cmake args...]" >&2; exit 2;;
esac
