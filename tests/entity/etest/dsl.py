"""How a test is written (docs/dev/ENTITY_TESTS.md "How a test is written").

A suite is one generated map; each test gets its own sealed room in it and its own prefix for every entity name
(t07_cv), so tests never see each other. A test function declares entities, steps and expectations:

    S = Suite('sbo', 'SBO entities on develop')

    @S.test('trigger_changevalue adds to health', doc='TRIGGER_CHANGEVALUE.md#trigger_changevalue')
    def changevalue_add(t):
        t.brush('func_breakable', size=(32, 32, 32), targetname='crate', health='50')
        t.point('trigger_changevalue', targetname='cv', target='crate', m_iszValueName='health',
                m_iszNewValue='25', m_iszValueType='1')
        t.fire('cv')
        t.wait(0.1)
        t.snap('after', 'crate', 'health')
        t.expect.value('after', 'crate', 'health', 75)
        t.expect.used('crate', caller='cv')

Entity references in steps and expectations: a local name (prefixed for you), 'player1'.. (the test's players, in
the order the test asked for them), '@classname', '#index', 'world', '!activator' / '!caller' (kept), and '=name'
for a global name that must not be prefixed (game_playerjoin, ...).
"""

import re

from . import mapkit

GLOBAL_NAME_KEYS = {'targetname', 'target', 'killtarget', 'master'}
# FGD string keys that name entities (target1..16, trigger_on_*, the logic and calc entities' outputs and operands)
NAME_KEY_PATTERN = re.compile(r'^(target\d+|trigger_on_\w+|trigger_after_run|fire_when_\w+|pass_target|fail_target|'
                              r'store_result|moveto|left_operand|right_operand|third_operand|m_iszAltTarget|'
                              r'm_iszBothTarget|spawnorigin|face_position|movewith|parent|m_iszNewParent)$')


class Ent:
    def __init__(self, test, classname, kv, at, brush):
        self.test, self.classname, self.kv, self.at, self.brush = test, classname, kv, at, brush

    @property
    def name(self):
        return self.kv.get('targetname')


