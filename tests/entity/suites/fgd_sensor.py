"""Tier 5, batch 2: trigger_sensor FGD items that no 1.38 map uses and no other test asserted
(docs/LOGIC_ENTITIES.md#trigger_sensor): line blocked by glass / monsters, trace to center / eyes / body, dead only,
aimable, sensed master, every Act as Classify choice and the neutral / enemy-or-neutral / ally-or-neutral filters.

The kit puts an info_target named "center" in every room, so the sensed items here are path_corners or monsters."""

from etest import Suite

SUITE = Suite('fgd_sensor', 'Tier 5: trigger_sensor FGD-only choices', players=2)
S = SUITE
DOC = 'LOGIC_ENTITIES.md#trigger_sensor'


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', at=at, targetname=name, **kv)


def sensor(t, name, target, **kv):
    keys = dict(radius='256', line_blocked='0', trace_to='0', fire_trigger_policy='0', alive_filter='0',
                takedamage_filter='0', classify='0', relationship_filter='0')
    keys.update(kv)
    at = keys.pop('at', (0, 0, 32))
    return t.point('trigger_sensor', at=at, targetname=name, target=target, names=('position', 'sensed_master'),
                   **keys)


GLASS = dict(rendermode='2', renderamt='100')


@S.test('Line is blocked by 2 (walls & glass) / 4 (walls, monsters & glass): a glass pane blocks; 1 (walls) does '
        'not; with Entity is alive / damageable No filter (0), Act as Classify None (0) and relationship Any (0) the '
        'sensor finds a path_corner (neither alive nor damageable, no class)', doc=DOC,
        covers=('trigger_sensor.line_blocked=2', 'trigger_sensor.line_blocked=4', 'trigger_sensor.line_blocked=1',
                'trigger_sensor.alive_filter=0', 'trigger_sensor.takedamage_filter=0', 'trigger_sensor.classify=0',
                'trigger_sensor.relationship_filter=0'))
def glass(t):
    t.point('path_corner', targetname='behind', at=(0, 120, 32))
    # a glass pane: a func_breakable drawn transparent (a func_wall would not do: it is a world brush, FL_WORLDBRUSH,
    # which the engine's ignore_glass never skips)
    t.brush('func_breakable', size=(128, 8, 128), at=(0, 60, 0), material='0', health='1000', **GLASS)
    for n, x in (('h1', -200), ('h2', 0), ('h4', 200)):
        info(t, n, (x, -200, 32))
    sensor(t, 's1', 'h1', sensed_classname='path_corner', line_blocked='1')
    sensor(t, 's2', 'h2', sensed_classname='path_corner', line_blocked='2')
    sensor(t, 's4', 'h4', sensed_classname='path_corner', line_blocked='4')
    t.fire('s1')
    t.fire('s2')
    t.fire('s4')
    t.wait(0.1)
    t.expect.fired('h1', activator='behind', count=1)
    t.expect.fired('h2', count=0)
    t.expect.fired('h4', count=0)


@S.test('Line is blocked by 3 (walls & monsters) / 4: a scientist in the line blocks; 1 / 2 do not', doc=DOC,
        covers=('trigger_sensor.line_blocked=3', 'trigger_sensor.line_blocked=4', 'trigger_sensor.line_blocked=2'))
def monster_block(t):
    t.point('path_corner', targetname='behind', at=(0, 160, 32))
    t.point('monster_scientist', targetname='sci', at=(0, 80, 0), angles='0 270 0')
    for n, x in (('h1', -200), ('h2', -70), ('h3', 70), ('h4', 200)):
        info(t, n, (x, -200, 32))
    for n in '1234':
        sensor(t, 's' + n, 'h' + n, sensed_classname='path_corner', line_blocked=n)
    t.wait(0.3)
    for n in '1234':
        t.fire('s' + n)
    t.wait(0.1)
    t.expect.fired('h1', activator='behind', count=1)
    t.expect.fired('h2', activator='behind', count=1)
    t.expect.fired('h3', count=0)
    t.expect.fired('h4', count=0)


