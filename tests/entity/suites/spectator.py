"""Spectators (docs/SPECTATOR.md): an observer with a target is moved to the target every frame (ReGameDLL_CS
SetupVisibility), so it hears what the target hears after a teleport or respawn far away (playtest 2026-10-17). Its
own suite: player2 stays in spectator mode."""

from etest import Suite

SUITE = Suite('spectator', 'Spectators: the observer follows its target', players=2)
S = SUITE
DOC = 'SPECTATOR.md'


@S.test('player2 spectates player1; player1 teleports 400 units on each axis: player2 is at player1', players=2,
        doc=DOC, size=(512, 512, 300))
def follows_teleport(t):
    t.teleport('player1', (-200, -200, 0), yaw=0)
    t.teleport('player2', (-200, -100, 0), yaw=0)
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.snap('before', 'player2', 'iuser1', 'iuser2')
    t.teleport('player1', (200, 200, 0), yaw=0)
    t.wait(1.0)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'player2', 'origin', 'iuser2')
    t.expect.value('before', 'player2', 'iuser1', 0, op='>')
    t.expect.value('after', 'player2', 'iuser2', 1)
    t.expect.distance('after', 'player1', 'player2', 0, 1, flat=False)
