"""Co-op scoreboard XP and level (docs/dev/SCOREBOARD_PORT.md "Coop Level/XP columns"): a teamkill / suicide penalty
lowers the level by 5 (m_iSboSpeed -25) and the scoreboard XP (m_iSboTotalXP, the SboXP message) by 5000 per level
lost, down into the negative with no lower limit, so the two columns agree (CCoop::PlayerKilled). The speed a level
gives stops at level -40's (SboLevelSpeed) and the max speed never goes under 80 (SBO_MIN_MAXSPEED), so a player can
always move; the penalty's speed applies at once (CBasePlayer::SboLevelPenalty). The penalty also takes 25 from the
spawn health (m_iSboHealth), never below 10 (CCoop::PlayerKilled, SboKillPenalty)."""

from etest import Suite

SUITE = Suite('xp', 'Co-op scoreboard: XP follows the teamkill / suicide level penalty', players=2)
S = SUITE
DOC = 'dev/SCOREBOARD_PORT.md'


def fresh_round(t):
    t.console('mp_friendlyfire 1')
    t.console('test_round restart')     # both alive again (an earlier test's victim)
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn: m_iSpawnProtectTime)


@S.test('suicide at level 0, XP 0: level -5, XP -25000', players=2, doc=DOC)
def suicide_level0(t):
    fresh_round(t)
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player1')    # killed by itself: the self-kill branch
    t.wait(0.5)
    t.expect.msg('ScoreInfo', fields={'client': 1, 'score': -5}, after='kill', within=(0, 0.5))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': -25000}, after='kill', within=(0, 0.5))


@S.test('teamkill at level 1, XP 5000 (a monster level-up): level -4, XP -20000', players=2, doc=DOC)
def teamkill_level1(t):
    t.point('monster_cyborg', at=(150, 0, 0), targetname='cy')
    fresh_round(t)
    t.mark('levelup')
    t.damage('cy', 5000, 0, 'player2')         # 5000 XP: one level-up (SBO_XP_GAIN stops at 5000)
    t.wait(0.3)
    t.expect.msg('ScoreInfo', fields={'client': 2, 'score': 1}, after='levelup', within=(0, 0.3))
    t.expect.msg('SboXP', fields={'client': 2, 'xp': 5000}, after='levelup', within=(0, 0.3))
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player2')    # player2 kills its teammate
    t.wait(0.5)
    t.expect.msg('ScoreInfo', fields={'client': 2, 'score': -4}, after='kill', within=(0, 0.5))
    t.expect.msg('SboXP', fields={'client': 2, 'xp': -20000}, after='kill', within=(0, 0.5))


# Loadouts (test_join <player> blade light heavy item cyberware armor malefemale; coop.cpp CCoop::PlayerSpawn weights):
# LIGHT: chainsaw 0, rc28 0, federal -20, grenade 0, speed cyberware +50, no armor: level speed + 30.
# HEAVY: warhammer -30, canon -15, chaingun -80, satchel -15, health cyberware, light armor -30: level speed - 170,
# which at level -40 (100) was -70: the player could not move and the client, sent the negative maxspeed as a large
# unsigned number, predicted him walking (rubber-banding).
LIGHT = '2 1 2 1 1 4 1'
HEAVY = '1 3 8 3 4 1 1'


def level_round(t, player, level, loadout=None):
    """Sets the player's level (test_player level) and loadout, then a new round: the spawn builds the speed."""
    if loadout:
        t.console('test_join %s %s' % (player, loadout))
    t.console('test_player level %s %d' % (player, level))
    fresh_round(t)


@S.test('teamkill at level -55: level -60, XP -300000 (no floor at -40)', players=2, doc=DOC)
def teamkill_below_40(t):
    level_round(t, 'player2', -55)
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player2')
    t.wait(0.5)
    t.snap('after', 'player2', 'level', 'total_xp')
    t.expect.msg('ScoreInfo', fields={'client': 2, 'score': -60}, after='kill', within=(0, 0.5))
    t.expect.msg('SboXP', fields={'client': 2, 'xp': -300000}, after='kill', within=(0, 0.5))
    t.expect.value('after', 'player2', 'level', -60)
    t.expect.value('after', 'player2', 'total_xp', -300000)


