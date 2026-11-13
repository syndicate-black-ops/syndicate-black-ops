"""Tier 5, final pass: "Squad Name" (netname) on the Demon Slave hunter and the werewolves (hunter2.cpp, werewolf.cpp,
werewolf_hunter.cpp CallForHelp, as Half-Life's alien slave): when one of them takes an enemy it calls for help, and
every monster with the same squad name takes that enemy too (PushEnemy) and is provoked. A sees a target; B, a
Prisoner (16: it sees nobody itself) with A's name, takes it from A's call; C, a Prisoner with another name, does not.
Each test makes its monsters (test_create) once the player is notarget."""

from etest import Suite

SUITE = Suite('fgd_squads', 'Tier 5: Squad Name on the Demon Slave hunter and the werewolves', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
CLASSES = ('monster_hunter', 'monster_werewolf', 'monster_werewolf_hunter')


def squad_case(cls):
    @S.test('%s "Squad Name": one that takes an enemy calls the others of its name, which take it too; one of '
            'another name does not' % cls, players=1, doc=DOC, covers=('%s.netname' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        t.point(cls, targetname='pre', at=(200, -200, 0), angles='0 90 0', spawnflags='16')
        t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
        t.fire('rm_pre')
        t.point('monster_target', targetname='foe', at=(-150, 60, 36), frags='2', spawnflags='1')    # off: none yet
        for name, x, y, sq, fl in (('a', -150, 200, 'pair', 0), ('b', 150, 200, 'pair', 16),
                                   ('c', 150, -150, 'solo', 16)):
            p = t._abs((x, y, 0))
            t.console('test_create %s %g %g %g 270 targetname %s netname %s spawnflags %d' % (
                cls, p[0], p[1], p[2], t.name(name), t.name(sq), fl))
        t.wait(7.0)                     # a named monster stands its first 5 s (SCHED_WAIT_TRIGGER)
        t.fire('foe', use='on')         # it counts as a player: A takes it
        t.wait(3.0)
        for n in ('a', 'b', 'c'):
            t.snap('s', n, 'enemy_name')
        for n in ('a', 'b', 'c', 'foe'):
            t.point('trigger_relay', targetname='rm_' + n, killtarget=n)
            t.fire('rm_' + n)
        t.flags('player1', '-FL_NOTARGET')
        t.expect.value('s', 'a', 'enemy_name', t.name('foe'))
        t.expect.value('s', 'b', 'enemy_name', t.name('foe'))
        t.expect.value('s', 'c', 'enemy_name', '')
    case.__name__ = 'squad_' + cls.replace('monster_', '')
    return case


for _c in CLASSES:
    squad_case(_c)
