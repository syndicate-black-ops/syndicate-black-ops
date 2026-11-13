"""Tier 5, batch 7: mover FGD items no 1.38 map uses and no other test asserted: the Fire On use type choices the suite
fireon does not set (each key gets every choice here: the fireon mapping shifted by one and by two), func_plat /
func_platrot Move and Stop Sound choices, func_platrot speed, height, rotation and axis flags."""

from etest import Suite

SUITE = Suite('fgd_movers', 'Tier 5: movers, Fire On use types, plat sounds, func_platrot keys')
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
USE = {0: 0, 1: 1, 2: 3}                    # choice -> use type (Toggle is 3)
BASE = {'fireonstart': 0, 'fireonopening': 1, 'fireonstop': 2, 'fireonopened': 0, 'fireonclosing': 1,
        'fireonclosed': 2}                  # suite fireon's choices


def states(shift):
    return {k: (v + shift) % 3 for k, v in BASE.items()}


def fire_keys(t, st):
    kv = {}
    for k, v in st.items():
        kv[k] = k
        kv[k + '_triggerstate'] = str(v)
        t.point('info_target', targetname=k)
    return kv


def covers(cls, st):
    return tuple('%s.%s_triggerstate=%d' % (cls, k, v) for k, v in st.items())


def expect_cycle(t, st, open_t, wait):
    x = t.expect
    x.fired('fireonstart', use=USE[st['fireonstart']], count=2, after='go')
    x.fired('fireonopening', use=USE[st['fireonopening']], count=1, after='go', within=(0, 0.05))
    x.fired('fireonstop', use=USE[st['fireonstop']], count=2, after='go')
    x.fired('fireonopened', use=USE[st['fireonopened']], count=1, after='go', within=(open_t - 0.1, open_t + 0.15))
    x.fired('fireonclosing', use=USE[st['fireonclosing']], count=1, after='go',
            within=(open_t + wait - 0.1, open_t + wait + 0.2))
    x.fired('fireonclosed', use=USE[st['fireonclosed']], count=1, after='go',
            within=(2 * open_t + wait - 0.1, 2 * open_t + wait + 0.3))


def door_case(cls, shift):
    st = states(shift)

    @S.test('%s Fire On use types shifted by %d: every target with its use type over an open / close cycle' % (
            cls, shift), doc=DOC, covers=covers(cls, st))
    def case(t):
        kv = fire_keys(t, st)
        if cls == 'func_door':
            t.brush(cls, targetname='d', size=(96, 16, 96), at=(0, 100, 0), tex='666MURMETAL', angles='-90 0 0',
                    speed='400', lip='8', wait='0.5', names=tuple(st), **kv)
        elif cls == 'func_door_rotating':
            t.brush(cls, targetname='d', size=(64, 8, 96), at=(0, 100, 0), tex='666MURMETAL', origin=True,
                    speed='400', distance='88', wait='0.5', names=tuple(st), **kv)
        else:
            t.brush(cls, targetname='d', skin='-3', size=(96, 96, 96), at=(0, 100, 0), angles='-90 0 0',
                    speed='400', lip='8', wait='0.5', names=tuple(st), **kv)
        t.mark('go')
        t.fire('d')
        t.wait(1.5)
        expect_cycle(t, st, 0.22, 0.5)
    case.__name__ = '%s_fireon_%d' % (cls.replace('func_', ''), shift)
    return case


for _cls in ('func_door', 'func_door_rotating', 'func_water'):
    for _shift in (1, 2):
        door_case(_cls, _shift)


def plat_case(cls, shift):
    st = states(shift)

    @S.test('%s (Toggle) Fire On use types shifted by %d: down, then up' % (cls, shift), doc=DOC,
            covers=covers(cls, st) + ('%s:1' % cls,))
    def case(t):
        kv = fire_keys(t, st)
        extra = {'rotation': '90'} if cls == 'func_platrot' else {}
        t.brush(cls, targetname='p', size=(96, 96, 16), at=(0, 100, 0), tex='666MURMETAL', height='88',
                spawnflags='1', speed='400' if cls == 'func_plat' else '200', origin=cls == 'func_platrot',
                names=tuple(st), **dict(kv, **extra))
        t.wait(0.2)
        t.mark('go')
        t.fire('p')
        t.wait(0.8)
        t.mark('up')
        t.fire('p')
        t.wait(0.8)
        x = t.expect
        x.fired('fireonstart', use=USE[st['fireonstart']], min=2, after='go')
        x.fired('fireonclosing', use=USE[st['fireonclosing']], count=1, after='go', within=(0, 0.05))
        x.fired('fireonstop', use=USE[st['fireonstop']], min=2, after='go')
        x.fired('fireonclosed', use=USE[st['fireonclosed']], count=1, after='go', within=(0.1, 0.7))
        x.fired('fireonopening', use=USE[st['fireonopening']], count=1, after='up', within=(0, 0.05))
        x.fired('fireonopened', use=USE[st['fireonopened']], count=1, after='up', within=(0.1, 0.7))
    case.__name__ = '%s_fireon_%d' % (cls.replace('func_', ''), shift)
    return case


