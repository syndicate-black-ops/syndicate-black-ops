"""game_player_settings co-op level and XP keys (docs/FEATUREFUL_GUIDE_ENTITIES.md "game_player_settings"): "level"
and "xp" with the health / armor prefixes (= set, + add, - take, < at most, > at least; no prefix sets). A level
change works as level-ups / the teamkill penalty do (level speed, spawn health, the speed at once) and moves the
scoreboard XP by 5000 per level; "xp" is the XP toward the next level (m_iXP): with mp_XP 1 each full 5000 is a
level-up, taking stops at 0. ScoreInfo (the level) and SboXP (the scoreboard XP) are sent at once."""

from etest import Suite

SUITE = Suite('player_settings_xp', 'game_player_settings: co-op level and XP keys', players=2)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'


def start(t, level=0, xp=1):
    t.console('mp_XP %d' % xp)
    t.point('game_player_settings', targetname='xp0', xp='=0')
    t.fire('xp0', activator='player1')     # XP toward the next level 0 (an earlier test's left over)
    t.wait(0.2)
    t.console('test_player level player1 %d' % level)   # then the level and scoreboard XP (5000 per level)
    t.wait(0.2)


def step(t, label, name, **kv):
    t.point('game_player_settings', targetname=name, **kv)
    t.mark(label)
    t.fire(name, activator='player1')
    t.wait(0.2)
    t.snap(label, 'player1', 'level', 'total_xp', 'xp', 'sbo_health', 'sbo_maxspeed', 'maxspeed')


@S.test('level: =5, +2, -3, <2 (at most), >3 (at least), =-4, 6 (no prefix sets); scoreboard XP 5000 per level, '
        'ScoreInfo and SboXP at once', players=2, doc=DOC, covers=('game_player_settings.level',))
def level_ops(t):
    start(t)
    cases = (('set5', '=5', 5), ('add2', '+2', 7), ('take3', '-3', 4), ('atmost2', '<2', 2), ('atmost9', '<9', 2),
             ('atleast3', '>3', 3), ('atleast1', '>1', 3), ('setneg', '=-4', -4), ('plain', '6', 6))
    for label, value, level in cases:
        step(t, label, 'l_' + label, level=value)
    for label, value, level in cases:
        t.expect.value(label, 'player1', 'level', level)
        t.expect.value(label, 'player1', 'total_xp', level * 5000)
    t.expect.msg('ScoreInfo', fields={'client': 1, 'score': 5}, after='set5', within=(0, 0.05))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': 25000}, after='set5', within=(0, 0.05))
    t.expect.msg('ScoreInfo', fields={'client': 1, 'score': -4}, after='setneg', within=(0, 0.05))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': -20000}, after='setneg', within=(0, 0.05))
    t.expect.no_msg('ScoreInfo', after='atmost9', within=(0, 0.15))     # no change: nothing sent


@S.test('level: speed at once and spawn health follow: +2 is +20 speed and +4 spawn health, then -3 is -15 '
        'speed and -6 spawn health', players=2, doc=DOC, covers=('game_player_settings.level',))
def level_speed_health(t):
    t.console('mp_XP 1')
    t.console('test_join player1 2 1 2 1 1 4 1')    # the xp suite's LIGHT loadout: level speed + 30
    t.console('test_player level player1 0')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)
    t.console('test_player sbo_health player1 100')
    t.wait(0.2)
    step(t, 'up', 'l_up', level='+2')
    step(t, 'down', 'l_down', level='-3')
    t.expect.value('up', 'player1', 'level', 2)
    t.expect.value('up', 'player1', 'maxspeed', 350)
    t.expect.value('up', 'player1', 'sbo_health', 104)
    t.expect.value('down', 'player1', 'level', -1)
    t.expect.value('down', 'player1', 'maxspeed', 335)
    t.expect.value('down', 'player1', 'sbo_health', 98)


@S.test('xp: =3000, +1000, -1500 (take), <1000 (at most), >2000 (at least), 500 (no prefix sets), -9999 stops at 0; '
        'scoreboard XP moves by the same amount', players=2, doc=DOC, covers=('game_player_settings.xp',))
def xp_ops(t):
    start(t)
    cases = (('set', '=3000', 3000, 3000), ('add', '+1000', 4000, 4000), ('take', '-1500', 2500, 2500),
             ('atmost', '<1000', 1000, 1000), ('atleast', '>2000', 2000, 2000), ('plain', '500', 500, 500),
             ('floor', '-9999', 0, 0))
    for label, value, xp, total in cases:
        step(t, label, 'x_' + label, xp=value)
    for label, value, xp, total in cases:
        t.expect.value(label, 'player1', 'xp', xp)
        t.expect.value(label, 'player1', 'level', 0)
        t.expect.value(label, 'player1', 'total_xp', total)
    t.expect.msg('SboXP', fields={'client': 1, 'xp': 3000}, after='set', within=(0, 0.05))


@S.test('xp with mp_XP 1: +12000 at 0 XP is two level-ups and 2000 XP (scoreboard XP +12000, ScoreInfo level 2); '
        '=5000 one more', players=2, doc=DOC, covers=('game_player_settings.xp',))
def xp_levelup(t):
    start(t)
    step(t, 'gain', 'x_gain', xp='+12000')
    step(t, 'exact', 'x_exact', xp='=5000')
    t.expect.value('gain', 'player1', 'level', 2)
    t.expect.value('gain', 'player1', 'xp', 2000)
    t.expect.value('gain', 'player1', 'total_xp', 12000)
    t.expect.msg('ScoreInfo', fields={'client': 1, 'score': 2}, after='gain', within=(0, 0.05))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': 12000}, after='gain', within=(0, 0.05))
    t.expect.value('exact', 'player1', 'level', 3)
    t.expect.value('exact', 'player1', 'xp', 0)
    t.expect.value('exact', 'player1', 'total_xp', 15000)


@S.test('xp with mp_XP 0: +12000 is 12000 XP, no level-up', players=2, doc=DOC, covers=('game_player_settings.xp',))
def xp_no_levelup(t):
    start(t, xp=0)
    step(t, 'gain', 'x_gain0', xp='+12000')
    t.expect.value('gain', 'player1', 'level', 0)
    t.expect.value('gain', 'player1', 'xp', 12000)
    t.expect.value('gain', 'player1', 'total_xp', 12000)


@S.test('level then xp in one entity: =1 and +6000 gives level 2, 1000 XP', players=2, doc=DOC,
        covers=('game_player_settings.level', 'game_player_settings.xp'))
def level_and_xp(t):
    start(t, level=4)
    step(t, 'both', 'lx', level='=1', xp='+6000')
    t.expect.value('both', 'player1', 'level', 2)
    t.expect.value('both', 'player1', 'xp', 1000)
    t.expect.value('both', 'player1', 'total_xp', 5000 + 6000)


@S.test('a dead player (not in play) is not changed', players=2, doc=DOC, covers=('game_player_settings.level',))
def spectator_ignored(t):
    start(t)
    t.point('game_player_settings', targetname='spec', level='=9', xp='=100')
    t.damage('player1', 5000, 0, 'player1')     # dead: not in play (Logic_IsPlayerInPlay)
    t.wait(0.5)
    t.mark('dead')
    t.fire('spec', activator='player1')
    t.wait(0.2)
    t.snap('dead', 'player1', 'level', 'xp')
    t.expect.value('dead', 'player1', 'level', 9, op='!=')
    t.expect.value('dead', 'player1', 'xp', 100, op='!=')