class Expectations:
    """t.expect.<kind>(...): checked after the run against the trace (etest/check.py)."""

    def __init__(self, test):
        self._t = test

    def _add(self, kind, **args):
        self._t.expectations.append(dict(kind=kind, **args))

    def fired(self, target, caller=None, activator=None, use=None, count=None, min=1, after=None, within=None):
        """FireTargets fired `target` (by name). within=(lo, hi): seconds after the mark `after` (default: begin)."""
        self._add('fired', target=self._t.ref(target), caller=caller, activator=activator, use=use, count=count,
                  min=min, after=after, within=within)

    def not_fired(self, target, after=None, within=None):
        self._add('fired', target=self._t.ref(target), caller=None, activator=None, use=None, count=0, min=0,
                  after=after, within=within)

    def used(self, ent, caller=None, activator=None, use=None, count=None, min=1, after=None, within=None):
        """The entity's Use was called (any caller: FireTargets, buttons, the engine)."""
        self._add('used', ent=ent, caller=caller, activator=activator, use=use, count=count, min=min,
                  after=after, within=within)

    def not_used(self, ent, after=None, within=None):
        self._add('used', ent=ent, caller=None, activator=None, use=None, count=0, min=0, after=after, within=within)

    def order(self, first, then, gap=None):
        """The first firing of `first` comes before the first firing of `then`; gap (lo, hi): that many seconds
        later."""
        self._add('order', first=self._t.ref(first), then=self._t.ref(then), gap=list(gap) if gap else None)

    def value(self, label, ent, field, expected, tol=0.01, op='==', where=None):
        """A field of the snapshot `label` (t.snap) equals `expected` (numbers and vectors within tol).
        op '<', '<=', '>', '>=' compare numbers instead; '&' all the bits of `expected` are set, '!&' none is; '!=' any other value.
        field 'velocity.y': one component of a vector. where {field: value}: of the entities `ent` matches (e.g.
        '@env_sprite', every one of them snapped), the one whose snapped fields have those values."""
        self._add('value', label=label, ent=ent, field=field, expected=expected, tol=tol, op=op, where=where)

    def value_any(self, labels, ent, field, expected, op='=='):
        """A field equals `expected` (or one of a list) in at least one of the snapshots `labels`; op '<', '<=', '>',
        '>=': a number compares so with `expected` in at least one of them."""
        self._add('value_any', labels=list(labels), ent=ent, field=field, expected=expected, op=op)

    def distance(self, label, a, b, lo, hi, flat=True):
        """In snapshot `label` (which must hold the origins of both), the distance between a and b is in [lo, hi];
        flat: horizontal distance only."""
        self._add('distance', label=label, a=a, b=b, lo=lo, hi=hi, flat=flat)

    def distance_any(self, labels, a, b, lo, hi, flat=True):
        """The distance between a and b is in [lo, hi] in at least one of the snapshots `labels` (each holding the
        origins of both); flat: horizontal distance only."""
        self._add('distance_any', labels=list(labels), a=a, b=b, lo=lo, hi=hi, flat=flat)

    def damaged(self, ent, attacker=None, min=1, count=None, after=None, within=None, total=None, type=None):
        """`ent` took damage (TakeDamage, any source) from `attacker`. total=(lo, hi): the summed amount. type: DMG_*
        bits every counted hit carries."""
        self._add('damaged', ent=ent, attacker=attacker, min=min, count=count, after=after, within=within, total=total,
                  type=type)

    def not_damaged(self, ent, attacker=None, after=None, within=None):
        self._add('damaged', ent=ent, attacker=attacker, min=0, count=0, after=after, within=within, total=None)

    def ratio(self, label1, ent1, label2, ent2, field, ratio, tol=0.05):
        """field in snapshot 2 / field in snapshot 1 = ratio (within tol, relative); field may be 'v.y'."""
        self._add('ratio', label1=label1, ent1=ent1, label2=label2, ent2=ent2, field=field, ratio=ratio, tol=tol)

    def moved(self, label1, label2, ent, delta=None, tol=1.0, flat=False, min_dist=None, field=None):
        """origin in snapshot 2 - origin in snapshot 1 = delta (a vector, within tol per component); with min_dist
        instead of delta: it moved at least that far. field: another vector field instead of origin (e.g.
        'fullpack_origin:player1', what a client is sent)."""
        assert (delta is None) != (min_dist is None), 'moved: delta or min_dist'
        self._add('moved', label1=label1, label2=label2, ent=ent, delta=list(delta) if delta else None, tol=tol,
                  flat=flat, min_dist=min_dist, **({'field': field} if field else {}))

    def max_rate(self, labels, ent, field, rate):
        """Over the snapshots `labels` in order, a vector field never changes faster than `rate` units per second
        of game time between two of them (e.g. a mover sent to a client steps no further than its speed)."""
        self._add('max_rate', labels=list(labels), ent=ent, field=field, rate=rate)

    def still_offset(self, labels, child, parent, offset, tol=0.05, min_still=3):
        """In every snapshot of `labels` (each holding `origin` and `velocity` of `parent`, `origin` of `child`) where
        the parent stands still (velocity 0), child origin - parent origin == offset within tol; at least `min_still`
        such snapshots (e.g. a held chain: every child at its place on the held parent, with no gap)."""
        self._add('still_offset', labels=list(labels), child=child, parent=parent, offset=list(offset), tol=tol,
                  min_still=min_still)

    def same(self, label1, label2, ent, field):
        """A field has the same value in two snapshots."""
        self._add('same', label1=label1, label2=label2, ent=ent, field=field)

    def aisound(self, label, ent, type, pos=None, tol=4.0, count=None, min=1):
        """Snapshot field 'aisounds' of `ent` at `label` (any entity: the list is global) has AI sounds of `type`
        (bits, CSoundEnt), at `pos` (room-relative) within tol when given."""
        self._add('aisound', label=label, ent=ent, type=type,
                  pos=[float(v) for v in self._t._abs(pos)] if pos is not None else None, tol=tol, count=count,
                  min=min)

    def save_same(self, label, ent1, ent2, noise=None, ignore=(), differ=False):
        """Two entities' saved fields (snapshot field 'save') are equal at `label`. noise: two more entities of the
        same setup as ent2, whose differing fields are left out (random values); ignore: field names left out.
        differ: the opposite, at least one field differs (or ent2 is gone)."""
        self._add('save_same', label=label, ent1=ent1, ent2=ent2, noise=list(noise) if noise else None,
                  ignore=list(ignore), differ=differ)

    def msg(self, name, to=None, min=1, count=None, fields=None, after=None, within=None, max=None):
        """A user message `name` was sent (to: a player reference). fields: decoded values that must match.
        max: at most that many (with min: a range)."""
        self._add('msg', name=name, to=to, min=min, count=count, fields=fields, after=after, within=within, max=max)

    def spread(self, name, center, lo, hi, fields=None, min=1, after=None, within=None):
        """Messages `name` (with these decoded `fields`) that carry a position ('pos', e.g. the temp entities of
        shots: TE_GUNSHOTDECAL, TE_SPARKS, TE_EXPLOSION): at least `min` of them, and the largest distance of a
        position from `center` (absolute coordinates, t._abs) is in [lo, hi] (a firing cone)."""
        self._add('spread', name=name, center=list(center), lo=lo, hi=hi, fields=fields, min=min, after=after,
                  within=within)

    def no_msg(self, name, to=None, after=None, within=None):
        self._add('msg', name=name, to=to, min=0, count=0, fields=None, after=after, within=within)

    def sound(self, sample, ent=None, min=1, count=None, after=None, within=None, flags=None, attn=None, vol=None,
              pitch=None, ambient=None, chan=None, max=None):
        """A sound whose sample name (or, for a sentence sent by its number, the sentence name, e.g. "SC_POK") contains
        `sample` was emitted (by `ent`). flags: bits that must all be set
        (e.g. 32, SND_STOP: the sound was stopped). attn: its attenuation; vol: a (lo, hi) range of its volume.
        ambient: True = only ambient sounds (EMIT_AMBIENT_SOUND, sent to every client), False = only the entity's
        own sounds (EMIT_SOUND, sent to the clients in its PAS); None = either. chan: its channel (CHAN_WEAPON 1,
        CHAN_VOICE 2, CHAN_ITEM 3, CHAN_BODY 4, CHAN_STATIC 6); sample '' matches every sample. max: at most that
        many (with min: a range, for a rate)."""
        self._add('sound', sample=sample, ent=ent, min=min, count=count, after=after, within=within, flags=flags,
                  attn=attn, vol=vol, pitch=pitch, ambient=ambient, chan=chan, max=max)

    def setview(self, ent, view, min=1, count=None, after=None, within=None):
        """The engine was told to show player `ent` the view of `view` (SET_VIEW; `ent` itself: his own view back)."""
        self._add('setview', ent=ent, view=view, min=min, count=count, after=after, within=within)

    def clcmd(self, text, ent=None, min=1, count=None, after=None, within=None):
        """The game sent client `ent` (any player when None) a console command containing `text` (CLIENT_COMMAND)."""
        self._add('clcmd', text=text, ent=ent, min=min, count=count, after=after, within=within)

    def lightstyle(self, pattern=None, style=None, min=1, count=None, after=None, within=None):
        """The game set a light style (LIGHT_STYLE) to `pattern` (None: any). style: its number, a list / range of
        numbers (named lights have 32-63), or None for any."""
        if isinstance(style, range):
            style = list(style)
        self._add('lightstyle', pattern=pattern, style=style, min=min, count=count, after=after, within=within)

    def event(self, name, ent=None, fields=None, min=1, count=None, after=None, within=None):
        """An event (PLAYBACK_EVENT_FULL) whose precached name contains `name` was played (by `ent`). fields: values
        of fparam1, fparam2, iparam1, iparam2, bparam1, bparam2 that must match; a (lo, hi) pair is a range."""
        self._add('event', name=name, ent=ent, fields=fields, min=min, count=count, after=after, within=within)

    def spawned(self, ent, min=1, count=None, after=None, within=None):
        """Entities matching `ent` ('@classname' or a name) were created during the test (after / within: a mark and a
        window, as for fired)."""
        self._add('spawned', ent=ent, min=min, count=count, after=after, within=within)

    def removed(self, ent, min=1, count=None, after=None, within=None):
        """Entities matching `ent` were removed (after / within: a mark and a window, as for fired)."""
        self._add('removed', ent=ent, min=min, count=count, after=after, within=within)

    def not_removed(self, ent, after=None, within=None):
        self._add('removed', ent=ent, min=0, count=0, after=after, within=within)

    def present(self, ent, at=None, tol=2.0):
        """An entity matching `ent` exists at the map start or is spawned during the run; at: within `tol` units of
        this position (relative to the room's floor center)."""
        self._add('present', ent=ent, at=list(self._t._abs(at)) if at is not None else None, tol=tol)

    def absent(self, ent):
        """No entity matching `ent` exists at any time of the run (e.g. removed while the map spawned)."""
        self._add('absent', ent=ent)

    def precached(self, name, kind=None):
        """`name` (case-insensitive) was precached at any time of the run, the map load included. kind: 'model',
        'sound' or 'generic'."""
        self._add('precached', name=name, res=kind, min=1, count=None)

    def not_precached(self, name, kind=None):
        self._add('precached', name=name, res=kind, min=0, count=0)

    def console(self, text):
        """The server console printed `text` at any time of the run, the map load included (a suite that needs
        developer messages sets Suite.server_args = ('+developer', '1'))."""
        self._add('console', text=text)

    def no_console(self, text):
        """The server console never printed `text` during the run, the map load included."""
        self._add('console', text=text, absent=True)

    def edicts(self, label1, label2, lo=0, hi=0):
        """The number of entities in the game at mark `label2` minus the number at mark `label1` is in [lo, hi] (from
        the trace's entity scan: a leaked or lost edict shows as a difference)."""
        self._add('edicts', label1=label1, label2=label2, lo=lo, hi=hi)

    def kv_handled(self, ent, key):
        """test_kv on `ent` / `key` was taken by the entity (KeyValue set fHandled)."""
        self._add('kv_handled', ent=ent, key=key)


