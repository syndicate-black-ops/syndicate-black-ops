#!/usr/bin/env python3
"""Runs the entity test suites (docs/dev/ENTITY_TESTS.md). Use scripts/run_entity_tests.sh.

    run.py [suite ...] [--hl hl.so] [--hlds DIR] [--asan] [--ci] [--repeat N] [--timescale X] [--list]
           [--coverage [--coverage-out DIR]] [--coverage-gate]

Each suite: one generated map (tests/entity/suites/<suite>.py), compiled with ReSDHLT (cached), run on a throw-away
copy of a stock HLDS with the hl.so under test, checked against the trace the test layer writes. Output in
build/entity-tests/: report.json, junit.xml, per suite the map, steps, trace and server console.
"""

import argparse
import glob
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

from etest import build, check, report, server  # noqa: E402


def load_suites():
    import fgd
    classes, _ = fgd.parse_fgd(os.path.join(ROOT, 'sdk', 'syndicate.fgd'))
    suites = []
    for path in sorted(glob.glob(os.path.join(HERE, 'suites', '*.py'))):
        name = os.path.splitext(os.path.basename(path))[0]
        if name.startswith('_'):
            continue
        spec = importlib.util.spec_from_file_location('suite_' + name, path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        s = mod.SUITE
        s.set_fgd(classes, fgd.effective_props)
        suites.append(s)
    return suites, classes


def build_main_checkout():
    """The main checkout of a git worktree (the gitignored build/ map scan lives there), or ROOT."""
    import subprocess
    try:
        common = subprocess.run(['git', '-C', ROOT, 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(common)
    except (OSError, subprocess.CalledProcessError):
        return ROOT


def default_hl():
    for p in ('build/linux/hl.so', 'build/linux/cmake/hl.so'):
        p = os.path.join(ROOT, p)
        if os.path.exists(p):
            return p
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('suites', nargs='*')
    ap.add_argument('--hl', default=os.environ.get('ET_HL') or default_hl(), help='hl.so built with -DSBO_TEST_COMMANDS=ON')
    ap.add_argument('--hlds', default=os.environ.get('HLDS_DIR', os.path.expanduser('~/hlds')),
                    help='stock HLDS install (hlds_linux, valve/); env HLDS_DIR')
    ap.add_argument('--sdhlt', default=os.environ.get('SDHLT_DIR',
                                                      os.path.expanduser('~/ReSDHLT/tools')))
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'entity-tests'))
    ap.add_argument('--asan', action='store_true', help='run in the sniper image with libasan preloaded (ASan hl.so)')
    ap.add_argument('--ci', action='store_true', help='CI mode: every suite twice, a test that differs is FLAKY')
    ap.add_argument('--repeat', type=int, default=None)
    ap.add_argument('--timescale', type=float, default=float(os.environ['ET_TIMESCALE']) if 'ET_TIMESCALE' in os.environ
                    else None, help='game clock speed (tests/entity/timescale); default 5, or 2 on a loaded machine')
    ap.add_argument('--list', action='store_true', help='list the suites and tests')
    ap.add_argument('--ignore-requires', action='store_true', help="run suites whose branch code the hl.so lacks")
    ap.add_argument('--coverage', action='store_true',
                    help='FGD classes no test uses, and classes / keys / spawnflag bits / choice values set and asserted '
                         'by the entity suites and the map-test cases (etest/coverage.py)')
    ap.add_argument('--coverage-out', default=None, help='with --coverage: write per-item TSV tables to this folder')
    ap.add_argument('--coverage-gate', action='store_true',
                    help='fail unless every FGD item is asserted by a test or exempted in tests/fgd_exemptions.json '
                         '(etest/coverage.py gate)')
    a = ap.parse_args()

    suites, classes = load_suites()
    if a.suites:
        unknown = set(a.suites) - {s.name for s in suites}
        if unknown:
            sys.exit('unknown suite(s): %s (have: %s)' % (', '.join(sorted(unknown)), ', '.join(s.name for s in suites)))
        suites = [s for s in suites if s.name in a.suites]
    for s in suites:
        build.layout(s)
        build.run_tests(s)
        s.players = max([t.players for t in s.tests] + [0])

    if a.list:
        for s in suites:
            print('%s: %s (%d tests)' % (s.name, s.title, len(s.tests)))
            for t in s.tests:
                print('  %s %s' % (t.id, t.title))
        return 0
    if a.coverage_gate:
        import fgd
        from etest import coverage
        fails, lines = coverage.gate(fgd, classes, suites, os.path.join(ROOT, 'tests', 'maps', 'cases'),
                                     os.path.join(ROOT, 'tests', 'fgd_exemptions.json'))
        print('\n'.join(lines))
        return 1 if fails else 0
    if a.coverage:
        import fgd
        from etest import coverage
        cases_dir = os.path.join(ROOT, 'tests', 'maps', 'cases')
        used = {e.classname for s in suites for t in s.tests for e in t.ents} | coverage.case_classes(cases_dir)
        missing = [c for c, v in classes.items() if v['kind'] in ('pointclass', 'solidclass') and c not in used]
        print('%d of %d FGD entity classes have no test (entity suites and map-test cases):'
              % (len(missing), len(missing) + len(used & set(classes))))
        print('  ' + ' '.join(missing))
        scan = os.path.join(build_main_checkout(), 'build', 'map-entities', 'entities.jsonl')
        for line in coverage.report(fgd, classes, suites, cases_dir, map_scan=scan, out_dir=a.coverage_out):
            print(line)
        return 0

    if not a.hl or not os.path.exists(a.hl):
        sys.exit('no hl.so (built with -DSBO_TEST_COMMANDS=ON): give --hl')
    with open(a.hl, 'rb') as f:
        if b'SBO_TEST_COMMANDS test layer' not in f.read():
            sys.exit('%s has no test layer: build it with scripts/build.sh linux RelWithDebInfo -DSBO_TEST_COMMANDS=ON '
                     '(the option is off by default)' % a.hl)
    if not os.path.exists(os.path.join(a.hlds, 'hlds_linux')):
        sys.exit('no stock HLDS in %s: give --hlds or HLDS_DIR' % a.hlds)
    with open(a.hl, 'rb') as f:
        lib = f.read()
    skipped = [s for s in suites if not a.ignore_requires and any(r.encode() not in lib for r in s.requires)]
    for s in skipped:
        print('== suite %s: skipped (%s has no %s)' % (s.name, os.path.basename(a.hl), ', '.join(s.requires)))
    suites = [s for s in suites if s not in skipped]
    if a.timescale is None:
        # a loaded machine stretches every frame; at 5x a 50 ms frame is 0.25 s of game time (the engine's cap)
        load = os.getloadavg()[0]
        a.timescale = 5 if load < 0.75 * (os.cpu_count() or 4) else 2
        print('== timescale %g (load %.1f)' % (a.timescale, load))
    repeat = a.repeat or (2 if a.ci else 1)
    os.makedirs(a.out, exist_ok=True)
    rows_all = []
    failed = False
    for s in suites:
        mapname = s.map_prefix + 'et_' + s.name
        sdir = os.path.join(a.out, s.name)
        os.makedirs(sdir, exist_ok=True)
        text = build.write_map(s, 'anzymut.wad')
        try:
            bsp, compiled = server.compile_map(text, mapname, os.path.join(sdir, 'map'), a.sdhlt)
        except RuntimeError as e:
            print('== suite %s: map compile failed: %s' % (s.name, e))
            failed = True
            continue
        server.log('== suite %s: %d tests, map %s%s' % (s.name, len(s.tests), mapname, ' (compiled)' if compiled else ''))
        seq = build.write_seq(s)
        steps_name = 'et_%s.steps' % s.name
        trace_name = 'et_%s.trace.jsonl' % s.name
        steps = build.write_steps(s, trace_name)
        with open(os.path.join(sdir, steps_name), 'w') as f:
            f.write(steps)
        results_runs, problems_all, wall_all = [], [], 0.0
        for n in range(1, repeat + 1):
            srv = os.path.join(a.out, 'run', '%s-%d' % (s.name, n))
            server.setup_server(a.hlds, srv, os.path.abspath(a.hl), bsp, mapname, seq, steps, steps_name, '', s.compat,
                                getattr(s, 'lump_index', {}), getattr(s, 'files', {}))
            console = os.path.join(sdir, 'console-%d.log' % n)
            timeout = 90 + build.duration(s) / a.timescale * 3
            status, wall = server.run_server(srv, mapname, steps_name, a.timescale, timeout, console, asan=a.asan,
                                             maxplayers=max(8, s.players + 2),
                                             extra_args=getattr(s, 'server_args', ()))
            wall_all += wall
            tpath = os.path.join(srv, 'syndicate', trace_name)
            tcopy = os.path.join(sdir, 'trace-%d.jsonl' % n)
            if os.path.exists(tpath):
                with open(tpath) as fi, open(tcopy, 'w') as fo:
                    fo.write(fi.read())
            run = check.Run(check.load_trace(tcopy), open(console, errors='replace').read())
            results, problems = check.check_run(s, run)
            if status not in (0, 'timeout') and not problems:
                problems.append('server exit status %s' % status)
            if status == 'timeout':
                problems.append('server timeout after %.0f s' % timeout)
            for asan in glob.glob(os.path.join(srv, 'asan*')):
                problems.append('sanitizer report %s' % asan)
            results_runs.append(results)
            problems_all += ['run %d: %s' % (n, p) if repeat > 1 else p for p in problems]
        rows = report.merge(results_runs)
        rows_all.append((s, rows, problems_all, wall_all))
        failed |= bool(problems_all) or any(r[1] not in ('pass', 'xfail') for r in rows)
    print(report.table(rows_all))
    report.write_json(os.path.join(a.out, 'report.json'), rows_all)
    report.write_junit(os.path.join(a.out, 'junit.xml'), rows_all)
    print('report: %s, %s' % (os.path.join(a.out, 'report.json'), os.path.join(a.out, 'junit.xml')))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
