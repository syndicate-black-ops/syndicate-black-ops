"""Entity parenting with trigger_motion, motion_manager and motion_thread (docs/MOVEWITH.md): on a child they act on
its own pose, as Spirit of Half-Life's do. A position or angles given to a child are where it goes now, and it keeps
that place on its parent from then on; a velocity or angular velocity is its own movement on top of its parent's."""

from etest import Suite

SUITE = Suite('movewith_motion', 'Entity parenting: trigger_motion / motion_manager on a child (feat/movewith)',
              requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


@S.test('trigger_motion set position on a child: it goes there and is carried from there by its parent', doc=DOC)
def motion_set_position(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    t.point('info_target', at=(-100, 60, 16), targetname='spot')
    t.point('trigger_motion', targetname='tm', target='kid', m_iszPosition='spot', m_iPosMode='0',
            names=('m_iszPosition',))
    t.wait(0.2)
    t.snap('o', 'kid', 'origin')
    t.fire('tm')
    t.wait(0.2)
    t.snap('a', 'kid', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'kid', 'origin')
    t.expect.moved('o', 'a', 'kid', (50, -40, 0), tol=0.5)    # there, and not put back by its parent
    t.expect.moved('a', 'b', 'kid', (192, 0, 0), tol=2)       # carried from there


@S.test('trigger_motion add offset on a solid child: moved by the offset, still carried', doc=DOC)
def motion_add_offset(t):
    mover(t, 'p', (-150, 150, 0))
    t.brush('func_wall', at=(-150, 40, 0), tex=METAL, targetname='wall', movewith='p')
    t.point('trigger_motion', targetname='tm', target='wall', m_iszPosition='0 -30 0', m_iPosMode='1')
    t.wait(0.2)
    t.snap('o', 'wall', 'origin')
    t.fire('tm')
    t.wait(0.2)
    t.snap('a', 'wall', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'wall', 'origin')
    t.expect.moved('o', 'a', 'wall', (0, -30, 0), tol=0.5)
    t.expect.moved('a', 'b', 'wall', (192, 0, 0), tol=2)


@S.test('trigger_motion velocity on a child: its own movement on top of its parent\'s; Add reads its own', doc=DOC)
def motion_velocity(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='p')
    t.point('trigger_motion', targetname='go', target='kid', m_iszVelocity='0 -20 0', m_iVelMode='0')
    t.point('trigger_motion', targetname='more', target='kid', m_iszVelocity='0 -20 0', m_iVelMode='1')
    t.point('trigger_motion', targetname='stop', target='kid', m_iszVelocity='0 0 0', m_iVelMode='0')
    t.wait(0.2)
    t.snap('a', 'kid', 'origin')
    t.fire('p')
    t.fire('go')
    t.fire('more')
    t.wait(0.05)
    t.snap('v', 'kid', 'kv:velocity')
    t.wait(0.95)
    t.fire('stop')
    t.wait(1.0)
    t.snap('b', 'kid', 'origin')
    t.expect.value('v', 'kid', 'kv:velocity', [0, -40, 0], tol=0.5)    # its own velocity, not the parent's
    t.expect.moved('a', 'b', 'kid', (192, -40, 0), tol=4)


@S.test('motion_manager rotate by values on a child of a turning parent: it turns on its parent', doc=DOC)
def motion_manager_turn(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 0, 0), tex=METAL, origin=True, targetname='rot', speed='90',
            spawnflags='0')
    t.point('env_sprite', at=(96, 0, 16), targetname='kid', model='sprites/glow01.spr', movewith='rot',
            angles='0 0 0', spawnflags='8')
    t.point('motion_manager', targetname='mm', target='kid', m_iszFacing='0 30 0', m_iFaceMode='2')
    t.wait(0.2)
    t.fire('mm', use='on')
    t.wait(1.0)
    t.snap('a', 'kid', 'angles')
    t.fire('mm', use='off')
    t.fire('rot')
    t.wait(1.0)
    t.snap('b', 'kid', 'angles', 'origin')
    t.snap('b', 'rot', 'origin')
    t.expect.value('a', 'kid', 'angles.y', 25, op='>')        # its own turn (30 deg/s), kept on the parent
    t.expect.value('b', 'kid', 'angles.y', 25 + 70, op='>')   # and the parent's turn on top
    t.expect.distance('b', 'kid', 'rot', 94, 98)              # still at its place on the parent


@S.test('control: trigger_motion on an entity without a parent sets its own fields as before', doc=DOC)
def motion_unparented(t):
    t.brush('func_wall', at=(-150, 40, 0), tex=METAL, targetname='wall')
    t.point('trigger_motion', targetname='tm', target='wall', m_iszPosition='0 -30 0', m_iPosMode='1',
            m_iszAVelocity='0 10 0', m_iAVelMode='0')
    t.wait(0.2)
    t.snap('o', 'wall', 'origin')
    t.fire('tm')
    t.wait(0.05)
    t.snap('a', 'wall', 'origin', 'avelocity')
    t.expect.moved('o', 'a', 'wall', (0, -30, 0), tol=0.5)
    t.expect.value('a', 'wall', 'avelocity', [0, 10, 0], tol=0.01)
