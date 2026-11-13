"""Entity parenting on a map where nothing moves with a parent (docs/MOVEWITH.md "Map compatibility"): no entity has a
movewith / parent / parentflags key and there is no trigger_changeparent, so movewith must do nothing at all. A stock
func_door, func_plat and func_train each push a monster that is stuck a hair inside a wall beside it: they are blocked
as on 1.38 and the monster is never moved (movewith frees blockers only for its own chains). With developer 2 every
movewith console line would show, and none may."""

from etest import Suite

SUITE = Suite('movewith_none', 'Entity parenting: a map without movewith keys is untouched')
S = SUITE
S.server_args = ('+developer', '2')
DOC = 'MOVEWITH.md#map-compatibility-scan-of-the-138-maps'
METAL = '666MURMETAL'
STUCK_Y = 16.1      # the monster's hull (16 units wide each side) is 0.1 unit inside the wall that starts at y 32


def stuck_monster(t):
    """A monster_generic whose hull reaches 0.1 unit into a func_wall at its side (y 32 .. 64), so any push it gets
    leaves it inside a solid: the pusher is blocked. Returns the label of its snapshot before the push."""
    t.brush('func_wall', size=(320, 32, 128), at=(0, 48, 0), tex=METAL, targetname='side')
    t.point('monster_generic', targetname='mon', at=(0, -100, 0), model='models/scientist.mdl', spawnflags='16')
    t.wait(0.3)
    t.teleport('mon', (0, STUCK_Y, 0))
    t.wait(0.1)
    t.snap('a', 'mon', 'origin')


def no_nudge(t, mover):
    t.snap('b', 'mon', 'origin')
    t.snap('b', mover, 'origin')
    t.expect.moved('a', 'b', 'mon', (0, 0, 0), tol=0.001)     # never moved: not pushed, not freed
    t.expect.no_console('movewith:')   # every movewith console line starts so


def held(t, mover):
    """The mover stays blocked at the monster (a door with wait -1 and a train with no dmg keep pushing)."""
    t.wait(0.4)
    t.snap('c', mover, 'origin')
    t.expect.moved('b', 'c', mover, (0, 0, 0), tol=1)


@S.test('a stock func_door blocked by a monster stuck in a wall: the monster stays put, movewith is silent', doc=DOC,
        covers=('func_door',))
def door(t):
    stuck_monster(t)
    t.brush('func_door', size=(32, 64, 72), at=(-100, 0, 0), tex=METAL, targetname='d', angles='0 0 0',
            speed='192', lip=str(32 - 150), wait='-1')
    t.fire('d')
    t.wait(1.2)
    no_nudge(t, 'd')
    held(t, 'd')


@S.test('a stock func_plat lowered onto a monster stuck in a wall: the monster stays put, movewith is silent',
        doc=DOC, covers=('func_plat',))
def plat(t):
    stuck_monster(t)
    t.brush('func_plat', targetname='p', size=(64, 64, 8), at=(0, 0, 80), tex=METAL, height='64', spawnflags='1',
            speed='200')
    t.fire('p')
    t.wait(1.2)
    no_nudge(t, 'p')


@S.test('a stock func_train blocked by a monster stuck in a wall: the monster stays put, movewith is silent',
        doc=DOC, covers=('func_train',))
def train(t):
    stuck_monster(t)
    t.point('path_corner', targetname='A', at=(-150, 0, 36), target='B')
    t.point('path_corner', targetname='B', at=(150, 0, 36))
    t.brush('func_train', size=(32, 64, 72), at=(-150, 0, 0), tex=METAL, targetname='tr', target='A', speed='192',
            dmg='0')
    t.fire('tr')
    t.wait(1.5)
    no_nudge(t, 'tr')
    held(t, 'tr')