@S.test('Trace line to 1 (center): a brush entity whose origin is the world origin is seen at its center; '
        'traced to its origin (0) a wall is in the way', doc=DOC,
        covers=('trigger_sensor.trace_to=1', 'trigger_sensor.trace_to=0'))
def trace_center(t):
    t.brush('func_illusionary', targetname='ill', size=(32, 32, 32), at=(0, 100, 16))
    info(t, 'h0', (-200, -200, 32))
    info(t, 'h1', (200, -200, 32))
    sensor(t, 's0', 'h0', sensed_classname='func_illusionary', line_blocked='1', trace_to='0')
    sensor(t, 's1', 'h1', sensed_classname='func_illusionary', line_blocked='1', trace_to='1')
    t.fire('s0')
    t.fire('s1')
    t.wait(0.1)
    t.expect.fired('h0', count=0)
    t.expect.fired('h1', activator='ill', count=1)


def low_wall_case(trace_to, label):
    @S.test('Trace line to %d (%s): a scientist behind a 40-unit wall is seen over it; to its origin (feet) not' % (
            trace_to, label), doc=DOC, covers=('trigger_sensor.trace_to=%d' % trace_to,))
    def case(t):
        t.point('monster_scientist', targetname='sci', at=(0, 120, 0), angles='0 270 0')
        t.brush('func_wall', size=(128, 16, 40), at=(0, 60, 0))
        info(t, 'h0', (-200, -200, 32))
        info(t, 'hx', (200, -200, 32))
        kw = dict(at=(0, 0, 64), sensed_classname='monster_scientist', line_blocked='1')
        sensor(t, 's0', 'h0', trace_to='0', **kw)
        sensor(t, 'sx', 'hx', trace_to=str(trace_to), **kw)
        t.wait(0.3)
        t.fire('s0')
        t.fire('sx')
        t.wait(0.1)
        t.expect.fired('h0', count=0)
        t.expect.fired('hx', activator='sci', count=1)
    case.__name__ = 'trace_%d' % trace_to
    return case


low_wall_case(2, 'eyes')
low_wall_case(3, 'body')


@S.test('Entity is alive -1 (dead only): a monster_scientist_dead is sensed, a living scientist and a player not',
        players=1, doc=DOC,
        covers=('trigger_sensor.alive_filter=-1',))
def dead_only(t):
    t.point('monster_scientist_dead', targetname='corpse', at=(-80, 80, 0))
    t.point('monster_scientist', targetname='sci', at=(80, 80, 0))
    info(t, 'hit', (-200, -200, 32))
    sensor(t, 's', 'hit', alive_filter='-1', takedamage_filter='0')
    t.wait(0.2)
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', activator='corpse', count=1)
    t.expect.fired('hit', activator='sci', count=0)
    t.expect.fired('hit', activator='player1', count=0)


@S.test('Entity is damageable 2 (aimable): a scientist is sensed, a func_breakable (damageable, not aimable) not',
        doc=DOC, covers=('trigger_sensor.takedamage_filter=2',))
def aimable(t):
    t.point('monster_scientist', targetname='sci', at=(80, 80, 0))
    t.brush('func_breakable', targetname='crate', health='50', at=(-80, 80, 16))
    info(t, 'hit', (-200, -200, 32))
    sensor(t, 's', 'hit', takedamage_filter='2', sensed_classname='', at=(0, 60, 32))
    t.wait(0.2)
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', activator='sci', count=1)
    t.expect.fired('hit', activator='crate', count=0)


@S.test('Trigger Master: only entities the master lets through (env_state off: none; on: all)', doc=DOC,
        covers=('trigger_sensor.sensed_master',))
