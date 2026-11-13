"""Mover prediction (docs/MOVER_PREDICTION.md): what AddToFullPack sends of a moving pusher's move.

The fields are read with `test_snap <label> <ref> mover` (the test layer fills an entity_state_t with the same
MoverPrediction_FillState the server sends; fake clients get no AddToFullPack). Keys: mover_basevelocity (velocity),
mover_startpos (angular velocity), mover_impacttime (linear stop: stop point . direction), mover_starttime (angular
stop: the angle still to turn on the fastest axis), mover_iuser1 (1 linear stop, 2 angular stop, 3 linear stop that
ends the turn too), mover_endpos (velocity after the
linear stop: a func_train corner it goes straight on through).
"""

from etest import Suite

SUITE = Suite('mover', 'Mover prediction: the move sent for pushers')
S = SUITE

ZERO = [0, 0, 0]


def expect_nothing_sent(t, label, ent):
    for f in ('mover_basevelocity', 'mover_startpos', 'mover_endpos'):
        t.expect.value(label, ent, f, ZERO)
    for f in ('mover_impacttime', 'mover_starttime', 'mover_iuser1'):
        t.expect.value(label, ent, f, 0)


@S.test('func_door moving: velocity and its linear stop; nothing once open', doc='MOVER_PREDICTION.md#entity_state_t-fields-for-pushers-deltalst')
def door(t):
    # up 96 - 8 = 88 units at 200 u/s (0.44 s); wait -1: stays open. No ORIGIN brush: the stop is (0, 0, 88)
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='200', wait='-1', lip='8')
    t.snap('closed', 'door', 'mover')
    t.fire('door')
    t.wait(0.2)
    t.snap('moving', 'door', 'mover')
    t.wait(0.6)
    t.snap('open', 'door', 'mover')
    expect_nothing_sent(t, 'closed', 'door')
    t.expect.value('moving', 'door', 'mover_basevelocity', [0, 0, 200], tol=0.5)
    t.expect.value('moving', 'door', 'mover_iuser1', 1)
    t.expect.value('moving', 'door', 'mover_impacttime', 88, tol=0.1)
    t.expect.value('moving', 'door', 'mover_starttime', 0)      # never both: no engine parametric movement
    t.expect.value('moving', 'door', 'mover_startpos', ZERO)
    t.expect.value('moving', 'door', 'mover_endpos', ZERO)
    expect_nothing_sent(t, 'open', 'door')


@S.test('func_door_rotating moving: angular velocity and its angular stop; nothing once open')
def door_rotating(t):
    # 90 degrees about yaw at 90 deg/s (1 s); wait -1: stays open
    t.brush('func_door_rotating', size=(96, 8, 96), at=(0, 64, 0), tex='666MURMETAL', origin=True,
            targetname='rdoor', speed='90', distance='90', wait='-1')
    t.fire('rdoor')
    t.wait(0.4)
    t.snap('moving', 'rdoor', 'mover')
    t.wait(1.0)
    t.snap('open', 'rdoor', 'mover')
    t.expect.value('moving', 'rdoor', 'mover_startpos', [0, 90, 0], tol=0.5)
    t.expect.value('moving', 'rdoor', 'mover_iuser1', 2)
    t.expect.value('moving', 'rdoor', 'mover_starttime', 70, op='<')     # the angle still to turn: 90 - 36
    t.expect.value('moving', 'rdoor', 'mover_starttime', 40, op='>')
    t.expect.value('moving', 'rdoor', 'mover_impacttime', 0)    # never both
    t.expect.value('moving', 'rdoor', 'mover_basevelocity', ZERO)
    expect_nothing_sent(t, 'open', 'rdoor')


@S.test('func_train: the next segment is sent through a corner without a wait, not at a wait corner')
def train(t):
    # c0 -> c1 (no wait: straight on) -> c2 (wait 1: stops) -> c3 -> c0; 100 u/s, 256 units a side (2.56 s)
    t.brush('func_train', size=(32, 32, 8), at=(-128, -128, 60), tex='666MURMETAL', targetname='train',
            target='c0', speed='100', dmg='0')
    t.point('path_corner', at=(-128, -128, 64), targetname='c0', target='c1', wait='0')
    t.point('path_corner', at=(128, -128, 64), targetname='c1', target='c2', wait='0')
    t.point('path_corner', at=(128, 128, 64), targetname='c2', target='c3', wait='1')
    t.point('path_corner', at=(-128, 128, 64), targetname='c3', target='c0', wait='0')
    t.fire('train')
    t.wait(1.0)
    t.snap('to_c1', 'train', 'mover')
    t.wait(2.6)
    t.snap('to_c2', 'train', 'mover')
    t.expect.value('to_c1', 'train', 'mover_basevelocity', [100, 0, 0], tol=0.5)
    t.expect.value('to_c1', 'train', 'mover_iuser1', 1)
    t.expect.value('to_c1', 'train', 'mover_endpos', [0, 100, 0], tol=0.5)     # goes straight on to c2
    t.expect.value('to_c2', 'train', 'mover_basevelocity', [0, 100, 0], tol=0.5)
    t.expect.value('to_c2', 'train', 'mover_iuser1', 1)
    t.expect.value('to_c2', 'train', 'mover_endpos', ZERO)                     # c2 waits: a stop


