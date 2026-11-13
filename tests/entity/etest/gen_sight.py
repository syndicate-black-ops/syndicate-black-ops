"""Generated sight tests (suites fgd_gen_sight_*): the Monster keys "prisonerto" and "ignoredby" with every FGD choice,
on a class, checked by whom a monster takes as its enemy (snapshot field enemy_name).

One test per (class, key) runs every choice in turn in its room: the monster under test is made for that choice
(test_create, keys set before it spawns), looked at for a while, snapshot, removed. Its class is placed once per test,
a Prisoner (16) that sees nobody and is seen by nobody, so the map precaches it; that copy is removed first.

The targets are monster_targets (not solid, never hurt; they count as the class in their "frags"), made for each
choice and removed with it, so a monster that sees across rooms (the hunters look 12000 units around, with no line of
sight) sees only the targets of the choice it is made for, and the map holds few monsters: Look takes the first 100
monsters in its box (UTIL_EntitiesInBox), and placed targets of every room would push this room's out of the list.

prisonerto V: the monster (classified as an insect, 10, which hates every class but Human military) has a target of
class V near it and a second target it hates farther away. It takes the far one: it does not see class V (V 0: no
key, it takes the near one). For V 4 (an insect fears Human military) it is classified as a player (2).

ignoredby V: a watcher (a Fed classified as V, made for the choice) has the monster (classified as an insect: every
class hates insects) near it and an insect target farther away. It takes the far target: class V does not see the
monster (V 0: no key and no far target, the Fed's own class, Human military: the monster is classified as Alien
military and taken).

Relationships: CBaseMonster::IDefaultRelationship (monsters.cpp); prisonerto / ignoredby: CBaseMonster::LookSkips.
"""

VALUES = tuple(range(14))
INSECT, PLAYER, HMIL, AMIL = 10, 2, 4, 5
NEAR, FAR, LOOKER_AT = (0, 60, 36), (0, 200, 36), (0, -120, 0)
WATCHER_AT = (0, -200, 0)
SF_PRISONER = 16
TURRETS = ('monster_turret', 'monster_miniturret', 'monster_sentry')
WAIT = 9.0       # a named monster stands its first 5 s or more (SCHED_WAIT_TRIGGER) before it looks around


def _extra(cls):
    # turrets: Autostart (32) so they look by themselves
    return {'spawnflags': '32'} if cls in TURRETS else {'spawnflags': '0'}


def _precache_copy(t, cls):
    # a Prisoner; an Apache also Waits For Trigger (64), else it flies off
    t.point(cls, targetname='pre', at=(200, -200, 0 if cls != 'monster_apache' else 120), angles='0 90 0',
            spawnflags=str(SF_PRISONER | (64 if cls == 'monster_apache' else 0)))
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.fire('rm_pre')
    t.wait(0.2)


def _create(t, cls, name, at, yaw, keys):
    p = t._abs(at)
    words = ['test_create', cls, '%g' % p[0], '%g' % p[1], '%g' % p[2], '%d' % yaw, 'targetname', t.name(name)]
    for k, v in keys.items():
        words += [k, str(v)]
    t.console(' '.join(words))


def _target(t, name, cls, at):
    """a monster_target `name` of class `cls` made now (test_create), with its remover rm_<name>"""
    _remover(t, name)
    _create(t, 'monster_target', name, at, 0, {'frags': cls})


def _gone(t, *names):
    for n in names:
        t.fire('rm_' + n)


def _remover(t, name):
    t.point('trigger_relay', targetname='rm_' + name, killtarget=name)


def prisonerto_case(S, cls, doc=None):
    covers = ('%s.prisonerto' % cls,) + tuple('%s.prisonerto=%d' % (cls, v) for v in VALUES)

    @S.test('%s "prisonerto" 0-13: made for each choice, it does not take a near target of that class as its '
            'enemy and goes for a far one it hates instead; 0 (none) takes the near one' % cls, players=1,
            doc=doc, covers=covers)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        _precache_copy(t, cls)
        t.point('monster_target', targetname='mtpre', at=(-200, 200, 0), spawnflags='1')     # the class in the map
        for v in VALUES:
            me = PLAYER if v == HMIL else INSECT
            far = AMIL if v in (3, HMIL) else 3
            name = 'm%d' % v
            _remover(t, name)
            _target(t, 'near%d' % v, v, NEAR)
            _target(t, 'far%d' % far, far, FAR)
            keys = dict(_extra(cls), classify=me, netname=t.name('sq%d' % v))
            if v:
                keys['prisonerto'] = v
            _create(t, cls, name, LOOKER_AT, 90, keys)
            t.wait(WAIT)
            t.snap('v%d' % v, name, 'enemy_name', 'state', 'schedule')
            _gone(t, name, 'near%d' % v, 'far%d' % far)
            t.wait(0.3)
            t.expect.value('v%d' % v, name, 'enemy_name', t.name('far%d' % far) if v else t.name('near%d' % v))
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'prisonerto_' + cls.replace('monster_', '')
    return case


