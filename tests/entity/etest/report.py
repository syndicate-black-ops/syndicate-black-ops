"""Summary table, report.json and junit.xml."""

import json
import os
from xml.sax.saxutils import escape, quoteattr


def merge(runs_results):
    """Results of the same suite over several runs (CI mode): a test that passes in one run and fails in another
    is "flaky" and counts as a failure."""
    merged = []
    for i, first in enumerate(runs_results[0]):
        states = [rr[i] for rr in runs_results]
        oks = [s.ok for s in states]
        if all(oks):
            status = 'pass'
        elif not any(oks):
            status = 'FAIL'
        else:
            status = 'FLAKY'
        if first.test.xfail:
            status = {'pass': 'XPASS', 'FAIL': 'xfail', 'FLAKY': 'FLAKY'}[status]
        failures = []
        for n, s in enumerate(states):
            for f in s.failures:
                failures.append(('run %d: ' % (n + 1) if len(states) > 1 else '') + f)
        notes = []
        for n, s in enumerate(states):
            notes += [('run %d: ' % (n + 1) if len(states) > 1 else '') + x for x in s.notes]
        merged.append((first.test, status, failures, notes))
    return merged


def table(suite_rows):
    lines = []
    for suite, rows, problems, wall in suite_rows:
        npass = sum(1 for r in rows if r[1] in ('pass', 'xfail'))
        lines.append('== suite %s: %d/%d pass%s (%.1f s)' % (suite.name, npass, len(rows),
                                                            ', SERVER PROBLEMS' if problems else '', wall))
        for test, status, failures, notes in rows:
            lines.append('  %-5s %-4s %s' % (status, test.id, test.title))
            if test.xfail:
                lines.append('             known bug: %s' % test.xfail)
            for f in failures[:6]:
                lines.append('             - %s' % f)
            for n in notes[:6]:
                lines.append('             . %s' % n)
            if failures and test.doc:
                lines.append('             (docs/%s)' % test.doc)
        for p in problems[:20]:
            lines.append('  SERVER %s' % p)
    return '\n'.join(lines)


def write_json(path, suite_rows):
    out = []
    for suite, rows, problems, wall in suite_rows:
        out.append({'suite': suite.name, 'wall': round(wall, 2), 'problems': problems,
                    'tests': [{'id': t.id, 'key': t.key, 'title': t.title, 'doc': t.doc, 'status': s, 'failures': f,
                               'notes': n} for t, s, f, n in rows]})
    with open(path, 'w') as f:
        json.dump(out, f, indent=1)


def write_junit(path, suite_rows):
    parts = ['<?xml version="1.0" encoding="UTF-8"?>\n<testsuites>\n']
    for suite, rows, problems, wall in suite_rows:
        nfail = sum(1 for r in rows if r[1] not in ('pass', 'xfail')) + (1 if problems else 0)
        parts.append('<testsuite name=%s tests="%d" failures="%d" time="%.2f">\n' % (
            quoteattr('entity.' + suite.name), len(rows) + 1, nfail, wall))
        for t, s, f, _n in rows:
            parts.append('<testcase classname=%s name=%s>' % (quoteattr('entity.' + suite.name),
                                                              quoteattr('%s %s' % (t.id, t.title))))
            if s not in ('pass', 'xfail'):
                parts.append('<failure message=%s>%s</failure>' % (quoteattr(s + ': ' + (f[0] if f else '')),
                                                                   escape('\n'.join(f))))
            parts.append('</testcase>\n')
        parts.append('<testcase classname=%s name="server">' % quoteattr('entity.' + suite.name))
        if problems:
            parts.append('<failure message=%s>%s</failure>' % (quoteattr(problems[0]), escape('\n'.join(problems))))
        parts.append('</testcase>\n</testsuite>\n')
    parts.append('</testsuites>\n')
    with open(path, 'w') as f:
        f.write(''.join(parts))
