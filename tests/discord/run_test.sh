#!/usr/bin/env bash
# Protocol test for the Discord IPC client (src/cl_dll/discord/discord_ipc.cpp) on Linux: builds tests/discord/
# ipc_driver.cpp with the POSIX transport and runs it against fake_discord.py. Needs g++ and python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
g++ -std=c++17 -Wall -Wextra -I"$ROOT/src/cl_dll/discord" -o "$T/driver" \
  "$ROOT/tests/discord/ipc_driver.cpp" "$ROOT/src/cl_dll/discord/discord_ipc.cpp"
python3 "$ROOT/tests/discord/fake_discord.py" "$T/discord-ipc-0" "$T/result" > "$T/server.log" 2>&1 &
SRV=$!
for _ in $(seq 50); do [ -S "$T/discord-ipc-0" ] && break; sleep 0.1; done
XDG_RUNTIME_DIR=$T timeout 60 "$T/driver" 2> "$T/driver.log" || { echo "driver failed"; cat "$T/driver.log" "$T/server.log"; exit 1; }
wait $SRV || true
cat "$T/driver.log"
result=$(cat "$T/result")
[ "$result" = OK ] || { echo "FAIL: $result"; cat "$T/server.log"; exit 1; }
echo "PASS"