def ignoredby_case(S, cls, doc=None):
    covers = ('%s.ignoredby' % cls,) + tuple('%s.ignoredby=%d' % (cls, v) for v in VALUES)

    @S.test('%s "ignoredby" 0-13: a Fed classified as each choice does not take it as its enemy (near) and goes for '
            'a far insect target instead; 0 (none): the Fed takes it' % cls, players=1, doc=doc, covers=covers)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        _precache_copy(t, cls)
        t.point('monster_human_grunt', targetname='fedpre', at=(-200, 200, 0), spawnflags=str(SF_PRISONER))
        t.point('trigger_relay', targetname='rm_fedpre', killtarget='fedpre')
        t.fire('rm_fedpre')
        t.point('monster_target', targetname='mtpre', at=(-200, 200, 0), spawnflags='1')     # the class in the map
        for v in VALUES:
            name, watcher = 'm%d' % v, 'w%d' % v
            _remover(t, name)
            _remover(t, watcher)
            if v:
                _target(t, 'far%d' % v, INSECT, FAR)    # (0: none, the monster under test may walk off from the Fed)
            keys = dict(_extra(cls), classify=INSECT if v else AMIL, netname=t.name('sq%d' % v))
            if v:
                keys['ignoredby'] = v
            # the monster under test where the near target would be, facing away (it may fight the Fed: tough)
            _create(t, cls, name, (NEAR[0], NEAR[1], 0), 90, keys)
            wkeys = {'netname': t.name('wsq%d' % v)}
            if v:
                wkeys['classify'] = v
            _create(t, 'monster_human_grunt', watcher, WATCHER_AT, 90, wkeys)
            t.wait(0.1)
            t.kv(name, 'health', '100000')
            t.kv(watcher, 'health', '100000')
            t.wait(WAIT)
            t.snap('v%d' % v, watcher, 'enemy_name', 'state', 'schedule')
            _gone(t, name, watcher, *(('far%d' % v,) if v else ()))
            t.wait(0.3)
            t.expect.value('v%d' % v, watcher, 'enemy_name', t.name('far%d' % v) if v else t.name(name))
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'ignoredby_' + cls.replace('monster_', '')
    return case


# ---------------------------------------------------------------------------------------------- classify, ally, view
# CBaseMonster::IDefaultRelationship (monsters.cpp): R_AL -2, R_FR -1, R_NO 0, R_DL 1, R_HT 2, R_NM 3
REL = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, -2, 1, 1, -2, 1, 1, 1, 1, 1, 2, 1, 1, 1],
    [0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1],
    [0, -1, -1, -2, 0, -1, 0, 2, 1, -1, 2, -2, 0, 0],
    [0, -2, 2, 0, -2, 2, 1, 1, 1, 1, 2, 2, 0, 0],
    [0, 1, 2, 1, 2, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, 1, 3, 1, 1, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 1, 1, 1, 1, 0, 0, 0, 0, -1, 2, 1, 0, 0],
    [0, 1, 3, 2, 2, 0, 0, 0, 2, 1, 2, 1, 0, 0],
    [2, 2, 2, 2, -1, 2, 2, 2, 2, 2, 2, 2, 2, 2],
    [0, 1, -2, -2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 1],
    [0, 0, 1, 1, 1, -2, 0, 1, 1, 0, 2, 1, 1, 0],
]
CLASSIFY = (-1, 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)
# the class each tested monster has of its own (its Classify)
OWN = {'monster_human_grunt': HMIL, 'monster_turret': HMIL, 'monster_miniturret': HMIL, 'monster_sentry': HMIL,
       'monster_apache': HMIL, 'monster_snark': 13,      # a Pitbull without an enemy: Alien bioweapon
       'monster_osprey': 1}                               # Machine


