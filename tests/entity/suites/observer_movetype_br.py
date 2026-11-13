"""Observers are never pushed, Battle Royale (the co-op side and the cause: suite observer_movetype). A player who
joins while a round runs presses a key in the go menu (CBr::ClientCommand "menuselect") and respawns as an observer
until the next round (CBr::PlayerSpawn, spawn mode 1): MOVETYPE_NONE, as StartObserver sets it. The players are added
by the test, edicts 1-3 (=player1..3; no suite players: test_round and test_join are co-op only); each answers the go
menu itself."""

from etest import Suite

SUITE = Suite('observer_movetype_br', 'Battle Royale: a late joiner\'s observer is MOVETYPE_NONE, never pushed',
              players=0)
S = SUITE
S.map_prefix = 'br_'
DOC = 'SPECTATOR.md'
OBS_ROAMING = 3
MOVETYPE_NONE = 0


@S.test('a player joins a running round: the observer it respawns as is MOVETYPE_NONE; roaming inside a turning '
        'func_rotating it stays put', doc=DOC, size=(512, 512, 256))
def late_joiner_observer(t):
    t.brush('func_rotating', size=(96, 96, 96), at=(150, 150, 0), origin=True, targetname='coin', speed='45',
            spawnflags='1')
    for name in ('br1', 'br2'):
        t.console('test_player add %s' % name)
        t.wait_frames(2)
    t.console('test_cmd player1 menuselect 1')     # the go menu: ready (two ready players start a round)
    t.console('test_cmd player2 menuselect 1')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(1.0)
    t.snap('round', '=player1', 'movetype', 'deadflag')
    t.console('test_player add br3')
    t.wait_frames(2)
    t.console('test_cmd player3 menuselect 1')     # joins the running round: an observer until the next one
    t.wait(1.0)
    t.snap('obs', '=player3', 'movetype', 'iuser1')
    t.cmd('=player3', 'specmode', str(OBS_ROAMING))
    t.wait(0.5)
    t.teleport('=player3', (182, 150, 30))
    t.wait(0.2)
    t.snap('in1', '=player3', 'origin', 'movetype', 'iuser1')
    t.wait(1.0)
    t.snap('in2', '=player3', 'origin', 'movetype')
    t.expect.value('round', '=player1', 'deadflag', 0)
    t.expect.value('obs', '=player3', 'iuser1', 0, op='>')
    t.expect.value('obs', '=player3', 'movetype', MOVETYPE_NONE)
    t.expect.value('in1', '=player3', 'iuser1', OBS_ROAMING)
    t.expect.value('in2', '=player3', 'movetype', MOVETYPE_NONE)
    t.expect.moved('in1', 'in2', '=player3', delta=(0, 0, 0), tol=1.0)
