"""The six SBO "hunter" monsters' sight: monster_grunt_hunter (grunt_hunter.cpp), monster_imp_hunter (imp_hunter.cpp),
monster_cyberminotor (minotaure.cpp), monster_hunter (hunter2.cpp), monster_zombie_hunter (zombies_hunter.cpp) and
monster_werewolf_hunter (werewolf_hunter.cpp). Each has its own Look, as the Big Cyborg (suite `cyborg`): every monster
and player within 12000 units, through walls and without a view cone (1.38), skipping an entity with FL_NOTARGET (the
`notarget` cheat, observers), as CBaseMonster::Look does (1.38: they took a notarget player as their enemy).

They see through walls, so each test's monster is made by a monstermaker once the player is in place (and notarget
set), and is removed at the test's end (as in suite `minos`). A named monster starts in SCHED_WAIT_TRIGGER (TASK_WAIT_PVS), which the engine's
FIND_CLIENT_IN_PVS never ends for a notarget player: it is used once to wake it (wake), as a map's unnamed monster
would be awake."""

from etest import Suite

SUITE = Suite('hunters', 'the six hunter monsters: FL_NOTARGET players are not seen (their own Look)')
S = SUITE
ROOM = (512, 512, 512)

HUNTERS = (
    ('grunt_hunter', 'monster_grunt_hunter'),
    ('imp_hunter', 'monster_imp_hunter'),
    ('cyberminotor', 'monster_cyberminotor'),
    ('hunter', 'monster_hunter'),
    ('zombie_hunter', 'monster_zombie_hunter'),
    ('werewolf_hunter', 'monster_werewolf_hunter'),
)


def player_and_maker(t, classname):
    """player1 (tough: he must not die) facing west, and a monstermaker for the monster 350 units ahead of him at the
    west wall, facing east (made by wake)."""
    t.point('monstermaker', at=(-200, 0, 0), targetname='make_npc', monstertype=classname, monstercount='1',
            m_imaxlivechildren='1', netname='npc', angles='0 0 0', names=('netname',))
    t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
    t.console('wait_for alive player1 20')
    t.teleport('player1', (150, 0, 0), yaw=180)
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')


def wake(t):
    t.fire('make_npc')
    t.wait(0.3)
    t.kv('npc', 'health', '100000')
    t.fire('npc', use='on')


def done(t):
    t.fire('kill_npc')
    t.flags('player1', '-FL_NOTARGET')
    t.wait(0.2)


def make_tests(key, classname):
    def notarget(t):
        player_and_maker(t, classname)
        t.flags('player1', '+FL_NOTARGET')
        t.wait(0.3)
        t.mark('awake')
        wake(t)
        t.wait(1.5)
        t.snap('a', 'npc', 'enemy')
        t.wait(1.5)
        t.snap('b', 'npc', 'enemy')
        t.expect.value('a', 'npc', 'enemy', '')     # 1.38: player
        t.expect.value('b', 'npc', 'enemy', '')
        t.expect.not_damaged('player1', after='awake', within=(0, 3.0))
        t.flags('player1', '-FL_NOTARGET')
        t.wait(3.0)
        t.snap('c', 'npc', 'enemy')
        t.expect.value('c', 'npc', 'enemy', 'player')
        done(t)

    def control(t):
        player_and_maker(t, classname)
        t.wait(0.3)
        wake(t)
        t.wait(3.0)
        t.snap('a', 'npc', 'enemy')
        t.expect.value('a', 'npc', 'enemy', 'player')
        done(t)

    notarget.__name__ = key + '_notarget'
    control.__name__ = key + '_control'
    S.test('%s: a player with notarget in front of it is not its enemy and takes no damage; notarget off: he is'
           % classname, players=1, size=ROOM)(notarget)
    S.test('%s control: the same player without notarget is its enemy' % classname, players=1, size=ROOM)(control)


for _key, _classname in HUNTERS:
    make_tests(_key, _classname)


# monster_hunter's health by mp_juniorpvmonst (hunter2.cpp Spawn): 0 Junior 150, 1 Normal 400, 2 HardCore 900
# (1.38: 9000), 3 Survivor 1700
HUNTER_HEALTH = ((0, 150), (1, 400), (2, 900), (3, 1700))


@S.test('monster_hunter health by mp_juniorpvmonst: 150, 400, 900 (HardCore; 1.38: 9000), 1700', players=1, size=ROOM)
def hunter_health(t):
    """One monstermaker per level; each monster is made after its mp_juniorpvmonst value is set and removed after
    its health is read."""
    for level, health in HUNTER_HEALTH:
        t.point('monstermaker', at=(-200, -150 + 100 * level, 0), targetname='make_h%d' % level,
                monstertype='monster_hunter', monstercount='1', m_imaxlivechildren='1', netname='h%d' % level,
                angles='0 0 0', names=('netname',))
        t.point('trigger_relay', targetname='kill_h%d' % level, killtarget='h%d' % level)
    t.console('wait_for alive player1 20')
    t.teleport('player1', (150, 0, 0), yaw=180)
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.flags('player1', '+FL_NOTARGET')
    for level, health in HUNTER_HEALTH:
        t.console('mp_juniorpvmonst %d' % level)
        t.fire('make_h%d' % level)
        t.wait(0.3)
        t.snap('l%d' % level, 'h%d' % level, 'health')
        t.expect.value('l%d' % level, 'h%d' % level, 'health', health, tol=0)
        t.fire('kill_h%d' % level)
        t.wait(0.2)
    t.console('mp_juniorpvmonst 1')
    t.flags('player1', '-FL_NOTARGET')