def master(t):
    t.point('path_corner', targetname='p', at=(60, 60, 32))
    t.point('env_state', targetname='gate')
    info(t, 'hit', (-200, -200, 32))
    sensor(t, 's', 'hit', sensed_classname='path_corner', sensed_master='gate')
    t.fire('s')
    t.wait(0.1)
    t.mark('open')
    t.fire('gate', use='on')
    t.wait(0.1)
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', activator='p', count=1, after='open')
    t.expect.fired('hit', count=1)


# R_AL -2, R_FR -1, R_NO 0, R_DL 1, R_HT 2, R_NM 3: CBaseMonster::IDefaultRelationship, rows and columns of class 2
# (player): REL_TO_PLAYER[c] = row c, column 2; REL_FROM_PLAYER[c] = row 2, column c
REL_TO_PLAYER = {1: 1, 2: 0, 5: 2, 6: 0, 7: 3, 8: 1, 9: 3, 10: 2, 12: 1, 13: 1}
REL_FROM_PLAYER = {1: 1, 2: 0, 5: 1, 6: 1, 7: 1, 8: 1, 9: 1, 10: 2, 12: 1, 13: 1}


@S.test('Act as Classify 1, 2, 5-10, 12, 13 with relationship Enemy: players are sensed as the class table says '
        '(enemies of every class but Player)', players=2, doc=DOC,
        covers=tuple('trigger_sensor.classify=%d' % c for c in REL_TO_PLAYER))
def classes(t):
    kv = dict(at=(0, -120, 40), radius='300', sensed_classname='player', line_blocked='0')
    for i, c in enumerate(sorted(REL_TO_PLAYER)):
        info(t, 'h%d' % c, (-220 + i * 48, 200, 32))
        sensor(t, 's%d' % c, 'h%d' % c, classify=str(c), relationship_filter='2', **kv)
        t.fire('s%d' % c)
    t.wait(0.1)
    for c in REL_TO_PLAYER:
        enemy = REL_TO_PLAYER[c] >= 1 or REL_FROM_PLAYER[c] >= 1
        t.expect.fired('h%d' % c, activator='player1', count=1 if enemy else 0)


@S.test('relationship 1 (neutral), 3 (enemy or neutral), 5 (ally or neutral): players as Player (neutral both ways) '
        'and as human military (enemies)', players=2, doc=DOC,
        covers=('trigger_sensor.relationship_filter=1', 'trigger_sensor.relationship_filter=3',
                'trigger_sensor.relationship_filter=5', 'trigger_sensor.classify=2', 'trigger_sensor.classify=4'))
def neutral(t):
    kv = dict(at=(0, -120, 40), radius='300', sensed_classname='player', line_blocked='0')
    cases = {('2', '1'): True, ('2', '3'): True, ('2', '5'): True, ('4', '1'): False, ('4', '3'): True,
             ('4', '5'): False}
    for i, (c, r) in enumerate(sorted(cases)):
        info(t, 'h%s%s' % (c, r), (-200 + i * 70, 200, 32))
        sensor(t, 's%s%s' % (c, r), 'h%s%s' % (c, r), classify=c, relationship_filter=r, **kv)
        t.fire('s%s%s' % (c, r))
    t.wait(0.1)
    for (c, r), ok in cases.items():
        t.expect.fired('h%s%s' % (c, r), activator='player1', count=1 if ok else 0)


@S.test('relationship 3 (enemy or neutral) leaves out allies: as player ally (ally of players) and as human passive '
        '(fears players, players neutral to it) no player is sensed', players=2, doc=DOC,
        covers=('trigger_sensor.relationship_filter=3',))
def enemy_or_neutral_allies(t):
    kv = dict(at=(0, -120, 40), radius='300', sensed_classname='player', line_blocked='0')
    info(t, 'h11', (-100, 200, 32))
    info(t, 'h3', (100, 200, 32))
    sensor(t, 's11', 'h11', classify='11', relationship_filter='3', **kv)
    sensor(t, 's3', 'h3', classify='3', relationship_filter='3', **kv)
    t.fire('s11')
    t.fire('s3')
    t.wait(0.1)
    t.expect.fired('h11', count=0)
    t.expect.fired('h3', count=0)
