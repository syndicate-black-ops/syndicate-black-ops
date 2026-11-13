#!/usr/bin/env bash
# Assemble the complete SBO release from the repo and one release build (the tag pipeline's release-package job;
# also runs locally). Layout = the playtest package dist/playtest-2.0.0 (see its BUILD.txt):
#
#   sbo-<tag>.zip           player package
#     syndicate/            the game folder: gamedir/syndicate (git-tracked files, LFS content; with the FGD's editor
#                           models and sprites, models/editor + sprites/editor) + sdk/syndicate.fgd
#                           + client.dll + client.pdb, hl.dll + hl.pdb, client.so, hl.so (as built, with debug info)
#     syndicate_addon/      gamedir/syndicate_addon: empty but for readme.txt (the player's own content folder)
#                           nothing else: no config.cfg (the player's own; a new install gets default.cfg's binds,
#                           docs/DEFAULT_BINDS.md), no files at the zip's top level
#   sbo-<tag>-server.zip    server/ (only with an addons dir)
#     syndicate/dlls/       hl.so with debug info, hl.dll + hl.pdb
#     syndicate/addons/     the addons dir's addons/
#     symbols/cl_dlls/      client.so with debug info, client.dll + client.pdb (crash analysis only)
#   SHA256SUMS              sha256 of the two zips
#   manifest.sha256         sha256 of every file in syndicate/ (sha256sum format, paths relative; the input of
#                           scripts/verify.sh / verify.ps1, which the script also runs on the package)
#   build.txt               tag, commit, version lines, layout, install steps, checks, binary hashes (build record)
#   manifest.json (+ .sig)  the in-game updater's file list of the player zip, signed with SBO_UPDATE_SIGNING_KEY when
#                           that is set (docs/UPDATER.md); the player zip has syndicate/updater/installed.json
#
#   scripts/package_full_release.sh <tag> <linux build dir> <windows build dir> <addons dir> <out dir>
#     linux build dir    client.so + hl.so with debug info (scripts/cmake_build.sh linux, -DSBO_TEST_COMMANDS=OFF)
#     windows build dir  client.dll/.pdb + hl.dll/.pdb (scripts/cmake_build.sh windows, -DSBO_TEST_COMMANDS=OFF)
#     addons dir         a folder whose addons/ goes into the server zip (server plugin data built for THIS
#                        hl.so), or "-": no server zip (the player zip holds the same server libraries)
#   Env: SRC  repo tree to take the game data / SDK files from (default: this script's repo)
#        KEEP_STAGE=1  keep the unzipped trees in <out dir>/stage
#        UPDATE_REMOVE_LIST  file of game paths the updater deletes (old files no installed.json lists)
#        UPDATE_PREVIOUS  the previous release's manifest.json (or a manifest.sha256): its versions of the files
#                         players edit, so an unedited one is replaced on installs without installed.json
#        SBO_UPDATE_SIGNING_KEY  the Ed25519 private key (PEM text) that signs manifest.json
#
# Fails when: a game file is an un-fetched Git LFS pointer, a binary holds the test layer
# (scripts/check_no_test_commands.sh), verify.sh disagrees with the manifest, or
# a zip contains a .git* / dot file, editor or build leftover, or a name in the player zip has an upper-case letter
# (scripts/check_lowercase.sh; base game files only, the server zip is not checked).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$(cd "${SRC:-$ROOT}" && pwd)
[ $# -eq 5 ] || { sed -n '2,30p' "$0" >&2; exit 2; }
TAG=$1 LIN=$(cd "$2" && pwd) WIN=$(cd "$3" && pwd) AMXX=
[ "$4" = - ] || AMXX=$(cd "$4" && pwd)
mkdir -p "$5"; OUT=$(cd "$5" && pwd)
NAME=sbo-$(printf '%s' "$TAG" | tr -c 'A-Za-z0-9._-' '_')
for f in "$LIN/client.so" "$LIN/hl.so" "$WIN/client.dll" "$WIN/client.pdb" "$WIN/hl.dll" "$WIN/hl.pdb"; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 2; }
done
[ -z "$AMXX" ] || [ -d "$AMXX/addons" ] || { echo "missing $AMXX/addons (no addons/ folder in it)" >&2; exit 2; }
for so in "$LIN/client.so" "$LIN/hl.so"; do
  file "$so" | grep -q 'with debug_info' || { echo "$so has no debug info (expected a RelWithDebInfo build)" >&2; exit 1; }