@S.test('speed at level -60 is the level -40 speed (130 with the light loadout); -39: 135', players=2, doc=DOC)
def speed_below_40(t):
    level_round(t, 'player2', -40, LIGHT)
    t.snap('l40', 'player2', 'maxspeed', 'sbo_maxspeed', 'level')
    level_round(t, 'player2', -60)
    t.snap('l60', 'player2', 'maxspeed', 'sbo_maxspeed', 'level')
    level_round(t, 'player2', -39)
    t.snap('l39', 'player2', 'maxspeed', 'level')
    level_round(t, 'player2', 0, '1 1 1 1 1 1 1')    # back to the suite's default loadout and level
    t.expect.value('l40', 'player2', 'level', -40)
    t.expect.value('l40', 'player2', 'maxspeed', 130)
    t.expect.value('l60', 'player2', 'level', -60)
    t.expect.value('l60', 'player2', 'maxspeed', 130)
    t.expect.value('l60', 'player2', 'sbo_maxspeed', 130)
    t.expect.value('l39', 'player2', 'maxspeed', 135)


@S.test('heavy loadout at level -60: max speed 80, the player walks forward', players=2, doc=DOC)
def walk_below_40(t):
    t.point('info_target', at=(-200, 0, 0), targetname='start')
    level_round(t, 'player2', -60, HEAVY)
    t.teleport('player2', (-200, 0, 0), 0)
    t.wait(0.3)
    t.snap('before', 'player2', 'origin', 'maxspeed')
    t.snap('before', 'start', 'origin')
    t.walk('player2', 400, 0, 1.5)
    t.snap('after', 'player2', 'origin', 'maxspeed')
    t.snap('after', 'start', 'origin')
    level_round(t, 'player2', 0, '1 1 1 1 1 1 1')
    t.expect.value('before', 'player2', 'maxspeed', 80)
    t.expect.distance('before', 'player2', 'start', 0, 2)
    t.expect.distance('after', 'player2', 'start', 60, 200)    # 80 units/s for 1.5 s, less the acceleration


@S.test('teamkill penalty: the speed drops at once, no respawn (light loadout: level 0 330 -> 305; level -38 140 -> 130)',
        players=2, doc=DOC)
def penalty_speed_at_once(t):
    level_round(t, 'player2', 0, LIGHT)
    t.snap('l0', 'player2', 'maxspeed')
    t.mark('kill1')
    t.damage('player1', 5000, 0, 'player2')
    t.wait(0.3)
    t.snap('k1', 'player2', 'maxspeed', 'level', 'deadflag')
    level_round(t, 'player2', -38)
    t.snap('l38', 'player2', 'maxspeed')
    t.damage('player1', 5000, 0, 'player2')
    t.wait(0.3)
    t.snap('k2', 'player2', 'maxspeed', 'level')
    level_round(t, 'player2', 0, '1 1 1 1 1 1 1')
    t.expect.value('l0', 'player2', 'maxspeed', 330)
    t.expect.value('k1', 'player2', 'level', -5)
    t.expect.value('k1', 'player2', 'deadflag', 0)
    t.expect.value('k1', 'player2', 'maxspeed', 305)
    t.expect.value('l38', 'player2', 'maxspeed', 140)
    t.expect.value('k2', 'player2', 'level', -43)
    t.expect.value('k2', 'player2', 'maxspeed', 130)


@S.test('teamkill spawn health: 100 -> 75, 30 -> 10, 36 -> 11, 10 -> 10 (25 less, never below 10)', players=2, doc=DOC)
def penalty_health_floor(t):
    for i, start in enumerate((100, 30, 36, 10)):
        t.console('test_player sbo_health player2 %d' % start)
        fresh_round(t)
        t.damage('player1', 5000, 0, 'player2')
        t.wait(0.3)
        t.snap('h%d' % i, 'player2', 'sbo_health', 'level')
    t.console('test_player sbo_health player2 100')
    level_round(t, 'player2', 0)
    for i, want in enumerate((75, 10, 11, 10)):
        t.expect.value('h%d' % i, 'player2', 'sbo_health', want)


@S.test('a teamkill costs 5 levels, 25000 XP and 25 spawn health', players=2, doc=DOC)
def penalty_xp_on(t):
    t.console('test_player sbo_health player2 100')
    level_round(t, 'player2', 0)
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player2')
    t.wait(0.5)
    t.snap('tk', 'player2', 'level', 'total_xp', 'sbo_health')
    t.console('test_player sbo_health player2 100')
    level_round(t, 'player2', 0)
    t.expect.msg('ScoreInfo', fields={'client': 2, 'score': -5}, after='kill', within=(0, 0.5))
    t.expect.value('tk', 'player2', 'level', -5)
    t.expect.value('tk', 'player2', 'total_xp', -25000)
    t.expect.value('tk', 'player2', 'sbo_health', 75)


