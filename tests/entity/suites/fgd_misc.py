"""Tier 5, batch 5: FGD items of env_mirror, env_overlay, vgui_screen, env_static, trigger_motion, motion_manager,
env_spritetrain and env_modeltrain that no 1.38 map uses and no other test asserted."""

from etest import Suite

SUITE = Suite('fgd_misc', 'Tier 5: mirror, overlay, screen, static prop, motion, sprite / model train FGD items',
              players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')      # trigger_motion / motion_manager Debug notes
GUIDE = 'FEATUREFUL_GUIDE_ENTITIES.md'
WS_DEFINE = 0
F_ACTIVE, F_NOINPUT, F_MOVIE, F_LOOP = 1, 8, 16, 32


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


# ---------------------------------------------------------------------------------------------- env_mirror
MIRROR_MODES = (0, 1, 2, 5)


@S.test('env_mirror Render Mode 0 / 1 / 2 / 5, FX Amount and FX Color: the mirror brush is drawn with them', doc=GUIDE,
        covers=('env_mirror.renderamt', 'env_mirror.rendercolor') + tuple('env_mirror.rendermode=%d' % m
                                                                          for m in MIRROR_MODES))
def mirror_render(t):
    for i, m in enumerate(MIRROR_MODES):
        t.brush('env_mirror', targetname='m%d' % m, size=(8, 64, 64), at=(-180 + i * 120, 0, 0), rendermode=str(m),
                renderamt='120', rendercolor='10 20 30')
    t.wait(0.2)
    for m in MIRROR_MODES:
        t.snap('s', 'm%d' % m, 'rendermode', 'renderamt', 'rendercolor')
        t.expect.value('s', 'm%d' % m, 'rendermode', m)
        t.expect.value('s', 'm%d' % m, 'renderamt', 120)
        t.expect.value('s', 'm%d' % m, 'rendercolor', [10, 20, 30])


# ---------------------------------------------------------------------------------------------- env_overlay
@S.test('env_overlay Channel 1: the overlay is sent on channel 1', players=1, doc='ENV_OVERLAY.md',
        covers=('env_overlay.channel=1',))
def overlay_channel(t):
    t.point('env_overlay', targetname='ov', sprite='sprites/640_logo.spr', channel='1')
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 1, 'on': 1})


# ---------------------------------------------------------------------------------------------- vgui_screen
@S.test('vgui_screen Movie Looping 0: the movie screen is defined without the loop flag', players=1,
        doc='WORLD_SCREENS.md#movie-screens', covers=('vgui_screen.looping=0',))
def screen_no_loop(t):
    t.point('vgui_screen', targetname='scr', angles='0 90 0', width='128', height='72',
            moviefilename='sbo_sample.ogv', looping='0', spawnflags='1')
    t.wait(0.2)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.0)
    t.expect.msg('WScreen', fields={'op': WS_DEFINE, 'movie': 'sbo_sample.ogv', 'flags': F_NOINPUT | F_MOVIE},
                 after='join', min=1)
    t.console('test_player remove latejoin')


# ---------------------------------------------------------------------------------------------- env_static
@S.test('env_static Body, Skin, Sequence, Animation speed, Scale reach the clients in its StaticProp; Start invisible '
        '(2): sent invisible', players=1, doc='ENV_STATIC.md',
        covers=('env_static.body', 'env_static.skin', 'env_static.sequence', 'env_static.framerate',
                'env_static.scale', 'env_static:2'))
def static_keys(t):
    t.point('env_static', targetname='prop', at=(0, 100, 0), model='models/barney.mdl', body='1', skin='0',
            sequence='4', framerate='0.5', scale='1.5')
    t.point('env_static', targetname='hidden', at=(100, 100, 0), model='models/barney.mdl', skin='2',
            spawnflags='2')
    t.wait(0.2)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.5)
    t.expect.msg('StaticProp', after='join', min=1,
                 fields={'type': 2, 'body': 1, 'skin': 0, 'sequence': 4, 'framerate': 0.5, 'scale': 1.5,
                         'visible': 1})
    t.expect.msg('StaticProp', after='join', min=1, fields={'type': 2, 'skin': 2, 'visible': 0})
    t.console('test_player remove latejoin')


# ---------------------------------------------------------------------------------------------- trigger_motion
@S.test('trigger_motion Meaning of Angles 1 (rotate by a direction\'s angles) and of Velocity 2 (rotate velocity by '
        'a direction\'s angles); Debug (1) prints what it changed', doc=GUIDE,
        covers=('trigger_motion.m_iAngMode=1', 'trigger_motion.m_iVelMode=2', 'trigger_motion:1'))
def motion_rotate(t):
    info(t, 'box', at=(0, 0, 100), angles='0 30 0')
    t.point('trigger_motion', targetname='setv', target='box', m_iszVelocity='10 0 0', m_iVelMode='0')
    t.point('trigger_motion', targetname='rot', target='box', m_iszAngles='0 1 0', m_iAngMode='1',
            m_iszVelocity='0 1 0', m_iVelMode='2', spawnflags='1')
    t.wait(0.1)
    t.fire('setv')
    t.wait(0.05)
    t.fire('rot')
    t.wait(0.05)
    t.snap('s', 'box', 'angles', 'velocity')
    t.expect.value('s', 'box', 'angles.y', 120, tol=0.5)
    t.expect.value('s', 'box', 'velocity', [0, 10, 0], tol=0.1)
    t.expect.console('DEBUG: trigger_motion affects info_target "%s"' % t.name('box'))


