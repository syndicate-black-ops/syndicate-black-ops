"""vgui_screen panel screens and vgui_control_proxy (docs/WORLD_SCREENS.md, sdk/syndicate.fgd): the "WScreen" DEFINE
field by field (origin, angles, width, height, input distance and their defaults / limit, Show Cursor, Input Disabled,
Start Inactive, the panel name, a surface carrier), STATE on toggling, the layout's precache; every vgui_control_proxy
mode as a CONTROL message, kept controls sent again at a HUD re-initialization, a text cut at 127 characters, a
proxy without a control name. Movie screens: suite video. Drawing and clicks need the client layer (a button command
needs the screen's entity index, which a test cannot name yet)."""

from etest import Suite

SUITE = Suite('screens', 'vgui_screen / vgui_control_proxy: world screen state (server side)', players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')    # console notes
DOC = 'WORLD_SCREENS.md'
DEFINE, STATE, CONTROL = 0, 1, 2
F_ACTIVE, F_NOCURSOR, F_BOUND, F_NOINPUT = 1, 2, 4, 8
C_TEXT, C_ENABLED, C_VISIBLE, C_ANIM = 0, 1, 2, 3


@S.test('every key in the DEFINE (fullupdate): origin, angles, width, height, input distance, panel name; Show Cursor '
        '0 sets no-cursor; the layout and its animation script are precached', players=1, doc=DOC,
        covers=('vgui_screen', 'vgui_screen.panelname', 'vgui_screen.width', 'vgui_screen.height',
                'vgui_screen.inputdistance', 'vgui_screen.angles', 'vgui_screen.showcursor=0'))
def define_keys(t):
    t.point('vgui_screen', targetname='scr', at=(0, 100, 64), angles='0 90 0', panelname='security_main', width='96',
            height='48', inputdistance='200', showcursor='0')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((0, 100, 64))), 'angles': [0, 16384, 0], 'width': 96,
                         'height': 48, 'inputdistance': 200, 'flags': F_ACTIVE | F_NOCURSOR,
                         'panel': 'security_main'})
    t.expect.precached('ui/scripts/screens/security_main.res', kind='generic')
    t.expect.precached('ui/scripts/screens/security_main_anims.txt', kind='generic')


@S.test('keys left out: width and height 32, input distance 128, the cursor shown (Show Cursor -1 and 1 the same); '
        'an input distance above 4096 is 4096', players=1, doc=DOC,
        covers=('vgui_screen.width', 'vgui_screen.height', 'vgui_screen.inputdistance', 'vgui_screen.showcursor=-1',
                'vgui_screen.showcursor=1'))
def defaults(t):
    t.point('vgui_screen', at=(-100, 100, 64), panelname='example_screen')
    t.point('vgui_screen', at=(0, 100, 64), panelname='example_screen', showcursor='-1', inputdistance='9999')
    t.point('vgui_screen', at=(100, 100, 64), panelname='example_screen', showcursor='1')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((-100, 100, 64))), 'width': 32, 'height': 32,
                         'inputdistance': 128, 'flags': F_ACTIVE})
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((0, 100, 64))), 'inputdistance': 4096, 'flags': F_ACTIVE})
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((100, 100, 64))), 'flags': F_ACTIVE})


@S.test('flag 1 Start Inactive: defined inactive; triggered: STATE active to everybody, again: inactive; On when on '
        'sends nothing; flag 2 Input Disabled sets the no-input flag', players=1, doc=DOC,
        covers=('vgui_screen:1', 'vgui_screen:2', 'vgui_screen.targetname'))
def start_inactive(t):
    t.point('vgui_screen', targetname='scr', at=(0, 100, 64), panelname='example_screen', spawnflags='3')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('on')
    t.fire('scr')
    t.wait(0.2)
    t.mark('on2')
    t.fire('scr', use='on')
    t.wait(0.2)
    t.mark('off')
    t.fire('scr')
    t.wait(0.2)
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((0, 100, 64))), 'flags': F_NOINPUT})
    t.expect.msg('WScreen', count=1, after='on', within=(0, 0.15), fields={'op': STATE, 'flags': F_ACTIVE | F_NOINPUT})
    t.expect.no_msg('WScreen', after='on2', within=(0, 0.15))
    t.expect.msg('WScreen', count=1, after='off', fields={'op': STATE, 'flags': F_NOINPUT})


@S.test('Surface: a brush entity carries the screen (bound flag, the carrier\'s submodel); its render mode becomes '
        'Solid / 255; a surface that is not a brush entity: shown on the rectangle, error line', players=1, doc=DOC,
        covers=('vgui_screen.surface',))
