"""Spectator start (docs/SPECTATOR.md "Spectator start"): with nobody to watch, a joining player and a player who
starts spectating from the menus start at one of the map's info_intermission points, picked at random, looking at
its target or along its angles. With a player to watch, or on a map without a point, the start stays as it was.
Its own suite: the points are global to the map, so each test makes its own (test_create) and removes them at its
end, and nobody is in play until the last test, which adds a living player."""

from etest import Suite

SUITE = Suite('spectator_start', 'Spectator start: info_intermission points', players=0,
              requires=('SpectatorStart_Joining',))
S = SUITE
DOC = 'SPECTATOR.md#spectator-start'
OBS_ROAMING = 3
EYE = 28    # VEC_VIEW: a waiting player's view is origin + view_ofs


def view_after_join(t, label):
    """The joiner's view angles right after its HUD init (InitHUD runs in its first move, the frame after test_player
    add). A test player's next move sets v_angle from its own command again: the engine's svc_setangle (fixangle),
    which turns a real client's view, never reaches a fake client."""
    t.wait_frames(1)
    t.snap(label, '=player1', 'v_angle')


def make_point(t, name, at, **keys):
    """An info_intermission made during the test (test_create), at `at` relative to the room's floor center, with a
    local targetname."""
    p = t._abs(at)
    words = ['test_create', 'info_intermission', '%g' % p[0], '%g' % p[1], '%g' % p[2], 'targetname', t.name(name)]
    for k, v in keys.items():
        words += [k, '"%s"' % v]
    t.console(' '.join(words))


def remove_points(t, *names):
    t.point('trigger_relay', targetname='rm', killtarget=names[0])
    for i, n in enumerate(names[1:]):
        t.point('trigger_relay', targetname='rm%d' % i, killtarget=n)
    t.fire('rm')
    for i in range(len(names) - 1):
        t.fire('rm%d' % i)


@S.test('no info_intermission on the map: a joining player starts at a spawn point, as before', doc=DOC)
def no_point(t):
    t.point('info_target', targetname='mid', at=(0, 0, 100))
    t.console('test_player add late')
    t.wait(1.0)
    t.snap('s', '=player1', 'origin')
    t.snap('s', 'mid', 'origin')
    t.expect.distance('s', '=player1', 'mid', 400, 100000)     # in the lobby, not in this room
    t.console('test_player remove player1')
    t.wait(0.3)


@S.test('nobody to watch, one point with a target: the joiner\'s eyes on it, looking at the target (45 down); '
        '"spectate" from the menus: a roaming observer at the point', doc=DOC)
def point_target(t):
    # the target 128 east and 128 below the point: yaw 0, pitch 45 down
    t.point('info_target', targetname='look', at=(0, 0, 72))
    t.point('info_target', targetname='spot', at=(-128, 0, 200))
    make_point(t, 'pa', (-128, 0, 200), target=t.name('look'))
    t.wait(0.3)
    t.console('test_player add late')
    view_after_join(t, 'first')
    t.wait(1.0)
    t.snap('join', '=player1', 'origin', 'iuser1')
    t.snap('join', 'spot', 'origin')
    t.cmd('=player1', 'spectate')
    t.snap('spec0', '=player1', 'v_angle')          # set by StartSpectating, before the next move (view_after_join)
    t.wait(0.5)
    t.snap('spec', '=player1', 'origin', 'iuser1', 'iuser2')
    t.snap('spec', 'spot', 'origin')
    t.expect.value('first', '=player1', 'v_angle', [45, 0, 0], tol=0.5)
    t.expect.distance('join', '=player1', 'spot', 0, 1)                       # straight under it, 1 s later
    t.expect.distance('join', '=player1', 'spot', EYE - 1, EYE + 1, flat=False)  # the eyes on it
    t.expect.value('join', '=player1', 'iuser1', 0)                          # still waiting in the menus
    t.expect.distance('spec', '=player1', 'spot', 0, 1, flat=False)
    t.expect.value('spec', '=player1', 'iuser1', OBS_ROAMING)                # nobody to watch
    t.expect.value('spec', '=player1', 'iuser2', 0)
    t.expect.value('spec0', '=player1', 'v_angle', [45, 0, 0], tol=0.5)
    t.cmd('=player1', 'spectate')
    t.console('test_player remove player1')
    remove_points(t, 'pa')
    t.wait(0.3)


