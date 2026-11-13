"""The Fire On keys (Featureful) of the movers (sdk/syndicate.fgd bases Door, PlatSounds, MomentaryDoor; docs
FEATUREFUL_OPTIONS.md): fireonstart / fireonstop / fireonopening / fireonopened / fireonclosing / fireonclosed, each with
its use type choice (0 Off, 1 On, 2 Toggle), fired at the moment the FGD names, on func_door, func_door_rotating,
func_water and func_plat."""

from etest import Suite

SUITE = Suite('fireon', 'Movers: Fire On targets and their use types')
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3
USE = {0: USE_OFF, 1: USE_ON, 2: USE_TOGGLE}
# key -> its use type in the tests: every choice on two keys
STATES = {'fireonstart': 0, 'fireonopening': 1, 'fireonstop': 2, 'fireonopened': 0, 'fireonclosing': 1,
          'fireonclosed': 2}


def fire_keys(t):
    kv = {}
    for k, st in STATES.items():
        kv[k] = k
        kv[k + '_triggerstate'] = str(st)
        t.point('info_target', targetname=k)
    return kv


def covers(cls):
    return (cls,) + tuple('%s.%s' % (cls, k) for k in STATES) + \
        tuple('%s.%s_triggerstate=%d' % (cls, k, st) for k, st in STATES.items())


def expect_cycle(t, open_t, wait):
    """open at the mark 'go' (moves for open_t seconds), stays open `wait` seconds, closes (open_t again)"""
    x = t.expect
    x.fired('fireonstart', use=USE[0], count=2, after='go')
    x.fired('fireonopening', use=USE[1], count=1, after='go', within=(0, 0.05))
    x.fired('fireonstop', use=USE[2], count=2, after='go')
    x.fired('fireonopened', use=USE[0], count=1, after='go', within=(open_t - 0.1, open_t + 0.15))
    x.fired('fireonclosing', use=USE[1], count=1, after='go', within=(open_t + wait - 0.1, open_t + wait + 0.2))
    x.fired('fireonclosed', use=USE[2], count=1, after='go', within=(2 * open_t + wait - 0.1, 2 * open_t + wait + 0.3))


@S.test('func_door: start and opening when it starts to open, stop and opened at the top, start and closing when it '
        'starts to close, stop and closed at the bottom, each with its use type', doc=DOC, covers=covers('func_door'))
def door(t):
    kv = fire_keys(t)
    t.brush('func_door', targetname='d', size=(96, 16, 96), at=(0, 100, 0), tex='666MURMETAL', angles='-90 0 0',
            speed='400', lip='8', wait='0.5', names=tuple(STATES), **kv)
    t.mark('go')
    t.fire('d')
    t.wait(1.5)
    expect_cycle(t, 0.22, 0.5)


@S.test('func_door_rotating: the same six Fire On targets and use types over a 90 degree turn', doc=DOC,
        covers=covers('func_door_rotating'))
def door_rotating(t):
    kv = fire_keys(t)
    t.brush('func_door_rotating', targetname='d', size=(64, 8, 96), at=(0, 100, 0), tex='666MURMETAL', origin=True,
            speed='400', distance='88', wait='0.5', names=tuple(STATES), **kv)
    t.mark('go')
    t.fire('d')
    t.wait(1.5)
    expect_cycle(t, 0.22, 0.5)


@S.test('func_water: the same six Fire On targets and use types (a door of water)', doc=DOC, covers=covers('func_water'))
def water(t):
    kv = fire_keys(t)
    t.brush('func_water', targetname='d', skin='-3', size=(96, 96, 96), at=(0, 100, 0), angles='-90 0 0',
            speed='400', lip='8', wait='0.5', names=tuple(STATES), **kv)
    t.mark('go')
    t.fire('d')
    t.wait(1.5)
    expect_cycle(t, 0.22, 0.5)


@S.test('func_plat (named, flag 1 Toggle: starts at the top): triggered it goes down: start and closing, then stop and closed at the '
        'bottom; triggered again: start and opening, stop and opened', doc=DOC, covers=covers('func_plat'))
def plat(t):
    kv = fire_keys(t)
    t.brush('func_plat', targetname='p', size=(96, 96, 16), at=(0, 100, 0), tex='666MURMETAL', height='88', spawnflags='1',
            speed='400', names=tuple(STATES), **kv)
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
    x.fired('fireonclosed', use=USE[2], count=1, after='go', within=(0.1, 0.5))
    x.fired('fireonopening', use=USE[1], count=1, after='up', within=(0, 0.05))
    x.fired('fireonopened', use=USE[0], count=1, after='up', within=(0.1, 0.5))


@S.test('func_train: Fire On Start when it starts to move, Fire On Stop where it stops (Off), each with its use type',
        doc=DOC, covers=('func_train', 'func_train.fireonstart', 'func_train.fireonstop',
                         'func_train.fireonstart_triggerstate=1', 'func_train.fireonstop_triggerstate=2'))
def train(t):
    t.point('info_target', targetname='st')
    t.point('info_target', targetname='sp')
    t.point('path_corner', targetname='p1', at=(-150, 100, 40), target='p2')
    t.point('path_corner', targetname='p2', at=(150, 100, 40), target='p1')
    t.brush('func_train', targetname='tr', size=(32, 32, 32), at=(-150, 100, 40), target='p1', speed='100',
            fireonstart='st', fireonstart_triggerstate='1', fireonstop='sp', fireonstop_triggerstate='2',
            names=('fireonstart', 'fireonstop'))
    t.wait(0.3)
    t.mark('go')
    t.fire('tr')
    t.wait(0.5)
    t.mark('stop')
    t.fire('tr')
    t.wait(0.3)
    t.expect.fired('st', use=USE_ON, min=1, after='go', within=(0, 0.1))
    t.expect.fired('sp', use=USE_TOGGLE, count=1, after='stop', within=(0, 0.1))


@S.test('scripted_sequence: Fire on Possession when it takes its monster, Fire after moving when the monster reaches '
        'the mark (Walk), Fire on Animation start when the action animation starts; activator the script', doc=DOC,
        covers=('scripted_sequence.m_iszfireonpossessed', 'scripted_sequence.m_iszfireonbegin',
                'scripted_sequence.m_iszfireonanimstart'))
def scripted(t):
    for n in ('poss', 'begin', 'anim'):
        t.point('info_target', targetname=n)
    t.point('monster_scientist', targetname='sci', at=(-100, 100, 0), angles='0 0 0')
    t.point('scripted_sequence', targetname='ss', at=(0, 100, 0), m_iszEntity='sci', m_iszPlay='console', m_fMoveTo='1',
            m_iszFireOnPossessed='poss', m_iszFireOnBegin='begin', m_iszFireOnAnimStart='anim',
            names=('m_iszEntity', 'm_iszFireOnPossessed', 'm_iszFireOnBegin', 'm_iszFireOnAnimStart'))
    t.wait(0.5)
    t.mark('go')
    t.fire('ss')
    t.wait(4.0)
    t.expect.fired('poss', activator='ss', count=1, after='go', within=(0, 0.3))
    t.expect.fired('begin', activator='ss', count=1, after='go')
    t.expect.fired('anim', activator='ss', count=1, after='go')
    t.expect.order('begin', 'anim')
    t.expect.order('poss', 'begin')