# XP for damage and heals (barney.cpp CBarney::TakeDamage, medikit.cpp CMedikit::Swing / SecondaryAttack). These run
# after the tests above, where player1 gained no XP: its SBO_XP is below 5000, so a small gain counts in full.
def medikit_ready(t, player, at, yaw):
    """`player` picks up a medikit at `at`, selects it and faces `yaw` (level 0, scoreboard XP 0)."""
    t.point('weapon_medikit', at=at)
    t.console('test_player level %s 0' % t.ref(player))
    fresh_round(t)
    t.teleport(player, at, yaw)
    t.wait(0.5)
    t.cmd(player, 'weapon_medikit')
    t.wait(1.5)     # deploy
    t.look(player, 0, yaw)
    t.wait(0.2)


@S.test('monster_barney: 10 damage from a player gives 20 XP (2 per point)', players=2, doc=DOC)
def barney_damage_xp(t):
    t.point('monster_barney', at=(150, 0, 0), targetname='ba')
    t.console('test_player level %s 0' % t.ref('player1'))
    fresh_round(t)
    t.damage('ba', 10, 0, 'player1')
    t.wait(0.3)
    t.snap('hit', 'player1', 'total_xp')
    t.snap('hit', 'ba', 'health', 'deadflag')
    t.damage('ba', 5000, 0)     # no attacker: no XP; the barney cannot shoot into the next tests
    t.expect.value('hit', 'ba', 'deadflag', 0)
    t.expect.value('hit', 'player1', 'total_xp', 20)


@S.test('medikit: a swing that hits the wall gives no XP', players=2, doc=DOC)
def medikit_wall_no_xp(t):
    medikit_ready(t, 'player1', (0, 230, 0), 90)    # 26 units from the north wall, facing it (the reach is 32)
    t.snap('before', 'player1', 'total_xp')
    t.mark('swing')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.5)
    t.snap('after', 'player1', 'total_xp')
    t.expect.sound('medshot4', ent='player1', after='swing', within=(0, 0.3))     # it hit (the wall sound)
    t.expect.value('before', 'player1', 'total_xp', 0)
    t.expect.value('after', 'player1', 'total_xp', 0)


@S.test('medikit: healing a hurt teammate (50 health) gives 500-1000 XP', players=2, doc=DOC)
def medikit_heal_xp(t):
    medikit_ready(t, 'player1', (-40, 0, 0), 0)     # 8 units between the two hulls, player2 in reach
    t.teleport('player2', (0, 0, 0), 180)
    t.kv('player2', 'health', '50')
    t.wait(0.3)
    t.snap('before', 'player2', 'health')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.5)
    t.snap('after', 'player1', 'total_xp')
    t.snap('after', 'player2', 'health')
    t.expect.value('before', 'player2', 'health', 50)
    t.expect.value('after', 'player2', 'health', 50, op='>')
    t.expect.value('after', 'player1', 'total_xp', 500, op='>=')
    t.expect.value('after', 'player1', 'total_xp', 1000, op='<=')


# The scoreboard XP keeps to the level (multiplay_gamerules.cpp SboScoreboard_KeepXPToLevel; docs/dev/SCOREBOARD_PORT.md
# "Negative levels"): level L shows at least L * 5000 XP, and below level 0 less than (L + 1) * 5000, so a negative
# level never shows next to positive XP and a level of 0 or more never next to negative XP. Out of that range the XP
# becomes the level's start plus the XP toward the next level (m_iXP, at most 4999). The scoreboard XP is the XP
# gained this map; it leaves the range on these paths, each tested below: a medikit level-up takes 7500 XP (the
# penalty gives back 5000 per level), the XP toward the next level halves at a death while the scoreboard XP keeps
# it, game_player_settings takes levels from a player whose XP is ahead of his level, and a plugin changes the level
# speed m_iSboSpeed alone (the sentry guns' friendly-fire penalty).
def xp_settings(t, player, value, name):
    """game_player_settings "xp" (the XP toward the next level, m_iXP) for `player`; the scoreboard XP moves by the
    same amount (maprules.cpp PlayerSettings_ChangeXP)."""
    t.point('game_player_settings', targetname=name, xp=value)
    t.fire(name, activator=player)
    t.wait(0.2)


