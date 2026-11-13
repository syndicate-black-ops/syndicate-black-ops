"""Tier 5, final pass: the sound choice values of the movers and buttons no test asserted yet: func_door_rotating
Return Stop Sound and Sound radius, momentary_door Move / Stop Sound and Sound radius, func_plat / func_platrot Sound
radius, every momentary_rot_button Sounds value, func_button Sounds 11. Each value is checked against the sound file
the code plays for it (doors.cpp, plats.cpp, buttons.cpp ButtonSound) and the attenuation of soundradius.h."""

from etest import Suite

SUITE = Suite('fgd_sounds', 'Tier 5: sound choice values of movers and buttons', players=1)
S = SUITE
DOC = 'FGD sound choices'
MOVES = {k: 'doors/doormove%d.wav' % k for k in range(1, 9)}
STOPS = {k: 'doors/doorstop%d.wav' % k for k in range(1, 9)}
RADII = {-2: 2.0, -1: 1.25, 0: 0.8, 1: 0.5, 2: 0.25, 3: 0.0}
BUTTON = {0: 'common/null.wav', 1: 'buttons/button1.wav', 2: 'buttons/button2.wav', 3: 'buttons/button3.wav',
          4: 'buttons/button4.wav', 5: 'buttons/button5.wav', 6: 'buttons/button6.wav', 7: 'buttons/button7.wav',
          8: 'buttons/button8.wav', 9: 'buttons/button9.wav', 10: 'buttons/button10.wav', 11: 'buttons/button11.wav',
          14: 'buttons/lightswitch2.wav', 21: 'buttons/lever1.wav', 22: 'buttons/lever2.wav', 23: 'buttons/lever3.wav',
          24: 'buttons/lever4.wav', 25: 'buttons/lever5.wav'}


def rot_door(t, name, x, **kv):
    kv.setdefault('speed', '400')
    kv.setdefault('wait', '0.2')
    return t.brush('func_door_rotating', targetname=name, size=(16, 32, 64), at=(x, 100, 0), origin=True,
                   distance='45', **kv)


def mdoor(t, name, x, **kv):
    return t.brush('momentary_door', targetname=name, size=(24, 16, 64), at=(x, 100, 0), angles='-90 0 0', lip='8',
                   speed='200', **kv)


# ---------------------------------------------------------------------------------------------- func_door_rotating
@S.test('func_door_rotating Return Stop Sound 1-8 plays that stop sound when it is back closed (Stop Sound 0); 0 the '
        'same as Stop Sound; 255 none', doc=DOC,
        covers=tuple('func_door_rotating.returnstopsnd=%d' % k for k in list(range(0, 9)) + [255]))
def rot_returnstopsnd(t):
    for k in range(1, 9):
        rot_door(t, 'r%d' % k, -240 + 50 * k, returnstopsnd=str(k), stopsnd='0')
    rot_door(t, 'same', 210, returnstopsnd='0', stopsnd='1')
    rot_door(t, 'none', -240, returnstopsnd='255', stopsnd='1')
    t.wait(0.2)
    t.mark('go')
    for k in range(1, 9):
        t.fire('r%d' % k)
    t.fire('same')
    t.fire('none')
    t.wait(1.5)
    for k, snd in STOPS.items():
        t.expect.sound(snd, ent='r%d' % k, count=1, after='go')
    t.expect.sound('doors/doorstop1.wav', ent='same', count=2, after='go')
    t.expect.sound('doors/doorstop1.wav', ent='none', count=1, after='go')


