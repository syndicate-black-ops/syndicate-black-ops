"""Tier 5, final pass: inventory choices no test asserted (docs/INVENTORY.md): every HUD icon of item_pickup and
player_inventory (the Inventory message names it to the holder's client), and item_pickup "TOUCH only" (128) with
"USE only" (256): touch takes it, +use does not."""

from etest import Suite

SUITE = Suite('fgd_inventory_more', 'Tier 5: inventory icons, TOUCH only', players=1,
              requires=('Inventory_RoundReset',))
S = SUITE
DOC = 'INVENTORY.md'
HOME, HELD = 0, 1


def take(t, at):
    t.teleport('player1', at)
    t.wait(0.3)
    t.teleport('player1', (-150, -150, 0))
    t.wait(0.1)


@S.test('item_pickup "item_icon" Blue keycard / Yellow keycard / Key: picked up, each sends its icon in the Inventory '
        'message', players=1, doc=DOC,
        covers=tuple('item_pickup.item_icon=%s' % i for i in ('inv_card_blue', 'inv_card_yellow', 'inv_key')))
def pickup_icons(t):
    icons = ('inv_card_blue', 'inv_card_yellow', 'inv_key')
    for i, icon in enumerate(icons):
        t.point('item_pickup', targetname='c%d' % i, at=(-120 + i * 120, 120, 16), item_name=t.name('k%d' % i),
                item_icon=icon)
    t.wait(0.5)
    for i, icon in enumerate(icons):
        t.mark('m%d' % i)
        take(t, (-120 + i * 120, 120, 0))
        t.expect.msg('Inventory', to='player1', min=1, after='m%d' % i, fields={'item': t.name('k%d' % i), 'icon': icon})


@S.test('player_inventory "item_icon" each choice: giving a new item sends that icon in the Inventory message',
        players=1, doc=DOC,
        covers=tuple('player_inventory.item_icon=%s' % i
                     for i in ('inv_card_blue', 'inv_card_red', 'inv_card_yellow', 'inv_key')))
def give_icons(t):
    icons = ('inv_card_blue', 'inv_card_red', 'inv_card_yellow', 'inv_key')
    for i, icon in enumerate(icons):
        t.point('player_inventory', targetname='g%d' % i, item_name=t.name('i%d' % i), item_icon=icon,
                operation='1', count='1')
    for i, icon in enumerate(icons):
        t.mark('m%d' % i)
        t.fire('g%d' % i, activator='player1')
        t.wait(0.2)
        t.expect.msg('Inventory', to='player1', min=1, after='m%d' % i, fields={'item': t.name('i%d' % i), 'icon': icon})


@S.test('item_pickup "TOUCH only" (128) with "USE only" (256): +use does not take it, walking onto it does', players=1,
        doc=DOC, covers=('item_pickup:128',))
def touch_only(t):
    t.point('item_pickup', targetname='card', at=(120, 0, 16), item_name=t.name('key'), spawnflags=str(128 | 256))
    t.wait(0.5)
    t.fire('card', activator='player1', caller='player1')
    t.wait(0.2)
    t.snap('used', 'card', 'kv:inv_state')
    take(t, (120, 0, 0))
    t.snap('touched', 'card', 'kv:inv_state')
    t.snap('touched', 'player1', 'inv:' + t.name('key'))
    t.expect.value('used', 'card', 'kv:inv_state', HOME)
    t.expect.value('touched', 'card', 'kv:inv_state', HELD)
    t.expect.value('touched', 'player1', 'inv:' + t.name('key'), 1)
