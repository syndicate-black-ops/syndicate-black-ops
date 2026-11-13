"""env_overlay (docs/ENV_OVERLAY.md): every key and flag in the "Overlay" message (decoded field by field), the
activator / All Players recipients, toggling, the 8 channels (a channel another entity took is not cleared), Off,
removal, the precache of the picture, a bad picture, the picture refused at run time, and the resend at a HUD
re-initialization (fullupdate). Players leaving and entering play: suite overlay_play."""

from etest import Suite

SUITE = Suite('overlay', 'env_overlay: screen overlays (keys, flags, channels)', players=2,
              requires=('Overlay_StartFrame',))
S = SUITE
SUITE.server_args = ('+developer', '1')    # the entity's console lines (ALERT)
DOC = 'ENV_OVERLAY.md'
SPR = 'sprites/640_logo.spr'     # animated (the HUD logo)
TGA = 'gfx/env/boatft.tga'       # 24-bit


@S.test('activator: every key in the message, only the activator gets it, the sprite is precached', players=2,
        doc=DOC,
        covers=('env_overlay', 'env_overlay.sprite', 'env_overlay.x', 'env_overlay.y', 'env_overlay.scale',
                'env_overlay.rendermode=0', 'env_overlay.renderamt', 'env_overlay.rendercolor', 'env_overlay.framerate',
                'env_overlay.fadein', 'env_overlay.holdtime', 'env_overlay.fadeout', 'env_overlay.channel=3'))
def keys(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, x='0.25', y='-0.1', scale='1.5', rendermode='0',
            renderamt='200', rendercolor='10 20 30', framerate='12.5', fadein='0.5', holdtime='2', fadeout='1',
            channel='3')
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.expect.msg('Overlay', to='player1', count=1, after='go', within=(0, 0.2),
                 fields={'channel': 3, 'on': 1, 'fullscreen': 0, 'loop': 0, 'additive': 0, 'stay': 0, 'image': SPR,
                         'x': 0.25, 'y': -0.1, 'scale': 1.5, 'r': 10, 'g': 20, 'b': 30, 'a': 200,
                         'framerate': 12.5, 'fadein': 0.5, 'hold': 2.0, 'fadeout': 1.0, 'elapsed': 0})
    t.expect.no_msg('Overlay', to='player2', after='go')
    t.expect.precached(SPR, kind='model')


@S.test('.tga picture, keys left out: the FGD defaults (centered, color 255 255 255, 10 frames a second, fade in / out '
        '0.5 s), generic precache, scale 1, FX Amount 255, stays until turned off',
        players=1, doc=DOC, covers=('env_overlay', 'env_overlay.sprite', 'env_overlay.x', 'env_overlay.y',
                                    'env_overlay.framerate', 'env_overlay.fadein', 'env_overlay.fadeout'))
def tga_defaults(t):
    t.point('env_overlay', targetname='ov', sprite=TGA)
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.fire('ov', activator='player1')   # toggles it off again
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=1, after='go',
                 fields={'channel': 1, 'on': 1, 'stay': 1, 'loop': 0, 'image': TGA, 'x': -1, 'y': -1, 'scale': 1,
                         'r': 255, 'g': 255, 'b': 255, 'a': 255, 'framerate': 10, 'fadein': 0.5, 'hold': 0,
                         'fadeout': 0.5})
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 1, 'on': 0, 'now': 0})
    t.expect.precached(TGA, kind='generic')


@S.test('All Players + Full Screen + Additive, no picture: a full-screen color flash to every player in play',
        players=2, doc=DOC,
        covers=('env_overlay', 'env_overlay:1', 'env_overlay:2', 'env_overlay.rendermode=5', 'env_overlay.renderamt',
                'env_overlay.rendercolor', 'env_overlay.holdtime', 'env_overlay.fadeout', 'env_overlay.channel=2'))
def flash(t):
    t.point('env_overlay', targetname='ov', spawnflags='3', rendermode='5', renderamt='180',
            rendercolor='255 255 255', fadein='0', holdtime='0.1', fadeout='0.5', channel='2')
    t.mark('go')
    t.fire('ov')        # no activator: All Players needs none
    t.wait(0.3)
    for p in ('player1', 'player2'):
        t.expect.msg('Overlay', to=p, count=1, after='go',
                     fields={'channel': 2, 'on': 1, 'fullscreen': 1, 'additive': 1, 'stay': 0, 'image': '',
                             'r': 255, 'a': 180, 'hold': 0.1, 'fadeout': 0.5})


@S.test('Loop: the loop flag, not stay; Glow render mode is sent as additive; Use toggles it', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay:8', 'env_overlay.holdtime', 'env_overlay.channel=4'))
def loop(t):
    t.point('env_overlay', targetname='ov', spawnflags='8', sprite=SPR, rendermode='3', fadein='0.3',
            holdtime='0.4', fadeout='0.3', channel='4')
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(1.5)
    t.mark('off')
    t.fire('ov', activator='player1')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=1, after='go',
                 fields={'channel': 4, 'on': 1, 'loop': 1, 'stay': 0, 'additive': 1, 'hold': 0.4})
    t.expect.msg('Overlay', to='player1', count=1, after='off', fields={'channel': 4, 'on': 0, 'now': 0})


