"""Tier 5, final pass: Sound Volume on func_trackchange, and Sound Volume, Travel altitude and Move speed on
func_trackautochange (plats.cpp CFuncTrackChange: it moves "height" down from where it is placed at "speed").
A func_trackautochange moves when its train drives onto it (etest.gen_movers.train_lane)."""

from etest import Suite, gen_movers

SUITE = Suite('fgd_trackchange_more', 'Tier 5: track change Sound Volume, Travel altitude, Move speed', players=1)
S = SUITE
DOC = 'FGD Trackchange'
MOVE1 = 'plats/bigmove1.wav'


def train(t):
    """the train func_trackchange names, far from it on a track of its own"""
    t.point('path_track', targetname='far1', target='far2', at=(-220, -220, 0))
    t.point('path_track', targetname='far2', at=(-120, -220, 0))
    t.brush('func_tracktrain', targetname='tr', size=(32, 32, 16), at=(-220, -220, 0), target='far1', wheels='8',
            height='0', speed='0')


@S.test('func_trackchange "Sound Volume" 0.3: its move sound plays at volume 0.3 (left out: 0.85)', doc=DOC,
        players=1, covers=('func_trackchange.volume',))
def trackchange_volume(t):
    train(t)
    for name, x, kv in (('qv', -60, {'volume': '0.3'}), ('dv', 60, {})):
        t.point('path_track', targetname=name + '_top', at=(x, 60, 72))
        t.point('path_track', targetname=name + '_bot', at=(x, 60, 8))
        t.brush('func_trackchange', targetname=name, size=(48, 48, 16), at=(x, 60, 64), tex='666MURMETAL',
                origin=True, toptrack=name + '_top', bottomtrack=name + '_bot', train='tr', speed='200',
                height='64', movesnd='1', stopsnd='1', **kv)
    t.wait(0.5)
    t.mark('go')
    t.fire('qv', activator='player1')
    t.fire('dv', activator='player1')
    t.wait(1.5)
    t.expect.sound(MOVE1, ent='qv', vol=(0.28, 0.32), after='go', min=1)
    t.expect.sound(MOVE1, ent='dv', vol=(0.83, 0.87), after='go', min=1)


@S.test('func_trackautochange "Sound Volume" 0.4: its move sound plays at volume 0.4 when its train drives onto it',
        doc=DOC, players=1, covers=('func_trackautochange.volume',))
def autochange_volume(t):
    gen_movers.train_lane(t, 'av', 0, movesnd='1', stopsnd='1', volume='0.4')
    t.wait(0.3)
    t.mark('go')
    t.fire('av_tr', use='on')
    t.wait(2.5)
    t.expect.sound(MOVE1, ent='av', vol=(0.38, 0.42), after='go', min=1)


@S.test('func_trackautochange "Travel altitude" 40 and "Move/Rotate speed" 50: when its train drives onto it, it '
        'goes 40 down at 50 units a second', doc=DOC, players=1,
        covers=('func_trackautochange.height', 'func_trackautochange.speed'))
def autochange_height_speed(t):
    gen_movers.train_lane(t, 'ah', 0, height='40', speed='50')
    t.wait(0.3)
    t.snap('pre', 'ah', 'origin')
    t.mark('go')
    t.fire('ah_tr', use='on')
    t.wait_for_fire('ah', 4.0)      # the dead end of its path uses it (the train is there)
    t.wait(0.4)
    t.snap('mid', 'ah', 'velocity')
    t.wait(2.0)
    t.snap('end', 'ah', 'origin')
    t.expect.value('mid', 'ah', 'velocity', [0, 0, -50], tol=1)
    t.expect.moved('pre', 'end', 'ah', delta=(0, 0, -40), tol=1)


@S.test('func_trackchange Rotate Only (16): each turn plays the move sound and fires Fire On Start and Close Start '
        '(going down) or Open Start (going up), then the stop sound and Close End / Open End', doc=DOC, players=1,
        covers=('func_trackchange:16', 'func_trackchange.fireonstart', 'func_trackchange.fireonclosing',
                'func_trackchange.fireonopening'))
def trackchange_rotate_fireon(t):
    train(t)
    names = ('fireonstart', 'fireonclosing', 'fireonopening', 'fireonclosed', 'fireonopened')
    for n in names:
        t.point('info_target', targetname=n)
    t.point('path_track', targetname='ro_top', at=(0, 60, 72))
    t.point('path_track', targetname='ro_bot', at=(0, 60, 8))
    t.brush('func_trackchange', targetname='ro', size=(48, 48, 16), at=(0, 60, 64), tex='666MURMETAL',
            origin=True, toptrack='ro_top', bottomtrack='ro_bot', train='tr', speed='180', rotation='90',
            spawnflags='16', movesnd='1', stopsnd='1', names=names, **{n: n for n in names})
    t.wait(0.5)
    t.mark('go')
    t.fire('ro', activator='player1')
    t.wait(1.5)
    t.mark('up')
    t.fire('ro', activator='player1')
    t.wait(1.5)
    x = t.expect
    x.fired('fireonstart', count=1, after='go', within=(0, 0.3))
    x.fired('fireonstart', count=1, after='up', within=(0, 0.3))
    x.fired('fireonclosing', count=1, after='go', within=(0, 0.3))
    x.fired('fireonclosed', count=1, after='go', within=(0, 1.2))
    x.fired('fireonopening', count=1, after='up', within=(0, 0.3))
    x.fired('fireonopened', count=1, after='up', within=(0, 1.2))
    x.sound(MOVE1, ent='ro', after='go', min=2)
    x.sound('plats/bigstop1.wav', ent='ro', after='go', min=2)
