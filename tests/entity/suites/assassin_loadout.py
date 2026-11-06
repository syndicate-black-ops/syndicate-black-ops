"""Leaving the Assassin cyberware (co-op): the loadout menu sends blade, light, heavy, item, cyberware, armor and
malefemale in that order (CMenuHandler_Go, CSboBriefMenu / CSboLoadoutMenu). For a dead or waiting player the "item"
step joins (CCoop::ClientCommand); the join now waits until the commands after it are in (CBasePlayer::LoadoutJoin
from UpdateClientData), so the spawn sees the new cyberware. SBO joined at "item", while the cyberware saved was
still Assassin's: the Assassin spawn cleared the light, heavy and item picks and the next round had the blade only.
The Bounty Killer side is suite assassin_loadout_dm."""

from etest import Suite

SUITE = Suite('assassin_loadout', 'Co-op: leaving Assassin from the loadout menu keeps the new loadout', players=2)
S = SUITE
DOC = 'LOADOUT_MENU.md'

# test_join <player> blade light heavy item cyberware armor malefemale (the menu's order)
ASSASSIN = '2 1 2 1 5 4 1'     # chainsaw, rc28, federal, grenade, Assassin, no armor
NORMAL = '2 1 2 1 1 4 1'       # the same with the speed legs

# pev->weapons bits (weapons.h)
RC28 = 1 << 12
FEDERAL = 1 << 17
HANDGRENADE = 1 << 3
SATCHEL = 1 << 5
K444 = 1 << 13
MEDIKIT = 1 << 20
PM66 = 1 << 18
NM33 = 1 << 16
TRIPMINE = 1 << 4
ASSASSIN_KIT = K444 | MEDIKIT | HANDGRENADE | SATCHEL


def new_round(t):
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn: m_iSpawnProtectTime)


def die_and_choose(t, loadout):
    """player1 dies (player2 keeps the round going), is a waiting observer, then confirms the menu with `loadout`."""
    t.damage('player1', 5000, 0)
    t.wait(3.0)     # the death animation, then CCoop::FPlayerCanRespawn: an observer at a spawn point
    t.snap('dead', 'player1', 'deadflag')
    t.console('test_join player1 %s' % loadout)
    t.wait(0.5)


@S.test('Assassin dies, the menu picks the legs: the next round has the rc28, the federal and the grenade',
        players=2, doc=DOC)
def leave_assassin(t):
    t.console('mp_XP 0')
    t.console('test_join player1 %s' % ASSASSIN)
    new_round(t)
    t.snap('assassin', 'player1', 'weapons')
    die_and_choose(t, NORMAL)
    new_round(t)
    t.snap('next', 'player1', 'weapons', 'health')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('assassin', 'player1', 'weapons', ASSASSIN_KIT, op='&')
    t.expect.value('assassin', 'player1', 'weapons', RC28 | FEDERAL, op='!&')
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('next', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('next', 'player1', 'weapons', K444 | SATCHEL, op='!&')


@S.test('new menu: Remove Assassin, then Deploy (its earlier pm66, nm33, tripmine picks): the next round has them',
        players=2, doc=DOC)
def leave_assassin_new_menu(t):
    # CSboLoadoutMenu::Deploy sends every slot's m_Sel + 1; "Remove Assassin" is Pick(SLOT_CYBER, 0) (cyberware 1),
    # the sidearm / primary / equipment picks it dimmed are still the ones made before Assassin
    t.console('mp_XP 0')
    t.console('test_join player1 %s' % ASSASSIN)
    new_round(t)
    die_and_choose(t, '2 4 4 2 1 4 1')     # chainsaw, pm66, nm33, tripmine, legs, no armor
    new_round(t)
    t.snap('next', 'player1', 'weapons')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('next', 'player1', 'weapons', PM66 | NM33 | TRIPMINE, op='&')
    t.expect.value('next', 'player1', 'weapons', K444 | SATCHEL, op='!&')


@S.test('timing: a dead co-op player is a waiting observer about a second after dying, until the next round',
        players=2, doc=DOC)
def dead_observer_timing(t):
    t.console('mp_XP 0')
    new_round(t)
    t.damage('player1', 5000, 0)
    for i, s in enumerate((0.3, 0.7, 1.0, 1.0, 2.0, 5.0)):
        t.wait(s)
        t.snap('d%d' % i, 'player1', 'deadflag', 'health', 'movetype')
    new_round(t)
    t.expect.value('d5', 'player1', 'deadflag', 0, op='>')


@S.test('a dead player picks Assassin from the menu: the next round has the Assassin kit', players=2, doc=DOC)
def pick_assassin(t):
    t.console('mp_XP 0')
    t.console('test_join player1 %s' % NORMAL)
    new_round(t)
    t.snap('normal', 'player1', 'weapons')
    die_and_choose(t, ASSASSIN)
    new_round(t)
    t.snap('next', 'player1', 'weapons', 'armorvalue')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('normal', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('next', 'player1', 'weapons', ASSASSIN_KIT, op='&')
    t.expect.value('next', 'player1', 'weapons', RC28 | FEDERAL, op='!&')
    t.expect.value('next', 'player1', 'armorvalue', 50)
