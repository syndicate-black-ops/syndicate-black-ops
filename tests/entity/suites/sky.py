"""env_sky (docs/ENV_SKY.md, sdk/syndicate.fgd): the "SetSky" message decoded field by field. One env_sky is on at a
time: at the start the first in the map without Start off; a trigger (not Off) makes that one the sky; Off on the
one that is on leaves the map without 3D sky; a removed active sky turns it off. Parallax scale (frags) 0-255. Every
HUD (re)initialization sends the state. One suite: the sky is map-wide state. Drawing needs the client test layer."""

from etest import Suite

SUITE = Suite('sky', 'env_sky: the 3D sky camera (server side)', players=1)
S = SUITE
DOC = 'ENV_SKY.md'


@S.test('the map\'s first env_sky without Start off is the sky: a fullupdate sends it on, at its point, with its '
        'parallax scale', players=1, doc=DOC, covers=('env_sky', 'env_sky.frags'))
def first_on(t):
    t.point('env_sky', targetname='a', at=(0, 0, 100), frags='16')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.msg('SetSky', to='player1', count=1, after='fu',
                 fields={'mode': 1, 'origin': list(t._abs((0, 0, 100))), 'scale': 16})


@S.test('flag 1 Start off: not the sky at the start; triggered (toggle), it becomes the sky for everybody and the '
        'one that was on goes off; parallax above 255 is sent as 255', players=1, doc=DOC,
        covers=('env_sky:1', 'env_sky.targetname', 'env_sky.frags'))
def start_off_take_over(t):
    t.point('env_sky', targetname='b', at=(0, 0, 100), frags='300', spawnflags='1')
    t.mark('go')
    t.fire('b')
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    org = list(t._abs((0, 0, 100)))
    t.expect.msg('SetSky', count=1, after='go', within=(0, 0.1), fields={'mode': 1, 'origin': org, 'scale': 255})
    t.expect.msg('SetSky', to='player1', count=1, after='fu', fields={'mode': 1, 'origin': org})


@S.test('triggered with On while it is the sky: nothing; Off: no 3D sky (mode 0) for everybody, and a fullupdate '
        'then sends the first env_sky, off', players=1, doc=DOC, covers=('env_sky.targetname',))
def off(t):
    t.point('env_sky', targetname='c', at=(0, 0, 100), spawnflags='1')
    t.fire('c', use='on')
    t.wait(0.2)
    t.mark('on2')
    t.fire('c', use='on')
    t.wait(0.2)
    t.mark('off')
    t.fire('c', use='off')
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.no_msg('SetSky', after='on2', within=(0, 0.15))
    t.expect.msg('SetSky', count=1, after='off', within=(0, 0.1), fields={'mode': 0, 'origin': list(t._abs((0, 0, 100)))})
    t.expect.msg('SetSky', to='player1', count=1, after='fu', fields={'mode': 0})


@S.test('the active sky removed (killtarget): no 3D sky (mode 0) for everybody', players=1, doc=DOC,
        covers=('env_sky',))
def removed(t):
    t.point('env_sky', targetname='d', at=(0, 0, 100), spawnflags='1')
    t.point('trigger_relay', targetname='kill', killtarget='d')
    t.fire('d')
    t.wait(0.2)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.2)
    t.expect.msg('SetSky', count=1, after='kill', fields={'mode': 0, 'origin': list(t._abs((0, 0, 100)))})


# ---- the sky room's entities (SoHL 1.9; src/dlls/client.cpp AddToFullPack, effects.cpp EnvSky_ListEntities). Each
# test room is sealed: from the lobby, nothing in a test room is in the player's PVS. fullpack:player1 is what the
# player's next update holds of the entity: -1 not sent, else its renderfx (128: drawn in the 3D sky).
SKY_BIT = 128


def _lobby(t):
    """The lobby's floor center (0 0 0), relative to the test room."""
    x0, y0, _z0, x1, y1, _z1 = t.room
    return (-(x0 + x1) / 2, -(y0 + y1) / 2, 0)


@S.test('an entity in the sky room of the env_sky that is on goes to a player outside its PVS, marked for the 3D '
        'sky, its render FX as set; before the sky is on it is not sent', players=1, doc=DOC,
        covers=('env_sky', 'env_sky:1'))
def sky_entities_sent(t):
    t.point('env_sky', targetname='sky', at=(0, 0, 100), spawnflags='1')
    t.point('env_sprite', targetname='sun', at=(120, 120, 120), model='sprites/glow01.spr', rendermode='3',
            renderamt='255', renderfx='14', spawnflags='1')
    t.brush('func_wall', size=(64, 64, 64), at=(-120, 120, 0), targetname='wall')
    t.teleport('player1', _lobby(t), yaw=90)
    t.wait(0.2)
    t.snap('off', 'sun', 'fullpack:player1')
    t.snap('off', 'wall', 'fullpack:player1')
    t.fire('sky')
    t.wait(0.2)
    t.snap('on', 'sun', 'fullpack:player1')
    t.snap('on', 'wall', 'fullpack:player1')
    t.expect.value('off', 'sun', 'fullpack:player1', -1)
    t.expect.value('off', 'wall', 'fullpack:player1', -1)
    t.expect.value('on', 'sun', 'fullpack:player1', SKY_BIT | 14)
    t.expect.value('on', 'wall', 'fullpack:player1', SKY_BIT)


@S.test('a player in the sky room itself gets its entities as ordinary entities (not marked)', players=1, doc=DOC,
        covers=('env_sky',))
def sky_entities_in_room(t):
    t.point('env_sky', targetname='sky', at=(0, 0, 100), spawnflags='1')
    t.brush('func_wall', size=(64, 64, 64), at=(-120, 120, 0), targetname='wall', renderfx='19')
    t.fire('sky')
    t.wait(0.2)
    t.snap('in', 'wall', 'fullpack:player1')
    t.expect.value('in', 'wall', 'fullpack:player1', 19)


@S.test('a monster put into the sky room later is sent to every player within a second (the sky room is listed '
        'again every second); with the sky off nothing of the room is sent', players=1, doc=DOC,
        covers=('env_sky.targetname',))
def sky_entities_later(t):
    t.point('env_sky', targetname='sky', at=(0, 0, 100), spawnflags='1')
    t.point('monstermaker', targetname='maker', at=(0, 150, 0), monstertype='monster_scientist', netname='kid',
            monstercount='1', names=('netname',))
    t.teleport('player1', _lobby(t), yaw=90)
    t.fire('sky')
    t.wait(0.2)
    t.fire('maker')
    t.wait(1.3)
    t.snap('later', 'kid', 'fullpack:player1')
    t.fire('sky', use='off')
    t.wait(0.2)
    t.snap('off', 'kid', 'fullpack:player1')
    t.expect.value('later', 'kid', 'fullpack:player1', SKY_BIT, op='&')
    t.expect.value('off', 'kid', 'fullpack:player1', -1)