def radius_case(cls):
    @S.test('%s Sound radius: each choice is the attenuation of its move sound (small 2, medium 1.25, default 0.8, '
            'huge 0.5, enormous 0.25, everywhere 0)' % cls, doc=DOC,
            covers=tuple('%s.soundradius=%d' % (cls, k) for k in RADII))
    def case(t):
        for i, k in enumerate(RADII):
            x = -200 + 80 * i
            if cls == 'func_door_rotating':
                rot_door(t, 's%d' % i, x, movesnd='1', soundradius=str(k))
            elif cls == 'momentary_door':
                mdoor(t, 's%d' % i, x, movesnd='1', soundradius=str(k))
            else:
                t.brush(cls, targetname='s%d' % i, size=(48, 48, 16), at=(x, 100, 0), tex='666MURMETAL', height='64',
                        spawnflags='1', speed='300', movesnd='1', stopsnd='1', soundradius=str(k),
                        origin=cls == 'func_platrot')
        t.wait(0.2)
        t.mark('go')
        for i in range(len(RADII)):
            if cls == 'momentary_door':
                t.fire('s%d' % i, use='set', value=1)
            else:
                t.fire('s%d' % i)
        t.wait(0.6)
        snd = 'plats/bigmove1.wav' if cls in ('func_plat', 'func_platrot') else 'doors/doormove1.wav'
        for i, (k, attn) in enumerate(RADII.items()):
            t.expect.sound(snd, ent='s%d' % i, attn=attn, min=1, after='go')
    case.__name__ = 'radius_' + cls.replace('func_', '')
    return case


for _cls in ('func_door_rotating', 'momentary_door', 'func_plat', 'func_platrot'):
    radius_case(_cls)


# ---------------------------------------------------------------------------------------------- momentary_door
@S.test('momentary_door Move Sound 1-8 and Stop Sound 1-8: each door plays them when a value moves it and when it '
        'stops; 0: none', doc=DOC,
        covers=tuple('momentary_door.movesnd=%d' % k for k in range(0, 9))
        + tuple('momentary_door.stopsnd=%d' % k for k in range(0, 9)))
def mdoor_sounds(t):
    for k in range(0, 9):
        mdoor(t, 'm%d' % k, -200 + 50 * k, movesnd=str(k), stopsnd=str(k))
    t.wait(0.2)
    t.mark('go')
    for k in range(0, 9):
        t.fire('m%d' % k, use='set', value=1)
    t.wait(1.5)
    for k in range(1, 9):
        t.expect.sound(MOVES[k], ent='m%d' % k, min=1, after='go')
        t.expect.sound(STOPS[k], ent='m%d' % k, min=1, after='go')
    t.expect.sound('doors/', ent='m0', count=0, after='go')


# ---------------------------------------------------------------------------------------------- buttons
@S.test('momentary_rot_button Sounds: every choice plays its sound (buttons.cpp ButtonSound) while it turns; 0 none',
        players=1, doc=DOC,
        covers=tuple('momentary_rot_button.sounds=%d' % k for k in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 22, 23, 24, 25)))
def momentary_sounds(t):
    vals = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 22, 23, 24, 25)
    for i, k in enumerate(vals):
        t.brush('momentary_rot_button', size=(8, 16, 32), at=(-210 + 30 * i, 100, 28), origin=True,
                targetname='b%d' % i, speed='90', distance='90', sounds=str(k), target='bt%d' % i)
        t.point('info_target', targetname='bt%d' % i, at=(-210 + 30 * i, 0, 0))
    t.wait(0.2)
    t.mark('go')
    for i in range(len(vals)):
        t.fire('b%d' % i, activator='player1')
    t.wait(0.4)
    for i, k in enumerate(vals):
        if k:
            t.expect.sound(BUTTON[k], ent='b%d' % i, min=1, after='go')
        else:
            t.expect.sound('buttons/', ent='b%d' % i, count=0, after='go')


@S.test('func_button Sounds 11: pressed, it plays buttons/button11.wav', players=1, doc=DOC,
        covers=('func_button.sounds=11',))
def button_11(t):
    t.brush('func_button', size=(16, 32, 32), at=(0, 120, 40), targetname='b', speed='400', wait='1', sounds='11',
            angles='0 90 0')
    t.wait(0.2)
    t.mark('go')
    t.fire('b', activator='player1')
    t.wait(0.3)
    t.expect.sound('buttons/button11.wav', ent='b', count=1, after='go')
