"""trigger_camera "All Players" (8): every player in play gets the camera's view, whoever fired it, and every one of
them gets his own view back when it ends (docs/FEATUREFUL_OPTIONS.md#trigger_camera). The test that disconnects
player3 and the one that kills the others stay last, in that order."""

from etest import Suite

SUITE = Suite('camera_all', 'trigger_camera "All Players" (8): the view for every player in play', players=3)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#trigger_camera'
FL_FROZEN = 4096
ALL = 8
FREEZE = 4
INVULNERABLE = 256
ALIVE_ONLY = 1024
PLAYERS = ('player1', 'player2', 'player3')


def camera(t, name='cam', flags=ALL, wait='10', at=(-150, -150, 100), look=(200, 200, 40), **kv):
    t.point('info_target', targetname='look', at=look)
    t.point('trigger_camera', targetname=name, at=at, target='look', wait=wait, spawnflags=str(flags), **kv)


def healthy(t, players=PLAYERS, hp='100'):
    for p in players:
        t.kv(p, 'health', hp)
        t.kv(p, 'max_health', hp)


@S.test('"All Players" with Freeze Player and Player Invulnerable, fired by player2: all three get the view, are '
        'frozen and take no damage; at the hold time all three get their own view back, unfrozen, and take damage again',
        players=3, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:4', 'trigger_camera:8', 'trigger_camera:256', 'trigger_camera.wait',
                'trigger_camera.target'))
def all_freeze_invulnerable(t):
    camera(t, flags=ALL | FREEZE | INVULNERABLE, wait='0.8')
    healthy(t)
    t.mark('on')
    t.fire('cam', activator='player2')
    t.wait(0.3)
    for p in PLAYERS:
        t.damage(p, 30)
    t.wait(0.1)
    for p in PLAYERS:
        t.snap('during', p, 'flags', 'health')
    t.wait(0.8)
    t.mark('ended')
    for p in PLAYERS:
        t.damage(p, 30)
    t.wait(0.1)
    for p in PLAYERS:
        t.snap('after', p, 'flags', 'health')
    for p in PLAYERS:
        t.expect.setview(p, 'cam', count=1)
        t.expect.setview(p, p, after='on', count=1)
        t.expect.value('during', p, 'flags', FL_FROZEN, op='&')
        t.expect.value('during', p, 'health', 100, tol=0.5)
        t.expect.value('after', p, 'flags', FL_FROZEN, op='!&')
        t.expect.value('after', p, 'health', 70, tol=1.5)
    t.expect.used('look', caller='cam', count=1)


@S.test('"All Players" fired with no activator (a trigger_auto, a timer): all three get the view, and it is turned off '
        '(Off): all three get their own view back at once', players=3, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:4', 'trigger_camera:8'))
def all_no_activator_off(t):
    camera(t, flags=ALL | FREEZE, wait='10')
    t.fire('cam')
    t.wait(0.4)
    for p in PLAYERS:
        t.snap('during', p, 'flags')
    t.mark('off')
    t.fire('cam', use='off')
    t.wait(0.3)
    for p in PLAYERS:
        t.snap('after', p, 'flags')
    for p in PLAYERS:
        t.expect.setview(p, 'cam', count=1)
        t.expect.setview(p, p, after='off', within=(0, 0.2), count=1)
        t.expect.value('during', p, 'flags', FL_FROZEN, op='&')
        t.expect.value('after', p, 'flags', FL_FROZEN, op='!&')


@S.test('"All Players" removed (killtarget) while it has the views: all three get their own view back, unfrozen',
        players=3, doc=DOC, covers=('trigger_camera', 'trigger_camera:4', 'trigger_camera:8'))
def all_removed(t):
    camera(t, flags=ALL | FREEZE, wait='10')
    t.point('trigger_relay', targetname='kill', killtarget='cam')
    t.fire('cam', activator='player1')
    t.wait(0.4)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.3)
    for p in PLAYERS:
        t.snap('after', p, 'flags')
    for p in PLAYERS:
        t.expect.setview(p, 'cam', count=1)
        t.expect.setview(p, p, after='kill', count=1)
        t.expect.value('after', p, 'flags', FL_FROZEN, op='!&')


@S.test('"All Players" with "Stop by player input": player1\'s use key gives only him his view back (his controls '
        'too); the others keep it until the hold time, then the camera ends and fires its target once', players=3, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:8', 'trigger_camera.stop_by_player_input=1', 'trigger_camera.target'))
def all_stop_by_use(t):
    camera(t, flags=ALL | FREEZE, wait='1.5', stop_by_player_input='1')
    t.mark('on')
    t.fire('cam', activator='player2')
    t.wait(0.4)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.3)
    t.mark('used')
    t.wait(1.2)
    t.mark('ended')
    t.wait(0.2)
    for p in PLAYERS:
        t.expect.setview(p, 'cam', count=1)
    t.expect.setview('player1', 'player1', after='use', within=(0, 0.3), count=1)
    t.expect.setview('player2', 'player2', after='used', count=1)
    t.expect.setview('player3', 'player3', after='used', count=1)
    t.expect.setview('player2', 'player2', after='on', within=(0, 1.0), count=0)
    t.expect.msg('Capability', to='player1', fields={'bits': 0}, after='use', within=(0, 0.3), count=1)
    t.expect.used('look', caller='cam', after='used', count=1)