def medikit_levels(t, player, n):
    """`n` medikit level-ups, 7500 XP each, healing himself (secondary attack): the XP toward the next level is set
    to 7499 (test_player xp: no level-up, the scoreboard XP moves by the same amount), then a heal of 500-1000 stops
    at 7500 (SBO_XP_GAIN) and levels up (CMedikit::SecondaryAttack)."""
    for _ in range(n):
        t.console('test_player xp %s 7499' % t.ref(player))
        t.wait(0.1)
        t.kv(player, 'health', '10')
        t.move(player, 0, 0, 3, '+attack2')
        t.wait(8.5)     # the medikit's heal delay (CMedikit::SecondaryAttack: 4 s; a fake client's weapon clock runs slow)


def medikit_self_ready(t, player, level):
    """`player` at `level` (scoreboard XP 5000 per level, XP toward the next level 0) with the medikit out."""
    t.point('weapon_medikit', at=(-40, 0, 0))
    t.point('game_player_settings', targetname='mk0', xp='=0')
    fresh_round(t)
    t.fire('mk0', activator=player)
    t.wait(0.1)
    t.console('test_player level %s %d' % (t.ref(player), level))
    t.teleport(player, (-40, 0, 0), 0)
    t.wait(0.5)
    t.cmd(player, 'weapon_medikit')
    t.wait(1.5)     # deploy


@S.test('four medikit level-ups (30000 XP), then a suicide: level -1 shows XP -5000, not +5000', players=2, doc=DOC)
def medikit_levels_then_suicide(t):
    medikit_self_ready(t, 'player1', 0)
    medikit_levels(t, 'player1', 4)
    t.snap('levels', 'player1', 'level', 'total_xp')
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player1')
    t.wait(0.5)
    t.snap('after', 'player1', 'level', 'total_xp')
    t.console('test_player level %s 0' % t.ref('player1'))
    t.expect.value('levels', 'player1', 'level', 4)
    t.expect.value('levels', 'player1', 'total_xp', 30000)
    t.expect.value('after', 'player1', 'level', -1)
    t.expect.value('after', 'player1', 'total_xp', -5000)
    t.expect.msg('ScoreInfo', fields={'client': 1, 'score': -1}, after='kill', within=(0, 0.5))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': -5000}, after='kill', within=(0, 0.5))
    t.expect.msg('SboXP', fields={'client': 1, 'xp': 5000}, after='kill', within=(0, 0.5), min=0, count=0)


@S.test('four medikit level-ups from level -5 (XP -25000): level -1 shows XP -5000, not +5000', players=2, doc=DOC)
def medikit_levels_below_zero(t):
    medikit_self_ready(t, 'player1', -5)
    medikit_levels(t, 'player1', 4)
    t.snap('levels', 'player1', 'level', 'total_xp', 'xp')
    t.console('test_player level %s 0' % t.ref('player1'))
    t.expect.value('levels', 'player1', 'level', -1)
    t.expect.value('levels', 'player1', 'xp', 0)
    t.expect.value('levels', 'player1', 'total_xp', -5000)


@S.test('a death halves the XP toward the next level, the scoreboard XP keeps it: at level -1 it stops at -1 '
        '(was +2499)', players=2, doc=DOC)
def death_halving_below_zero(t):
    t.point('game_player_settings', targetname='dh0', xp='=0')
    t.point('game_player_settings', targetname='dh4999', xp='=4999')
    fresh_round(t)
    t.fire('dh0', activator='player2')
    t.wait(0.1)
    t.console('test_player level player2 -1')      # XP -5000
    t.wait(0.1)
    t.fire('dh4999', activator='player2')          # XP -1: one short of level 0
    t.wait(0.2)
    t.snap('first', 'player2', 'level', 'total_xp', 'xp')
    t.damage('player2', 5000, 0)                   # killed by the world: no penalty, the XP toward the level halves
    t.wait(0.3)
    t.snap('dead', 'player2', 'level', 'total_xp', 'xp')
    fresh_round(t)
    t.fire('dh4999', activator='player2')          # 2500 more: the scoreboard XP would be +2499 at level -1
    t.wait(0.2)
    t.snap('again', 'player2', 'level', 'total_xp', 'xp')
    t.console('test_player level player2 0')
    t.expect.value('first', 'player2', 'total_xp', -1)
    t.expect.value('dead', 'player2', 'xp', 2499)
    t.expect.value('dead', 'player2', 'total_xp', -1)
    t.expect.value('again', 'player2', 'level', -1)
    t.expect.value('again', 'player2', 'xp', 4999)
    t.expect.value('again', 'player2', 'total_xp', -1)


