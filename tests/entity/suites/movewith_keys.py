"""Entity parenting, keys changed while the map runs (docs/MOVEWITH.md): trigger_changevalue on "movewith", "parent"
and "parentflags" re-links the entity the way trigger_changeparent does; "origin", "angles" and "velocity" written on
a child set its own pose and movement on its parent."""

from etest import Suite

SUITE = Suite('movewith_keys', 'Entity parenting: run-time keys (feat/movewith)', requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


def changevalue(t, name, target, key, value):
    """A trigger_changevalue (Replace); a value that names an entity must be given as t.name(...)."""
    t.point('trigger_changevalue', targetname=name, target=target, m_iszValueName=key, m_iszNewValue=value)


@S.test('trigger_changevalue "movewith" links an entity to a parent; it follows it from where it is', doc=DOC)
def key_movewith_links(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid')
    t.brush('func_wall', at=(-150, 40, 0), tex=METAL, targetname='wall')
    changevalue(t, 'cv1', 'kid', 'movewith', t.name('p'))
    changevalue(t, 'cv2', 'wall', 'parent', t.name('p'))
    t.wait(0.2)
    t.fire('cv1')
    t.fire('cv2')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.snap('a', 'wall', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.snap('b', 'wall', 'origin')
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)
    t.expect.moved('a', 'b', 'wall', (192, 0, 0), tol=2)


@S.test('trigger_changevalue "movewith" to another name moves the child to that parent; empty: no parent', doc=DOC)
def key_movewith_change(t):
    mover(t, 'pa', (-150, 150, 0))
    mover(t, 'pb', (-150, -150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='pa')
    t.point('info_target', at=(-150, 50, 16), targetname='free', movewith='pa')
    changevalue(t, 'cv1', 'kid', 'movewith', t.name('pb'))
    changevalue(t, 'cv2', 'free', 'movewith', '')
    t.wait(0.2)
    t.fire('cv1')
    t.fire('cv2')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.snap('a', 'free', 'origin')
    t.fire('pa')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.snap('b', 'free', 'origin')
    t.fire('pb')
    t.wait(1.5)
    t.snap('c', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (0, 0, 0), tol=0.5)       # the old parent moves it no more
    t.expect.moved('a', 'b', 'free', (0, 0, 0), tol=0.5)      # no parent: stays
    t.expect.moved('b', 'c', 'kid', (192, 0, 0), tol=2)       # follows the new one


@S.test('trigger_changevalue "parentflags" on a child: the frozen axis keeps its world value from then on', doc=DOC)
def key_parentflags(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    changevalue(t, 'cv', 'kid', 'parentflags', '2')     # PARENT_FROZEN_POS_X
    t.wait(0.2)
    t.fire('cv')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (0, 0, 0), tol=0.5)


@S.test('trigger_changevalue "movewith" naming nothing: a console line, the link stays', doc=DOC)
def key_movewith_missing(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    changevalue(t, 'cv', 'kid', 'movewith', 'nosuchthing')
    t.wait(0.2)
    t.fire('cv')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.expect.console('no entity named "nosuchthing"')
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)


@S.test('trigger_changevalue "origin" on a child: it goes there and keeps that place on its parent', doc=DOC)
def key_origin(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    t.brush('func_wall', at=(-150, 40, 0), tex=METAL, targetname='wall', movewith='p')
    kid_to = t._abs((-100, 60, 16))
    wall_to = (50, -40, 0)      # a brush without an origin brush: its origin is the map's origin, moved
    changevalue(t, 'cv1', 'kid', 'origin', '%g %g %g' % tuple(kid_to))
    changevalue(t, 'cv2', 'wall', 'origin', '%g %g %g' % tuple(wall_to))
    t.wait(0.2)
    t.snap('o', 'kid', 'origin')
    t.snap('o', 'wall', 'origin')
    t.fire('cv1')
    t.fire('cv2')
    t.wait(0.2)
    t.snap('a', 'kid', 'origin')
    t.snap('a', 'wall', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.snap('b', 'wall', 'origin')
    t.expect.moved('o', 'a', 'kid', (50, -40, 0), tol=0.5)    # there, and not put back by its parent
    t.expect.moved('o', 'a', 'wall', (50, -40, 0), tol=0.5)
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)       # and carried from there
    t.expect.moved('a', 'b', 'wall', (192, 0, 0), tol=2)


@S.test('trigger_changevalue "velocity" on a child: its own movement, on top of its moving parent', doc=DOC)
def key_velocity(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    changevalue(t, 'cv', 'kid', 'velocity', '0 -40 0')
    changevalue(t, 'stop', 'kid', 'velocity', '0 0 0')
    t.wait(0.2)
    t.snap('a', 'kid', 'origin')
    t.fire('cv')
    t.fire('p')
    t.wait(1.0)
    t.fire('stop')
    t.wait(1.0)
    t.snap('b', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (192, -40, 0), tol=4)


@S.test('trigger_changevalue "angles" on a child: it turns to them and keeps that turn on its parent', doc=DOC)
def key_angles(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 0, 0), tex=METAL, origin=True, targetname='rot', speed='90',
            spawnflags='0')
    t.point('env_sprite', at=(96, 0, 16), targetname='kid', model='sprites/glow01.spr', movewith='rot', angles='0 0 0',
            spawnflags='8')
    changevalue(t, 'cv', 'kid', 'angles', '0 45 0')
    t.wait(0.2)
    t.fire('cv')
    t.wait(0.1)
    t.snap('a', 'kid', 'angles')
    t.fire('rot')
    t.wait(1.0)
    t.snap('b', 'kid', 'angles')
    t.snap('b', 'rot', 'angles')
    t.expect.value('a', 'kid', 'angles.y', 45, tol=0.5)
    # it keeps its 45 degrees on the parent while that turns (90 deg/s for about 1 s)
    t.expect.value('b', 'kid', 'angles.y', 45 + 70, op='>')


@S.test('"parentflags" on a child moved by trigger_changeparent keeps its new parent', doc=DOC)
def key_parentflags_keeps_link(t):
    mover(t, 'pa', (-150, 150, 0))
    mover(t, 'pb', (-150, -150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='pa')
    t.point('info_target', at=(-150, -100, 16), targetname='loose')
    t.point('trigger_changeparent', targetname='swap', target='kid', m_iszNewParent='pb')
    t.point('trigger_changeparent', targetname='take', target='loose', m_iszNewParent='pb')
    changevalue(t, 'cv1', 'kid', 'parentflags', '8')       # PARENT_FROZEN_POS_Z: no effect on a move in x
    changevalue(t, 'cv2', 'loose', 'parentflags', '8')
    t.wait(0.2)
    t.fire('swap')
    t.fire('take')
    t.wait(0.1)
    t.fire('cv1')
    t.fire('cv2')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.snap('a', 'loose', 'origin')
    t.fire('pb')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.snap('b', 'loose', 'origin')
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)       # still on pb, not back on pa
    t.expect.moved('a', 'b', 'loose', (192, 0, 0), tol=2)     # a child with no key stays a child


@S.test('"movewith" set empty on a child the map gave "parent": no parent', doc=DOC)
def key_movewith_empty_over_parent(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', parent='p')
    changevalue(t, 'cv', 'kid', 'movewith', '')
    t.wait(0.2)
    t.fire('cv')
    t.wait(0.1)
    t.snap('a', 'kid', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.expect.moved('a', 'b', 'kid', (0, 0, 0), tol=0.5)


@S.test('"parentflags" changed while a child door moves: the door keeps moving to its end', doc=DOC)
def key_parentflags_moving_child(t):
    t.brush('func_wall', size=(32, 32, 32), at=(-150, 150, 0), tex=METAL, targetname='p')
    t.brush('func_door', size=(16, 16, 16), at=(-150, 60, 0), tex=METAL, targetname='cd', angles='0 90 0',
            speed='48', lip=str(16 - 96), wait='-1', movewith='p')
    changevalue(t, 'cv', 'cd', 'parentflags', '8')
    t.wait(0.2)
    t.snap('a', 'cd', 'origin')
    t.fire('cd')
    t.wait(0.5)
    t.fire('cv')
    t.wait(0.05)
    t.snap('m1', 'cd', 'origin')
    t.wait(0.5)
    t.snap('m2', 'cd', 'origin')
    t.wait(3.0)
    t.snap('b', 'cd', 'origin')
    t.expect.moved('m1', 'm2', 'cd', (0, 24, 0), tol=4)    # still moving at its 48 u/s after the change
    t.expect.moved('a', 'b', 'cd', (0, 96, 0), tol=1)
