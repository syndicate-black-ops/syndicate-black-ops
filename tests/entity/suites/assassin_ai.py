"""monster_human_assassin (hassassin.cpp, Half-Life Unified SDK fixes): an Assassin that lands out of combat walks
again (MOVETYPE_STEP), and each Assassin times its own footsteps."""

from etest import Suite

SUITE = Suite('assassin_ai', 'monster_human_assassin: landing out of combat, footsteps per Assassin', players=1)
S = SUITE
CLS = 'monster_human_assassin'
MOVETYPE_STEP, MOVETYPE_TOSS = 4, 6


def precache(t):
    """A copy so the map precaches the class, removed before the test's Assassins are made ('@class' is theirs)."""
    t.point(CLS, targetname='pre', at=(-200, 200, 0), angles='0 270 0')
    t.point('trigger_relay', targetname='rm', killtarget='pre')
    t.fire('rm')
    t.wait(0.2)


def create(t, at, *keys):
    """An unnamed Assassin (a named one waits for a trigger); Prisoner (16): it sees nobody and nobody attacks it."""
    p = t._abs(at)
    t.console(' '.join(['test_create', CLS, '%g' % p[0], '%g' % p[1], '%g' % p[2], '90', 'spawnflags', '16']
                       + list(keys)))
    t.wait(0.3)


@S.test('an Assassin that comes down out of combat (MOVETYPE_TOSS, no enemy) is set back to MOVETYPE_STEP when it lands',
        size=(512, 512, 384), players=1, covers=(CLS,))
def lands_out_of_combat(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (-200, -200, 0))
    precache(t)
    create(t, (0, 0, 0))
    t.kv('@%s' % CLS, 'targetname', t.name('a'))
    t.kv('a', 'movetype', str(MOVETYPE_TOSS))   # as a jump leaves it
    t.teleport('a', (0, 0, 120))
    t.wait(1.0)
    t.snap('air', 'a', 'movetype', 'flags')
    t.wait(8.0)                                 # the idle schedule ends (5 s wait) and the next one is chosen
    t.snap('after', 'a', 'movetype')
    t.expect.value('after', 'a', 'movetype', MOVETYPE_STEP)
    t.point('trigger_relay', targetname='rm_a', killtarget='a')
    t.fire('rm_a')                              # gone before the next test counts its Assassins by class
    t.wait(0.2)


@S.test('two Assassins walking their paths at the same time both make footsteps (each its own timing)',
        size=(512, 512, 256), players=1, covers=(CLS, CLS + '.target'))
def footsteps_each(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, 0, 0))
    precache(t)
    for k, y in ((1, -160), (2, 160)):
        t.point('path_corner', targetname='p%da' % k, target='p%db' % k, at=(-180, y, 0))
        t.point('path_corner', targetname='p%db' % k, target='p%da' % k, at=(180, y, 0))
    for k, y in ((1, -160), (2, 160)):
        create(t, (-120, y, 0), 'target', t.name('p%da' % k), 'netname', t.name('squad%d' % k))
    t.kv('@%s[0]' % CLS, 'targetname', t.name('a1'))
    t.kv('@%s[1]' % CLS, 'targetname', t.name('a2'))
    t.wait(1.0)
    t.mark('walk')
    t.wait(6.0)
    t.snap('end', 'a1', 'origin')
    t.snap('end', 'a2', 'origin')
    t.expect.sound('pl_step', ent='a1', min=6, after='walk')
    t.expect.sound('pl_step', ent='a2', min=6, after='walk')
