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



def _save_text(h):
    """A saved field's hex as text: absent (zero), a string, or 4-byte words as int / float."""
    if h is None:
        return '0'
    raw = bytes.fromhex(h)
    t = raw.rstrip(b'\0')
    if t and all(32 <= c < 127 for c in t) and (len(raw) % 4 or len(t) > 3):
        return json.dumps(t.decode())
    if len(raw) % 4 == 0:
        import struct
        out = []
        for i in range(0, len(raw), 4):
            iv, = struct.unpack('<i', raw[i:i + 4])
            fv, = struct.unpack('<f', raw[i:i + 4])
            out.append(str(iv) if abs(iv) < 1 << 20 else '%g' % fv)
        return ','.join(out)
    return h

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
            elif x.get('gap') and not x['gap'][0] - 0.05 <= fb[0] - fa[0] <= x['gap'][1] + 0.05:
                msg = 'order: %s fired %.3f s after %s, expected %g-%g s' % (x['then'], fb[0] - fa[0], x['first'],
                                                                            x['gap'][0], x['gap'][1])
        elif k == 'value_any':
            spec = ref(x['ent'])
            got = []
            for lab in x['labels']:
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec)]
                fname, _, comp = x['field'].partition('.')
                g = hit[0]['v'].get(fname, '<missing>') if hit else '<no snapshot>'
                if comp and isinstance(g, list) and comp in 'xyz':
                    g = g['xyz'.index(comp)]      # 'origin.x': one component, as value does
                got.append(g)
            op = x.get('op', '==')
            if op == '==':
                want = x['expected'] if isinstance(x['expected'], list) else [x['expected']]
                seen = [g for g in got if g in want]
            else:
                cmp = {'<': lambda a: a < x['expected'], '<=': lambda a: a <= x['expected'],
                       '>': lambda a: a > x['expected'], '>=': lambda a: a >= x['expected']}[op]
                seen = [g for g in got if isinstance(g, (int, float)) and cmp(g)]
            if not seen:
                msg = '%s.%s never %s%s in %s: %s' % (spec, x['field'], '' if op == '==' else op + ' ',
                                                     json.dumps(x['expected']), ', '.join(x['labels']), json.dumps(got))
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
                if x.get('where'):
                    hit = [s for s in hit if all(s['v'].get(wk) == wv for wk, wv in x['where'].items())]
                if not hit:
                    msg = 'snapshot %s: no entity %s%s' % (x['label'], spec,
                                                            ' with %s' % json.dumps(x['where']) if x.get('where') else '')
                else:
                    fname, _, comp = x['field'].partition('.')
                    got = hit[0]['v'].get(fname, '<missing>')
                    if comp and isinstance(got, list) and comp in 'xyz':
                        got = got['xyz'.index(comp)]
                    elif fname == 'save' and comp and isinstance(got, dict):
                        # 'save.<field>': one saved field (snapshot field 'save'), as an int when it is one
                        got = _save_text(got.get(comp))
                        try:
                            got = int(got)
                        except ValueError:
                            pass
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
        elif k == 'distance_any':
            got = []
            for lab in x['labels']:
                label = '%s:%s' % (test.id, lab)
                pos = []
                for r_ in (x['a'], x['b']):
                    spec = ref(r_)
                    hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == label and e.get('ent')
                           and ent_matches(e['ent'], spec) and 'origin' in e['v']]
                    pos.append(hit[0]['v']['origin'] if hit else None)
                if None in pos:
                    got.append(None)
                    continue
                d = [pos[0][i] - pos[1][i] for i in range(2 if x['flat'] else 3)]
                got.append(sum(c * c for c in d) ** 0.5)
            seen = [(lab, g) for lab, g in zip(x['labels'], got) if g is not None and x['lo'] <= g <= x['hi']]
            if not seen:
                msg = 'distance %s - %s never %g to %g in %s: %s' % (
                    ref(x['a']), ref(x['b']), x['lo'], x['hi'], ', '.join(x['labels']),
                    json.dumps([None if g is None else round(g, 1) for g in got]))
            else:
                r.notes.append('distance %s - %s at %s: %.1f' % (x['a'], x['b'], seen[0][0], seen[0][1]))
        elif k == 'damaged':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'damage' and ent_matches(e['ent'], spec) and timed(e, x)
                 and (x['attacker'] is None or ent_matches(e['attacker'], ref(x['attacker'])))
                 and (x.get('type') is None or (e.get('type', 0) & x['type']) == x['type'])]
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
                fld = x.get('field') or 'origin'
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec) and isinstance(e['v'].get(fld), list)]
                pos.append(hit[0]['v'][fld] if hit else None)
            if None in pos:
                msg = 'moved %s: no %s snapshot %s' % (spec, x.get('field') or 'origin',
                                                      x['label1'] if pos[0] is None else x['label2'])
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
        elif k == 'still_offset':
            cspec, pspec = ref(x['child']), ref(x['parent'])
            still, worst, wlab = 0, 0.0, None
            for lab in x['labels']:
                full = '%s:%s' % (test.id, lab)
                ph = [e for e in ev if e['ev'] == 'snap' and e['label'] == full and e.get('ent')
                      and ent_matches(e['ent'], pspec)]
                ch = [e for e in ev if e['ev'] == 'snap' and e['label'] == full and e.get('ent')
                      and ent_matches(e['ent'], cspec)]
                if not ph or not ch or not isinstance(ph[0]['v'].get('velocity'), list):
                    continue
                if any(abs(v) > 1e-3 for v in ph[0]['v']['velocity']):
                    continue
                still += 1
                po, co = ph[0]['v']['origin'], ch[0]['v']['origin']
                gap = max(abs(co[i] - po[i] - x['offset'][i]) for i in range(3))
                if gap > worst:
                    worst, wlab = gap, lab
            if still < x['min_still']:
                msg = 'still_offset %s on %s: %d snapshots with the parent still, at least %d expected' % (
                    cspec, pspec, still, x['min_still'])
            elif worst > x['tol']:
                msg = '%s is %.3f off its place on %s (still) at %s, at most %g expected' % (cspec, worst, pspec, wlab,
                                                                                         x['tol'])
            else:
                r.notes.append('%s on %s: at most %.3f off over %d still snapshots' % (x['child'], x['parent'], worst,
                                                                                    still))
        elif k == 'max_rate':
            # consecutive snapshots of a vector field: how fast it changed, by the snapshots' game times
            spec = ref(x['ent'])
            pts = []
            for lab in x['labels']:
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, lab) and e.get('ent')
                       and ent_matches(e['ent'], spec) and isinstance(e['v'].get(x['field']), list)]
                if hit:
                    pts.append((hit[0]['t'], hit[0].get('f'), hit[0].get('dt'), hit[0]['v'][x['field']]))
            worst, at = 0.0, None
            for (t1, f1, _, a), (t2, f2, dt2, b) in zip(pts, pts[1:]):
                d = sum((q - p) ** 2 for p, q in zip(a, b)) ** 0.5
                # one frame apart: that frame's length (what a mover moved by); else the game times between them
                span = dt2 if f1 is not None and f2 == f1 + 1 and dt2 else t2 - t1
                if span > 0:
                    rate = d / span
                    if rate > worst:
                        worst, at = rate, t1
            if len(pts) < 2:
                msg = 'max_rate %s: fewer than 2 %s snapshots' % (spec, x['field'])
            elif worst > x['rate']:
                msg = '%s.%s changed at %.1f units/s at t=%.3f, at most %g expected' % (spec, x['field'], worst, at,
                                                                                       x['rate'])
            else:
                r.notes.append('%s.%s at most %.1f units/s over %d snapshots' % (x['ent'], x['field'], worst, len(pts)))
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
        elif k == 'aisound':
            spec = ref(x['ent'])
            hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, x['label'])
                   and e.get('ent') and ent_matches(e['ent'], spec)]
            if not hit or 'aisounds' not in hit[0]['v']:
                msg = 'snapshot %s: no aisounds' % x['label']
            else:
                m = [a for a in hit[0]['v']['aisounds'] if a['type'] == x['type'] and (
                     x['pos'] is None or all(abs(a['pos'][i] - x['pos'][i]) <= x['tol'] for i in range(3)))]
                msg = count_check('AI sound type %d%s' % (x['type'], ' at %s' % x['pos'] if x['pos'] else ''), m, x)
                if msg:
                    msg += '; alive: %s' % json.dumps(hit[0]['v']['aisounds'])
        elif k == 'save_same':
            def saved(r):
                spec = ref(r)
                hit = [e for e in ev if e['ev'] == 'snap' and e['label'] == '%s:%s' % (test.id, x['label'])
                       and e.get('ent') and ent_matches(e['ent'], spec)]
                return hit[0]['v'].get('save') if hit else None
            a, b = saved(x['ent1']), saved(x['ent2'])
            if x.get('differ') and a is not None and b is None:
                pass    # ent2 is gone: it differs
            elif a is None or b is None:
                msg = 'no saved fields of %s' % (x['ent1'] if a is None else x['ent2'])
            else:
                skip = set(x['ignore'])
                if x['noise']:
                    n1, n2 = saved(x['noise'][0]), saved(x['noise'][1])
                    for n in (n1, n2):
                        if n is not None:
                            skip |= {f for f in set(n) | set(b) if n.get(f) != b.get(f)}
                # a name starting with a capital is a block header (CSave::WriteFields' class name: the count of
                # fields written), not a field
                diff = sorted(f for f in set(a) | set(b)
                              if f not in skip and not f[:1].isupper() and a.get(f) != b.get(f))
                if x.get('differ'):
                    if not diff:
                        msg = 'saved fields are the same with the key and without it'
                elif diff:
                    msg = 'saved fields differ (with the keys, without): ' + '; '.join(
                        '%s %s / %s' % (f, _save_text(a.get(f)), _save_text(b.get(f))) for f in diff)
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
        elif k == 'spread':
            pos = []
            for e in ev:
                if e['ev'] != 'msg' or not timed(e, x) or not _msg_is(e, x['name']):
                    continue
                d = decode.decode(e['name'], bytes.fromhex(e['hex']), e['type'])
                if d is None or 'pos' not in d:
                    continue
                if x['fields'] and any(not _close(d.get(fk), _model_ref(e, fv), 0.01) for fk, fv in x['fields'].items()):
                    continue
                pos.append(d['pos'])
            far = max([sum((a - b) ** 2 for a, b in zip(p_, x['center'])) ** 0.5 for p_ in pos] or [0.0])
            if len(pos) < x['min']:
                msg = 'message %s%s: %d with a position, expected at least %d' % (
                    x['name'], ' %s' % json.dumps(x['fields']) if x['fields'] else '', len(pos), x['min']) + _when(x)
            elif not x['lo'] <= far <= x['hi']:
                msg = 'message %s: farthest of %d positions %.1f from %s, expected %g to %g' % (
                    x['name'], len(pos), far, x['center'], x['lo'], x['hi']) + _when(x)
            else:
                msg = None
                r.notes.append('spread %s: farthest %.1f of %d positions' % (x['name'], far, len(pos)))
        elif k == 'sound':
            spec = ref(x['ent']) if x['ent'] else None
            kinds = {None: ('sound', 'ambient'), True: ('ambient',), False: ('sound',)}[x.get('ambient')]
            m = [e for e in ev if e['ev'] in kinds
                 and (x['sample'] in e['sample'] or x['sample'] in e.get('sentence', '')) and timed(e, x)
                 and (spec is None or ent_matches(e['ent'], spec))
                 and (not x.get('flags') or (e.get('flags', 0) & x['flags']) == x['flags'])
                 and (x.get('attn') is None or abs(e.get('attn', -1) - x['attn']) < 0.01)
                 and (x.get('chan') is None or e.get('chan') == x['chan'])
                 and (x.get('vol') is None or x['vol'][0] <= e.get('vol', -1) <= x['vol'][1])
                 and (x.get('pitch') is None or (e.get('pitch') == x['pitch'] if not isinstance(x['pitch'], (list, tuple))
                                                     else x['pitch'][0] <= (e.get('pitch') or 0) <= x['pitch'][1]))]
            msg = count_check('%s %s%s%s%s' % ({None: 'sound', True: 'ambient sound', False: 'entity sound'}[x.get('ambient')],
                                                x['sample'] + (' chan %d' % x['chan'] if x.get('chan') is not None else ''),
                                                ' flags %d' % x['flags'] if x.get('flags') else '',
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
        elif k == 'lightstyle':
            m = [e for e in ev if e['ev'] == 'lightstyle' and timed(e, x)
                 and (x['pattern'] is None or e['pattern'] == x['pattern'])
                 and (x['style'] is None or (e['style'] in x['style'] if isinstance(x['style'], (list, tuple))
                                             else e['style'] == x['style']))]
            msg = count_check('light style %s set to "%s"' % (x['style'] if x['style'] is not None else 'any',
                                                              x['pattern'] if x['pattern'] is not None else '*'), m, x)
            if msg:
                seen = [(e['style'], e['pattern']) for e in ev if e['ev'] == 'lightstyle' and timed(e, x)]
                msg += _when(x) + ('; set: %s' % json.dumps(seen[:8]) if seen else '')
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
            m = [e for e in ev if e['ev'] == 'spawn' and ent_matches(e['ent'], spec) and timed(e, x)]
            msg = count_check('spawned %s' % spec, m, x)
            if msg and x.get('within') is not None:
                msg += _when(x)
        elif k == 'waited':
            done = [e for e in ev if e['ev'] == 'wait_done' and e.get('cond') == x['cond']]
            late = [e for e in ev if e['ev'] == 'wait_timeout' and e.get('cond') == x['cond']]
            if late or not done:
                msg = 'wait for %s: %s' % (x['cond'], 'timed out' if late else 'never ended')
        elif k == 'edicts':
            counts = []
            for lab in (x['label1'], x['label2']):
                f = mark_frame.get(lab)
                if f is None:
                    counts.append(None)
                    continue
                live = set()
                for e in run.events:
                    if e['f'] > f:
                        break
                    if e['ev'] in ('exists', 'spawn'):
                        live.add(e['ent']['i'])
                    elif e['ev'] == 'remove':
                        live.discard(e['i'])
                counts.append(len(live))
            if None in counts:
                msg = 'edicts: no mark %s' % (x['label1'] if counts[0] is None else x['label2'])
            elif not x['lo'] <= counts[1] - counts[0] <= x['hi']:
                msg = 'edicts: %d at %s, %d at %s (difference %+d, expected %+d to %+d)' % (
                    counts[0], x['label1'], counts[1], x['label2'], counts[1] - counts[0], x['lo'], x['hi'])
            else:
                r.notes.append('edicts: %d at %s, %d at %s' % (counts[0], x['label1'], counts[1], x['label2']))
        elif k == 'removed':
            spec = ref(x['ent'])
            m = [e for e in ev if e['ev'] == 'remove' and ent_matches(e['ent'], spec) and timed(e, x)]
            msg = count_check('removed %s' % spec, m, x)
            if msg:
                msg += _when(x)
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
            if x.get('absent'):
                if x['text'] in run.console:
                    msg = 'the console printed "%s"' % x['text']
            elif x['text'] not in run.console:
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
