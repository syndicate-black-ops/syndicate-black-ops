"""Observers are never pushed, Bounty Killer (the co-op side and the cause: suite observer_movetype). A dead player in
the death camera (mp_forcerespawn 0: StartDeathCam after 6 s) and a player who says spectate are MOVETYPE_NONE; a
player who spawns walks. The map is dm_et_observer_movetype_dm (deathmatch game rules)."""

from etest import Suite

SUITE = Suite('observer_movetype_dm', 'Bounty Killer: observers are MOVETYPE_NONE, never pushed', players=2)
S = SUITE
S.map_prefix = 'dm_'
DOC = 'SPECTATOR.md'
OBS_ROAMING = 3
MOVETYPE_NONE = 0
MOVETYPE_WALK = 3


def rotator(t):
    t.brush('func_rotating', size=(96, 96, 96), at=(150, 150, 0), origin=True, targetname='coin', speed='45',
            spawnflags='1')


def roam_in_rotator(t, who):
    t.cmd(who, 'specmode', str(OBS_ROAMING))
    t.wait(0.5)
    t.teleport(who, (182, 150, 30))
    t.wait(0.2)
    t.snap('in1', who, 'origin', 'movetype', 'iuser1')
    t.wait(1.0)
    t.snap('in2', who, 'origin', 'movetype')
    t.expect.value('in1', who, 'iuser1', OBS_ROAMING)
    t.expect.value('in2', who, 'movetype', MOVETYPE_NONE)
    t.expect.moved('in1', 'in2', who, delta=(0, 0, 0), tol=1.0)


@S.test('dead player in the death camera: MOVETYPE_NONE; roaming inside a turning func_rotating it stays put',
        players=2, doc=DOC, size=(512, 512, 256))
def deathcam_observer(t):
    rotator(t)
    t.console('mp_forcerespawn 0')
    t.teleport('player1', (-150, -150, 0), yaw=0)
    t.teleport('player2', (-150, 150, 0), yaw=0)
    t.wait(0.5)
    t.damage('player1', 5000, 0)
    t.wait(8.0)     # the death animation, DEAD_RESPAWNABLE, the death camera 6 s later
    t.snap('obs', 'player1', 'movetype', 'iuser1', 'deadflag')
    roam_in_rotator(t, 'player1')
    t.console('mp_forcerespawn 1')
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap('alive', 'player1', 'movetype', 'iuser1')
    t.expect.value('obs', 'player1', 'iuser1', 0, op='>')
    t.expect.value('obs', 'player1', 'movetype', MOVETYPE_NONE)
    t.expect.value('alive', 'player1', 'movetype', MOVETYPE_WALK)
    t.expect.value('alive', 'player1', 'iuser1', 0)


@S.test('living player says spectate: MOVETYPE_NONE', players=2, doc=DOC)
def spectate_observer(t):
    t.cmd('player1', 'spectate')
    t.wait(1.0)
    t.snap('obs', 'player1', 'movetype', 'iuser1')
    t.cmd('player1', 'spectate')
    t.expect.value('obs', 'player1', 'iuser1', 0, op='>')
    t.expect.value('obs', 'player1', 'movetype', MOVETYPE_NONE)
