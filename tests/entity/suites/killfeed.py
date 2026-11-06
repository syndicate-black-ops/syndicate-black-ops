"""Kill feed weapon codes (docs/dev/KILLFEED_PORT.md "SBO weapons"): the DeathMsg weapon string the server sends for a
projectile kill, which the client draws as d_<code> from sprites/hud.txt (else the skull). A test player shoots
another one (mp_friendlyfire 1) with the weapon of its loadout ("heavy" choice, then a new round)."""

from etest import Suite

SUITE = Suite('killfeed', 'Kill feed: DeathMsg weapon codes of projectile kills', players=2)
S = SUITE
DOC = 'dev/KILLFEED_PORT.md'


def duel(t, loadout, weapon):
    """player1 (the shooter, tough) 300 units west of player2 (1 health), facing it, holding `weapon`."""
    t.console('mp_friendlyfire 1')
    t.cmd('player1', *loadout)
    t.console('test_round restart')     # both alive again (the last test's victim), with the new loadout
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(0.5)
    t.teleport('player1', (-150, 0, 0), yaw=0)
    t.teleport('player2', (150, 0, 0), yaw=180)
    t.kv('player1', 'health', '1000')
    t.kv('player2', 'health', '1')
    t.cmd('player1', weapon)
    t.wait(1.5)
    t.look('player1', 0, 0)
    t.mark('fire')


@S.test('plasma sniper, unzoomed: the plasma bolt kills -> "plasma" (d_plasma = d_sw)', players=2, doc=DOC)
def plasma(t):
    duel(t, ('heavy', '3'), 'weapon_sw')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.0)
    t.expect.spawned('@plasma', min=1)
    t.expect.msg('DeathMsg', fields={'killer': 1, 'victim': 2, 'weapon': 'plasma'}, after='fire', within=(0, 1.0))


@S.test('grenade launcher: a contact grenade kills -> "glauncher", not "grenade"', players=2, doc=DOC)
def glauncher(t):
    duel(t, ('heavy', '7'), 'weapon_glauncher')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.0)
    t.expect.msg('DeathMsg', fields={'killer': 1, 'victim': 2, 'weapon': 'glauncher'}, after='fire', within=(0, 1.0))


@S.test('hand grenade: a timed grenade kills -> "grenade"', players=2, doc=DOC)
def handgrenade(t):
    duel(t, ('heavy', '1'), 'weapon_handgrenade')    # item 1 (the join's default): hand grenades
    t.look('player1', 10, 0)    # a little down: it lands near player2
    t.move('player1', 0, 0, 10, '+attack')     # pull the pin; thrown on release (CHandGrenade::WeaponIdle)
    t.wait(5.0)
    t.expect.msg('DeathMsg', fields={'killer': 1, 'victim': 2, 'weapon': 'grenade'}, after='fire', within=(0, 5.0))


# NPC kills (docs/dev/KILLFEED_PORT.md, mp_killfeed_npc): with mp_killfeed_npc 1 a player's kill of an NPC sends an
# SboDeathMsg with the NPC's kill feed name as the victim (SboKillfeed_NpcKilled). dark_grunt has no monster_ prefix
# (its name comes from the table: "Fed"); the turrets die in TakeDamage, not in Killed.
def npc_kill(t, classname, name, at=(150, 0, 0), **kv):
    t.point(classname, at=at, targetname='npc', **kv)
    t.console('mp_killfeed_npc 1')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.mark('kill')
    t.damage('npc', 5000, 0, 'player1')
    t.wait(0.5)
    t.expect.msg('SboDeathMsg', fields={'flags': 0, 'killer': 1, 'killer_name': '', 'victim': 0, 'victim_name': name},
                 after='kill', within=(0, 0.5), count=1)


@S.test('mp_killfeed_npc 1: a player kills a dark_grunt -> SboDeathMsg victim "Fed"', players=2, doc=DOC)
def npc_dark_grunt(t):
    npc_kill(t, 'dark_grunt', 'Fed')


@S.test('mp_killfeed_npc 1: a player kills a monster_human_grunt -> SboDeathMsg victim "Fed"', players=2, doc=DOC)
def npc_human_grunt(t):
    npc_kill(t, 'monster_human_grunt', 'Fed')


@S.test('mp_killfeed_npc 1: a player kills a monster_turret -> SboDeathMsg victim "Auto Turret"', players=2, doc=DOC)
def npc_turret(t):
    npc_kill(t, 'monster_turret', 'Auto Turret', at=(150, 0, 200), orientation='1')


@S.test('mp_killfeed_npc 1: a player kills a monster_miniturret -> SboDeathMsg victim "Mini Auto Turret"', players=2,
        doc=DOC)
def npc_miniturret(t):
    npc_kill(t, 'monster_miniturret', 'Mini Auto Turret', at=(150, 0, 200), orientation='1')


@S.test('mp_killfeed_npc 1: a player kills a monster_sentry -> SboDeathMsg victim "Sentry Gun"', players=2, doc=DOC)
def npc_sentry(t):
    npc_kill(t, 'monster_sentry', 'Sentry Gun')


@S.test('mp_killfeed_npc 2 (default): a player kills a dark_grunt -> no SboDeathMsg', players=2, doc=DOC)
def npc_dark_grunt_default(t):
    t.point('dark_grunt', at=(150, 0, 0), targetname='npc')
    t.console('mp_killfeed_npc 2')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.mark('kill')
    t.damage('npc', 5000, 0, 'player1')
    t.wait(0.5)
    t.expect.no_msg('SboDeathMsg', after='kill', within=(0, 0.5))


# A Big Cyborg (monster_cyborg) shoots the same plasma bolt as the plasma sniper (class "plasma", CPlasma). Its kill
# is the NPC's own attack: weapon "cyborg" (no d_cyborg in hud.txt: the skull, like other NPC kills), killer name
# "Big Cyborg" -- never "plasma" (d_plasma, the sniper icon), which stays for a player's bolt (test 'plasma' above).
@S.test('monster_cyborg kills a player with its plasma bolt -> weapon "cyborg" (the skull), killer "Big Cyborg"',
        players=2, doc=DOC)
def npc_cyborg_plasma(t):
    # It sees through walls (CCyborg::Look, 12000 units): it waits as a prisoner (spawnflag 16:
    # it sees nobody) until both players are in place, then takes the nearer one, player2, 300 units ahead (past its
    # 200-unit kick); player1 stands farther away, off the line of fire
    t.point('monster_cyborg', at=(-200, 0, 0), targetname='npc', angles='0 0 0', spawnflags='16')
    t.console('mp_killfeed_npc 2')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.teleport('player1', (230, -230, 0), yaw=180)
    t.teleport('player2', (100, 0, 0), yaw=180)
    t.kv('player1', 'health', '1000')
    t.kv('player2', 'health', '1')
    t.wait(0.5)
    t.mark('fire')
    t.kv('npc', 'spawnflags', '0')
    t.wait(8.0)
    t.expect.spawned('@plasma', min=1)
    t.expect.damaged('player2', attacker='npc', after='fire')
    t.expect.msg('SboDeathMsg', fields={'killer': 0, 'killer_name': 'Big Cyborg', 'victim': 2, 'weapon': 'cyborg'},
                 after='fire', count=1)
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 2, 'weapon': 'cyborg'}, after='fire', count=1)