done

STAGE=$OUT/stage; P=$STAGE/player; S=$STAGE/server
rm -rf "$STAGE" "$OUT/$NAME.zip" "$OUT/$NAME-server.zip" "$OUT/SHA256SUMS" "$OUT/manifest.sha256" "$OUT/build.txt"
mkdir -p "$P/syndicate" "$P/syndicate_addon" "$S/server/syndicate/dlls" "$S/server/symbols/cl_dlls"

# copy git-tracked files under <repo dir> into <dest> (no untracked / ignored leftovers from the checkout)
copy_tracked() {  # <repo-relative dir> <dest>
  local n=0 rel
  while IFS= read -r -d '' rel; do
    mkdir -p "$2/$(dirname "${rel#"$1"/}")"
    cp -p "$SRC/$rel" "$2/${rel#"$1"/}"; n=$((n + 1))
  done < <(git -C "$SRC" -c safe.directory="$SRC" -c submodule.recurse=false ls-files -z -- "$1")
  echo "  $1: $n files"
}
echo "==== assemble ($NAME, from $(git -C "$SRC" -c safe.directory="$SRC" rev-parse --short=8 HEAD)) ===="
copy_tracked gamedir/syndicate "$P/syndicate"
# never ship a config.cfg: unpacking a build must not overwrite the player's settings (default.cfg instead)
[ ! -e "$P/syndicate/config.cfg" ] || { echo "syndicate/config.cfg must not be shipped (use gamedir/syndicate/default.cfg)" >&2; exit 1; }
[ -f "$P/syndicate/default.cfg" ] || { echo "syndicate/default.cfg is missing (a new install's binds)" >&2; exit 1; }
copy_tracked gamedir/syndicate_addon "$P/syndicate_addon"
cp -p "$SRC/sdk/syndicate.fgd" "$P/syndicate/syndicate.fgd"

# Git LFS: CI must have fetched the objects; a pointer file would ship as a broken model / sound / map
pointers=$(grep -rlI -m1 '^version https://git-lfs.github.com/spec/' "$P" || true)
if [ -n "$pointers" ]; then
  echo "Git LFS pointers instead of content (LFS objects not fetched):" >&2; echo "$pointers" | head -20 >&2; exit 1
fi

# binaries as built: the .so files keep their debug info, the .pdb files sit next to their .dll
mkdir -p "$P/syndicate/cl_dlls" "$P/syndicate/dlls"
cp "$WIN/client.dll" "$P/syndicate/cl_dlls/client.dll"
cp "$WIN/hl.dll" "$P/syndicate/dlls/hl.dll"
cp "$LIN/client.so" "$P/syndicate/cl_dlls/client.so"
cp "$LIN/hl.so" "$P/syndicate/dlls/hl.so"
cp "$WIN/client.pdb" "$P/syndicate/cl_dlls/client.pdb"
cp "$WIN/hl.pdb" "$P/syndicate/dlls/hl.pdb"
cp "$LIN/hl.so" "$WIN/hl.dll" "$WIN/hl.pdb" "$S/server/syndicate/dlls/"
cp "$LIN/client.so" "$WIN/client.dll" "$WIN/client.pdb" "$S/server/symbols/cl_dlls/"
if [ -n "$AMXX" ]; then mkdir -p "$S/server/syndicate/addons"; cp -a "$AMXX/addons/." "$S/server/syndicate/addons/"; fi

echo "==== checks ===="
BINS=(syndicate/cl_dlls/client.dll syndicate/cl_dlls/client.so syndicate/dlls/hl.dll syndicate/dlls/hl.so)
SBINS=(server/syndicate/dlls/hl.so server/syndicate/dlls/hl.dll server/syndicate/dlls/hl.pdb
       server/symbols/cl_dlls/client.so server/symbols/cl_dlls/client.dll server/symbols/cl_dlls/client.pdb)
testlayer=$( (cd "$P" && bash "$ROOT/scripts/check_no_test_commands.sh" "${BINS[@]}") 2>&1 \
  && (cd "$S" && bash "$ROOT/scripts/check_no_test_commands.sh" server/syndicate/dlls/hl.so server/syndicate/dlls/hl.dll \
                     server/symbols/cl_dlls/client.so server/symbols/cl_dlls/client.dll) 2>&1 ) \
  || { echo "$testlayer" >&2; echo "release binaries contain the test layer (configure with -DSBO_TEST_COMMANDS=OFF)" >&2; exit 1; }
echo "$testlayer"

# the updater (docs/UPDATER.md): the version's file list, read by the game when it updates itself. updater/ is the
# updater's own folder (it rewrites installed.json): not in manifest.sha256, so verify.sh never flags it
TAG_VERSION=${TAG#v}
mkdir -p "$P/syndicate/updater"
python3 "$ROOT/scripts/updater_release.py" installed "$P/syndicate" --version "$TAG_VERSION" \
  --commit "$(git -C "$SRC" -c safe.directory="$SRC" rev-parse --short=8 HEAD)" -o "$P/syndicate/updater/installed.json.tmp"
mv "$P/syndicate/updater/installed.json.tmp" "$P/syndicate/updater/installed.json"
(cd "$P/syndicate" && find . -type f ! -path ./config.cfg ! -path './updater/*' -printf '%P\n' | LC_ALL=C sort | tr '\n' '\0' \
  | xargs -0 sha256sum) > "$OUT/manifest.sha256"
manifest_lines=$(wc -l < "$OUT/manifest.sha256")
verify_all=$(bash "$ROOT/scripts/verify.sh" "$P" "$OUT/manifest.sha256") \
  || { head -40 <<< "$verify_all" >&2; echo "verify.sh disagrees with the package" >&2; exit 1; }
verify_out=$(grep ' identical, ' <<< "$verify_all")
echo "verify.sh: $verify_out"

version_of() {  # the "SBO x.y.z+..." banner, else the first "x.y.z+..." string (awk reads all input: no SIGPIPE)
  strings -a "$1" | awk '/^SBO [0-9]+\.[0-9]+\.[0-9]+\+/ && !s {s=$0} /^[0-9]+\.[0-9]+\.[0-9]+\+/ && !v {v=$0}
    END {print s ? s : (v ? v : "(no version line)")}'
}
HL_VER=$(version_of "$LIN/hl.so"); CL_VER=$(version_of "$LIN/client.so")
HLDLL_VER=$(version_of "$WIN/hl.dll"); CLDLL_VER=$(version_of "$WIN/client.dll")
PROJECT_VER=$(sed -n 's/^project(sbo VERSION \([0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt")
case $TAG in v"$PROJECT_VER"*) ;; *) echo "WARNING: tag $TAG does not start with v$PROJECT_VER (CMakeLists.txt project version)" >&2;; esac
COMMIT=${CI_COMMIT_SHA:-$(git -C "$SRC" -c safe.directory="$SRC" rev-parse HEAD)}
count() { find "$1" -type f | wc -l; }

echo "==== build.txt ===="
{
  echo "SBO $TAG release, built $(date -u '+%Y-%m-%d %H:%M UTC')${CI_PIPELINE_URL:+ by CI}. ONE complete build (not an overlay)."
  echo "Tag $TAG, commit $COMMIT${CI_PIPELINE_URL:+, pipeline $CI_PIPELINE_URL}"
  echo "Version lines:"
  echo "  hl.so      $HL_VER"
  echo "  client.so  $CL_VER"
  echo "  hl.dll     $HLDLL_VER"
  echo "  client.dll $CLDLL_VER"
  echo "TEST LAYER OFF: configured with -DSBO_TEST_COMMANDS=OFF; scripts/check_no_test_commands.sh passes on every binary."
  echo "DEBUG INFO: syndicate/ has hl.so / client.so as built, with their debug info, and client.pdb / hl.pdb next to"
  echo "  client.dll / hl.dll."
  echo "  Config RelWithDebInfo (-O2 -g / clang-cl /O2 /Zi): the .pdb files come from the same build, so they match the"
  echo "  player binaries exactly."
  echo
  echo "$NAME.zip (players, mappers, listen servers)"
  echo "  syndicate/            the complete game folder ($(count "$P/syndicate") files): Windows and Linux client and server"
  echo "                        libraries, maps, sprites, sounds, configs, models, fonts, syndicate.fgd, ..."
  echo "  syndicate_addon/      empty but for readme.txt: the players' own content (Options, \"Allow custom addon content\")"
  echo "manifest.sha256 (not in a zip): sha256 of every file in syndicate/ ($manifest_lines lines), for scripts/verify.sh /"
  echo "  verify.ps1 (read only)"
  if [ -n "$AMXX" ]; then
    echo "$NAME-server.zip (dedicated servers, crash analysis)"
    echo "  server/syndicate/     dlls/hl.so (with debug info), dlls/hl.dll + hl.pdb, and addons/ (server plugin data"
    echo "                        for THIS hl.so, $(count "$S/server/syndicate/addons") files)"
    echo "  server/symbols/       crash analysis only, not for installing: cl_dlls/client.so (with debug info),"
    echo "                        client.dll + client.pdb (the same client.dll as in syndicate/)"
  fi
  echo
  echo "INSTALL (client or listen server), Windows"
  echo "  1. Copy Half-Life\\syndicate\\config.cfg somewhere safe (your keys and settings; this build has no config.cfg)."
  echo "  2. Delete (or rename as a backup) Half-Life\\syndicate."
  echo "  3. Copy this syndicate folder into Half-Life\\."
  echo "  4. Put your config.cfg back into Half-Life\\syndicate. Without one, the first start gives you SBO's default"
  echo "     keys, name Sadako and running speed (default.cfg) on top of your own Half-Life settings, and the game writes a"
  echo "     new config.cfg."
  echo "  5. Half-Life\\syndicate_addon (with \"Allow custom addon content\" on) and syndicate_hd are read BEFORE syndicate. For a clean test,"
  echo "     rename them too: a stale hud.txt, sprite, menu file, overview or model in there replaces the one in this build."
  echo "  Check afterwards: scripts/verify.ps1 (PowerShell) with this manifest.sha256 next to it, e.g. .\\verify.ps1 \"D:\\path\\to\\Half-Life\""
  echo "INSTALL, Linux (native client)"
  echo "  Same steps with ~/.local/share/Steam/steamapps/common/Half-Life/syndicate."
  echo "  Check afterwards: scripts/verify.sh \"/path/to/Half-Life\" manifest.sha256"
  echo "INSTALL, server"
  if [ -n "$AMXX" ]; then
    echo "  The syndicate folder as above, then copy server/syndicate from $NAME-server.zip over it (hl.so with debug"
    echo "  info, hl.dll + hl.pdb, addons/ for this hl.so)."
  else
    echo "  The syndicate folder as above (dlls/hl.so, dlls/hl.dll)."
  fi
  echo "  Metamod on Linux: addons/metamod/config.ini needs the line \"gamedll dlls/hl.so\" (1.38: dlls/hl_i386.so), or"
  echo "  start with +localinfo mm_gamedll dlls/hl.so. Without it the server stops with \"Game DLL version mismatch\"."
  echo "  Renaming hl.so to hl_i386.so instead does not help: Metamod then loads Half-Life, not SBO."
  echo "COPYING OVER AN EXISTING INSTALL instead of replacing the folder: files removed since your build stay behind;"
  echo "  verify.sh / verify.ps1 do not list them. When in doubt, replace the whole folder."
  echo
  echo "CHECKS"
  echo "  scripts/check_no_test_commands.sh: all player and server binaries pass."
  echo "  verify.sh against the package's own syndicate folder: $verify_out"
  echo "  Both zips: no .git* / dot files, editor or build leftovers (checked by scripts/package_full_release.sh)."
  echo
  echo "BINARIES"
  for f in "${BINS[@]}"; do
    printf '  %-40s %9s  sha256 %s\n' "$f" "$(stat -c %s "$P/$f")" "$(sha256sum < "$P/$f" | cut -d' ' -f1)"
  done
  for f in "${SBINS[@]}"; do
    printf '  %-40s %9s  sha256 %s\n' "$f" "$(stat -c %s "$S/$f")" "$(sha256sum < "$S/$f" | cut -d' ' -f1)"
  done
} > "$OUT/build.txt"
cat "$OUT/build.txt"

echo "==== zip ===="
make_zip() {  # <dir> <zip>
  (cd "$1" && find . -type f -printf '%P\n' | LC_ALL=C sort | zip -X -q -@ "$2")
}
make_zip "$P" "$OUT/$NAME.zip"
ZIPS=("$NAME.zip")
if [ -n "$AMXX" ]; then make_zip "$S" "$OUT/$NAME-server.zip"; ZIPS+=("$NAME-server.zip"); fi

# the updater's release files (docs/UPDATER.md): manifest.json = every syndicate/ file of the player zip with its place
# in the zip; signed when the signing key is there (CI secret SBO_UPDATE_SIGNING_KEY)
rm -f "$OUT/manifest.json" "$OUT/manifest.json.sig"
python3 "$ROOT/scripts/updater_release.py" manifest "$OUT/$NAME.zip" --version "$TAG_VERSION" \
  --commit "$(git -C "$SRC" -c safe.directory="$SRC" rev-parse --short=8 HEAD)" \
  ${UPDATE_REMOVE_LIST:+--remove "$UPDATE_REMOVE_LIST"} ${UPDATE_PREVIOUS:+--previous "$UPDATE_PREVIOUS"} \
  -o "$OUT/manifest.json"
if [ -n "${SBO_UPDATE_SIGNING_KEY:-}" ]; then
  python3 "$ROOT/scripts/updater_release.py" sign "$OUT/manifest.json" --key-env SBO_UPDATE_SIGNING_KEY
else
  echo "manifest.json NOT signed (no SBO_UPDATE_SIGNING_KEY): sign it before publishing (docs/UPDATER.md)"
fi

# Leftover check on what the zips really hold: any dot file / dot folder (.git*, .vscode, .DS_Store, ...), editor
# backups and swap files, build outputs (CMake / Ninja / objects / logs / linker maps / Python caches).
LEFTOVER='(^|/)\.|~$|\.(swp|swo|bak|orig|rej|tmp|log|o|obj|ilk|exp|lib|a|pyc|ninja|debug)$|(^|/)(__pycache__|CMakeFiles|cmake|logs|\.vs|\.idea|\.vscode)/|(^|/)(CMakeCache\.txt|Thumbs\.db|desktop\.ini|hashes\.txt)$|(^|/)(dlls|cl_dlls)/[^/]*\.map$'
bad=0
for z in "${ZIPS[@]/#/$OUT/}"; do
  hits=$(unzip -Z1 "$z" | grep -E "$LEFTOVER" || true)
  if [ -n "$hits" ]; then echo "$(basename "$z"): leftover files:" >&2; while IFS= read -r h; do echo "  $h"; done <<< "$hits" >&2; bad=1; fi
  echo "$(basename "$z"): $(unzip -Z1 "$z" | wc -l) files, $(stat -c %s "$z") bytes"
done
[ $bad -eq 0 ] || { echo "release zips contain .git* / editor / build leftovers" >&2; exit 1; }
top=$(unzip -Z1 "$OUT/$NAME.zip" | sed 's|/.*||' | LC_ALL=C sort -u | tr '\n' ' ')
[ "$top" = "syndicate syndicate_addon " ] || { echo "$NAME.zip holds more than syndicate/ and syndicate_addon/: $top" >&2; exit 1; }
bash "$ROOT/scripts/check_lowercase.sh" --zip "$OUT/$NAME.zip" \
  || { echo "the player zip contains names with upper-case letters" >&2; exit 1; }
(cd "$OUT" && sha256sum "${ZIPS[@]}" > SHA256SUMS && cat SHA256SUMS)
[ -n "${KEEP_STAGE:-}" ] || rm -rf "$STAGE"
