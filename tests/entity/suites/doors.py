"""func_door and func_door_rotating on a real server (GAPS.md tier 1 "func_door / door_rotating", "Masters on the
classes"): flags 1 Starts Open, 8 Passable, 32 Toggle, 256 Use Only; master with the locked / unlocked sounds; move and
stop sounds; delay, killtarget, "Fire on Close" (netname); dmg when blocked; door_rotating distance, flags 2 Reverse
Dir, 16 One-way, 64 / 128 axes, master. What each checks is the FGD's description (sdk/syndicate.fgd, base Door).

A door that has a name opens only when triggered (touching it plays its locked sound): the touch tests use doors
without a name, found in the trace by class (@func_door, in the test's own window)."""

from etest import Suite

SUITE = Suite('doors', 'func_door / func_door_rotating: flags, master, sounds, delay, killtarget, dmg')
S = SUITE

SOLID_NOT, SOLID_BSP = 0, 4
DMG_CRUSH = 1
MOVE1, STOP1 = 'doors/doormove1.wav', 'doors/doorstop1.wav'


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def lock(t, name='lock', key='key'):
    t.point('multisource', targetname=name)
    t.point('trigger_relay', targetname=key, target=name, triggerstate='1')


def updoor(t, at=(0, 100, 0), size=(96, 16, 96), **kv):
    """A door 96 high moving up 88 (lip 8) at 400 u/s: open in 0.22 s."""
    kv.setdefault('speed', '400')
    kv.setdefault('lip', '8')
    kv.setdefault('wait', '1')
    return t.brush('func_door', size=size, at=at, tex='666MURMETAL', angles='-90 0 0', **kv)


def wall_door(t, **kv):
    """A door across the whole room (y = 0): a player south of it can't get north unless it is open or passable."""
    return t.brush('func_door', size=(512, 16, 256), at=(0, 0, 0), tex='666MURMETAL', angles='-90 0 0', speed='100',
                   lip='8', **kv)


# ------------------------------------------------------------------------------------------------ func_door

@S.test('func_door flag 1 Starts Open: it spawns open; fired, it closes and fires its "Fire on Close" (netname)',
        covers=('func_door:1', 'func_door.netname'))
def door_starts_open(t):
    updoor(t, targetname='door', spawnflags='1', wait='-1', netname='closed', names=('netname',))
    info(t, 'closed')
    t.wait(0.3)
    t.snap('spawn', 'door', 'origin')
    t.mark('go')
    t.fire('door')
    t.wait(0.6)
    t.snap('after', 'door', 'origin')
    t.expect.value('spawn', 'door', 'origin', [0, 0, 88], tol=0.5)
    t.expect.value('after', 'door', 'origin', [0, 0, 0], tol=0.5)
    t.expect.fired('closed', count=1, after='go', within=(0.15, 0.4))


@S.test('func_door flag 8 Passable: a player walks through the closed door; through a door without the flag he can\'t',
        players=1, covers=('func_door:8',))
def door_passable(t):
    wall_door(t, targetname='door', spawnflags='8')
    info(t, 'start', at=(0, -100, 0))
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'start', 'origin')
    t.snap('after', 'door', 'solid')
    t.expect.distance('after', 'player1', 'start', 150, 400)        # past the door at 100
    t.expect.value('after', 'door', 'solid', SOLID_NOT)


@S.test('func_door without Passable: a player can\'t walk through the closed door (control)', players=1,
        covers=('func_door',))
def door_not_passable(t):
    wall_door(t, targetname='door')
    info(t, 'start', at=(0, -100, 0))
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'start', 'origin')
    t.snap('after', 'door', 'solid')
    t.expect.distance('after', 'player1', 'start', 0, 90)           # stopped at the door
    t.expect.value('after', 'door', 'solid', SOLID_BSP)


@S.test('func_door flag 32 Toggle: it stays open past its wait; fired again, it closes', covers=('func_door:32',))
def door_toggle(t):
    updoor(t, targetname='door', spawnflags='32', wait='0.2')
    t.fire('door')
    t.wait(1.0)
    t.snap('open', 'door', 'origin')
    t.fire('door')
    t.wait(0.6)
    t.snap('closed', 'door', 'origin')
    t.expect.value('open', 'door', 'origin', [0, 0, 88], tol=0.5)
    t.expect.value('closed', 'door', 'origin', [0, 0, 0], tol=0.5)


@S.test('func_door flag 256 Use Only: a player touching it does not open it; +use does (player as activator)',
        players=1, covers=('func_door:256',))
