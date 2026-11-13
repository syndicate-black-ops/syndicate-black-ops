"""Coverage of the FGD's classes, keys, spawnflag bits and choice values by the tests (run.py --coverage).

Two test trees count:
  - the entity suites (tests/entity/suites, generated as --list does): every entity a test places, its keys, spawnflag
    bits and values, test_kv keys and test_create entities;
  - the map-test cases (tests/maps/cases/*.txt, docs/dev/MAP_TESTS.md): every "spawn <class> key value ..." line.

Two levels:
  - set: some test places the class / sets the key, bit or value (it may not check what it does);
  - asserted: a test says it checks it: a map-test case's "# covers:" header, or an entity test's covers=(...) tag
    (@S.test(..., covers=('class.key', 'class:bit'))). Item syntax: "class", "class.key", "class.key=value" (a
    choices value) and "class:bit" (a spawnflag bit). An asserted key, value or bit also counts its class.
    A covers tag counts only from a test that can show the effect (weak_reason): not an xfail, with an expectation
    that checks behavior (not only save_same, which the FGD defaults suites use: a key the game ignores passes it,
    nor only kv_handled), and that sets the item on that class (a key / value / bit on an entity of the class it
    places, test_kv or test_create; a case's spawn lines). A tag that fails this is listed, not counted.

Keys of the shared bases (SHARED: Monster, TalkMonster; tests/fgd_code_paths.json) are counted on every class that
inherits them, like any other key. A test on one class of the base credits the key on the others ("via" credit)
unless the other class has its own code on the key's path (etest/code_paths.py: an override of TakeDamage, Killed,
GibMonster, ... that does not call the base's, or no MonsterInit): such a class needs its own test or exemption.

What is counted (the coverage audit's rules):
point and solid classes; per class, every key but the generic engine-applied base keys (Targetname, Angles, render,
skin, body, ... counted once per base in the audit) and compiler keys (zhlt_*, _*, light_origin); spawnflag bits merged
over the base chain (not Appearflags); the values of choices keys with at most 24 choices. With the map scan
(build/map-entities/entities.jsonl in the main checkout, not committed) it also counts what 1.38 maps use.
"""
import collections
import fnmatch
import glob
import json
import os
import re

GENERIC = {'Targetname', 'Angles', 'RenderFxChoices', 'RenderFields', 'RenderFieldsUnset', 'ModelScale', 'Skin', 'Body',
           'BodySkin', 'Colormap', 'Effects', 'EntityEffects', 'CustomObjectHint', 'Global'}
# expectation kinds that do not show a key's effect: save_same (a key set to its FGD default saves the same fields as
# the key left out; a key the game ignores passes too), kv_handled (KeyValue took the key)
NO_EFFECT = {'save_same', 'kv_handled'}
TOOL_KEYS = ['zhlt_*', '_*', 'light_origin']
MAX_CHOICES = 24
# maps the audit leaves out: HLDM maps, the project's own test maps, the 2.0 showcase
EXCLUDED_MAPS = {'fellaville', 'co_fellaville', 'co_amxx', 'rc_kart_sbotest01', 'syndicate_bot', 'co_sbo_showcase'}


def _int(v):
    try:
        return int(float(v))
    except (TypeError, ValueError):
        return 0


def _tokens(line):
    """A case line's tokens as the runner reads them ("quoted strings" keep spaces, # starts a comment)."""
    out, i = [], 0
    while i < len(line):
        while i < len(line) and line[i] <= ' ':
            i += 1
        if i >= len(line) or line[i] == '#':
            break
        if line[i] == '"':
            j = line.find('"', i + 1)
            j = len(line) if j < 0 else j
            out.append(line[i + 1:j])
            i = j + 1
        else:
            j = i
            while j < len(line) and line[j] > ' ':
                j += 1
            out.append(line[i:j])
            i = j
    return out


