"""monster_cyborg (Cyborg) sight, src/dlls/cyborg.cpp: CCyborg::Look sees every monster and player within 12000
units, through walls and without a view cone (1.38), and skips an entity with FL_NOTARGET (the `notarget` cheat,
observers), as CBaseMonster::Look does (1.38: it took a notarget player as its enemy).

The cyborg sees through walls, so each test's cyborg waits as a prisoner (spawnflag 16: it sees nobody) until the
player is in place, and is removed at the test's end. A named monster starts in SCHED_WAIT_TRIGGER (TASK_WAIT_PVS),
which the engine's FIND_CLIENT_IN_PVS never ends for a notarget player: it is used once to wake it (wake), as a
map's unnamed cyborg would be awake."""

from etest import Suite

SUITE = Suite('cyborg', 'monster_cyborg: FL_NOTARGET players are not seen (CCyborg::Look)')
S = SUITE
ROOM = (512, 512, 512)
SF_PRISONER = 16


def cyborg_and_player(t):
    """A cyborg at the west wall facing east and player1 (tough: it must not die) 350 units ahead, facing it."""
    t.point('monster_cyborg', at=(-200, 0, 0), targetname='npc', angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
    t.console('wait_for alive player1 20')
    t.teleport('player1', (150, 0, 0), yaw=180)
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')


def wake(t):
    t.kv('npc', 'spawnflags', '0')
    t.fire('npc', use='on')


def done(t):
    t.fire('kill_npc')
    t.flags('player1', '-FL_NOTARGET')
    t.wait(0.2)


@S.test('a player with notarget in front of it: the cyborg does not take him as its enemy; notarget off: it does',
        players=1, size=ROOM, covers=('monster_cyborg',))
def notarget_not_seen(t):
    cyborg_and_player(t)
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
    t.expect.not_damaged('player1', attacker='npc', after='awake', within=(0, 3.0))
    t.flags('player1', '-FL_NOTARGET')
    t.wait(1.5)
    t.snap('c', 'npc', 'enemy')
    t.expect.value('c', 'npc', 'enemy', 'player')
    done(t)


@S.test('control: the same player without notarget: the cyborg takes him as its enemy', players=1, size=ROOM,
        covers=('monster_cyborg',))
def player_seen(t):
    cyborg_and_player(t)
    t.wait(0.3)
    wake(t)
    t.wait(1.5)
    t.snap('a', 'npc', 'enemy')
    t.expect.value('a', 'npc', 'enemy', 'player')
    done(t)


# ---------------------------------------------------------------------------------------------- walking (fix/cyborg-walk)
# robot.mdl has two ACT_WALK sequences: 4 "walk" (106 units of ground movement a cycle) and 5 "run" (none).
# LookupActivity picks one at random, and on "run" the 1.38 cyborg stood in place on its walk. CCyborg::SetActivity
# walks with "walk". Each test seeds the random numbers differently (the runner seeds 1 at the start).
SEQ_WALK = 4
DIRS = (('east', (150, 0, 0), 0), ('north', (0, 150, 0), 90), ('west', (-150, 0, 0), 180), ('south', (0, -150, 0), 270))


def make_walk_test(seed, dname, mark_at, yaw):
    def walks(t):
        t.console('test_seed %d' % seed)
        # it starts facing away from the mark half the time (the turn is part of the walk)
        t.point('monster_cyborg', at=(0, 0, 0), targetname='npc', angles='0 %d 0' % ((yaw + 180 * (seed % 2)) % 360),
                spawnflags=str(SF_PRISONER))
        t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
        t.point('info_target', targetname='mark', at=mark_at)
        t.point('scripted_sequence', targetname='ss', at=mark_at, angles='0 %d 0' % yaw, m_iszEntity='npc',
                m_fMoveTo='1', m_iszPlay='idle1', names=('m_iszEntity',))
        t.teleport('player1', (-200, -200, 0))
        t.flags('player1', '+FL_NOTARGET')
        t.wait(0.3)
        wake(t)
        t.kv('npc', 'spawnflags', str(SF_PRISONER))
        t.wait(0.5)
        t.fire('ss')
        t.wait(1.5)
        t.snap('mid', 'npc', 'sequence', 'origin')
        t.wait(4.0)
        t.snap('end', 'npc', 'origin')
        t.snap('end', 'mark', 'origin')
        t.expect.value('mid', 'npc', 'sequence', SEQ_WALK)
        t.expect.distance('end', 'npc', 'mark', 0, 40)
        done(t)

    walks.__name__ = 'walks_%s_seed%d' % (dname, seed)
    S.test('walking to a scripted_sequence mark %s (seed %d): the cyborg walks with "walk" and gets there' % (dname, seed),
           players=1, size=ROOM, covers=('monster_cyborg',))(walks)


for _seed in (2, 3, 5, 7):
    for _dname, _at, _yaw in DIRS:
        make_walk_test(_seed, _dname, _at, _yaw)


def make_chase_test(seed):
    def chases(t):
        t.console('test_seed %d' % seed)
        t.point('monster_cyborg', at=(-200, -150, 0), targetname='npc', angles='0 90 0', spawnflags=str(SF_PRISONER))
        t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
        t.point('info_target', targetname='start', at=(-200, -150, 0))
        # a wall between them: no line of fire, so it chases the player round it
        t.brush('func_wall', size=(32, 360, 200), at=(0, -76, 0))
        for at in ((-180, -150), (-180, 0), (-180, 180), (0, 180), (180, 180), (180, 0), (180, -150)):
            t.point('info_node', at=(at[0], at[1], 16))     # the way round the wall
        t.console('wait_for alive player1 20')
        t.teleport('player1', (180, -150, 0), yaw=180)
        t.kv('player1', 'health', '100000')
        t.kv('player1', 'max_health', '100000')
        t.wait(0.3)
        wake(t)
        for k in range(4):
            t.wait(1.5)
            t.snap('c%d' % k, 'npc', 'sequence', 'schedule')
        t.snap('c3', 'npc', 'origin')
        t.snap('c3', 'start', 'origin')
        for k in range(4):
            t.expect.value("c%d" % k, "npc", "sequence", 5, op="<")     # never 5 "run" (the highest sequence number)
        t.expect.distance('c3', 'npc', 'start', 64, 2000)
        done(t)

    chases.__name__ = 'chases_seed%d' % seed
    S.test('chasing the player round a wall (seed %d): the cyborg walks with "walk" and moves' % seed, players=1,
           size=ROOM, covers=('monster_cyborg',))(chases)


for _seed in (2, 3, 5):
    make_chase_test(_seed)