@S.test('Without "All Players": fired by player2 only player2 gets the view', players=3, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:4'))
def without_flag(t):
    camera(t, flags=FREEZE, wait='0.5')
    t.fire('cam', activator='player2')
    t.wait(0.3)
    for p in PLAYERS:
        t.snap('during', p, 'flags')
    t.wait(0.5)
    t.expect.setview('player2', 'cam', count=1)
    t.expect.setview('player1', 'cam', count=0)
    t.expect.setview('player3', 'cam', count=0)
    t.expect.value('during', 'player2', 'flags', FL_FROZEN, op='&')
    t.expect.value('during', 'player1', 'flags', FL_FROZEN, op='!&')
    t.expect.value('during', 'player3', 'flags', FL_FROZEN, op='!&')


@S.test('Without "All Players", fired with no activator, by an info_target, by a spectating player1 and by a dead '
        'player3: nobody gets the view or is frozen, and each time the camera ends and fires its target',
        players=3, doc=DOC, covers=('trigger_camera', 'trigger_camera:4', 'trigger_camera.target'))
def without_flag_no_viewer(t):
    camera(t, flags=FREEZE, wait='0.5')
    healthy(t)
    t.mark('none')
    t.fire('cam')
    t.wait(0.3)
    for p in PLAYERS:
        t.snap('none', p, 'flags')
    t.wait(0.6)
    t.mark('target')
    t.fire('cam', activator='look')
    t.wait(0.3)
    for p in PLAYERS:
        t.snap('target', p, 'flags')
    t.wait(0.6)
    t.cmd('player1', 'spectate')
    t.wait(0.5)
    t.mark('spec')
    t.fire('cam', activator='player1')
    t.wait(0.9)
    t.damage('player3', 10000)
    t.wait(0.3)
    t.mark('dead')
    t.fire('cam', activator='player3')
    t.wait(0.3)
    t.snap('dead', 'player2', 'flags')
    t.wait(0.6)
    t.wait(1.0)                         # spectator mode toggles at most every 3 s
    t.cmd('player1', 'spectate')        # back in the game for the tests after this one
    t.wait(0.5)
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player3 20')
    t.wait(2.5)
    for p in PLAYERS:
        t.expect.setview(p, 'cam', count=0)
        t.expect.value('none', p, 'flags', FL_FROZEN, op='!&')
        t.expect.value('target', p, 'flags', FL_FROZEN, op='!&')
    t.expect.value('dead', 'player2', 'flags', FL_FROZEN, op='!&')
    for mark in ('none', 'target', 'spec', 'dead'):
        t.expect.used('look', caller='cam', after=mark, within=(0.4, 0.9), count=1)


@S.test('"All Players" (Freeze Player): a player disconnects while it runs: no crash, the others get their own view '
        'back and their controls at the hold time, the camera fires its target once', players=3, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:4', 'trigger_camera:8', 'trigger_camera.target'))
def all_disconnect(t):
    camera(t, flags=ALL | FREEZE, wait='1.2')
    t.fire('cam', activator='player2')
    t.wait(0.4)
    t.mark('left')
    t.console('test_player remove player3')
    t.wait(1.2)
    t.mark('ended')
    for p in ('player1', 'player2'):
        t.snap('after', p, 'flags')
    for p in ('player1', 'player2'):
        t.expect.setview(p, 'cam', count=1)
        t.expect.setview(p, p, after='left', count=1)
        t.expect.value('after', p, 'flags', FL_FROZEN, op='!&')
    t.expect.used('look', caller='cam', after='left', count=1)


@S.test('"All Players" with "Alive player only" (player3 left in the test before): player2 dies, the camera runs on '
        'for player1; it ends (firing its target) when none of its players is in play', players=2, doc=DOC,
        covers=('trigger_camera', 'trigger_camera:8', 'trigger_camera:1024', 'trigger_camera.target'))
def all_alive_only(t):
    camera(t, flags=ALL | ALIVE_ONLY, wait='10')
    healthy(t, ('player1', 'player2'))
    t.fire('cam', activator='player2')
    t.wait(0.4)
    t.mark('kill2')
    t.damage('player2', 10000)
    t.wait(0.5)
    t.mark('kill1')
    t.damage('player1', 10000)
    t.wait(0.5)
    for p in ('player1', 'player2'):
        t.expect.setview(p, 'cam', count=1)
    t.expect.not_used('look', after='kill2', within=(0, 0.4))
    t.expect.used('look', caller='cam', after='kill1', within=(0, 0.3), count=1)
