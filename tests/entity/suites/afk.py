"""AFK players (docs/SPECTATOR.md "AFK players"): a key held down is no input (ReGameDLL_CS), except at the controls:
driving a func_vehicle or using a func_tank, a held key counts, as the user_afk_status plugin counted every held key
(playtest 2026-10-17: vehicle drivers holding forward were moved to spectator mode; held keys count
only at the controls). Its own suite: it sets the AFK cvars, and the last test leaves player1 in spectator mode, so
the tests run in this order with the one player."""

from etest import Suite

SUITE = Suite('afk', 'AFK players: held keys count only at the controls', players=1)
S = SUITE
DOC = 'SPECTATOR.md#afk-players-mp_afk_time-mp_afk_spectate_time-branch-featafk-native'
FL_ONTRAIN = 1 << 24


def afk_cvars(t):
    t.console('mp_afk_time 2')
    t.console('mp_afk_spectate_time 4')


@S.test('driving a func_vehicle, forward held with a steady view for 7 s: no AFK sign, not moved', players=1,
        doc=DOC, size=(512, 512, 200))
def held_vehicle(t):
    afk_cvars(t)
    # a 128 x 128 slab on a path of two path_tracks (south -> north); the controls cover it and the driver above it
    t.point('path_track', targetname='p1', target='p2', at=(0, -150, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 150, 0))
    t.brush('func_vehicle', size=(128, 128, 16), at=(0, -150, 0), origin=True, targetname='car', target='p1',
            height='8', length='128', width='128', speed='200', acceleration='5', sounds='0')
    t.brush('func_vehiclecontrols', size=(128, 128, 128), at=(0, -150, 0), target='car')
    t.wait(0.5)
    t.teleport('player1', (0, -150, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    t.move('player1', 0, 0, 3, '+use')         # take the controls (PlayerUse: standing on it, inside the controls)
    t.wait(1.5)                                 # the teleport's view turn is ignored for 1 s
    t.snap('driving', 'player1', 'flags')
    t.mark('held')
    t.walk('player1', 250, 0, 7.0, '+forward')
    t.snap('end', 'player1', 'iuser1', 'flags')
    t.move('player1', 0, 0, 3, '+use')         # leave the controls (a teleport off the vehicle would not)
    t.wait(0.3)
    t.expect.value('driving', 'player1', 'flags', FL_ONTRAIN, op='&')
    t.expect.no_msg('PlayerAfk', after='held')
    t.expect.value('end', 'player1', 'iuser1', 0)


@S.test('using a func_tank, attack held with a steady view for 7 s: no AFK sign, not moved', players=1, doc=DOC)
def held_tank(t):
    afk_cvars(t)
    t.brush('func_tank', size=(32, 32, 32), at=(0, 100, 40), origin=True, targetname='gun', spawnflags='33', yawrate='30',
            yawrange='180', pitchrate='30', pitchrange='30', barrel='16', firerate='2', bullet='1')
    t.brush('func_tankcontrols', size=(64, 64, 96), at=(0, 0, 0), target='gun', targetname='tc')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)                                 # landed: the tank lets go of a player who moves 30 units
    t.fire('tc', activator='player1')           # the controls' Use with the player: CFuncTank::StartControl
    t.wait(1.5)
    # held from here; the mark 0.5 s later (pressing it is input and takes off a sign the wait gave)
    t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
    t.wait(0.5)
    t.mark('held')
    t.wait(7.0)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.snap('end', 'player1', 'iuser1')
    t.expect.msg('svc:23', min=1, after='held')     # temp entities of its shots: the player has the gun
    t.expect.no_msg('PlayerAfk', after='held')
    t.expect.value('end', 'player1', 'iuser1', 0)
    t.fire('tc', activator='player1', use='off')


@S.test('on foot, forward held with a steady view for 7 s: the AFK sign, then spectator mode (no input)',
        players=1, doc=DOC)
def held_on_foot(t):
    afk_cvars(t)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.5)
    t.mark('held')
    t.walk('player1', 250, 0, 7.0, '+forward')     # into the wall: the same usercmd every frame
    t.snap('end', 'player1', 'iuser1')
    t.expect.msg('PlayerAfk', after='held', min=1)
    t.expect.value('end', 'player1', 'iuser1', 0, op='>')
