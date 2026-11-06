#!/usr/bin/env bash
# Release check: the game libraries of a release build must not contain the test layer (docs/dev/ENTITY_TESTS.md,
# CMake option SBO_TEST_COMMANDS=OFF). Fails when a library holds the marker or any test_* command name.
#
#   scripts/check_no_test_commands.sh <hl.so|hl.dll|client.so|client.dll> ...
set -uo pipefail
[ $# -gt 0 ] || { echo "usage: $0 <library> ..." >&2; exit 2; }
CMDS='test_(run|trace|seed|mark|time|snap|player|join|cmd|userinfo|flags|spawn|teleport|move|look|fire|kv|damage|round|quit|precache_first|cl_[a-z_]+|skipmenus)'
status=0
for f in "$@"; do
  [ -f "$f" ] || { echo "$f: missing" >&2; status=1; continue; }
  hits=$(strings -a "$f" | grep -E "SBO_TEST_COMMANDS|^${CMDS}\$" | sort -u)
  if [ -n "$hits" ]; then
    echo "$f: contains the test layer:" >&2; echo "$hits" | sed 's/^/  /' >&2; status=1
  else
    echo "$f: no test layer"
  fi
done
exit $status
