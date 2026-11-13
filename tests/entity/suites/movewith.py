"""Entity parenting, server side (branch feat/movewith, docs/MOVEWITH.md): children follow a moving and a turning
parent, a blocked chain holds and goes on when the blocker leaves or dies, trigger_changeparent, a rider carried on a
solid child, links kept through a round restart (last test: it respawns the players)."""

from etest import Suite

SUITE = Suite('movewith', 'Entity parenting, server side (feat/movewith)', requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')     # movewith's console lines (refused links)
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


def blocked_train(t, blocker, frames=0):
    """A func_train (A -> B, 400 units at 100 u/s) whose child arm pins `blocker` against a static wall after 128.
    frames: also snapshots held1_0 .. held1_<frames-1> (each frame from held1 on: the train's origin and velocity, the
    arm's origin)."""
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
    for i in range(frames):
        t.wait_frames(1)
        t.snap('held1_%d' % i, 'tr', 'origin', 'velocity')
        t.snap('held1_%d' % i, 'arm', 'origin')
    t.wait(0.6)
    t.snap('held2', 'tr', 'origin')
    t.snap('held2', 'arm', 'origin')


# The arm touches the blocker when the train has moved 127 (128 once it has pushed the player the last unit to the
# wall). A blocked pusher keeps none of its move for that frame (the engine throws the blocked frame's move away), as
# any stock pusher: a held chain stops up to one frame's travel short of the blocker, and each retry after a hold may
# come closer by as much, so where it stops depends on the frame times. Held, the arm sits exactly at its place on the
# held train whenever the train stands still (a retry frame moves the train first, then the arm is blocked), never
# gets into the blocker, and is never more than one long frame's travel (100 units/s x 0.25 s) short of it.
HELD_CONTACT = 128.5
HELD_FRAME = 100 * 0.25
HELD_FRAMES = 60


def release_check(t):
    t.wait(4.0)
    t.snap('done', 'tr', 'origin')
    t.snap('done', 'arm', 'origin')
    window = ['held1_%d' % i for i in range(HELD_FRAMES)]
    t.expect.still_offset(window, 'arm', 'tr', (0, 0, 0), tol=0.05, min_still=3)     # no gap on the held train
    for label in ['held1', 'held2'] + window:                          # never into the blocker
        t.expect.value(label, 'arm', 'origin.x', HELD_CONTACT, op='<')
    for label in ('held1', 'held2'):                                    # held at the blocker
        t.expect.value(label, 'arm', 'origin.x', HELD_CONTACT - HELD_FRAME, op='>')
    t.expect.value('done', 'tr', 'origin.x', 400, tol=1)           # and goes on to B (400 from A)
    t.expect.value('done', 'arm', 'origin.x', 400, tol=1)          # with its child, in step


@S.test('a blocked chain holds and goes on when the blocker steps away', players=2, doc=DOC)
def hold_leave(t):
    blocked_train(t, 'player1', frames=HELD_FRAMES)
    t.teleport('player1', (-150, -150, 0))
    release_check(t)


@S.test('a blocked chain holds and goes on when the blocker dies', players=2, doc=DOC)
def hold_die(t):
    blocked_train(t, 'player2', frames=HELD_FRAMES)
    t.damage('player2', 5000)
    release_check(t)


@S.test('trigger_changeparent: a child moves to a new parent and keeps its place; empty parent = stays', doc=DOC,
        covers=('trigger_changeparent', 'trigger_changeparent.target', 'trigger_changeparent.m_isznewparent'))
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


@S.test('control: a player standing on a stock func_door moving the same way is carried', players=1, doc=DOC,
        covers=('func_door',))
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


@S.test('name.N: a missing attachment is refused with a console line and the child follows the origin', doc=DOC)
def attachment_missing(t):
    # apache.mdl has 2 attachments, zombie.mdl none: prop.4 and z.2 don't exist; prop.1 does
    t.point('cycler', at=(-100, 0, 0), targetname='prop', model='models/apache.mdl')
    t.point('cycler', at=(100, 0, 0), targetname='z', model='models/zombie.mdl')
    t.point('env_sprite', at=(-100, 60, 40), targetname='ok', model='sprites/glow01.spr', movewith='prop.1')
    t.point('env_sprite', at=(-100, 100, 40), targetname='bad', model='sprites/glow01.spr', movewith='prop.4')
    t.point('env_sprite', at=(100, 60, 40), targetname='none', model='sprites/glow01.spr', movewith='z.2')
    t.wait(0.3)
    t.snap('a', 'bad', 'origin', 'movetype')
    t.snap('a', 'ok', 'movetype')
    t.teleport('prop', (-100, -64, 0))
    t.wait(0.3)
    t.snap('b', 'bad', 'origin')
    t.expect.console('has no attachment 4')
    t.expect.console('has no attachment 2')
    t.expect.no_console('has no attachment 1')
    t.expect.value('a', 'ok', 'movetype', 12)               # MOVETYPE_FOLLOW: on the real attachment
    t.expect.moved('a', 'b', 'bad', (0, -64, 0), tol=1)     # the refused one follows the parent's origin


@S.test('a held chain holds a child mover that a client is watching, even when only its parent is blocked',
        players=1, doc=DOC)
def hold_watched(t):
    # a base (func_door, +x) pins player1 against a wall after about 1.75 s; its child hoist (func_door lowering 80
    # units at 20 u/s, off to the side) is not blocked itself but waits with its chain. player1's client gets an
    # update every frame (test_fullpack), as a real client would.
    t.brush('func_door', size=(32, 64, 96), at=(-200, 0, 0), tex=METAL, targetname='base', angles='0 0 0',
            speed='100', lip=str(32 - 400), wait='-1')
    t.brush('func_door', size=(16, 16, 16), at=(-200, 60, 100), tex=METAL, targetname='hoist', angles='0 -2 0',
            speed='20', lip=str(16 - 80), wait='-1', movewith='base')
    t.brush('func_wall', size=(32, 96, 96), at=(40, 0, 0), targetname='stop')
    t.teleport('player1', (7, 0, 0), yaw=180)
    t.fullpack('player1')
    t.wait(0.3)
    t.fire('base')
    t.fire('hoist')
    t.wait(2.5)
    t.snap('held1', 'hoist', 'origin', 'fullpack_origin:player1', 'fullpack_iuser4:player1')
    t.snap('held1', 'base', 'origin')
    t.snap('held1', 'player1', 'origin')
    t.wait(0.6)
    t.snap('held2', 'hoist', 'origin', 'fullpack_origin:player1')
    t.snap('held2', 'base', 'origin')
    t.snap('held2', 'player1', 'origin')
    t.expect.value('held1', 'hoist', 'fullpack_iuser4:player1', 0, op='>')        # sent on its parent
    # the hoist waits with its chain: its own pose, which is what the client is sent (it used to lower on at 20 u/s,
    # 12 units in 0.6 s, whenever a client was watching). Its server origin is not checked here: the chain retries
    # every 0.1 s, and when the pinned player shifts a little the base moves on that far, carrying the hoist.
    t.expect.moved('held1', 'held2', 'hoist', (0, 0, 0), tol=2, field='fullpack_origin:player1')
    t.expect.moved('held1', 'held2', 'player1', (0, 0, 0), tol=1, flat=True)        # the player stays pinned
    t.expect.moved('held1', 'held2', 'base', (0, 0, 0), tol=1)                         # so the base holds
    # released: it goes on to its end, and what the client is sent never steps faster than its 20 u/s (no jump
    # back from a pose that ran on during the hold, no snap at LinearMoveDone)
    t.teleport('player1', (150, 150, 0))
    labels = []
    for i in range(420):     # past its end (45 units left at 20 u/s)
        t.wait_frames(1)
        labels.append('r%d' % i)
        t.snap(labels[-1], 'hoist', 'fullpack_origin:player1')
    t.wait(3.0)
    t.snap('done', 'hoist', 'origin')
    t.expect.max_rate(labels, 'hoist', 'fullpack_origin:player1', 20 * 1.25)
    t.expect.value('done', 'hoist', 'origin.z', -80, tol=1)


@S.test('a child is placed on its parent exactly when the parent is sent (EF_NODRAW alone, or with another bit)',
        players=1, doc=DOC)
def parent_nodraw_bits(t):
    # The game's AddToFullPack (HL SDK) leaves out an entity whose effects are exactly EF_NODRAW (128); with another
    # bit as well (136 = EF_NODRAW + EF_DIMLIGHT) it is sent. A child is sent on its parent only when the parent is
    # sent too, so that the client has the parent to place it on.
    t.brush('func_wall', size=(32, 32, 32), at=(-150, 0, 0), tex=METAL, targetname='hidden', effects='128')
    t.brush('func_wall', size=(32, 32, 32), at=(-150, 60, 0), tex=METAL, targetname='kid1', movewith='hidden')
    t.brush('func_wall', size=(32, 32, 32), at=(0, 0, 0), tex=METAL, targetname='bits', effects='136')
    t.brush('func_wall', size=(32, 32, 32), at=(0, 60, 0), tex=METAL, targetname='kid2', movewith='bits')
    t.brush('func_wall', size=(32, 32, 32), at=(150, 0, 0), tex=METAL, targetname='shown')
    t.brush('func_wall', size=(32, 32, 32), at=(150, 60, 0), tex=METAL, targetname='kid3', movewith='shown')
    t.wait(0.3)
    for n in ('hidden', 'bits', 'shown'):
        t.snap('a', n, 'fullpack:player1', 'effects')
    for n in ('kid1', 'kid2', 'kid3'):
        t.snap('a', n, 'fullpack_iuser4:player1')
    x = t.expect
    x.value('a', 'hidden', 'fullpack:player1', -1)              # not sent
    x.value('a', 'kid1', 'fullpack_iuser4:player1', 0)          # so its child is sent with its world pose
    x.value('a', 'bits', 'fullpack:player1', -1, op='>')        # sent
    x.value('a', 'kid2', 'fullpack_iuser4:player1', 0, op='>')  # so its child is placed on it
    x.value('a', 'kid3', 'fullpack_iuser4:player1', 0, op='>')


@S.test('a brush child without an origin brush on a turning parent: an entity stuck far away is not pushed and does '
        'not block it', players=1, doc=DOC)
def turning_child_box(t):
    # The stock collision box of a turned brush entity is a cube around its origin as big as its largest bound. For a
    # child without an origin brush the origin is far from its brush, so that cube covered the whole map, and
    # anything stuck in something anywhere in it was shoved by the child's turn (or, when it can't be moved, held the
    # chain with a block). A monster stuck 0.1 unit in a wall stands in the far corner.
    t.brush('func_rotating', size=(64, 64, 16), at=(-100, -100, 40), tex=METAL, origin=True, targetname='rot',
            speed='30', spawnflags='0')
    t.brush('func_wall', size=(16, 16, 32), at=(-100, -40, 56), tex=METAL, targetname='kid', movewith='rot')
    t.brush('func_wall', size=(64, 32, 128), at=(150, 220, 0), tex=METAL, targetname='side')
    t.point('monster_generic', targetname='mon', at=(150, 100, 0), model='models/scientist.mdl', spawnflags='16')
    t.fullpack('player1')
    t.wait(0.3)
    t.teleport('mon', (150, 204 - 16 + 0.1, 0))
    t.teleport('player1', (200, -200, 0))
    t.wait(0.1)
    t.snap('a', 'mon', 'origin')
    t.snap('a', 'kid', 'origin', 'absmin', 'absmax')
    t.snap('a', 'rot', 'origin')
    t.fire('rot')
    t.wait(2.0)
    t.snap('b', 'mon', 'origin')
    t.snap('b', 'kid', 'origin', 'absmin', 'absmax')
    t.snap('b', 'rot', 'origin')
    t.expect.moved('a', 'b', 'mon', (0, 0, 0), tol=0.01)       # never pushed
    # it turned with the parent the whole time (30 deg/s for 2 s: its brush, 60 units out, moved about 60 units)
    t.expect.moved('a', 'b', 'kid', min_dist=20)
    # and its box holds its brush: no bigger than the brush turned (16 x 16 x 32, so under 40 across), not the map
    t.expect.value('b', 'kid', 'absmax.z', t._abs((0, 0, 0))[2] + 100, op='<')


def riders(t, at, deck):
    """A scientist (prisoner: it doesn't react) and a func_pushable standing on `deck`, near `at`."""
    t.point('monster_scientist', targetname='sci', at=(at[0], at[1] - 30, at[2]), spawnflags='16')
    t.brush('func_pushable', size=(32, 32, 32), at=(at[0], at[1] + 30, at[2]), tex=METAL, targetname='box', origin=True)


def rider_checks(t, delta, lo='a', hi='b'):
    for n in ('sci', 'box'):
        t.expect.moved(lo, hi, n, delta, tol=8, flat=True)          # carried, no drift
        t.expect.value(hi, n, 'flags', 512, op='&')                 # FL_ONGROUND: still standing on it
    t.expect.same(lo, hi, 'sci', 'kv:health')                       # not hurt (not stuck, not crushed)


@S.test('a monster and a func_pushable ride a solid child of a moving parent, across two parts of one group',
        players=1, doc=DOC, covers=('func_pushable', 'monster_scientist'))
def riders_child(t):
    mover(t, 'p', (-150, 200, 0))
    t.brush('func_wall', size=(128, 128, 16), at=(-130, 0, 0), tex=METAL, targetname='deck', movewith='p')
    t.brush('func_wall', size=(128, 128, 16), at=(-2, 0, 0), tex=METAL, targetname='deck2', movewith='deck')
    riders(t, (-66, 0, 16), 'deck')     # on the seam between the two parts
    t.wait(1.0)
    for n in ('sci', 'box'):
        t.snap('a', n, 'origin', 'flags', 'kv:health')
    t.fire('p')
    t.wait(1.5)
    for n in ('sci', 'box'):
        t.snap('b', n, 'origin', 'flags', 'kv:health')
    rider_checks(t, (192, 0, 0))
    # no sinking or lag beyond what a stock pusher gives a monster (a stock func_train: 149.1 of 150; the engine takes
    # FL_ONGROUND off a rider that doesn't walk at every push)
    for n in ('sci', 'box'):
        t.expect.moved('a', 'b', n, (192, 0, 0), tol=3)


@S.test('a monster and a func_pushable ride a child (no origin brush) of a turning parent', players=1, doc=DOC,
        covers=('func_pushable', 'monster_scientist'))
def riders_turning(t):
    t.brush('func_rotating', size=(32, 32, 16), at=(0, 0, 0), tex=METAL, origin=True, targetname='rot', speed='45',
            spawnflags='0')
    t.brush('func_wall', size=(256, 256, 16), at=(0, 0, 16), tex=METAL, targetname='disc', movewith='rot')
    riders(t, (90, 0, 32), 'disc')
    t.teleport('player1', (0, 90, 32))
    t.wait(1.0)
    for n in ('sci', 'box', 'player1'):
        t.snap('a', n, 'origin', 'flags', 'kv:health')
    t.snap('a', 'rot', 'origin')
    t.fire('rot')
    t.wait(2.0)          # 90 degrees
    for n in ('sci', 'box', 'player1'):
        t.snap('b', n, 'origin', 'flags', 'kv:health')
    t.snap('b', 'rot', 'origin')
    t.expect.distance('b', 'player1', 'rot', 80, 100)                   # the player went round with it too
    t.expect.moved('a', 'b', 'player1', min_dist=100)
    t.expect.console('disc has no origin brush, so anything it bumps into while it turns is pushed the wrong way')
    for n in ('sci', 'box'):
        t.expect.moved('a', 'b', n, min_dist=60)                     # went round with it
        t.expect.value('b', n, 'flags', 512, op='&')
        t.expect.distance('b', n, 'rot', 50, 130)                     # kept on the disc, not flung off
    t.expect.same('a', 'b', 'sci', 'kv:health')


@S.test('a monster and a func_pushable ride a child with an origin brush that goes round a turning parent',
        players=1, doc=DOC, covers=('func_pushable', 'monster_scientist'))
def riders_orbit(t):
    t.brush('func_rotating', size=(32, 32, 16), at=(-120, 0, 0), tex=METAL, origin=True, targetname='rot',
            speed='45', spawnflags='0')
    t.brush('func_wall', size=(160, 160, 16), at=(40, 0, 16), tex=METAL, origin=True, targetname='disc',
            movewith='rot')
    riders(t, (40, 0, 32), 'disc')
    t.wait(1.0)
    for n in ('sci', 'box'):
        t.snap('a', n, 'origin', 'flags', 'kv:health')
    t.snap('a', 'disc', 'origin')
    t.fire('rot')
    t.wait(2.0)          # 90 degrees round the parent
    for n in ('sci', 'box'):
        t.snap('b', n, 'origin', 'flags', 'kv:health')
    t.snap('b', 'disc', 'origin')
    for n in ('sci', 'box'):
        t.expect.moved('a', 'b', n, min_dist=150)                    # went round with it (radius 160)
        t.expect.value('b', n, 'flags', 512, op='&')
        t.expect.distance('b', n, 'disc', 0, 70)                      # still on the disc
    t.expect.same('a', 'b', 'sci', 'kv:health')
    t.expect.no_console('t15_disc has no origin brush')


@S.test('control: a monster and a func_pushable ride a stock func_train', players=1, doc=DOC,
        covers=('func_train', 'func_pushable', 'monster_scientist'))
def riders_stock(t):
    t.point('path_corner', targetname='A', at=(-150, 0, 8), target='B')
    t.point('path_corner', targetname='B', at=(150, 0, 8))
    t.brush('func_train', size=(192, 160, 16), at=(-150, 0, 0), tex=METAL, targetname='tr', target='A', speed='100',
            dmg='0')
    riders(t, (-150, 0, 16), 'tr')
    t.wait(1.0)
    for n in ('sci', 'box'):
        t.snap('a', n, 'origin', 'flags', 'kv:health')
    t.fire('tr')
    t.wait(1.5)
    for n in ('sci', 'box'):
        t.snap('b', n, 'origin', 'flags', 'kv:health')
    rider_checks(t, (150, 0, 0))


@S.test('a monster or a func_pushable with Move with is refused with a console line and stays put', doc=DOC,
        covers=('func_pushable', 'monster_scientist'))
def refused_movers(t):
    mover(t, 'p', (-150, 200, 0))
    t.point('monster_scientist', targetname='sci', at=(0, 0, 0), spawnflags='16', movewith='p')
    t.brush('func_pushable', size=(32, 32, 32), at=(100, 0, 0), tex=METAL, targetname='box', movewith='p')
    t.wait(0.5)
    t.snap('a', 'sci', 'origin')
    t.snap('a', 'box', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'sci', 'origin')
    t.snap('b', 'box', 'origin')
    t.expect.console('because it moves on its own, but it can ride along by standing on a moving object')
    t.expect.moved('a', 'b', 'sci', (0, 0, 0), tol=1, flat=True)
    t.expect.moved('a', 'b', 'box', (0, 0, 0), tol=1, flat=True)


@S.test('riders on the side of a seesaw that goes down: the chain is never held and they stay on', players=2,
        doc=DOC, covers=('func_pushable', 'monster_scientist'))
def riders_seesaw(t):
    # A func_rotating pitching (turning about the Y axis) carries two origin-brush decks, 100 units each side of its
    # axis along X: one goes
    # down while it turns, the other up. A player and a scientist stand on each. A rider put at its end-of-frame
    # spot before the engine's two-step move (turn, then move) sat inside the down-going deck's half-done pose, the
    # turn was blocked and the chain held every frame: the seesaw froze.
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 0, 60), tex=METAL, origin=True, targetname='rot', speed='15',
            spawnflags='8')
    for side, x in (('n', 100), ('s', -100)):
        t.brush('func_wall', size=(112, 112, 8), at=(x, 0, 72), tex=METAL, origin=True, targetname='deck' + side,
                movewith='rot')
        t.point('monster_scientist', targetname='sci' + side, at=(x, -30, 80), spawnflags='16')
    t.teleport('player1', (100, 30, 80))
    t.teleport('player2', (-100, 30, 80))
    t.wait(1.0)
    t.snap('a', 'rot', 'angles')
    for n in ('scin', 'scis', 'player1', 'player2'):
        t.snap('a', n, 'origin')
    t.fire('rot')
    t.wait(1.0)
    t.snap('b', 'rot', 'angles')
    for n in ('scin', 'scis', 'player1', 'player2', 'deckn', 'decks'):
        t.snap('b', n, 'origin')
    # turned on (15 degrees a second, so the decks moved about 26 units up and down), never held
    t.expect.value('b', 'rot', 'angles.x', 13, op='>')                       # turned on, never held (15 in 1 s)
    for n, d in (('scin', 'deckn'), ('player1', 'deckn'), ('scis', 'decks'), ('player2', 'decks')):
        t.expect.distance('b', n, d, 0, 55, flat=False)                         # (a player starts 50 away)
        t.expect.moved('a', 'b', n, min_dist=15)