@S.test('timed (hold time): every Use starts it again, never toggles; Off turns it off', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay.holdtime', 'env_overlay.channel=5'))
def timed(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, fadein='0.2', holdtime='5', fadeout='0.2', channel='5')
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.mark('off')
    t.fire('ov', activator='player1', use='off')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=2, after='go', fields={'channel': 5, 'on': 1, 'stay': 0})
    t.expect.msg('Overlay', to='player1', count=1, after='off', fields={'channel': 5, 'on': 0})


@S.test('timed overlay ended: Off sends nothing (the client ended it already)', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay.holdtime'))
def timed_ended(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, fadein='0', holdtime='0.3', fadeout='0', channel='5')
    t.fire('ov', activator='player1')
    t.wait(0.8)
    t.mark('off')
    t.fire('ov', activator='player1', use='off')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=0, after='off', fields={'channel': 5, 'on': 0})


@S.test('channels: a second overlay on a channel replaces the first; turning the first off leaves the second; '
        'another channel is separate', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay.channel=6', 'env_overlay.channel=7'))
def channels(t):
    t.point('env_overlay', targetname='a', sprite=SPR, channel='6')
    t.point('env_overlay', targetname='b', sprite=TGA, channel='6')
    t.point('env_overlay', targetname='c', sprite=SPR, channel='7')
    t.mark('go')
    t.fire('a', activator='player1', use='on')
    t.fire('c', activator='player1', use='on')
    t.wait(0.2)
    t.fire('b', activator='player1', use='on')
    t.wait(0.2)
    t.mark('a_off')
    t.fire('a', activator='player1', use='off')     # channel 6 shows b now: nothing is sent
    t.wait(0.2)
    t.mark('b_off')
    t.fire('b', activator='player1', use='off')
    t.fire('c', activator='player1', use='off')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 6, 'on': 1, 'image': SPR})
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 6, 'on': 1, 'image': TGA})
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 7, 'on': 1})
    t.expect.msg('Overlay', to='player1', count=0, after='a_off', within=(0, 0.15), fields={'channel': 6, 'on': 0})
    t.expect.msg('Overlay', to='player1', count=1, after='b_off', fields={'channel': 6, 'on': 0})
    t.expect.msg('Overlay', to='player1', count=1, after='b_off', fields={'channel': 7, 'on': 0})


@S.test('channel out of range (0, 9): channel 1', players=1, doc=DOC, covers=('env_overlay', 'env_overlay.channel'))
def channel_range(t):
    t.point('env_overlay', targetname='a', sprite=SPR, channel='9', holdtime='1')
    t.point('env_overlay', targetname='b', sprite=SPR, channel='0', holdtime='1')
    t.mark('go')
    t.fire('a', activator='player1')
    t.fire('b', activator='player1')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=2, after='go', fields={'channel': 1, 'on': 1})


@S.test('killtarget: the overlay goes off on the screens that show it', players=2, doc=DOC,
        covers=('env_overlay', 'env_overlay:1', 'env_overlay.channel=3'))
def removed(t):
    t.point('env_overlay', targetname='ov', spawnflags='1', sprite=SPR, channel='3')
    t.point('trigger_relay', targetname='kill', killtarget='ov')
    t.fire('ov')
    t.wait(0.3)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.3)
    for p in ('player1', 'player2'):
        t.expect.msg('Overlay', to=p, count=1, after='kill', fields={'channel': 3, 'on': 0})
    t.expect.removed('ov')


@S.test('fullupdate: the player\'s overlays are sent again where they are (elapsed time)', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay.channel=2'))
def fullupdate(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, channel='2', fadein='1')
    t.fire('ov', activator='player1')
    t.wait(1.0)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.3)
    t.fire('ov', activator='player1', use='off')
    t.wait(0.1)
    t.expect.msg('Overlay', to='player1', count=1, after='fu',
                 fields={'channel': 2, 'on': 1, 'image': SPR, 'elapsed': {'range': (0.8, 1.5)}})


@S.test('bad pictures (a path outside the game folder, not .spr/.tga) and no picture without Full Screen: an error, '
        'nothing sent, nothing precached', players=1, doc=DOC, covers=('env_overlay', 'env_overlay.sprite'))
def bad_picture(t):
    t.point('env_overlay', targetname='a', sprite='../hl.spr')
    t.point('env_overlay', targetname='b', sprite='sprites/x.mdl')
    t.point('env_overlay', targetname='c')
    t.mark('go')
    for n in ('a', 'b', 'c'):
        t.fire(n, activator='player1')
    t.wait(0.2)
    t.expect.no_msg('Overlay', to='player1', after='go')
    t.expect.not_precached('../hl.spr')
    t.expect.console('env_overlay')
    t.expect.console('bad picture')
    t.expect.console('a plain color needs "Full Screen"')


@S.test('the picture can\'t be changed at run time (it would need a precache); other keys can', players=1, doc=DOC,
        covers=('env_overlay', 'env_overlay.sprite', 'env_overlay.x'))
def runtime_key(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, holdtime='1', channel='4')
    t.kv('ov', 'sprite', 'sprites/glow01.spr')
    t.kv('ov', 'x', '0.5')
    t.mark('go')
    t.fire('ov', activator='player1')
    t.wait(0.2)
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 4, 'image': SPR, 'x': 0.5})
    t.expect.console("its picture can't be changed at run time")


@S.test('an activator that is not a player (no activator): nothing sent', players=1, doc=DOC, covers=('env_overlay',))
def no_player_activator(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, holdtime='1')
    t.mark('go')
    t.fire('ov')
    t.wait(0.2)
    t.expect.no_msg('Overlay', after='go')
