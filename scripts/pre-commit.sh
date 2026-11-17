#!/usr/bin/env bash
# The repository's pre-commit hook: .git/hooks/pre-commit is a link to this file
#   ln -sf ../../scripts/pre-commit.sh .git/hooks/pre-commit
# 1. src/ line endings (scripts/check_eol.sh)
# 2. docs site links, images and anchors in the staged pages (scripts/check_docs_links.py), so a commit can't leave
#    the docs site unbuildable. Never skip it with --no-verify: fix the link instead.
# 3. change lines (scripts/check_changes_sync.py) when CHANGELOG.md or a docs page is staged: a change described in
#    two places has the same words in both
dir=$(dirname "$(readlink -f "$0")")
status=0
bash "$dir/check_eol.sh" || status=1
if git diff --cached --name-only -- docs-site | grep -q .; then
  python3 -I "$dir/check_docs_links.py" --staged || status=1
fi
if git diff --cached --name-only -- docs-site CHANGELOG.md scripts/changes_sync_allow.txt | grep -q .; then
  python3 -I "$dir/check_changes_sync.py" || status=1
fi
exit $status
