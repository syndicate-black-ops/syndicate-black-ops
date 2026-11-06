"""monster_gargantua's stomp (garg_stomp, gargantua.cpp CStomp): its damage and kills are the Gargantua's (the
stomp's owner, as in SevenKewp), with the stomp as the inflictor. When the Gargantua is gone before the stomp hits
(removed after its death, its edict free or reused), the stomp is its own attacker, as before the fix.

The Gargantua stands in a 56-high cage (it cannot walk up to the player) and the test player on a 400-high pillar,
over 600 units away (past the flame thrower's reach): the stomp is its first attack. It sees the player for 5 s
before it stomps. In the tests after the first, the stomp spawns 6.4-6.6 s after the Gargantua is let go and
reaches the player 5.6 s later (the first test's Gargantua is slower to stomp: 9.9 s); timings from the trace.

The last test: a player who hurts a Gargantua gets the XP for it (CGargantua::TakeDamage), up to a level-up."""

from etest import Suite

SUITE = Suite('gargstomp', 'Gargantua stomp: kill credit to the Gargantua', players=1)
S = SUITE
DOC = 'dev/KILLFEED_PORT.md'
ROOM = (512, 512, 1024)
GX, GY = -180, -180
DMG_BLAST = 64


def arena(t, health):
    t.point('monster_gargantua', at=(GX, GY, 0), targetname='garg', angles='0 45 0', spawnflags='16')
    for dx, dy, sx, sy in ((-56, 0, 16, 128), (56, 0, 16, 128), (0, -56, 128, 16), (0, 56, 128, 16)):
        t.brush('func_wall', size=(sx, sy, 56), at=(GX + dx, GY + dy, 0))
    t.brush('func_wall', size=(64, 64, 400), at=(200, 200, 0))
    t.console('wait_for alive player1 20')
    t.teleport('player1', (178, 178, 400), yaw=225)
    t.kv('player1', 'health', str(health))
    t.wait(0.5)
    t.mark('go')
    t.kv('garg', 'spawnflags', '0')     # no longer a prisoner: it sees the player


@S.test('a stomp kills a player: the Gargantua is the attacker, DeathMsg weapon "garg_stomp"', players=1, size=ROOM,
        doc=DOC)
def kill_credit(t):
    arena(t, 20)
    t.wait(20)
    t.expect.spawned('@garg_stomp', min=1)
    t.expect.damaged('player1', attacker='garg', after='go')
    t.expect.not_damaged('player1', attacker='@garg_stomp', after='go')
    t.expect.msg('SboDeathMsg', fields={'killer': 0, 'killer_name': 'Gargantua', 'victim': 1, 'weapon': 'garg_stomp'},
                 after='go', count=1)
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 1, 'weapon': 'garg_stomp'}, after='go', count=1)


@S.test('the Gargantua is removed while its stomp flies: the stomp kills as its own attacker, no crash', players=1,
        size=ROOM, doc=DOC)
def owner_removed(t):
    arena(t, 20)
    t.wait(8.5)                         # the stomp is on its way (spawned at about 6.5 s, hits at about 12 s)
    t.mark('kill')
    t.damage('garg', 100000, DMG_BLAST)   # dies; removed 1.75 s later (TASK_DIE), its edict reused by stomp trails
    t.wait(8)
    t.expect.spawned('@garg_stomp', min=1)
    t.expect.removed('garg')
    t.expect.damaged('player1', attacker='@garg_stomp', after='kill')
    t.expect.msg('SboDeathMsg', fields={'killer': 0, 'killer_name': 'Gargantua', 'victim': 1, 'weapon': 'garg_stomp'},
                 after='kill', count=1)
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 1, 'weapon': 'garg_stomp'}, after='kill', count=1)


@S.test('the Gargantua dies (not yet removed) while its stomp flies: the dead Gargantua keeps the credit', players=1,
        size=ROOM, doc=DOC)
def owner_dead(t):
    arena(t, 20)
    t.wait(11.2)                        # the stomp reaches the player at about 12 s; the body stays 1.75 s
    t.mark('kill')
    t.damage('garg', 100000, DMG_BLAST)
    t.wait(6)
    t.expect.damaged('player1', attacker='garg', after='kill')
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 1, 'weapon': 'garg_stomp'}, after='kill', count=1)


@S.test('a player hurts the Gargantua: he gets the XP, and a level-up past 5000 (level, frags, SboXP)', players=1,
        size=ROOM, doc=DOC)
def player_xp(t):
    t.point('monster_gargantua', at=(GX, GY, 0), targetname='garg', angles='0 45 0', spawnflags='16')
    t.console('mp_XP 1')
    t.console('wait_for alive player1 20')
    t.console('test_player level player1 0')
    t.wait(0.5)
    t.snap('before', 'player1', 'level', 'total_xp', 'frags')
    t.damage('garg', 100, 0, 'player1')             # 100 XP (CGargantua::TakeDamage: the damage)
    t.wait(0.2)
    t.snap('hit', 'player1', 'level', 'total_xp', 'frags')
    t.mark('blast')
    t.damage('garg', 500, DMG_BLAST, 'player1')     # blast x10: 5000, past the level-up (SBO_XP_GAIN stops at 5000)
    t.wait(0.3)
    t.snap('levelup', 'player1', 'level', 'total_xp', 'frags')
    t.expect.value('before', 'player1', 'total_xp', 0)
    t.expect.value('hit', 'player1', 'total_xp', 100)
    t.expect.value('hit', 'player1', 'level', 0)
    t.expect.value('levelup', 'player1', 'total_xp', 5000)
    t.expect.value('levelup', 'player1', 'level', 1)
    t.expect.value('levelup', 'player1', 'frags', 1)
    t.expect.msg('SboXP', fields={'client': 1, 'xp': 5000}, after='blast', within=(0, 0.3))
