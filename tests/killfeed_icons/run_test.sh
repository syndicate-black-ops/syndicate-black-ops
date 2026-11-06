#!/usr/bin/env bash
# Offline test of the killfeed icon checks (src/game_shared/killfeed_icons_shared.h, docs/KILLFEED_ICONS.md) with
# ASan/UBSan: weapon codes and sheet paths from the server command and the KfIcon message, valid, hostile and
# random input. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/game_shared" -o "$T/kficon_test" "$ROOT/tests/killfeed_icons/kficon_test.cpp"
"$T/kficon_test"
