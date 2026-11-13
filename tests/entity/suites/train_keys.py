"""Featureful PlatSounds keys (sdk/syndicate.fgd) on func_train and func_platrot: custom move / stop sound files,
Sound radius as the attenuation; func_platrot's Fire On targets with their use types (suite fireon checks
func_plat); path_corner "New Train rot. Speed" (yaw_speed) turning a func_train."""

from etest import Suite

SUITE = Suite('train_keys', 'func_train / func_platrot: custom sounds, sound radius, Fire On targets; path_corner '
              'yaw_speed')
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3


@S.test('func_train: custommovesnd / customstopsnd replace the numbered sounds, soundradius 1 (Huge) is their '
        'attenuation 0.5', doc=DOC,
        covers=('func_train.custommovesnd', 'func_train.customstopsnd', 'func_train.soundradius=1'))
def train_sounds(t):
    t.point('path_corner', targetname='p1', at=(-150, 100, 40), target='p2')
    t.point('path_corner', targetname='p2', at=(-100, 100, 40))
    t.brush('func_train', targetname='tr', size=(32, 32, 32), at=(-150, 100, 40), target='p1', speed='200',
            movesnd='1', stopsnd='1', custommovesnd='doors/doormove3.wav', customstopsnd='buttons/blip1.wav',
            soundradius='1')
    t.wait(0.3)
    t.mark('go')
    t.fire('tr')
    t.wait(1.0)
    t.expect.sound('doors/doormove3.wav', ent='tr', attn=0.5, min=1, after='go')
    t.expect.sound('buttons/blip1.wav', ent='tr', attn=0.5, min=1, after='go')
    t.expect.sound('plats/', ent='tr', count=0, after='go')


RADII = {-2: 2.0, -1: 1.25, 0: 0.8, 2: 0.25, 3: 0.0}


@S.test('func_train soundradius: each other choice is the attenuation of its move sound', doc=DOC,
        covers=tuple('func_train.soundradius=%d' % k for k in RADII))
def train_radius(t):
    for i, k in enumerate(RADII):
        x = -200 + 90 * i
        t.point('path_corner', targetname='a%d' % i, at=(x, -100, 40), target='b%d' % i)
        t.point('path_corner', targetname='b%d' % i, at=(x, 100, 40))
        t.brush('func_train', targetname='tr%d' % i, size=(32, 32, 32), at=(x, -100, 40), target='a%d' % i,
                speed='200', custommovesnd='doors/doormove3.wav', soundradius=str(k))
    t.wait(0.3)
    t.mark('go')
    for i in range(len(RADII)):
        t.fire('tr%d' % i)
    t.wait(0.5)
    for i, attn in enumerate(RADII.values()):
        t.expect.sound('doors/doormove3.wav', ent='tr%d' % i, attn=attn, min=1, after='go')


STATES = {'fireonstart': 0, 'fireonclosing': 1, 'fireonstop': 2, 'fireonclosed': 0, 'fireonopening': 1,
          'fireonopened': 2}
USE = {0: USE_OFF, 1: USE_ON, 2: USE_TOGGLE}


@S.test('func_platrot (named, Toggle): triggered it goes down: start and closing, stop and closed; again: start and '
        'opening, stop and opened; each with its use type; its custom sounds play', doc=DOC,
        covers=('func_platrot',) + tuple('func_platrot.%s' % k for k in STATES) +
        tuple('func_platrot.%s_triggerstate=%d' % (k, v) for k, v in STATES.items()) +
        ('func_platrot.custommovesnd', 'func_platrot.customstopsnd'))
def platrot(t):
    kv = {}
    for k, st in STATES.items():
        kv[k] = k
        kv[k + '_triggerstate'] = str(st)
        t.point('info_target', targetname=k)
    t.brush('func_platrot', targetname='p', size=(96, 96, 16), at=(0, 100, 0), origin=True, height='88', speed='400',
            rotation='45', spawnflags='1', movesnd='1', stopsnd='1', custommovesnd='doors/doormove3.wav',
            customstopsnd='buttons/blip1.wav', names=tuple(STATES), **kv)
    t.wait(0.2)
    t.mark('go')
    t.fire('p')
    t.wait(0.6)
    t.mark('up')
    t.fire('p')
    t.wait(0.6)
    x = t.expect
    x.fired('fireonstart', use=USE[0], count=2, after='go')          # down, then up
    x.fired('fireonclosing', use=USE[1], count=1, after='go', within=(0, 0.05))
    x.fired('fireonstop', use=USE[2], count=2, after='go')
    x.fired('fireonclosed', use=USE[0], count=1, after='go', within=(0.1, 0.5))
    x.fired('fireonopening', use=USE[1], count=1, after='up', within=(0, 0.05))
    x.fired('fireonopened', use=USE[2], count=1, after='up', within=(0.1, 0.5))
    x.sound('doors/doormove3.wav', ent='p', min=2, after='go')
    x.sound('buttons/blip1.wav', ent='p', min=2, after='go')


