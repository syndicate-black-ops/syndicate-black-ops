"""Leaving the Assassin cyberware in Bounty Killer (the co-op side and the cause: suite assassin_loadout). A dead
player in the death camera (mp_forcerespawn 0: StartDeathCam after 6 s) who confirms the loadout menu joins at once
(CDm::PlayerJoins), now once all the menu's commands are in (CBasePlayer::LoadoutJoin): the spawn has the new
loadout. SBO spawned the player at the "item" step with Assassin's cyberware: one more Assassin life, then the blade
only. The map is dm_et_assassin_loadout_dm (deathmatch game rules)."""

from etest import Suite

SUITE = Suite('assassin_loadout_dm', 'Bounty Killer: leaving Assassin from the loadout menu, no extra Assassin life',
              players=2)
S = SUITE
S.map_prefix = 'dm_'
DOC = 'LOADOUT_MENU.md'

ASSASSIN = '2 1 2 1 5 4 1'     # chainsaw, rc28, federal, grenade, Assassin, no armor
NORMAL = '2 1 2 1 1 4 1'       # the same with the speed legs

RC28 = 1 << 12
FEDERAL = 1 << 17
HANDGRENADE = 1 << 3
SATCHEL = 1 << 5
K444 = 1 << 13
MEDIKIT = 1 << 20
ASSASSIN_KIT = K444 | MEDIKIT | HANDGRENADE | SATCHEL


def spawn_with(t, loadout):
    t.console('test_join player1 %s' % loadout)
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(1.5)     # past the 1 s spawn protection (CDm::PlayerSpawn: m_iSpawnProtectTime)


def die_and_choose(t, loadout):
    """player1 dies and waits in the death camera, then confirms the menu with `loadout`: spawns at once."""
    t.damage('player1', 5000, 0)
    t.wait(8.0)     # the death animation, DEAD_RESPAWNABLE, the death camera 6 s later (no forced respawn)
    t.snap('dead', 'player1', 'deadflag')
    t.console('test_join player1 %s' % loadout)
    t.wait(1.0)


@S.test('Assassin dies, the menu picks the legs: the spawn has the rc28, the federal and the grenade, no Assassin kit',
        players=2, doc=DOC)
def leave_assassin_dm(t):
    t.console('mp_forcerespawn 0')
    spawn_with(t, ASSASSIN)
    t.snap('assassin', 'player1', 'weapons')
    die_and_choose(t, NORMAL)
    t.snap('next', 'player1', 'weapons', 'deadflag')
    t.console('mp_forcerespawn 1')
    t.wait(2.0)     # past the spawn protection
    t.damage('player1', 5000, 0)     # and the life after it (forced respawn)
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap('second', 'player1', 'weapons')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('second', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('assassin', 'player1', 'weapons', ASSASSIN_KIT, op='&')
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('next', 'player1', 'deadflag', 0)
    t.expect.value('next', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('next', 'player1', 'weapons', K444 | SATCHEL, op='!&')


@S.test('default mp_forcerespawn 1: the menu while dead (before the forced respawn): the respawn has the new loadout',
        players=2, doc=DOC)
def leave_assassin_forced_respawn_dm(t):
    spawn_with(t, ASSASSIN)
    t.damage('player1', 5000, 0)
    t.wait(2.0)
    t.snap('dead', 'player1', 'deadflag')
    t.console('test_join player1 %s' % NORMAL)
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap('next', 'player1', 'weapons')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('next', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('next', 'player1', 'weapons', K444 | SATCHEL, op='!&')


@S.test('a dead player picks Assassin from the menu: the spawn has the Assassin kit', players=2, doc=DOC)
def pick_assassin_dm(t):
    t.console('mp_forcerespawn 0')
    spawn_with(t, NORMAL)
    t.snap('normal', 'player1', 'weapons')
    die_and_choose(t, ASSASSIN)
    t.snap('next', 'player1', 'weapons', 'deadflag', 'armorvalue')
    t.console('mp_forcerespawn 1')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('normal', 'player1', 'weapons', RC28 | FEDERAL | HANDGRENADE, op='&')
    t.expect.value('next', 'player1', 'deadflag', 0)
    t.expect.value('next', 'player1', 'weapons', ASSASSIN_KIT, op='&')
    t.expect.value('next', 'player1', 'weapons', RC28 | FEDERAL, op='!&')
    t.expect.value('next', 'player1', 'armorvalue', 50)
