"""Checks a run: the trace (JSON lines from the test layer) and the server's console against each test's
expectations. Event based with tolerances: frame times vary with the machine, so nothing depends on frame counts."""

import json
import re

from . import decode

# console lines that mean the server is broken (engine errors, sanitizers, crashes)
FATAL = re.compile(r'Host_Error|Sys_Error|FATAL ERROR|Segmentation fault|ERROR: AddressSanitizer|runtime error:|'
                   r'SZ_GetSpace: overflow|Precache can only be done in spawn|ED_Alloc: no free edicts|'
                   r'Tried to write to an uninitialized|Mod_NumForName')


def load_trace(path):
    events = []
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        events.append(json.loads(line))
                    except ValueError:
                        events.append({'ev': 'bad_line', 't': -1, 'line': line[:200]})
    except OSError:
        pass
    return events


class Run:
    """One server run: events, marks, entity names by index."""

    def __init__(self, events, console):
        self.events = events
        self.console = console
        self.marks = {}
        for e in events:
            if e['ev'] == 'mark':
                self.marks.setdefault(e['label'], e['t'])
        self.ended = any(e['ev'] == 'end' for e in events)
        self.idx_name = {}      # index -> last known ref, for "remove" events
        # the model table of the map each message was sent on (precache events carry the engine's index): a message
        # field expected as 'model:<name>' must be that name's index on that map, whatever an earlier map had
        models = {}
        for e in events:
            if e['ev'] == 'precache' and e.get('kind') == 'model' and 'index' in e:
                models[e['name'].lower()] = e['index']
            elif e['ev'] == 'map_end':
                models = {}
            elif e['ev'] == 'msg':
                e['_models'] = models
        for e in events:
            if e['ev'] in ('exists', 'spawn'):
                e_ent = e['ent']
                self.idx_name[e_ent['i']] = e_ent
            elif e['ev'] == 'remove':
                e['ent'] = self.idx_name.get(e['i'], {'i': e['i'], 'c': '', 'n': ''})

    def window(self, tid):
        """Events of one test: from its begin mark to its end mark."""
        t0, t1 = self.marks.get(tid + ':begin'), self.marks.get(tid + ':end')
        if t0 is None:
            return None, None, []
        if t1 is None:
            t1 = float('inf')
        out = []
        inside = False
        for e in self.events:
            if e['ev'] == 'mark' and e['label'] == tid + ':begin':
                inside = True
            if inside:
                out.append(e)
            if e['ev'] == 'mark' and e['label'] == tid + ':end':
                break
        return t0, t1, out


def ent_matches(ent, spec):
    """ent: {"i","c","n"} from the trace. spec: a resolved reference (prefixed name, playerN, @class, #i, world)."""
    if spec is None:
        return True
    if ent is None:
        return spec in ('null', None)
    if spec.startswith('player') and spec[6:].isdigit():
        return ent['i'] == int(spec[6:])
    if spec.startswith('@'):
        return ent['c'] == spec[1:]
    if spec.startswith('#'):
        return ent['i'] == int(spec[1:])
    if spec == 'world':
        return ent['i'] == 0
    return ent['n'] == spec


def _msg_is(e, name):
    """a msg event is the named user message, or the engine message 'svc:<number>' (e.g. 'svc:23', a temp entity)"""
    if name.startswith('svc:'):
        return e['type'] == int(name[4:])
    return e['name'] == name


def _model_ref(e, value):
    """'model:<name>': that model's (or sprite's) index on the map the message was sent on, None if not precached
    there; any other value as it is"""
    if isinstance(value, str) and value.startswith('model:'):
        return e.get('_models', {}).get(value[6:].lower())
    return value


def _close(a, b, tol):
    if isinstance(b, dict) and 'range' in b:     # {'range': (lo, hi)}: a number within lo .. hi
        lo, hi = b['range']
        return isinstance(a, (int, float)) and lo <= a <= hi
    if isinstance(b, (list, tuple)):
        return isinstance(a, list) and len(a) == len(b) and all(_close(x, y, tol) for x, y in zip(a, b))
    if isinstance(b, (int, float)) and isinstance(a, (int, float)):
        return abs(a - b) <= tol
    return a == b


class TestResult:
    def __init__(self, test):
        self.test = test
        self.failures = []
        self.notes = []      # measured values worth reporting (distances, damage)

    @property
    def ok(self):
        return not self.failures


