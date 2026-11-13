"""Movers and physics of SBO 2.0 on a real server (GAPS.md tier 2 "Movers and physics"): func_pushable after the HL25
rework (6fa513ad: pushed by walking into it, not while +use is held; sv_pushable_fixed_tick_fudge), func_train with
path_corner wait and speed, func_tracktrain on its path (the think fix of fix/tracktrain-crush 9bc54b63) and
path_track speed, func_rotating flags and spin-up (fabs, 6fa513ad), func_plat, func_wall_toggle. What each checks is
the FGD's description (sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('movers', 'func_pushable, func_train, func_tracktrain, func_rotating, func_plat, func_wall_toggle')
S = SUITE

SOLID_NOT, SOLID_BSP = 0, 4
EF_NODRAW = 128
DMG_CRUSH = 1


def info(t, name, at=(200, -200, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


# ------------------------------------------------------------------------------------------------ func_pushable

@S.test('func_pushable: a player walking into it pushes it the way he walks (HL25: the push is the touch without '
        '+use)', players=1, covers=('func_pushable',))
def pushable_push(t):
    t.brush('func_pushable', size=(32, 32, 32), at=(0, 0, 0), origin=True, targetname='box', friction='50')
    t.wait(1.0)
    t.teleport('player1', (0, -60, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.snap('before', 'box', 'origin')
    t.walk('player1', 250, 0, 1.0)
    t.wait(0.5)
    t.snap('after', 'box', 'origin')
    t.expect.moved('before', 'after', 'box', min_dist=24, flat=True)
    t.expect.value('after', 'box', 'origin.y', t._abs((0, 30, 0))[1], op='>')


@S.test('func_pushable: a player holding +use on it and walking backwards pulls it along (HL25: the pull adds his '
        'velocity at a fixed rate per second, sv_pushable_fixed_tick_fudge 15, not per frame)', players=1,
        covers=('func_pushable',))
def pushable_pull(t):
    t.brush('func_pushable', size=(32, 32, 32), at=(0, 0, 0), origin=True, targetname='box', friction='50')
    t.wait(1.0)
    t.teleport('player1', (0, -40, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.snap('before', 'box', 'origin')
    t.walk('player1', -150, 0, 1.5, '+use')
    t.wait(0.5)
    t.snap('after', 'box', 'origin')
    t.expect.value('after', 'box', 'origin.y', t._abs((0, -8, 0))[1], op='<')


@S.test('sv_pushable_fixed_tick_fudge (HL25) is registered with its default 15', covers=('func_pushable',))
def pushable_fudge_cvar(t):
    t.console('sv_pushable_fixed_tick_fudge')
    t.wait(0.1)
    t.expect.console('"sv_pushable_fixed_tick_fudge" is "15"')


# ------------------------------------------------------------------------------------------------ func_train

@S.test('func_train: it travels to its next path_corner at its speed, waits there for the corner\'s "wait" and fires '
        'the corner\'s "message" (Fire On Pass) on arrival', covers=('func_train', 'func_train.speed', 'path_corner.wait',
                                                    'path_corner.message'))
def train_corner_wait(t):
    t.point('path_corner', at=(-200, 100, 64), targetname='c1', target='c2')
    t.point('path_corner', at=(200, 100, 64), targetname='c2', target='c3', wait='1', message='arrived',
            names=('message',))
    t.point('path_corner', at=(200, -100, 64), targetname='c3')
    info(t, 'arrived')
    t.brush('func_train', size=(32, 32, 32), at=(-200, 100, 64), targetname='train', target='c1', speed='200')
    t.wait(0.5)
    t.mark('go')
    t.fire('train')
    t.wait(2.2)     # 400 units at 200: there after 2 s
    t.snap('at_c2', 'train', 'origin')
    t.wait(0.5)     # still waiting (wait 1: until 3 s)
    t.snap('waiting', 'train', 'origin')
    t.wait(1.0)     # on its way to c3
    t.snap('leaving', 'train', 'origin')
    t.expect.fired('arrived', after='go', within=(1.7, 2.4))
    t.expect.same('at_c2', 'waiting', 'train', 'origin')
    t.expect.moved('waiting', 'leaving', 'train', min_dist=40)


# ------------------------------------------------------------------------------------------------ func_tracktrain

@S.test('func_tracktrain: started by a trigger it moves along its path_tracks (CFuncTrackTrain::Next thinks again; '
        '9bc54b63), and with flag 2 No User Control a path_track "speed" changes its speed when it passes (HL SDK: '
        'only then)', covers=('func_tracktrain', 'func_tracktrain:2', 'func_tracktrain.speed', 'path_track.speed'))
def tracktrain_path(t):
    t.point('path_track', at=(-200, 0, 32), targetname='p1', target='p2')
    t.point('path_track', at=(-100, 0, 32), targetname='p2', target='p3', speed='50')
    t.point('path_track', at=(200, 0, 32), targetname='p3')
    t.brush('func_tracktrain', size=(64, 32, 32), at=(-200, 0, 32), origin=True, targetname='tt', target='p1',
            speed='200', height='0', wheels='16', bank='0', spawnflags='2')
    t.wait(0.5)
    t.snap('start', 'tt', 'origin')
    t.fire('tt', use='on')
    t.wait(1.5)
    t.snap('a', 'tt', 'origin', 'speed')
    t.wait(1.0)
    t.snap('b', 'tt', 'origin', 'speed')
    t.expect.moved('start', 'a', 'tt', min_dist=90)
    t.expect.value('a', 'tt', 'origin.y', t._abs((0, 0, 0))[1], tol=1)     # on the path line, no jumps aside
    t.expect.value('b', 'tt', 'speed', 50, tol=1)


# ------------------------------------------------------------------------------------------------ func_rotating

@S.test('func_rotating: flag 1 Start ON spins at its speed about Z; flag 2 Reverse spins the other way; flags 4 / 8 '
        'spin about X / Y', covers=('func_rotating:1', 'func_rotating:2', 'func_rotating:4', 'func_rotating:8',
                                    'func_rotating.speed'))
def rotating_axes(t):
    for x, name, flags in ((-180, 'rz', 1), (-60, 'rrev', 3), (60, 'rx', 5), (180, 'ry', 9)):
        t.brush('func_rotating', size=(32, 32, 32), at=(x, 100, 32), origin=True, targetname=name,
                spawnflags=str(flags), speed='90')
    t.wait(0.5)
    t.snap('s', 'rz', 'avelocity')
    t.snap('s', 'rrev', 'avelocity')
    t.snap('s', 'rx', 'avelocity')
    t.snap('s', 'ry', 'avelocity')
    t.expect.value('s', 'rz', 'avelocity', [0, 90, 0], tol=0.5)
    t.expect.value('s', 'rrev', 'avelocity', [0, -90, 0], tol=0.5)
    t.expect.value('s', 'rx', 'avelocity', [0, 0, 90], tol=0.5)
    t.expect.value('s', 'ry', 'avelocity', [90, 0, 0], tol=0.5)


@S.test('func_rotating flag 16 Spin up with acceleration, Friction 10: switched on it speeds up over about a second '
        '(halfway there after 0.45 s), then spins at exactly its speed (fabs: 1.38 truncated the speeds and could end '
        'the spin-up below it); switched off it spins down to 0', covers=('func_rotating:16', 'func_rotating.fanfriction'))
def rotating_spinup(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 100, 32), origin=True, targetname='fan', spawnflags='16',
            speed='45.5', fanfriction='10')
    t.wait(0.5)
    t.fire('fan')
    t.wait(0.45)
    t.snap('mid', 'fan', 'avelocity')
    t.wait(3.55)
    t.snap('up', 'fan', 'avelocity')
    t.fire('fan')
    t.wait(5.0)
    t.snap('down', 'fan', 'avelocity')
    t.expect.value('mid', 'fan', 'avelocity.y', 5, op='>')
    t.expect.value('mid', 'fan', 'avelocity.y', 40, op='<')
    t.expect.value('up', 'fan', 'avelocity', [0, 45.5, 0], tol=0.05)
    t.expect.value('down', 'fan', 'avelocity', [0, 0, 0], tol=0.05)


# ------------------------------------------------------------------------------------------------ func_plat

@S.test('func_plat (no name): a player stepping on it at the bottom takes it up by its height; it goes down again '
        'after he leaves', players=1, covers=('func_plat', 'func_plat.height', 'func_plat.speed'))
def plat_ride(t):
    t.brush('func_plat', size=(96, 96, 16), at=(0, 100, 0), tex='666MURMETAL', height='64', speed='200')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(1.0)
    t.snap('bottom', '@func_plat', 'origin')
    t.teleport('player1', (0, 100, 16), yaw=90)
    t.wait(1.5)
    t.snap('top', '@func_plat', 'origin')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(3.5)
    t.snap('back', '@func_plat', 'origin')
    t.expect.value('top', '@func_plat', 'origin.z', 0, tol=0.5)
    t.expect.value('bottom', '@func_plat', 'origin.z', -64, tol=0.5)
    t.expect.value('back', '@func_plat', 'origin.z', -64, tol=0.5)


# ------------------------------------------------------------------------------------------------ func_wall_toggle

@S.test('func_wall_toggle flag 1 Starts Invisible: not solid and not drawn; fired, it is solid and drawn; fired '
        'again, gone again', covers=('func_wall_toggle', 'func_wall_toggle:1'))
def wall_toggle(t):
    t.brush('func_wall_toggle', size=(64, 16, 64), at=(0, 100, 0), targetname='wt', spawnflags='1')
    t.wait(0.3)
    t.snap('start', 'wt', 'solid', 'effects')
    t.fire('wt')
    t.wait(0.2)
    t.snap('on', 'wt', 'solid', 'effects')
    t.fire('wt')
    t.wait(0.2)
    t.snap('off', 'wt', 'solid', 'effects')
    t.expect.value('start', 'wt', 'solid', SOLID_NOT)
    t.expect.value('start', 'wt', 'effects', EF_NODRAW, op='&')
    t.expect.value('on', 'wt', 'solid', SOLID_BSP)
    t.expect.value('on', 'wt', 'effects', EF_NODRAW, op='!&')
    t.expect.value('off', 'wt', 'solid', SOLID_NOT)