@S.test('a child whose parent broke stays where it is through a round restart, even when the name exists again',
        players=1, doc=DOC)
def orphan_round(t):
    # The first entity named "p" is a func_breakable (the parent); a func_wall far off has the same name. Break the
    # parent, restart the round: the child must not be linked to the other "p" and put on it.
    t.brush('func_breakable', size=(32, 32, 32), at=(-150, 0, 0), tex=METAL, targetname='p', health='10', origin=True)
    t.point('info_target', at=(-150, 60, 16), targetname='kid', movewith='p')
    t.brush('func_wall', size=(32, 32, 32), at=(150, -150, 0), tex=METAL, targetname='p', origin=True)
    t.wait(0.3)
    t.damage('p', 5000)
    t.wait(0.3)
    t.snap('a', 'kid', 'origin')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.5)
    t.snap('b', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (0, 0, 0), tol=0.5)


@S.test('a door closing across a player riding a turning deck pushes the player as any rider', players=1, doc=DOC)
def seesaw_door(t):
    # A slowly pitching func_rotating carries an origin-brush deck along X. player1 rides it; a func_door above the
    # deck slides across the player. A carried player stays a normal entity: the door pushes it.
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 0, 60), tex=METAL, origin=True, targetname='rot', speed='4',
            spawnflags='8')
    t.brush('func_wall', size=(112, 160, 8), at=(100, 0, 72), tex=METAL, origin=True, targetname='deck',
            movewith='rot')
    t.teleport('player1', (100, 10, 80))
    t.brush('func_door', size=(48, 16, 40), at=(100, -100, 100), tex=METAL, targetname='door', angles='0 90 0',
            speed='100', lip=str(16 - 160), wait='-1')
    t.wait(1.0)
    t.fire('rot')
    t.wait(0.5)
    t.snap('a', 'player1', 'origin')
    t.fire('door')
    t.wait(2.0)
    t.snap('b', 'player1', 'origin')
    t.snap('b', 'door', 'origin')
    t.expect.value('b', 'player1', 'origin.y', t._abs((0, 0, 0))[1] + 40, op='>')     # pushed along by the door



