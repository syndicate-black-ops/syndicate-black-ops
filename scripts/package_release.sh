#!/usr/bin/env bash
# Package the built binaries in the release layout and list their hashes (informational only).
#   scripts/package_release.sh <client.dll> <hl.dll> <hl.so> <out dir> [client.so]
# Writes <out>/syndicate/cl_dlls/client.dll, syndicate/dlls/hl.dll, syndicate/dlls/hl.so, with the optional fifth
# argument syndicate/cl_dlls/client.so (the Linux client, docs/dev/LINUX_CLIENT.md), and <out>/hashes.txt (size, md5 and
# sha256 of each). hl.so and client.so are shipped without debug info (symbols kept: plugins resolve hl.so's); the
# unstripped ones stay in the build-linux artifacts.
set -uo pipefail
[ $# -ge 4 ] || { echo "usage: package_release.sh <client.dll> <hl.dll> <hl.so> <out dir> [client.so]" >&2; exit 2; }
OUT=$4
CLIENT_SO=${5:-}
rm -rf "$OUT/syndicate"; mkdir -p "$OUT/syndicate/cl_dlls" "$OUT/syndicate/dlls"
cp "$1" "$OUT/syndicate/cl_dlls/client.dll" && cp "$2" "$OUT/syndicate/dlls/hl.dll" && cp "$3" "$OUT/syndicate/dlls/hl.so" || exit 1
objcopy --strip-debug "$OUT/syndicate/dlls/hl.so" || exit 1
FILES=(syndicate/cl_dlls/client.dll syndicate/dlls/hl.dll syndicate/dlls/hl.so)
if [ -n "$CLIENT_SO" ]; then
  cp "$CLIENT_SO" "$OUT/syndicate/cl_dlls/client.so" || exit 1
  objcopy --strip-debug "$OUT/syndicate/cl_dlls/client.so" || exit 1
  FILES=(syndicate/cl_dlls/client.dll syndicate/cl_dlls/client.so syndicate/dlls/hl.dll syndicate/dlls/hl.so)
fi
cd "$OUT" || exit 1
{
  echo "SBO build (hashes are informational)"
  [ -n "${CI_COMMIT_SHA:-}" ] && echo "commit $CI_COMMIT_SHA, pipeline ${CI_PIPELINE_URL:-?}"
  echo
  for path in "${FILES[@]}"; do
    printf '%s (%s bytes)\n' "$path" "$(stat -c %s "$path")"
    printf '  md5     %s\n' "$(md5sum < "$path" | cut -d' ' -f1)"
    printf '  sha256  %s\n' "$(sha256sum < "$path" | cut -d' ' -f1)"
  done
} > hashes.txt
cat hashes.txt