class Model:
    """The FGD items to cover."""

    def __init__(self, fgd, classes):
        self.fgd, self.classes = fgd, classes
        self.concrete = sorted(n for n, c in classes.items() if c['kind'] in ('pointclass', 'solidclass'))
        self.keys, self.flags, self.choices = {}, {}, {}   # (class, key) / (class, bit) / (class, key, value) -> label
        self.all_choices = set()                          # every (class, key, value) of a choices key, counted or not
        self.definer = {}                                 # (class, key) -> the FGD class that defines the key
        self.defaults = {}                                # (class, key) -> its FGD default (None: none)
        for cn in self.concrete:
            props = fgd.effective_props(classes, cn)
            for c in fgd.base_chain(classes, cn):
                sp = classes[c]['props'].get('spawnflags')
                if c != 'Appearflags' and sp and sp['choices']:
                    for v in sp['choices']:
                        if re.match(r'^\d+$', str(v)):
                            self.flags[(cn, int(v))] = True
            for kl, p in props.items():
                if kl == 'spawnflags' or any(fnmatch.fnmatchcase(kl, x) for x in TOOL_KEYS):
                    continue
                if self._definer(cn, kl) in GENERIC:
                    continue
                self.keys[(cn, kl)] = p['name']
                self.definer[(cn, kl)] = self._definer(cn, kl)
                self.defaults[(cn, kl)] = p.get('default')
                if p['type'] == 'choices' and p['choices']:
                    for v in p['choices']:
                        # every value is valid in a covers tag; only keys with at most MAX_CHOICES values are counted
                        self.all_choices.add((cn, kl, str(v).strip('"')))
                        if len(p['choices']) <= MAX_CHOICES:
                            self.choices[(cn, kl, str(v).strip('"'))] = True

    def _definer(self, cn, kl):
        d = None
        for c in self.fgd.base_chain(self.classes, cn):
            if kl in self.classes[c]['props']:
                d = c
        return d


class Hits:
    """Items a set of tests reaches: class, (class, key), (class, bit), (class, key, value) -> test names."""

    def __init__(self):
        self.cls, self.key, self.bit, self.val = (collections.defaultdict(set) for _ in range(4))
        self.named = set()      # lower-case values the placed keys hold (a key name trigger_changevalue sets)

    def place(self, test, cn, kv, concrete=None):
        """concrete: the FGD's point / solid classes; a key whose value names one (monstermaker monstertype) places
        that class too, with the maker's child keys (childspawnflags -> spawnflags, child<key> -> <key>)."""
        self.cls[cn].add(test)
        for k, v in kv:
            self.named.add(str(v).lower())
            if concrete and v in concrete and v != cn:
                child = [('spawnflags' if kk.lower() == 'childspawnflags' else kk[5:] if kk.lower().startswith('child')
                          else kk, vv) for kk, vv in kv if kk.lower() not in ('targetname', 'monstertype', 'origin')]
                self.place(test, v, child)
        for k, v in kv:
            kl = k.lower()
            self.key[(cn, kl)].add(test)
            self.val[(cn, kl, v)].add(test)
            if kl == 'spawnflags':
                sf = _int(v)
                for b in range(32):
                    if sf & (1 << b):
                        self.bit[(cn, 1 << b)].add(test)

    def cover(self, test, item):
        """A covers tag item; returns False when it is not well formed."""
        m = re.match(r'^([A-Za-z_][\w]*)(?::(\d+)|\.([\w]+)(?:=(.*))?)?$', item)
        if not m:
            return False
        cn, bit, key, val = m.groups()
        self.cls[cn].add(test)
        if bit:
            self.bit[(cn, int(bit))].add(test)
        if key:
            self.key[(cn, key.lower())].add(test)
            if val is not None:
                self.val[(cn, key.lower(), val)].add(test)
        return True


ITEM = re.compile(r'^([A-Za-z_][\w]*)(?::(\d+)|\.([\w]+)(?:=(.*))?)?$')


def item_set(item, here, fallback=None, defaults=None):
    """Whether the covers item is set in `here` (Hits of one test): the key, value or bit on an entity of the class.
    A key also counts when a value in the test names it (trigger_changevalue m_iszValueName) or, with the class
    placed, when the FGD gives it a default (a test of the key left out); a value also when it is the key's FGD
    default (defaults: (class, key) -> default). A class the test does not place at all may come from elsewhere in
    the map: then `fallback` (the suite's Hits, or None: accept) decides."""
    m = ITEM.match(item)
    if not m:
        return True
    cn, bit, key, val = m.groups()
    if cn not in here.cls:
        if fallback is None:
            return True
        here = fallback
        if cn not in here.cls:
            return False
    defaults = defaults or {}
    if bit:
        return bool(here.bit.get((cn, int(bit))))
    if key:
        kl = key.lower()
        dflt = defaults.get((cn, kl))
        if val is not None:
            return bool(here.val.get((cn, kl, val))) or (dflt is not None and str(dflt).strip('"') == val)
        return bool(here.key.get((cn, kl))) or kl in here.named or dflt not in (None, '')
    return True


