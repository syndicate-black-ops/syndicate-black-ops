#!/usr/bin/env bash
# CHANGELOG.md is the release text (GitHub release, in-game update dialog, docs site): it parses, every version
# heading is valid, no group is empty, and the newest version is the one the build packages (project(VERSION)).
# Also tests the parser and scripts/updater_release.py notes on bad input. Needs python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
python3 -I "$ROOT/tests/changelog/test_changelog.py" "$ROOT"
