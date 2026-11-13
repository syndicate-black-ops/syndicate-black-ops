"""Inventory (docs/INVENTORY.md): item_pickup, player_inventory, player_hasinventory and player_calc_ratio's
"Inventory item count". The picked-up item_pickup is the item (hidden while held, remembers its holder): pickup,
the "Inventory" message, refusals (held, two takers in one frame, collect_limit), the death drop and the return
timer, keep on death, out of reach, strips, round restarts (personal back home, shared kept unless round_reset),
the shared pool and the world as activator, masters, player_inventory's operations and targets, using up a
pickup, the fullupdate resend, and the players who never hold items (Game Master, disconnect, spectator; last,
as they change the players).

Item states (kv:inv_state): 0 at its spot, 1 held, 2 in the shared pool, 3 dropped, 4 used up."""

from etest import Suite

SUITE = Suite('inventory', 'Inventory: item_pickup, player_inventory, player_hasinventory (personal and shared)',
              players=2, requires=('Inventory_RoundReset',))
S = SUITE
SUITE.server_args = ('+developer', '1')
DOC = 'INVENTORY.md'

HOME, HELD, POOL, DROPPED, SPENT = 0, 1, 2, 3, 4


def card(t, name='card', item=None, at=(120, 0, 16), **kv):
    """An item_pickup; its item name is the test's own (K(t)) unless named."""
    t.point('item_pickup', targetname=name, at=at, item_name=item or K(t), **kv)


def K(t):
    """The test's own item name (items of other tests may still be held: names never repeat)."""
    return t.name('key')


def die(t, player='player1'):
    """Killed without being gibbed or thrown far (SBO scales damage to players 0.25x to 2x)."""
    t.kv(player, 'health', '1')
    t.damage(player, 20)


def take(t, player, at=(120, 0, 0)):
    """The player stands on the item (touch)."""
    t.teleport(player, at)
    t.wait(0.3)
    t.teleport(player, (-150, 0, 0) if player == 'player1' else (-150, 100, 0))
    t.wait(0.1)


def revive(t):
    t.console('test_round restart')
    t.wait(1.5)


# ---------------------------------------------------------------------------------------------- pickup

@S.test('pickup: the item is hidden and held, the player\'s count, the Inventory message, target, sound and '
        'message; a second player gets nothing', players=2, doc=DOC,
        covers=('item_pickup', 'item_pickup.item_name', 'item_pickup.item_icon=inv_card_red', 'item_pickup.count',
                'item_pickup.noise', 'item_pickup.message', 'item_pickup.target'))
def pickup(t):
    card(t, item_icon='inv_card_red', count='2', noise='items/gunpickup2.wav', message='got it', target='after')
    t.point('info_target', targetname='after')
    t.wait(1.0)     # the first test: the players settle after the round start
    t.mark('go')
    take(t, 'player1')
    take(t, 'player2')
    t.snap('s', 'card', 'effects', 'solid', 'kv:inv_state', 'kv:inv_holder')
    t.snap('s', 'player1', 'inv:' + K(t))
    t.snap('s', 'player2', 'inv:' + K(t))
    t.expect.value('s', 'card', 'kv:inv_state', HELD)
    t.expect.value('s', 'card', 'kv:inv_holder', 1)
    t.expect.value('s', 'card', 'effects', 128, op='&')     # EF_NODRAW
    t.expect.value('s', 'card', 'solid', 0)
    t.expect.value('s', 'player1', 'inv:' + K(t), 2)
    t.expect.value('s', 'player2', 'inv:' + K(t), 0)
    t.expect.msg('Inventory', to='player1', count=1, after='go',
                 fields={'count': 2, 'item': K(t), 'icon': 'inv_card_red', 'shared': 0})
    t.expect.no_msg('Inventory', to='player2', after='go')
    t.expect.used('after', activator='player1', count=1)
    t.expect.sound('gunpickup2', ent='player1')