def weak_reason(item, xfail, kinds, here, fallback=None, defaults=None):
    """Why a covers item of a test does not count (None: it counts)."""
    if xfail:
        return 'xfail'
    if not any(k not in NO_EFFECT for k in kinds):
        return 'no effect expectation (%s)' % (', '.join(sorted(set(kinds))) or 'none')
    if not item_set(item, here, fallback, defaults):
        return 'the test does not set it'
    return None


def read_cases(cases_dir, placed, asserted, problems, weak=None, model=None):
    """Map-test cases: spawn lines are placed; "# covers:" items are asserted (when the case has an expect line and
    sets the item: weak_reason; else they go to weak)."""
    n = 0
    for path in sorted(glob.glob(os.path.join(cases_dir, '*.txt'))):
        name = 'cases:' + os.path.basename(path)[:-4]
        n += 1
        header = True
        items, here, kinds = [], Hits(), []
        for line in open(path, encoding='latin-1'):
            line = line.rstrip('\n')
            if header and line.startswith('#'):
                m = re.match(r'^#\s*covers:\s*(.*)$', line)
                if m:
                    items += m.group(1).split()
                continue
            header = False
            t = _tokens(line)
            if len(t) >= 2 and t[0] == 'spawn':
                kv = list(zip(t[2::2], t[3::2]))
                placed.place(name, t[1], kv)
                here.place(name, t[1], kv, set(model.concrete) if model else None)
            elif t and t[0].startswith('expect'):
                kinds.append(t[0])
        for item in items:
            why = weak_reason(item, False, kinds, here, None, model.defaults if model else None)
            if why and weak is not None:
                weak[item].append((name, why))
                if ITEM.match(item):
                    continue
            if not asserted.cover(name, item):
                problems.append('%s: covers item not understood: %s' % (name, item))
    return n


def case_classes(cases_dir):
    """The classes the map-test cases spawn."""
    placed = Hits()
    read_cases(cases_dir, placed, Hits(), [])
    return set(placed.cls)


def _place_test(h, name, s, t, concrete=None):
    """Everything test t of suite s places into Hits h (entities, test_kv, test_create, the suite's world keys)."""
    names = {'world': 'worldspawn'}
    for e in t.ents:
        h.place(name, e.classname, list(e.kv.items()), concrete)
        if e.name:
            names[e.name] = e.classname
    for st in t.steps:
        w = st.split(' ')
        if w[0] == 'test_kv' and len(w) >= 3 and names.get(w[1].strip('"')):
            h.place(name, names[w[1].strip('"')], [(w[2], ' '.join(w[3:]).strip('"'))], concrete)
        if w[0] == 'test_create' and len(w) >= 5:
            rest = w[5:]
            if len(rest) % 2:
                rest = rest[1:]
            h.place(name, w[1], [(rest[i], rest[i + 1].strip('"')) for i in range(0, len(rest) - 1, 2)], concrete)
    wk = getattr(s, 'world_keys', None) or {}
    if wk:
        h.place(name, 'worldspawn', list(wk.items()))


def read_suites(suites, placed, asserted, problems, weak=None, model=None):
    """The suites' tests: what they place goes to placed; their covers items to asserted, or, with weak (a
    defaultdict(list)), to weak[item] with the reason when the test cannot show the effect (weak_reason)."""
    n = 0
    for s in suites:
        suite_h = Hits()
        per = []
        for t in s.tests:
            n += 1
            name = '%s:%s' % (s.name, t.key)
            here = Hits()
            concrete = set(model.concrete) if model else None
            _place_test(here, name, s, t, concrete)
            _place_test(placed, name, s, t)
            _place_test(suite_h, name, s, t, concrete)
            per.append((t, name, here))
        for t, name, here in per:
            kinds = [e.get('kind') for e in getattr(t, 'expectations', ())]
            for item in getattr(t, 'covers', ()) or ():
                if weak is not None and ITEM.match(item):
                    why = weak_reason(item, getattr(t, 'xfail', None), kinds, here, suite_h,
                                      model.defaults if model else None)
                    if why:
                        weak[item].append((name, why))
                        continue
                if not asserted.cover(name, item):
                    problems.append('%s: covers item not understood: %s' % (name, item))
    return n


