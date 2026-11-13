"""point_worldtext (docs/POINT_WORLDTEXT.md, sdk/syndicate.fgd): the "WorldText" / "WorldTextP" messages decoded field
by field: every key (text with \\n, size, spacing, color with its alpha, font, orientation, rainbow), the defaults,
the flags Start Off and Center lines, toggling and killtarget for everybody, a long text in pieces, out-of-range
values, a key changed at run time (trigger_changevalue), the resend at a HUD re-initialization (fullupdate, only
texts that are on). How the text is drawn needs the client test layer."""

from etest import Suite

SUITE = Suite('worldtext', 'point_worldtext: world text state (server side)', players=1)
S = SUITE
DOC = 'POINT_WORLDTEXT.md'


def at(t, pos):
    return list(t._abs(pos))


@S.test('every key in the message (fullupdate): the text with \\n as a new line, origin, angles, size, spacing X / Y, '
        'color with alpha, font, orientation, rainbow; no name: still sent (kept without an edict)', players=1,
        doc=DOC, covers=('point_worldtext', 'point_worldtext.message', 'point_worldtext.textsize',
                         'point_worldtext.textspacingX', 'point_worldtext.textspacingY', 'point_worldtext.color',
                         'point_worldtext.angles', 'point_worldtext.font=9', 'point_worldtext.orientation=0',
                         'point_worldtext.rainbow=1'))
def keys(t):
    t.point('point_worldtext', at=(0, 0, 64), message='Line one\\nline two', angles='10 90 0', textsize='24',
            textspacingX='2.5', textspacingY='-3', color='10 20 30 40', font='9', orientation='0', rainbow='1')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='fu',
                 fields={'on': 1, 'rainbow': 1, 'center': 0, 'origin': at(t, (0, 0, 64)), 'angles': [10, 90, 0],
                         'size': 24, 'spacing_x': 2.5, 'spacing_y': -3, 'color': [10, 20, 30, 40], 'font': 9,
                         'orientation': 0, 'text': 'Line one\nline two'})


@S.test('keys left out: size 10, white with alpha 255, font 0, orientation 0, no rainbow, spacing 0; a color with '
        'three numbers keeps alpha 255', players=1, doc=DOC,
        covers=('point_worldtext.textsize', 'point_worldtext.color', 'point_worldtext.font=0',
                'point_worldtext.rainbow=0'))
def defaults(t):
    t.point('point_worldtext', at=(-100, 0, 64), message='plain')
    t.point('point_worldtext', at=(100, 0, 64), message='rgb', color='1 2 3')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='fu',
                 fields={'origin': at(t, (-100, 0, 64)), 'rainbow': 0, 'center': 0, 'size': 10, 'spacing_x': 0,
                         'spacing_y': 0, 'color': [255, 255, 255, 255], 'font': 0, 'orientation': 0, 'text': 'plain'})
    t.expect.msg('WorldText', to='player1', count=1, after='fu',
                 fields={'origin': at(t, (100, 0, 64)), 'color': [1, 2, 3, 255]})


@S.test('the font choices (Sans 0 / 1 / 12 / 13, Mono 8 / 9 / 10 / 11) and orientations 1 / 2 are sent as given; '
        'flag 2 Center lines sets the centered flag', players=1, doc=DOC,
        covers=tuple('point_worldtext.font=%d' % f for f in (0, 1, 12, 13, 8, 9, 10, 11)) +
        ('point_worldtext.orientation=1', 'point_worldtext.orientation=2', 'point_worldtext:2'))
def fonts_orientation(t):
    fonts = (0, 1, 12, 13, 8, 9, 10, 11)
    for i, f in enumerate(fonts):
        t.point('point_worldtext', at=(-210 + 60 * i, 0, 64), message='f%d' % f, font=str(f),
                orientation=str(1 + i % 2), spawnflags='2' if i == 0 else '0')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    for i, f in enumerate(fonts):
        t.expect.msg('WorldText', to='player1', count=1, after='fu',
                     fields={'origin': at(t, (-210 + 60 * i, 0, 64)), 'font': f, 'orientation': 1 + i % 2,
                             'center': 1 if i == 0 else 0, 'text': 'f%d' % f})


@S.test('out of range: font 99 and orientation 7 give 0, a color above 255 / below 0 is clamped, textsize 5000 gives '
        '4096', players=1, doc=DOC, covers=('point_worldtext.font', 'point_worldtext.orientation'))
