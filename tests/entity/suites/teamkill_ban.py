"""Co-op teamkiller ban (mp_ban 1): a player who killed 5 or more other players on the map (CBasePlayer::m_iTeamKills,
counted in CCoop::PlayerKilled, cleared at InitHUD) spawns with 3 health and no weapons, not even the fists, and the
loadout picks cleared (CCoop::PlayerSpawn). Four kills are not enough; suicides do not count."""

from etest import Suite

SUITE = Suite('teamkill_ban', 'Co-op: mp_ban 1 bans a player after 5 teamkills', players=2)
S = SUITE
DOC = 'ROUNDS.md'

LOADOUT = '2 1 2 1 1 4 1'      # chainsaw, rc28, federal, grenade, speed legs, no armor
RC28 = 1 << 12
WONG = 1 << 21                 # the fists (weapon_wong, WEAPON_WONG 21)
ALL = 0x7FFFFFFF               # every bit of pev->weapons but the suit (WEAPON_SUIT 31)


def new_round(t):
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn: m_iSpawnProtectTime)


def teamkill(t):
    """player2 kills player1 (a player kill: CCoop::PlayerKilled counts it for player2), then a new round."""
    t.damage('player1', 5000, 0, 'player2')
    t.wait(0.5)
    new_round(t)


@S.test('mp_ban 1: four teamkills keep the loadout; the fifth gives 3 health, no weapons (no fists), no loadout; '
        'suicides do not count', players=2, doc=DOC)
def ban_after_five(t):
    t.console('mp_friendlyfire 1')
    t.console('mp_ban 1')
    t.console('test_join player2 %s' % LOADOUT)
    new_round(t)
    t.damage('player2', 5000, 0, 'player2')      # a suicide: not a teamkill
    t.wait(0.5)
    new_round(t)
    for _ in range(4):
        teamkill(t)
    t.snap('four', 'player2', 'health', 'weapons')
    teamkill(t)
    t.snap('five', 'player2', 'health', 'weapons')
    t.console('mp_ban 0')
    t.console('test_join player2 %s' % LOADOUT)
    new_round(t)
    t.snap('off', 'player2', 'health', 'weapons')
    t.expect.value('four', 'player2', 'weapons', RC28 | WONG, op='&')
    t.expect.value('four', 'player2', 'health', 3, op='>')
    t.expect.value('five', 'player2', 'health', 3)
    t.expect.value('five', 'player2', 'weapons', ALL, op='!&')
    t.expect.value('off', 'player2', 'weapons', RC28 | WONG, op='&')
