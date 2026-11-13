"""Limitator in play (GAPS.md tier 1 "Limitator weap1-11", 35 maps): the reserve ammo a player's loadout weapon comes
with (coop.cpp loadout: the Limitator's value instead of the default; the magazine comes full as usual) and what an
ammo_reserve crate adds. weapN 0 and a weapN left out: suite limitator_zero. The default test loadout is light 1 (RC28,
weap1) and heavy 1 (FA18, weap5). Its own suite: the limits are the map's. The last test changes the map: it must stay
last (the suite's map is gone after it)."""

from etest import Suite

SUITE = Suite('limitator', 'Limitator: the loadout ammo and the ammo crate follow weap1..weap11')
S = SUITE
NO_LIMITATOR_MAP = 'co_launchrock'     # a stock map without a Limitator, a safe spawn


@S.test('Limitator weap1 60 / weap5 70: the RC28 comes with 60 reserve rounds (default 120), the FA18 with 70 (default '
        '250)', players=1, covers=('Limitator', 'Limitator.weap1', 'Limitator.weap5'))
def loadout(t):
    t.point('Limitator', at=(-200, -200, 0), weap1='60', weap2='61', weap3='62', weap4='63', weap5='70', weap6='71',
            weap7='72', weap8='73', weap9='74', weap10='75', weap11='76')
    t.snap('spawned', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('spawned', 'player1', 'ammo:rc28m', 60)
    t.expect.value('spawned', 'player1', 'ammo:fa18m', 70)


@S.test('Limitator: an ammo_reserve crate gives each weapon its weap value', players=1,
        covers=('Limitator.weap1', 'Limitator.weap5'))
def crate(t):
    t.snap('before', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.point('ammo_reserve', targetname='crate', at=(0, 120, 0))
    t.teleport('player1', (0, 120, 0))
    t.wait(0.5)
    t.snap('after', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('after', 'player1', 'ammo:rc28m', 120)
    t.expect.value('after', 'player1', 'ammo:fa18m', 140)


@S.test('Limitator weap2-4, weap6-11: each ammo_reserve crate adds each value to its own ammo (the player holds none '
        'of those weapons: the crate gives every type; the crate of t02 gave them once already)', players=1,
        covers=('Limitator.weap2', 'Limitator.weap3', 'Limitator.weap4', 'Limitator.weap6', 'Limitator.weap7',
                'Limitator.weap8', 'Limitator.weap9', 'Limitator.weap10', 'Limitator.weap11'))
def crate_every_weapon(t):
    ammo = (('k444m', 61), ('canonm', 62), ('pm66m', 63), ('federalm', 71), ('spam', 72), ('benm', 73), ('swm', 74),
            ('nm33m', 75), ('chainm', 76))
    t.snap('before', 'player1', *['ammo:' + a for a, _ in ammo])
    t.point('ammo_reserve', targetname='crate', at=(0, 120, 0))
    t.teleport('player1', (0, 120, 0))
    t.wait(0.5)
    t.snap('after', 'player1', *['ammo:' + a for a, _ in ammo])
    for a, v in ammo:
        t.expect.value('before', 'player1', 'ammo:' + a, v)
        t.expect.value('after', 'player1', 'ammo:' + a, 2 * v)


@S.test('Map change to %s (no Limitator): the loadout gives the default reserve again, the RC28 120 rounds and the '
        'FA18 250 (not this map\'s 60 / 70)' % NO_LIMITATOR_MAP, players=1, covers=('Limitator',))
def map_change(t):
    t.console('changelevel %s' % NO_LIMITATOR_MAP)
    t.wait_frames(20)
    t.console('test_player add et1')    # the test player is dropped (fake clients are) and added again there
    t.wait_frames(2)
    t.console('test_join player1')
    t.wait(0.5)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap('other', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('other', 'player1', 'ammo:rc28m', 120)
    t.expect.value('other', 'player1', 'ammo:fa18m', 250)