@S.test('a held chain keeps a stop that came during the hold: the train stays stopped after the blocker leaves',
        players=2, doc=DOC)
def hold_use_off(t):
    blocked_train(t, 'player1')
    t.fire('tr')                     # a func_train's Use while it moves stops it (during the hold)
    t.wait(0.3)
    t.snap('stopped', 'tr', 'origin')
    t.teleport('player1', (-150, -150, 0))
    t.wait(4.0)
    t.snap('done', 'tr', 'origin')
    t.snap('done', 'arm', 'origin')
    t.expect.moved('stopped', 'done', 'tr', (0, 0, 0), tol=12)       # it stayed stopped (it used to drive on to B)


@S.test('a speed changed during a hold takes effect at the next corner: the released train never jumps',
        players=2, doc=DOC)
def hold_speed(t):
    blocked_train(t, 'player1')
    t.kv('tr', 'speed', '50')        # during the hold
    t.teleport('player1', (-150, -150, 0))
    labels = []
    for i in range(300):
        t.wait_frames(1)
        labels.append('r%d' % i)
        t.snap(labels[-1], 'tr', 'origin')
    t.expect.max_rate(labels, 'tr', 'origin', 100 * 1.3)     # at most its speed (100) until the corner, no jump



SND_STOP, SND_CHANGE_VOL = 32, 64
LOOP = 'ambience/drips.wav'


