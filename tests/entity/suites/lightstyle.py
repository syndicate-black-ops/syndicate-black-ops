"""trigger_lightstyle and global_light_control (docs/LIGHT_STYLES.md, sdk/syndicate.fgd): the light styles the game
sets (LIGHT_STYLE, traced by the test layer): every Appearance choice on a named light's style, a custom pattern
(cleaned), the fade in steps, the hold time and the light's own appearance after it, -1 for good, !activator, a target
that is not a named light; global_light_control on named lights, the default style, all custom styles, its flags, a
blank pattern; round_reset at a new round. One suite: light styles are map-wide. How the light looks needs the client
test layer."""

from etest import Suite

SUITE = Suite('lightstyle', 'trigger_lightstyle / global_light_control: light styles', players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')    # the console note for a target that is not a named light
DOC = 'LIGHT_STYLES.md'
NAMED = range(32, 64)       # the compiler gives each named light a switchable style

# lights.cpp Lights_StdStyle / Lights_WorldStyle: the FGD's Appearance choices
APPEARANCE = {
    0: 'm', 13: 'a',
    1: 'mmnmmommommnonmmonqnmmo', 2: 'abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba',
    3: 'mmmmmaaaaammmmmaaaaaabcdefgabcdefg', 4: 'mamamamamama', 5: 'jklmnopqrstuvwxyzyxwvutsrqponmlkj',
    6: 'nmonqnmomnmomomno', 7: 'mmmaaaabcdefgmmmmaaaammmaamm', 8: 'mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa',
    9: 'aaaaaaaazzzzzzzz', 10: 'mmamammmmammamamaaamammma', 11: 'abcdefghijklmnopqrrqponmlkjihgfedcba',
    12: 'mmnnmmnnnmmnn',
    14: 'aabbccddeeffgghhiijjkkllmmmmmmmmmmmmmm', 15: 'abcdefghijklmmmmmmmmmmmmmmmmmmmmmmmmmm',
    16: 'acegikmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm', 17: 'llkkjjiihhggffeeddccbbaaaaaaaaaaaaaaaa',
    18: 'lkjihgfedcbaaaaaaaaaaaaaaaaaaaaaaaaaaa', 19: 'kigecaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
}


@S.test('trigger_lightstyle: every Appearance choice sets its pattern on the named light\'s style', doc=DOC,
        covers=('trigger_lightstyle', 'trigger_lightstyle.target') +
        tuple('trigger_lightstyle.appearance=%d' % a for a in APPEARANCE))
def appearances(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100')
    for a in APPEARANCE:
        t.point('trigger_lightstyle', targetname='ls%d' % a, target='lamp', appearance=str(a), m_iWait='-1')
    for a, pattern in APPEARANCE.items():
        t.mark('a%d' % a)
        t.fire('ls%d' % a)
        t.wait(0.1)
        t.expect.lightstyle(pattern, style=NAMED, count=1, after='a%d' % a, within=(0, 0.05))


@S.test('trigger_lightstyle: a custom pattern replaces the appearance, cleaned (capitals lowered, other characters '
        'dropped); !activator: the light that is the activator', doc=DOC,
        covers=('trigger_lightstyle.pattern', 'trigger_lightstyle.target'))
def custom_pattern(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', appearance='13', pattern='AbC 1d-Z', m_iWait='-1')
    t.point('trigger_lightstyle', targetname='act', target='!activator', pattern='qrs', m_iWait='-1')
    t.mark('go')
    t.fire('ls')
    t.wait(0.1)
    t.mark('act')
    t.fire('act', activator='lamp')
    t.wait(0.1)
    t.expect.lightstyle('abcdz', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('a', count=0, after='go', within=(0, 0.05))
    t.expect.lightstyle('qrs', style=NAMED, count=1, after='act')


@S.test('trigger_lightstyle Fade time: from the first letter of the current pattern to the new one\'s, a step every '
        '0.1 s, then the new pattern at the end', doc=DOC, covers=('trigger_lightstyle.m_iFade',))
def fade(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='aaz', m_iFade='1', m_iWait='-1')
    t.mark('go')
    t.fire('ls')
    t.wait(1.5)
    # m (the light's own) .. a: the letters in between, about half way at 0.5 s; the pattern itself at 1 s
    t.expect.lightstyle('l', style=NAMED, min=1, after='go', within=(0, 0.25))
    t.expect.lightstyle('b', style=NAMED, min=1, after='go', within=(0.6, 1.15))
    t.expect.lightstyle('aaz', style=NAMED, count=1, after='go', within=(0.95, 1.25))
    t.expect.lightstyle('m', count=0, after='go')        # Hold -1: the light's own does not come back
    t.expect.lightstyle(style=NAMED, min=8, after='go', within=(0, 1.2))       # a step every 0.1 s


@S.test('trigger_lightstyle Hold time: the new appearance for that long, then the light\'s own comes back (its '
        'pattern; off: a); -1: for good', doc=DOC, covers=('trigger_lightstyle.m_iWait',))
def hold(t):
    t.point('light', targetname='lamp', at=(-100, 0, 100), _light='255 255 255 100', pattern='mno')
    t.point('light', targetname='dark', at=(100, 0, 100), _light='255 255 255 100', spawnflags='1')
    t.point('light', targetname='keep', at=(0, 100, 100), _light='255 255 255 100')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='zzz', m_iWait='1')
    t.point('trigger_lightstyle', targetname='ls2', target='dark', pattern='yyy', m_iWait='1')
    t.point('trigger_lightstyle', targetname='ls3', target='keep', pattern='xxx', m_iWait='-1')
    t.mark('go')
    t.fire('ls')
    t.fire('ls2')
    t.fire('ls3')
    t.wait(2.0)
    t.expect.lightstyle('zzz', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('mno', style=NAMED, count=1, after='go', within=(0.95, 1.2))
    t.expect.lightstyle('yyy', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('a', style=NAMED, count=1, after='go', within=(0.95, 1.2))
    t.expect.lightstyle('xxx', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('m', count=0, after='go', within=(0.05, 2.0))


@S.test('trigger_lightstyle Hold time with decimals: 0.6 holds the new appearance 0.6 s, then the light\'s own '
        'comes back', doc=DOC, covers=('trigger_lightstyle.m_iWait',))
def hold_decimal(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100', pattern='mno')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='zazmz', m_iWait='0.6')
    t.mark('go')
    t.fire('ls')
    t.wait(1.5)
    t.expect.lightstyle('zazmz', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('mno', style=NAMED, count=0, after='go', within=(0, 0.5))
    t.expect.lightstyle('mno', style=NAMED, count=1, after='go', within=(0.55, 0.8))


@S.test('trigger_lightstyle: a target that is not a named light is left alone, with a console note', doc=DOC,
        covers=('trigger_lightstyle.target',))
def not_a_light(t):
    t.point('info_target', targetname='box')
    t.point('trigger_lightstyle', targetname='ls', target='box', pattern='zz', m_iWait='-1')
    t.mark('go')
    t.fire('ls')
    t.wait(0.1)
    t.expect.lightstyle('zz', count=0, after='go')
    t.expect.console('is not a named light')


@S.test('trigger_lightstyle with Hold time left out: the FGD / doc default -1, the new appearance stays', doc=DOC,
        covers=('trigger_lightstyle.m_iWait',))
def hold_default(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', appearance='13')
    t.mark('go')
    t.fire('ls')
    t.wait(0.5)
    t.expect.lightstyle('a', style=NAMED, count=1, after='go')
    t.expect.lightstyle('m', count=0, after='go')


@S.test('global_light_control Only these named lights: their style gets the pattern; a blank pattern is m', doc=DOC,
        covers=('global_light_control', 'global_light_control.target_entity', 'global_light_control.pattern'))
def glc_named(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100', spawnflags='1')
    t.point('global_light_control', targetname='g', target_entity='lamp', pattern='bcd', names=('target_entity',))
    t.point('global_light_control', targetname='g2', target_entity='lamp', pattern='', names=('target_entity',))
    t.mark('go')
    t.fire('g')
    t.wait(0.1)
    t.mark('blank')
    t.fire('g2')
    t.wait(0.1)
    t.expect.lightstyle('bcd', style=NAMED, count=1, after='go', within=(0, 0.05))
    t.expect.lightstyle('bcd', count=1, after='go')
    t.expect.lightstyle('m', style=NAMED, count=1, after='blank')


@S.test('global_light_control flags: none: the default style 0 only; 65536: also every custom style 1-63; 65536 + '
        '131072: the custom styles but not 0; 131072 alone: nothing', doc=DOC,
        covers=('global_light_control:65536', 'global_light_control:131072', 'global_light_control.pattern'))
def glc_flags(t):
    t.point('global_light_control', targetname='g0', pattern='b')
    t.point('global_light_control', targetname='g1', pattern='c', spawnflags='65536')
    t.point('global_light_control', targetname='g2', pattern='d', spawnflags=str(65536 | 131072))
    t.point('global_light_control', targetname='g3', pattern='e', spawnflags='131072')
    for n in ('g0', 'g1', 'g2', 'g3'):
        t.mark(n)
        t.fire(n)
        t.wait(0.1)
    t.point('global_light_control', targetname='back', pattern='m', spawnflags='65536')
    t.fire('back')
    t.wait(0.1)
    t.expect.lightstyle('b', style=0, count=1, after='g0', within=(0, 0.05))
    t.expect.lightstyle('b', count=1, after='g0', within=(0, 0.05))
    t.expect.lightstyle('c', style=0, count=1, after='g1', within=(0, 0.05))
    t.expect.lightstyle('c', style=range(1, 64), count=63, after='g1', within=(0, 0.05))
    t.expect.lightstyle('d', style=0, count=0, after='g2', within=(0, 0.05))
    t.expect.lightstyle('d', style=range(1, 64), count=63, after='g2', within=(0, 0.05))
    t.expect.lightstyle('e', count=0, after='g3', within=(0, 0.05))


@S.test('round_reset 1: at a new round every style goes back to the map start\'s (the world\'s 0-12 and 63, named '
        'lights\' own) and the unnamed global_light_control applies itself again; a fade or hold ends', players=1,
        doc=DOC, covers=('trigger_lightstyle.round_reset=1', 'global_light_control.round_reset=1'))
def round_reset(t):
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100', pattern='mmo')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='zzz', m_iWait='30', round_reset='1')
    t.point('light', targetname='other', at=(100, 0, 100), _light='255 255 255 100')
    t.point('global_light_control', pattern='kkj', target_entity='other', names=('target_entity',))   # no name
    t.point('global_light_control', targetname='dim', pattern='b', round_reset='1')
    t.fire('ls')
    t.fire('dim')
    t.wait(0.2)
    t.mark('restart')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.5)
    t.expect.lightstyle('m', style=0, min=1, after='restart')
    t.expect.lightstyle('mmnmmommommnonmmonqnmmo', style=1, min=1, after='restart')
    t.expect.lightstyle('a', style=63, min=1, after='restart')
    t.expect.lightstyle('mmo', style=NAMED, min=1, after='restart')
    t.expect.lightstyle('kkj', style=NAMED, min=1, after='restart')
    t.expect.lightstyle('zzz', count=0, after='restart')
    t.expect.lightstyle('b', count=0, after='restart')
