#!/usr/bin/env bash
# GitHub Actions build step (.github/workflows/build.yml): scripts/build.sh <platform> RelWithDebInfo with the
# CMake settings the workflow passes in the environment.
#   RELEASE=true              -DSBO_TEST_COMMANDS=OFF (release builds state it explicitly)
#   VERSION_OVERRIDE          -DSBO_VERSION_OVERRIDE (pre-release tags)
#   UPDATE_REPO, UPDATE_PUBKEY, UPDATE_PRERELEASES   the updater settings (repository variables; test repos only)
# CI_COMMIT_SHORT_SHA / CI_COMMIT_REF_NAME make the version line a CI one (cmake/sbo_version.cmake).
set -euo pipefail
PLATFORM=${1:?usage: ci_build.sh <windows|linux>}
args=()
[ "${RELEASE:-false}" = true ] && args+=(-DSBO_TEST_COMMANDS=OFF)
[ -n "${VERSION_OVERRIDE:-}" ] && args+=("-DSBO_VERSION_OVERRIDE=$VERSION_OVERRIDE")
[ -n "${UPDATE_REPO:-}" ] && args+=("-DSBO_UPDATE_REPO=$UPDATE_REPO")
[ -n "${UPDATE_PUBKEY:-}" ] && args+=("-DSBO_UPDATE_PUBKEY=$UPDATE_PUBKEY")
[ "${UPDATE_PRERELEASES:-}" = ON ] && args+=(-DSBO_UPDATE_PRERELEASES=ON)
export CI_COMMIT_SHORT_SHA=${GITHUB_SHA::8} CI_COMMIT_REF_NAME=$GITHUB_REF_NAME JOBS=${JOBS:-$(nproc)}
echo "scripts/build.sh $PLATFORM RelWithDebInfo ${args[*]}"
bash scripts/build.sh "$PLATFORM" RelWithDebInfo "${args[@]}"
