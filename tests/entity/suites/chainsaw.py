"""Chainsaw: its looped idle sound (weapons/chain_idle.wav, CHAN_WEAPON on the player) stops whenever the chainsaw is
put away (CChainsaw::Holster), and putting it away plays no holster sound (weapons/chain_holster.wav). A disconnect
takes the strip's path (RemoveAllItems)."""

from etest import Suite

SUITE = Suite('chainsaw', 'Chainsaw: idle loop stopped on every holster path, no holster sound', players=2)
S = SUITE
ROOM = (512, 512, 256)
SND_STOP = 32


def chainsaw_up(t, at):
    t.point('weapon_chainsaw', at=at)
    t.teleport('player1', at, yaw=90)
    t.kv('player1', 'health', '100000')
    t.wait(0.5)
    t.cmd('player1', 'weapon_chainsaw')
    t.wait(1.5)


def put_away(t, mark):
    """The idle loop was stopped after `mark`, and no holster sound was played."""
    t.expect.sound('chain_idle', ent='player1', after=mark, flags=SND_STOP, min=1)
    t.expect.sound('chain_holster', ent='player1', after=mark, count=0)


@S.test('chainsaw out: the idle loop starts', players=2, size=ROOM)
def idle_starts(t):
    t.mark('m')
    chainsaw_up(t, (0, -150, 0))
    t.expect.sound('chain_idle', ent='player1', after='m', min=1)


@S.test('chainsaw out, switch to the fists: idle loop stopped, no holster sound', players=2, size=ROOM)
def switch(t):
    chainsaw_up(t, (0, -150, 0))
    t.mark('switch')
    t.cmd('player1', 'weapon_wong')
    t.wait(1.0)
    put_away(t, 'switch')


@S.test('chainsaw out, dropped: idle loop stopped, no holster sound', players=2, size=ROOM)
def drop(t):
    chainsaw_up(t, (-150, -150, 0))
    t.mark('drop')
    t._step('test_drop', t.ref('player1'))
    t.wait(1.0)
    put_away(t, 'drop')


@S.test('chainsaw out, player_weaponstrip: idle loop stopped, no holster sound', players=2, size=ROOM)
def strip(t):
    t.point('player_weaponstrip', targetname='strip')
    chainsaw_up(t, (-150, 150, 0))
    t.teleport('player1', (150, 150, 0), yaw=90)   # off the chainsaw's placed spot (the strip puts it back there)
    t.wait(0.3)
    t.mark('strip')
    t.fire('strip', activator='player1')
    t.wait(1.0)
    put_away(t, 'strip')


@S.test('chainsaw out, player killed: idle loop stopped, no holster sound', players=2, size=ROOM)
def death(t):
    chainsaw_up(t, (150, -150, 0))
    t.kv('player1', 'armorvalue', '0')
    t.kv('player1', 'health', '1')
    t.mark('death')
    t.damage('player1', 5, 0)
    t.wait(1.0)
    put_away(t, 'death')