# ------------------------------------------------------------------------------------------- path_corner yaw_speed
SF_CORNER_WAIT_RETRIGGER = 1


@S.test('path_corner New Train rot. Speed (yaw_speed 30): a func_train leaving it turns at 30 degrees a second while '
        'it moves and stops turning when it stops at a corner (Wait for retrigger); a corner with 0 after it keeps the '
        'rate', doc='SBO-2.0-CHANGES.md', covers=('path_corner.yaw_speed',))
def path_corner_yaw_speed(t):
    t.brush('func_train', size=(32, 32, 32), at=(-150, 0, 64), origin=True, targetname='tr', target='p1', speed='100')
    t.point('path_corner', targetname='p1', at=(-150, 0, 48), target='p2', yaw_speed='30')
    t.point('path_corner', targetname='p2', at=(-50, 0, 48), target='p3', yaw_speed='0')
    t.point('path_corner', targetname='p3', at=(150, 0, 48), spawnflags=str(SF_CORNER_WAIT_RETRIGGER))
    t.wait(0.5)
    t.fire('tr')
    t.wait(0.5)
    t.snap('first', 'tr', 'avelocity', 'angles')
    t.wait(1.5)
    t.snap('second', 'tr', 'avelocity')
    t.wait(2.5)
    t.snap('stopped', 'tr', 'avelocity', 'angles')
    t.wait(1.0)
    t.snap('later', 'tr', 'angles')
    t.expect.value('first', 'tr', 'avelocity.y', 30, tol=0.01)
    t.expect.value('second', 'tr', 'avelocity.y', 30, tol=0.01)
    t.expect.value('stopped', 'tr', 'avelocity.y', 0, tol=0.01)
    t.expect.value('stopped', 'tr', 'angles.y', 0, op='>')
    t.expect.same('stopped', 'later', 'tr', 'angles')


@S.test('path_corner yaw_speed 0 (every map\'s value): the train\'s own avelocity is left as it is, moving and '
        'stopped, as in 1.38; a train without one does not turn', doc='SBO-2.0-CHANGES.md',
        covers=('path_corner.yaw_speed',))
def path_corner_yaw_speed_zero(t):
    t.brush('func_train', size=(32, 32, 32), at=(-150, -100, 64), origin=True, targetname='spin', target='a1',
            speed='100', avelocity='0 10 0')
    t.point('path_corner', targetname='a1', at=(-150, -100, 48), target='a2', yaw_speed='0')
    t.point('path_corner', targetname='a2', at=(-50, -100, 48), spawnflags=str(SF_CORNER_WAIT_RETRIGGER))
    t.brush('func_train', size=(32, 32, 32), at=(-150, 100, 64), origin=True, targetname='still', target='b1',
            speed='100')
    t.point('path_corner', targetname='b1', at=(-150, 100, 48), target='b2')
    t.point('path_corner', targetname='b2', at=(-50, 100, 48), spawnflags=str(SF_CORNER_WAIT_RETRIGGER))
    t.wait(0.5)
    t.fire('spin')
    t.fire('still')
    t.wait(0.5)
    t.snap('moving', 'spin', 'avelocity')
    t.snap('moving', 'still', 'avelocity')
    t.wait(1.5)
    t.snap('stopped', 'spin', 'avelocity')
    t.snap('stopped', 'still', 'avelocity', 'angles')
    t.expect.value('moving', 'spin', 'avelocity.y', 10, tol=0.01)
    t.expect.value('stopped', 'spin', 'avelocity.y', 10, tol=0.01)
    t.expect.value('moving', 'still', 'avelocity', [0, 0, 0], tol=0.01)
    t.expect.value('stopped', 'still', 'angles', [0, 0, 0], tol=0.01)