@S.test('no item_name: nobody can take it; model and noise left out: the security card and the pickup sound are '
        'precached', players=1, doc=DOC, covers=('item_pickup', 'item_pickup.model'))
def defaults(t):
    t.point('item_pickup', targetname='card', at=(120, 0, 16))
    take(t, 'player1')
    t.snap('s', 'card', 'kv:inv_state', 'model')
    t.expect.value('s', 'card', 'kv:inv_state', HOME)
    t.expect.value('s', 'card', 'model', 'models/w_security.mdl')
    t.expect.precached('models/w_security.mdl', kind='model')
    t.expect.precached('items/gunpickup2.wav', kind='sound')


@S.test('two takers in one frame (USE only, +use by both): only the first gets it', players=2, doc=DOC,
        covers=('item_pickup', 'item_pickup:256'))
def double_pickup(t):
    card(t, spawnflags='256')
    t.fire('card', activator='player1', caller='player1')
    t.fire('card', activator='player2', caller='player2')
    t.wait(0.2)
    t.snap('s', 'player1', 'inv:' + K(t))
    t.snap('s', 'player2', 'inv:' + K(t))
    t.snap('s', 'card', 'kv:inv_holder')
    t.expect.value('s', 'player1', 'inv:' + K(t), 1)
    t.expect.value('s', 'player2', 'inv:' + K(t), 0)
    t.expect.value('s', 'card', 'kv:inv_holder', 1)


@S.test('two players +use one USE-only item_battery in the same frame (CItem::Use): only one gets it', players=2, doc=DOC,
        covers=('item_battery', 'item_battery:256'))
def double_battery(t):
    t.point('item_battery', targetname='bat', at=(120, 0, 16), spawnflags='256')
    for p in ('player1', 'player2'):
        t.kv(p, 'armorvalue', '0')
    # both stand next to it, look at it and press +use in the same frame (the engine's +use: PlayerUse calls the
    # item's Use directly; a fire by name can't reach it once taken, UTIL_Remove clears the name)
    t.teleport('player1', (72, 0, 0), yaw=0)
    t.teleport('player2', (168, 0, 0), yaw=180)
    t.look('player1', 50, 0)
    t.look('player2', 50, 180)
    t.wait(0.3)
    t.mark('go')
    t.move('player1', 0, 0, 2, '+use')
    t.move('player2', 0, 0, 2, '+use')
    t.wait(0.3)
    t.expect.msg('ItemPickup', min=1, after='go')       # it was taken
    t.expect.msg('ItemPickup', count=1, after='go')     # by one player only


@S.test('collect_limit: a second card of the same item is refused while the player holds the limit', players=1,
        doc=DOC, covers=('item_pickup', 'item_pickup.collect_limit'))
def collect_limit(t):
    card(t, name='c1', item='coin', collect_limit='1', at=(120, 0, 16))
    card(t, name='c2', item='coin', collect_limit='1', at=(120, 120, 16))
    take(t, 'player1', (120, 0, 0))
    take(t, 'player1', (120, 120, 0))
    t.snap('s', 'player1', 'inv:coin')
    t.snap('s', 'c2', 'kv:inv_state')
    t.expect.value('s', 'player1', 'inv:coin', 1)
    t.expect.value('s', 'c2', 'kv:inv_state', HOME)


# ---------------------------------------------------------------------------------------------- death

@S.test('death (Keep on death No): the item drops at the body, the count goes to 0, another player can take it there',
        players=2, doc=DOC, covers=('item_pickup', 'item_pickup.return_timelimit', 'item_pickup.holder_keep_on_death=0'))