def clamps(t):
    t.point('point_worldtext', at=(0, 0, 64), message='x', font='99', orientation='7', color='300 -5 7 999',
            textsize='5000')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='fu',
                 fields={'origin': at(t, (0, 0, 64)), 'font': 0, 'orientation': 0, 'color': [255, 0, 7, 255],
                         'size': 4096})


@S.test('flag 1 Start Off: not sent at a fullupdate; a trigger shows it to everybody; triggered again it is turned '
        'off (on 0); On when on changes nothing', players=1, doc=DOC,
        covers=('point_worldtext:1', 'point_worldtext.targetname'))
def start_off_toggle(t):
    t.point('point_worldtext', targetname='w', at=(0, 0, 64), message='hello', spawnflags='1')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('on')
    t.fire('w')
    t.wait(0.3)
    t.mark('on2')
    t.fire('w', use='on')
    t.wait(0.3)
    t.mark('off')
    t.fire('w')
    t.wait(0.3)
    t.expect.msg('WorldText', count=0, after='fu', within=(0, 0.95), fields={'text': 'hello'})
    t.expect.msg('WorldText', to='player1', count=1, after='on', within=(0, 0.25),
                 fields={'on': 1, 'origin': at(t, (0, 0, 64)), 'text': 'hello'})
    t.expect.no_msg('WorldText', after='on2', within=(0, 0.25))
    t.expect.msg('WorldText', to='player1', count=1, after='off', fields={'on': 0})


@S.test('a text longer than one message (1000 bytes): "WorldTextP" pieces of the start, then "WorldText" with the '
        'rest; together the whole text', players=1, doc=DOC, covers=('point_worldtext.message',))
def long_text(t):
    text = ''.join(chr(ord('a') + i % 26) for i in range(1000))
    t.point('point_worldtext', targetname='w', at=(0, 0, 64), message=text, spawnflags='1')
    t.mark('on')
    t.fire('w')
    t.wait(0.3)
    # 1000 bytes: 148 in WorldText, 852 in pieces of 180 (180 x 4 + 132)
    t.expect.msg('WorldTextP', to='player1', count=5, after='on')
    t.expect.msg('WorldTextP', to='player1', count=1, after='on', fields={'text': text[:180]})
    t.expect.msg('WorldTextP', to='player1', count=1, after='on', fields={'text': text[720:852]})
    t.expect.msg('WorldText', to='player1', count=1, after='on', fields={'on': 1, 'text': text[852:]})


@S.test('a key changed at run time (trigger_changevalue: message, color, textsize) is sent to everybody at once',
        players=1, doc=DOC, covers=('point_worldtext.message', 'point_worldtext.color', 'point_worldtext.textsize'))
def runtime_change(t):
    t.point('point_worldtext', targetname='w', at=(0, 0, 64), message='before')
    t.point('trigger_changevalue', targetname='cv1', target='w', m_iszValueName='message', m_iszNewValue='after')
    t.point('trigger_changevalue', targetname='cv2', target='w', m_iszValueName='color', m_iszNewValue='0 255 0 128')
    t.point('trigger_changevalue', targetname='cv3', target='w', m_iszValueName='textsize', m_iszNewValue='32')
    t.mark('go')
    t.fire('cv1')
    t.wait(0.3)
    t.fire('cv2')
    t.wait(0.3)
    t.fire('cv3')
    t.wait(0.3)
    t.expect.msg('WorldText', to='player1', count=1, after='go', within=(0, 0.25),
                 fields={'on': 1, 'text': 'after', 'color': [255, 255, 255, 255]})
    t.expect.msg('WorldText', to='player1', count=1, after='go',
                 fields={'on': 1, 'text': 'after', 'color': [0, 255, 0, 128], 'size': 10})
    t.expect.msg('WorldText', to='player1', count=1, after='go',
                 fields={'on': 1, 'text': 'after', 'color': [0, 255, 0, 128], 'size': 32})


@S.test('killtarget: every client forgets the text (on 0), and a later fullupdate does not send it', players=1,
        doc=DOC, covers=('point_worldtext.targetname',))
def removed(t):
    t.point('point_worldtext', targetname='w', at=(0, 0, 64), message='gone soon')
    t.point('trigger_relay', targetname='kill', killtarget='w')
    t.mark('kill')
    t.fire('kill')
    t.wait(0.3)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('WorldText', to='player1', count=1, after='kill', within=(0, 0.25), fields={'on': 0})
    t.expect.msg('WorldText', count=0, after='fu', fields={'text': 'gone soon'})
