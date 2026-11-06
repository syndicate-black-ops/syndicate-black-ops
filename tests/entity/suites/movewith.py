"""Entity parenting, server side (branch feat/movewith, docs/MOVEWITH.md): children follow a moving and a turning
parent, a blocked chain holds and goes on when the blocker leaves or dies, trigger_changeparent, a rider carried on a
solid child, links kept through a round restart (last test: it respawns the players)."""

from etest import Suite

SUITE = Suite('movewith', 'Entity parenting, server side (feat/movewith)', requires=('MoveWith_StartFrame',))
S = SUITE
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


@S.test('children of every kind follow a moving parent: solid brush, trigger, point entity, solid model', doc=DOC)
def follow(t):
    mover(t, 'p', (-150, 150, 0))
    t.brush('func_wall', at=(-150, 100, 0), targetname='cw', movewith='p')
    t.trigger('trigger_multiple', size=(32, 32, 32), at=(-150, 50, 0), targetname='ct', movewith='p')
    t.point('info_target', at=(-150, 0, 16), targetname='ci', movewith='p')
    t.point('cycler', at=(-150, -60, 0), targetname='cc', model='models/zombie.mdl', movewith='p')
    names = ('p', 'cw', 'ct', 'ci', 'cc')
    t.wait(0.2)
    for n in names:
        t.snap('a', n, 'origin')
    t.fire('p')
    t.wait(1.5)
    for n in names:
        t.snap('b', n, 'origin')
    for n in names:
        t.expect.moved('a', 'b', n, (192, 0, 0), tol=2)


@S.test('a child orbits a turning parent at its distance', doc=DOC)
def orbit(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 0, 0), tex=METAL, origin=True, targetname='rot', speed='90',
            spawnflags='0')
    t.point('info_target', at=(96, 0, 16), targetname='ci', movewith='rot')
    t.snap('a', 'ci', 'origin')
    t.fire('rot')
    t.wait(1.0)
    t.snap('b', 'ci', 'origin')
    t.snap('b', 'rot', 'origin')
    t.expect.distance('b', 'ci', 'rot', 94, 98)                       # kept its radius
    t.expect.value('b', 'ci', 'origin.x', t._abs((96, 0, 0))[0] - 40, op='<')   # and went round (90 deg/s)


def blocked_train(t, blocker):
    """A func_train (A -> B, 400 units at 100 u/s) whose child arm pins `blocker` against a static wall after 128."""
    t.point('path_corner', targetname='A', at=(-200, 150, 16), target='B')
    t.point('path_corner', targetname='B', at=(200, 150, 16))
    t.brush('func_train', size=(32, 32, 32), at=(-200, 150, 0), tex=METAL, targetname='tr', target='A', speed='100',
            dmg='0')
    t.brush('func_wall', size=(32, 64, 32), at=(-152, 150, 0), tex=METAL, targetname='arm', movewith='tr')
    t.brush('func_wall', size=(32, 96, 96), at=(40, 150, 0), targetname='stop')
    t.teleport(blocker, (7, 150, 0), yaw=180)
    t.wait(0.3)
    t.fire('tr')
    t.wait(2.2)          # blocked after about 1.3 s
    t.snap('held1', 'tr', 'origin')
    t.snap('held1', 'arm', 'origin')
    t.wait(0.6)
    t.snap('held2', 'tr', 'origin')
    t.snap('held2', 'arm', 'origin')


def release_check(t):
    t.wait(4.0)
    t.snap('done', 'tr', 'origin')
    t.snap('done', 'arm', 'origin')
    # the root may be seen up to a frame's push off its held pose: snapshots run at the start of StartFrame, before
    # MoveWith_StartFrame puts it back (seen: -9.9 once in 6 runs); its child stays exactly
    t.expect.moved('held1', 'held2', 'tr', (0, 0, 0), tol=12)       # the chain holds
    t.expect.moved('held1', 'held2', 'arm', (0, 0, 0), tol=3)
    t.expect.moved('held2', 'done', 'tr', (272, 0, 0), tol=10)      # and goes on to B
    t.expect.moved('held2', 'done', 'arm', (272, 0, 0), tol=10)     # with its child, in step


@S.test('a blocked chain holds and goes on when the blocker steps away', players=2, doc=DOC)
def hold_leave(t):
    blocked_train(t, 'player1')
    t.teleport('player1', (-150, -150, 0))
    release_check(t)


@S.test('a blocked chain holds and goes on when the blocker dies', players=2, doc=DOC)
def hold_die(t):
    blocked_train(t, 'player2')
    t.damage('player2', 5000)
    release_check(t)


@S.test('trigger_changeparent: a child moves to a new parent and keeps its place; empty parent = stays', doc=DOC)
def changeparent(t):
    mover(t, 'pa', (-150, 150, 0))
    mover(t, 'pb', (-150, -150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='pa')
    t.point('info_target', at=(-150, -100, 16), targetname='free', movewith='pb')
    t.point('trigger_changeparent', targetname='swap', target='kid', m_iszNewParent='pb')
    t.point('trigger_changeparent', targetname='drop', target='free')
    t.fire('swap')
    t.fire('drop')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.snap('a', 'free', 'origin')
    t.fire('pb')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.snap('b', 'free', 'origin')
    t.fire('pa')
    t.wait(1.5)
    t.snap('c', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)       # follows its new parent
    t.expect.moved('a', 'b', 'free', (0, 0, 0), tol=0.5)      # no parent: stays
    t.expect.moved('b', 'c', 'kid', (0, 0, 0), tol=0.5)       # the old parent moves it no more


@S.test('a player standing on a solid child is carried with it', players=1, doc=DOC)
def rider(t):
    mover(t, 'p', (-150, 200, 0))
    t.brush('func_wall', size=(128, 128, 16), at=(-130, 0, 0), tex=METAL, targetname='deck', movewith='p')
    t.teleport('player1', (-130, 0, 16))
    t.wait(0.3)
    t.snap('a', 'player1', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'player1', 'origin')
    t.expect.moved('a', 'b', 'player1', (192, 0, 0), tol=6, flat=True)


@S.test('control: a player standing on a stock func_door moving the same way is carried', players=1, doc=DOC)
def rider_stock(t):
    mover(t, 'deck', (-130, 0, 0), size=(128, 128, 16))
    t.teleport('player1', (-130, 0, 16))
    t.wait(0.3)
    t.snap('a', 'player1', 'origin')
    t.fire('deck')
    t.wait(1.5)
    t.snap('b', 'player1', 'origin')
    t.expect.moved('a', 'b', 'player1', (192, 0, 0), tol=6, flat=True)


@S.test('round restart: links kept, a child still follows its parent afterwards', players=1, doc=DOC)
def round_restart(t):
    mover(t, 'p', (-150, 150, 0))
    t.brush('func_wall', at=(-150, 100, 0), targetname='cw', movewith='p')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.5)
    t.snap('a', 'cw', 'origin')
    t.snap('a', 'p', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'cw', 'origin')
    t.snap('b', 'p', 'origin')
    t.expect.moved('a', 'b', 'p', (192, 0, 0), tol=2)
    t.expect.moved('a', 'b', 'cw', (192, 0, 0), tol=2)
