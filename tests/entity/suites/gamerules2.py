"""Game rules and player-facing rule changes of SBO 2.0 on a real server (GAPS.md tier 2 "Round, mission and rules
commits"): cvar defaults (87245126, 4d64759a, 4755c07b, 10b402b5, 802ed4bf), the camera view given back when its
player dies or becomes a spectator (30c40fd6), game_messagebox All Players only to players in play (506baf66) and
game_zone_player leaving spectators out (719d7d4e). The round flow itself (hardcore default, spectators not counted,
mp_loadouttime, mp_missionendtime, observers at a held mission end, counting by scan) is in the map-test cases 101-101f.
The spectator tests come last: player2 stays a spectator to the end."""

from etest import Suite

SUITE = Suite('gamerules2', 'Cvar defaults, camera view back on death / spectate, message box and player zone with a '
              'spectator', players=2)
S = SUITE

DMG_NEVERGIB = 4096


def info(t, name, at=(200, -200, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


@S.test('cvar defaults as the shipped cfg files set them: mp_hardcore 100, mp_gmmode 0, mp_loadouttime 45, '
        'mp_missionendtime 5 (mp_timelimit 20 is not checked: the harness\'s server.cfg sets 0)',
        covers=('worldspawn',))
def cvar_defaults(t):
    for cvar in ('mp_hardcore', 'mp_gmmode', 'mp_loadouttime', 'mp_missionendtime'):
        t.console(cvar)
    t.wait(0.1)
    t.expect.console('"mp_hardcore" is "100"')
    t.expect.console('"mp_gmmode" is "0"')
    t.expect.console('"mp_loadouttime" is "45"')
    t.expect.console('"mp_missionendtime" is "5"')


def restart_round(t):
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)


def camera(t, name='cam'):
    t.point('info_target', targetname='look', at=(150, 150, 40))
    t.point('trigger_camera', targetname=name, at=(-150, -150, 100), target='look', wait='10')


@S.test('trigger_camera: the player whose view it has dies: he gets his own view back at once (30c40fd6)', players=2,
        covers=('trigger_camera',))
def camera_death(t):
    camera(t)
    t.wait(2.5)     # past the 2 s spawn protection of the suite start (CCoop::PlayerSpawn)
    t.fire('cam', activator='player1')
    t.wait(0.5)
    t.mark('kill')
    t.damage('player1', 5000)
    t.wait(0.5)
    t.snap('dead', 'player1', 'deadflag')
    restart_round(t)
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.setview('player1', 'cam')
    t.expect.setview('player1', 'player1', after='kill', within=(0, 0.3))


@S.test('change_touch: its model idles (an idle sequence playing, framerate 1) instead of standing frozen on its first '
        'frame (ef4f5053)', covers=('change_touch', 'change_touch.model'))
def change_touch_idle(t):
    t.point('change_touch', at=(220, 220, 0), angles='0 225 0', model='models/gman.mdl')
    t.wait(1.0)
    t.snap('s', '@change_touch', 'framerate')
    t.expect.value('s', '@change_touch', 'framerate', 0, op='>')


@S.test('Death weapon box: a player with his loadout weapons who dies drops a weaponbox (control of the next test)',
        players=2, covers=())
def deathbox_loadout(t):
    t.teleport('player1', (0, 0, 0))
    t.wait(0.5)
    t.mark('kill')
    t.damage('player1', 5000, DMG_NEVERGIB)
    t.wait(0.5)
    restart_round(t)
    t.expect.spawned('@weaponbox', min=1)


@S.test('Death weapon box: a player without weapons (player_weaponstrip) who dies drops no empty weaponbox '
        '(ade6d4f9: an empty box was an edict for 120 s per death)', players=2,
        covers=('player_weaponstrip',))
def deathbox_empty(t):
    t.point('player_weaponstrip', targetname='strip')
    t.teleport('player1', (0, 0, 0))
    t.wait(0.5)
    t.fire('strip', activator='player1')
    t.wait(0.3)
    t.mark('kill')
    t.damage('player1', 5000, DMG_NEVERGIB)
    t.wait(0.5)
    restart_round(t)
    t.expect.spawned('@weaponbox', count=0)


@S.test('game_messagebox All Players (1): only the players in play get it; a spectator does not (506baf66)',
        players=2, covers=('game_messagebox:1',))
def messagebox_all_spectator(t):
    t.point('game_messagebox', targetname='mb', spawnflags='1', message='tier2 box')
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.mark('go')
    t.fire('mb', activator='player1')
    t.wait(0.5)
    t.expect.msg('MessageBox', to='player1', after='go')
    t.expect.no_msg('MessageBox', to='player2', after='go')


@S.test('game_zone_player: a spectator is neither in nor out of the zone; the player outside is counted out '
        '(719d7d4e)', players=2, covers=('game_zone_player', 'game_zone_player.outtarget',
                                          'game_zone_player.intarget'))
def zone_spectator(t):
    t.trigger('game_zone_player', size=(128, 128, 96), at=(0, 150, 0), targetname='zone', intarget='in',
              outtarget='out')
    info(t, 'in')
    info(t, 'out', at=(-200, -200, 0))
    t.teleport('player1', (0, -150, 0))
    t.wait(0.5)
    t.mark('go')
    t.fire('zone', activator='player1')
    t.wait(0.3)
    t.expect.fired('out', activator='player1', count=1, after='go')
    t.expect.not_fired('in', after='go')


@S.test('trigger_camera: the player whose view it has goes to spectator mode: he gets his own view back at once '
        '(30c40fd6)', players=2, covers=('trigger_camera',))
def camera_spectate(t):
    camera(t, 'cam2')
    t.cmd('player2', 'spectate')        # back in the game (the earlier tests left him spectating)
    t.wait(0.5)
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(2.5)
    t.fire('cam2', activator='player1')
    t.wait(0.5)
    t.mark('spec')
    t.cmd('player1', 'spectate')
    t.wait(0.5)
    t.expect.setview('player1', 'cam2')
    t.expect.setview('player1', 'player1', after='spec', within=(0, 0.3))