@S.test('a looping sound of a child ambient_generic stops with its parent when the parent is removed (C2)', players=1,
        doc=DOC)
def sound_parent_removed(t):
    t.brush('func_wall', size=(64, 64, 32), at=(0, 150, 0), tex=METAL, targetname='cart')
    t.point('ambient_generic', targetname='siren', at=(0, 150, 48), message=LOOP, health='10', spawnflags='16',
            movewith='cart')
    t.point('trigger_relay', targetname='kill', killtarget='cart')
    t.wait(0.3)
    t.mark('on')
    t.fire('siren')
    t.wait(0.3)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.3)
    t.expect.sound('drips', ent='cart', chan=6, min=1, after='on', within=(0, 0.2))     # it plays on its parent
    t.expect.sound('drips', ent='cart', flags=SND_STOP, count=1, after='kill', within=(0, 0.2))


@S.test('a child ambient_generic whose parent is hidden stops on the entity its sound started on (C2)', players=1,
        doc=DOC)
def sound_parent_hidden(t):
    t.brush('func_wall_toggle', size=(64, 64, 32), at=(0, 150, 0), tex=METAL, targetname='hid')
    t.point('ambient_generic', targetname='amb', at=(0, 150, 48), message=LOOP, health='10', spawnflags='16',
            movewith='hid')
    t.wait(0.3)
    t.fire('amb')                       # on: plays on the parent
    t.wait(0.3)
    t.fire('hid')                       # the parent is hidden (EF_NODRAW: no longer sent)
    t.wait(0.3)
    t.mark('off')
    t.fire('amb')                       # off
    t.wait(0.3)
    t.expect.sound('drips', ent='hid', flags=SND_STOP, count=1, after='off', within=(0, 0.2))
    t.expect.sound('drips', ent='amb', flags=SND_STOP, count=0)     # not where the ambient stands


