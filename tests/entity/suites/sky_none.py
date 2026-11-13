"""A map without env_sky (docs/ENV_SKY.md): nothing is sent outside a player's PVS and no entity is marked for the 3D
sky (src/dlls/client.cpp AddToFullPack). fullpack:player1: -1 not sent, else the state's renderfx."""

from etest import Suite

SUITE = Suite('sky_none', 'env_sky: a map without it sends as before', players=1)
S = SUITE
DOC = 'ENV_SKY.md'


@S.test('no env_sky: an entity in a sealed room is not sent to a player in the lobby, and sent unmarked to a player '
        'in its room', players=1, doc=DOC)
def no_sky(t):
    t.brush('func_wall', size=(64, 64, 64), at=(0, 120, 0), targetname='wall', renderfx='19')
    x0, y0, _z0, x1, y1, _z1 = t.room
    t.snap('room', 'wall', 'fullpack:player1')
    t.teleport('player1', (-(x0 + x1) / 2, -(y0 + y1) / 2, 0), yaw=90)
    t.wait(0.2)
    t.snap('lobby', 'wall', 'fullpack:player1')
    t.expect.value('room', 'wall', 'fullpack:player1', 19)
    t.expect.value('lobby', 'wall', 'fullpack:player1', -1)