def check_test(test, run):
    r = TestResult(test)
    t0, t1, ev = run.window(test.id)
    if t0 is None:
        r.failures.append('the test never started (no begin mark: the server stopped earlier)')
        return r
    if t1 == float('inf'):
        r.failures.append('the test never ended (no end mark)')
    ref = test.ref
    marks = {k.split(':', 1)[1]: v for k, v in run.marks.items() if k.startswith(test.id + ':')}

    mark_dt = {e['label'].split(':', 1)[1]: e.get('dt', 0) for e in ev if e['ev'] == 'mark' and
               e['label'].startswith(test.id + ':')}

    mark_frame = {e['label'].split(':', 1)[1]: e['f'] for e in ev if e['ev'] == 'mark' and
                  e['label'].startswith(test.id + ':')}

    def timed(e, x):
        """the event passes the expectation's after / within. Frames are not fixed: an event that is due at time T
        happens in the first frame at or after T, so the window grows by the frame times at both ends. "After" is
        by frame: a think runs with gpGlobals->time set to its own (earlier) think time, in a frame whose steps (and
        marks) already ran at the frame's time."""
        name = x.get('after') or 'begin'
        base = marks.get(name, t0)
        if e['f'] < mark_frame.get(name, 0):
            return False
        w = x.get('within')
        if w is None:
            return True
        slack = e.get('dt', 0) + mark_dt.get(name, 0) + 2e-3
        if x.get('count') == 0:
            # an absence: the window shrinks by the frame times (an event just after it may land in its last frame)
            return w[0] + slack <= max(e['t'] - base, 0) <= w[1] - slack
        return w[0] - slack <= max(e['t'] - base, 0) <= w[1] + slack

    def count_check(name, matches, x):
        n = len(matches)
        if x.get('count') is not None and n != x['count']:
            return '%s: %d, expected exactly %d' % (name, n, x['count'])
        if x.get('count') is None and n < x.get('min', 1):
            return '%s: %d, expected at least %d' % (name, n, x.get('min', 1))
        if x.get('count') is None and x.get('max') is not None and n > x['max']:
            return '%s: %d, expected at most %d' % (name, n, x['max'])
        return None

    for e in ev:
        if e['ev'] in ('error', 'engine_error', 'bad_line'):
            r.failures.append('trace %s: %s' % (e['ev'], e.get('msg', e.get('line', ''))))

    for x in test.expectations:
        k = x['kind']
        msg = None
        if k == 'fired':
            m = [e for e in ev if e['ev'] == 'fire' and e['target'] == x['target'] and timed(e, x)
                 and (x['caller'] is None or ent_matches(e['caller'], ref(x['caller'])))
                 and (x['activator'] is None or ent_matches(e['activator'], ref(x['activator'])))
                 and (x['use'] is None or e['use'] == x['use'])]
            msg = count_check('fired %s' % x['target'], m, x)
            if msg:
                msg += _when(x)
        elif k == 'used':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'use' and ent_matches(e['ent'], spec) and timed(e, x)
                 and (x['caller'] is None or ent_matches(e['caller'], ref(x['caller'])))
                 and (x['activator'] is None or ent_matches(e['activator'], ref(x['activator'])))
                 and (x['use'] is None or e['use'] == x['use'])]
            msg = count_check('Use of %s' % spec, m, x)
            if msg:
                msg += _when(x)
        elif k == 'order':
            fa = [e['t'] for e in ev if e['ev'] == 'fire' and e['target'] == x['first']]
            fb = [e['t'] for e in ev if e['ev'] == 'fire' and e['target'] == x['then']]
            if not fa or not fb:
                msg = 'order %s before %s: %s never fired' % (x['first'], x['then'], x['first'] if not fa else x['then'])
            elif fa[0] > fb[0]:
                msg = 'order: %s (%.3f) fired after %s (%.3f)' % (x['first'], fa[0], x['then'], fb[0])
        elif k == 'value_any':
            spec = ref(x['ent'])
            got = []
            for lab in x['labels']:
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec)]
                got.append(hit[0]['v'].get(x['field'], '<missing>') if hit else '<no snapshot>')
            want = x['expected'] if isinstance(x['expected'], list) else [x['expected']]
            seen = [g for g in got if g in want]
            if not seen:
                msg = '%s.%s never %s in %s: %s' % (spec, x['field'], json.dumps(x['expected']), ', '.join(x['labels']),
                                                   json.dumps(got))
            else:
                r.notes.append('%s.%s %s at %s' % (x['ent'], x['field'], json.dumps(seen[0]),
                                                   x['labels'][got.index(seen[0])]))
        elif k == 'value':
            label = '%s:%s' % (test.id, x['label'])
            spec = ref(x['ent'])
            snaps = [e for e in ev if e['ev'] == 'snap' and e['label'] == label]
            if not snaps:
                msg = 'no snapshot %s' % x['label']
            else:
                hit = [s for s in snaps if s.get('ent') and ent_matches(s['ent'], spec)]
                if not hit:
                    msg = 'snapshot %s: no entity %s' % (x['label'], spec)
                else:
                    fname, _, comp = x['field'].partition('.')
                    got = hit[0]['v'].get(fname, '<missing>')
                    if comp and isinstance(got, list) and comp in 'xyz':
                        got = got['xyz'.index(comp)]
                    op = x.get('op', '==')
                    if op == '!=':
                        if got == x['expected']:
                            msg = '%s.%s at %s: %s, expected anything else' % (spec, x['field'], x['label'],
                                                                              json.dumps(got))
                    elif op != '==':
                        okv = isinstance(got, (int, float)) and {'<': got < x['expected'], '<=': got <= x['expected'],
                                                                  '>': got > x['expected'], '>=': got >= x['expected'],
                                                                  '&': (int(got) & int(x['expected'])) == int(x['expected']),
                                                                  '!&': (int(got) & int(x['expected'])) == 0}[op]
                        if not okv:
                            msg = '%s.%s at %s: %s, expected %s %s' % (spec, x['field'], x['label'], json.dumps(got), op,
                                                                       json.dumps(x['expected']))
                    elif not _close(got, x['expected'], x['tol']):
                        msg = '%s.%s at %s: %s, expected %s' % (spec, x['field'], x['label'], json.dumps(got),
                                                                 json.dumps(x['expected']))
        elif k == 'distance':
            label = '%s:%s' % (test.id, x['label'])
            pos = []
            for r_ in (x['a'], x['b']):
                spec = ref(r_)
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == label and e.get('ent')
                       and ent_matches(e['ent'], spec) and 'origin' in e['v']]
                pos.append(hit[0]['v']['origin'] if hit else None)
            if None in pos:
                msg = 'snapshot %s: no origin of %s' % (x['label'], x['a'] if pos[0] is None else x['b'])
            else:
                d = [pos[0][i] - pos[1][i] for i in range(2 if x['flat'] else 3)]
                dist = sum(c * c for c in d) ** 0.5
                if not (x['lo'] <= dist <= x['hi']):
                    msg = 'distance %s - %s at %s: %.1f, expected %g to %g' % (ref(x['a']), ref(x['b']), x['label'], dist,
                                                                             x['lo'], x['hi'])
                else:
                    r.notes.append('distance %s - %s at %s: %.1f' % (x['a'], x['b'], x['label'], dist))
        elif k == 'damaged':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'damage' and ent_matches(e['ent'], spec) and timed(e, x)
                 and (x['attacker'] is None or ent_matches(e['attacker'], ref(x['attacker'])))]
            msg = count_check('damage to %s%s' % (spec, ' by %s' % x['attacker'] if x['attacker'] else ''), m, x)
            if msg:
                msg += _when(x)
                seen = [(e['attacker'] or {}).get('c') for e in ev if e['ev'] == 'damage' and ent_matches(e['ent'], spec)]
                if seen:
                    msg += '; damaged by %s' % ', '.join(sorted(set(str(c) for c in seen)))
            elif x['total'] is not None:
                tot = sum(e['amount'] for e in m)
                if not (x['total'][0] <= tot <= x['total'][1]):
                    msg = 'damage to %s: %g in all, expected %g to %g' % (spec, tot, x['total'][0], x['total'][1])
            if not msg and m:
                r.notes.append('damage to %s: %d hits, %g in all' % (x['ent'], len(m), sum(e['amount'] for e in m)))
        elif k == 'ratio':
            vals = []
            for lab, e_ in ((x['label1'], x['ent1']), (x['label2'], x['ent2'])):
                spec = ref(e_)
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec)]
                fname, _, comp = x['field'].partition('.')
                v = hit[0]['v'].get(fname) if hit else None
                if comp and isinstance(v, list):
                    v = v['xyz'.index(comp)]
                vals.append(v)
            if None in vals or not vals[0]:
                msg = 'ratio %s: missing or zero values %s' % (x['field'], vals)
            else:
                got = vals[1] / vals[0]
                if abs(got - x['ratio']) > x['tol'] * abs(x['ratio']):
                    msg = 'ratio of %s (%s / %s): %.3f, expected %g' % (x['field'], x['label2'], x['label1'], got, x['ratio'])
                else:
                    r.notes.append('ratio %s: %.3f' % (x['field'], got))
        elif k == 'moved':
            spec = ref(x['ent'])
            pos = []
            for lab in (x['label1'], x['label2']):
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec) and 'origin' in e['v']]
                pos.append(hit[0]['v']['origin'] if hit else None)
            if None in pos:
                msg = 'moved %s: no origin snapshot %s' % (spec, x['label1'] if pos[0] is None else x['label2'])
            else:
                d = [b - a for a, b in zip(pos[0], pos[1])]
                n = 2 if x['flat'] else 3
                if x.get('min_dist') is not None:
                    dist = sum(d[i] * d[i] for i in range(n)) ** 0.5
                    if dist < x['min_dist']:
                        msg = '%s moved %.1f from %s to %s, expected at least %s' % (spec, dist, x['label1'],
                                                                                  x['label2'], x['min_dist'])
                    else:
                        r.notes.append('%s moved %.1f' % (x['ent'], dist))
                elif any(abs(d[i] - x['delta'][i]) > x['tol'] for i in range(n)):
                    msg = '%s moved %s from %s to %s, expected %s' % (spec, '[%.1f, %.1f, %.1f]' % tuple(d), x['label1'],
                                                                   x['label2'], x['delta'])
                else:
                    r.notes.append('%s moved [%.1f, %.1f, %.1f]' % (x['ent'], d[0], d[1], d[2]))
        elif k == 'same':
            spec = ref(x['ent'])
            vals = []
            for lab in (x['label1'], x['label2']):
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec)]
                vals.append(hit[0]['v'].get(x['field'], '<missing>') if hit else '<no snapshot %s>' % lab)
            if vals[0] != vals[1]:
                msg = '%s.%s changed: %s at %s, %s at %s' % (spec, x['field'], json.dumps(vals[0]), x['label1'],
                                                             json.dumps(vals[1]), x['label2'])
        elif k == 'msg':
            to = ref(x['to']) if x['to'] else None
            m = []
            for e in ev:
                if e['ev'] != 'msg' or not timed(e, x):
                    continue
                if not _msg_is(e, x['name']):
                    continue
                if to and to.startswith('player') and e['to'] != int(to[6:]) and e['dest'] not in (0, 2):
                    continue    # MSG_ONE to someone else (MSG_BROADCAST 0 / MSG_ALL 2 reach everybody)
                if x['fields']:
                    d = decode.decode(e['name'], bytes.fromhex(e['hex']), e['type'])
                    if d is None or any(not _close(d.get(fk), _model_ref(e, fv), 0.01) for fk, fv in x['fields'].items()):
                        continue
                m.append(e)
            msg = count_check('message %s%s' % (x['name'], ' %s' % json.dumps(x['fields']) if x['fields'] else ''), m, x)
            if msg:
                seen = [decode.decode(e['name'], bytes.fromhex(e['hex']), e['type']) for e in ev if e['ev'] == 'msg'
                        and _msg_is(e, x['name']) and timed(e, x)]
                msg += _when(x) + ('; sent: %s' % json.dumps(seen[:4]) if seen else '')
        elif k == 'sound':
            spec = ref(x['ent']) if x['ent'] else None
            kinds = {None: ('sound', 'ambient'), True: ('ambient',), False: ('sound',)}[x.get('ambient')]
            m = [e for e in ev if e['ev'] in kinds
                 and (x['sample'] in e['sample'] or x['sample'] in e.get('sentence', '')) and timed(e, x)
                 and (spec is None or ent_matches(e['ent'], spec))
                 and (not x.get('flags') or (e.get('flags', 0) & x['flags']) == x['flags'])
                 and (x.get('attn') is None or abs(e.get('attn', -1) - x['attn']) < 0.01)
                 and (x.get('vol') is None or x['vol'][0] <= e.get('vol', -1) <= x['vol'][1])
                 and (x.get('pitch') is None or (e.get('pitch') == x['pitch'] if not isinstance(x['pitch'], (list, tuple))
                                                     else x['pitch'][0] <= (e.get('pitch') or 0) <= x['pitch'][1]))]
            msg = count_check('%s %s%s%s%s' % ({None: 'sound', True: 'ambient sound', False: 'entity sound'}[x.get('ambient')],
                                                x['sample'], ' flags %d' % x['flags'] if x.get('flags') else '',
                                                ' attn %g' % x['attn'] if x.get('attn') is not None else '',
                                                ' vol %s' % list(x['vol']) if x.get('vol') else ''), m, x)
        elif k == 'setview':
            spec, view = ref(x['ent']), ref(x['view'])
            m = [e for e in ev if e['ev'] == 'setview' and timed(e, x) and ent_matches(e['ent'], spec)
                 and ent_matches(e['view'], view)]
            msg = count_check('view of %s set to %s' % (spec, view), m, x)
        elif k == 'clcmd':
            spec = ref(x['ent']) if x['ent'] else None
            m = [e for e in ev if e['ev'] == 'clcmd' and timed(e, x) and x['text'] in e['cmd']
                 and (spec is None or ent_matches(e['ent'], spec))]
            msg = count_check('client command "%s" to %s' % (x['text'], spec or 'any player'), m, x)
        elif k == 'event':
            spec = ref(x['ent']) if x['ent'] else None

            def fits(e):
                for fk, fv in (x['fields'] or {}).items():
                    got = e.get(fk)
                    if isinstance(fv, (tuple, list)):
                        if got is None or not fv[0] <= got <= fv[1]:
                            return False
                    elif got is None or abs(got - fv) > 0.01:
                        return False
                return True
            m = [e for e in ev if e['ev'] == 'event' and x['name'] in e['name'] and timed(e, x)
                 and (spec is None or ent_matches(e['ent'], spec)) and fits(e)]
            msg = count_check('event %s%s' % (x['name'], ' %s' % json.dumps(x['fields']) if x['fields'] else ''), m, x)
            if msg:
                seen = [{f: e.get(f) for f in ('fparam1', 'iparam1', 'iparam2', 'bparam1')} for e in ev
                        if e['ev'] == 'event' and x['name'] in e['name'] and timed(e, x)]
                msg += '; played: %s' % json.dumps(seen[:6]) if seen else ''
        elif k == 'spawned':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'spawn' and ent_matches(e['ent'], spec)]
            msg = count_check('spawned %s' % spec, m, x)
        elif k == 'removed':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'remove' and ent_matches(e['ent'], spec)]
            msg = count_check('removed %s' % spec, m, x)
        elif k == 'present':
            spec = ref(x['ent'])
            m = [e for e in run.events if e['ev'] in ('exists', 'spawn') and ent_matches(e['ent'], spec)
                 and (x['at'] is None or ('origin' in e and all(abs(a - b) <= x['tol'] for a, b in zip(e['origin'], x['at']))))]
            msg = None if m else 'no %s%s' % (spec, ' at %s' % x['at'] if x['at'] else '')
        elif k == 'absent':
            spec = ref(x['ent'])
            m = [e for e in run.events if e['ev'] in ('exists', 'spawn') and ent_matches(e['ent'], spec)]
            if m:
                msg = '%s exists (%d times in the trace), expected none' % (spec, len(m))
        elif k == 'precached':
            m = [e for e in run.events if e['ev'] == 'precache' and e['name'].lower() == x['name'].lower()
                 and (x['res'] is None or e['kind'] == x['res'])]
            msg = count_check('precache %s%s' % (x['res'] + ' ' if x['res'] else '', x['name']), m, x)
        elif k == 'console':
            if x['text'] not in run.console:
                msg = 'the console never printed "%s"' % x['text']
        elif k == 'kv_handled':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'kv' and ent_matches(e['ent'], spec) and e['key'] == x['key']]
            if not m:
                msg = 'no test_kv %s on %s' % (x['key'], spec)
            elif not m[-1]['handled']:
                msg = '%s did not take the key %s' % (spec, x['key'])
        if msg:
            r.failures.append(msg)
    return r


def _when(x):
    if x.get('within'):
        return ' within %g-%g s after %s' % (x['within'][0], x['within'][1], x.get('after') or 'begin')
    return ''


def check_run(suite, run):
    """Per test results, and the suite's own problems (crash, engine errors, sanitizer reports)."""
    results = [check_test(t, run) for t in suite.tests]
    problems = []
    for line in run.console.splitlines():
        if FATAL.search(line):
            problems.append('server: ' + line.strip()[:300])
    if not run.ended:
        problems.append('the server stopped before the end of the run (crash, hang or timeout)')
    for e in run.events:
        if e['ev'] in ('error', 'engine_error') and not any(r.test.id and _in(run, r.test.id, e) for r in results):
            problems.append('trace %s outside a test: %s' % (e['ev'], e.get('msg', '')))
    return results, problems


def _in(run, tid, e):
    t0 = run.marks.get(tid + ':begin')
    t1 = run.marks.get(tid + ':end', float('inf'))
    return t0 is not None and t0 <= e['t'] <= t1