def _pick(row):
    """(a class this row does not fight, one it fights: dislike or worse), the near one the first"""
    calm = [c for c in range(1, 14) if row[c] <= 0]
    hated = [c for c in range(1, 14) if row[c] >= 1]
    return calm[0], hated[-1]


def _sight_loop(t, cls, cases, doc_label):
    """cases: (label, looker keys, near class, far class, expected 'near' / 'far')"""
    for i, (label, keys, near, far, want) in enumerate(cases):
        name = 'm%d' % i
        _remover(t, name)
        _target(t, 'near%d' % i, near, NEAR)
        _target(t, 'far%d' % i, far, FAR)
        _create(t, cls, name, LOOKER_AT, 90, dict(_extra(cls), netname=t.name('sq%d' % i), **keys))
        t.wait(WAIT)
        t.snap(label, name, 'enemy_name', 'state', 'schedule')
        _gone(t, name, 'near%d' % i, 'far%d' % i)
        t.wait(0.3)
        t.expect.value(label, name, 'enemy_name', t.name('%s%d' % (want, i)))


def classify_case(S, cls, doc=None):
    covers = ('%s.classify' % cls,) + tuple('%s.classify=%d' % (cls, v) for v in CLASSIFY)

    @S.test('%s "classify" each choice: made with it, it leaves a near target of a class that choice does not fight '
            'and takes a far one it fights (the class table); 0: its own class' % cls, players=1, doc=doc,
            covers=covers)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        _precache_copy(t, cls)
        t.point('monster_target', targetname='mtpre', at=(-200, 200, 0), spawnflags='1')
        cases = []
        for v in CLASSIFY:
            row = REL[0 if v == -1 else (OWN[cls] if v == 0 else v)]
            near, far = _pick(row)
            cases.append(('c%d' % (v + 1), {'classify': v}, near, far, 'far'))
        _sight_loop(t, cls, cases, 'classify')
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'classify_' + cls.replace('monster_', '')
    return case


def ally_case(S, cls, doc=None):
    covers = ('%s.is_player_ally' % cls, '%s.is_player_ally=0' % cls, '%s.is_player_ally=1' % cls)

    @S.test('%s "is_player_ally" 0: it takes a near target that counts as a player; 1: it leaves it (a player ally) '
            'and takes a far Alien military one' % cls, players=1, doc=doc, covers=covers)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        _precache_copy(t, cls)
        t.point('monster_target', targetname='mtpre', at=(-200, 200, 0), spawnflags='1')
        _sight_loop(t, cls, [('a0', {'is_player_ally': 0}, PLAYER, AMIL, 'near'),
                             ('a1', {'is_player_ally': 1}, PLAYER, AMIL, 'far')], 'ally')
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'ally_' + cls.replace('monster_', '')
    return case


FOV = (0, -1, -0.7, -0.2, 0.2, 0.5, 0.7, 0.9)


def fov_case(S, cls, doc=None):
    """It faces north; the near target stands 120 away at an angle whose cosine is just under the field of view (out
    of it), the far one 220 away just inside it. -1 (all round): the near one, behind it, is seen and taken; 0 (its
    own): the near one straight ahead is."""
    import math
    covers = ('%s.field_of_view' % cls,) + tuple('%s.field_of_view=%g' % (cls, f) for f in FOV)

    @S.test('%s "field_of_view" each choice: a near target just outside that cone is not taken, a far one just '
            'inside it is; -1 (all round): the near one behind it is; 0 (its own): one ahead is' % cls, players=1, doc=doc, covers=covers)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        _precache_copy(t, cls)
        t.point('monster_target', targetname='mtpre', at=(-200, 200, 0), spawnflags='1')
        ox, oy = 0, -40
        for i, f in enumerate(FOV):
            name = 'm%d' % i
            if f == -1:
                near_c, far_c, want = -0.95, -0.9, 'near'
            elif f == 0:                # Empty or 0: its own; a near target straight ahead
                near_c, far_c, want = 0.99, 0.98, 'near'
            else:
                near_c, far_c, want = max(f - 0.15, -0.99), min(f + 0.1, 0.99), 'far'
            pos = []
            for c, r in ((near_c, 120), (far_c, 220)):
                a = math.acos(c)        # from north, to the east
                pos.append((ox + r * math.sin(a), oy + r * math.cos(a), 36))
            _remover(t, name)
            _target(t, 'near%d' % i, 3, pos[0])
            _target(t, 'far%d' % i, 3, pos[1])
            _create(t, cls, name, (ox, oy, 0), 90, dict(_extra(cls), classify=INSECT, field_of_view=f,
                                                       netname=t.name('sq%d' % i)))
            # snapshots from 4 s on: a monster that takes the far target turns or walks to it, and then sees the
            # near one too; that it took the far one while the near one (nearer, as hated) was there shows the near
            # one was out of its view
            t.wait(4.0)
            labels = []
            for k in range(11):
                t.snap('f%d_%d' % (i, k), name, 'enemy_name')
                labels.append('f%d_%d' % (i, k))
                t.wait(0.5)
            _gone(t, name, 'near%d' % i, 'far%d' % i)
            t.wait(0.3)
            t.expect.value_any(labels, name, 'enemy_name', t.name('%s%d' % (want, i)))
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'fov_' + cls.replace('monster_', '')
    return case