def collect(suites, cases_dir, model=None):
    """placed, asserted (the covers tags that count), weak (item -> [(test, reason)]), problems, named (Hits of the
    tags that do not count, to check their names)."""
    placed, asserted, problems = Hits(), Hits(), []
    weak = collections.defaultdict(list)
    read_suites(suites, placed, asserted, problems, weak, model)
    read_cases(cases_dir, placed, asserted, problems, weak, model)
    # a tag that does not count still names its item: check the name like a counted one
    named = Hits()
    for item in weak:
        named.cover('-', item)
    return placed, asserted, weak, problems, named


def read_maps(path):
    """1.38 map use from the map scan (optional): Hits with map names instead of test names."""
    if not os.path.exists(path):
        return None
    used = Hits()
    for line in open(path, encoding='utf-8'):
        e = json.loads(line)
        if e.get('map') in EXCLUDED_MAPS:
            continue
        kv = [(k, v if isinstance(v, str) else v[0]) for k, v in e['kv'].items()]
        cn = dict(kv).get('classname')
        if cn:
            used.place(e['map'], cn, [(k, v) for k, v in kv if k != 'classname'])
    return used


def _name_problems(fgd, classes, model, hits):
    """covers items that name nothing in the FGD (a typo, or a class / key the FGD does not offer)."""
    out = []
    known_cls = set(model.concrete)
    for (cn, kl), tests in sorted(hits.key.items()):
        if (cn, kl) not in model.keys and cn in known_cls and not any(
                kl in classes[c]['props'] for c in fgd.base_chain(classes, cn)):
            out.append('covers %s.%s (%s): no such key on the class in the FGD' % (cn, kl, ', '.join(sorted(tests))))
    for cn, tests in sorted(hits.cls.items()):
        if cn not in known_cls:
            out.append('covers %s (%s): no such point / solid class in the FGD' % (cn, ', '.join(sorted(tests))))
    for (cn, bit), tests in sorted(hits.bit.items()):
        if cn in known_cls and (cn, bit) not in model.flags:
            out.append('covers %s:%d (%s): no such spawnflag in the FGD' % (cn, bit, ', '.join(sorted(tests))))
    for (cn, kl, v), tests in sorted(hits.val.items()):
        if cn in known_cls and (cn, kl) in model.keys and (cn, kl, v) not in model.all_choices:
            out.append('covers %s.%s=%s (%s): not one of the key\'s FGD choices' % (cn, kl, v, ', '.join(sorted(tests))))
    return out


def _merge(a, b):
    out = Hits()
    for h in (a, b):
        for f in ('cls', 'key', 'bit', 'val'):
            for k, v in getattr(h, f).items():
                getattr(out, f)[k] |= v
    return out


def shared_credit(model, asserted, paths=None):
    """Keys and choice values of the SHARED bases that a class gets from another class's test.
    Returns (via, blocked): item -> (donor classes, own code). via: credited (the class keeps the base's code; own
    code that extends a method is listed); blocked: not credited, the class replaces a method on the key's path or
    never calls MonsterInit (own is None: its C++ class is not known)."""
    from . import code_paths
    paths = paths or code_paths.Paths()
    via, blocked = {}, {}
    by_def = collections.defaultdict(list)
    for (cn, kl), d in model.definer.items():
        d = paths.base_for(d)
        if d in paths.bases:
            by_def[(d, kl)].append(cn)
    items = [((cn, kl), None) for (cn, kl) in model.keys] + [((cn, kl), v) for (cn, kl, v) in model.choices]
    for (cn, kl), v in items:
        d = paths.base_for(model.definer.get((cn, kl)))
        if d not in paths.bases:
            continue
        it = (cn, kl) if v is None else (cn, kl, v)
        got = asserted.key if v is None else asserted.val
        if got.get(it):
            continue
        donors = sorted(c for c in by_def[(d, kl)] if c != cn and got.get((c, kl) if v is None else (c, kl, v)))
        if not donors:
            continue
        own = paths.own_code(d, cn, kl, v)
        if own is None:
            blocked[it] = (donors, None)
        elif any(how != 'extends' for _, _, how in own):
            blocked[it] = (donors, [x for x in own if x[2] != 'extends'])
        else:
            via[it] = (donors, own)
    return via, blocked


