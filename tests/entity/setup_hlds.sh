#!/usr/bin/env bash
# Sets up a throw-away dedicated server folder for a sanitizer soak (scripts/check_sanitizers.sh asan, then
# tests/soak/run_soak.sh): every file of a stock HLDS install is a read-only symlink, the "syndicate" folder links the
# repo's gamedir/syndicate except dlls/hl.so (the sanitizer build) and, with a Metamod and a jk_botti build given,
# addons/ for bots. Nothing in the HLDS install, the repo or jk_botti is written.
#
#   tests/soak/setup_hlds.sh <stock HLDS dir> <new server dir> [hl.so] [metamod.so] [jk_botti_mm_i386.so jk_botti dir]
#     hl.so default build/sanitize/asan/hl.so
# Copied from tests/soak/setup_hlds.sh on fix/sanitizer-audit (2026-10-15) for the entity tests
# (docs/dev/ENTITY_TESTS.md); deduplicate when that branch merges.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=$(cd "${1:?stock HLDS dir}" && pwd)
DST=${2:?new server dir}
HL=$(readlink -f "${3:-$ROOT/build/sanitize/asan/hl.so}")
MM=${4:-}; BOT=${5:-}; BOTDIR=${6:-}
[ -x "$SRC/hlds_linux" ] || { echo "no hlds_linux in $SRC" >&2; exit 2; }
[ -f "$HL" ] || { echo "no $HL" >&2; exit 2; }
[ -e "$DST" ] && { echo "$DST exists: give a new folder" >&2; exit 2; }
mkdir -p "$DST"
for f in "$SRC"/* ; do
  case $(basename "$f") in syndicate|steam_appid.txt) continue;; esac
  ln -s "$f" "$DST/"
done
echo 70 > "$DST/steam_appid.txt"

G=$DST/syndicate; mkdir -p "$G/dlls" "$G/maps"
for f in "$ROOT"/gamedir/syndicate/*; do
  case $(basename "$f") in dlls|maps|liblist.gam|server.cfg) continue;; esac
  ln -s "$f" "$G/"
done
for f in "$ROOT"/gamedir/syndicate/maps/*; do ln -s "$f" "$G/maps/"; done
ln -s "$HL" "$G/dlls/hl.so"
# server.cfg: the shipped one, plus soak settings (no password, no master server)
{ cat "$ROOT/gamedir/syndicate/server.cfg"; printf 'sv_lan 1\nsv_cheats 0\nmp_timelimit 0\n'; } > "$G/server.cfg"
GAMEDLL=dlls/hl.so
if [ -n "$MM" ]; then
  mkdir -p "$G/addons/metamod/dlls"
  ln -s "$(readlink -f "$MM")" "$G/addons/metamod/dlls/metamod.so"
  echo "linux addons/jk_botti/dlls/jk_botti_mm_i386.so" > "$G/addons/metamod/plugins.ini"
  GAMEDLL=addons/metamod/dlls/metamod.so
  if [ -n "$BOT" ]; then
    mkdir -p "$G/addons/jk_botti/dlls"
    ln -s "$(readlink -f "$BOT")" "$G/addons/jk_botti/dlls/jk_botti_mm_i386.so"
    # its configs and waypoints, copied (jk_botti writes waypoints it learns)
    [ -n "$BOTDIR" ] && [ -d "$BOTDIR/addons/jk_botti" ] && \
      (cd "$BOTDIR/addons/jk_botti" && find . -type f ! -name '*.so' -exec install -D -m 644 {} "$G/addons/jk_botti/{}" \;)
  fi
fi
sed -E "s#^gamedll_linux .*#gamedll_linux \"$GAMEDLL\"#" "$ROOT/gamedir/syndicate/liblist.gam" > "$G/liblist.gam"
grep -q '^gamedll_linux' "$G/liblist.gam" || echo "gamedll_linux \"$GAMEDLL\"" >> "$G/liblist.gam"
echo "server folder: $DST (game dll $GAMEDLL -> $HL)"
