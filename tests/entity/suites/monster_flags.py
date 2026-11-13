"""The Monster flags every monster class offers: 65536 Non-solid corpse (CBaseMonster::RunTask TASK_DIE) and 1048576
Act out of PVS (CBaseMonster::RunAI, TASK_WAIT_PVS), and TalkMonster's 524288 Ignore move away requests
(CTalkMonster::Touch). Each test room is a sealed box (tests/entity/etest/mapkit.py) and the map is vis-compiled,
so a test without players has no client in its PVS: a monster without Act out of PVS neither looks nor listens
there, one with it does."""

from etest import Suite

SUITE = Suite('monster_flags', 'Monster flags: Non-solid corpse, Act out of PVS, Ignore move away requests')
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#monstermaker-batch-3'
NONSOLID, ACT_OUT_OF_PVS, IGNORE_PUSH = 65536, 1048576, 524288


PRISONER = 16


def release(t, *names):
    """Every monster of a test starts as a prisoner (16: it sees nobody and nobody sees it), so nothing happens before
    the test's turn (the map holds all the tests' rooms from the start); the test clears the flag."""
    for n, flags in names:
        t.kv(n, 'spawnflags', str(flags))


def insect_target(t, at=(0, 100, 0)):
    """A gagged Fed with classify 10 (Insect): every class's relationship row hates it (monsters.cpp iEnemy). Without
    a player in its PVS it does not look either, so it stands still."""
    t.point('monster_human_grunt', targetname='tgt', at=at, angles='0 270 0', spawnflags=str(PRISONER | 2),
            classify='10')


# a talk monster's Use is its follow use: only a player's use takes it out of SCHED_WAIT_TRIGGER (CTalkMonster::
# FollowerUse; Half-Life and 1.38 the same). The player stays in the lobby, out of the room's PVS
PLAYER_USE = ('monster_scientist',)
# a monster in water does not see out of it (CBaseEntity::FVisible, Half-Life and 1.38): the ichthyosaur and its
# target are under water
IN_WATER = ('monster_ichthyosaur',)


def pvs_run(t, cls, flags, **kv):
    if cls in IN_WATER:
        t.brush('func_water', size=(480, 480, 200), at=(0, 0, 0), skin='-3')
    t.point(cls, targetname='mon', at=(0, -150, 60 if cls in IN_WATER else 0), angles='0 90 0',
            spawnflags=str(PRISONER | flags), **kv)
    insect_target(t)
    t.wait(0.3)
    t.kv('tgt', 'health', '100000')
    release(t, ('mon', flags), ('tgt', 2))
    if cls in PLAYER_USE:
        t.fire('mon', activator='player1', caller='player1', use='on')
    else:
        t.fire('mon', use='on')        # named: it waits in SCHED_WAIT_TRIGGER until triggered
    t.wait(3.0)
    t.snap('b', 'mon', 'enemy', 'state', 'schedule')


@S.test('monster_zombie without Act out of PVS, no player in its PVS: it neither looks nor listens, no enemy',
        doc=DOC, covers=('monster_zombie',))
def pvs_zombie_stock(t):
    pvs_run(t, 'monster_zombie', 0)
    t.expect.value('b', 'mon', 'enemy', '')
    t.expect.not_damaged('tgt')


# ------------------------------------------------------------------------------------- generated, one test per class
AI_CLASSES = ('dark_grunt', 'monster_alien_slave', 'monster_barney', 'monster_cyberminotor', 'monster_cyborg',
              'monster_dark', 'monster_furniture', 'monster_gargantua', 'monster_generic', 'monster_grunt_hunter',
              'monster_human_assassin', 'monster_human_grunt', 'monster_hunter', 'monster_ichthyosaur', 'monster_imp',
              'monster_imp_hunter', 'monster_scientist', 'monster_werewolf', 'monster_werewolf_hunter',
              'monster_zombie', 'monster_zombie_grunt', 'monster_zombie_hunter')
