"""Entity parenting with trigger_camera (docs/MOVEWITH.md): a camera with a parent rides with it by position only.
Its aim, turning and target tracking stay its own, and it ignores its own path (path_corner movement, its speed
keys) while it has a parent."""

from etest import Suite

SUITE = Suite('movewith_camera', 'Entity parenting: trigger_camera as a child (feat/movewith)',
              requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


@S.test('a camera on a moving parent rides with it and keeps aiming at its target', doc=DOC)
def camera_rides_and_aims(t):
    mover(t, 'p', (-150, -100, 0))
    # the camera starts at (-150, -150) looking +x; its target is straight +y of where the parent takes it
    t.point('trigger_camera', at=(-150, -150, 40), targetname='cam', target='look', movewith='p', wait='20',
            angles='0 0 0')
    t.point('info_target', at=(42, 150, 40), targetname='look')
    t.wait(0.2)
    t.snap('a', 'cam', 'origin')
    t.fire('cam')
    t.fire('p')
    t.wait(7.0)     # it turns at Camera Rotation Speed 40: most of the way in 7 s
    t.snap('b', 'cam', 'origin', 'angles')
    t.expect.moved('a', 'b', 'cam', (192, 0, 0), tol=2)       # rode with the parent
    # its own aim: turned to the target (yaw 90 from its new place), not held at the parent's angles (0)
    t.expect.value('b', 'cam', 'angles.y', 80, op='>')
    t.expect.value('b', 'cam', 'angles.y', 100, op='<')


@S.test('a camera with a parent and a path: a console line, it ignores the path and rides with the parent', doc=DOC)
def camera_ignores_path(t):
    mover(t, 'p', (-150, -100, 0))
    t.point('path_corner', targetname='c1', at=(-150, 100, 40), target='c2')
    t.point('path_corner', targetname='c2', at=(100, 100, 40))
    t.point('trigger_camera', at=(-150, -150, 40), targetname='cam', target='look', movewith='p', wait='20',
            moveto='c1', speed='200', spawnflags='0')
    t.point('info_target', at=(0, 0, 40), targetname='look')
    t.wait(0.2)
    t.snap('a', 'cam', 'origin')
    t.fire('cam')
    t.fire('p')
    t.wait(2.0)
    t.snap('b', 'cam', 'origin')
    t.expect.console('has a parent, so it ignores its path and rides with the parent')
    t.expect.moved('a', 'b', 'cam', (192, 0, 0), tol=2)

