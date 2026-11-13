#!/bin/sh
# Builds the Aurora editor as a static site from this checkout (the game's particle code compiled with emscripten,
# the parity test, SBO's scripts and sprites, the allow-listed Half-Life sprites) and pushes it to the gh-pages
# branch of the editor's GitHub repository, which GitHub Pages serves. README.md "Static site".
#
#   tools/aurora-editor/publish.sh <valve/sprites dir> [--no-push]
#
# <valve/sprites dir>: Half-Life's valve/sprites (a Half-Life dedicated server has it:
#   steamcmd +login anonymous +app_update 90); only the files in valve-sprites.allow are published.
# --no-push: build and commit in the work folder, do not push.
# Environment: AURORA_PAGES_REPO (default syndicate-black-ops/aurora-editor), AURORA_PUBLISH_DIR (work folder,
#   default build/aurora-editor-publish in the repository root).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
[ $# -ge 1 ] || { echo "usage: $0 <valve/sprites dir> [--no-push]" >&2; exit 2; }
VALVE=$(cd "$1" && pwd)
PUSH=1
[ "${2:-}" = "--no-push" ] && PUSH=0
REPO=${AURORA_PAGES_REPO:-syndicate-black-ops/aurora-editor}
WORK=${AURORA_PUBLISH_DIR:-$ROOT/build/aurora-editor-publish}
SITE=$WORK/site
PAGES=$WORK/gh-pages
BUILDER=aurora-editor-builder

GAME_SHA=$(git -C "$ROOT" rev-parse --short=8 HEAD)
if [ -n "$(git -C "$ROOT" status --porcelain -- tools/aurora-editor src/cl_dll/aurora src/cl_dll/parsemsg.cpp \
	src/cl_dll/parsemsg.h src/game_shared/aurora_shared.h gamedir/syndicate/aurora gamedir/syndicate/sprites)" ]; then
	GAME_SHA="$GAME_SHA+local"
fi

# a builder with hard limits (BuildKit ignores docker build --memory)
if ! docker buildx inspect "$BUILDER" >/dev/null 2>&1; then
	docker buildx create --name "$BUILDER" --driver docker-container \
		--driver-opt memory=4g --driver-opt cpu-quota=200000 --driver-opt cpu-period=100000 >/dev/null
fi

rm -rf "$SITE"
mkdir -p "$WORK"
timeout 1500 docker buildx build --builder "$BUILDER" -f "$HERE/Dockerfile" --target static \
	--build-context valve_sprites="$VALVE" --output type=local,dest="$SITE" "$ROOT"
[ -f "$SITE/index.html" ] && [ -f "$SITE/core/aurora-core.wasm" ] && [ -f "$SITE/assets/index.json" ] \
	|| { echo "publish: the build wrote no site" >&2; exit 1; }
touch "$SITE/.nojekyll"
printf '%s\n' "$GAME_SHA" > "$SITE/version.txt"

# the gh-pages branch: the site's files only, one commit per publish
rm -rf "$PAGES"
if git ls-remote --exit-code --heads "https://github.com/$REPO.git" gh-pages >/dev/null 2>&1; then
	git clone --quiet --depth 1 --branch gh-pages "https://github.com/$REPO.git" "$PAGES"
else
	git init --quiet "$PAGES"
	git -C "$PAGES" checkout --quiet --orphan gh-pages
	git -C "$PAGES" remote add origin "https://github.com/$REPO.git"
fi
find "$PAGES" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -R "$SITE"/. "$PAGES"/
git -C "$PAGES" add -A
if git -C "$PAGES" diff --cached --quiet; then
	echo "publish: no change"
	exit 0
fi
git -C "$PAGES" commit --quiet -S -m "Publish the editor built from game $GAME_SHA"
git -C "$PAGES" log --oneline -1
if [ "$PUSH" = 1 ]; then
	git -C "$PAGES" push --quiet origin gh-pages
	echo "publish: pushed to $REPO gh-pages"
fi