def watcher_case(S, cls, key, doc=None, extra=None, z=0, size=(512, 512, 256)):
    """How a Fed watcher sees `cls` made with each choice of `key`, for a class that does not look itself (the
    Osprey: COsprey::Update looks, but it takes no enemy). prisonerto / ignoredby V: a Fed classified as V leaves the
    monster (classified as an insect: every class hates insects) and takes a far insect target; 0: the Fed (its own
    class) takes the monster (classified as Alien military). classify V: a Fed (Human military) takes the monster when
    it fights class V (dislike or worse; it is nearer than the far target, which counts as Alien passive, disliked),
    else the far target."""
    values = CLASSIFY if key == 'classify' else VALUES
    covers = ('%s.%s' % (cls, key),) + tuple('%s.%s=%d' % (cls, key, v) for v in values)

    @S.test('%s "%s" each choice, as a Fed sees it: it takes the monster as its enemy or the far target, as the '
            'choice says' % (cls, key), players=1, doc=doc, covers=covers, size=size)
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        t.point(cls, targetname='pre', at=(200, -200, z or 0), angles='0 90 0',
                spawnflags=str(SF_PRISONER | int((extra or {}).get('spawnflags', '0'))))
        t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
        t.fire('rm_pre')
        t.point('monster_human_grunt', targetname='fedpre', at=(-200, 200, 0), spawnflags=str(SF_PRISONER))
        t.point('trigger_relay', targetname='rm_fedpre', killtarget='fedpre')
        t.fire('rm_fedpre')
        t.point('monster_target', targetname='mtpre', at=(-200, 0, 0), spawnflags='1')
        for i, v in enumerate(values):
            name, watcher = 'm%d' % i, 'w%d' % i
            _remover(t, name)
            _remover(t, watcher)
            keys = dict(extra or {}, netname=t.name('sq%d' % i))
            wkeys = {'netname': t.name('wsq%d' % i)}
            if key == 'classify':
                far_cls = 6
                keys['classify'] = v
                row = REL[HMIL]
                mine = 0 if v == -1 else (OWN.get(cls, HMIL) if v == 0 else v)
                want_monster = row[mine] >= 1
            else:
                far_cls = INSECT
                keys['classify'] = INSECT if v else AMIL
                if v:
                    keys[key] = v
                    wkeys['classify'] = v
                want_monster = v == 0
            _target(t, 'far%d' % i, far_cls, FAR)
            _create(t, cls, name, (NEAR[0], NEAR[1], z), 90, keys)
            _create(t, 'monster_human_grunt', watcher, WATCHER_AT, 90, wkeys)
            t.wait(0.1)
            t.kv(watcher, 'health', '100000')
            t.wait(WAIT)
            t.snap('v%d' % i, watcher, 'enemy_name', 'state', 'schedule')
            _gone(t, name, watcher, 'far%d' % i)
            t.wait(0.3)
            t.expect.value('v%d' % i, watcher, 'enemy_name', t.name(name) if want_monster else t.name('far%d' % i))
        t.flags('player1', '-FL_NOTARGET')
    case.__name__ = 'watch_%s_%s' % (key, cls.replace('monster_', ''))
    return case
