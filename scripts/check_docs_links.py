#!/usr/bin/env python3
"""Fast check of the docs site's Markdown links, so a commit can't leave the site unbuildable.

Docusaurus stops the build on a broken link, image or anchor. This finds the same faults in seconds, without a build:
every relative link or image in docs-site/docs, entity-text and templates must point at a file that exists (or an
entity page the generator makes from the FGD), and a #anchor must match a heading or an explicit {#id} of that page.

  scripts/check_docs_links.py            every page
  scripts/check_docs_links.py --staged   only pages staged for commit (the pre-commit hook)
Exit status 1 lists each broken link as file:line.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITE = os.path.join(ROOT, 'docs-site')
DOCS = os.path.join(SITE, 'docs')
ENTITIES = os.path.join(DOCS, 'mapping', 'entities')
SOURCES = [DOCS, os.path.join(SITE, 'entity-text'), os.path.join(SITE, 'templates')]

LINK = re.compile(r'!?\[(?:[^\]\[]|\[[^\]]*\])*\]\(\s*<?([^)\s>]+)>?(?:\s+"[^"]*")?\s*\)')
HTML_SRC = re.compile(r'<(?:img|a|source|video)\b[^>]*?\b(?:src|href)=["\']([^"\']+)["\']', re.I)
HEADING = re.compile(r'^(#{1,6})\s+(.*?)\s*$')
EXPLICIT_ID = re.compile(r'\{#([\w-]+)\}')
JSX_ID = re.compile(r'\bid=["\']([\w-]+)["\']')
JSX_ANCHORS = re.compile(r'\banchors=["\']([^"\']+)["\']')          # <Keyvalue anchors="a b">
INLINE_CODE = re.compile(r'`[^`]*`')
FENCE = re.compile(r'^\s*(```|~~~)')


def fgd_classes():
    text = open(os.path.join(ROOT, 'sdk', 'syndicate.fgd'), encoding='latin-1').read()
    return set(re.findall(r'^@\w+Class\b[^=]*=\s*([A-Za-z_]\w*)', text, re.M))


def slug(text):
    """github-slugger, as Docusaurus uses it: lower case, drop punctuation, spaces to hyphens."""
    text = re.sub(r'\[\[[^\]]*\]\]', '', text)              # [[ported:x]] badges
    text = re.sub(r'`([^`]*)`', r'\1', text)
    text = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text)    # links keep their text
    text = re.sub(r'<[^>]+>', '', text).strip().lower()
    text = re.sub(r'[^\w\- ]', '', text)
    return text.replace(' ', '-')


_anchor_cache = {}


def anchors(path):
    if path not in _anchor_cache:
        found, counts, fenced = set(), {}, False
        try:
            lines = open(path, encoding='utf-8').read().split('\n')
        except OSError:
            lines = []
        for line in lines:
            if FENCE.match(line):
                fenced = not fenced
                continue
            if fenced:
                continue
            found.update(JSX_ID.findall(line))
            for names in JSX_ANCHORS.findall(line):
                found.update(names.split())
            m = HEADING.match(line)
            if not m:
                continue
            explicit = EXPLICIT_ID.search(m.group(2))
            if explicit:
                found.add(explicit.group(1))
                continue
            base = slug(m.group(2))
            n = counts.get(base, 0)
            counts[base] = n + 1
            found.add(base if n == 0 else f'{base}-{n}')
        _anchor_cache[path] = found
    return _anchor_cache[path]


def target_ok(src, url, classes):
    """None when the link resolves, else the reason."""
    if re.match(r'^[a-z][a-z0-9+.-]*:', url, re.I) or url.startswith(('//', '/')) or url.startswith('{'):
        return None                                           # external, site-absolute or JSX expression
    path, _, anchor = url.partition('#')
    path = path.split('?')[0]
    if not path:                                              # same-page anchor
        dest = src
    else:
        base = os.path.dirname(src)
        if src.startswith(os.path.join(SITE, 'entity-text')) or src.startswith(os.path.join(SITE, 'templates')):
            base = ENTITIES                                   # those texts are rendered as entity pages
        dest = os.path.normpath(os.path.join(base, path))
        if not os.path.exists(dest):
            if os.path.dirname(dest) == ENTITIES and dest.endswith('.md') and os.path.basename(dest)[:-3] in classes:
                return None                                   # generated entity page, not on disk until generated
            return 'no such file'
    if anchor and dest.endswith(('.md', '.mdx')) and os.path.exists(dest):
        if anchor not in anchors(dest):
            return f'no heading #{anchor}'
    return None


def files(staged):
    if staged:
        def changed(flt):
            return subprocess.run(['git', 'diff', '--cached', '--name-only', f'--diff-filter={flt}', '--', 'docs-site'],
                                  cwd=ROOT, capture_output=True, text=True).stdout.split()
        if not changed('DR'):                                 # a removed or moved file can break any page's link
            return [os.path.join(ROOT, f) for f in changed('ACM') if f.endswith(('.md', '.mdx'))]
    result = []
    for top in SOURCES:
        for d, _, names in os.walk(top):
            result += [os.path.join(d, n) for n in names if n.endswith(('.md', '.mdx'))]
    return result


def main():
    staged = '--staged' in sys.argv[1:]
    classes = fgd_classes()
    bad = 0
    for f in sorted(files(staged)):
        if not os.path.exists(f):
            continue
        fenced = False
        for i, line in enumerate(open(f, encoding='utf-8').read().split('\n'), 1):
            if FENCE.match(line):
                fenced = not fenced
                continue
            if fenced:
                continue
            line = INLINE_CODE.sub('', line)                  # code samples are text, not links
            for url in LINK.findall(line) + HTML_SRC.findall(line):
                why = target_ok(f, url, classes)
                if why:
                    print(f'{os.path.relpath(f, ROOT)}:{i}: {url}: {why}', file=sys.stderr)
                    bad += 1
    if bad:
        print(f'{bad} broken docs link(s). The docs site will not build until they are fixed.', file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
