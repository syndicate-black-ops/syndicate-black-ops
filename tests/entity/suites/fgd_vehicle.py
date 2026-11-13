"""Tier 5, batch 6: func_vehicle FGD items that no 1.38 map uses and no other test asserted
(docs/FEATUREFUL_OPTIONS.md#func_vehicle; Half-Life 25's func_vehicle): Sound choices and Volume, Speed,
Height above track, flags 1 / 2 / 8, Stop sound at halt 0."""

from etest import Suite

SUITE = Suite('fgd_vehicle', 'Tier 5: func_vehicle FGD-only keys and flags', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#func_vehicle'
SOUNDS = {1: 'plats/vehicle1.wav', 2: 'plats/vehicle2.wav', 4: 'plats/vehicle4.wav', 5: 'plats/vehicle6.wav',
          6: 'plats/vehicle7.wav'}


def vehicle(t, name, at=(0, 0, 0), track_dz=0, **kv):
    """A 64 x 32 slab on a two-path_track path (100 apart, the second track_dz higher)."""
    x, y, z = at
    t.point('path_track', at=(x, y, 16), targetname=name + 'p1', target=name + 'p2')
    t.point('path_track', at=(x + 100, y, 16 + track_dz), targetname=name + 'p2')
    keys = dict(speed='300', sounds='3', length='64', width='32', height='4')
    keys.update(kv)
    t.brush('func_vehicle', size=(64, 32, 16), at=at, origin=True, targetname=name, target=name + 'p1', **keys)


@S.test('func_vehicle Sound 1 / 2 / 4 / 6 / 7 at Volume 5: each plays its engine sound at volume 0.5', doc=DOC,
        size=(512, 512, 200),
        covers=('func_vehicle.volume',) + tuple('func_vehicle.sounds=%d' % s for s in SOUNDS))
def sounds(t):
    for i, s in enumerate(SOUNDS):
        vehicle(t, 'v%d' % s, at=(-200, -200 + i * 100, 0), sounds=str(s), volume='5')
    t.wait(0.5)
    for s in SOUNDS:            # on, off (sound stopped), on again: the engine sound starts
        t.fire('v%d' % s)
    t.wait(0.2)
    for s in SOUNDS:
        t.fire('v%d' % s)
    t.wait(0.2)
    t.mark('go')
    for s in SOUNDS:
        t.fire('v%d' % s)
    t.wait(1.2)
    for s, sample in SOUNDS.items():
        t.expect.sound(sample, ent='v%d' % s, vol=(0.45, 0.55), after='go', min=1)


@S.test('func_vehicle Speed 500 (0: 165): toggled on, it runs at that speed', doc=DOC, size=(512, 512, 200),
        covers=('func_vehicle.speed',))
def speed(t):
    vehicle(t, 'fast', at=(-100, -100, 0), speed='500')
    vehicle(t, 'dflt', at=(-100, 100, 0), speed='0')
    t.wait(0.5)
    t.fire('fast')
    t.fire('dflt')
    t.wait_frames(1)
    t.snap('s', 'fast', 'speed')
    t.snap('s', 'dflt', 'speed')
    t.expect.value('s', 'fast', 'speed', 440, op='>')
    t.expect.value('s', 'dflt', 'speed', 120, op='>')
    t.expect.value('s', 'dflt', 'speed', 170, op='<')


@S.test('func_vehicle Height above track 24: its origin rides 24 units above the track (4: 4)', doc=DOC,
        size=(512, 512, 200), covers=('func_vehicle.height',))
def height(t):
    vehicle(t, 'hi', at=(-100, -100, 0), height='24')
    vehicle(t, 'lo', at=(-100, 100, 0), height='4')
    t.wait(0.1)
    for n in ('hi', 'hip1', 'lo', 'lop1'):
        t.snap('s', n, 'origin')
    t.expect.distance('s', 'hi', 'hip1', 23, 25, flat=False)
    t.expect.distance('s', 'lo', 'lop1', 3, 5, flat=False)


@S.test('func_vehicle No Pitch (1): on a sloping track it stays level; without it it pitches', doc=DOC,
        size=(512, 512, 200), covers=('func_vehicle:1',))
def no_pitch(t):
    vehicle(t, 'flat', at=(-100, -100, 0), track_dz=60, spawnflags='1')
    vehicle(t, 'tilt', at=(-100, 100, 0), track_dz=60)
    t.wait(0.1)
    t.snap('s', 'flat', 'angles')
    t.snap('s', 'tilt', 'angles')
    t.expect.value('s', 'flat', 'angles.x', 0, tol=0.5)
    t.expect.value('s', 'tilt', 'angles', [0, 0, 0], op='!=')


@S.test('func_vehicle Passable (8): not solid; without it solid (BSP)', doc=DOC, size=(512, 512, 200),
        covers=('func_vehicle:8',))
def passable(t):
    vehicle(t, 'ghost', at=(-100, -100, 0), spawnflags='8')
    vehicle(t, 'solid', at=(-100, 100, 0))
    t.wait(0.1)
    t.snap('s', 'ghost', 'solid')
    t.snap('s', 'solid', 'solid')
    t.expect.value('s', 'ghost', 'solid', 0)
    t.expect.value('s', 'solid', 'solid', 4)


@S.test('func_vehicle No User Control (2): +use at the controls does not take the wheel (no ignition sound)',
        players=1, doc=DOC, size=(512, 512, 200), covers=('func_vehicle:2',))
def no_user_control(t):
    t.point('path_track', targetname='p1', target='p2', at=(0, -150, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 150, 0))
    t.brush('func_vehicle', size=(128, 128, 16), at=(0, -150, 0), origin=True, targetname='car', target='p1',
            height='8', length='128', width='128', speed='200', acceleration='5', sounds='3', spawnflags='2',
            ignition_sound='plats/ttrain_start1.wav')
    t.brush('func_vehiclecontrols', size=(128, 128, 128), at=(0, -150, 0), target='car')
    t.wait(0.5)
    t.teleport('player1', (0, -150, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.sound('ttrain_start1', after='use', count=0)


@S.test('func_vehicle Stop sound at halt 0: the engine keeps sounding when it stands with nobody at the wheel',
        doc=DOC, size=(512, 512, 200), covers=('func_vehicle.stop_sound_at_halt=0',))
def stop_sound_no(t):
    vehicle(t, 'car', at=(-100, 0, 0), deceleration='150', stop_sound_at_halt='0')
    t.wait(0.5)
    t.mark('go')
    t.fire('car')
    t.wait(1.0)
    t.expect.event('vehicle', ent='car', fields={'bparam1': 1}, after='go', count=0)


@S.test('func_vehicle "acceleration" 10 / 1: the first push forward (the driver\'s Use with value 1) from a standstill '
        'gives 0.0081 / 0.0027 of its Speed (0.015 / 7 + acceleration x 0.0006): about 3 times as fast', doc=DOC,
        size=(512, 512, 200), covers=('func_vehicle.acceleration',))
def acceleration(t):
    vehicle(t, 'a10', at=(-100, -100, 0), acceleration='10')
    vehicle(t, 'a1', at=(-100, 100, 0), acceleration='1')
    t.wait(0.5)
    t.fire('a10', use='set', value=1)
    t.fire('a1', use='set', value=1)
    t.wait_frames(1)
    t.snap('s', 'a10', 'speed')
    t.snap('s', 'a1', 'speed')
    t.expect.value('s', 'a10', 'speed', 300 * (0.015 / 7 + 10 * 0.0006), tol=0.05)
    t.expect.ratio('s', 'a1', 's', 'a10', 'speed', (0.015 / 7 + 10 * 0.0006) / (0.015 / 7 + 0.0006), tol=0.05)



def probe_car(t, name, y, **kv):
    # a 64 x 64 car facing east (yaw 0) on a path to the west: it drives west (CFuncVehicle::Next: the inverted
    # forward); toggled on it runs at its Speed 300 and slows by 20 every 0.1 s (no driver): about 225 units
    t.point('path_track', at=(150, y, 16), targetname=name + 'p1', target=name + 'p2')
    t.point('path_track', at=(-200, y, 16), targetname=name + 'p2')
    keys = dict(speed='300', sounds='0', length='64', width='32', height='4', dmg='0')
    keys.update(kv)
    t.brush('func_vehicle', size=(64, 64, 16), at=(150, y, 0), origin=True, targetname=name, target=name + 'p1', **keys)


def drive(t, names, labels):
    t.wait(0.3)
    for n in names:
        t.fire(n)
    for k in range(25):
        t.wait(0.1)
        labels.append('d%d' % k)
        for n in names:
            t.snap('d%d' % k, n, 'origin')


@S.test('func_vehicle "length" 200 / 64: its front probes (Length / 2 ahead of its origin, 16 long) find a wall '
        'ahead sooner on the longer car: it bounces back with its origin over 90 from the wall; the 64 one comes '
        'within 70', doc=DOC, size=(512, 512, 200), covers=('func_vehicle.length',))
def length_probe(t):
    probe_car(t, 'lng', -120, length='200')
    probe_car(t, 'sht', 120, length='64')
    t.brush('func_wall', targetname='wall', size=(16, 512, 96), at=(0, 0, 0))     # x -8 .. 8, west of both cars
    labels = []
    drive(t, ('lng', 'sht'), labels)
    wall_x = t._abs((8, 0, 0))[0]
    for lab in labels:
        t.expect.value(lab, 'lng', 'origin.x', wall_x + 90, op='>')
    t.expect.value_any(labels, 'sht', 'origin.x', wall_x + 70, op='<')


@S.test('func_vehicle "width" 90 / 32: its front corner probes (Width / 2 to each side) catch a post 36 .. 52 to its side, '
        'beside the car body (32): the 90 wide car bounces back before it, the 32 wide one drives past it', doc=DOC,
        size=(512, 512, 200), covers=('func_vehicle.width',))
def width_probe(t):
    probe_car(t, 'wid', -120, width='90')
    probe_car(t, 'nar', 120, width='32')
    for n, y in (('pw', -120), ('pn', 120)):
        t.brush('func_wall', targetname=n, size=(16, 16, 96), at=(0, y + 44, 0))      # y 36 .. 52 off the lane
    labels = []
    drive(t, ('wid', 'nar'), labels)
    post_x = t._abs((0, 0, 0))[0]
    for lab in labels:
        t.expect.value(lab, 'wid', 'origin.x', post_x, op='>')
    t.expect.value_any(labels, 'nar', 'origin.x', post_x - 20, op='<')
