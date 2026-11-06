#!/usr/bin/env bash
# Fail if a staged src/ file has LF-only lines while its committed (HEAD) version is pure CRLF (keeps diffs readable).
# Installed as .git/hooks/pre-commit; also runnable by hand.
#   scripts/check_eol.sh                 the staged files against HEAD (pre-commit)
#   scripts/check_eol.sh <base> <head>   the src/ files <head> changed since <base> (CI: a push or a pull request)
#   scripts/check_eol.sh --all           every tracked src/ file: CRLF and LF-only lines must not be mixed (CI: a commit
#                                        without a usable base, e.g. a repository's first commit)
if [ "${1:-}" = --all ]; then
  status=0
  while IFS= read -r -d '' f; do
    if perl -0777 -ne 'exit(/\r\n/ && /(?<!\r)\n/ ? 0 : 1)' "$f"; then
      echo "mixed CRLF / LF line endings in $f. Fix: sed -i 's/\\r*\$/\\r/' '$f'" >&2
      status=1
    fi
  done < <(git ls-files -z -- src)
  exit $status
fi
if [ $# -eq 2 ]; then OLD=$1 NEW=$2: DIFF=("$1" "$2"); else OLD=HEAD NEW=: DIFF=(--cached); fi
status=0
while IFS= read -r f; do
  if git cat-file -e "$OLD:$f" 2>/dev/null && git show "$OLD:$f" | perl -0777 -ne 'exit(/\r\n/ && !/(?<!\r)\n/ ? 0 : 1)' \
     && git show "$NEW$f" | perl -0777 -ne 'exit(/(?<!\r)\n/ ? 0 : 1)'; then
    echo "LF line endings in $f (committed version is CRLF). Fix: sed -i 's/\\r*\$/\\r/' '$f'" >&2
    status=1
  fi
done < <(git diff "${DIFF[@]}" --name-only --diff-filter=M -- src)
exit $status
