"""Tier 4: classes, keys and choice values the 1.38 maps use most that need a real server (player movement, monster
routes, damage). Each test checks the FGD's description (sdk/syndicate.fgd); the logic-only items are map-test cases
(tests/maps/cases 110-119, 1.38 and develop side by side).

- func_ladder: a player looking up and walking into it climbs;
- trigger_hurt Damage Type: every value the maps use reaches the player's damage as that type;
- info_node: a scientist told to run to a mark behind a wall gets there round the wall along the nodes; without
  nodes near him he does not;
- path_corner "New Train rot. Speed" (yaw_speed): a func_train passing it turns at that speed."""

from etest import Suite

SUITE = Suite('map_used', 'Tier 4, map-used items on a real server: func_ladder, trigger_hurt damage types, info_node, '
              'path_corner yaw_speed')
S = SUITE

SF_PRISONER = 16


def hold(t, player, forward, *buttons):
    """Holds the input until release() (test_move with -1 frames, as suites tank and pmove)."""
    t.console('test_move %s %s 0 -1 %s' % (t.ref(player), forward, ' '.join(buttons)))


def release(t, player):
    t.console('test_move %s 0 0 0' % t.ref(player))


# ------------------------------------------------------------------------------------------------ func_ladder

@S.test('func_ladder: a player facing it, looking up and walking forward climbs it (origin rises more than 100 units '
        'in 1.5 s); the same walk against a plain wall does not lift him', players=1,
        covers=('func_ladder',))
def ladder_climb(t):
    t.brush('func_wall', size=(128, 16, 240), at=(0, 100, 0))
    t.brush('func_ladder', size=(64, 24, 240), at=(0, 80, 0))
    t.brush('func_wall', size=(64, 16, 240), at=(-200, 100, 0))
    t.point('info_target', targetname='foot', at=(0, 40, 0))
    t.point('info_target', targetname='foot2', at=(-200, 40, 0))
    t.teleport('player1', (0, 40, 0), yaw=90)
    t.look('player1', -45, 90)
    t.wait(0.5)
    t.snap('before', 'player1', 'origin')
    t.snap('before', 'foot', 'origin')
    hold(t, 'player1', 200, '+forward')     # the ladder code reads the button (pm_shared PM_LadderMove)
    t.wait(1.5)
    t.snap('up', 'player1', 'origin')
    t.snap('up', 'foot', 'origin')
    release(t, 'player1')
    # the control: a plain wall
    t.teleport('player1', (-200, 40, 0), yaw=90)
    t.look('player1', -45, 90)
    t.wait(0.5)
    hold(t, 'player1', 200, '+forward')     # the ladder code reads the button (pm_shared PM_LadderMove)
    t.wait(1.5)
    t.snap('wall', 'player1', 'origin')
    t.snap('wall', 'foot2', 'origin')
    release(t, 'player1')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.5)
    # the player's origin is 36 units above the floor where he stands
    t.expect.distance('before', 'player1', 'foot', 30, 60, flat=False)
    t.expect.distance('up', 'player1', 'foot', 136, 400, flat=False)
    t.expect.distance('wall', 'player1', 'foot2', 30, 60, flat=False)


# ------------------------------------------------------------------------------------------------ trigger_hurt types

HURT_TYPES = ((0, 'GENERIC'), (1, 'CRUSH'), (4, 'SLASH'), (16, 'FREEZE'), (32, 'FALL'), (64, 'BLAST'),
              (256, 'SHOCK'), (1024, 'ENERGYBEAM'), (131072, 'POISON'), (262144, 'RADIATION'),
              (524288, 'DROWNRECOVER'), (1048576, 'CHEMICAL'))


def make_hurt_test(value, name):
    def hurt_type(t):
        t.trigger('trigger_hurt', size=(96, 96, 72), at=(0, 120, 0), dmg='20', damagetype=str(value))
        t.flags('player1', '+FL_GODMODE')       # the damage is still recorded (TakeDamage is called)
        t.wait(0.2)
        t.mark('in')
        t.teleport('player1', (0, 120, 0))
        t.wait(0.7)
        t.teleport('player1', (0, -150, 0))
        t.wait(0.3)
        t.flags('player1', '-FL_GODMODE')
        if value:
            t.expect.damaged('player1', after='in', within=(0, 0.7), type=value)
        else:
            t.expect.damaged('player1', after='in', within=(0, 0.7))

    hurt_type.__name__ = 'hurt_type_%d' % value
    S.test('trigger_hurt Damage Type %d %s: the player inside takes its damage with that type' % (value, name),
           players=1, covers=('trigger_hurt.damagetype', 'trigger_hurt.damagetype=%d' % value))(hurt_type)


for _v, _n in HURT_TYPES:
    make_hurt_test(_v, _n)


# ------------------------------------------------------------------------------------------------ info_node

def walk_round_wall(t, nodes):
    """A scientist west of a wall, told (scripted_sequence, run) to a mark east of it: the straight way is blocked."""
    t.brush('func_wall', size=(32, 360, 200), at=(0, -60, 0))
    if nodes:
        for at in ((-150, -150), (-150, 0), (-150, 180), (0, 180), (150, 180), (150, 0), (150, -150)):
            t.point(nodes, at=(at[0], at[1], 16))
    t.point('monster_scientist', targetname='sci', at=(-150, -150, 0), angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('info_target', targetname='mark', at=(150, -150, 0))
    t.point('scripted_sequence', targetname='ss', at=(150, -150, 0), angles='0 0 0', m_iszEntity='sci',
            m_fMoveTo='2', m_iszPlay='idle1', names=('m_iszEntity',))
    t.teleport('player1', (-200, 200, 0))
    t.flags('player1', '+FL_NOTARGET')
    t.wait(1.0)
    t.fire('ss')
    t.wait(12.0)
    t.snap('end', 'sci', 'origin')
    t.snap('end', 'mark', 'origin')
    t.flags('player1', '-FL_NOTARGET')


@S.test('info_node: a scientist told to run to a mark behind a wall walks round it along the nodes and gets there',
        players=1, covers=('info_node',))
def node_route(t):
    walk_round_wall(t, 'info_node')
    t.expect.distance('end', 'sci', 'mark', 0, 48)


@S.test('info_node: control: the same room without nodes: the scientist cannot find the way and stays west of the wall',
        players=1, covers=('info_node',))
def node_route_none(t):
    walk_round_wall(t, None)
    t.expect.distance('end', 'sci', 'mark', 150, 1000)


# ------------------------------------------------------------------------------------------------ path_corner

@S.test('path_corner New Train rot. Speed (yaw_speed 30): a func_train passing it turns at 30 degrees a second',
        covers=('path_corner.yaw_speed',))
def path_corner_yaw_speed(t):
    t.brush('func_train', size=(32, 32, 32), at=(-150, 0, 64), origin=True, targetname='tr', target='p1', speed='100')
    t.point('path_corner', targetname='p1', at=(-150, 0, 48), target='p2', yaw_speed='30')
    t.point('path_corner', targetname='p2', at=(150, 0, 48), target='p1', yaw_speed='30')
    t.wait(0.5)
    t.fire('tr')
    t.wait(1.0)
    t.snap('moving', 'tr', 'avelocity')
    t.expect.value('moving', 'tr', 'avelocity.y', 30, tol=1)
