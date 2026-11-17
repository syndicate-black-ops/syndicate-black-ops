#!/usr/bin/env python3
"""A change that is described in more than one place uses the same words everywhere.

The change lines live in CHANGELOG.md (every release), docs-site/docs/changes.md and the "Changes since 1.38"
section of each docs-site page and entity-text page. A change described in two of these files must have identical
text in both: CHANGELOG.md's wording wins where it has the change, otherwise changes.md's. This check finds pairs of
bullets in different files that describe the same change in different words (near-duplicates) and prints both
lines as file:line.

Compared after normalizing: a Markdown link [text](target) counts as its text (page links mean nothing in
CHANGELOG.md, which the in-game update dialog and GitHub show), <kbd>X</kbd> counts as X, and case, trailing
punctuation and runs of spaces are ignored. `code` spans and every word must match.

A pair is two lines that are not identical but are near-duplicates: one is the other plus more sentences, or their
word-level difflib similarity (without the shared "This was a bug in SBO 1.38" sentence) is at least THRESHOLD, or at
least SUBJECT_THRESHOLD when they share at least SUBJECT_OVERLAP of the names they are about (`code`, **bold** and
capitalized names). Never a pair: two lines about different names, two lines with different `code` names (a summary
and one of its parts), or two entity pages that don't name each other's entity. Pairs that really are two different
changes go in scripts/changes_sync_allow.txt, one pair per line: "<normalized line A> || <normalized line B> # reason".

  scripts/check_changes_sync.py            every file
  scripts/check_changes_sync.py --staged   only when CHANGELOG.md or a docs-site file is staged (pre-commit)
Exit status 1 lists each near-duplicate pair.
"""
import difflib
import itertools
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOW = os.path.join(ROOT, 'scripts', 'changes_sync_allow.txt')
THRESHOLD = 0.6
SUBJECT_THRESHOLD = 0.36     # a lower similarity counts when both lines name mostly the same things
SUBJECT_OVERLAP = 0.5

CHANGES_HEADING = re.compile(r'^#{2,6}\s+Changes since 1\.38\b')
HEADING = re.compile(r'^#{1,6}\s')
BULLET = re.compile(r'^( {0,2})- (.*)$')
LINK = re.compile(r'\[((?:[^\]\[]|\[[^\]]*\])*)\]\([^)]*\)')
KBD = re.compile(r'</?kbd>')
COMMON_NAMES = {'sbo', 'co-op', 'battle', 'royale', 'bounty', 'killer', 'half-life', 'fixed', 'the', 'a', 'an', 'in',
                'when', 'if', 'while', 'with', 'this', 'it', 'its', 'new', 'players', 'you', 'your', 'options',
                'advanced', 'game', 'master', 'hd'}


def subjects(text):
    """The names a change is about: `code` spans, **bold** names and capitalized words inside the sentence."""
    text = LINK.sub(r'\1', text)
    names = {c.lower() for c in re.findall(r'`([^`]+)`', text)}
    names |= {b.lower() for b in re.findall(r'\*\*([^*]+)\*\*', text)}
    words = re.findall(r"[A-Za-z][\w'-]*", re.sub(r'`[^`]*`', ' ', text))
    names |= {w.lower() for w in words[1:] if w[0].isupper() and w.lower() not in COMMON_NAMES}
    return names


# sentences many change lines share; they say nothing about which change a line is
BOILERPLATE = re.compile(r"\s*(this was a bug in sbo 1\.38[^.]*|this comes from [^.]*|this protection comes from [^.]*)"
                         r"(\.|$)")


def codes(text):
    """The `code` names of a line (entities, keys, cvars)."""
    return {c.lower() for c in re.findall(r'`([^`]+)`', LINK.sub(r'\1', text))}


def norm(text):
    text = LINK.sub(r'\1', text)
    text = KBD.sub('', text)
    text = re.sub(r'\s+', ' ', text).strip().rstrip('.:;,').lower()
    return text


def bullets(path, whole):
    """(line number, text) of each bullet in the change sections of a Markdown file."""
    out, cur, inside, fence = [], None, whole, False
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f.read().split('\n'), 1):
            if re.match(r'^\s*(```|~~~)', line):
                fence = not fence
            if fence:
                continue
            if HEADING.match(line):
                inside = whole or bool(CHANGES_HEADING.match(line))
                cur = None
                continue
            if not inside:
                continue
            m = BULLET.match(line)
            if m:
                cur = [n, m.group(2)]
                out.append(cur)
            elif cur and line.startswith('  ') and line.strip():
                cur[1] += ' ' + line.strip()
            else:
                cur = None
    return out