def _weak_note(weak, name):
    w = weak.get(name)
    if not w:
        return ''
    return '; covers tags that do not count: ' + ', '.join('%s (%s)' % x for x in sorted(set(w))[:4]) + \
        (' ...' if len(w) > 4 else '')


def report(fgd, classes, suites, cases_dir, map_scan=None, out_dir=None):
    """The coverage table (lines to print); with out_dir, per-item TSV files there."""
    model = Model(fgd, classes)
    placed, asserted, weak, problems, named = collect(suites, cases_dir, model)
    n_suite = sum(len(s.tests) for s in suites)
    n_case = len(glob.glob(os.path.join(cases_dir, '*.txt')))
    used = read_maps(map_scan) if map_scan else None
    via, _ = shared_credit(model, asserted)

    kinds = [('classes', [(c,) for c in model.concrete], lambda h, it: h.cls.get(it[0])),
             ('keys', sorted(model.keys), lambda h, it: h.key.get(it)),
             ('spawnflag bits', sorted(model.flags), lambda h, it: h.bit.get(it)),
             ('choice values', sorted(model.choices), lambda h, it: h.val.get(it))]
    lines = ['coverage of sdk/syndicate.fgd: %d entity-suite tests, %d map-test cases' % (n_suite, n_case),
             '%-15s %6s %6s %9s %6s' % ('', 'FGD', 'set', 'asserted', 'via') + ('  %8s %13s' % ('map-used', 'map-used set')
                                                                         + ' %18s' % 'map-used asserted' if used else '')]
    tables = {}
    for kind, items, get in kinds:
        s = sum(1 for it in items if get(placed, it))
        a = sum(1 for it in items if get(asserted, it))
        v = sum(1 for it in items if it in via)
        line = '%-15s %6d %6d %9d %6d' % (kind, len(items), s, a, v)
        if used:
            mu = [it for it in items if get(used, it)]
            line += '  %8d %13d %18d' % (len(mu), sum(1 for it in mu if get(placed, it)),
                                         sum(1 for it in mu if get(asserted, it) or it in via))
        lines.append(line)
        tables[kind] = [(it, len(get(placed, it) or ()), len(get(asserted, it) or ()),
                         len(get(used, it) or ()) if used else '',
                         (' '.join(sorted(get(asserted, it) or ())) or
                          ('via ' + ' '.join(via[it][0]) if it in via else ''))[:200])
                        for it in items]
    problems += _name_problems(fgd, classes, model, _merge(asserted, named))
    n_weak = sum(len(v) for v in weak.values())
    if n_weak:
        lines.append('covers tags that do not count (%d tags on %d items: xfail, no effect expectation, or the test '
                     'does not set the item)' % (n_weak, len(weak)))
    if problems:
        lines.append('covers tags to check (%d):' % len(problems))
        lines += ['  ' + p for p in problems]
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        for kind, rows in tables.items():
            with open(os.path.join(out_dir, kind.replace(' ', '_') + '.tsv'), 'w') as fh:
                fh.write('item\ttests_setting\ttests_asserting\tmaps_using\tasserted_by\n')
                for it, s, a, m, by in rows:
                    fh.write('%s\t%s\t%s\t%s\t%s\n' % (item_name(kind, it), s, a, m, by))
        with open(os.path.join(out_dir, 'weak_covers.tsv'), 'w') as fh:
            fh.write('item\ttest\treason\n')
            for item, w in sorted(weak.items()):
                for t, why in sorted(set(w)):
                    fh.write('%s\t%s\t%s\n' % (item, t, why))
        lines.append('per-item tables: %s/{classes,keys,spawnflag_bits,choice_values,weak_covers}.tsv' % out_dir)
    return lines


