#!/usr/bin/env python3
"""Map regression tests: the 1.38 server .so and develop's hl.so run the same map logic side by side on the stub engine
(tests/fuzz/server's case runner), and every difference must be listed in tests/maps/baseline.toml.

Two suites (docs/dev/MAP_TESTS.md):
  replay  every map in tests/maps/maps.txt: its entity lump spawned with all its keys, every targetname fired once,
          every trigger_once / trigger_multiple / func_button / func_rot_button touched by a player; the trigger lines
          (Firing / Found / MultiSrc / Multisource), dropped classes and engine-error conditions are compared per
          fired name
  cases   the case files in tests/maps/cases/: monsters, monstermakers, scripts, round rules, entity logic; their
          observations (OBS lines: count, dump, soundcount, expect*) are compared. A case with a "# covers:" header
          is a spec case: its expectations are what the FGD / docs say, so each must also pass on develop, unless
          the case names a "# known-bug: <id>" listed in baseline.toml ([[known_bug]]), which must then still fail

  map_tests.py [all|replay|cases] [name ...] [--update-baseline] [--list]
    name               maps (replay) or case names (cases), without extension; default: all of them
    --update-baseline  write the differences seen into baseline.toml (new ones as decision "unreviewed", which fails
                       until a person gives them a decision; listed ones that no longer show are dropped)
    --keep-logs        replay: keep the full console logs (several hundred MB), not only the compared lines

Inputs (environment; relative paths are relative to the repository; the 1.38 binary, maps and content also to the
main checkout of a git worktree, where the gitignored dist/ is):
  SBO_138_SO        1.38 server binary       default dist/release-1.38/syndicate/dlls/hl_i386.so
  SBO_DEVELOP_SO    the hl.so under test     default build/linux/hl.so
  SBO_MAP_DIRS      folders with the .bsp files, searched in order (colon-separated)
                    default gamedir/syndicate/maps:dist/custom-maps
  SBO_CONTENT_PATH  folders the stub engine reads game files from after gamedir/syndicate (models, sprites, sounds a
                    production server has; colon-separated, missing ones skipped)
                    default dist/hlds-1.38/syndicate:dist/hlds-1.38/valve:
                            $HLDS_DIR/valve (HLDS_DIR default ~/hlds):dist/custom-maps/content
  SNIPER_IMAGE      the 32-bit build/run image (CI's)
  MAP_TESTS_OUT     output folder            default build/map-tests
  MAP_TESTS_DOCKER  0: run the runner here instead of in the sniper image (a CI job that runs in that image)
Exit status: 0 = only the baseline's differences, 1 = a difference not in the baseline, a listed one that did not
show, an unreviewed one, or a run that crashed / hung; 2 = setup error (a missing binary or map).
"""
import collections
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
BASELINE = os.path.join(HERE, 'baseline.toml')
CASES_DIR = os.path.join(HERE, 'cases')
MAPS_LIST = os.path.join(HERE, 'maps.txt')
SNIPER_IMAGE = os.environ.get('SNIPER_IMAGE',
                              'registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014')
BINARIES = ('legacy', 'develop')
CASE_TIMEOUT = 300           # seconds per case (the largest maps replay in under a minute)
# decision statuses that keep the run green (baseline.toml header); same-total is also checked: each line's total over
# the test's segments must be equal on both binaries (the same lines at another moment)
STATUSES = ('approved', 'pending-fix', 'awaiting-decision', 'same-total')