@S.test('nobody to watch, one point without a target: the view along its angles (20 225 0)', doc=DOC)
def point_angles(t):
    t.point('info_target', targetname='spot', at=(100, 100, 180))
    make_point(t, 'pb', (100, 100, 180), angles='20 225 0')
    t.wait(0.3)
    t.console('test_player add late')
    view_after_join(t, 'first')
    t.wait(1.0)
    t.snap('join', '=player1', 'origin')
    t.snap('join', 'spot', 'origin')
    t.expect.value('first', '=player1', 'v_angle', [20, 225, 0], tol=0.5)
    t.expect.distance('join', '=player1', 'spot', EYE - 1, EYE + 1, flat=False)
    t.expect.distance('join', '=player1', 'spot', 0, 1)
    t.console('test_player remove player1')
    remove_points(t, 'pb')
    t.wait(0.3)


@S.test('two points: each start picks one at random (six joins: both are used)', doc=DOC)
def random_pick(t):
    # both points 150 from "mid", at the same height: a joiner is 150 from mid either way
    t.point('info_target', targetname='mid', at=(0, 0, 150))
    t.point('info_target', targetname='west', at=(-150, 0, 150))
    t.point('info_target', targetname='east', at=(150, 0, 150))
    make_point(t, 'pw', (-150, 0, 150), angles='0 0 0')
    make_point(t, 'pe', (150, 0, 150), angles='0 180 0')
    t.wait(0.3)
    labels = []
    for k in range(6):
        t.console('test_player add late')
        t.wait(0.8)
        lab = 'j%d' % k
        labels.append(lab)
        t.snap(lab, '=player1', 'origin')
        t.snap(lab, 'mid', 'origin')
        t.expect.distance(lab, '=player1', 'mid', 149, 151)
        t.console('test_player remove player1')
        t.wait(0.3)
    # both points used: the eyes on the west one, and on the east one
    w, e = t._abs((-150, 0, 150)), t._abs((150, 0, 150))
    t.expect.value_any(labels, '=player1', 'origin', [[w[0], w[1], w[2] - EYE]])
    t.expect.value_any(labels, '=player1', 'origin', [[e[0], e[1], e[2] - EYE]])
    remove_points(t, 'pw', 'pe')
    t.wait(0.3)


@S.test('a living player to watch: the joiner stays at its spawn point; "spectate" follows the living player',
        doc=DOC)
def someone_to_watch(t):
    t.point('info_target', targetname='spot', at=(0, 0, 200))
    make_point(t, 'pc', (0, 0, 200), angles='0 90 0')
    t.wait(0.3)
    # a player in play (slot 1), alive in the lobby
    t.console('test_player add alive')
    t.wait_frames(2)
    t.console('test_join player1')
    t.wait(0.5)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.duration += 20
    t.wait(0.5)
    t.snap('alive', '=player1', 'health', 'iuser1')
    t.console('test_player add late')
    t.wait(1.0)
    t.snap('join', '=player2', 'origin')
    t.snap('join', 'spot', 'origin')
    t.cmd('=player2', 'spectate')
    t.wait(0.5)
    t.snap('spec', '=player2', 'origin', 'iuser1', 'iuser2')
    t.snap('spec', 'spot', 'origin')
    t.expect.value('alive', '=player1', 'iuser1', 0)
    t.expect.value('alive', '=player1', 'health', 0, op='>')
    t.expect.distance('join', '=player2', 'spot', 300, 100000)               # not at the point
    t.expect.value('spec', '=player2', 'iuser2', 1)                          # watching the living player
    t.expect.value('spec', '=player2', 'iuser1', OBS_ROAMING, op='!=')
    t.expect.distance('spec', '=player2', 'spot', 300, 100000)
    remove_points(t, 'pc')
    t.wait(0.3)
