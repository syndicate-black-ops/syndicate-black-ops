#!/usr/bin/env bash
# Fail on a folder or file name with an upper-case letter in what we ship (every name lower case;
# Linux file systems are case-sensitive, so a name must match its references exactly).
#
#   scripts/check_lowercase.sh                  the git-tracked files of the shipped trees: gamedir/, sdk/syndicate.fgd,
#                                               sdk/editor_assets.md, sdk/examples/, sdk/prefabs/ (CI job check-names)
#   scripts/check_lowercase.sh --zip <zip>...   every entry of a zip: the player package (scripts/package_full_release.sh)
#   scripts/check_lowercase.sh --dir <dir>...   every file and folder under <dir> (a package stage, a dist folder)
#
# Base game files only: the server package (addons/, AMX Mod X gamedata) and the
# plugins are not checked.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)

list() {
  case "${1:-}" in
    '')
      git -C "$ROOT" -c safe.directory="$ROOT" -c core.quotepath=off -c submodule.recurse=false ls-files -- \
        gamedir sdk/syndicate.fgd sdk/editor_assets.md sdk/examples sdk/prefabs ;;
    --zip)
      shift
      for z in "$@"; do unzip -Z1 "$z" | awk -v s="$(basename "$z")" '{print s "\t" $0}' || exit 2; done ;;
    --dir)
      shift
      for d in "$@"; do (cd "$d" && find . -mindepth 1 -printf '%P\n') | awk -v s="$d" '{print s "\t" $0}' || exit 2; done ;;
    *)
      sed -n '2,12p' "$0" >&2; exit 2 ;;
  esac
}

out=$(list "$@") || exit 2
# "<zip or dir>\t<path>" (--zip, --dir) or "<path>": only the path inside is checked, every folder and the file
bad=$(printf '%s\n' "$out" | sed '/^$/d' | LC_ALL=C awk -F'\t' '{
  p = $NF
  if (p ~ /[A-Z]/) print (NF > 1 ? $1 ": " p : p)
}')
if [ -n "$bad" ]; then
  echo "Names with upper-case letters (every shipped folder and file name must be lower case; rename with git mv," >&2
  echo "fix every reference, and see docs/UPDATER.md for case-only renames):" >&2
  printf '%s\n' "$bad" | sed 's/^/  /' >&2
  exit 1
fi
echo "check_lowercase: $(printf '%s\n' "$out" | sed '/^$/d' | wc -l) names, all lower case"
