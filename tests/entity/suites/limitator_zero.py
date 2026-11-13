"""Limitator weapN 0 and above 900 (limitator.cpp): 0 gives no reserve ammo; the loadout weapon still comes with a
full magazine, and an ammo_reserve crate adds nothing for that weapon. A value above 900 gives 900, the most each
weapon carries. A weapN the Limitator leaves out gives the reserve the loadout gives without a Limitator (weap1:
120), with the loadout and with each crate; never the value of the map before. The default test loadout is light 1
(RC28, weap1) and heavy 1 (FA18, weap5, the weapon in hand). Its own suite: the values are the map's. The last test changes the map and comes back: it must stay last."""

from etest import Suite

SUITE = Suite('limitator_zero', 'Limitator: weapN 0 gives no reserve ammo, above 900 gives 900; a weapN left out gives '
              'the default reserve')
S = SUITE
WEAPON_FA18 = 6
SUITE_MAP = 'co_et_limitator_zero'
OTHER_MAP = 'co_forgottenworld'     # a stock map whose Limitator sets weap1 500, weap5 100


def player_back(t):
    """After a map change: the test player again, joined (loadout light 1, heavy 1) and alive in a new round (fake
    clients are dropped at a changelevel)."""
    t.wait_frames(20)
    t.console('test_player add et1')
    t.wait_frames(2)
    t.console('test_join player1')
    t.wait(0.5)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.flags('player1', '+FL_NOTARGET')          # the map's monsters leave it alone


@S.test('Limitator weap5 0, weap1 left out: the FA18 comes with a full magazine (50) and 0 reserve rounds; the RC28 '
        'with 120 reserve rounds (the loadout default)', players=1, covers=('Limitator', 'Limitator.weap5'))
def loadout(t):
    t.point('Limitator', at=(-200, -200, 0), weap2='5000', weap3='62', weap4='63', weap5='0', weap6='71', weap7='72',
            weap8='73', weap9='74', weap10='75', weap11='76')
    t.wait(0.3)
    t.snap('spawned', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('spawned', 'player1', 'ammo:fa18m', 0)
    t.expect.value('spawned', 'player1', 'ammo:rc28m', 120)
    t.mark('switch')              # the spawn's CurWeapon comes before the test starts: switch away and back
    t.cmd('player1', 'weapon_rc28')
    t.wait(1.0)
    t.cmd('player1', 'weapon_fa18')
    t.wait(1.0)
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_FA18, 'clip': 50}, after='switch')


@S.test('Limitator weap5 0: an ammo_reserve crate adds no FA18 rounds; weap1 left out: 120 RC28 rounds; weap2 5000: '
        '900 .444k rounds (the player holds no .444k)', players=1, covers=('Limitator.weap2', 'Limitator.weap5'))
def crate(t):
    t.snap('before', 'player1', 'ammo:rc28m', 'ammo:fa18m', 'ammo:k444m')
    t.expect.value('before', 'player1', 'ammo:k444m', 0)
    t.point('ammo_reserve', targetname='crate', at=(0, 120, 0))
    t.teleport('player1', (0, 120, 0))
    t.wait(0.5)
    t.snap('after', 'player1', 'ammo:rc28m', 'ammo:fa18m', 'ammo:k444m')
    t.expect.value('after', 'player1', 'ammo:fa18m', 0)
    t.expect.value('after', 'player1', 'ammo:rc28m', 240)
    t.expect.value('after', 'player1', 'ammo:k444m', 900)


@S.test('Map change: after %s (its Limitator: weap1 500, weap5 100) this map\'s Limitator, weap1 left out, gives the '
        'RC28 120 reserve rounds again (not 500) and its crate adds 120; weap5 0 stays 0' % OTHER_MAP, players=1,
        covers=('Limitator',))
def map_change(t):
    t.point('ammo_reserve', targetname='crate', at=(0, 120, 0))
    t.console('changelevel %s' % OTHER_MAP)
    player_back(t)
    t.snap('other', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('other', 'player1', 'ammo:rc28m', 500)
    t.expect.value('other', 'player1', 'ammo:fa18m', 100)
    t.console('changelevel %s' % SUITE_MAP)
    player_back(t)
    t.snap('back', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('back', 'player1', 'ammo:rc28m', 120)
    t.expect.value('back', 'player1', 'ammo:fa18m', 0)
    t.teleport('player1', (0, 120, 0))
    t.wait(0.5)
    t.snap('crate', 'player1', 'ammo:rc28m', 'ammo:fa18m')
    t.expect.value('crate', 'player1', 'ammo:rc28m', 240)
    t.expect.value('crate', 'player1', 'ammo:fa18m', 0)