for _cls in ('func_plat', 'func_platrot'):
    for _shift in (0, 1, 2):
        if _cls == 'func_plat' and _shift == 0:
            continue                         # suite fireon
        plat_case(_cls, _shift)


# ---------------------------------------------------------------------------------------------- plat sounds
MOVE = {1: 'plats/bigmove1.wav', 2: 'plats/bigmove2.wav', 3: 'plats/elevmove1.wav', 4: 'plats/elevmove2.wav',
        5: 'plats/elevmove3.wav', 6: 'plats/freightmove1.wav', 7: 'plats/freightmove2.wav',
        8: 'plats/heavymove1.wav', 9: 'plats/rackmove1.wav', 10: 'plats/railmove1.wav',
        11: 'plats/squeekmove1.wav', 12: 'plats/talkmove1.wav', 13: 'plats/talkmove2.wav'}
STOP = {1: 'plats/bigstop1.wav', 2: 'plats/bigstop2.wav', 3: 'plats/freightstop1.wav', 4: 'plats/heavystop2.wav',
        5: 'plats/rackstop1.wav', 6: 'plats/railstop1.wav', 7: 'plats/squeekstop1.wav', 8: 'plats/talkstop1.wav'}


def sound_case(cls, half):
    moves = [m for m in MOVE if (m <= 7) == (half == 0)]

    @S.test('%s Move Sound %s and Stop Sound: each plat plays its sounds when it moves and stops; 0: none' % (
            cls, ', '.join(str(m) for m in moves)), doc=DOC, size=(512, 512, 256),
            covers=('%s.movesnd' % cls, '%s.stopsnd' % cls)
            + tuple('%s.movesnd=%d' % (cls, m) for m in moves)
            + tuple('%s.stopsnd=%d' % (cls, (m - 1) % 8 + 1) for m in moves)
            + (('%s.movesnd=0' % cls, '%s.stopsnd=0' % cls) if half == 0 else ()))
    def case(t):
        plats = [(m, (m - 1) % 8 + 1) for m in moves] + ([(0, 0)] if half == 0 else [])
        for i, (m, s) in enumerate(plats):
            x, y = -200 + (i % 4) * 130, -150 + (i // 4) * 200
            t.brush(cls, targetname='p%d' % i, size=(48, 48, 16), at=(x, y, 0), tex='666MURMETAL', height='64',
                    spawnflags='1', speed='300', movesnd=str(m), stopsnd=str(s), origin=cls == 'func_platrot')
        t.wait(0.2)
        t.mark('go')
        for i in range(len(plats)):
            t.fire('p%d' % i)
        t.wait(1.0)
        for i, (m, s) in enumerate(plats):
            if m:
                t.expect.sound(MOVE[m], ent='p%d' % i, after='go', min=1)
                t.expect.sound(STOP[s], ent='p%d' % i, after='go', min=1)
            else:
                t.expect.sound('plats/', ent='p%d' % i, after='go', count=0)
    case.__name__ = '%s_sounds_%d' % (cls.replace('func_', ''), half)
    return case


for _cls in ('func_plat', 'func_platrot'):
    for _half in (0, 1):
        sound_case(_cls, _half)


# ---------------------------------------------------------------------------------------------- func_platrot keys
@S.test('func_platrot Spin amount 90 at Speed of rotation 90, Travel altitude 64: going down it turns 90 degrees '
        'about Z (90 at the top, 0 at the bottom); X Axis (64) and Y Axis (128) turn about those axes instead', doc=DOC, size=(512, 512, 256),
        covers=('func_platrot.rotation', 'func_platrot.speed', 'func_platrot.height', 'func_platrot:64',
                'func_platrot:128'))
def platrot_axes(t):
    for n, x, fl in (('pz', -150, '1'), ('px', 0, str(1 | 64)), ('py', 150, str(1 | 128))):
        t.brush('func_platrot', targetname=n, size=(64, 64, 16), at=(x, 0, 0), tex='666MURMETAL', height='64',
                spawnflags=fl, speed='90', rotation='90', origin=True)
    t.wait(0.2)
    for n in ('pz', 'px', 'py'):
        t.snap('a', n, 'angles', 'origin')
        t.fire(n)
    t.wait(2.5)
    for n in ('pz', 'px', 'py'):
        t.snap('b', n, 'angles', 'origin')
    t.expect.moved('a', 'b', 'pz', delta=[0, 0, -64], tol=2)
    t.expect.value('a', 'pz', 'angles', [0, 90, 0], tol=2)
    t.expect.value('a', 'px', 'angles', [0, 0, 90], tol=2)
    t.expect.value('a', 'py', 'angles', [90, 0, 0], tol=2)
    for n in ('pz', 'px', 'py'):
        t.expect.value('b', n, 'angles', [0, 0, 0], tol=2)
