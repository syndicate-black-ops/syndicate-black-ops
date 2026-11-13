"""Tier 4 batch 4, real server: func_breakable Gib Model, path_track Fire On Pass, trigger_camera acceleration /
deceleration / delay, func_traincontrols, monster_scientist Prisoner. Each test checks the FGD's description
(sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('map_used6', 'Tier 4 batch 4: breakable gib model, path_track message, camera acceleration / '
              'deceleration / delay, func_traincontrols, scientist Prisoner')
S = SUITE

TE_BREAKMODEL = 108
GIBS = 'models/bonegibs.mdl'
SF_PRISONER = 16


@S.test('func_breakable Gib Model: it breaks into that model\'s gibs (TE_BREAKMODEL with its index)',
        covers=('func_breakable.gibmodel',))
def gib_model(t):
    t.brush('func_breakable', size=(32, 32, 32), at=(0, 100, 0), targetname='br', health='10', material='1',
            gibmodel=GIBS)
    t.wait(0.3)
    t.mark('break')
    t.fire('br')
    t.wait(0.3)
    t.expect.precached(GIBS, kind='model')
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'model': 'model:' + GIBS}, after='break', count=1)


@S.test('path_track Fire On Pass (message): a func_tracktrain passing the path_track fires it, once per pass',
        covers=('path_track.message',))
def path_track_message(t):
    t.point('path_track', at=(-200, 0, 32), targetname='p1', target='p2')
    t.point('path_track', at=(0, 0, 32), targetname='p2', target='p3', message='passed')
    t.point('path_track', at=(200, 0, 32), targetname='p3')
    t.point('info_target', targetname='passed')
    t.brush('func_tracktrain', size=(64, 32, 32), at=(-200, 0, 32), origin=True, targetname='tt', target='p1',
            speed='200', height='0', wheels='16', spawnflags='2')
    t.wait(0.5)
    t.mark('go')
    t.fire('tt', use='on')
    t.wait(2.5)
    t.expect.used('passed', count=1, after='go', within=(0.5, 2.5))
    t.expect.not_used('passed', after='go', within=(0, 0.5))


def camera_path(t, accel, decel='500'):
    t.point('info_target', targetname='look', at=(0, 200, 40))
    t.point('path_corner', targetname='c1', at=(-200, -100, 100), target='c2', speed='400')
    t.point('path_corner', targetname='c2', at=(200, -100, 100), target='c1', speed='400')
    t.point('trigger_camera', targetname='cam', at=(-200, -100, 100), target='look', moveto='c1', speed='0',
            wait='10', acceleration=accel, deceleration=decel, spawnflags='4')


@S.test('trigger_camera Acceleration 100: moving along its path to a path_corner speed 400 it speeds up by 100 a '
        'second (about 100 after 1 s); Acceleration 1000 reaches 400 within that second', players=1,
        covers=('trigger_camera.acceleration', 'trigger_camera.moveto'))
def camera_acceleration(t):
    camera_path(t, '100')
    t.fire('cam', activator='player1')
    t.wait(1.0)
    t.snap('slow', 'cam', 'speed')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.3)
    t.kv('cam', 'acceleration', '1000')
    t.fire('cam', activator='player1')
    t.wait(1.0)
    t.snap('fast', 'cam', 'speed')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.3)
    t.expect.value('slow', 'cam', 'speed', 100, tol=30)
    t.expect.value('fast', 'cam', 'speed', 400, tol=10)


@S.test('trigger_camera "Delay before trigger" 0.5: when its hold time (0.5 s) ends it fires its look target 0.5 s '
        'later', players=1, covers=('trigger_camera.delay',))
def camera_delay(t):
    t.point('info_target', targetname='look', at=(0, 200, 40))
    t.point('trigger_camera', targetname='cam', at=(-150, -150, 100), target='look', wait='0.5', delay='0.5',
            spawnflags='4')
    t.mark('on')
    t.fire('cam', activator='player1')
    t.wait(1.8)
    t.expect.used('look', count=1, after='on', within=(0.8, 1.4))


@S.test('func_traincontrols: a player standing on a func_tracktrain inside its controls who presses +use takes the '
        'controls; then +forward speeds the train up', players=1,
        covers=('func_traincontrols', 'func_traincontrols.target'))
def train_controls(t):
    t.point('path_track', at=(-150, 0, 8), targetname='p1', target='p2')
    t.point('path_track', at=(150, 0, 8), targetname='p2')
    t.brush('func_tracktrain', size=(128, 128, 16), at=(-150, 0, 0), origin=True, targetname='tt', target='p1',
            speed='200', height='0', wheels='32', bank='0')
    t.brush('func_traincontrols', size=(128, 128, 72), at=(-150, 0, 16), target='tt')
    t.wait(1.0)
    t.teleport('player1', (-150, 0, 17), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.5)
    t.move('player1', 0, 0, 2, '+use')
    t.wait(0.3)
    t.console('test_move %s 0 0 -1 +forward' % t.ref('player1'))
    t.wait(1.0)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.snap('driven', 'tt', 'speed')
    t.move('player1', 0, 0, 2, '+use')      # let go
    t.teleport('player1', (-200, -200, 0))
    t.expect.value('driven', 'tt', 'speed', 1, op='>=')


@S.test('monster_scientist flag 16 Prisoner: other monsters do not see it: a zombie facing a prisoner scientist takes no '
        'enemy; a zombie facing a scientist without the flag takes it as its enemy', players=1,
        covers=('monster_scientist:16',))
def scientist_prisoner(t):
    t.flags('player1', '+FL_NOTARGET')     # a client in the room: the monsters run their AI
    t.teleport('player1', (0, -220, 0))
    t.point('monster_scientist', targetname='pris', at=(-150, 100, 0), angles='0 270 0', spawnflags=str(SF_PRISONER))
    t.point('monster_zombie', targetname='g1', at=(-150, -100, 0), angles='0 90 0')
    t.point('monster_scientist', targetname='free', at=(150, 100, 0), angles='0 270 0')
    t.point('monster_zombie', targetname='g2', at=(150, -100, 0), angles='0 90 0')
    t.brush('func_wall', size=(16, 400, 200), at=(0, 0, 0))
    t.wait(2.0)
    t.snap('s', 'g1', 'enemy')
    t.snap('s', 'g2', 'enemy')
    t.point('trigger_relay', targetname='rm', killtarget='g2')
    t.fire('rm')
    t.flags('player1', '-FL_NOTARGET')
    t.teleport('player1', (-220, -220, 0))
    t.expect.value('s', 'g1', 'enemy', '')
    t.expect.value('s', 'g2', 'enemy', 'monster_scientist')
