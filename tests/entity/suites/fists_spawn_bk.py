"""Fists at every spawn in Bounty Killer, whatever game_player_equip entities the map has (dm.cpp CDm::PlayerSpawn): an
equip only adds items. One pair only: an equip that lists weapon_wong gives no second pair, at spawn (equip first,
then the default fists) or when it is fired at a player who has them (CGamePlayerEquip::EquipPlayer), and leaves no
weapon_wong behind in the world. Every equip in the map applies to every spawn, so the phases run in one test: both
equips, then the Use Only one alone, then none (killtarget). Suites fists_spawn (co-op) and fists_spawn_br: the same
in the other modes. The map is bk_et_fists_spawn_bk (Bounty Killer game rules)."""

from etest import Suite

SUITE = Suite('fists_spawn_bk', 'Bounty Killer: fists at every spawn beside game_player_equip, never two pairs')
S = SUITE
S.map_prefix = 'bk_'
DOC = 'FGD game_player_equip'

WONG = 1 << 21
SWORD = 1 << 10
PM66 = 1 << 18


def respawn(t, label):
    """player1 loses everything (player_weaponstrip), spawns again; snapshot of its weapons after."""
    t.fire('strip', activator='player1')
    t.wait(0.3)
    t.mark(label)
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap(label + '_w', 'player1', 'weapons')
    t.wait(1.0)


@S.test('fists at each spawn: with an equip listing weapon_wong and a sword one pair and the sword; fired at a player '
        'with fists, a Use Only equip listing weapon_wong gives its pm66 and no second pair; with only the Use Only '
        'equip, and with no equip, the fists', players=1, doc=DOC, size=(512, 512, 256),
        covers=('game_player_equip', 'game_player_equip:1', 'game_player_equip.weapon_wong'))
def fists_spawn_bk(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='eq', weapon_wong='1', weapon_sword='1')
    t.point('game_player_equip', targetname='eq_use', spawnflags='1', weapon_wong='1', weapon_pm66='1')
    t.point('trigger_relay', targetname='kill_eq', killtarget='eq')
    t.point('trigger_relay', targetname='kill_use', killtarget='eq_use')

    respawn(t, 'both')                       # equip at spawn, then the default fists
    t.mark('use')
    t.fire('eq_use', activator='player1')    # the fists are held: the equip's weapon_wong is left out
    t.wait(0.3)
    t.snap('use_w', 'player1', 'weapons')

    t.fire('kill_eq')
    t.wait(0.3)
    respawn(t, 'useonly')

    t.fire('kill_use')
    t.wait(0.3)
    respawn(t, 'none')

    t.expect.value('both_w', 'player1', 'weapons', WONG | SWORD, op='&')
    t.expect.spawned('@weapon_wong', count=1, after='both', within=(0, 1.0))
    t.expect.value('use_w', 'player1', 'weapons', WONG | PM66, op='&')
    t.expect.spawned('@weapon_wong', count=0, after='use', within=(0, 0.3))
    t.expect.value('useonly_w', 'player1', 'weapons', WONG, op='&')
    t.expect.value('useonly_w', 'player1', 'weapons', SWORD | PM66, op='!&')
    t.expect.spawned('@weapon_wong', count=1, after='useonly', within=(0, 1.0))
    t.expect.value('none_w', 'player1', 'weapons', WONG, op='&')
    t.expect.spawned('@weapon_wong', count=1, after='none', within=(0, 1.0))