EXTRA = {'monster_generic': {'model': 'models/scientist.mdl'}, 'monster_furniture': {'model': 'models/scientist.mdl'}}
# their own RunAI has no PVS test (cyborg.cpp, hunter2.cpp, ...) and they see through walls: they act without a
# player anyway and may take a monster of another room; the flag changes only TASK_WAIT_PVS for them
NO_PVS_TEST = ('monster_cyborg', 'monster_hunter', 'monster_werewolf_hunter', 'monster_imp_hunter',
               'monster_zombie_hunter', 'monster_grunt_hunter', 'monster_cyberminotor')


def pvs_case(cls):
    @S.test('%s with Act out of PVS (1048576), no player in its PVS: it looks and takes the target in front of it as '
            'its enemy' % cls, doc=DOC, covers=(cls, '%s:%d' % (cls, ACT_OUT_OF_PVS)),
            players=1 if cls in PLAYER_USE else 0)
    def case(t):
        pvs_run(t, cls, ACT_OUT_OF_PVS, **EXTRA.get(cls, {}))
        if cls in NO_PVS_TEST:
            t.expect.value('b', 'mon', 'enemy', '', op='!=')
        else:
            t.expect.value('b', 'mon', 'enemy', 'monster_human_grunt')
    case.__name__ = 'pvs_' + cls.replace('monster_', '')
    return case


def corpse_case(cls):
    @S.test('%s with Non-solid corpse (65536): killed, its corpse is not solid' % cls, doc=DOC,
            covers=(cls, '%s:%d' % (cls, NONSOLID)))
    def case(t):
        t.point(cls, targetname='mon', at=(0, 0, 0), angles='0 270 0', spawnflags=str(PRISONER | NONSOLID),
                **EXTRA.get(cls, {}))
        t.wait(0.5)
        for _ in range(3):         # the werewolf hunter may heal from a hit; dead monsters take no more damage
            t.kv('mon', 'health', '5')
            t.damage('mon', 15)
            t.wait(0.3)
        t.wait(5.0)
        t.snap('s', 'mon', 'solid', 'deadflag')
        t.expect.value('s', 'mon', 'deadflag', 2)
        t.expect.value('s', 'mon', 'solid', 0)
    case.__name__ = 'corpse_' + cls.replace('monster_', '')
    return case


for _c in AI_CLASSES:
    pvs_case(_c)
for _c in AI_CLASSES:
    if _c not in ('monster_gargantua', 'monster_furniture'):
        corpse_case(_c)


PUSH_EXTRA = {'monster_barney': {'is_player_ally': '1'}}


def push_case(cls):
    @S.test('%s with Ignore move away requests (524288): a player walking into it does not make it step aside; the same '
            'monster without the flag steps aside' % cls, players=1, doc=DOC,
            covers=(cls, '%s:%d' % (cls, IGNORE_PUSH)))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, fl in (('ign', -120, IGNORE_PUSH), ('stk', 120, 0)):
            # a player-ally Cop: SBO's Cops are hostile (provoked), and CTalkMonster::Touch ignores a provoked one
            t.point(cls, targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags=str(fl), **PUSH_EXTRA.get(cls, {}))
        t.wait(0.5)
        for n in ('ign', 'stk'):
            t.fire(n, use='on')        # named: out of SCHED_WAIT_TRIGGER
        t.wait(1.0)
        t.snap('a', 'ign', 'origin')
        t.snap('a', 'stk', 'origin')
        for x in (-120, 120):
            t.teleport('player1', (x, -70, 0), yaw=90)
            t.wait(0.2)
            t.walk('player1', 400, 0, 0.6)
            t.wait(0.2)
        t.teleport('player1', (0, -200, 0), yaw=90)
        t.wait(3.0)
        t.snap('b', 'ign', 'origin')
        t.snap('b', 'stk', 'origin')
        t.expect.moved('a', 'b', 'ign', delta=(0, 0, 0), tol=8, flat=True)
        t.expect.moved('a', 'b', 'stk', min_dist=24, flat=True)
    case.__name__ = 'push_' + cls.replace('monster_', '')
    return case


for _c in ('monster_barney', 'monster_scientist'):
    push_case(_c)