def item_name(kind, it):
    if kind == 'classes':
        return it[0]
    if kind == 'spawnflag bits':
        return '%s:%d' % it
    return '.'.join(it[:2]) + ('=' + it[2] if len(it) == 3 else '')


VAGUE = re.compile(r'\b(hard|difficult|tricky|later|todo|tbd|maybe|probably)\b', re.I)


def gate(fgd, classes, suites, cases_dir, exemptions_path, paths=None):
    """The coverage gate: (failures, lines). Every FGD item the coverage counts (classes, keys, spawnflag bits, choice
    values of keys with at most MAX_CHOICES values; keys of the GENERIC bases and compiler keys are not counted) must
    be asserted on its class by a test that can show the effect (covers= / "# covers:", weak_reason), credited from
    another class of a SHARED base whose code the class keeps (shared_credit), or exempted in exemptions_path with a
    specific reason. It fails too on an exemption for an item the FGD no longer has, an exemption for an item a test
    asserts, a vague or short reason, and a covers tag that names nothing in the FGD."""
    from . import code_paths
    model = Model(fgd, classes)
    placed, asserted, weak, problems, named = collect(suites, cases_dir, model)
    paths = paths or code_paths.Paths()
    via, blocked = shared_credit(model, asserted, paths)
    data = json.load(open(exemptions_path)) if os.path.exists(exemptions_path) else {'items': {}}
    exempt = data.get('items', {})
    kinds = [('classes', [(c,) for c in model.concrete], lambda it: asserted.cls.get(it[0])),
             ('keys', sorted(model.keys), lambda it: asserted.key.get(it)),
             ('spawnflag bits', sorted(model.flags), lambda it: asserted.bit.get(it)),
             ('choice values', sorted(model.choices), lambda it: asserted.val.get(it))]
    fails, known = [], set()
    counts = []
    for kind, items, get in kinds:
        n_a = n_v = n_e = 0
        for it in items:
            name = item_name(kind, it)
            known.add(name)
            if get(it):
                n_a += 1
                if name in exempt:
                    fails.append('%s: asserted by a test and exempted: drop the exemption' % name)
            elif it in via:
                n_v += 1
                if name in exempt:
                    fails.append('%s: credited through %s (shared base code) and exempted: drop the exemption'
                                 % (name, via[it][0][0]))
            elif name in exempt:
                n_e += 1
            elif it in blocked:
                donors, own = blocked[it]
                fails.append('%s: no test on this class; tests on %s%s do not count, its own code: %s%s'
                             % (name, ', '.join(donors[:3]), ' ...' if len(donors) > 3 else '',
                                'C++ class not found' if own is None else code_paths.describe(own),
                                _weak_note(weak, name)))
            else:
                fails.append('%s: no test asserts it and it is not exempted%s' % (name, _weak_note(weak, name)))
        counts.append('%-15s %5d items: %5d asserted, %5d via a shared base, %5d exempted, %5d open'
                      % (kind, len(items), n_a, n_v, n_e, len(items) - n_a - n_v - n_e))
    for name, why in sorted(exempt.items()):
        if name not in known:
            fails.append('exemption %s: no such item in the FGD (or not counted): drop it' % name)
        if not isinstance(why, str) or len(why) < 20 or VAGUE.search(why):
            fails.append('exemption %s: the reason must say specifically why (%r)' % (name, why))
    fails += _name_problems(fgd, classes, model, _merge(asserted, named))
    fails += problems
    n_weak = sum(len(v) for v in weak.values())
    lines = ['coverage gate (sdk/syndicate.fgd, tests/%s):' % os.path.basename(exemptions_path)] + ['  ' + c for c in counts]
    lines.append('  covers tags that do not count: %d tags on %d items (xfail, no effect expectation, or the test does '
                 'not set the item; run.py --coverage --coverage-out DIR lists them)' % (n_weak, len(weak)))
    lines.append('  open because the class has its own code on the key\'s path: %d (etest/code_paths.py)'
                 % sum(1 for it in blocked if item_name('keys' if len(it) == 2 else 'choice values', it) not in exempt))
    lines += ['FAIL %s' % f for f in fails]
    lines.append('coverage gate: %s (%d failure(s))' % ('PASS' if not fails else 'FAIL', len(fails)))
    return fails, lines
