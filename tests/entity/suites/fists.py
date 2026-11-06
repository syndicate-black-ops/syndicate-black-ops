"""Fists (weapon_wong) with no world model: placed in a map, picked up, given by game_player_equip.

Its own suite: a game_player_equip in the map replaces every player's default fists at spawn
(multiplay_gamerules.cpp / coop.cpp PlayerSpawn), which the weapons suite's punch test needs."""

from etest import Suite

SUITE = Suite('fists', 'weapon_wong: no world model (placed, picked up, game_player_equip)')
S = SUITE
ROOM = (512, 512, 1024)


@S.test('Wong: no world model; placed in the air it falls and is picked up, game_player_equip gives it', players=1,
        size=ROOM)
def wong_no_model(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='equip', spawnflags='1', weapon_wong='1')     # Use Only
    t.point('weapon_wong', at=(150, 150, 96), targetname='fists')
    t.teleport('player1', (-150, -150, 0), yaw=90)
    t.wait(1.0)
    t.snap('landed', 'fists', 'modelindex', 'solid', 'flags')
    t.fire('strip', activator='player1')       # whatever the spawn gave
    t.wait(0.5)
    t.mark('stripped')
    t.teleport('player1', (150, 150, 0), yaw=90)
    t.wait(1.0)
    t.snap('taken', 'fists', 'movetype')
    t.fire('strip', activator='player1')
    t.wait(0.5)
    t.mark('stripped2')
    t.fire('equip', activator='player1')
    t.wait(0.5)
    t.expect.value('landed', 'fists', 'modelindex', 0)
    t.expect.value('landed', 'fists', 'solid', 1)                  # SOLID_TRIGGER
    t.expect.value('landed', 'fists', 'flags', 512, op='&')        # FL_ONGROUND: it fell to the floor
    t.expect.msg('WeapPickup', to='player1', after='stripped', count=2)     # the placed one, then the equip
    t.expect.value('taken', 'fists', 'movetype', 12)               # MOVETYPE_FOLLOW: attached to the player
    t.expect.msg('WeapPickup', to='player1', after='stripped2', count=1)
