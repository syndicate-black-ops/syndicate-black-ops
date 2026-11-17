#!/usr/bin/env bash
# The offline manual: the docs site built with SBO_DOCS_MANUAL=1 (docs-site/docusaurus.config.ts), which the game's
# release zip carries as syndicate/manual/ (scripts/package_full_release.sh MANUAL_DIR). The website's workflow builds
# it and publishes the zip; the game's release workflow downloads it.
#
#   scripts/docs_manual.sh check <manual dir>                    the checks below; exit 1 on a problem
#   scripts/docs_manual.sh pack <manual dir> <out dir> <version> check, then <out dir>/sbo-manual-v<version>.zip
#                                                                (the files at the zip's top level) + SHA256SUMS
#
# Checks: index.html and search-index.js are there; only page, script, style, picture and font files (ALLOWED: no
# zip, archive, program or any other download a page might link, e.g. the Useful tools downloads, which are release
# files of the website repository); no dot file, link or upper-case name (check_lowercase.sh rule of the player zip);
# at most MANUAL_MAX_MB (default 40) MB in all.
set -euo pipefail
ALLOWED='html|js|css|png|webp|gif|jpg|jpeg|svg|ico|woff2'
MAX_MB=${MANUAL_MAX_MB:-40}

check() {  # <dir>
  local d=$1 bad=0 bytes list
  [ -f "$d/index.html" ] || { echo "manual: no index.html in $d" >&2; return 1; }
  [ -f "$d/search-index.js" ] || { echo "manual: no search-index.js in $d (built without SBO_DOCS_MANUAL=1?)" >&2; return 1; }
  list=$(cd "$d" && find . -mindepth 1 -printf '%y %P\n')
  local hits
  hits=$(grep -vE '^d ' <<< "$list" | grep -vE "^f [^ ].*\.($ALLOWED)\$" || true)
  [ -z "$hits" ] || { echo "manual: files of a type the manual must not carry (allowed: $ALLOWED):" >&2; sed 's/^/  /' <<< "$hits" | head -40 >&2; bad=1; }
  hits=$(sed 's/^. //' <<< "$list" | grep -E '(^|/)\.' || true)
  [ -z "$hits" ] || { echo "manual: dot files:" >&2; sed 's/^/  /' <<< "$hits" | head -20 >&2; bad=1; }
  hits=$(sed 's/^. //' <<< "$list" | grep '[A-Z]' || true)
  [ -z "$hits" ] || { echo "manual: names with upper-case letters:" >&2; sed 's/^/  /' <<< "$hits" | head -20 >&2; bad=1; }
  bytes=$(cd "$d" && find . -type f -printf '%s\n' | awk '{s += $1} END {print s + 0}')
  if [ "$bytes" -gt $((MAX_MB * 1024 * 1024)) ]; then
    echo "manual: $((bytes / 1024 / 1024)) MB, more than MANUAL_MAX_MB=$MAX_MB" >&2; bad=1
  fi
  [ $bad -eq 0 ] || return 1
  echo "manual: $(grep -c '^f ' <<< "$list") files, $((bytes / 1024)) KB, check OK"
}

case "${1:-}" in
  check)
    [ $# -eq 2 ] || { sed -n '2,14p' "$0" >&2; exit 2; }
    check "$2" ;;
  pack)
    [ $# -eq 4 ] || { sed -n '2,14p' "$0" >&2; exit 2; }
    dir=$(cd "$2" && pwd); ver=$4
    [[ $ver =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version $ver is not x.y.z" >&2; exit 2; }
    check "$dir"
    mkdir -p "$3" && out=$(cd "$3" && pwd)
    zip=sbo-manual-v$ver.zip
    rm -f "$out/$zip" "$out/SHA256SUMS"
    (cd "$dir" && find . -type f -printf '%P\n' | LC_ALL=C sort | zip -X -q -@ "$out/$zip")
    (cd "$out" && sha256sum "$zip" > SHA256SUMS && cat SHA256SUMS)
    echo "$out/$zip: $(stat -c %s "$out/$zip") bytes" ;;
  *)
    sed -n '2,14p' "$0" >&2; exit 2 ;;
esac