class Test:
    def __init__(self, suite, func, title, doc, players, size, tags, xfail=None, covers=()):
        self.suite, self.func, self.title, self.doc = suite, func, title, doc
        self.xfail = xfail      # a known bug: failing is expected (XFAIL), passing is reported (XPASS)
        # FGD items the test's expectations check ('class', 'class.key', 'class.key=value', 'class:bit'), counted
        # as asserted by run.py --coverage (etest/coverage.py)
        self.covers = tuple(covers)
        self.players, self.size, self.tags = players, size, tags
        self.key = func.__name__
        self.id = None
        self.ents = []
        self.steps = []
        self.expectations = []
        self.seq = []
        self.duration = 0.0     # seconds of game time the steps wait (for the timeout)
        self.room = None        # (x0, y0, z0, x1, y1, z1) inside, set by the suite layout
        self.expect = Expectations(self)

    # -------------------------------------------------------------------- names
    @property
    def prefix(self):
        return self.id + '_'

    def name(self, local):
        """The real name of a local entity name (for text that names entities, e.g. a sequence file)."""
        return self.prefix + local

    def ref(self, r):
        if r is None:
            return None
        r = str(r)
        if r.startswith('='):
            return r[1:]
        if r.startswith('~'):
            return '~' + self.ref(r[1:])     # an inverted master: "~name"
        if r in ('*locus', '*player'):
            return r
        if r.startswith('*') and len(r) > 1:
            return '*' + self.ref(r[1:])     # an alias reference: "*name" (branch feat/spirit-aliases)
        if r.startswith(('#', '@', '!')) or r in ('world', 'self', '-'):
            return r
        if r.startswith('player') and r[6:].isdigit():
            return self.player_ref(int(r[6:]))
        return self.prefix + r

    def player_ref(self, n):
        """player<n> of this test -> the suite's player slot (edict index)."""
        assert 1 <= n <= self.players, '%s: player%d but the test asked for %d players' % (self.key, n, self.players)
        return 'player%d' % n      # the suite's players are added first: slot n = edict n

    # -------------------------------------------------------------------- entities
    def _kv(self, classname, kv, names=()):
        out = {'classname': classname}
        name_keys = self.suite.name_keys(classname) | set(names)
        for k, v in kv.items():
            if v is None:
                continue
            k = k.rstrip('_')       # class_='x' style for Python keywords
            v = str(v)
            if classname == 'multi_manager' and k not in ('targetname', 'wait', 'spawnflags', 'origin', 'master'):
                # multi_manager: the keys are the names it fires (a "#n" suffix makes duplicates)
                out[self.ref(k)] = v
                continue
            if classname in ('info_alias', 'multi_alias') and k not in ('targetname', 'target', 'netname', 'mode',
                                                                     'm_iMode', 'spawnflags', 'origin'):
                # info_alias (list mode) / multi_alias: the other keys are names, as multi_manager's
                out[self.ref(k)] = v
                continue
            if classname == 'info_group' and k not in ('targetname', 'target', 'spawnflags', 'origin'):
                # info_group: the keys are member names, the values (and the default prefix) entity names
                out[k] = self.ref(v)
                continue
            if classname == 'multi_watcher' and k not in ('targetname', 'target', 'spawnflags', 'origin', 'm_fLogic',
                                                          'm_iszWatch'):
                # multi_watcher: the other keys are the names it watches, as multi_manager's
                out[self.ref(k)] = v
                continue
            if (k in name_keys or NAME_KEY_PATTERN.match(k)) and v and not v.startswith('!') and not re.match(r'^-?[\d.]+( -?[\d.]+)*$', v):
                v = self.ref(v)
            out[k] = v
        return out

    def _abs(self, at):
        x0, y0, z0, x1, y1, z1 = self.room
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        return (cx + at[0], cy + at[1], z0 + at[2])

    def point(self, classname, at=(0, 0, 0), names=(), **kv):
        """A point entity at `at` (relative to the room's floor center)."""
        e = Ent(self, classname, self._kv(classname, kv, names), at, None)
        self.ents.append(e)
        return e

    def brush(self, classname, size=(32, 32, 32), at=(0, 0, 0), tex=mapkit.CRATE, origin=False, names=(), parts=(),
              **kv):
        """A brush entity: one box of `size` whose bottom center is at `at`. origin=True adds an ORIGIN brush at its
        center (rotating entities); origin=(x, y, z) puts it there instead (room coordinates, like `at`). parts: more
        boxes of the entity, each (size, at)."""
        e = Ent(self, classname, self._kv(classname, kv, names), at, (size, tex, origin, tuple(parts)))
        self.ents.append(e)
        return e

    def trigger(self, classname, size=(64, 64, 72), at=(0, 0, 0), names=(), **kv):
        return self.brush(classname, size=size, at=at, tex=mapkit.TRIGGER, names=names, **kv)

    def map_compat(self, op, classname, selector, key, value=None, crc=None):
        """An entry of maps/map_compat.txt for the suite's map (docs/MAP_COMPAT.md): op 'set' or 'drop'; selector a
        local name, '*' or '#N'. crc: None = the compiled map's (applies), or a fixed one (another map version)."""
        if selector.startswith('#@'):
            sel = '#@' + self.ref(selector[2:])     # '#@name': the entity-lump index of that entity (build.py)
        else:
            sel = selector if selector == '*' or selector.startswith('#') else self.ref(selector)
        words = [op, classname, sel, key] + ([value] if value is not None else [])
        self.suite.compat.append((crc, ' '.join('"%s"' % w if ' ' in str(w) else str(w) for w in words)))

    def sequence(self, name, body):
        """A sequence of the suite's sequence file (sequences/<map>.seq): '%<prefix><name> { body }'. Name entities
        in the body with t.name('x')."""
        self.seq.append('%%%s\n{\n%s\n}\n' % (self.name(name), body))
        return self.name(name)

    # -------------------------------------------------------------------- steps
    def _step(self, *words):
        out = []
        for w in words:
            w = str(w)
            out.append('"%s"' % w if (' ' in w or w == '') else w)
        self.steps.append(' '.join(out))

    def fire(self, target, activator=None, caller=None, use='toggle', value=None):
        self._step('test_fire', self.ref(target), self.ref(activator) or '-', self.ref(caller) or '-', use,
                   value if value is not None else 0)

    def wait(self, seconds):
        self.duration += seconds
        self._step('wait', seconds)

    def wait_frames(self, n):
        self.duration += n * 0.02
        self._step('wait_frames', n)

    def wait_for_fire(self, target, timeout):
        self.duration += timeout
        self._step('wait_for', 'fire', self.ref(target), timeout)

    def wait_for_round(self, state, timeout):
        """Waits until the co-op round state is `state` (3 in play, 5 mission end, ...); the test fails if it is not
        reached within `timeout` game seconds."""
        self.duration += timeout
        self._step('wait_for', 'round', state, timeout)
        self.expectations.append(dict(kind='waited', cond='round %d' % state))

    def kv(self, ent, key, value):
        self._step('test_kv', self.ref(ent), key, value)

    def damage(self, ent, amount, dmgtype=0, attacker=None, hitgroup=None):
        """TakeDamage on `ent`; with `hitgroup` (HITGROUP_*: 1 head, 2 chest, 3 stomach, 4/5 arms, 6/7 legs) a hit on
        that part: TraceAttack, then ApplyMultiDamage. The attacker is the world unless named."""
        if hitgroup is not None and not attacker:
            attacker = 'world'
        extra = ([self.ref(attacker)] if attacker else []) + ([hitgroup] if hitgroup is not None else [])
        self._step('test_damage', self.ref(ent), amount, dmgtype, *extra)

    def teleport(self, who, where=(0, 0, 0), yaw=None):
        """Moves a player (or entity) to `where`: a position relative to the room's floor center (for a player: where
        it stands), or an entity."""
        if isinstance(where, (tuple, list)):
            p = self._abs(where)
            if str(who).startswith('player'):
                p = (p[0], p[1], p[2] + 36)     # a player's origin is 36 above its feet: `where` is where it stands
            args = ['%g' % p[0], '%g' % p[1], '%g' % p[2]]
        else:
            args = [self.ref(where)]
        self._step('test_teleport', self.ref(who), *(args + ([yaw] if yaw is not None else [])))

    def move(self, player, forward, side=0, frames=10, *buttons):
        """Input held for `frames` frames: for a button press (+use). Frame lengths vary with the clock speed and
        the machine, so a walk over a distance is walk()."""
        self._step('test_move', self.ref(player), forward, side, frames, *buttons)

    def walk(self, player, forward, side=0, seconds=1.0, *buttons):
        """Input held for `seconds` of game time, then released (test_move -1 holds until changed)."""
        self._step('test_move', self.ref(player), forward, side, -1, *buttons)
        self.wait(seconds)
        self._step('test_move', self.ref(player), 0, 0, 0)

    def flags(self, ent, change):
        """pev->flags: '+FL_NOTARGET', '-FL_GODMODE', '=0' (test_flags)."""
        self._step('test_flags', self.ref(ent), change)

    def look(self, player, pitch, yaw):
        self._step('test_look', self.ref(player), pitch, yaw)

    def cmd(self, player, *command):
        self._step('test_cmd', self.ref(player), *command)

    def snap(self, label, ent, *fields):
        self._step('test_snap', '%s:%s' % (self.id, label), self.ref(ent), *fields)

    def hitscan(self, label, ent, frm, to):
        """A bullet's trace (meets hitboxes) from `frm` to `to`, both relative to `ent`'s origin (test_hitscan): a
        snapshot `label` of `ent` with `hit` (1: this entity was hit), `hitgroup`, `fraction`, `endpos`."""
        self._step('test_hitscan', '%s:%s' % (self.id, label), self.ref(ent),
                   *(['%g' % c for c in frm] + ['%g' % c for c in to]))

    def mark(self, label):
        self._step('test_mark', '%s:%s' % (self.id, label))

    def fullpack(self, *players):
        """From the next frame on, every frame: the update a real client would get (SetupVisibility, then
        AddToFullPack for every entity, these players as the host), which the engine never builds for fake clients.
        No players: off. Off at every test's start."""
        self._step('test_fullpack', *([self.ref(p) for p in players] or ['off']))

    def exact_damage(self):
        """No random perfect / critical / failed hits (test_shotluck 0: SBO's 1-in-100 damage multipliers of
        CBaseMonster::TakeDamage are off). Every test starts so (its begin mark); this says so where it matters."""
        self._step('test_shotluck', '0')

    def random_hits(self):
        """From here to the end of the test, SBO's random perfect / critical / failed hits are on, as in the game
        (test_shotluck 1)."""
        self._step('test_shotluck', '1')

    def console(self, line):
        """An engine / game console command (a cvar for the rest of the suite, ...)."""
        self.steps.append(line)


