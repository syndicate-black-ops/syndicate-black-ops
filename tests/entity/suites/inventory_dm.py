"""Inventory in Bounty Killer (docs/INVENTORY.md; map dm_et_inventory_dm: deathmatch game rules, players respawn):
a personal item drops where its holder died, as in co-op; Keep on death keeps it through the death and the respawn."""

from etest import Suite

SUITE = Suite('inventory_dm', 'Inventory in Bounty Killer: death drop, keep on death through a respawn', players=1,
              requires=('Inventory_RoundReset',))
S = SUITE
S.map_prefix = 'dm_'
DOC = 'INVENTORY.md'
HOME, HELD, POOL, DROPPED = 0, 1, 2, 3


def spawn(t):
    t.console('test_join player1 2 1 2 1 2 4 1')
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(2.5)  # past the 2 s spawn protection, so a kill takes the card


def take(t, at=(120, 0, 0)):
    t.teleport('player1', at)
    t.wait(0.3)
    t.teleport('player1', (-150, 0, 0))
    t.wait(0.1)


def die(t):
    t.kv('player1', 'health', '1')
    t.damage('player1', 20)


@S.test('Bounty Killer: the item drops where its holder died', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup.item_name'))
def drop_dm(t):
    spawn(t)
    t.point('item_pickup', targetname='card', at=(120, 0, 16), item_name=t.name('key'))
    take(t)
    t.snap('held', 'card', 'kv:inv_state')
    die(t)
    t.wait(0.5)
    t.snap('dead', 'card', 'kv:inv_state', 'effects')
    spawn(t)
    t.expect.value('held', 'card', 'kv:inv_state', HELD)
    t.expect.value('dead', 'card', 'kv:inv_state', DROPPED)
    t.expect.value('dead', 'card', 'effects', 128, op='!&')


@S.test('Bounty Killer, Keep on death: the holder still has it after he died and respawned', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup.holder_keep_on_death=1'))
def keep_dm(t):
    spawn(t)
    t.point('item_pickup', targetname='card', at=(120, 0, 16), item_name=t.name('key'), holder_keep_on_death='1')
    take(t)
    die(t)
    t.wait(0.5)
    spawn(t)
    t.snap('respawned', 'card', 'kv:inv_state', 'kv:inv_holder')
    t.snap('respawned', 'player1', 'inv:' + t.name('key'))
    t.expect.value('respawned', 'card', 'kv:inv_state', HELD)
    t.expect.value('respawned', 'card', 'kv:inv_holder', 1)
    t.expect.value('respawned', 'player1', 'inv:' + t.name('key'), 1)