@S.test('two child ambient_generics playing the same sound on one parent use two entities, so one stops alone (R10)',
        players=1, doc=DOC)
def sound_shared_channel(t):
    t.brush('func_wall', size=(64, 64, 32), at=(0, 150, 0), tex=METAL, targetname='cart')
    t.brush('func_wall', size=(16, 16, 32), at=(150, 150, 0), tex=METAL, targetname='panel', movewith='cart')
    for name in ('a1', 'a2'):
        t.point('ambient_generic', targetname=name, at=(0, 150, 48), message=LOOP, health='10', spawnflags='16',
                movewith='cart')
    t.wait(0.3)
    t.mark('on')
    t.fire('a1')
    t.wait(0.2)
    t.fire('a2')
    t.wait(0.3)
    t.expect.sound('drips', ent='cart', count=1, after='on')
    t.expect.sound('drips', ent='panel', count=1, after='on')


@S.test('a child ambient_random playing a loop is caught up when its parent comes into a player\'s view (C2)',
        players=1, doc=DOC)
def sound_random_catch_up(t):
    t.brush('func_wall', size=(64, 64, 32), at=(0, 150, 0), tex=METAL, targetname='cart')
    t.point('ambient_random', targetname='ar', at=(0, 150, 48), message=LOOP, health='10', spawnflags='16',
            movewith='cart')
    t.wait(0.3)
    t.fire('ar')
    t.wait(0.3)
    t.mark('seen')
    t.fullpack('player1')               # a real client's update from here on: the cart comes into its view
    t.wait(1.5)
    t.fullpack()
    t.expect.sound('drips', ent='cart', flags=SND_CHANGE_VOL, min=1, after='seen')


