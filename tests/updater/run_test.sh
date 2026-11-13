#!/usr/bin/env bash
# Offline tests of the updater (src/cl_dll/updater, docs/UPDATER.md) with ASan/UBSan: JSON, versions, the release
# list, paths, manifests, the install plan, markdown, SHA-256, Ed25519, inflate, the installer (rollback, crash
# recovery, files in use) and a whole update through a fake HTTP server, on a scratch folder; and the newest
# CHANGELOG.md section rendered as the update dialog shows it. Needs g++, gcc, python3.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
U="$ROOT/src/cl_dll/updater"
SAN="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all"
gcc -std=c99 $SAN -c "$ROOT/external/monocypher/monocypher.c" -o "$T/mc.o"
gcc -std=c99 $SAN -I"$ROOT/external/monocypher" -c "$ROOT/external/monocypher/monocypher-ed25519.c" -o "$T/mced.o"
g++ -std=c++17 -Wall -Wextra $SAN -DSBO_UPDATER_TESTING -I"$U" -I"$ROOT/external/monocypher" \
  -I"$ROOT/src/cl_dll/vgui2" -o "$T/updater_test" "$ROOT/tests/updater/updater_test.cpp" \
  "$U/updater_json.cpp" "$U/updater_core.cpp" "$U/updater_sha256.cpp" "$U/updater_crypto.cpp" \
  "$U/updater_install.cpp" "$U/updater_client.cpp" "$T/mc.o" "$T/mced.o" -lpthread
python3 -I - "$T/data.deflate" "$T/data.txt" <<'PY'
import sys, zlib, random
random.seed(3)
plain = b"".join(b"line %d %s\n" % (i, bytes(random.choice(b"abcdef ") for _ in range(40))) for i in range(3000))
c = zlib.compressobj(9, zlib.DEFLATED, -15)
open(sys.argv[1], "wb").write(c.compress(plain) + c.flush())
open(sys.argv[2], "wb").write(plain)
PY
mkdir -p "$T/scratch"
VERSION=$(sed -n 's/^project([a-z]* VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")
python3 "$ROOT/scripts/updater_release.py" notes "$VERSION" > "$T/notes.md"
"$T/updater_test" "$T/scratch" "$T/data.deflate" "$T/data.txt" "$T/notes.md"
