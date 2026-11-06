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
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WHAT=${1:-all}; shift || true
CLANG_CL_IMAGE=${CLANG_CL_IMAGE:-sbo/clang-cl:local}
SNIPER_IMAGE=${SNIPER_IMAGE:-registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014}

# repo mounted at its host path so PDB/map/diagnostic paths are the real ones
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
run() { local img=$1; shift
  docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e JOBS \
    -e CI_COMMIT_SHORT_SHA -e CI_COMMIT_REF_NAME -e CI_COMMIT_BRANCH -e GITHUB_ACTIONS -v "$ROOT:$ROOT" "${EXTRA_MOUNTS[@]}" -w "$ROOT" "$img" "$@"; }

windows() {
  if ! docker image inspect "$CLANG_CL_IMAGE" >/dev/null 2>&1; then
    mkdir -p "$ROOT/build"
    echo "building toolchain image $CLANG_CL_IMAGE (log: build/docker-clang-cl.log)" >&2
    docker build -t "$CLANG_CL_IMAGE" "$ROOT/docker/clang-cl" > "$ROOT/build/docker-clang-cl.log" 2>&1 \
      || { tail -20 "$ROOT/build/docker-clang-cl.log" >&2; exit 1; }
  fi
  run "$CLANG_CL_IMAGE" bash scripts/cmake_build.sh windows "$ROOT/build/windows" "$@"
}
linux() { run "$SNIPER_IMAGE" bash scripts/cmake_build.sh linux "$ROOT/build/linux" "$@"; }

case $WHAT in
  windows) windows "$@";;
  linux)   linux "$@";;
  all)     windows "$@"; linux "$@";;
  *) echo "usage: scripts/build.sh [windows|linux|all] [config] [cmake args...]" >&2; exit 2;;
esac
