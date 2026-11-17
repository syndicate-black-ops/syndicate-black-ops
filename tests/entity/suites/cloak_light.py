"""The cloak in the dark (Cloak_HidesFrom, player.cpp): a monster's sight range against a cloaked player shrinks with
the player's light, down to half at a light of 16 or less (full at 64 or more); a flashlight that is on cancels it.
The light is the one a player's client reports in its moves (pev->light_level, which GETENTITYILLUM returns for a
player); the test player reports it with test_lightlevel (200: a lit room, 0: dark).
The cloaked player is kept hurt (world damage every 0.3 s: render amount 160), so he is seen within
2048 * (160 / 255) ^ 1.5 = about 1017 units when lit and about 508 in the dark; each test's Fed stands 636 units from
him. Suite cloak: the rest of the cloak."""

from etest import Suite

SUITE = Suite('cloak_light', 'Cloak: a hurt, cloaked player 636 units away is seen in a lit room, not in a dark one '
              'unless his flashlight is on', players=2)
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
EF_DIMLIGHT = 8


def setup(t, lit):
    """A Fed in the south-west corner facing north-east; player1 (tough, cloaked) in the north-east corner, 636 units
    away, facing it. lit: the player reports light 200, else 0. The Fed waits as a prisoner (sees nobody) until the
    player is in place."""
    t.cmd('player1', 'cyberware', '5')
    t.cmd('player2', 'cyberware', '5')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(2.5)
    t.point('monster_human_grunt', at=(-220, -220, 0), targetname='npc', angles='0 45 0', spawnflags='16')
    t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
    t.teleport('player1', (230, 230, 0), yaw=225)
    t.kv('npc', 'health', '100000')     # the player's .444 shot must not kill it
    t.teleport('player2', (230, -200, 0), yaw=180)
    t.flags('player2', '+FL_NOTARGET')
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.kv('player1', 'armorvalue', '0')
    t.console('test_lightlevel %s %d' % (t.ref('player1'), 200 if lit else 0))
    t.cmd('player1', 'cloak')
    t.wait(1.0)


def wake_and_look(t):
    """Wakes the Fed while the player is kept hurt for 2.4 s; snapshots at the end."""
    t.kv('npc', 'spawnflags', '0')
    t.fire('npc', use='on')
    for _ in range(8):
        t.damage('player1', 1, 0)
        t.wait(0.3)
    t.snap('a', 'npc', 'enemy')
    t.snap('p', 'player1', 'renderamt', 'effects')


def done(t):
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')
    t.wait(0.2)


@S.test('lit room: the Fed takes the hurt, cloaked player 636 units away as its enemy', players=2, doc=DOC)
def lit_seen(t):
    setup(t, True)
    wake_and_look(t)
    t.expect.value('p', 'player1', 'renderamt', 160)
    t.expect.value('a', 'npc', 'enemy', 'player')
    done(t)


@S.test('dark room: the same Fed does not see him (half the range: about 508 units)', players=2, doc=DOC)
def dark_hidden(t):
    setup(t, False)
    wake_and_look(t)
    t.expect.value('p', 'player1', 'renderamt', 160)
    t.expect.value('a', 'npc', 'enemy', '')
    done(t)


@S.test('dark room, flashlight on: the Fed sees him (no darkness bonus)', players=2, doc=DOC)
def dark_flashlight(t):
    setup(t, False)
    t.kv('player1', 'impulse', '100')       # the flashlight
    t.wait(0.3)
    wake_and_look(t)
    t.expect.value('p', 'player1', 'effects', EF_DIMLIGHT, op='&')
    t.expect.value('a', 'npc', 'enemy', 'player')
    done(t)
