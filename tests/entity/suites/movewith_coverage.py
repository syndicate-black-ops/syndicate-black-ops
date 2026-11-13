"""Entity parenting, the FGD's "Move with (parent)" (movewith) and "Axis locks" (parentflags) keys on every class that
offers them (docs/MOVEWITH.md): per class, a child with movewith follows a parent sliding along Y, and a twin with
parentflags 4 (keep the Y position) stays where it is."""

from etest import Suite

SUITE = Suite('movewith_coverage', 'Entity parenting: movewith and parentflags on every class that offers them',
              requires=('MoveWith_StartFrame',))
S = SUITE
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'

BRUSH = {
    'func_breakable': {'health': '100'}, 'func_button': {}, 'func_door': {'wait': '-1'}, 'func_door_rotating': {'wait': '-1'},
    'func_illusionary': {}, 'func_illusionary_toggle': {}, 'func_plat': {}, 'func_recharge': {}, 'func_rot_button': {},
    'func_tank': {}, 'func_tanklaser': {'laserentity': 'beam'}, 'func_tankmortar': {}, 'func_tankrocket': {}, 'func_tankcontrols': {},
    'func_wall': {}, 'func_wall_toggle': {}, 'func_water': {}, 'momentary_door': {}, 'momentary_rot_button': {},
    'trigger_hurt': {'dmg': '0'},
    'trigger_multiple': {}, 'trigger_once': {},
}
POINT = {
    'ambient_generic': {'message': 'ambience/drips.wav', 'health': '10'}, 'env_glow': {'model': 'sprites/glow01.spr'},
    'env_shooter': {'shootmodel': 'models/hgibs.mdl'}, 'env_spark': {}, 'env_sprite': {'model': 'sprites/glow01.spr'},
    'info_player_deathmatch': {}, 'info_player_go2': {}, 'info_player_go3': {}, 'info_player_go4': {},
    'info_player_go5': {}, 'info_player_start': {}, 'info_target': {}, 'point_worldtext': {'message': 'x'},
    'trigger_camera': {}, 'vgui_screen': {'panelname': 'example_screen', 'width': '64', 'height': '32'},
}


def case(cls, kv, brush):
    @S.test('%s: Move with follows the parent; Axis locks 4 keeps the Y position' % cls, doc=DOC,
            covers=('%s.movewith' % cls, '%s.parentflags' % cls))
    def test(t):
        t.brush('func_door', size=(32, 32, 32), at=(-200, -200, 0), tex=METAL, targetname='p', angles='0 90 0',
                speed='192', lip=str(32 - 128), wait='-1')
        for name, x, flags in (('free', -60, None), ('kept', 60, '4')):
            keys = dict(kv, targetname=name, movewith='p')
            if flags:
                keys['parentflags'] = flags
            if brush:
                t.brush(cls, size=(16, 16, 16), at=(x, -100, 32), tex=METAL, **keys)
            else:
                t.point(cls, at=(x, -100, 32), **keys)
        if cls == 'func_tanklaser':
            t.point('env_laser', at=(0, 0, 16), targetname='beam', texture='sprites/laserbeam.spr')
        t.wait(0.3)
        t.snap('a', 'free', 'origin')
        t.snap('a', 'kept', 'origin')
        t.fire('p')
        t.wait(1.2)
        t.snap('b', 'free', 'origin')
        t.snap('b', 'kept', 'origin')
        t.expect.moved('a', 'b', 'free', (0, 128, 0), tol=3)
        t.expect.moved('a', 'b', 'kept', (0, 0, 0), tol=3)
    test.__name__ = 'mw_' + cls
    return test


for _cls, _kv in sorted(BRUSH.items()):
    case(_cls, _kv, True)
for _cls, _kv in sorted(POINT.items()):
    case(_cls, _kv, False)
