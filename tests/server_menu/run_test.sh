#!/usr/bin/env bash
# Server menu protocol test (docs/SERVER_MENUS.md): builds the client's ServerMenu parser and store
# (src/cl_dll/vgui2/sbo_server_menu_proto.cpp) alone, without the SDK, with AddressSanitizer and
# UndefinedBehaviorSanitizer, and feeds it the API plugin's records and hostile input
# (tests/server_menu/server_menu_test.cpp). Optional argument: the Pawn include (server_menu.inc) whose constants must
# equal src/game_shared/server_menu_shared.h. Needs g++.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# no ulimit -v: AddressSanitizer reserves terabytes of address space (it uses a few hundred MB)
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
  -I"$ROOT/src/cl_dll/vgui2" -o "$T/server_menu_test" \
  "$ROOT/tests/server_menu/server_menu_test.cpp" "$ROOT/src/cl_dll/vgui2/sbo_server_menu_proto.cpp"
timeout 300 "$T/server_menu_test" "$@"