def door_use_only(t):
    updoor(t, at=(0, 120, 0), target='opened', spawnflags='256')
    info(t, 'opened')
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.wait(0.2)
    t.mark('touch')
    t.walk('player1', 200, 0, 0.5)              # pushes into the door
    t.wait(0.3)
    t.mark('use')
    t.look('player1', 0, 90)
    t.move('player1', 0, 0, 10, '+use')
    t.wait(0.6)
    t.expect.not_fired('opened', after='touch', within=(0, 0.8))
    t.expect.used('@func_door', activator='player1', after='use', min=1)
    t.expect.fired('opened', activator='player1', count=1, after='use', within=(0, 0.6))


@S.test('func_door without Use Only and without a name: a player touching it opens it (control)', players=1,
        covers=('func_door',))
def door_touch_opens(t):
    updoor(t, at=(0, 120, 0), target='opened')
    info(t, 'opened')
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.wait(0.2)
    t.mark('touch')
    t.walk('player1', 200, 0, 0.5)
    t.wait(0.3)
    t.expect.fired('opened', activator='player1', count=1, after='touch', within=(0, 0.8))


@S.test('func_door master: touched while locked it stays shut and plays its locked sound; unlocked, it opens with its '
        'unlocked sound', players=1,
        covers=('func_door.master', 'func_door.locked_sound=2', 'func_door.unlocked_sound=3'))
def door_master_sounds(t):
    lock(t)
    updoor(t, at=(0, 120, 0), target='opened', master='lock', locked_sound='2', unlocked_sound='3')
    info(t, 'opened')
    t.wait(0.3)
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.mark('locked')
    t.walk('player1', 200, 0, 0.5)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(3.2)                                 # the lock sounds share one 3 s wait (DOOR_SOUNDWAIT)
    t.mark('unlock')
    t.fire('key')
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.walk('player1', 200, 0, 0.5)
    t.wait(0.3)
    t.expect.sound('buttons/button2.wav', ent='@func_door', after='locked', min=1)
    t.expect.not_fired('opened', after='locked', within=(0, 0.5))
    t.expect.fired('opened', activator='player1', count=1, after='unlock')
    t.expect.sound('buttons/button3.wav', ent='@func_door', after='unlock', count=1)


@S.test('func_door master: fired while locked it does not move; fired once unlocked it opens',
        covers=('func_door.master',))
def door_master_fired(t):
    lock(t)
    updoor(t, targetname='door', master='lock')
    t.wait(0.3)
    t.fire('door')
    t.wait(0.5)
    t.snap('locked', 'door', 'origin')
    t.fire('key')
    t.fire('door')
    t.wait(0.4)
    t.snap('open', 'door', 'origin')
    t.expect.value('locked', 'door', 'origin', [0, 0, 0], tol=0.5)
    t.expect.value('open', 'door', 'origin', [0, 0, 88], tol=0.5)


@S.test('func_door movesnd / stopsnd: the move sound when it starts, the stop sound when it arrives',
        covers=('func_door.movesnd=1', 'func_door.stopsnd=1'))
def door_sounds(t):
    updoor(t, targetname='door', movesnd='1', stopsnd='1', wait='-1', speed='200')
    t.mark('go')
    t.fire('door')
    t.wait(0.8)
    t.expect.sound(MOVE1, ent='door', count=1, after='go', within=(0, 0.1))
    t.expect.sound(STOP1, ent='door', count=1, after='go', within=(0.35, 0.6))     # 88 units at 200 u/s


@S.test('func_door delay and killtarget: its target fires and the killtarget is removed "delay" seconds after it is open',
        covers=('func_door.delay', 'func_door.killtarget', 'func_door.target'))
def door_delay_killtarget(t):
    updoor(t, targetname='door', target='out', killtarget='victim', delay='0.5', wait='-1')
    info(t, 'out')
    info(t, 'victim', at=(100, -100, 0))
    t.mark('go')
    t.fire('door')
    t.wait(1.2)
    t.expect.fired('out', count=1, after='go', within=(0.65, 0.9))      # open at 0.22 s, then the delay
    t.expect.removed('victim', count=1, after='go', within=(0.65, 0.9))


@S.test('func_door dmg: a player blocking it is hurt (DMG_CRUSH)', players=1, covers=('func_door.dmg',))
def door_dmg(t):
    # slides east (angle 0) over 30 + 350 units into the player standing against the east wall
    t.brush('func_door', size=(32, 128, 128), at=(-150, 0, 0), tex='666MURMETAL', targetname='door', angles='0 0 0',
            speed='300', lip='-350', wait='-1', dmg='15')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (220, 0, 0))
    t.wait(0.2)
    t.mark('go')
    t.fire('door')
    t.wait(1.2)
    t.flags('player1', '-FL_GODMODE')
    t.expect.damaged('player1', attacker='door', after='go', type=DMG_CRUSH)


