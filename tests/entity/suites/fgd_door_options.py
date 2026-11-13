"""Tier 5, final pass: the Featureful door options on func_door and func_water (CBaseDoor, doors.cpp) with every choice:
ignore_corpses 0 / 1, instant_gib_corpses 0 / 1, blocker_recheck 0 / 1 / 2 (each against the stock behavior)."""

from etest import Suite

SUITE = Suite('fgd_door_options', 'Tier 5: door corpse and blocker options, every choice', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
CLASSES = ('func_door',)   # func_water: its Contents make it non-solid (Spawn: skin != 0 -> SOLID_NOT): nothing blocks it


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def door(t, cls, name, y, **kv):
    """a door moving +x down a lane at y; a func_water gets Contents water (as in maps)"""
    if cls == 'func_water':
        kv.setdefault('skin', '-3')
    info(t, name + '_opened')
    info(t, name + '_closing')
    t.brush(cls, targetname=name, size=(16, 48, 72), at=(-120, y, 0), angles='0 0 0', speed='200',
            fireonopened=name + '_opened', fireonopened_triggerstate='1', fireonclosing=name + '_closing',
            fireonclosing_triggerstate='1', names=('fireonopened', 'fireonclosing'), **kv)


def corpse_case(cls):
    @S.test('%s ignore_corpses 0 pushes a corpse along, 1 moves through it' % cls, players=1, doc=DOC,
            covers=('%s.ignore_corpses=0' % cls, '%s.ignore_corpses=1' % cls))
    def case(t):
        for v, y in (('0', 60), ('1', -60)):
            door(t, cls, 'd' + v, y, wait='-1', lip='-144', ignore_corpses=v)
            t.point('monster_hgrunt_dead', targetname='c' + v, at=(-60, y, 0), angles='0 0 0')
        t.flags('player1', '+FL_NOTARGET')
        t.wait(0.5)
        t.snap('a', 'c0', 'origin')
        t.snap('a', 'c1', 'origin')
        t.fire('d0')
        t.fire('d1')
        t.wait(1.5)
        t.snap('b', 'c0', 'origin')
        t.snap('b', 'c1', 'origin')
        t.expect.moved('a', 'b', 'c0', min_dist=20)
        t.expect.moved('a', 'b', 'c1', delta=(0, 0, 0), tol=1.0)
    case.__name__ = cls.replace('func_', '') + '_ignore_corpses'
    return case


def gib_case(cls):
    @S.test('%s instant_gib_corpses 1 gibs a corpse pinned against a wall and goes on; 0 is blocked by it' % cls,
            players=1, doc=DOC, covers=('%s.instant_gib_corpses=0' % cls, '%s.instant_gib_corpses=1' % cls))
    def case(t):
        t.brush('func_wall', size=(16, 300, 128), at=(0, 0, 0))
        for v, y in (('0', 60), ('1', -60)):
            door(t, cls, 'g' + v, y, wait='-1', lip='-84', instant_gib_corpses=v)
            t.point('monster_hgrunt_dead', targetname='c' + v, at=(-30, y, 0), angles='0 0 0')
        t.flags('player1', '+FL_NOTARGET')
        t.wait(0.5)
        t.fire('g0')
        t.fire('g1')
        t.wait(2.0)
        t.expect.removed('c1')
        t.expect.used('g1_opened', count=1)
        t.expect.not_removed('c0')
        t.expect.not_used('g0_opened')
    case.__name__ = cls.replace('func_', '') + '_instant_gib'
    return case


def recheck_case(cls):
    @S.test('%s blocker_recheck 1: a monster its damage kills does not turn it back; 0 and 2 go back' % cls,
            players=1, doc=DOC,
            covers=tuple('%s.blocker_recheck=%d' % (cls, v) for v in (0, 1, 2)))
    def case(t):
        t.brush('func_wall', size=(16, 400, 128), at=(0, 0, 0))
        for v, y in (('0', 130), ('1', 0), ('2', -130)):
            door(t, cls, 'r' + v, y, wait='5', lip='-84', dmg='500', blocker_recheck=v)
            t.point('monster_cyborg', targetname='m' + v, at=(-30, y, 0), angles='0 180 0')
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (150, 0, 0), yaw=180)
        t.wait(0.5)
        for v in '012':
            t.kv('m' + v, 'health', '10')
        for v in '012':
            t.fire('r' + v)
        t.wait(2.0)
        t.expect.used('r1_opened', count=1)
        t.expect.not_used('r1_closing')
        for v in '02':
            t.expect.used('r%s_closing' % v, min=1)
    case.__name__ = cls.replace('func_', '') + '_blocker_recheck'
    return case


for _c in CLASSES:
    corpse_case(_c)
    gib_case(_c)
    recheck_case(_c)