def surface(t):
    t.brush('func_wall', targetname='wall', size=(8, 96, 64), at=(0, 150, 32))
    t.point('vgui_screen', at=(-50, 100, 64), panelname='example_screen', surface='wall', names=('surface',))
    t.point('info_target', targetname='pt', at=(50, 100, 64))
    t.point('vgui_screen', at=(50, 100, 64), panelname='example_screen', surface='pt', names=('surface',))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.snap('s', 'wall', 'rendermode', 'renderamt')
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((-50, 100, 64))), 'flags': F_ACTIVE | F_BOUND})
    t.expect.msg('WScreen', to='player1', count=1, after='fu',
                 fields={'op': DEFINE, 'origin': list(t._abs((50, 100, 64))), 'flags': F_ACTIVE})
    t.expect.value('s', 'wall', 'rendermode', 4)
    t.expect.value('s', 'wall', 'renderamt', 255)
    t.expect.console('is not a brush entity')


MODES = [(0, C_TEXT, 'Door open'), (1, C_ENABLED, '1'), (2, C_ENABLED, '0'), (3, C_VISIBLE, '1'), (4, C_VISIBLE, '0'),
         (5, C_ANIM, 'Blink')]


@S.test('vgui_control_proxy: every Change mode sends its CONTROL to everybody (set text, enable, disable, show, hide, '
        'play animation with its state group); a screen of another name is not changed', players=1, doc=DOC,
        covers=('vgui_control_proxy', 'vgui_control_proxy.screen', 'vgui_control_proxy.controlname',
                'vgui_control_proxy.text') + tuple('vgui_control_proxy.mode=%d' % m for m, _o, _v in MODES))
def proxy_modes(t):
    t.point('vgui_screen', targetname='scr', at=(0, 100, 64), panelname='security_main')
    for m, _op, v in MODES:
        t.point('vgui_control_proxy', targetname='p%d' % m, screen='scr', controlname='StatusLabel',
                mode=str(m), text=v if m in (0, 5) else None)
    for m, op, v in MODES:
        t.mark('m%d' % m)
        t.fire('p%d' % m)
        t.wait(0.1)
        t.expect.msg('WScreen', count=1, after='m%d' % m, within=(0, 0.05),
                     fields={'op': CONTROL, 'cop': op, 'control': 'StatusLabel', 'value': v})


@S.test('the changed controls are kept: a HUD re-initialization (fullupdate) sends the DEFINE, then the latest text, '
        'enabled and visible state and the animation of each state group', players=1, doc=DOC,
        covers=('vgui_control_proxy.mode=0', 'vgui_control_proxy.mode=2', 'vgui_control_proxy.mode=4',
                'vgui_control_proxy.mode=5'))
def kept(t):
    t.point('vgui_screen', targetname='scr', at=(0, 100, 64), panelname='security_main')
    t.point('vgui_control_proxy', targetname='a', screen='scr', controlname='Lbl', text='first')
    t.point('vgui_control_proxy', targetname='b', screen='scr', controlname='Lbl', text='second')
    t.point('vgui_control_proxy', targetname='c', screen='scr', controlname='Lbl', mode='2')
    t.point('vgui_control_proxy', targetname='d', screen='scr', controlname='Lbl', mode='4')
    t.point('vgui_control_proxy', targetname='e', screen='scr', controlname='Grp', mode='5', text='Pulse')
    for n in ('a', 'b', 'c', 'd', 'e'):
        t.fire(n)
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WScreen', to='player1', count=1, after='fu', fields={'op': CONTROL, 'cop': C_TEXT, 'control': 'Lbl', 'value': 'second'})
    t.expect.msg('WScreen', to='player1', count=0, after='fu', fields={'op': CONTROL, 'value': 'first'})
    t.expect.msg('WScreen', to='player1', count=1, after='fu', fields={'op': CONTROL, 'cop': C_ENABLED, 'control': 'Lbl', 'value': '0'})
    t.expect.msg('WScreen', to='player1', count=1, after='fu', fields={'op': CONTROL, 'cop': C_VISIBLE, 'control': 'Lbl', 'value': '0'})
    t.expect.msg('WScreen', to='player1', count=1, after='fu', fields={'op': CONTROL, 'cop': C_ANIM, 'control': 'Grp', 'value': 'Pulse'})


@S.test('vgui_control_proxy: a text of 127+ characters is cut to 127 (console note); without a control name it does '
        'nothing (console note)', players=1, doc=DOC, covers=('vgui_control_proxy.text', 'vgui_control_proxy.controlname'))
def proxy_limits(t):
    long_text = 'x' * 150
    t.point('vgui_screen', targetname='scr', at=(0, 100, 64), panelname='security_main')
    t.point('vgui_control_proxy', targetname='long', screen='scr', controlname='Lbl', text=long_text)
    t.point('vgui_control_proxy', targetname='none', screen='scr', text='nothing')
    t.mark('go')
    t.fire('long')
    t.fire('none')
    t.wait(0.2)
    t.expect.msg('WScreen', count=1, after='go', fields={'op': CONTROL, 'cop': C_TEXT, 'value': 'x' * 127})
    t.expect.msg('WScreen', count=0, after='go', fields={'value': 'nothing'})
    t.expect.console('"text" longer than 127 characters, cut')
    t.expect.console('"controlname" missing or not a plain name: does nothing')