# ------------------------------------------------------------------------------------------------ func_door_rotating

def rotdoor(t, name, at, **kv):
    kv.setdefault('speed', '360')
    kv.setdefault('distance', '90')
    kv.setdefault('wait', '-1')
    return t.brush('func_door_rotating', size=(64, 8, 64), at=at, origin=True, targetname=name, tex='666MURMETAL',
                   **kv)


@S.test('func_door_rotating: distance (degrees) about the Z axis; flag 2 Reverse Dir turns the other way; flags 64 X '
        'Axis / 128 Y Axis turn about those axes (roll, pitch)',
        covers=('func_door_rotating', 'func_door_rotating.distance', 'func_door_rotating:2', 'func_door_rotating:64',
                'func_door_rotating:128'))
def rotdoor_axes(t):
    rotdoor(t, 'rz', (-150, 100, 0), distance='70')
    rotdoor(t, 'rrev', (-50, 100, 0), spawnflags='2')
    rotdoor(t, 'rx', (50, 100, 0), spawnflags='64')
    rotdoor(t, 'ry', (150, 100, 0), spawnflags='128')
    for n in ('rz', 'rrev', 'rx', 'ry'):
        t.fire(n)
    t.wait(0.8)
    for n in ('rz', 'rrev', 'rx', 'ry'):
        t.snap('open', n, 'angles')
    t.expect.value('open', 'rz', 'angles', [0, 70, 0], tol=0.5)
    t.expect.value('open', 'rrev', 'angles', [0, -90, 0], tol=0.5)
    t.expect.value('open', 'rx', 'angles', [0, 0, 90], tol=0.5)
    t.expect.value('open', 'ry', 'angles', [90, 0, 0], tol=0.5)


@S.test('func_door_rotating: it opens away from the player who opens it; flag 16 One-way: always the same way',
        players=1, covers=('func_door_rotating:16',))
def rotdoor_oneway(t):
    # the player stands south-east of the hinges; facing north the door turns +90, facing south -90 (away from him)
    rotdoor(t, 'away_n', (-150, 100, 0))
    rotdoor(t, 'away_s', (0, 100, 0))
    rotdoor(t, 'oneway_s', (150, 100, 0), spawnflags='16')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.2)
    t.fire('away_n', activator='player1')
    t.look('player1', 0, 270)
    t.wait(0.1)
    t.fire('away_s', activator='player1')
    t.fire('oneway_s', activator='player1')
    t.wait(0.8)
    for n in ('away_n', 'away_s', 'oneway_s'):
        t.snap('open', n, 'angles')
    t.expect.value('open', 'away_n', 'angles', [0, 90, 0], tol=0.5)
    t.expect.value('open', 'away_s', 'angles', [0, -90, 0], tol=0.5)
    t.expect.value('open', 'oneway_s', 'angles', [0, 90, 0], tol=0.5)


@S.test('func_door_rotating master: fired while locked it does not turn; unlocked, it does',
        covers=('func_door_rotating.master',))
def rotdoor_master(t):
    lock(t)
    rotdoor(t, 'rd', (0, 100, 0), master='lock')
    t.wait(0.3)
    t.fire('rd')
    t.wait(0.5)
    t.snap('locked', 'rd', 'angles')
    t.fire('key')
    t.fire('rd')
    t.wait(0.6)
    t.snap('open', 'rd', 'angles')
    t.expect.value('locked', 'rd', 'angles', [0, 0, 0], tol=0.5)
    t.expect.value('open', 'rd', 'angles', [0, 90, 0], tol=0.5)


@S.test('func_door_rotating flag 256 Use Only: touching does not open it, +use does', players=1,
        covers=('func_door_rotating:256',))
def rotdoor_use_only(t):
    t.brush('func_door_rotating', size=(96, 16, 96), at=(0, 120, 0), origin=True, tex='666MURMETAL', target='opened',
            speed='360', distance='90', wait='-1', spawnflags='256')
    info(t, 'opened')
    t.teleport('player1', (0, 60, 0), yaw=90)
    t.wait(0.2)
    t.mark('touch')
    t.walk('player1', 200, 0, 0.5)
    t.wait(0.2)
    t.mark('use')
    t.look('player1', 0, 90)
    t.move('player1', 0, 0, 10, '+use')
    t.wait(0.6)
    t.expect.not_fired('opened', after='touch', within=(0, 0.7))
    t.expect.fired('opened', activator='player1', count=1, after='use')