def corpus():
    files = [('CHANGELOG.md', True), ('docs-site/docs/changes.md', True)]
    for base in ('docs-site/docs', 'docs-site/entity-text'):
        for dirpath, _, names in os.walk(os.path.join(ROOT, base)):
            for name in sorted(names):
                rel = os.path.relpath(os.path.join(dirpath, name), ROOT)
                if name.endswith('.md') and '/mapping/entities/' not in rel and rel != 'docs-site/docs/changes.md':
                    files.append((rel, False))
    items = []
    for rel, whole in files:
        for n, text in bullets(os.path.join(ROOT, rel), whole):
            t = norm(text)
            if len(t) >= 20:        # sub-bullet fragments ("`env_state` did ...") are too short to compare
                items.append((rel, n, t, subjects(text), codes(text)))
    return items


STEM_PREFIX = re.compile(r'^(monster|func|env|trigger|info|game|xen|item|weapon|ammo|player|path|calc|locus|multi|'
                         r'scripted|momentary|ambient|point|light|vgui)_')


def entity_of(rel):
    """An entity-text page's class name and its short form (monster_apache: apache), else None."""
    if not rel.startswith('docs-site/entity-text/'):
        return None
    name = os.path.splitext(os.path.basename(rel))[0].lower()
    return name, STEM_PREFIX.sub('', name)


def mentions(text, entity):
    name, stem = entity
    return bool(re.search(r'\b(%s|%s)s?\b' % (re.escape(name), re.escape(stem.replace('_', ' '))), text))


def allowed():
    pairs = set()
    if os.path.exists(ALLOW):
        with open(ALLOW, encoding='utf-8') as f:
            for line in f:
                line = line.split(' # ')[0].strip()
                if ' || ' in line and not line.startswith('#'):
                    a, b = line.split(' || ', 1)
                    pairs.add(frozenset((a.strip(), b.strip())))
    return pairs


def near_duplicates(items):
    allow = allowed()
    found = []
    for (fa, la, ta, sa, ca), (fb, lb, tb, sb, cb) in itertools.combinations(items, 2):
        if fa == fb or ta == tb:
            continue
        # one line names more entities or cvars than the other: a summary and one of its parts, or two lists
        if ca and cb and ca != cb:
            continue
        # the same sentence about two different things (two entities, two weapons, two versions) is two changes
        if sa and sb and not (sa & sb):
            continue
        # two entity pages: the same wording about two entities is two changes, unless one names the other
        ea, eb = entity_of(fa), entity_of(fb)
        if ea and eb and ea != eb and not mentions(ta, eb) and not mentions(tb, ea):
            continue
        if frozenset((ta, tb)) in allow:
            continue
        short, long_ = sorted((ta, tb), key=len)
        if long_.startswith(short):     # one line is the other plus more sentences
            found.append((difflib.SequenceMatcher(None, ta.split(), tb.split()).ratio(), fa, la, ta, fb, lb, tb))
            continue
        if len(BOILERPLATE.sub('', short).split()) < 4:     # too little left to compare
            continue
        overlap = len(sa & sb) / len(sa | sb) if sa and sb else 0
        need = SUBJECT_THRESHOLD if overlap >= SUBJECT_OVERLAP else THRESHOLD
        wa, wb = BOILERPLATE.sub('', ta).split(), BOILERPLATE.sub('', tb).split()
        m = difflib.SequenceMatcher(None, wa, wb, autojunk=False)
        if m.real_quick_ratio() < need or m.quick_ratio() < need:
            continue
        r = m.ratio()
        if r >= need:
            found.append((r, fa, la, ta, fb, lb, tb))
    return found


def main():
    if '--staged' in sys.argv[1:]:
        staged = subprocess.run(['git', '-C', ROOT, 'diff', '--cached', '--name-only'], capture_output=True,
                                text=True).stdout.split()
        if not any(p == 'CHANGELOG.md' or p.startswith('docs-site/') for p in staged):
            return 0
    found = near_duplicates(corpus())
    for r, fa, la, ta, fb, lb, tb in sorted(found, reverse=True):
        print('%.2f %s:%d\n     %s\n     %s:%d\n     %s' % (r, fa, la, ta, fb, lb, tb))
    if found:
        print('%d change line pair(s) describe the same change in different words. Use the same text in both: '
              "CHANGELOG.md's wording, else changes.md's (links and <kbd> may differ). Two different changes go in "
              'scripts/changes_sync_allow.txt with a reason.' % len(found))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
