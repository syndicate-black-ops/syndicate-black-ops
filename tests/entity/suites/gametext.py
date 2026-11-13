"""game_text on a real server (GAPS.md tier 1 "game_text", 42 maps / 298 entities): the HUD text message
(TE_TEXTMESSAGE, util.cpp UTIL_HudMessage) to the activator or, with flag 1 All Players, to every player; channel,
effect, x / y, colors, fade / hold / scan times as the FGD offers them; master. What each checks is the FGD's
description (sdk/syndicate.fgd game_text)."""

from etest import Suite

SUITE = Suite('gametext', 'game_text: activator / All Players, channel, effect, position, colors, times, master')
S = SUITE

TE = 'svc:23'


def text(**fields):
    f = {'te': 29}
    f.update(fields)
    return f


@S.test('game_text: the text goes to the activator only; flag 1 All Players: to every player', players=2,
        covers=('game_text', 'game_text.message', 'game_text:1'))
def all_players(t):
    t.point('game_text', targetname='one', message='ET_TEXT_ONE')
    t.point('game_text', targetname='all', message='ET_TEXT_ALL', spawnflags='1')
    t.mark('one')
    t.fire('one', activator='player1')
    t.wait(0.2)
    t.mark('all')
    t.fire('all', activator='player1')
    t.wait(0.2)
    t.expect.msg(TE, to='player1', fields=text(text='ET_TEXT_ONE'), count=1, after='one')
    t.expect.msg(TE, to='player2', fields=text(text='ET_TEXT_ONE'), count=0, after='one')
    t.expect.msg(TE, to='player1', fields=text(text='ET_TEXT_ALL'), count=1, after='all')
    t.expect.msg(TE, to='player2', fields=text(text='ET_TEXT_ALL'), count=1, after='all')


@S.test('game_text without All Players, fired with no player as activator: no text, nothing else happens', players=1,
        covers=('game_text',))
def no_activator(t):
    t.point('game_text', targetname='txt', message='ET_TEXT_NOBODY')
    t.point('trigger_relay', targetname='relay', target='txt')
    t.mark('go')
    t.fire('txt')
    t.fire('relay')
    t.wait(0.3)
    t.expect.used('txt', min=2, after='go')
    t.expect.msg(TE, fields=text(text='ET_TEXT_NOBODY'), count=0, after='go')


@S.test('game_text: channel, x / y, effect 2 (Scan Out), color1 / color2, fade in / fade out / hold / scan time as set',
        players=1,
        covers=('game_text.channel=3', 'game_text.x', 'game_text.y', 'game_text.effect=2', 'game_text.color',
                'game_text.color2', 'game_text.fadein', 'game_text.fadeout', 'game_text.holdtime',
                'game_text.fxtime'))
def fields_scan(t):
    t.point('game_text', targetname='txt', message='ET_TEXT_SCAN', channel='3', x='0.25', y='0.75', effect='2',
            color='255 0 0', color2='0 255 0', fadein='0.5', fadeout='1', holdtime='3', fxtime='0.125')
    t.fire('txt', activator='player1')
    t.wait(0.2)
    t.expect.msg(TE, to='player1', count=1, fields=text(
        text='ET_TEXT_SCAN', channel=3, x=0.25, y=0.75, effect=2, color=[255, 0, 0], color2=[0, 255, 0],
        fadein=0.5, fadeout=1.0, hold=3.0, fxtime=0.125))


@S.test('game_text: effect 0 (Fade In/Out) and 1 (Credits) carry no scan time; x / y -1 center; channels 1, 2, 4',
        players=1, covers=('game_text.effect=0', 'game_text.effect=1', 'game_text.channel=1', 'game_text.channel=2',
                           'game_text.channel=4'))
def fields_fade_credits(t):
    t.point('game_text', targetname='fade', message='ET_TEXT_FADE', channel='1', x='-1', y='-1', effect='0',
            fxtime='0.5')
    t.point('game_text', targetname='credits', message='ET_TEXT_CREDITS', channel='2', x='0', y='1', effect='1')
    t.point('game_text', targetname='four', message='ET_TEXT_FOUR', channel='4')
    for n in ('fade', 'credits', 'four'):
        t.fire(n, activator='player1')
    t.wait(0.2)
    t.expect.msg(TE, to='player1', count=1, fields=text(text='ET_TEXT_FADE', channel=1, x=-1, y=-1, effect=0))
    t.expect.msg(TE, to='player1', count=1, fields=text(text='ET_TEXT_CREDITS', channel=2, x=0, y=1, effect=1))
    t.expect.msg(TE, to='player1', count=1, fields=text(text='ET_TEXT_FOUR', channel=4))


@S.test('game_text master: no text while the multisource is locked; the text once it is unlocked', players=1,
        covers=('game_text.master',))
def master(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')
    t.point('game_text', targetname='txt', message='ET_TEXT_MASTER', master='lock', spawnflags='1')
    t.wait(0.3)
    t.mark('locked')
    t.fire('txt', activator='player1')
    t.wait(0.2)
    t.fire('key')
    t.mark('open')
    t.fire('txt', activator='player1')
    t.wait(0.2)
    t.expect.msg(TE, fields=text(text='ET_TEXT_MASTER'), count=0, after='locked', within=(0, 0.15))
    t.expect.msg(TE, to='player1', fields=text(text='ET_TEXT_MASTER'), count=1, after='open')


@S.test('game_text target: the key is ignored (no "target" in the FGD; CGameText::Use never calls SUB_UseTargets, HL '
        'SDK and 1.38 the same); the text still shows', players=1,
        covers=('game_text',))
def target(t):
    t.point('game_text', targetname='txt', message='ET_TEXT_TARGET', target='after', names=('target',))
    t.point('info_target', targetname='after')
    t.fire('txt', activator='player1')
    t.wait(0.2)
    t.expect.msg(TE, to='player1', fields=text(text='ET_TEXT_TARGET'), count=1)
    t.expect.not_used('after')
