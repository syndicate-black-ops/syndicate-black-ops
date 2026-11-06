#!/usr/bin/env python3
"""Summarize clang-tidy / cppcheck output and compare it with the triaged baseline (scripts/check_sanitizers.sh static).

  report.py <root> <out dir> --tidy <log> --cppcheck <log> --warnings <log> [--baseline <tsv>] [--update-baseline]

Writes <out dir>/findings.tsv (every finding, deduplicated: tool, check, file, line, message) and prints the findings
that are not in the baseline (tests/static/baseline.tsv: tool, check, file, message; no line numbers, so edits
elsewhere in a file don't bring triaged findings back). A finding that is in the baseline was looked at:
noise, or a bug proposed / left for a decision. Exit status 1 when
there are new findings.

  --update-baseline   rewrites the baseline with the current findings (after triage only)
"""
import argparse, collections, os, re, sys

# checks that only report style in this code base: summarized, never listed one by one
QUIET = {
    'cert-err34-c',                      # atoi / sscanf without error reporting: the SDK's KeyValue idiom
    'clang-analyzer-deadcode.DeadStores',
    'bugprone-suspicious-include',
    'bugprone-undefined-memory-manipulation',  # memset / memcpy of the SDK's plain structs (Vector members)
    'bugprone-incorrect-roundings',      # (int)(x + 0.5) on positive screen values
    'cert-flp30-c', 'clang-analyzer-security.FloatLoopCounter',
}

LINE = re.compile(r'^([^:\s][^:]*):(\d+):(\d+): (warning|error|style|portability|performance|information): (.*?) \[([\w.,-]+)\]\s*$')


def norm_msg(m):
    # numbers inside messages (offsets, sizes) change with unrelated edits
    return re.sub(r'\d+', 'N', m)


def parse(root, path, tool):
    out = set()
    if not os.path.exists(path):
        return out
    for line in open(path, errors='replace'):
        m = LINE.match(line.rstrip('\n'))
        if not m:
            continue
        f, ln, _col, _sev, msg, check = m.groups()
        f = os.path.normpath(f if os.path.isabs(f) else os.path.join(root, f))
        if not f.startswith(root + '/src/'):
            continue
        rel = os.path.relpath(f, root)
        out.add((tool, check, rel, int(ln), msg))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('root'); ap.add_argument('outdir')
    ap.add_argument('--tidy', default=''); ap.add_argument('--cppcheck', default=''); ap.add_argument('--warnings', default='')
    ap.add_argument('--baseline'); ap.add_argument('--update-baseline', action='store_true')
    a = ap.parse_args()
    root = os.path.normpath(a.root)
    outdir, update = a.outdir, a.update_baseline
    baseline = a.baseline or os.path.join(root, 'tests/static/baseline.tsv')

    findings = sorted(parse(root, a.tidy, 'clang-tidy') | parse(root, a.cppcheck, 'cppcheck') | parse(root, a.warnings, 'clang'),
                      key=lambda x: (x[2], x[3], x[1]))
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, 'findings.tsv'), 'w') as fh:
        for t, c, f, ln, msg in findings:
            fh.write(f'{t}\t{c}\t{f}\t{ln}\t{msg}\n')

    keys = collections.Counter((t, c, f, norm_msg(msg)) for t, c, f, ln, msg in findings)
    # the baseline: tool, check, file, message (digits as N), count[, reason] -- a hand-added false alarm carries its
    # reason in the sixth column; --update-baseline keeps the reasons of the entries that are still there
    base = collections.Counter()
    reasons = {}
    if os.path.exists(baseline):
        for line in open(baseline):
            if line.startswith('#') or not line.strip():
                continue
            parts = line.rstrip('\n').split('\t')
            t, c, f, msg, n = parts[:5]
            base[(t, c, f, msg)] = int(n)
            if len(parts) > 5 and parts[5]:
                reasons[(t, c, f, msg)] = parts[5]
    if update:
        with open(baseline, 'w') as fh:
            fh.write('# tool\tcheck\tfile\tmessage (digits as N)\tcount\treason -- triaged findings (false alarms and '
                     'style). New warnings only are reported. A new '
                     'false alarm: add its line with a reason; a real bug: fix it, never add it here. Regenerate with '
                     'report.py --update-baseline only after triage (reasons are kept).\n')
            for k, n in sorted(keys.items()):
                t, c, f, msg = k
                fh.write(f'{t}\t{c}\t{f}\t{msg}\t{n}\t{reasons.get(k, "")}\n')
        print(f'baseline: {len(keys)} entries -> {baseline}')
        return 0


    by_check = collections.Counter(c for _t, c, _f, _ln, _m in findings)
    print('findings per check:')
    for c, n in by_check.most_common():
        print(f'  {n:6d}  {c}')

    new = []
    for t, c, f, ln, msg in findings:
        k = (t, c, f, norm_msg(msg))
        if base[k] > 0:
            base[k] -= 1
            continue
        new.append((t, c, f, ln, msg))
    loud = [x for x in new if x[1] not in QUIET]
    print(f'\n{len(findings)} findings, {len(new)} not in the baseline ({len(loud)} outside the quiet checks)')
    for t, c, f, ln, msg in loud:
        print(f'NEW {f}:{ln}: [{c}] {msg}')
    return 1 if loud else 0


if __name__ == '__main__':
    sys.exit(main())