@S.test('env_modeltrain and env_spritetrain with Move with are refused with a console line and stay put (C5)', doc=DOC,
        covers=('env_modeltrain', 'env_spritetrain'))
def refused_trains(t):
    mover(t, 'p', (-150, 200, 0))
    t.point('env_modeltrain', targetname='mt', at=(0, 0, 32), model='models/robot.mdl', movewith='p')
    t.point('env_spritetrain', targetname='st', at=(100, 0, 32), model='sprites/glow01.spr', movewith='p')
    t.wait(0.5)
    t.snap('a', 'mt', 'origin')
    t.snap('a', 'st', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'mt', 'origin')
    t.snap('b', 'st', 'origin')
    t.expect.console("can't move with %s, because it moves along its own path or axis, but it can be a parent"
                     % t.name('p'))
    t.expect.moved('a', 'b', 'mt', (0, 0, 0), tol=1)
    t.expect.moved('a', 'b', 'st', (0, 0, 0), tol=1)


TANK_KEYS = dict(angles='0 90 0', yawrate='60', yawrange='30', pitchrate='30', pitchrange='30', barrel='24',
                 firerate='1', yawtolerance='15', pitchtolerance='5', persistence='1', bullet='1', bullet_damage='1')


@S.test('func_tank "extra_barrels" with Move with: the extra barrel, a child too, takes the aim of the controlled tank '
        '(C5)', players=2, size=(512, 512, 256), doc=DOC, covers=('func_tank.extra_barrels',))
def tank_barrels(t):
    t.brush('func_wall', size=(64, 64, 16), at=(0, -120, 0), tex=METAL, targetname='mount')
    t.brush('func_tank', size=(32, 32, 32), at=(0, -120, 40), origin=True, targetname='tk', spawnflags='32',
            extra_barrels='tkx', movewith='mount', names=('extra_barrels',), **TANK_KEYS)
    t.brush('func_tank', size=(32, 32, 32), at=(0, -120, 100), origin=True, targetname='tkx', spawnflags='0',
            movewith='mount', **TANK_KEYS)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.look('player1', 0, 110)           # 20 degrees left of the tanks' centre (90)
    t.wait(1.0)
    t.fire('tk', activator='player1')   # CFuncTank::Use with a player: StartControl
    t.wait(2.5)                         # turned to 110 at 60 degrees/s, then still
    for i in range(5):                  # the extra barrel stays at the tank's yaw (the steering turned it back to 90)
        t.wait_frames(1)
        t.snap('a%d' % i, 'tk', 'angles')
        t.snap('a%d' % i, 'tkx', 'angles')
        t.expect.value('a%d' % i, 'tk', 'angles.y', 110, tol=1)
        t.expect.value('a%d' % i, 'tkx', 'angles.y', 110, tol=1)


@S.test('a momentary_door carried by a moving parent is Off at rest (its own movement, not its parent\'s) (C6)',
        players=1, doc=DOC, covers=('momentary_door',))
def momentary_child_state(t):
    mover(t, 'p', (-150, 200, 0))
    t.brush('momentary_door', size=(32, 16, 64), at=(-150, 150, 0), tex=METAL, targetname='md', angles='-90 0 0',
            lip='8', movewith='p')
    t.wait(0.3)
    t.snap('rest', 'md', 'ent_state')
    t.fire('p')
    t.wait(0.4)                         # the parent moves for 1 s
    t.snap('carried', 'md', 'origin', 'ent_state')
    t.snap('carried', 'p', 'origin')
    t.expect.value('rest', 'md', 'ent_state', 'Off')
    t.expect.value('carried', 'md', 'ent_state', 'Off')
    t.expect.value('carried', 'p', 'origin.x', 20, op='>')  # it was moving


@S.test('a momentary_rot_button on a moving parent turns on it while held, and drives a momentary_door on the same '
        'parent while both ride', players=1, doc=DOC, covers=('momentary_rot_button', 'momentary_door'))
def wheel_on_parent(t):
    mover(t, 'p', (-150, 200, 0), distance=192, speed=48)                  # slides +x for 4 s
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(-150, 150, 24), tex=METAL, origin=True, targetname='wheel',
            target='md', distance='90', speed='45', spawnflags='0', movewith='p')
    t.brush('momentary_door', size=(32, 8, 48), at=(-150, 100, 0), tex=METAL, targetname='md', angles='-90 0 0',
            lip='8', speed='100', movewith='p')
    t.wait(0.3)
    for e in ('wheel', 'md', 'p'):
        t.snap('a', e, 'origin', 'angles')
    t.fire('p')
    t.wait(0.3)
    for i in range(30):                 # a player holding Use on the wheel for 1.5 s
        t.fire('wheel', activator='player1')
        t.wait(0.05)
    t.wait(0.3)
    for e in ('wheel', 'md', 'p'):
        t.snap('b', e, 'origin', 'angles')
    # the parent slides (48 u/s) while the wheel is held; how far depends on the frame length (the 30 presses)
    for e in ('p', 'wheel', 'md'):                                          # the wheel and the door rode with it
        t.expect.moved('a', 'b', e, min_dist=50)
    t.expect.value('b', 'wheel', 'angles.y', 40, op='>')                   # and turned on it while held
    t.expect.value('b', 'md', 'origin.z', 8, op='>')                        # and the wheel drove it open (up)


@S.test('a held chain keeps its thinks: the train\'s arrival and a riding door\'s opening, due during the hold, each '
        'come once after the release, after the time they had left', players=2, doc=DOC,
        covers=('func_door.fireonopened',))
