#!/usr/bin/env bash
# Checks a Half-Life install's "syndicate" folder against a package's manifest.sha256: the shell counterpart of the
# package's verify.ps1 (same manifest: "<sha256>  <path relative to syndicate/>" per line, sha256sum's format).
#
#   ./verify.sh [Half-Life folder] [manifest.sha256]
#     Half-Life folder  default: ~/.local/share/Steam/steamapps/common/Half-Life
#     manifest          default: manifest.sha256 next to this script
#
# It only READS files. It reports files that are missing or different, and files in the override folders
# (syndicate_addon, syndicate_hd, syndicate_downloads, syndicate_<language>) that the game would load INSTEAD.
# Exit status: 0 when nothing is missing, different or shadowed, 1 otherwise, 2 on a usage error.
set -uo pipefail
HALFLIFE=${1:-$HOME/.local/share/Steam/steamapps/common/Half-Life}
MANIFEST=${2:-$(cd "$(dirname "$0")" && pwd)/manifest.sha256}
MOD=$HALFLIFE/syndicate
[ -d "$MOD" ] || { echo "Not found: $MOD" >&2; exit 2; }
[ -f "$MANIFEST" ] || { echo "No manifest: $MANIFEST" >&2; exit 2; }

missing=0; different=0; shadowed=0; ok=0
while IFS= read -r line || [ -n "$line" ]; do
  line=${line%$'\r'}
  [[ $line =~ ^([0-9a-f]{64})[[:space:]]+(.+)$ ]] || continue
  want=${BASH_REMATCH[1]}; rel=${BASH_REMATCH[2]}
  if [ ! -f "$MOD/$rel" ]; then
    echo "MISSING    $rel"; missing=$((missing + 1))
  else
    have=$(sha256sum < "$MOD/$rel" | cut -d' ' -f1)
    if [ "$have" != "$want" ]; then echo "DIFFERENT  $rel"; different=$((different + 1)); else ok=$((ok + 1)); fi
  fi
  for o in "$HALFLIFE"/syndicate_*/; do
    [ -d "$o" ] || continue
    if [ -e "$o$rel" ]; then
      echo "SHADOWED   $rel   by $(basename "$o")/$rel"; shadowed=$((shadowed + 1))
    fi
  done
done < "$MANIFEST"

echo
echo "$ok identical, $different different, $missing missing, $shadowed shadowed by an override folder."
echo "DIFFERENT / MISSING: copy the file from the package's syndicate folder."
echo "SHADOWED: the game loads the copy in the override folder instead; delete or rename that copy."
[ $((missing + different + shadowed)) -eq 0 ]
