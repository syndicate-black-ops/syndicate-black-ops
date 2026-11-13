"""trigger_camera flag 131072 "Hide HUD and crosshair" (docs/OBJECTIVES.md "Cameras that hide the HUD"): while the camera
has the view the player's HideWeapon byte has HIDEHUD_ALL (4); it goes when the view comes back: the camera's end,
turned off, death. A camera without the flag leaves the HUD alone."""

from etest import Suite

SUITE = Suite('camera_hud', 'trigger_camera "Hide HUD and crosshair" (131072)', players=1, requires=('Objective_SendQueued',))
S = SUITE
DOC = 'OBJECTIVES.md#cameras-that-hide-the-hud'
HIDE = 1 << 17


def camera(t, name='cam', flags=HIDE, wait='0.8'):
    t.point('info_target', targetname='look', at=(200, 200, 40))
    t.point('trigger_camera', targetname=name, at=(-150, -150, 100), target='look', wait=wait, spawnflags=str(flags))


@S.test('with the flag: HideWeapon with HIDEHUD_ALL when the camera starts, without it at the hold time',
        players=1, doc=DOC, covers=('trigger_camera', 'trigger_camera:131072', 'trigger_camera.wait'))
def hide_and_back(t):
    camera(t)
    t.wait(0.3)
    t.mark('on')
    t.fire('cam', activator='player1')
    t.wait(0.4)
    t.mark('mid')
    t.wait(0.8)
    t.expect.setview('player1', 'cam', count=1, after='on')
    t.expect.msg('HideWeapon', to='player1', count=1, after='on', within=(0, 0.2), fields={'all': True})
    t.expect.msg('HideWeapon', to='player1', count=1, after='mid', within=(0.2, 0.7), fields={'all': False})


@S.test('without the flag: no HIDEHUD_ALL while the camera has the view', players=1, doc=DOC,
        covers=('trigger_camera',))
def no_flag(t):
    camera(t, flags=0)
    t.wait(0.3)
    t.mark('on')
    t.fire('cam', activator='player1')
    t.wait(1.2)
    t.expect.setview('player1', 'cam', count=1, after='on')
    t.expect.msg('HideWeapon', to='player1', count=0, after='on', fields={'all': True})


@S.test('turned off (Off) before its hold time: the HUD comes back at once', players=1, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:131072'))
def turned_off(t):
    camera(t, wait='10')
    t.wait(0.3)
    t.mark('on')
    t.fire('cam', activator='player1')
    t.wait(0.3)
    t.mark('off')
    t.fire('cam', use='off')
    t.wait(0.3)
    t.expect.msg('HideWeapon', to='player1', count=1, after='on', within=(0, 0.2), fields={'all': True})
    t.expect.msg('HideWeapon', to='player1', count=1, after='off', within=(0, 0.2), fields={'all': False})


@S.test('death while the camera has the view: the HUD flag goes with the view (the dead HUD has no HIDEHUD_ALL)',
        players=1, doc=DOC, covers=('trigger_camera', 'trigger_camera:131072'))
def death(t):
    camera(t, wait='10')
    t.wait(0.3)
    t.mark('on')
    t.fire('cam', activator='player1')
    t.wait(0.3)
    t.mark('die')
    t.damage('player1', 500)
    t.wait(0.5)
    t.console('test_spawn player1')
    t.wait(0.3)
    t.expect.msg('HideWeapon', to='player1', count=1, after='on', within=(0, 0.2), fields={'all': True})
    t.expect.msg('HideWeapon', to='player1', min=1, after='die', within=(0, 0.3), fields={'all': False})
    t.expect.setview('player1', 'player1', after='die', min=1)