def hold_thinks(t):
    # the arm meets the blocker at 127; corner B is 40 further: the train's arrival is due while it is held (and more
    # than one long frame's travel past the blocker, as the train moves before the arm is blocked in a frame)
    t.point('path_corner', targetname='A', at=(-200, 150, 16), target='B')
    t.point('path_corner', targetname='B', at=(-33, 150, 16), message='atB', names=('message',))
    t.brush('func_train', size=(32, 32, 32), at=(-200, 150, 0), tex=METAL, targetname='tr', target='A', speed='100',
            dmg='0')
    t.brush('func_wall', size=(32, 64, 32), at=(-152, 150, 0), tex=METAL, targetname='arm', movewith='tr')
    t.brush('func_wall', size=(32, 96, 96), at=(40, 150, 0), targetname='stop')
    # a door riding the train, opening up 32 at 16 u/s: both arrivals (B at 1.67 s, the door at 2 s) are due during
    # the hold, which starts at 1.27 s
    t.brush('func_door', size=(16, 16, 48), at=(-200, 200, 0), tex=METAL, targetname='dr', angles='-90 0 0', speed='16',
            lip='16', wait='-1', fireonopened='drOpen', movewith='tr', names=('fireonopened',))
    t.point('info_target', targetname='atB')
    t.point('info_target', targetname='drOpen')
    t.teleport('player1', (7, 150, 0), yaw=180)
    t.wait(0.3)
    t.fire('tr')
    t.fire('dr')
    t.wait(2.5)                         # blocked after about 1.27 s; both thinks were due before now
    t.mark('held')
    t.snap('held', 'tr', 'origin')
    t.snap('held', 'dr', 'origin')
    t.teleport('player1', (-150, -150, 0))
    t.mark('released')
    t.wait(2.0)
    t.expect.fired('atB', count=1)                                          # once in all, never early:
    t.expect.fired('drOpen', count=1)
    t.expect.fired('atB', count=1, after='released', within=(0.2, 0.75))    # the ~40 units (0.4 s) it had left
    t.expect.fired('drOpen', count=1, after='released', within=(0.4, 1.2))  # the 0.73 s it had left


@S.test('a monster riding a func_tracktrain (the root of a chain) keeps its spot on the car through a curve',
        players=1, doc=DOC, covers=('func_tracktrain', 'monster_scientist'))
def riders_tracktrain_curve(t):
    # an L-shaped track: east 200, then north 200 (a 90 degree curve at p2)
    t.point('path_track', at=(-200, -100, 32), targetname='p1', target='p2')
    t.point('path_track', at=(0, -100, 32), targetname='p2', target='p3')
    t.point('path_track', at=(0, 150, 32), targetname='p3')
    t.brush('func_tracktrain', size=(128, 96, 16), at=(-200, -100, 24), origin=True, targetname='car', target='p1',
            speed='80', height='0', wheels='64', bank='0', spawnflags='2')
    t.point('env_sprite', at=(-200, -100, 96), model='sprites/glow01.spr', rendermode='5', renderamt='200',
            scale='0.2', spawnflags='1', movewith='car')        # a child: the car is the root of a chain
    t.point('monster_scientist', targetname='sci', at=(-160, -100, 40), spawnflags='16')    # 40 ahead of the centre
    t.teleport('player1', (150, 150, 0))                         # not aboard
    t.wait(1.5)
    t.snap('a', 'sci', 'origin', 'flags', 'kv:health')
    t.snap('a', 'car', 'origin', 'angles')
    t.fire('car', use='on')
    labels = []
    for i in range(8):
        t.wait(0.6)
        labels.append('c%d' % i)
        t.snap(labels[-1], 'sci', 'origin', 'flags')
        t.snap(labels[-1], 'car', 'origin', 'angles')
    t.expect.distance('a', 'sci', 'car', 36, 44)
    for lab in labels:                                           # through and after the curve: the same spot
        t.expect.distance(lab, 'sci', 'car', 34, 46)
    t.expect.value(labels[-1], 'car', 'angles.y', 45, op='>')    # the car went round the curve
    t.expect.value(labels[-1], 'sci', 'flags', 512, op='&')      # and the scientist still stands on it


@S.test('a player at the controls of a func_tank riding a func_tracktrain through a curve, aiming behind the gun: the '
        'gun turns back through its yaw range, never through the arc behind it, where its barrel would sweep him off '
        'his spot', players=1, size=(512, 512, 256), doc=DOC, covers=('func_tank', 'func_tank.yawrange'))
def tank_rider_curve(t):
    # the boat in co_movewith_test: west, then a right turn north. The car is built facing west (-x), as every
    # func_tracktrain is; the gun 60 ahead of its centre, a 20 cube base and a 28 long barrel, built facing east and
    # turned to the bow (yaw 180) with 120 degrees of yaw range each way; the player stands 16 behind the centre
    t.point('path_track', at=(180, -180, 32), targetname='p1', target='p2')
    t.point('path_track', at=(-180, -180, 32), targetname='p2', target='p3')
    t.point('path_track', at=(-180, 200, 32), targetname='p3')
    t.brush('func_tracktrain', size=(192, 96, 16), at=(180, -180, 24), origin=True, targetname='car', target='p1',
            speed='120', height='0', wheels='96', bank='0', spawnflags='2', dmg='0')
    t.brush('func_tank', size=(20, 20, 20), at=(120, -194, 48), origin=(120, -194, 58),
            parts=(((28, 6, 6), (144, -194, 55)),), targetname='tk', angles='0 180 0', movewith='car',
            spawnflags='32', yawrate='120', yawrange='120', pitchrate='80', pitchrange='30', barrel='28',
            firerate='1', yawtolerance='15', pitchtolerance='5', persistence='1', bullet='1', bullet_damage='1')
    t.teleport('player1', (164, -180, 40), yaw=180)
    t.wait(1.0)
    t.snap('a', 'player1', 'origin')
    t.snap('a', 'car', 'origin')
    t.fire('tk', activator='player1')   # CFuncTank::Use with a player: StartControl
    t.fire('car', use='on')
    labels = []
    for i in range(12):                 # behind the gun's left (the car still goes west), then ahead and behind again
        t.look('player1', 0, (0, 60, 300, 0)[i % 4])
        t.wait(0.5)
        labels.append('c%d' % i)
        t.snap(labels[-1], 'player1', 'origin')
        t.snap(labels[-1], 'car', 'origin', 'angles')
    t.expect.distance('a', 'player1', 'car', 15, 17)
    for lab in labels:                  # he stays on his spot behind the gun
        t.expect.distance(lab, 'player1', 'car', 14, 18)
    t.expect.value(labels[-1], 'car', 'angles.y', 280, op='<')   # the car went round the curve (360 -> 270)