@S.test('func_rotating: angular velocity without a stop; a func_wall sends nothing')
def rotating(t):
    t.brush('func_rotating', size=(96, 96, 8), at=(-96, 0, 0), tex='666MURMETAL', origin=True,
            targetname='disc', speed='60')
    t.brush('func_wall', size=(32, 32, 32), at=(128, 0, 0), tex='666MURMETAL', targetname='wall')
    t.fire('disc')
    t.wait(0.5)
    t.snap('spinning', 'disc', 'mover')
    t.snap('wall', 'wall', 'mover')
    t.expect.value('spinning', 'disc', 'mover_startpos', [0, 60, 0], tol=0.5)
    t.expect.value('spinning', 'disc', 'mover_iuser1', 0)
    t.expect.value('spinning', 'disc', 'mover_impacttime', 0)
    t.expect.value('spinning', 'disc', 'mover_starttime', 0)
    expect_nothing_sent(t, 'wall', 'wall')


@S.test('a movewith child sends no move of its own: it is predicted through its parent', players=1,
        doc='MOVER_PREDICTION.md#merging-with-movewith')
def movewith_child(t):
    # a lift (func_door, up 88 at 100 u/s) with a solid child (func_wall) and a child door (func_door, Move with the
    # lift) that opens sideways while the lift moves; the player watches from beside it
    t.brush('func_door', size=(64, 64, 16), at=(0, 96, 0), tex='666MURMETAL', targetname='lift', angles='-90 0 0',
            speed='100', wait='-1', lip='-72')
    t.brush('func_wall', size=(64, 8, 32), at=(0, 132, 16), tex='666MURMETAL', targetname='rail', movewith='lift')
    t.brush('func_door', size=(8, 48, 32), at=(-28, 96, 16), tex='666MURMETAL', targetname='gate', movewith='lift',
            angles='0 90 0', speed='40', wait='-1', lip='8')
    t.teleport('player1', (-160, 96, 0))
    t.fire('lift')
    t.fire('gate')
    t.wait(0.3)
    for n in ('lift', 'rail', 'gate'):
        t.snap('moving', n, 'mover', 'fullpack_iuser4:player1', 'fullpack_basevelocity:player1')
    x = t.expect
    x.value('moving', 'lift', 'mover_basevelocity', [0, 0, 100], tol=0.5)              # the parent: its move
    x.value('moving', 'lift', 'fullpack_basevelocity:player1', [0, 0, 100], tol=0.5)
    for n in ('rail', 'gate'):
        expect_nothing_sent(t, 'moving', n)                                            # children: none of their own
        x.value('moving', n, 'fullpack_iuser4:player1', 0, op='>')                     # placed on the lift
        x.value('moving', n, 'fullpack_basevelocity:player1', ZERO)


@S.test('a pusher blocked by someone sends no move while it stands, and its move again once free', players=2)
def blocked(t):
    # a func_train driving +x pins player2 against a wall: the engine keeps its velocity while it stands, so the move
    # sent would carry it through him on every client
    t.brush('func_train', size=(32, 32, 32), at=(-150, 200, 0), tex='666MURMETAL', targetname='bt', target='b0',
            speed='100', dmg='0', spawnflags='64')
    t.point('path_corner', at=(-150, 200, 16), targetname='b0', target='b1', wait='0')
    t.point('path_corner', at=(150, 200, 16), targetname='b1', target='b0', wait='0')
    t.brush('func_wall', size=(32, 64, 96), at=(102, 200, 0), tex='666MURMETAL', targetname='bwall')
    t.teleport('player2', (70, 200, 0))
    t.fire('bt')
    t.wait(3.0)
    t.snap('held1', 'bt', 'origin', 'mover')
    t.wait(0.5)
    t.snap('held2', 'bt', 'origin', 'mover')
    t.teleport('player2', (-150, -150, 0))
    t.wait(0.5)
    t.snap('free', 'bt', 'mover')
    t.expect.moved('held1', 'held2', 'bt', (0, 0, 0), tol=1)                     # it stands
    for lab in ('held1', 'held2'):
        expect_nothing_sent(t, lab, 'bt')                                         # and says so
    t.expect.value('free', 'bt', 'mover_iuser1', 1)                               # moving to its corner again


@S.test('func_platrot: its turn ends at its linear stop (iuser1 3)')
def platrot(t):
    t.brush('func_platrot', size=(64, 64, 16), at=(0, 160, 0), tex='666MURMETAL', origin=True, targetname='pr',
            speed='50', height='64', rotation='90', spawnflags='1')
    t.fire('pr')
    t.wait(0.4)
    t.snap('moving', 'pr', 'mover')
    t.expect.value('moving', 'pr', 'mover_iuser1', 3)


@S.test('func_door_rotating turning more than 360 degrees: the angle still to turn is sent')
def door_rotating_450(t):
    # 450 degrees at 90 deg/s (5 s); at 0.4 s about 414 are left: the stop angle itself (450) would be read modulo 360
    t.brush('func_door_rotating', size=(96, 8, 96), at=(0, -160, 0), tex='666MURMETAL', origin=True,
            targetname='rdoor450', speed='90', distance='450', wait='-1')
    t.fire('rdoor450')
    t.wait(0.4)
    t.snap('moving', 'rdoor450', 'mover')
    t.expect.value('moving', 'rdoor450', 'mover_iuser1', 2)
    t.expect.value('moving', 'rdoor450', 'mover_starttime', 440, op='<')
    t.expect.value('moving', 'rdoor450', 'mover_starttime', 380, op='>')
