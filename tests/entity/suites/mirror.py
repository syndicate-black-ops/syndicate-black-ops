"""env_mirror (docs/ENV_MIRROR.md "Keys", sdk/syndicate.fgd): the "Mirror" state message decoded field by field: on /
off, Draw Player, the axis (given or the brush's thinnest side), the reflecting side and its plane, the brush box,
the radius; the brush's render mode while on and off; sent at a HUD re-initialization (fullupdate, only active
mirrors) and to everybody when toggled. The reflections themselves need the client test layer."""

from etest import Suite

SUITE = Suite('mirror', 'env_mirror: mirror state (server side)', players=1)
S = SUITE
DOC = 'ENV_MIRROR.md#keys'


def box(t, size, at=(0, 0, 0)):
    """the mirror brush's faces (mins, maxs) as the message carries them"""
    x, y, z = t._abs(at)
    return [x - size[0] / 2, y - size[1] / 2, z], [x + size[0] / 2, y + size[1] / 2, z + size[2]]


@S.test('a wall (thin in X), keys left out: on at the start (fullupdate), axis X (automatic), both sides, the plane '
        'in the middle, radius 330, no players; the brush keeps its render mode', players=1, doc=DOC,
        covers=('env_mirror', 'env_mirror.radius', 'env_mirror.frags=0', 'env_mirror.side=0',
                'env_mirror.rendermode'))
def defaults(t):
    t.brush('env_mirror', size=(8, 128, 128), at=(100, 0, 0), rendermode='2', renderamt='100')
    lo, hi = box(t, (8, 128, 128), (100, 0, 0))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.snap('s', '@env_mirror', 'rendermode')
    t.expect.msg('Mirror', to='player1', count=1, after='fu',
                 fields={'on': 1, 'players': 0, 'axis': 0, 'side': 0, 'mins': lo, 'maxs': hi,
                         'plane': (lo[0] + hi[0]) / 2, 'radius': 330})
    t.expect.value('s', '@env_mirror', 'rendermode', 2)


@S.test('automatic axis: thin in Y gives Y (1), thin in Z gives Z (2, a floor: the top face reflects for views '
        'above); radius 0 gives 330', players=1, doc=DOC,
        covers=('env_mirror.frags=0', 'env_mirror.radius'))
def auto_axis(t):
    t.brush('env_mirror', size=(128, 8, 128), at=(-150, 100, 0), radius='0')
    t.brush('env_mirror', size=(128, 128, 8), at=(100, -100, 0))
    loy, hiy = box(t, (128, 8, 128), (-150, 100, 0))
    loz, hiz = box(t, (128, 128, 8), (100, -100, 0))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.msg('Mirror', to='player1', count=1, after='fu',
                 fields={'on': 1, 'axis': 1, 'side': 0, 'mins': loy, 'plane': (loy[1] + hiy[1]) / 2, 'radius': 330})
    t.expect.msg('Mirror', to='player1', count=1, after='fu',
                 fields={'on': 1, 'axis': 2, 'side': 1, 'mins': loz, 'plane': hiz[2]})


@S.test('Mirror axis given (1 on a brush thin in X; 2 Z) and Reflecting side 1 (positive: the max face) / 2 '
        '(negative: the min face); radius sent; flag 1 Draw Player', players=1, doc=DOC,
        covers=('env_mirror.frags=1', 'env_mirror.frags=2', 'env_mirror.side=1', 'env_mirror.side=2',
                'env_mirror.radius', 'env_mirror:1'))
def axis_side(t):
    t.brush('env_mirror', size=(8, 128, 128), at=(-150, 0, 0), frags='1', side='1', radius='500', spawnflags='1')
    t.brush('env_mirror', size=(64, 64, 16), at=(100, 0, 0), frags='2', side='2')
    lo1, hi1 = box(t, (8, 128, 128), (-150, 0, 0))
    lo2, hi2 = box(t, (64, 64, 16), (100, 0, 0))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.msg('Mirror', to='player1', count=1, after='fu',
                 fields={'on': 1, 'players': 1, 'axis': 1, 'side': 1, 'mins': lo1, 'plane': hi1[1], 'radius': 500})
    t.expect.msg('Mirror', to='player1', count=1, after='fu',
                 fields={'on': 1, 'players': 0, 'axis': 2, 'side': -1, 'mins': lo2, 'plane': lo2[2]})


@S.test('flag 2 Start Off: not sent at a fullupdate, the brush drawn Normal; a trigger turns it on for everybody with '
        'the render mode back; triggered again: off (on 0) and Normal', players=1, doc=DOC,
        covers=('env_mirror:2', 'env_mirror.targetname', 'env_mirror.rendermode'))
def start_off_toggle(t):
    t.brush('env_mirror', targetname='m', size=(8, 128, 128), at=(0, 0, 0), spawnflags='2', rendermode='5',
            renderamt='80')
    lo, hi = box(t, (8, 128, 128))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.snap('a', 'm', 'rendermode')
    t.mark('on')
    t.fire('m')
    t.wait(0.2)
    t.snap('b', 'm', 'rendermode')
    t.mark('off')
    t.fire('m')
    t.wait(0.2)
    t.snap('c', 'm', 'rendermode')
    t.expect.msg('Mirror', to='player1', count=0, after='fu', within=(0, 0.45), fields={'mins': lo})
    t.expect.value('a', 'm', 'rendermode', 0)
    t.expect.msg('Mirror', count=1, after='on', within=(0, 0.1), fields={'on': 1, 'axis': 0, 'mins': lo, 'maxs': hi})
    t.expect.value('b', 'm', 'rendermode', 5)
    t.expect.msg('Mirror', count=1, after='off', within=(0, 0.1), fields={'on': 0})
    t.expect.value('c', 'm', 'rendermode', 0)