@S.test('a func_tank with Move with let go of while it turns stops turning (the engine moves a pusher only up to its '
        'think time: no think, no own movement)', players=2, size=(512, 512, 256), doc=DOC, covers=('func_tank',))
def tank_released_stops(t):
    t.brush('func_wall', size=(64, 64, 16), at=(0, -120, 0), tex=METAL, targetname='mount')
    t.brush('func_tank', size=(32, 32, 32), at=(0, -120, 40), origin=True, targetname='tk', spawnflags='32',
            movewith='mount', **dict(TANK_KEYS, yawrange='180'))
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.look('player1', 0, 270)           # behind the tank: it turns at its 60 degrees/s
    t.wait(1.0)
    t.fire('tk', activator='player1')   # CFuncTank::Use with a player: StartControl
    t.wait(1.0)
    t.fire('tk', activator='player1', use='off')    # StopControl mid-turn
    t.wait(0.3)
    t.snap('a', 'tk', 'angles')
    t.wait(2.0)
    t.snap('b', 'tk', 'angles')
    t.expect.console('stopped using TANK')
    t.expect.moved('a', 'b', 'tk', (0, 0, 0), tol=0.5, field='angles')


@S.test('a func_tankcontrols without an origin brush riding a parent that has turned 90 degrees is where it is drawn: '
        'a player there takes the gun with Use (its box went round the far world origin\'s arc and Use missed it)',
        players=1, size=(512, 512, 256), doc=DOC, covers=('func_tankcontrols', 'func_door_rotating'))
def tankcontrols_turned(t):
    # the parent turns +90 about its origin at the room's centre: (100, 0) goes to (0, 100)
    t.brush('func_door_rotating', size=(32, 32, 16), at=(0, 0, 0), origin=True, tex=METAL, targetname='turn',
            speed='90', distance='90', wait='-1', spawnflags='0')
    t.brush('func_tank', size=(24, 24, 24), at=(100, 0, 40), origin=True, targetname='tk', spawnflags='32',
            movewith='turn', **TANK_KEYS)
    t.brush('func_tankcontrols', size=(16, 48, 48), at=(140, 0, 16), tex='AAATRIGGER', target='tk', movewith='turn')
    t.point('info_target', targetname='mark', at=(140, 0, 40), movewith='turn')     # the controls' centre
    t.teleport('player1', (-200, -200, 0))
    t.wait(0.5)
    t.fire('turn')
    t.wait(1.8)
    t.snap('a', 'mark', 'origin')
    t.snap('a', 'turn', 'angles')
    t.teleport('player1', (0, 176, 0), yaw=270)      # 36 beyond the controls, facing them
    t.look('player1', 0, 270)
    t.wait(0.3)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.value('a', 'turn', 'angles.y', 90, tol=1)
    t.expect.console('using TANK!')


@S.test('a player caught inside a riding gun (standing too close behind it on the boat) never locks the chain: he is '
        'moved out to a clear spot, the car goes on, and he can let go of the gun', players=1, size=(512, 512, 256),
        doc=DOC, covers=('func_tracktrain', 'func_tank', 'func_tankcontrols'))
def rider_caught_in_gun(t):
    t.point('path_track', at=(180, -180, 32), targetname='p1', target='p2')
    t.point('path_track', at=(-180, -180, 32), targetname='p2', target='p3')
    t.point('path_track', at=(-180, 200, 32), targetname='p3')
    t.brush('func_tracktrain', size=(192, 96, 16), at=(180, -180, 24), origin=True, targetname='car', target='p1',
            speed='120', height='0', wheels='96', bank='0', spawnflags='2', dmg='0')
    t.brush('func_tank', size=(20, 20, 20), at=(120, -194, 48), origin=(120, -194, 58),
            parts=(((28, 6, 6), (144, -194, 55)),), targetname='tk', angles='0 180 0', movewith='car',
            spawnflags='32', yawrate='120', yawrange='120', pitchrate='80', pitchrange='30', barrel='28',
            firerate='1', yawtolerance='15', pitchtolerance='5', persistence='1', bullet='1', bullet_damage='1')
    t.brush('func_tankcontrols', size=(8, 52, 44), at=(136, -180, 40), tex='AAATRIGGER', target='tk', movewith='car')
    t.teleport('player1', (141, -180, 40), yaw=180)     # his box 9 units into the gun's base
    t.wait(1.0)
    t.fire('tk', activator='player1')
    t.snap('a', 'car', 'origin')
    t.fire('car', use='on')
    t.wait(2.0)
    t.snap('b', 'car', 'origin')
    t.move('player1', 0, 0, 3, '+use')                  # lets go of the gun
    t.wait(0.5)
    t.expect.moved('a', 'b', 'car', min_dist=150)       # 120 u/s: not held
    t.expect.console('it rides the chain')
    t.expect.console('stopped using TANK')