def main_checkout():
    """The main checkout of a git worktree (gitignored inputs such as dist/ live there), or ROOT."""
    try:
        common = subprocess.run(['git', '-C', ROOT, 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(common)
    except (OSError, subprocess.CalledProcessError):
        return ROOT


MAIN = main_checkout()


def resolve(path, main_fallback=True):
    """An absolute path; a relative one from ROOT if it exists there, else from the main checkout (inputs only: the
    binary under test and the output always belong to this checkout)."""
    path = os.path.expanduser(path)
    if os.path.isabs(path):
        return path
    for base in (ROOT, MAIN) if main_fallback else (ROOT,):
        if os.path.exists(os.path.join(base, path)):
            return os.path.join(base, path)
    return os.path.join(ROOT, path)


def config():
    hlds = os.path.expanduser(os.environ.get('HLDS_DIR', '~/hlds'))
    content = os.environ.get('SBO_CONTENT_PATH',
                             'dist/hlds-1.38/syndicate:dist/hlds-1.38/valve:'
                             + hlds + '/valve:dist/custom-maps/content')
    return {
        'legacy': resolve(os.environ.get('SBO_138_SO', 'dist/release-1.38/syndicate/dlls/hl_i386.so')),
        'develop': resolve(os.environ.get('SBO_DEVELOP_SO', 'build/linux/hl.so'), False),
        'map_dirs': [resolve(p) for p in os.environ.get(
            'SBO_MAP_DIRS', 'gamedir/syndicate/maps:dist/custom-maps').split(':') if p],
        'content': [resolve(p) for p in content.split(':') if p and os.path.isdir(resolve(p))],
        'out': resolve(os.environ.get('MAP_TESTS_OUT', 'build/map-tests'), False),
    }


# ------------------------------------------------------------------------------------------------ replay cases

def find_bsp(name, dirs):
    for d in dirs:
        try:
            for f in os.listdir(d):
                if f.lower() == name.lower() + '.bsp':
                    return os.path.join(d, f)
        except OSError:
            pass
    return None


def entity_lump(path):
    """The entities of a GoldSrc BSP (version 30): a list of [(key, value), ...] in lump order."""
    with open(path, 'rb') as f:
        head = f.read(4 + 15 * 8)
        version, = struct.unpack_from('<i', head, 0)
        if version != 30:
            raise ValueError('%s: BSP version %d, not 30' % (path, version))
        off, length = struct.unpack_from('<ii', head, 4)
        f.seek(off)
        text = f.read(length).rstrip(b'\0').decode('latin-1')
    ents, cur, toks = [], None, re.findall(r'"([^"]*)"|([{}])', text)
    key = None
    for quoted, brace in toks:
        if brace == '{':
            cur, key = [], None
        elif brace == '}':
            if cur is not None:
                ents.append(cur)
            cur = None
        elif cur is not None:
            if key is None:
                key = quoted
            else:
                cur.append((key, quoted))
                key = None
    return ents


SKIP_FIRE = {'game_end', 'trigger_changelevel', 'multisource'}   # map end / map change; masters are not Used


def replay_game_dir(cfg):
    """A search folder of the replay runs with maps/<map>.bsp of the custom maps."""
    return os.path.join(cfg['out'], 'replay', 'game')


def link_bsp(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.exists(dst) and os.path.getsize(dst) == os.path.getsize(src) and \
            os.path.getmtime(dst) >= os.path.getmtime(src):
        return
    if os.path.lexists(dst):
        os.remove(dst)
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def replay_case(name, bsp):
    """Spawn every entity (lump order, all keys), activate, connect a player, fire every targetname once (sorted),
    5 frames apart, touch the player triggers and buttons, then 40 s."""
    def q(v):
        return '"%s"' % v.replace('"', "'").replace('\n', ' ').replace('\r', ' ')
    ents = entity_lump(bsp)
    lines = ['# replay of %s (%d entities) from %s' % (name, len(ents), bsp), 'map ' + name, 'cvar mp_makermon 1']
    names, skip = set(), set()
    for kv in ents:
        cls = next((v for k, v in kv if k == 'classname'), None)
        if not cls or cls == 'worldspawn':
            continue
        keys = ' '.join(q(k) + ' ' + q(v) for k, v in kv if k != 'classname')
        lines.append(('spawn %s %s' % (cls, keys)).rstrip())
        tn = next((v for k, v in kv if k == 'targetname'), None)
        if tn:
            (skip if cls in SKIP_FIRE else names).add(tn)
    lines += ['activate', 'connect 1', 'frames 20']
    for n in sorted(names - skip):
        if ' ' in n or '"' in n or not n:
            continue
        lines += ['fire ' + n, 'frames 5']
    lines += ['mark touches', 'touchall trigger_once', 'touchall trigger_multiple', 'touchall func_button',
              'touchall func_rot_button', 'mark end', 'frames 400']
    return '\n'.join(lines) + '\n'


# ------------------------------------------------------------------------------------------------ running

RUNNER_SOURCES = ('tests/fuzz/server/fuzz_server.cpp', 'tests/fuzz/server/stub_engine.cpp',
                  'tests/fuzz/server/stub_engine.h')
REPLAY_KEEP = r'^(@@|Firing: |Found: |MultiSrc|Multisource |ENGINE: |OBS |case |\*\*\*|fuzz_server)'


def docker(cfg, script, mounts, env):
    """Run a bash script in the sniper image (or here, with MAP_TESTS_DOCKER=0: CI jobs that run in that image)."""
    os.makedirs(cfg['out'], exist_ok=True)
    path = os.path.join(cfg['out'], 'run.sh')
    with open(path, 'w') as f:
        f.write(script + '\n')
    if os.environ.get('MAP_TESTS_DOCKER', '1') == '0':
        return subprocess.run(['nice', '-n', '15', 'timeout', '7200', 'bash', path], env=dict(os.environ, **env)).returncode
    args = ['docker', 'run', '--rm', '--memory=3g', '--memory-swap=3g', '--cpus=1', '-u',
            '%d:%d' % (os.getuid(), os.getgid()), '-e', 'HOME=/tmp', '-v', '%s:%s' % (ROOT, ROOT), '-w', ROOT]
    for m in sorted(set(mounts)):
        if not (m + '/').startswith(ROOT + '/'):
            args += ['-v', '%s:%s:ro' % (m, m)]
    for k, v in env.items():
        args += ['-e', '%s=%s' % (k, v)]
    args += [SNIPER_IMAGE, 'bash', path]
    return subprocess.run(['nice', '-n', '15', 'timeout', '7200'] + args).returncode


def build_runner(cfg):
    out = os.path.join(cfg['out'], 'runner')
    srcs = [os.path.join(ROOT, s) for s in RUNNER_SOURCES]
    if os.path.exists(out) and os.path.getmtime(out) > max(os.path.getmtime(s) for s in srcs):
        return out
    print('building the case runner (no sanitizers: the 1.38 .so cannot run under ASan)', flush=True)
    script = ("g++ -m32 -std=c++17 -g -O1 -fno-omit-frame-pointer -fpermissive -w -DFUZZ_NO_SANITIZER "
              "-Dstricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -DVALVE_DLL -DCLIENT_WEAPONS "
              "-I src/dlls -I src/engine -I src/common -I src/pm_shared -I src/game_shared "
              "-o '%s' tests/fuzz/server/fuzz_server.cpp tests/fuzz/server/stub_engine.cpp -ldl" % out)
    if docker(cfg, script, [], {}) != 0 or not os.path.exists(out):
        sys.exit('map tests: the case runner did not build')
    return out


def run_suite(cfg, runner, suite, cases, keep_logs):
    """Each case on both binaries; logs in <out>/<suite>/<binary>/<case>.log."""
    sh = ['set -u']
    mounts = [os.path.dirname(cfg[b]) for b in BINARIES] + cfg['content']
    env = {'STUB_SEARCH_PATH': ':'.join(cfg['content'])}
    if suite == 'replay':
        env['STUB_ASSUME_MODELS'] = '1'
        if os.path.isdir(replay_game_dir(cfg)):
            mounts.append(replay_game_dir(cfg))
            env['STUB_SEARCH_PATH'] = ':'.join([replay_game_dir(cfg)] + cfg['content'])
        flags = '--verbose --drop-unknown-classes'
    else:
        env.update(STUB_STUDIO_MODELS='1', STUB_PVS_CLIENT='1', STUB_LOG_SOUND='1', STUB_LOG_MSG='1', CASE_DUMP='1')
        flags = '--keep-going'
    for b in BINARIES:
        logdir = os.path.join(cfg['out'], suite, b)
        os.makedirs(logdir, exist_ok=True)
        for name, path in cases:
            log = os.path.join(logdir, name + '.log')
            cmd = ("timeout %d '%s' '%s' '%s/gamedir/syndicate' /dev/null /dev/null /dev/null /dev/null --mode cases "
                   "%s --case-timeout %d '%s'" % (CASE_TIMEOUT + 30, runner, cfg[b], ROOT, flags, CASE_TIMEOUT, path))
            if suite == 'replay' and not keep_logs:
                sh.append("%s 2>&1 | grep -aE '%s' > '%s'" % (cmd, REPLAY_KEEP, log))
            else:
                sh.append("%s > '%s' 2>&1" % (cmd, log))
            sh.append("echo \"%s %-8s %s: $(grep -aE '^case ' '%s' | sed -E 's/^case [^:]*: //' || true)\""
                      % (suite, b, name, log))
    print('running %d %s case(s) on 1.38 (%s) and develop (%s)' % (len(cases), suite, cfg['legacy'], cfg['develop']),
          flush=True)
    return docker(cfg, '\n'.join(sh), mounts, env)


# ------------------------------------------------------------------------------------------------ comparing

ENGINE_DURING = re.compile(r'\s+\[during: .*\]$')


def observations(path, suite):
    """{segment: Counter(line)}: replay segments start at each @@FIRE / @@MARK / @@TOUCH line; a case is one
    segment. Abnormal endings (crash, hang, bad case) are lines of segment '@result'."""
    segs = collections.OrderedDict([('@start', collections.Counter())])
    cur, result = '@start', None
    try:
        lines = open(path, encoding='latin-1').read().splitlines()
    except OSError:
        lines = []
    for line in lines:
        if line.startswith('case ') and ': ' in line:
            result = line.split(': ', 1)[1]
            continue
        if line.startswith('*** '):
            segs.setdefault('@result', collections.Counter())[re.sub(r' /\S+/', ' ', line)] += 1
            continue
        if line.startswith('ENGINE: '):
            segs[cur][ENGINE_DURING.sub('', line)] += 1
            continue
        if suite == 'replay':
            if line.startswith(('@@FIRE ', '@@MARK ', '@@TOUCH ')):
                cur = line
                segs.setdefault(cur, collections.Counter())
            elif re.match(r'^(Firing: |Found: |MultiSrc|Multisource |@@NOCLASS )', line):
                segs[cur][line] += 1
        elif line.startswith('OBS '):
            segs[cur][line] += 1
    # exit 0, 4 (engine-error conditions: compared as lines) and 5 (failed expects: compared as OBS lines) completed
    if result is None:
        segs.setdefault('@result', collections.Counter())['no result (the runner did not finish)'] += 1
    elif not re.match(r'^(ok|FAILED \(exit [45]\))$', result):
        segs.setdefault('@result', collections.Counter())['result: ' + result] += 1
    return segs


def differences(cfg, suite, names):
    out = {}
    for name in names:
        a = observations(os.path.join(cfg['out'], suite, 'legacy', name + '.log'), suite)
        b = observations(os.path.join(cfg['out'], suite, 'develop', name + '.log'), suite)
        for seg in list(a) + [s for s in b if s not in a]:
            ca, cb = a.get(seg, collections.Counter()), b.get(seg, collections.Counter())
            for line in set(ca) | set(cb):
                if ca[line] != cb[line]:
                    out[(suite, name, seg, line)] = (ca[line], cb[line])
    return out


def case_header(path):
    """The tags of a case file's leading comment lines: {'covers': [items], 'known-bug': [ids]}. "# covers: a b.c d:1"
    lists the classes, keys (class.key, class.key=value) and spawnflag bits (class:bit) the case asserts (read by
    tests/entity/etest/coverage.py); "# known-bug: id" marks a spec case that fails on develop because of a bug listed
    in baseline.toml."""
    tags = {'covers': [], 'known-bug': []}
    try:
        lines = open(path, encoding='latin-1').read().splitlines()
    except OSError:
        return tags
    for line in lines:
        if not line.startswith('#'):
            break
        m = re.match(r'^#\s*(covers|known-bug):\s*(.*)$', line)
        if m:
            tags[m.group(1)] += m.group(2).split()
    return tags


def failed_expects(path):
    """The expect* observations of a case log that did not pass."""
    out = []
    try:
        lines = open(path, encoding='latin-1').read().splitlines()
    except OSError:
        return ['no log']
    for line in lines:
        if line.startswith('OBS expect') and not line.endswith(': ok'):
            out.append(line[4:])
    return out


def check_spec(cfg, cases, known_bugs, report):
    """Spec cases (a "# covers:" header) must pass every expectation on develop; a case naming a known bug must still
    fail (the bug is fixed: drop the tag and the [[known_bug]] entry). Returns the number of problems."""
    bad, known = [], []
    for name in cases:
        tags = case_header(os.path.join(CASES_DIR, name + '.txt'))
        if not tags['covers']:
            continue
        fails = failed_expects(os.path.join(cfg['out'], 'cases', 'develop', name + '.log'))
        ids = tags['known-bug']
        unknown = [i for i in ids if i not in known_bugs]
        if unknown:
            bad.append('%s: known-bug %s is not a [[known_bug]] in baseline.toml' % (name, ', '.join(unknown)))
        elif ids and not fails:
            bad.append('%s: passes on develop but is tagged known-bug %s (fixed? drop the tag and the entry)'
                       % (name, ', '.join(ids)))
        elif ids:
            known.append('  [%s] %s: %d failed expectation(s)' % (', '.join(ids), name, len(fails)))
        elif fails:
            bad.append('%s: develop fails the documented behavior: %s' % (name, '; '.join(fails)))
    if bad:
        report.append('SPEC CASES FAILING ON DEVELOP (%d):' % len(bad))
        report += ['  ' + b for b in bad]
    if known:
        report.append('known bugs (spec cases that fail on develop as listed):')
        report += known
    return len(bad)


def parse_toml(text):
    """The TOML subset baseline.toml uses: comments, [[table]] arrays, key = "string" / integer (tomllib needs
    Python 3.11; this runs on older ones too)."""
    data, cur = {}, None
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        m = re.match(r'^\[\[([A-Za-z_]+)\]\]$', line)
        if m:
            cur = {}
            data.setdefault(m.group(1), []).append(cur)
            continue
        m = re.match(r'^([A-Za-z_]+)\s*=\s*(?:"((?:[^"\\]|\\.)*)"|(-?\d+))\s*(?:#.*)?$', line)
        if not m or cur is None:
            raise ValueError('baseline.toml line %d: not understood: %s' % (n, raw))
        cur[m.group(1)] = (re.sub(r'\\(.)', r'\1', m.group(2)) if m.group(2) is not None else int(m.group(3)))
    return data


def load_baseline():
    with open(BASELINE, encoding='utf-8') as f:
        data = parse_toml(f.read())
    decisions = {d['id']: d for d in data.get('decision', [])}
    decisions['@known_bugs'] = {d['id']: d for d in data.get('known_bug', [])}
    entries = {}
    for e in data.get('difference', []):
        key = (e['suite'], e['test'], e['segment'], e['line'])
        entries[key] = e
    return decisions, entries


def toml_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write_baseline(decisions, entries, seen, ran):
    """Rewrite baseline.toml: the decisions as they are, the differences seen (kept decision, or 'unreviewed')."""
    with open(BASELINE, encoding='utf-8') as f:
        text = f.read()
    head = text.split('\n[[difference]]', 1)[0].rstrip('\n') + '\n'
    new = {}
    for key, e in entries.items():
        if (key[0], key[1]) not in ran:
            new[key] = (e['legacy'], e['develop'], e['decision'])
    for key, (la, dv) in seen.items():
        old = entries.get(key)
        new[key] = (la, dv, old['decision'] if old else 'unreviewed')
    out = [head]
    for key in sorted(new, key=lambda k: (k[0], k[1], new[k][2], k[2], k[3])):
        la, dv, dec = new[key]
        out.append('\n[[difference]]\nsuite = %s\ntest = %s\nsegment = %s\nline = %s\nlegacy = %d\ndevelop = %d\n'
                   'decision = %s\n' % (toml_str(key[0]), toml_str(key[1]), toml_str(key[2]), toml_str(key[3]),
                                        la, dv, toml_str(dec)))
    # the [[known_bug]] blocks after the differences (the head above ends at the first difference)
    for kb in decisions.get('@known_bugs', {}).values():
        if '\nid = %s\n' % toml_str(kb['id']) not in head:
            out.append('\n[[known_bug]]\nid = %s\nreason = %s\n' % (toml_str(kb['id']), toml_str(kb.get('reason', ''))))
    with open(BASELINE, 'w', encoding='utf-8') as f:
        f.write(''.join(out))


def check(seen, ran, decisions, entries, report):
    bad = 0
    unknown = [k for k in seen if k not in entries]
    gone = [k for k, e in entries.items() if (k[0], k[1]) in ran and k not in seen]
    changed = [k for k in seen if k in entries and (entries[k]['legacy'], entries[k]['develop']) != seen[k]]
    unreviewed = [k for k in seen if k in entries and (entries[k]['decision'] not in decisions
                                                       or entries[k]['decision'] == '@known_bugs')]
    bad_status = [d for k, d in decisions.items() if k != '@known_bugs' and d.get('status') not in STATUSES]
    totals = collections.defaultdict(lambda: [0, 0])
    for k, (la, dv) in seen.items():
        if k in entries and decisions.get(entries[k]['decision'], {}).get('status') == 'same-total':
            totals[(k[0], k[1], k[3])][0] += la
            totals[(k[0], k[1], k[3])][1] += dv
    moved = [k for k, (la, dv) in totals.items() if la != dv]

    def show(title, keys):
        if keys:
            report.append('%s (%d):' % (title, len(keys)))
            for k in sorted(keys):
                la, dv = seen.get(k, (entries[k]['legacy'], entries[k]['develop']) if k in entries else (0, 0))
                report.append('  %-7s %-28s %-40s 1.38 %3d  develop %3d  %s' % (k[0], k[1], k[2][:40], la, dv, k[3]))
    show('NEW DIFFERENCES, not in tests/maps/baseline.toml', unknown)
    show('LISTED DIFFERENCES THAT DID NOT SHOW (fixed? update the baseline)', gone)
    show('LISTED DIFFERENCES WITH OTHER COUNTS', changed)
    show('DIFFERENCES WITHOUT A DECISION (decision "unreviewed" or unknown)', unreviewed)
    for k in sorted(moved):
        report.append('same-total difference with other totals: %s %s: 1.38 %d, develop %d: %s'
                      % (k[0], k[1], totals[k][0], totals[k][1], k[2]))
    for d in bad_status:
        report.append('decision %s: status %r is not one of %s' % (d['id'], d.get('status'), ', '.join(STATUSES)))
    bad = len(unknown) + len(gone) + len(changed) + len(unreviewed) + len(bad_status) + len(moved)
    # the known differences, by decision
    by_dec = collections.defaultdict(lambda: collections.Counter())
    for k in seen:
        if k in entries and entries[k]['decision'] in decisions:
            by_dec[entries[k]['decision']][(k[0], k[1])] += 1
    if by_dec:
        report.append('baseline differences seen, by decision:')
        for dec in sorted(by_dec):
            d = decisions[dec]
            tests = ', '.join('%s (%d)' % (t[1], n) for t, n in sorted(by_dec[dec].items()))
            report.append('  [%s] %s: %s' % (d['status'], dec, tests))
    return bad


# ------------------------------------------------------------------------------------------------ main

def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    opts = {a for a in argv if a.startswith('--')}
    unknown_opts = opts - {'--update-baseline', '--keep-logs', '--list', '--help'}
    if '--help' in opts or unknown_opts:
        print(__doc__)
        return 2 if unknown_opts else 0
    what = args.pop(0) if args and args[0] in ('all', 'replay', 'cases') else 'all'
    cfg = config()
    all_maps = [l.split('#')[0].strip() for l in open(MAPS_LIST) if l.split('#')[0].strip()]
    all_cases = sorted(f[:-4] for f in os.listdir(CASES_DIR) if f.endswith('.txt'))
    if '--list' in opts:
        print('replay maps (%d): %s' % (len(all_maps), ' '.join(all_maps)))
        print('cases (%d): %s' % (len(all_cases), ' '.join(all_cases)))
        return 0
    maps = [a for a in args if a in all_maps] if args else all_maps
    cases = [a for a in args if a in all_cases] if args else all_cases
    stray = [a for a in args if a not in all_maps and a not in all_cases]
    if stray:
        print('map tests: not a map in tests/maps/maps.txt or a case in tests/maps/cases/: %s' % ' '.join(stray))
        return 2
    if what == 'replay' or (args and not cases):
        cases = []
    if what == 'cases' or (args and not maps):
        maps = []
    for b in BINARIES:
        if not os.path.isfile(cfg[b]):
            print('map tests: no %s binary %s (set %s)' % (b, cfg[b], 'SBO_138_SO' if b == 'legacy' else
                                                            'SBO_DEVELOP_SO'))
            return 2
    replay = []
    if maps:
        cdir = os.path.join(cfg['out'], 'replay', 'cases')
        os.makedirs(cdir, exist_ok=True)
        for m in maps:
            bsp = find_bsp(m, cfg['map_dirs'])
            if not bsp:
                print('map tests: %s.bsp is in none of SBO_MAP_DIRS (%s)' % (m, ':'.join(cfg['map_dirs'])))
                return 2
            path = os.path.join(cdir, m + '.txt')
            with open(path, 'w', encoding='latin-1') as f:
                f.write(replay_case(m, bsp))
            replay.append((m, path))
            # the game reads maps/<map>.bsp for its map_compat.txt entries (the CRC, the entity lump): custom maps are
            # not in gamedir/syndicate/maps, so they get a copy (a link) in a search folder of the runs
            if os.path.dirname(os.path.abspath(bsp)) != os.path.join(ROOT, 'gamedir', 'syndicate', 'maps'):
                link_bsp(bsp, os.path.join(replay_game_dir(cfg), 'maps', m + '.bsp'))
    runner = build_runner(cfg)
    rc = 0
    if replay:
        rc |= run_suite(cfg, runner, 'replay', replay, '--keep-logs' in opts)
    if cases:
        rc |= run_suite(cfg, runner, 'cases', [(c, os.path.join(CASES_DIR, c + '.txt')) for c in cases], False)
    if rc not in (0, 1):
        print('map tests: the runner container failed (status %d)' % rc)
        return 2
    seen = {}
    seen.update(differences(cfg, 'replay', maps))
    seen.update(differences(cfg, 'cases', cases))
    ran = {('replay', m) for m in maps} | {('cases', c) for c in cases}
    decisions, entries = load_baseline()
    if '--update-baseline' in opts:
        write_baseline(decisions, entries, seen, ran)
        decisions, entries = load_baseline()
    report = ['map tests: %d map(s), %d case(s); 1.38 %s, develop %s' % (len(maps), len(cases), cfg['legacy'],
                                                                         cfg['develop'])]
    bad = check(seen, ran, decisions, entries, report)
    bad += check_spec(cfg, cases, decisions['@known_bugs'], report)
    report.append('RESULT: %s (%d difference(s) seen, %d problem(s))' % ('FAIL' if bad else 'PASS', len(seen), bad))
    os.makedirs(cfg['out'], exist_ok=True)
    with open(os.path.join(cfg['out'], 'report.txt'), 'w') as f:
        f.write('\n'.join(report) + '\n')
    print('\n'.join(report))
    print('report: %s; logs: %s/<suite>/<legacy|develop>/<name>.log' % (os.path.join(cfg['out'], 'report.txt'),
                                                                      cfg['out']))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
