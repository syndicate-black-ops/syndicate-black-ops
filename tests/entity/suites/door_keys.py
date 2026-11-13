"""Featureful door keys (sdk/syndicate.fgd base Door, docs/FEATUREFUL_OPTIONS.md) the suite options does not check:
every "Stop sound on the way back" choice (returnstopsnd), every "Sound radius" choice as the sound's attenuation,
and the custom sounds, return speed and locked_play_on_use on func_door_rotating and func_water (options checks them on
func_door)."""

from etest import Suite

SUITE = Suite('door_keys', 'Door keys: return stop sound, sound radius, custom sounds on the other door classes',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
STOPS = {k: 'doors/doorstop%d.wav' % k for k in range(1, 9)}


def door(t, name, x, cls='func_door', **kv):
    kv.setdefault('speed', '400')
    kv.setdefault('wait', '0.2')
    if cls == 'func_door_rotating':
        return t.brush(cls, targetname=name, size=(16, 32, 64), at=(x, 100, 0), origin=True, distance='45', **kv)
    extra = {'skin': '-3'} if cls == 'func_water' else {}
    return t.brush(cls, targetname=name, size=(24, 16, 64), at=(x, 100, 0), angles='-90 0 0', lip='8', **dict(extra, **kv))


@S.test('returnstopsnd: each choice 1-8 plays its stop sound when the door is back closed (none at the top with Stop '
        'Sound 0); 0 the same as Stop Sound; 255 none', doc=DOC,
        covers=tuple('func_door.returnstopsnd=%d' % k for k in list(range(0, 9)) + [255]))
def returnstopsnd(t):
    for k in range(1, 9):
        door(t, 'r%d' % k, -240 + 50 * k, returnstopsnd=str(k), stopsnd='0')
    door(t, 'same', 210, returnstopsnd='0', stopsnd='1')
    door(t, 'none', -240, returnstopsnd='255', stopsnd='1')
    t.mark('go')
    for k in range(1, 9):
        t.fire('r%d' % k)
    t.fire('same')
    t.fire('none')
    t.wait(1.0)
    for k, snd in STOPS.items():
        t.expect.sound(snd, ent='r%d' % k, count=1, after='go', within=(0.25, 0.8))
    t.expect.sound('doors/doorstop1.wav', ent='same', count=2, after='go')
    t.expect.sound('doors/doorstop1.wav', ent='none', count=1, after='go', within=(0, 0.3))
    t.expect.sound('doors/doorstop', ent='none', count=0, after='go', within=(0.3, 1.0))


RADII = {-2: 2.0, -1: 1.25, 0: 0.8, 1: 0.5, 2: 0.25, 3: 0.0}


@S.test('soundradius: each choice is the attenuation of the door\'s sounds (small 2, medium 1.25, default 0.8, huge 0.5, '
        'enormous 0.25, everywhere 0)', doc=DOC, covers=tuple('func_door.soundradius=%d' % k for k in RADII))
def soundradius(t):
    for i, k in enumerate(RADII):
        door(t, 's%d' % i, -200 + 80 * i, movesnd='1', soundradius=str(k))
    t.mark('go')
    for i in range(len(RADII)):
        t.fire('s%d' % i)
    t.wait(0.3)
    for i, (k, attn) in enumerate(RADII.items()):
        t.expect.sound('doors/doormove1.wav', ent='s%d' % i, attn=attn, min=1, after='go')


def custom_sounds_test(cls):
    def f(t):
        t.point('info_target', targetname='closed')
        door(t, 'd', 0, cls=cls, speed='50', wait='0.2', noise1='doors/doormove3.wav', noise2='doors/doorstop1.wav',
             noise3='doors/doorstop5.wav', soundradius='1', return_speed='800', fireonclosed='closed',
             names=('fireonclosed',))
        t.mark('go')
        t.fire('d')
        t.wait(2.5)
        t.expect.sound('doors/doormove3.wav', ent='d', attn=0.5, min=2, after='go')
        t.expect.sound('doors/doorstop1.wav', ent='d', count=1, after='go')
        t.expect.sound('doors/doorstop5.wav', ent='d', count=1, after='go')
        # 45 degrees at 50 there (0.9 s), 0.2 s wait, back at 800 (0.06 s): about 1.16 s (2 s without return_speed)
        t.expect.used('closed', after='go', within=(0.95, 1.6))
    f.__name__ = 'custom_' + cls
    S.test('%s custom sounds (noise1 move, noise2 stop, noise3 stop on the way back), Sound radius Huge, Return speed '
           '800 (4 times its speed)' % cls, doc=DOC,
           covers=(cls, cls + '.noise1', cls + '.noise2', cls + '.noise3', cls + '.soundradius=1',
                   cls + '.return_speed'))(f)


def locked_use_test(cls):
    def f(t):
        t.point('multisource', targetname='lockms')
        t.point('trigger_relay', targetname='lockin', target='lockms')
        door(t, 'dlk', -100, cls=cls, master='lockms', locked_sound='2', locked_play_on_use='1')
        door(t, 'dlq', 100, cls=cls, master='lockms', locked_sound='3', locked_play_on_use='0')
        t.mark('go')
        t.fire('dlk', activator='player1')
        t.fire('dlq', activator='player1')
        t.wait(0.5)
        t.expect.sound('buttons/button2.wav', ent='dlk', count=1, after='go')
        t.expect.sound('buttons/button3.wav', count=0, after='go')
    f.__name__ = 'locked_' + cls
    S.test('%s locked_play_on_use 1: triggered while its master is locked it plays its locked sound; the same door '
           'is silent with it No (0)' % cls, players=1, doc=DOC,
           covers=(cls, cls + '.locked_play_on_use=1', cls + '.locked_play_on_use=0'))(f)


custom_sounds_test('func_door_rotating')


@S.test('func_water is silent (SF_DOOR_SILENT, as in Half-Life): Door sound keys a map sets anyway (movesnd, stopsnd, '
        'noise1-3, returnstopsnd, soundradius; not in its FGD class) are ignored', doc=DOC, covers=('func_water',))
def water_silent(t):
    t.point('info_target', targetname='closed')
    door(t, 'd', 0, cls='func_water', speed='50', wait='0.2', movesnd='1', stopsnd='1', returnstopsnd='2',
         noise1='doors/doormove3.wav', noise2='doors/doorstop1.wav', noise3='doors/doorstop5.wav', soundradius='1',
         return_speed='800', fireonclosed='closed', names=('fireonclosed',))
    t.mark('go')
    t.fire('d')
    t.wait(2.5)
    t.expect.used('closed', after='go')         # it moved there and back
    t.expect.sound('doors/', ent='d', count=0, after='go')
for _c in ('func_door', 'func_door_rotating', 'func_water'):
    locked_use_test(_c)


@S.test('func_water Return speed 800: it comes back 4 times as fast as its speed (no sounds: test water_silent)',
        doc=DOC, covers=('func_water.return_speed',))
def water_return(t):
    t.point('info_target', targetname='opened')
    t.point('info_target', targetname='closed')
    door(t, 'd', 0, cls='func_water', speed='50', wait='0.2', return_speed='800', fireonopened='opened',
         fireonclosed='closed', names=('fireonopened', 'fireonclosed'))
    t.mark('go')
    t.fire('d')
    t.wait(2.5)
    # 56 units: 1.12 s up at 50, 0.2 s wait, 0.07 s down at 800: about 1.4 s (2.4 s without return_speed)
    t.expect.used('opened', after='go', within=(0.9, 1.4))
    t.expect.used('closed', after='go', within=(1.2, 1.9))