# ---------------------------------------------------------------------------------------------- motion_manager
@S.test('motion_manager Meaning of Facing 1 (rotate by a direction\'s angles, per second): yaw 90 a second; Debug (1) '
        'notes the motion thread; Facing 2 (rotate by Y Z X per second) 0 90 0 the same', doc=GUIDE,
        covers=('motion_manager.m_iFaceMode=1', 'motion_manager.m_iFaceMode=2', 'motion_manager:1'))
def manager_face(t):
    info(t, 'spin', at=(0, 0, 100))
    info(t, 'spin2', at=(100, 0, 100))
    t.point('motion_manager', targetname='mm', target='spin', m_iszFacing='0 1 0', m_iFaceMode='1',
            spawnflags=str(1 | 16))
    t.point('motion_manager', targetname='mm2', target='spin2', m_iszFacing='0 90 0', m_iFaceMode='2',
            spawnflags='16')
    t.fire('mm', use='on')
    t.fire('mm2', use='on')
    t.wait(0.1)
    t.snap('a', 'spin', 'angles')
    t.wait(2.0)
    t.snap('b', 'spin', 'angles')
    t.snap('b', 'spin2', 'angles')
    t.fire('mm', use='off')
    t.fire('mm2', use='off')
    # the thread turns by gpGlobals->frametime x rate each frame: 180 in 2 s on an idle machine, less when the
    # machine is loaded (the engine caps a frame's frametime, so game time runs ahead of the summed frame times).
    # So: both modes turn the same, at most 90 a second, and they turn
    for n in ('spin', 'spin2'):
        t.expect.value('b', n, 'angles.y', 40, op='>')
        t.expect.value('b', n, 'angles.y', 200, op='<')
    t.expect.ratio('b', 'spin', 'b', 'spin2', 'angles.y', 1.0, tol=0.02)
    t.expect.console('DEBUG: Creating motion_thread')


# ---------------------------------------------------------------------------------------------- sprite / model trains
def corners(t, a=(-150, -150, 64), b=(150, -150, 64)):
    t.point('path_corner', targetname='c1', at=a, target='c2')
    t.point('path_corner', targetname='c2', at=b, target='c1')


@S.test('env_spritetrain Sprite Name, Sprite Scale, Sound Volume: the sprite and scale it shows, its move sound at '
        'that volume; Obey Trigger Mode 0: On twice starts and stops it', doc=GUIDE,
        covers=('env_spritetrain.model', 'env_spritetrain.scale', 'env_spritetrain.volume',
                'env_spritetrain.m_iObeyTriggerMode=0'))
def spritetrain_keys(t):
    corners(t)
    t.point('env_spritetrain', targetname='st', at=(0, 0, 64), target='c1', speed='100', model='sprites/glow01.spr',
            scale='2', noise='plats/train1.wav', volume='0.4', m_iObeyTriggerMode='0')
    t.wait(0.3)
    t.snap('s', 'st', 'model', 'scale')
    t.fire('st', use='on')
    t.wait(0.5)
    t.snap('on', 'st', 'velocity')
    t.fire('st', use='on')
    t.wait(0.2)
    t.snap('off', 'st', 'velocity')
    t.expect.value('s', 'st', 'model', 'sprites/glow01.spr')
    t.expect.value('s', 'st', 'scale', 2)
    t.expect.sound('plats/train1.wav', ent='st', vol=(0.35, 0.45), min=1)
    t.expect.value('on', 'st', 'velocity.x', 50, op='>')
    t.expect.value('off', 'st', 'velocity', [0, 0, 0])


@S.test('env_modeltrain Model, Model Scale, Move / Stop Sound at Sound Volume; Obey Trigger Mode 1: On twice keeps it '
        'moving, Off stops it; Obey 0: On twice starts and stops it', doc=GUIDE,
        covers=('env_modeltrain.model', 'env_modeltrain.scale', 'env_modeltrain.noise', 'env_modeltrain.noise1',
                'env_modeltrain.volume', 'env_modeltrain.m_iObeyTriggerMode',
                'env_modeltrain.m_iObeyTriggerMode=0', 'env_modeltrain.m_iObeyTriggerMode=1'))
def modeltrain_keys(t):
    corners(t)
    corners_b = (('d1', (-150, 150, 64), 'd2'), ('d2', (150, 150, 64), 'd1'))
    for n, at, tgt in corners_b:
        t.point('path_corner', targetname=n, at=at, target=tgt)
    t.point('env_modeltrain', targetname='obey', at=(0, 0, 64), target='c1', speed='100',
            model='models/scientist.mdl', scale='1.5', noise='plats/train1.wav', noise1='plats/train2.wav',
            volume='0.4', m_iObeyTriggerMode='1')
    t.point('env_modeltrain', targetname='free', at=(0, 100, 64), target='d1', speed='100',
            model='models/scientist.mdl', m_iObeyTriggerMode='0')
    t.wait(0.3)
    t.snap('s', 'obey', 'model', 'scale')
    t.fire('obey', use='on')
    t.fire('free', use='on')
    t.wait(0.5)
    t.fire('obey', use='on')
    t.fire('free', use='on')
    t.wait(0.3)
    t.snap('twice', 'obey', 'velocity')
    t.snap('twice', 'free', 'velocity')
    t.mark('off')
    t.fire('obey', use='off')
    t.wait(0.3)
    t.snap('off', 'obey', 'velocity')
    t.expect.value('s', 'obey', 'model', 'models/scientist.mdl')
    t.expect.value('s', 'obey', 'scale', 1.5)
    t.expect.value('twice', 'obey', 'velocity.x', 50, op='>')
    t.expect.value('twice', 'free', 'velocity', [0, 0, 0])
    t.expect.value('off', 'obey', 'velocity', [0, 0, 0])
    t.expect.sound('plats/train1.wav', ent='obey', vol=(0.35, 0.45), min=1)
    t.expect.sound('plats/train2.wav', ent='obey', after='off', min=1)