@S.test('game_player_settings level -3 from level 2 with XP 22000 (XP ahead): level -1, XP -1, not +7000',
        players=2, doc=DOC, covers=('game_player_settings.level',))
def settings_level_down_xp_ahead(t):
    t.point('game_player_settings', targetname='sl0', xp='=0')
    t.point('game_player_settings', targetname='slm3', level='-3')
    fresh_round(t)
    t.fire('sl0', activator='player2')
    t.wait(0.1)
    t.console('test_player level player2 2')       # XP 10000
    t.console('test_player xp player2 12000')      # no level-up: XP 22000 at level 2
    t.wait(0.2)
    t.snap('ahead', 'player2', 'level', 'total_xp')
    t.mark('down')
    t.fire('slm3', activator='player2')
    t.wait(0.2)
    t.snap('down', 'player2', 'level', 'total_xp')
    t.fire('sl0', activator='player2')
    t.console('test_player level player2 0')
    t.expect.value('ahead', 'player2', 'level', 2)
    t.expect.value('ahead', 'player2', 'total_xp', 22000)
    t.expect.value('down', 'player2', 'level', -1)
    t.expect.value('down', 'player2', 'total_xp', -1)
    t.expect.msg('SboXP', fields={'client': 2, 'xp': -1}, after='down', within=(0, 0.1))


@S.test('a plugin writes the level speed alone (sentry guns friendly fire): level 2 -> -3 shows XP -15000, back to '
        '2 shows 10000', players=2, doc=DOC)
def plugin_level_alone(t):
    t.point('game_player_settings', targetname='pl0', xp='=0')
    fresh_round(t)
    t.fire('pl0', activator='player2')
    t.wait(0.1)
    t.console('test_player level player2 2')       # XP 10000
    t.wait(0.2)
    t.mark('down')
    t.console('test_player sbo_speed player2 285')     # level -3, the scoreboard XP not touched
    t.wait(0.2)
    t.snap('down', 'player2', 'level', 'total_xp')
    t.mark('up')
    t.console('test_player sbo_speed player2 310')     # level 2
    t.wait(0.2)
    t.snap('up', 'player2', 'level', 'total_xp')
    t.console('test_player level player2 0')
    t.expect.value('down', 'player2', 'level', -3)
    t.expect.value('down', 'player2', 'total_xp', -15000)
    t.expect.msg('ScoreInfo', fields={'client': 2, 'score': -3}, after='down', within=(0, 0.2))
    t.expect.msg('SboXP', fields={'client': 2, 'xp': -15000}, after='down', within=(0, 0.2))
    t.expect.value('up', 'player2', 'level', 2)
    t.expect.value('up', 'player2', 'total_xp', 10000)
    t.expect.msg('SboXP', fields={'client': 2, 'xp': 10000}, after='up', within=(0, 0.2))


@S.test('a player who leaves at level -5 (XP -25000) and joins again starts at level 0, XP 0 (InitHUD)', players=2,
        doc=DOC)
def rejoin_resets_both(t):
    # the joining player is player3 to the console (client 3 in the messages); before its first spawn it has no
    # "player" classname, so the test reads the scoreboard messages, not snapshots
    t.console('test_player add rejoin')
    t.wait(1.5)
    t.mark('level')
    t.console('test_player level player3 -5')
    t.wait(0.2)
    t.console('test_player remove player3')
    t.wait(0.5)
    t.mark('join')
    t.console('test_player add rejoin')
    t.wait(1.5)
    t.console('test_player remove player3')
    t.expect.msg('ScoreInfo', fields={'client': 3, 'score': -5}, after='level', within=(0, 0.2))
    t.expect.msg('SboXP', fields={'client': 3, 'xp': -25000}, after='level', within=(0, 0.2))
    t.expect.msg('ScoreInfo', fields={'client': 3, 'score': 0}, after='join', within=(0, 1.5))
    t.expect.msg('SboXP', fields={'client': 3, 'xp': 0}, after='join', within=(0, 1.5))
    t.expect.msg('ScoreInfo', fields={'client': 3, 'score': -5}, after='join', within=(0, 1.5), min=0, count=0)
