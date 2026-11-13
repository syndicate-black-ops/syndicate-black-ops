"""Observers are never pushed (docs/SPECTATOR.md): every observer keeps StartObserver's MOVETYPE_NONE, which the
engine's pushers skip (SV_PushMove / SV_PushRotate push every other movetype but NOCLIP, PUSH and FOLLOW, and turn a
pushed client's view with a rotating brush). A dead co-op player who confirms the loadout menu respawns as an
observer (CCoop::PlayerSpawn, spawn mode 1); the walking movetype is set only for a player who spawns alive. The
repro: a spectator's view turning with co_jollycompound's coin (func_rotating, 45 degrees a second). Bounty Killer
and Battle Royale: suites observer_movetype_dm and observer_movetype_br."""

from etest import Suite

SUITE = Suite('observer_movetype', 'Co-op: observers are MOVETYPE_NONE, never pushed by a rotating brush', players=2)
S = SUITE
DOC = 'SPECTATOR.md'
OBS_ROAMING = 3
MOVETYPE_NONE = 0
MOVETYPE_WALK = 3


def rotator(t):
    """A turning func_rotating (45 degrees a second about z, start on) away from the players' spots."""
    t.brush('func_rotating', size=(96, 96, 96), at=(150, 150, 0), origin=True, targetname='coin', speed='45',
            spawnflags='1')


def roam_in_rotator(t, who):
    """`who` (an observer) roams, is put inside the rotator 32 units off its axis; snapshots 1 s apart."""
    t.cmd(who, 'specmode', str(OBS_ROAMING))
    t.wait(0.5)
    t.teleport(who, (182, 150, 30))
    t.wait(0.2)
    t.snap('in1', who, 'origin', 'movetype', 'iuser1')
    t.wait(1.0)
    t.snap('in2', who, 'origin', 'movetype', 'iuser1')
    t.expect.value('in1', who, 'iuser1', OBS_ROAMING)
    t.expect.value('in2', who, 'movetype', MOVETYPE_NONE)
    t.expect.moved('in1', 'in2', who, delta=(0, 0, 0), tol=1.0)


@S.test('player who spawns alive walks (MOVETYPE_WALK)', players=2, doc=DOC)
def alive_walks(t):
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(1.0)
    t.snap('alive', 'player1', 'movetype', 'iuser1')
    t.expect.value('alive', 'player1', 'movetype', MOVETYPE_WALK)
    t.expect.value('alive', 'player1', 'iuser1', 0)


@S.test('dead player confirms the loadout menu: the observer it respawns as is MOVETYPE_NONE; roaming inside a '
        'turning func_rotating it stays put', players=2, doc=DOC, size=(512, 512, 256))
def dead_respawn_observer(t):
    rotator(t)
    t.teleport('player1', (-150, -150, 0), yaw=0)
    t.teleport('player2', (-150, 150, 0), yaw=0)
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn: m_iSpawnProtectTime)
    t.damage('player1', 5000, 0)
    t.wait(3.0)     # the death animation, then CCoop::FPlayerCanRespawn: an observer at a spawn point
    t.console('test_join player1')      # the menu's Go: CBasePlayer::Spawn with spawn mode 1 (StartObserver)
    t.wait(1.0)
    t.snap('obs', 'player1', 'movetype', 'iuser1', 'deadflag')
    roam_in_rotator(t, 'player1')
    t.expect.value('obs', 'player1', 'iuser1', 0, op='>')
    t.expect.value('obs', 'player1', 'movetype', MOVETYPE_NONE)


@S.test('living player says spectate: MOVETYPE_NONE; roaming inside a turning func_rotating it stays put',
        players=2, doc=DOC, size=(512, 512, 256))
def spectate_observer(t):
    rotator(t)
    t.console('test_round restart')     # player1 alive again (the test before left it an observer)
    t.console('wait_for alive player1 20')
    t.teleport('player1', (-150, -150, 0), yaw=0)
    t.teleport('player2', (-150, 150, 0), yaw=0)
    t.wait(0.5)
    t.cmd('player1', 'spectate')
    t.wait(1.0)
    t.snap('obs', 'player1', 'movetype', 'iuser1')
    roam_in_rotator(t, 'player1')
    t.expect.value('obs', 'player1', 'iuser1', 0, op='>')
    t.expect.value('obs', 'player1', 'movetype', MOVETYPE_NONE)
