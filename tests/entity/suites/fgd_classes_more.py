"""Tier 5, final pass: FGD classes no test asserted yet: monster_grunt_repel, xen_hair, xen_spore_small / medium,
cycler_wreckage, info_landmark (the landmark of a seamless trigger_teleport), func_vehiclecontrols. Each test shows
what the class does in play (hgrunt.cpp, xen.cpp, h_cycler.cpp, triggers.cpp, vehicle.cpp)."""

from etest import Suite

SUITE = Suite('fgd_classes_more', 'Tier 5: grunt repel, Xen plants, wreckage, landmark, vehicle controls', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
TE_SMOKE = 5
FL_ONTRAIN = 1 << 24


@S.test('monster_grunt_repel fired: a Fed slides down a rope (a beam) from it, lands on the floor, and the repel '
        'entity is gone', players=1, size=(512, 512, 400), doc=DOC, covers=('monster_grunt_repel',))
def grunt_repel(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_grunt_repel', targetname='rep', at=(0, 100, 300), angles='0 270 0')
    t.point('info_target', targetname='floor', at=(0, 100, 0))
    t.wait(0.5)
    t.mark('go')
    t.fire('rep')
    t.wait(0.3)
    t.snap('mid', '@monster_human_grunt', 'origin', 'movetype')
    t.wait(4.0)
    t.snap('end', '@monster_human_grunt', 'origin')
    t.snap('end', 'floor', 'origin')
    t.expect.spawned('@monster_human_grunt', count=1, after='go')
    t.expect.spawned('@beam', count=1, after='go')
    t.expect.removed('rep', after='go')
    t.expect.value('mid', '@monster_human_grunt', 'origin.z', t._abs((0, 0, 200))[2], op='>')
    t.expect.distance('end', '@monster_human_grunt', 'floor', 0, 20)


@S.test('xen_hair: the hair model sways (its animation plays); Sync Movement (1): every one at rate 1 (in step), '
        'without it a random rate (0.7 .. 1.4)', players=0, doc=DOC, covers=('xen_hair', 'xen_hair:1'))
def xen_hair(t):
    t.point('xen_hair', targetname='hs', at=(-100, 0, 0), spawnflags='1')
    t.point('xen_hair', targetname='hr', at=(100, 0, 0))
    t.wait(0.2)
    t.snap('s', 'hs', 'model', 'framerate')
    t.snap('s', 'hr', 'model', 'framerate')
    t.expect.value('s', 'hs', 'model', 'models/hair.mdl')
    t.expect.value('s', 'hs', 'framerate', 1)
    t.expect.value('s', 'hr', 'framerate', 0.7, op='>=')
    t.expect.value('s', 'hr', 'framerate', 1.4, op='<=')


@S.test('xen_spore_small / xen_spore_medium: the fungus model of that size, solid in its box (32 x 32 x 64 / 80 x 80 '
        'x 120), and it can be hurt', players=0, size=(512, 512, 256), doc=DOC,
        covers=('xen_spore_small', 'xen_spore_medium'))
def xen_spores(t):
    t.point('xen_spore_small', targetname='sm', at=(-120, 0, 0))
    t.point('xen_spore_medium', targetname='md', at=(120, 0, 0))
    t.wait(0.2)
    for n in ('sm', 'md'):
        t.snap('s', n, 'model', 'mins', 'maxs', 'solid', 'takedamage')
    t.expect.value('s', 'sm', 'model', 'models/fungus(small).mdl')
    t.expect.value('s', 'sm', 'mins', [-16, -16, 0])
    t.expect.value('s', 'sm', 'maxs', [16, 16, 64])
    t.expect.value('s', 'md', 'model', 'models/fungus.mdl')
    t.expect.value('s', 'md', 'mins', [-40, -40, 0])
    t.expect.value('s', 'md', 'maxs', [40, 40, 120])
    for n in ('sm', 'md'):
        t.expect.value('s', n, 'solid', 2)          # SOLID_BBOX
        t.expect.value('s', n, 'takedamage', 1)


@S.test('cycler_wreckage "Sprite Name" and "Scale" 2: it shows that sprite at scale 2 and puffs smoke (TE_SMOKE) '
        'about every 0.2 s', players=1, doc=DOC,
        covers=('cycler_wreckage', 'cycler_wreckage.model', 'cycler_wreckage.scale'))
def wreckage(t):
    t.point('cycler_wreckage', targetname='wk', at=(0, 100, 32), model='sprites/fire.spr', scale='2')
    t.wait(0.3)
    t.snap('s', 'wk', 'model', 'scale')
    t.mark('go')
    t.wait(1.0)
    t.expect.value('s', 'wk', 'model', 'sprites/fire.spr')
    t.expect.value('s', 'wk', 'scale', 2)
    t.expect.msg('svc:23', fields={'te': TE_SMOKE}, min=3, max=7, after='go', within=(0, 1.0))


@S.test('info_landmark as the landmark of a trigger_teleport: the player keeps his place relative to it, turned by '
        'the destination\'s yaw minus its own', players=1, doc=DOC, covers=('info_landmark',))
def landmark(t):
    t.trigger('trigger_teleport', at=(-150, -150, 0), size=(96, 96, 72), target='dest', message='mark')
    t.point('info_landmark', targetname='mark', at=(-150, -150, 0), angles='0 0 0')
    t.point('info_target', targetname='dest', at=(150, 120, 0), angles='0 90 0')
    t.wait(0.2)
    t.teleport('player1', (-120, -150, 0), yaw=0)      # 30 east of the landmark
    t.wait(0.3)
    t.snap('after', 'player1', 'origin', 'angles')
    t.snap('after', 'dest', 'origin')
    t.expect.value('after', 'player1', 'angles.y', 90, tol=1)
    t.expect.distance('after', 'player1', 'dest', 28, 32)


def car(t, name, y, controls):
    # a 128 x 128 slab on a path of two path_tracks east -> west: it keeps its spawn yaw 0 (on a path east it turns
    # to 180, and its controls turn with it); the controls: its west strip only
    t.point('path_track', targetname=name + 'p1', target=name + 'p2', at=(0, y, 0))
    t.point('path_track', targetname=name + 'p2', target=name + 'p1', at=(-200, y, 0))
    t.brush('func_vehicle', size=(128, 128, 16), at=(0, y, 0), origin=True, targetname=name, target=name + 'p1',
            height='8', length='128', width='128', speed='200', acceleration='5', sounds='0')
    if controls:
        t.brush('func_vehiclecontrols', size=(32, 128, 128), at=(-48, y, 0), target=name)


def use(t):
    t.move('player1', 0, 0, 3, '+use')
    t.wait(1.0)


@S.test('func_vehiclecontrols "target": a car is driven only from inside its controls (the west strip): +use on the '
        'east side of one car does not take it; on the west side of another it does', players=1,
        size=(512, 512, 200), doc=DOC, covers=('func_vehiclecontrols', 'func_vehiclecontrols.target'))
def vehicle_controls(t):
    car(t, 'ce', -150, True)
    car(t, 'cw', 100, True)
    t.wait(0.5)
    t.snap('a', 'ce', 'angles')
    t.teleport('player1', (40, -150, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    use(t)
    t.snap('east', 'player1', 'flags')
    t.teleport('player1', (-48, 100, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    use(t)
    t.snap('west', 'player1', 'flags')
    use(t)                                       # leave the controls
    t.expect.value('east', 'player1', 'flags', FL_ONTRAIN, op='!&')
    t.expect.value('west', 'player1', 'flags', FL_ONTRAIN, op='&')


@S.test('func_vehicle without controls: +use anywhere on it takes it (east side; control: the test above)',
        players=1, size=(512, 512, 200), doc=DOC, covers=('func_vehicle',))
def vehicle_no_controls(t):
    car(t, 'car', -150, False)
    t.wait(0.5)
    t.teleport('player1', (40, -150, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    use(t)
    t.snap('east', 'player1', 'flags')
    use(t)
    t.expect.value('east', 'player1', 'flags', FL_ONTRAIN, op='&')


@S.test('func_vehicle "target" (first stop target): placed away from its path, it starts on that path_track (its '
        'origin 8 above it, Height above track 8)', players=0, size=(512, 512, 200), doc=DOC,
        covers=('func_vehicle.target',))
def vehicle_target(t):
    t.point('path_track', targetname='q1', target='q2', at=(150, 150, 16))
    t.point('path_track', targetname='q2', at=(150, -150, 16))
    t.brush('func_vehicle', size=(64, 32, 16), at=(-150, -150, 0), origin=True, targetname='vt', target='q1',
            height='8', length='64', width='32', speed='200', sounds='0')
    t.wait(0.3)
    t.snap('s', 'vt', 'origin')
    t.snap('s', 'q1', 'origin')
    t.expect.distance('s', 'vt', 'q1', 7, 9, flat=False)


@S.test('vgui_screen "target": a button command from a player in front of the screen (vgui_screen_command) fires it '
        'with him as activator; the same command from 300 units away (out of reach) does not', players=1, doc=DOC,
        covers=('vgui_screen.target',))
def screen_target(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('vgui_screen', targetname='scr', at=(0, 100, 40), angles='0 270 0', panelname='example_screen',
            width='64', height='32', target='hit')
    t.point('info_target', targetname='hit')
    t.teleport('player1', (0, -220, 0), yaw=90)
    t.wait(1.0)
    t.mark('far')
    t.cmd('player1', 'vgui_screen_command', 'index:' + t.name('scr'), 'go')
    t.wait(1.5)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(0.5)
    t.mark('near')
    t.cmd('player1', 'vgui_screen_command', 'index:' + t.name('scr'), 'go')
    t.wait(0.5)
    t.expect.not_fired('hit', after='far', within=(0, 1.4))
    t.expect.fired('hit', activator='player1', count=1, after='near')
