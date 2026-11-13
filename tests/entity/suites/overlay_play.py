"""env_overlay and play (docs/ENV_OVERLAY.md): only players in play get overlays. Spectators, dead players and Game
Masters never get one, a player who leaves play loses his at once, a player who enters play gets the ones that are
on for every player where they are now, Start On. Its own suite: players die, spectate and become Game Master, and
a Start On overlay (channel 8) is on for the whole suite."""

from etest import Suite

SUITE = Suite('overlay_play', 'env_overlay: players in play only (spectators, dead, GM, late joiners)', players=3,
              requires=('Overlay_StartFrame', 'GmMode_RoundStart'))
S = SUITE
DOC = 'ENV_OVERLAY.md'
SPR = 'sprites/640_logo.spr'


@S.test('Start On (lasting, channel 8): players got it when they entered play; a fullupdate resends it', players=1,
        doc=DOC, covers=('env_overlay', 'env_overlay:4', 'env_overlay.channel=8'))
def start_on(t):
    t.point('env_overlay', targetname='starton', spawnflags='4', sprite=SPR, channel='8', fadein='0.5')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.3)
    t.expect.msg('Overlay', to='player1', count=1, after='fu',
                 fields={'channel': 8, 'on': 1, 'stay': 1, 'elapsed': {'range': (0.3, 600)}})


@S.test('activator mode: a dead activator gets nothing; All Players: the living get it, the dead do not', players=2,
        doc=DOC, covers=('env_overlay', 'env_overlay:1'))
def dead(t):
    t.point('env_overlay', targetname='one', sprite=SPR, channel='2', holdtime='3')
    t.point('env_overlay', targetname='all', spawnflags='1', sprite=SPR, channel='3', holdtime='3')
    t.wait(2.5)     # spawn protection (2 s) from the round start is over
    t.mark('die')
    t.damage('player2', 1000)
    t.wait(0.5)
    t.mark('go')
    t.fire('one', activator='player2')
    t.fire('all')
    t.wait(0.3)
    t.expect.msg('Overlay', to='player2', count=1, after='die', within=(0, 0.3), fields={'channel': 8, 'on': 0, 'now': 1})
    t.expect.no_msg('Overlay', to='player2', after='go', within=(0, 0.25))
    t.expect.msg('Overlay', to='player1', count=1, after='go', within=(0, 0.25), fields={'channel': 3, 'on': 1})
    # a new round puts him back in play: he gets the Start On overlay again (entering play)
    t.mark('spawn')
    t.console('test_round restart')
    t.wait(2.5)     # spawn protection (2 s) is over before the next test hurts anyone
    t.expect.msg('Overlay', to='player2', min=1, after='spawn', fields={'channel': 8, 'on': 1})


@S.test('leaving play: a player who dies loses his overlays at once (off, no fade), the Start On one too', players=1,
        doc=DOC, covers=('env_overlay',))
def leave_play(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, channel='5', fadeout='2')
    t.fire('ov', activator='player1')
    t.wait(0.3)
    t.mark('die')
    t.damage('player1', 1000)
    t.wait(0.3)
    t.expect.msg('Overlay', to='player1', count=1, after='die', fields={'channel': 5, 'on': 0, 'now': 1})
    t.expect.msg('Overlay', to='player1', count=1, after='die', fields={'channel': 8, 'on': 0, 'now': 1})
    t.console('test_round restart')
    t.wait(2.5)     # spawn protection (2 s) is over before the next test hurts anyone


@S.test('entering play (a new round): a player back in play gets the All Players overlays that are on, where they are now (a timed '
        'one with its elapsed time); an activator overlay is not given back', players=2, doc=DOC,
        covers=('env_overlay', 'env_overlay:1', 'env_overlay.holdtime'))
def enter_play(t):
    t.point('env_overlay', targetname='all', spawnflags='1', sprite=SPR, channel='6', holdtime='20')
    t.point('env_overlay', targetname='one', sprite=SPR, channel='7')
    t.fire('one', activator='player2')
    t.damage('player2', 1000)
    t.wait(0.3)
    t.fire('all')
    t.wait(1.0)
    t.mark('spawn')
    t.console('test_round restart')
    t.wait(2.5)     # spawn protection (2 s) is over before the next test hurts anyone
    t.expect.msg('Overlay', to='player2', min=1, after='spawn',
                 fields={'channel': 6, 'on': 1, 'elapsed': {'range': (0.8, 2.5)}})
    t.expect.msg('Overlay', to='player2', count=0, after='spawn', fields={'channel': 7, 'on': 1})
    t.fire('all', use='off')
    t.wait(0.1)


@S.test('spectator: never gets one (All Players, or as the activator)', players=3, doc=DOC,
        covers=('env_overlay', 'env_overlay:1'))
def spectator(t):
    t.point('env_overlay', targetname='all', spawnflags='1', sprite=SPR, channel='2', holdtime='1')
    t.point('env_overlay', targetname='one', sprite=SPR, channel='3', holdtime='1')
    t.cmd('player3', 'spectate')
    t.wait(1.0)
    t.mark('go')
    t.fire('all')
    t.fire('one', activator='player3')
    t.wait(0.3)
    t.expect.no_msg('Overlay', to='player3', after='go')
    t.expect.msg('Overlay', to='player1', count=1, after='go', fields={'channel': 2, 'on': 1})
    t.expect.msg('Overlay', to='player2', count=1, after='go', fields={'channel': 2, 'on': 1})


@S.test('Game Master: never gets one (All Players); becoming GM drops what he had', players=2, doc=DOC,
        covers=('env_overlay', 'env_overlay:1'))
def game_master(t):
    t.console('mp_gmmode 1')
    t.point('env_overlay', targetname='all', spawnflags='1', sprite=SPR, channel='4')
    t.fire('all')
    t.wait(0.3)
    t.mark('gm')
    t.cmd('player1', 'gmon')
    t.wait(0.3)
    t.cmd('player1', 'gm')
    t.wait(0.5)
    t.mark('go')
    t.fire('all', use='off')
    t.wait(0.1)
    t.fire('all', use='on')
    t.wait(0.3)
    t.expect.msg('Overlay', to='player1', count=1, after='gm', fields={'channel': 4, 'on': 0, 'now': 1})
    t.expect.msg('Overlay', to='player1', count=0, after='go', fields={'channel': 4, 'on': 1})
    t.expect.msg('Overlay', to='player2', count=1, after='go', fields={'channel': 4, 'on': 1})
