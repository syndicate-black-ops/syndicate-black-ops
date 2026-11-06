"""Deathmatch loadout items against their descriptions (#SBO_Item_<item> in
gamedir/syndicate/ui/resource/language/sbo_ui_english.txt): the Adrenaline Surge gives +75 health as in co-op
(1.38: health 150, +50), the tripmine costs no speed (1.38: -6). CDm::PlayerSpawn (dm.cpp). The co-op side is in
suite armor. The map is dm_et_items_dm (no co_ prefix: deathmatch game rules)."""

from etest import Suite

SUITE = Suite('items_dm', 'Deathmatch loadout: adrenaline +75 health, tripmines cost no speed, Assassin speed', players=1)
S = SUITE
S.map_prefix = 'dm_'


def respawn_with(t, loadout):
    """The loadout applies at the next spawn (test_join, then test_spawn: respawn -> CDm::PlayerSpawn)."""
    t.console('test_join player1 %s' % loadout)
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(0.5)


@S.test('adrenaline surge: +75 health (175 at the spawn health of 100)', players=1)
def adrenaline_dm(t):
    respawn_with(t, '2 1 2 1 4 4 1')     # chainsaw, rc28, federal, grenade, adrenaline, no armor
    t.snap('spawn', 'player1', 'health', 'sbo_health')
    t.expect.value('spawn', 'player1', 'sbo_health', 100)
    t.expect.value('spawn', 'player1', 'health', 175)


@S.test('tripmine: no speed penalty (the same max speed as with grenades); satchel -15', players=1)
def tripmine_speed_dm(t):
    respawn_with(t, '2 1 2 1 2 4 1')     # cyberware 2 (targeting): no speed change
    t.snap('grenade', 'player1', 'maxspeed')
    respawn_with(t, '2 1 2 2 2 4 1')
    t.snap('tripmine', 'player1', 'maxspeed')
    respawn_with(t, '2 1 2 3 2 4 1')
    t.snap('satchel', 'player1', 'maxspeed')
    t.expect.value('grenade', 'player1', 'maxspeed', 280)    # 300 - federal 20
    t.expect.value('tripmine', 'player1', 'maxspeed', 280)
    t.expect.value('satchel', 'player1', 'maxspeed', 265)


@S.test('Assassin: the armor skin\'s weight whatever the armor pick (570 with each of the 4; 1.38: 540 to 600)',
        players=1)
def assassin_armor_speed_dm(t):
    # Bounty Killer: 300 - skin 30 + Assassin 300; the chainsaw blade weighs nothing
    for armor in (1, 2, 3, 4):
        respawn_with(t, '2 1 2 1 5 %d 1' % armor)
        t.snap('armor%d' % armor, 'player1', 'maxspeed', 'armorvalue')
    for armor in (1, 2, 3, 4):
        t.expect.value('armor%d' % armor, 'player1', 'maxspeed', 570)
        t.expect.value('armor%d' % armor, 'player1', 'armorvalue', 50)
