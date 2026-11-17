"""Fists at every spawn in Battle Royale, whatever game_player_equip entities the map has (br.cpp CBr::PlayerSpawn): an
equip only adds items, beside the random bonus item of each spawn. One pair only: an equip that lists weapon_wong
gives no second pair, at spawn (equip first, then the default fists) or when it is fired at a player who has them
(CGamePlayerEquip::EquipPlayer), and leaves no weapon_wong behind in the world. Every equip in the map applies to
every spawn, so the phases run in one test: both equips, then the Use Only one alone, then none (killtarget). The
players are added by the test (edicts 1-2, =player1 / =player2) and answer the go menu themselves
(observer_movetype_br). Suites fists_spawn (co-op) and fists_spawn_bk: the same in the other modes."""

from etest import Suite

SUITE = Suite('fists_spawn_br', 'Battle Royale: fists at every spawn beside game_player_equip, never two pairs',
              players=0)
S = SUITE
S.map_prefix = 'br_'
DOC = 'FGD game_player_equip'

WONG = 1 << 21
PM66 = 1 << 18


def respawn(t, label):
    """=player1 loses everything (player_weaponstrip), spawns again; snapshot of its weapons after (the sword is not
    checked: the random bonus item can be one)."""
    t.fire('strip', activator='=player1')
    t.wait(0.3)
    t.mark(label)
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap(label + '_w', '=player1', 'weapons')
    t.wait(1.0)


@S.test('fists at each spawn: with an equip listing weapon_wong one pair; fired at a player with fists, a Use Only '
        'equip listing weapon_wong gives its pm66 and no second pair; with only the Use Only equip, and with no '
        'equip, the fists', doc=DOC, size=(512, 512, 256),
        covers=('game_player_equip', 'game_player_equip:1', 'game_player_equip.weapon_wong'))
def fists_spawn_br(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='eq', weapon_wong='1', weapon_sword='1')
    t.point('game_player_equip', targetname='eq_use', spawnflags='1', weapon_wong='1', weapon_pm66='1')
    t.point('trigger_relay', targetname='kill_eq', killtarget='eq')
    t.point('trigger_relay', targetname='kill_use', killtarget='eq_use')
    for name in ('br1', 'br2'):
        t.console('test_player add %s' % name)
        t.wait_frames(2)
    t.console('test_cmd player1 menuselect 1')     # the go menu: ready (two ready players start a round)
    t.console('test_cmd player2 menuselect 1')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.duration += 40
    t.wait(1.0)
    t.snap('round_w', '=player1', 'weapons')

    respawn(t, 'both')                         # equip at spawn, then the default fists
    t.mark('use')
    t.fire('eq_use', activator='=player1')     # the fists are held: the equip's weapon_wong is left out
    t.wait(0.3)
    t.snap('use_w', '=player1', 'weapons')

    t.fire('kill_eq')
    t.wait(0.3)
    respawn(t, 'useonly')

    t.fire('kill_use')
    t.wait(0.3)
    respawn(t, 'none')

    t.expect.value('round_w', '=player1', 'weapons', WONG, op='&')
    t.expect.value('both_w', '=player1', 'weapons', WONG, op='&')
    t.expect.spawned('@weapon_wong', count=1, after='both', within=(0, 1.0))
    t.expect.value('use_w', '=player1', 'weapons', WONG | PM66, op='&')
    t.expect.spawned('@weapon_wong', count=0, after='use', within=(0, 0.3))
    t.expect.value('useonly_w', '=player1', 'weapons', WONG, op='&')
    t.expect.spawned('@weapon_wong', count=1, after='useonly', within=(0, 1.0))
    t.expect.value('none_w', '=player1', 'weapons', WONG, op='&')
    t.expect.spawned('@weapon_wong', count=1, after='none', within=(0, 1.0))
