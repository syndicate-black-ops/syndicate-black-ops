"""Entity parenting for entities the players' games draw from a copy (docs/MOVEWITH.md): point_worldtext follows a
parent on the clients (its "WorldText" message carries the parent and the local pose, and the client draws it on the
parent's drawn pose each frame); env_mirror, env_static, func_rainvolume, func_snowvolume, color_correction_volume and
env_fogzone are refused as children with a console line and stay where they are."""

from etest import Suite

SUITE = Suite('movewith_drawn', 'Entity parenting: client-drawn entities (feat/movewith)', players=1,
              requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


@S.test('an unnamed point_worldtext with a parent keeps its edict and is sent with its parent and local pose; the '
        'world pose follows the parent', players=1, doc=DOC)
def worldtext_follows(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('point_worldtext', at=(-150, 100, 64), message='riding', movewith='p')
    t.wait(0.3)
    t.mark('fu1')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.fire('p')
    t.wait(1.5)
    t.mark('fu2')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    pos = list(t._abs((-150, 100, 64)))
    moved = [pos[0] + 192, pos[1], pos[2]]
    # the brush parent has no origin brush: its origin is the map's origin, so the local pose is the world place
    t.expect.msg('WorldText', to='player1', count=1, after='fu1', within=(0, 1.0),
                 fields={'text': 'riding', 'parented': 1, 'local_origin': pos, 'origin': pos})
    t.expect.msg('WorldText', to='player1', count=1, after='fu2',
                 fields={'text': 'riding', 'parented': 1, 'local_origin': pos, 'origin': moved})


@S.test('a point_worldtext whose parent starts hidden is sent with its parent, so the clients place it on the parent '
        'once they receive it (R49)', players=1, doc=DOC)
def worldtext_hidden_parent(t):
    t.brush('func_wall_toggle', size=(64, 64, 32), at=(-150, 150, 0), tex=METAL, targetname='hid', spawnflags='1')
    t.point('point_worldtext', at=(-150, 150, 64), message='waiting', movewith='hid')
    t.wait(0.3)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='fu', fields={'text': 'waiting', 'parented': 1})


@S.test('control: a point_worldtext without a parent is sent without one', players=1, doc=DOC)
def worldtext_plain(t):
    t.point('point_worldtext', at=(0, 0, 64), message='still')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='fu', fields={'text': 'still', 'parented': 0})


@S.test('func_rainvolume, func_snowvolume and env_fogzone are refused as children with a console line and stay put',
        players=1, doc=DOC)
def refused_copies(t):
    mover(t, 'p', (-150, 150, 0))
    t.brush('func_rainvolume', size=(64, 64, 64), at=(-150, 0, 0), tex='AAATRIGGER', targetname='rain', movewith='p')
    t.brush('func_snowvolume', size=(64, 64, 64), at=(0, 0, 0), tex='AAATRIGGER', targetname='snow', movewith='p')
    t.brush('env_fogzone', size=(64, 64, 64), at=(150, 0, 0), tex='AAATRIGGER', targetname='fog', movewith='p')
    t.wait(0.3)
    for n in ('rain', 'snow', 'fog'):
        t.snap('a', n, 'origin')
    t.fire('p')
    t.wait(1.5)
    for n in ('rain', 'snow', 'fog'):
        t.snap('b', n, 'origin')
        t.expect.moved('a', 'b', n, (0, 0, 0), tol=0.01)
    t.expect.console("can't move with %s, because each player's game draws it from a copy it gets once" % t.name('p'))


@S.test('a point vgui_screen with a parent: its DEFINE is followed by the parent op with its local pose', players=1,
        doc=DOC)
def screen_follows(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('vgui_screen', targetname='scr', at=(-150, 100, 64), angles='0 90 0', panelname='security_main',
            width='64', height='32', movewith='p')
    t.point('vgui_screen', targetname='still', at=(100, 100, 64), panelname='security_main')
    t.wait(0.3)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    pos = list(t._abs((-150, 100, 64)))
    t.expect.msg('WScreen', to='player1', count=1, after='fu', fields={'op': 3, 'local_origin': pos,
                                                                       'local_angles': [0, 90, 0]})
    t.expect.msg('WScreen', to='player1', count=2, after='fu', fields={'op': 0})     # both screens defined