def death_drop(t):
    card(t, return_timelimit='30', holder_keep_on_death='0')
    t.point('info_target', targetname='deathspot', at=(0, 120, 16))
    take(t, 'player1')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.3)
    t.mark('die')
    die(t)
    t.wait(1.0)
    t.snap('dead', 'card', 'kv:inv_state', 'origin', 'effects')
    t.snap('dead', 'player1', 'origin', 'inv:' + K(t))
    t.snap('dead', 'deathspot', 'origin')
    t.teleport('player2', 'card')
    t.wait(0.3)
    t.snap('taken', 'card', 'kv:inv_state', 'kv:inv_holder')
    t.snap('taken', 'player2', 'inv:' + K(t))
    revive(t)
    t.expect.value('dead', 'card', 'kv:inv_state', DROPPED)
    t.expect.value('dead', 'card', 'effects', 128, op='!&')
    t.expect.value('dead', 'player1', 'inv:' + K(t), 0)
    t.expect.distance('dead', 'card', 'deathspot', 0, 48)   # where he died (the observer view moves away)
    t.expect.msg('Inventory', to='player1', count=1, after='die', fields={'count': 0, 'item': K(t)})
    t.expect.value('taken', 'card', 'kv:inv_state', HELD)
    t.expect.value('taken', 'card', 'kv:inv_holder', 2)
    t.expect.value('taken', 'player2', 'inv:' + K(t), 1)


@S.test('return timer: a dropped item untouched for return_timelimit seconds goes back to its placed spot',
        players=1, doc=DOC, covers=('item_pickup', 'item_pickup.return_timelimit'))
def return_timer(t):
    card(t, return_timelimit='2')
    t.point('info_target', targetname='spot', at=(120, 0, 16))
    take(t, 'player1')
    die(t)
    t.wait(1.0)
    t.snap('dropped', 'card', 'kv:inv_state')
    t.wait(2.5)
    t.snap('back', 'card', 'kv:inv_state', 'origin', 'effects', 'solid')
    t.snap('back', 'spot', 'origin')
    revive(t)
    t.expect.value('dropped', 'card', 'kv:inv_state', DROPPED)
    t.expect.value('back', 'card', 'kv:inv_state', HOME)
    t.expect.value('back', 'card', 'effects', 128, op='!&')
    t.expect.value('back', 'card', 'solid', 1)      # SOLID_TRIGGER
    t.expect.distance('back', 'card', 'spot', 0, 1)