class Suite:
    def __init__(self, name, title, players=1, requires=()):
        """requires: names that must be in the hl.so (a branch's functions): without them the suite is skipped, so
        a branch's suite can live here before the branch is merged."""
        self.name, self.title = name, title
        self.requires = tuple(requires)
        self.compat = []        # (crc or None, rule) for maps/map_compat.txt (Test.map_compat)
        self.files = {}         # game-relative path -> bytes: test-only files in the server's game folder (add_file)
        self.server_args = ()   # extra command line for this suite's server, before +map (e.g. ('+developer', '1'))
        self.map_prefix = 'co_' # the generated map's prefix, which picks the game mode (gamerules.cpp): 'co_' co-op, 'dm_' deathmatch
        self.world_keys = {}    # worldspawn keys of the generated map (e.g. {'chaptertitle': 'X'}): for the whole suite
        self.tests = []
        self._fgd = None
        self.players = players

    def test(self, title, doc=None, players=0, size=(512, 512, 256), tags=(), xfail=None, covers=()):
        """xfail: the reason a test is expected to fail (a known bug, reported, not fixed yet). covers: the FGD items
        its expectations check, for --coverage ('class', 'class.key', 'class.key=value', 'class:bit')."""
        def deco(func):
            self.tests.append(Test(self, func, title, doc, players, size, tags, xfail, covers))
            return func
        return deco

    def add_file(self, path, data):
        """A test-only file (e.g. 'models/et_x.mdl') in the run's game folder; the repo's gamedir is not written."""
        self.files[path] = data

    def set_fgd(self, classes, effective_props):
        self._fgd = (classes, effective_props)

    def name_keys(self, classname):
        """Keys whose value names an entity: the FGD's target_source / target_destination ones, the usual, and keys
        the FGD types as plain strings whose names say they name an entity (NAME_KEY_PATTERN)."""
        keys = set(GLOBAL_NAME_KEYS)
        if self._fgd and classname in self._fgd[0]:
            for prop in self._fgd[1](self._fgd[0], classname).values():
                if NAME_KEY_PATTERN.match(prop['name']):
                    keys.add(prop['name'])
        if self._fgd and classname in self._fgd[0]:
            for prop in self._fgd[1](self._fgd[0], classname).values():
                if prop['type'] in ('target_source', 'target_destination'):
                    keys.add(prop['name'])
        return keys
