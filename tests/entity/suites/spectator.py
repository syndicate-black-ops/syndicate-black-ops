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


OBS_CHASE_LOCKED = 1
OBS_MAP_FREE = 5


def federal_zoom_setup(t, mode):
    """player1 holds the Federal (zoom 30 on secondary attack); player2 spectates player1 in `mode`."""
    t.point('weapon_federal', at=(0, -150, 0))
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.teleport('player2', (-200, -100, 0), yaw=0)
    t.wait(0.5)
    t.cmd('player1', 'weapon_federal')
    t.wait(1.5)                 # deploy
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.cmd('player2', 'specmode', str(mode))
    t.wait(0.5)
    t.snap('mode', 'player2', 'iuser1', 'iuser2')
    t.mark('zoom')
    t.move('player1', 0, 0, 1, '+attack2')     # one frame: the zoom toggles once
    t.wait(0.5)
    t.mark('unzoom')
    t.move('player1', 0, 0, 1, '+attack2')
    t.wait(0.5)
    t.expect.value('mode', 'player2', 'iuser1', mode)
    t.expect.value('mode', 'player2', 'iuser2', 1)


@S.test('free map overview: the spectator gets the watched player\'s zoom (SetFOV 30, then 0) for the client\'s '
        'first-person inset; the overview itself keeps its view (client)', players=2, doc=DOC, size=(512, 512, 300))
def map_overview_target_fov(t):
    federal_zoom_setup(t, OBS_MAP_FREE)
    t.expect.msg('SetFOV', to='player2', fields={'fov': 30}, after='zoom', within=(0, 0.5))
    t.expect.msg('SetFOV', to='player2', fields={'fov': 0}, after='unzoom')


@S.test('locked chase: the spectator keeps its own FOV while the watched player zooms (no SetFOV 30)', players=2,
        doc=DOC, size=(512, 512, 300))
def chase_no_target_fov(t):
    federal_zoom_setup(t, OBS_CHASE_LOCKED)
    t.expect.msg('SetFOV', to='player1', fields={'fov': 30}, after='zoom', within=(0, 0.5))
    t.expect.msg('SetFOV', to='player2', fields={'fov': 30}, count=0, after='zoom')