@S.test('out of reach: a dropped item that ends up in a wall goes back at once', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup.return_timelimit'))
def out_of_reach(t):
    card(t, return_timelimit='60')
    t.point('info_target', targetname='spot', at=(120, 0, 16))
    take(t, 'player1')
    die(t)
    t.wait(1.0)
    t.teleport('card', (0, 0, -64))      # under the floor: solid
    t.wait(1.0)
    t.snap('back', 'card', 'kv:inv_state', 'origin')
    t.snap('back', 'spot', 'origin')
    revive(t)
    t.expect.value('back', 'card', 'kv:inv_state', HOME)
    t.expect.distance('back', 'card', 'spot', 0, 1)


@S.test('return_timelimit 0: the item goes straight back when its holder dies', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup.return_timelimit'))
def return_zero(t):
    card(t, return_timelimit='0')
    take(t, 'player1')
    die(t)
    t.wait(0.5)
    t.snap('s', 'card', 'kv:inv_state')
    revive(t)
    t.expect.value('s', 'card', 'kv:inv_state', HOME)


@S.test('keep on death: the holder keeps the item when he dies; a round restart sends it back', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup.holder_keep_on_death=1'))
def keep_on_death(t):
    card(t, holder_keep_on_death='1')
    take(t, 'player1')
    die(t)
    t.wait(2.0)
    t.snap('dead', 'card', 'kv:inv_state', 'kv:inv_holder')
    t.snap('dead', 'player1', 'inv:' + K(t))
    revive(t)
    t.snap('round', 'card', 'kv:inv_state')
    t.snap('round', 'player1', 'inv:' + K(t))
    t.expect.value('dead', 'card', 'kv:inv_state', HELD)
    t.expect.value('dead', 'card', 'kv:inv_holder', 1)
    t.expect.value('dead', 'player1', 'inv:' + K(t), 1)
    t.expect.value('round', 'card', 'kv:inv_state', HOME)
    t.expect.value('round', 'player1', 'inv:' + K(t), 0)


# ---------------------------------------------------------------------------------------------- strips, rounds

@S.test('player_weaponstrip: every strip sends the held items back to their spots', players=1, doc=DOC,
        covers=('item_pickup', 'player_weaponstrip'))
def strip(t):
    card(t)
    t.point('player_weaponstrip', targetname='strip')
    take(t, 'player1')
    t.fire('strip', activator='player1')
    t.wait(0.2)
    t.snap('s', 'card', 'kv:inv_state')
    t.snap('s', 'player1', 'inv:' + K(t))
    revive(t)
    t.expect.value('s', 'card', 'kv:inv_state', HOME)
    t.expect.value('s', 'player1', 'inv:' + K(t), 0)


@S.test('round restart: personal items go home and the lists are emptied; a shared item stays in the pool, a '
        'shared one with round_reset 1 goes home', players=1, doc=DOC,
        covers=('item_pickup', 'item_pickup:4', 'item_pickup.round_reset=1', 'item_pickup.round_reset=0',
                'player_inventory'))
def round_restart(t):
    card(t, name='own', item='own_key', at=(120, 0, 16))
    card(t, name='keep', item='team_key', at=(120, 100, 16), spawnflags='4', round_reset='0')
    card(t, name='back', item='team_reset', at=(120, -100, 16), spawnflags='4', round_reset='1')
    t.point('player_inventory', targetname='give', item_name='virtual', operation='1', count='3')
    take(t, 'player1', (120, 0, 0))
    take(t, 'player1', (120, 100, 0))
    take(t, 'player1', (120, -100, 0))
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.snap('before', 'player1', 'inv:own_key', 'inv:virtual', 'pool:team_key', 'pool:team_reset')
    t.mark('restart')
    revive(t)
    t.snap('after', 'own', 'kv:inv_state')
    t.snap('after', 'keep', 'kv:inv_state')
    t.snap('after', 'back', 'kv:inv_state')
    t.snap('after', 'player1', 'inv:own_key', 'inv:virtual', 'pool:team_key', 'pool:team_reset')
    t.expect.value('before', 'player1', 'inv:own_key', 1)
    t.expect.value('before', 'player1', 'inv:virtual', 3)
    t.expect.value('before', 'player1', 'pool:team_key', 1)
    t.expect.value('before', 'player1', 'pool:team_reset', 1)
    t.expect.value('after', 'own', 'kv:inv_state', HOME)
    t.expect.value('after', 'keep', 'kv:inv_state', POOL)
    t.expect.value('after', 'back', 'kv:inv_state', HOME)
    t.expect.value('after', 'player1', 'inv:own_key', 0)
    t.expect.value('after', 'player1', 'inv:virtual', 0)
    t.expect.value('after', 'player1', 'pool:team_key', 1)
    t.expect.value('after', 'player1', 'pool:team_reset', 0)
    t.expect.msg('Inventory', count=1, after='restart', fields={'count': 0, 'item': 'team_reset', 'shared': 1})


# ---------------------------------------------------------------------------------------------- shared pool, checks

@S.test('shared: one taker adds it to the pool (message to everybody); player_hasinventory Shared passes for '
        'any player and the world; the personal check does not', players=2, doc=DOC,
        covers=('item_pickup', 'item_pickup:4', 'player_hasinventory', 'player_hasinventory:4',
                'player_hasinventory.item_name', 'player_hasinventory.pass_target',
                'player_hasinventory.fail_target'))
def shared_pool(t):
    card(t, item='crew_key', spawnflags='4', item_icon='inv_card_blue')
    t.point('player_hasinventory', targetname='hs', item_name='crew_key', spawnflags='4', pass_target='spass',
            fail_target='sfail')
    t.point('player_hasinventory', targetname='hp', item_name='crew_key', pass_target='ppass', fail_target='pfail')
    for n in ('spass', 'sfail', 'ppass', 'pfail'):
        t.point('info_target', targetname=n)
    t.mark('go')
    take(t, 'player1')
    t.fire('hs', activator='world')
    t.fire('hs', activator='player2')
    t.fire('hp', activator='player2')
    t.wait(0.2)
    t.snap('s', 'card', 'kv:inv_state')
    t.snap('s', 'player1', 'inv:crew_key', 'pool:crew_key')
    t.expect.value('s', 'card', 'kv:inv_state', POOL)
    t.expect.value('s', 'player1', 'inv:crew_key', 0)
    t.expect.value('s', 'player1', 'pool:crew_key', 1)
    t.expect.msg('Inventory', count=1, after='go',
                 fields={'count': 1, 'item': 'crew_key', 'icon': 'inv_card_blue', 'shared': 1})
    t.expect.used('spass', count=2)
    t.expect.used('sfail', count=0)
    t.expect.used('pfail', activator='player2', count=1)
    t.expect.used('ppass', count=0)


@S.test('player_hasinventory personal: pass for the holder, fail for another player, nothing for the world',
        players=2, doc=DOC, covers=('player_hasinventory', 'player_hasinventory.item_name',
                                    'player_hasinventory.pass_target', 'player_hasinventory.fail_target'))
def has_personal(t):
    card(t)
    t.point('player_hasinventory', targetname='has', item_name=K(t), pass_target='pass', fail_target='fail')
    t.point('info_target', targetname='pass')
    t.point('info_target', targetname='fail')
    take(t, 'player1')
    t.fire('has', activator='player1')
    t.fire('has', activator='player2')
    t.fire('has', activator='world')
    t.wait(0.2)
    t.expect.used('pass', activator='player1', count=1)
    t.expect.used('fail', activator='player2', count=1)


@S.test('master: a door with player_hasinventory as master opens for the card holder only', players=2, doc=DOC,
        covers=('player_hasinventory', 'func_door.master'))
def master(t):
    card(t)
    t.point('player_hasinventory', targetname='has', item_name=K(t))
    t.brush('func_door', targetname='door', size=(16, 128, 128), at=(200, 0, 0), master='has', speed='1000',
            angles='-90 0 0', lip='8', wait='-1')
    t.snap('closed', 'door', 'origin')
    t.fire('door', activator='player2')
    t.wait(0.5)
    t.snap('p2', 'door', 'origin')
    take(t, 'player1')
    t.fire('door', activator='player1')
    t.wait(0.5)
    t.snap('p1', 'door', 'origin')
    t.expect.same('closed', 'p2', 'door', 'origin')
    t.expect.value('p1', 'door', 'origin.z', 0, op='>')


# ---------------------------------------------------------------------------------------------- player_inventory

@S.test('player_inventory: Give, Remove, Set by Use type (On / Off / Set value), On Count Change', players=1, doc=DOC,
        covers=('player_inventory', 'player_inventory.item_name', 'player_inventory.item_icon',
                'player_inventory.operation=0', 'player_inventory.operation=1', 'player_inventory.operation=-1',
                'player_inventory.operation=2', 'player_inventory.count', 'player_inventory.on_count_change'))
def operations(t):
    t.point('player_inventory', targetname='give', item_name='coin', item_icon='inv_key', operation='1', count='3',
            on_count_change='changed')
    t.point('player_inventory', targetname='take', item_name='coin', operation='-1', count='1')
    t.point('player_inventory', targetname='set', item_name='coin', operation='2', count='7')
    t.point('player_inventory', targetname='input', item_name='coin', operation='0', count='2')
    t.point('info_target', targetname='changed')
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.snap('give', 'player1', 'inv:coin')
    t.fire('take', activator='player1')
    t.wait(0.1)
    t.snap('take', 'player1', 'inv:coin')
    t.fire('set', activator='player1')
    t.wait(0.1)
    t.snap('set', 'player1', 'inv:coin')
    t.fire('input', activator='player1', use='on')
    t.wait(0.1)
    t.snap('on', 'player1', 'inv:coin')
    t.fire('input', activator='player1', use='off')
    t.wait(0.1)
    t.snap('off', 'player1', 'inv:coin')
    t.fire('input', activator='player1', use='set', value=11)
    t.wait(0.1)
    t.snap('setv', 'player1', 'inv:coin')
    t.fire('give', activator='world')       # personal: the world is not a player in play
    t.wait(0.1)
    t.snap('world', 'player1', 'inv:coin')
    t.expect.value('give', 'player1', 'inv:coin', 3)
    t.expect.value('take', 'player1', 'inv:coin', 2)
    t.expect.value('set', 'player1', 'inv:coin', 7)
    t.expect.value('on', 'player1', 'inv:coin', 9)
    t.expect.value('off', 'player1', 'inv:coin', 7)
    t.expect.value('setv', 'player1', 'inv:coin', 11)
    t.expect.value('world', 'player1', 'inv:coin', 11)
    t.expect.used('changed', activator='player1', count=1)


@S.test('player_inventory Maximum: On Max Count Limit at the maximum, On Count Overflow (clamped) when going over, '
        'Allow going over Maximum', players=1, doc=DOC,
        covers=('player_inventory', 'player_inventory.collect_limit', 'player_inventory.on_max_count_limit',
                'player_inventory.on_count_overflow', 'player_inventory:2'))
def maximum(t):
    t.point('player_inventory', targetname='give', item_name='ammo_box', operation='1', count='3',
            collect_limit='5', on_max_count_limit='atmax', on_count_overflow='over')
    t.point('player_inventory', targetname='giveo', item_name='more', operation='1', count='3', collect_limit='5',
            spawnflags='2', on_count_overflow='over2')
    for n in ('atmax', 'over', 'over2'):
        t.point('info_target', targetname=n)
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.fire('give', activator='player1')     # 6 > 5: clamped to 5, overflow
    t.wait(0.1)
    t.fire('give', activator='player1')     # at 5: nothing, max count
    t.wait(0.1)
    t.fire('giveo', activator='player1')
    t.wait(0.1)
    t.fire('giveo', activator='player1')     # 6 allowed
    t.wait(0.1)
    t.snap('s', 'player1', 'inv:ammo_box', 'inv:more')
    t.expect.value('s', 'player1', 'inv:ammo_box', 5)
    t.expect.value('s', 'player1', 'inv:more', 6)
    t.expect.used('over', count=1)
    t.expect.used('atmax', count=1)
    t.expect.used('over2', count=1)


@S.test('player_inventory: On Item Limit when 16 different items are held; Remove On Fire removes it', players=1,
        doc=DOC, covers=('player_inventory', 'player_inventory.on_item_limit', 'player_inventory:1'))
def item_limit(t):
    for i in range(16):
        t.point('player_inventory', targetname='g%d' % i, item_name='thing%d' % i, operation='1')
    t.point('player_inventory', targetname='g16', item_name='thing16', operation='1', on_item_limit='full')
    t.point('player_inventory', targetname='once', item_name='once', operation='1', spawnflags='1')
    t.point('info_target', targetname='full')
    revive(t)       # an empty list (other tests' items)
    for i in range(17):
        t.fire('g%d' % i, activator='player1')
    t.fire('once', activator='player1')
    t.wait(0.2)
    t.snap('s', 'player1', 'inv:thing15', 'inv:thing16')
    revive(t)
    t.expect.value('s', 'player1', 'inv:thing15', 1)
    t.expect.value('s', 'player1', 'inv:thing16', 0)
    t.expect.used('full', count=1)
    t.expect.removed('once')


@S.test('player_inventory takes an item that came from an item_pickup: the pickup is used up (hidden) until the '
        'round restart puts it back', players=1, doc=DOC,
        covers=('player_inventory', 'player_inventory.operation=-1', 'item_pickup'))
def use_up(t):
    card(t)
    t.point('player_inventory', targetname='take', item_name=K(t), operation='-1', count='1')
    take(t, 'player1')
    t.fire('take', activator='player1')
    t.wait(0.2)
    t.snap('used', 'card', 'kv:inv_state', 'effects')
    t.snap('used', 'player1', 'inv:' + K(t))
    revive(t)
    t.snap('round', 'card', 'kv:inv_state')
    t.expect.value('used', 'card', 'kv:inv_state', SPENT)
    t.expect.value('used', 'card', 'effects', 128, op='&')
    t.expect.value('used', 'player1', 'inv:' + K(t), 0)
    t.expect.value('round', 'card', 'kv:inv_state', HOME)


@S.test('player_inventory Shared: the world fills the team pool; round_reset 1 takes back what it gave at the round '
        'start; blank item_name with Remove and Count 0 takes every item', players=1, doc=DOC,
        covers=('player_inventory', 'player_inventory:4', 'player_inventory.round_reset=1',
                'player_inventory.round_reset=0'))
def inventory_shared(t):
    t.point('player_inventory', targetname='give', item_name='fuel', operation='1', count='4', spawnflags='4',
            round_reset='1')
    t.point('player_inventory', targetname='keep', item_name='parts', operation='1', count='2', spawnflags='4',
            round_reset='0')
    t.point('player_inventory', targetname='mine', item_name='mine', operation='1', count='2')
    t.point('player_inventory', targetname='clear', operation='-1', count='0')
    t.fire('give', activator='world')
    t.fire('keep', activator='world')
    t.fire('mine', activator='player1')
    t.wait(0.1)
    t.snap('given', 'player1', 'pool:fuel', 'pool:parts', 'inv:mine')
    t.fire('clear', activator='player1')
    t.wait(0.1)
    t.snap('cleared', 'player1', 'inv:mine', 'pool:parts')
    revive(t)
    t.snap('round', 'player1', 'pool:fuel', 'pool:parts')
    t.console('test_inventory pool parts -2')
    t.wait(0.1)
    t.expect.value('given', 'player1', 'pool:fuel', 4)
    t.expect.value('given', 'player1', 'pool:parts', 2)
    t.expect.value('given', 'player1', 'inv:mine', 2)
    t.expect.value('cleared', 'player1', 'inv:mine', 0)
    t.expect.value('cleared', 'player1', 'pool:parts', 2)
    t.expect.value('round', 'player1', 'pool:fuel', 0)
    t.expect.value('round', 'player1', 'pool:parts', 2)


@S.test('player_calc_ratio Inventory item count: the activator\'s count; as a fraction 1 while he has any',
        players=1, doc=DOC,
        covers=('player_calc_ratio', 'player_calc_ratio.calc_param=4', 'player_calc_ratio.item_name',
                'player_calc_ratio.value_notion=1'))
def calc_ratio(t):
    t.point('player_inventory', targetname='give', item_name='coin', operation='1', count='3')
    t.point('player_calc_ratio', targetname='n', calc_param='4', item_name='coin', value_notion='0')
    t.point('player_calc_ratio', targetname='f', calc_param='4', item_name='coin', value_notion='1')
    t.point('locus_variable', targetname='lvn', m_iszRatio='n', m_iszPosition='', m_iszVelocity='',
            names=('m_iszRatio',))
    t.point('locus_variable', targetname='lvf', m_iszRatio='f', m_iszPosition='', m_iszVelocity='',
            names=('m_iszRatio',))
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.fire('lvn', activator='player1')
    t.fire('lvf', activator='player1')
    t.wait(0.1)
    t.snap('s', 'lvn', 'frags')
    t.snap('s', 'lvf', 'frags')
    t.expect.value('s', 'lvn', 'frags', 3)
    t.expect.value('s', 'lvf', 'frags', 1)


@S.test('fullupdate: the player\'s items and the shared pool are sent again', players=1, doc=DOC,
        covers=('item_pickup', 'player_inventory'))
def fullupdate(t):
    t.point('player_inventory', targetname='give', item_name='badge', operation='1', count='2')
    t.point('player_inventory', targetname='team', item_name='flag', operation='1', spawnflags='4')
    t.fire('give', activator='player1')
    t.fire('team', activator='world')
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.5)
    t.expect.msg('Inventory', to='player1', count=1, after='fu', fields={'count': 2, 'item': 'badge', 'shared': 0})
    t.expect.msg('Inventory', to='player1', min=1, after='fu', fields={'count': 1, 'item': 'flag', 'shared': 1})


# ---------------------------------------------------------------------------------------------- not in play (last)

@S.test('Game Master: his items go back at once; he can\'t take any', players=2, doc=DOC,
        covers=('item_pickup',))
def game_master(t):
    t.console('mp_gmmode 1')
    card(t, name='c1', at=(120, 0, 16))
    card(t, name='c2', item='other', at=(120, 120, 16))
    take(t, 'player2', (120, 0, 0))
    t.cmd('player2', 'gmon')
    t.wait(0.3)
    t.cmd('player2', 'gm')
    t.wait(0.3)
    t.snap('gm', 'c1', 'kv:inv_state')
    t.snap('gm', 'player2', 'game_master', 'inv:' + K(t))
    t.teleport('player2', (120, 120, 0))
    t.wait(0.3)
    t.snap('touch', 'c2', 'kv:inv_state')
    t.cmd('player2', 'gm_leave')
    t.wait(0.3)
    revive(t)
    t.console('mp_gmmode 0')
    t.expect.value('gm', 'player2', 'game_master', 1)
    t.expect.value('gm', 'c1', 'kv:inv_state', HOME)
    t.expect.value('gm', 'player2', 'inv:' + K(t), 0)
    t.expect.value('touch', 'c2', 'kv:inv_state', HOME)


@S.test('disconnect: the leaving player\'s items go back to their spots at once', players=2, doc=DOC,
        covers=('item_pickup', 'item_pickup.holder_keep_on_death=1'))
def disconnect(t):
    card(t, holder_keep_on_death='1')
    take(t, 'player2')
    t.snap('held', 'card', 'kv:inv_state')
    t.console('test_player remove player2')
    t.wait(0.5)
    t.snap('left', 'card', 'kv:inv_state', 'effects')
    t.expect.value('held', 'card', 'kv:inv_state', HELD)
    t.expect.value('left', 'card', 'kv:inv_state', HOME)
    t.expect.value('left', 'card', 'effects', 128, op='!&')


@S.test('spectator: his items go back at once; a spectator can\'t take items or be given any', players=1, doc=DOC,
        covers=('item_pickup', 'player_inventory'))
def spectator(t):
    card(t, name='c1', at=(120, 0, 16))
    card(t, name='c2', item='other', at=(120, 120, 16))
    t.point('player_inventory', targetname='give', item_name='coin', operation='1')
    take(t, 'player1', (120, 0, 0))
    t.cmd('player1', 'spectate')
    t.wait(1.0)
    t.snap('spec', 'c1', 'kv:inv_state')
    t.snap('spec', 'player1', 'inv:' + K(t))
    t.teleport('player1', (120, 120, 0))
    t.wait(0.3)
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.snap('touch', 'c2', 'kv:inv_state')
    t.snap('touch', 'player1', 'inv:coin')
    t.expect.value('spec', 'c1', 'kv:inv_state', HOME)
    t.expect.value('spec', 'player1', 'inv:' + K(t), 0)
    t.expect.value('touch', 'c2', 'kv:inv_state', HOME)
    t.expect.value('touch', 'player1', 'inv:coin', 0)
