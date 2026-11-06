"""Featureful options on shared entities (docs/FEATUREFUL_OPTIONS.md):
the new keys, flags and choices no stock or custom map sets. One or more tests per option, each showing the option
working (and, where it helps, the Half-Life behavior next to it). The trigger_camera "Alive player only" test kills
player2, so it stays last."""

from etest import Suite

SUITE = Suite('options', 'Featureful options: trigger_camera, func_vehicle, triggers, logic, env_explosion, env_beam',
              players=2)
S = SUITE
LOGIC = 'FEATUREFUL_OPTIONS.md'
VEHICLE = 'FEATUREFUL_OPTIONS.md#func_vehicle'
FL_FROZEN = 4096
SND_STOP = 32


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


def healthy(t, player='player1', hp='100'):
    t.kv(player, 'health', hp)
    t.kv(player, 'max_health', hp)


# ---------------------------------------------------------------------------------------------- trigger_camera
def camera(t, name='cam', flags=0, wait='10', at=(-150, -150, 100), look=(200, 200, 40), **kv):
    info(t, 'look', at=look)
    t.point('trigger_camera', targetname=name, at=at, target='look', wait=wait, spawnflags=str(flags), **kv)


@S.test('trigger_camera "Player Invulnerable" (256): no damage while it has the view, damage again after', players=1,
        doc=LOGIC)
def cam_invulnerable(t):
    camera(t, flags=4 | 256, wait='0.6')
    healthy(t)
    t.fire('cam', activator='player1')
    t.wait(0.2)
    t.damage('player1', 30)
    t.wait(0.1)
    t.snap('during', 'player1', 'health')
    t.wait(0.8)
    t.damage('player1', 30)
    t.wait(0.1)
    t.snap('after', 'player1', 'health')
    t.expect.value('during', 'player1', 'health', 100, tol=0.5)
    t.expect.value('after', 'player1', 'health', 70, tol=1.5)


@S.test('trigger_camera "Ignore Hold Time" (512): still running past its hold time, ends when turned off', players=1,
        doc=LOGIC)
def cam_ignore_hold(t):
    camera(t, flags=4 | 512, wait='0.3')
    t.fire('cam', activator='player1')
    t.wait(1.0)
    t.snap('held', 'player1', 'flags')
    t.mark('off')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.3)
    t.snap('after', 'player1', 'flags')
    t.expect.value('held', 'player1', 'flags', FL_FROZEN, op='&')        # past its 0.3 s hold time: still frozen
    t.expect.not_used('look', within=(0, 0.9))
    t.expect.setview('player1', 'player1', after='off', min=1)
    t.expect.value('after', 'player1', 'flags', FL_FROZEN, op='!&')


@S.test('trigger_camera "Don\'t fire look target" (4096) and "Target when turned off": only the off target fires, '
        'with the player as activator', players=1, doc=LOGIC)
def cam_off_target(t):
    camera(t, flags=4096, wait='0.3', m_iszTurnedOffTarget='off')
    info(t, 'off')
    t.fire('cam', activator='player1')
    t.wait(0.8)
    t.expect.not_used('look')
    t.expect.used('off', activator='player1', count=1)


@S.test('trigger_camera "Target when turned off" without 4096: the look target and the off target both fire',
        players=1, doc=LOGIC)
def cam_off_target_both(t):
    camera(t, flags=0, wait='0.3', m_iszTurnedOffTarget='off')
    info(t, 'off')
    t.fire('cam', activator='player1')
    t.wait(0.8)
    t.expect.used('look', caller='cam', count=1)
    t.expect.used('off', activator='player1', count=1)


def cam_path(t, flags, first=(150, -100, 100), second=(150, 200, 100)):
    info(t, 'look', at=(200, 200, 40))
    t.point('path_corner', targetname='p1', at=first, target='p2', speed='100')
    t.point('path_corner', targetname='p2', at=second, speed='100')
    t.point('trigger_camera', targetname='cam', at=(-150, -100, 100), target='look', moveto='p1', wait='1.2',
            spawnflags=str(flags), speed='100', acceleration='500', deceleration='500')


@S.test('trigger_camera "Don\'t slow down if not Freeze Player" (8192): keeps its path speed (Half-Life damps it)',
        players=1, doc=LOGIC)
def cam_no_slowdown(t):
    cam_path(t, 8192, first=(-150, 0, 100), second=(-150, 200, 100))
    t.point('path_corner', targetname='q1', at=(150, 0, 100), target='q2', speed='100')
    t.point('path_corner', targetname='q2', at=(150, 200, 100), speed='100')
    t.point('trigger_camera', targetname='ctl', at=(150, -100, 100), target='look', moveto='q1', wait='1.2',
            spawnflags='0', speed='100', acceleration='500', deceleration='500')
    t.fire('cam', activator='player1')
    t.fire('ctl', activator='player1')
    t.wait(1.0)     # the velocity eases to the path speed (2 x frametime a frame): past 40 after about 0.5 s, later at a low timescale
    t.snap('run', 'cam', 'velocity')
    t.snap('run', 'ctl', 'velocity')
    t.expect.value('run', 'cam', 'velocity.y', 40, op='>')
    t.expect.value('run', 'ctl', 'velocity.y', 30, op='<')


@S.test('trigger_camera "Don\'t skip the first path corner" (16384): heads for the first corner, not the second',
        players=1, doc=LOGIC)
def cam_first_corner(t):
    cam_path(t, 4 | 16384)
    t.fire('cam', activator='player1')
    t.wait(0.1)
    t.snap('run', 'cam', 'movedir')
    t.expect.value('run', 'cam', 'movedir', [1, 0, 0], tol=0.02)     # toward p1 (east), not p2 (north-east)


@S.test('trigger_camera without 16384 (Half-Life): heads for the second corner', players=1, doc=LOGIC)
def cam_first_corner_hl(t):
    cam_path(t, 4)
    t.fire('cam', activator='player1')
    t.wait(0.1)
    t.snap('run', 'cam', 'movedir')
    t.expect.value('run', 'cam', 'movedir', [0.707, 0.707, 0], tol=0.02)


@S.test('trigger_camera "Instant Initial Turn" (32768): faces its target when it starts', players=1, doc=LOGIC)
def cam_initial_turn(t):
    # the target straight north of the camera: yaw 90; Half-Life starts at the camera's angles (0) and turns
    camera(t, name='cam', flags=32768, at=(-150, -150, 100), look=(-150, 200, 100))
    t.point('trigger_camera', targetname='ctl', at=(-150, -150, 100), target='look', wait='10', spawnflags='0')
    t.fire('cam', activator='player1')
    t.snap('start', 'cam', 'angles')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.2)
    t.fire('ctl', activator='player1')
    t.snap('start', 'ctl', 'angles')
    t.fire('ctl', activator='player1', use='off')
    t.wait(0.2)
    t.expect.value('start', 'cam', 'angles.y', 90, tol=0.5)
    t.expect.value('start', 'ctl', 'angles.y', 0, tol=0.5)


@S.test('trigger_camera "Stop by player input" = USE input: +use ends it (Freeze Player)', players=1, doc=LOGIC)
def cam_stop_by_use(t):
    camera(t, flags=4, wait='10', stop_by_player_input='1')
    t.fire('cam', activator='player1')
    t.wait(0.5)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.4)
    t.snap('after', 'player1', 'flags')
    t.expect.setview('player1', 'cam', count=1)
    t.expect.setview('player1', 'player1', after='use', min=1)
    t.expect.used('look', caller='cam', after='use', count=1)
    t.expect.value('after', 'player1', 'flags', FL_FROZEN, op='!&')


@S.test('trigger_camera "Stop by player input" with Freeze Player: the player is held by his capabilities, not FL_FROZEN '
        '(the engine drops a frozen client\'s buttons), so he cannot walk but his +use gets through', players=1, doc=LOGIC)
def cam_stop_by_use_freeze(t):
    camera(t, flags=4, wait='10', stop_by_player_input='1')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(0.3)
    t.mark('cam_on')
    t.fire('cam', activator='player1')
    t.wait(0.3)
    t.snap('a', 'player1', 'flags', 'origin')
    t.walk('player1', 300, 0, 0.5)
    t.snap('b', 'player1', 'origin')
    t.mark('cam_off')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.3)
    t.expect.value('a', 'player1', 'flags', FL_FROZEN, op='!&')
    t.expect.moved('a', 'b', 'player1', (0, 0, 0), tol=2)
    # the client gets the camera's bits (movement, jump, duck, attack: 39) and 0 back: its weapon prediction must not
    # keep dropping the attack button after the camera (no weapon animation)
    t.expect.msg('Capability', to='player1', fields={'bits': 39}, after='cam_on', min=1)
    t.expect.msg('Capability', to='player1', fields={'bits': 0}, after='cam_off', count=1)


@S.test('trigger_camera without "Stop by player input": +use does not end it', players=1, doc=LOGIC)
def cam_stop_by_use_off(t):
    camera(t, flags=4, wait='10')
    t.fire('cam', activator='player1')
    t.wait(0.5)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.4)
    t.snap('after', 'player1', 'flags')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.2)
    t.expect.value('after', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.not_used('look', within=(0, 0.9))


# ---------------------------------------------------------------------------------------------- func_vehicle
def vehicle(t, name='car', at=(0, 0, 0), **kv):
    """A 64 x 32 slab on a two-path_track path in the middle of the room (room to move both ways)."""
    x, y, z = at
    t.point('path_track', at=(x, y, 16), targetname=name + 'p1', target=name + 'p2')
    t.point('path_track', at=(x + 100, y, 16), targetname=name + 'p2')
    keys = dict(speed='300', sounds='3', length='64', width='32', height='4')
    keys.update(kv)
    t.brush('func_vehicle', size=(64, 32, 16), at=at, origin=True, targetname=name, target=name + 'p1', **keys)


def reverse(t, name, calls=60):
    for _ in range(calls):
        t.fire(name, use='set', value=-1)
        t.wait_frames(1)


@S.test('func_vehicle "reverse_speed" 300: reverses past 0.35 x speed (Half-Life stops at -105 for speed 300)',
        doc=VEHICLE, size=(512, 512, 200))
def veh_reverse(t):
    vehicle(t, 'car', at=(0, -100, 0), reverse_speed='300')
    vehicle(t, 'ctl', at=(0, 100, 0))
    t.wait(0.5)
    reverse(t, 'car')
    t.snap('car', 'car', 'speed')
    reverse(t, 'ctl')
    t.snap('ctl', 'ctl', 'speed')
    t.expect.value('car', 'car', 'speed', -120, op='<')
    t.expect.value('ctl', 'ctl', 'speed', -105.5, op='>=')


@S.test('func_vehicle "deceleration" 50: loses 50 per 0.1 s with no throttle (Half-Life: 20)', doc=VEHICLE,
        size=(512, 512, 200))
def veh_deceleration(t):
    vehicle(t, 'car', at=(-100, -100, 0), deceleration='50')
    vehicle(t, 'ctl', at=(-100, 100, 0))
    t.wait(0.5)
    t.fire('car')           # toggled on at full speed 300, nobody at the wheel
    t.fire('ctl')
    t.wait(0.45)
    t.snap('s', 'car', 'speed')
    t.snap('s', 'ctl', 'speed')
    t.expect.value('s', 'car', 'speed', 120, op='<')
    t.expect.value('s', 'ctl', 'speed', 150, op='>')


@S.test('func_vehicle "stop_sound_at_halt" 1: the engine sound stops once it stands with nobody at the wheel',
        doc=VEHICLE, size=(512, 512, 200))
def veh_stop_at_halt(t):
    vehicle(t, 'car', at=(-100, -100, 0), deceleration='150', stop_sound_at_halt='1')
    vehicle(t, 'ctl', at=(-100, 100, 0), deceleration='150')
    t.wait(0.5)
    t.mark('go')
    t.fire('car')
    t.fire('ctl')
    t.wait(1.0)
    t.expect.event('vehicle', ent='car', fields={'bparam1': 1}, after='go', min=1)
    t.expect.event('vehicle', ent='ctl', fields={'bparam1': 1}, after='go', count=0)


@S.test('func_vehicle "brake_sound": the custom sound plays when the engine starts, not plats/vehicle_brake1.wav',
        doc=VEHICLE, size=(512, 512, 200))
def veh_brake_sound(t):
    # the engine sound (and with it the brake sound) starts when the vehicle finds its path at map start, or when it
    # moves again after stop_sound_at_halt / a stop: the stop and the restart here show the second
    vehicle(t, 'car', brake_sound='plats/ttrain_brake1.wav', sounds='5')
    t.wait(0.5)
    t.fire('car')
    t.wait(0.2)
    t.fire('car')           # stopped: StopSound
    t.wait(0.2)
    t.mark('go')
    t.fire('car')           # moves again: the engine sound starts with the brake sound
    t.wait(1.2)             # UpdateSound runs once a second
    t.expect.precached('plats/ttrain_brake1.wav', kind='sound')
    t.expect.sound('ttrain_brake1', ent='car', after='go', min=1)
    t.expect.sound('vehicle_brake1', ent='car', count=0)


@S.test('func_vehicle "ignition_sound": the custom sound plays when a player takes the wheel', players=1,
        doc=VEHICLE, size=(512, 512, 200))
def veh_ignition_sound(t):
    t.point('path_track', targetname='p1', target='p2', at=(0, -150, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 150, 0))
    t.brush('func_vehicle', size=(128, 128, 16), at=(0, -150, 0), origin=True, targetname='car', target='p1',
            height='8', length='128', width='128', speed='200', acceleration='5', sounds='3',
            ignition_sound='plats/ttrain_start1.wav')
    t.brush('func_vehiclecontrols', size=(128, 128, 128), at=(0, -150, 0), target='car')
    t.wait(0.5)
    t.teleport('player1', (0, -150, 16), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.move('player1', 0, 0, 3, '+use')         # leave the controls
    t.wait(0.3)
    t.expect.sound('ttrain_start1', ent='player1', after='use', min=1)
    t.expect.sound('vehicle_ignition', ent='player1', after='use', count=0)


@S.test('func_vehicle "touch_proxy_name": its proxy fires a trigger_multiple with "Everything else" (8), not one '
        'without it; the proxy goes with the vehicle', doc=VEHICLE, size=(512, 512, 200))
def veh_touch_proxy(t):
    vehicle(t, 'car', at=(0, 0, 0), touch_proxy_name='prox', names=('touch_proxy_name',))
    for x in (-140, 140):                       # whichever way it drives
        t.trigger('trigger_multiple', size=(64, 128, 96), at=(x, 0, 0), target='hit', spawnflags='8', wait='0.1')
        t.trigger('trigger_multiple', size=(64, 128, 96), at=(x, 0, 0), target='nohit', spawnflags='0', wait='0.1')
    info(t, 'hit')
    info(t, 'nohit')
    t.point('trigger_relay', targetname='kill', killtarget='car', triggerstate='2')
    t.wait(0.5)
    t.fire('car')
    t.wait(1.0)
    t.fire('kill')
    t.wait(0.5)
    t.expect.used('hit', min=1)
    t.expect.not_used('nohit')
    t.expect.removed('@func_vehicle_proxy')


# ---------------------------------------------------------------------------------------------- triggers
@S.test('trigger_once / trigger_multiple "noise" (Sound to play): precached with the map and played at the trigger '
        'when a player walks in', players=1, doc=LOGIC)
def trigger_noise(t):
    t.trigger('trigger_once', targetname='tonce', size=(64, 64, 96), at=(-120, 0, 0), target='once_out',
              noise='buttons/bell1.wav')
    t.trigger('trigger_multiple', targetname='tmult', size=(64, 64, 96), at=(120, 0, 0), target='mult_out',
              noise='buttons/blip1.wav', wait='0.2')
    info(t, 'once_out')
    info(t, 'mult_out')
    t.wait(0.3)
    t.teleport('player1', (-120, 0, 0))
    t.wait(0.3)
    t.teleport('player1', (120, 0, 0))
    t.wait(0.3)
    t.expect.precached('buttons/bell1.wav', kind='sound')
    t.expect.precached('buttons/blip1.wav', kind='sound')
    t.expect.used('once_out', count=1)
    t.expect.used('mult_out', min=1)
    t.expect.sound('buttons/bell1.wav', ent='tonce', count=1)
    t.expect.sound('buttons/blip1.wav', ent='tmult', min=1)


@S.test('delayed targets keep any activator (Featureful c0d5b843): a trigger_relay with delay fires its target with '
        'itself as activator, as an undelayed one does (Half-Life kept only a player and fired with none)', doc=LOGIC)
def delay_keeps_activator(t):
    t.point('trigger_relay', targetname='rl', target='out', delay='0.3', triggerstate='2')
    t.point('trigger_relay', targetname='rn', target='outn', triggerstate='2')
    info(t, 'out')
    info(t, 'outn')
    t.wait(0.3)
    t.fire('rl')
    t.fire('rn')
    t.wait(0.6)
    t.expect.used('out', activator='rl', count=1)
    t.expect.used('outn', activator='rn', count=1)


def push_pair(t, flag):
    """Two "Push Once Only" (1) triggers with `flag`: the toucher teleported into one is pushed and the trigger
    removed; a refused toucher leaves it in place."""
    t.point('monster_scientist', targetname='sci', at=(0, 200, 0), angles='0 90 0')
    t.trigger('trigger_push', targetname='tm', size=(96, 96, 160), at=(-120, 0, 0), angles='-90 0 0', speed='300',
              spawnflags=str(1 | flag))
    t.trigger('trigger_push', targetname='tp', size=(96, 96, 160), at=(120, 0, 0), angles='-90 0 0', speed='300',
              spawnflags=str(1 | flag))
    t.wait(0.3)
    # a monster touches triggers when it moves (SV_Physics_Step), not when it is placed: it falls into the trigger
    t.teleport('sci', (-120, 0, 110))
    t.flags('sci', '-FL_ONGROUND')
    t.teleport('player1', (120, 0, 0), yaw=90)
    t.wait(0.3)
    t.snap('after', 'tm', 'solid')
    t.snap('after', 'tp', 'solid')


@S.test('trigger_push "No clients" (8): the player is not pushed, a scientist is', players=1, doc=LOGIC)
def push_no_clients(t):
    push_pair(t, 8)
    t.expect.removed('tm')                              # pushed the scientist
    t.expect.value('after', 'tp', 'solid', 1)           # still there (SOLID_TRIGGER): refused the player


@S.test('trigger_push "No monsters" (16): a scientist is not pushed, the player is', players=1, doc=LOGIC)
def push_no_monsters(t):
    push_pair(t, 16)
    t.expect.value('after', 'tm', 'solid', 1)           # refused the scientist
    t.expect.removed('tp')                              # pushed the player


@S.test('trigger_hurt "Full dmg every 0.5 seconds" (128): 20 per tick for dmg 20; without it 10', players=2, doc=LOGIC)
def hurt_full(t):
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(-120, 0, 0), dmg='20', spawnflags='128')
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(120, 0, 0), dmg='20', spawnflags='0')
    healthy(t, 'player1', '1000')
    healthy(t, 'player2', '1000')
    t.mark('in')
    t.teleport('player1', (-120, 0, 0))
    t.teleport('player2', (120, 0, 0))
    t.wait(0.3)                                 # one tick each (the next comes 0.5 s later)
    t.teleport('player1', (-120, 180, 0))
    t.teleport('player2', (120, 180, 0))
    t.wait(0.2)
    t.expect.damaged('player1', after='in', total=(19, 21))
    t.expect.damaged('player2', after='in', total=(9, 11))


@S.test('trigger_hurt "Untouchable entity name" player: players are not hurt', players=1, doc=LOGIC)
def hurt_untouchable(t):
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(0, 0, 0), dmg='20', untouchable_ent_name='player')
    healthy(t, 'player1', '1000')
    t.mark('in')
    t.teleport('player1', (0, 0, 0))
    t.wait(0.7)
    t.teleport('player1', (0, 180, 0))
    t.wait(0.1)
    t.expect.not_damaged('player1', after='in')


@S.test('trigger_teleport "Random destination" (64) and info_teleport_destination "Trigger on arrival" (32): '
        'both destinations are used, each fires its target with the player as activator', players=1, doc=LOGIC)
def teleport_random(t):
    t.trigger('trigger_teleport', size=(64, 64, 96), at=(0, -150, 0), target='d', spawnflags='64')
    t.point('info_teleport_destination', targetname='d', at=(-150, 150, 0), target='arrA', spawnflags='32')
    t.point('info_teleport_destination', targetname='d', at=(150, 150, 0), target='arrB', spawnflags='32')
    info(t, 'arrA')
    info(t, 'arrB')
    for _ in range(10):
        t.teleport('player1', (0, -150, 0))
        t.wait(0.15)
    t.teleport('player1', (0, 0, 0))
    t.wait(0.1)
    t.expect.used('arrA', activator='player1', min=1)
    t.expect.used('arrB', activator='player1', min=1)


# ---------------------------------------------------------------------------------------------- logic
@S.test('game_counter "Fire if over limit" (4): fires at the limit and again above it; without it only at the limit',
        doc=LOGIC)
def counter_over_limit(t):
    t.point('game_counter', targetname='cnt', target='hit', health='2', frags='0', spawnflags='4')
    t.point('game_counter', targetname='ctl', target='hit2', health='2', frags='0', spawnflags='0')
    info(t, 'hit')
    info(t, 'hit2')
    for _ in range(3):
        t.fire('cnt', use='on')
        t.fire('ctl', use='on')
        t.wait(0.1)
    t.expect.used('hit', count=2)
    t.expect.used('hit2', count=1)


@S.test('multi_manager "Once only" (8): fires its targets once, then is removed', doc=LOGIC)
def mm_once(t):
    t.point('multi_manager', targetname='mm', a='0', b='0.2', spawnflags='8')
    info(t, 'a')
    info(t, 'b')
    t.fire('mm')
    t.wait(0.5)
    t.fire('mm')
    t.wait(0.5)
    t.expect.used('a', count=1)
    t.expect.used('b', count=1)
    t.expect.removed('mm')


@S.test('multi_manager "Instant" (1024): a delay 0 target fires in the call itself; Half-Life waits for its think',
        doc=LOGIC)
def mm_instant(t):
    t.point('multi_manager', targetname='mmi', ci='0', spawnflags='1024')
    t.point('multi_manager', targetname='mmn', cn='0', spawnflags='0')
    t.point('game_counter', targetname='ci', health='100', frags='0')
    t.point('game_counter', targetname='cn', health='100', frags='0')
    t.wait(0.1)
    t.fire('mmi')
    t.fire('mmn')
    t.snap('now', 'ci', 'frags')
    t.snap('now', 'cn', 'frags')
    t.wait(0.2)
    t.snap('later', 'cn', 'frags')
    t.expect.value('now', 'ci', 'frags', 1)
    t.expect.value('now', 'cn', 'frags', 0)
    t.expect.value('later', 'cn', 'frags', 1)


@S.test('trigger_relay "Delay Before Reset" 1: calls within 1 s of a fire are ignored', doc=LOGIC)
def relay_reset_delay(t):
    t.point('trigger_relay', targetname='r', target='hit', triggerstate='2', m_flDelayBeforeReset='1')
    info(t, 'hit')
    for _ in range(3):
        t.fire('r')
        t.wait(0.2)
    t.wait(0.8)
    t.fire('r')
    t.wait(0.1)
    t.expect.used('hit', count=2)


# ---------------------------------------------------------------------------------------------- env_explosion
@S.test('env_explosion "iradius" 300: hurts a crate 200 units away; the Half-Life radius (2.5 x 50) does not',
        doc='FEATUREFUL_OPTIONS.md#other-entities')
def explosion_radius(t):
    # 400 units between the two pairs: each crate is in reach of its own explosion only
    t.brush('func_breakable', targetname='far', at=(100, -200, 0), size=(32, 32, 32), health='1000')
    t.brush('func_breakable', targetname='far2', at=(100, 200, 0), size=(32, 32, 32), health='1000')
    t.point('env_explosion', targetname='boom', at=(-100, -200, 16), iMagnitude='50', iradius='300', spawnflags='0')
    t.point('env_explosion', targetname='boom2', at=(-100, 200, 16), iMagnitude='50', spawnflags='0')
    t.wait(0.1)
    t.fire('boom')
    t.fire('boom2')
    t.wait(0.5)
    t.expect.damaged('far', min=1)
    t.expect.not_damaged('far2')


@S.test('env_explosion "Activator is attacker" (128): the activator gets the damage credit; without it the explosion',
        players=1, doc='FEATUREFUL_OPTIONS.md#other-entities')
def explosion_attacker(t):
    t.brush('func_breakable', targetname='crate', at=(100, -100, 0), size=(32, 32, 32), health='1000')
    t.brush('func_breakable', targetname='crate2', at=(100, 100, 0), size=(32, 32, 32), health='1000')
    t.point('env_explosion', targetname='boom', at=(60, -100, 16), iMagnitude='50', spawnflags='128')
    t.point('env_explosion', targetname='boom2', at=(60, 100, 16), iMagnitude='50', spawnflags='0')
    t.teleport('player1', (-200, 0, 0))
    t.wait(0.1)
    t.fire('boom', activator='player1')
    t.fire('boom2', activator='player1')
    t.wait(0.5)
    t.expect.damaged('crate', attacker='player1', min=1)
    t.expect.damaged('crate2', attacker='boom2', min=1)


@S.test('env_explosion "No Sound" (64): the explosion temp entity carries TE_EXPLFLAG_NOSOUND', doc='FEATUREFUL_OPTIONS.md#other-entities')
def explosion_no_sound(t):
    t.point('env_explosion', targetname='boom', at=(0, 0, 16), iMagnitude='50', spawnflags='65')   # + No Damage
    t.wait(0.1)
    t.mark('go')
    t.fire('boom')
    t.wait(0.3)
    t.expect.msg('svc:23', fields={'te': 3, 'flags': 4}, after='go', min=1)


# ---------------------------------------------------------------------------------------------- env_beam
@S.test('env_beam "Draw Solid" (512) and "Draw Sine" (1024): the beam flags (pev->rendermode high bits 0x20, 0x10)',
        doc='FEATUREFUL_OPTIONS.md#other-entities')
def beam_flags(t):
    info(t, 'b1', at=(-150, 0, 64))
    info(t, 'b2', at=(150, 0, 64))
    t.point('env_beam', targetname='solid', LightningStart='b1', LightningEnd='b2', spawnflags=str(1 | 512),
            life='0', BoltWidth='20', texture='sprites/laserbeam.spr', renderamt='200', rendercolor='255 0 0',
            names=('LightningStart', 'LightningEnd'))
    t.point('env_beam', targetname='sine', LightningStart='b1', LightningEnd='b2', spawnflags=str(1 | 1024),
            life='0', BoltWidth='20', texture='sprites/laserbeam.spr', renderamt='200', rendercolor='0 255 0',
            names=('LightningStart', 'LightningEnd'))
    t.wait(0.5)
    t.snap('on', 'solid', 'rendermode')
    t.snap('on', 'sine', 'rendermode')
    t.expect.value('on', 'solid', 'rendermode', 0x20, op='&')      # BEAM_FSOLID
    t.expect.value('on', 'sine', 'rendermode', 0x10, op='&')       # BEAM_FSINE


# ---------------------------------------------------------------------------------------------- last: kills player2
# ---------------------------------------------------------------------- parity batch 1: triggers and logic (A)
@S.test('trigger_hurt "Minimum health threshold" 60: the player is hurt down to 60 and no further', players=1, doc=LOGIC)
def hurt_min_health(t):
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(0, 0, 0), dmg='30', spawnflags='128', min_health='60')
    healthy(t)
    t.teleport('player1', (0, 0, 0))
    t.wait(3.0)
    t.snap('s', 'player1', 'health', 'deadflag')
    t.teleport('player1', (0, 180, 0))
    t.expect.value('s', 'player1', 'health', 60, tol=0.5)
    t.expect.value('s', 'player1', 'deadflag', 0)


@S.test('trigger_hurt "Ignore Armor" (256): the armor stays at 100, health takes the whole damage (in SBO armor takes '
        'trigger damage only for a player without an armor class; test players have one, so the second trigger, '
        'without the flag, leaves the armor too)', players=2, doc=LOGIC)
def hurt_ignore_armor(t):
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(-120, 0, 0), dmg='20', spawnflags='384')
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(120, 0, 0), dmg='20', spawnflags='128')
    for p in ('player1', 'player2'):
        healthy(t, p, '1000')
        t.kv(p, 'armorvalue', '100')
    t.mark('in')
    t.teleport('player1', (-120, 0, 0))
    t.teleport('player2', (120, 0, 0))
    t.wait(0.3)
    t.teleport('player1', (-120, 180, 0))
    t.teleport('player2', (120, 180, 0))
    t.wait(0.1)
    t.snap('s', 'player1', 'armorvalue')
    t.snap('s', 'player2', 'armorvalue')
    t.expect.damaged('player1', after='in')
    t.expect.value('s', 'player1', 'armorvalue', 100, tol=0.01)
    t.expect.damaged('player2', after='in')


@S.test('trigger_hurt "No Camera Punch" (512): the view is not kicked; without the flag it is', players=2, doc=LOGIC)
def hurt_no_punch(t):
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(-120, 0, 0), dmg='20', spawnflags='640')
    t.trigger('trigger_hurt', size=(96, 96, 96), at=(120, 0, 0), dmg='20', spawnflags='128')
    for p in ('player1', 'player2'):
        healthy(t, p, '1000')
    t.mark('in')
    t.teleport('player1', (-120, 0, 0))
    t.teleport('player2', (120, 0, 0))
    t.wait_frames(1)
    t.snap('s', 'player1', 'punchangle')
    t.snap('s', 'player2', 'punchangle')
    t.teleport('player1', (-120, 180, 0))
    t.teleport('player2', (120, 180, 0))
    t.expect.damaged('player1', after='in')
    t.expect.value('s', 'player1', 'punchangle.x', 0, tol=0.01)
    t.expect.value('s', 'player2', 'punchangle.x', -0.5, op='<')     # Half-Life kicks the view by -2


@S.test('trigger_hurt "Gibbing policy": Always gib removes the killed zombie (gibs), Never gib leaves its body',
        doc=LOGIC)
def hurt_gib_policy(t):
    t.point('monster_zombie', targetname='zgib', at=(-120, 0, 0), angles='0 90 0')
    t.point('monster_zombie', targetname='zbody', at=(120, 0, 0), angles='0 90 0')
    t.trigger('trigger_hurt', targetname='hg', size=(96, 96, 96), at=(-120, 0, 0), dmg='4000', spawnflags='66',
              gib_policy='1')
    t.trigger('trigger_hurt', targetname='hb', size=(96, 96, 96), at=(120, 0, 0), dmg='4000', spawnflags='66',
              gib_policy='2')
    t.wait(0.3)
    t.fire('hg')
    t.fire('hb')
    t.wait(2.5)
    t.snap('s', 'zbody', 'health')
    t.expect.removed('zgib')
    t.expect.value('s', 'zbody', 'health', 0, op='<=')


@S.test('trigger_hurt "Affect non-moving NPC\'s" (64): a scientist standing in it is hurt; in a trigger without the '
        'flag one is not (a monster that does not move touches nothing)', doc=LOGIC)
def hurt_non_moving(t):
    t.point('monster_scientist', targetname='sa', at=(-120, 0, 0), angles='0 90 0')
    t.point('monster_scientist', targetname='sb', at=(120, 0, 0), angles='0 90 0')
    t.trigger('trigger_hurt', targetname='ha', size=(96, 96, 96), at=(-120, 0, 0), dmg='4', spawnflags='66')
    t.trigger('trigger_hurt', targetname='hb', size=(96, 96, 96), at=(120, 0, 0), dmg='4', spawnflags='2')
    t.wait(0.5)
    t.mark('go')
    t.fire('ha')
    t.fire('hb')
    t.wait(2.0)
    # turning a trigger on touches what stands in it once (the engine links it); after that only the flag hurts
    t.expect.damaged('sa', after='go', within=(0.4, 2.0), min=2)
    t.expect.not_damaged('sb', after='go', within=(0.4, 2.0))


@S.test('trigger_teleport "Start Inactive": nothing is teleported until it is turned on, then it teleports', players=1,
        doc=LOGIC)
def teleport_inactive(t):
    t.trigger('trigger_teleport', targetname='tp', size=(64, 64, 96), at=(0, -150, 0), target='dest',
              teleport_start_inactive='1')
    t.point('info_teleport_destination', targetname='dest', at=(150, 150, 0))
    t.teleport('player1', (0, -150, 0))
    t.wait(0.4)
    t.snap('off', 'player1', 'origin')
    t.snap('off', 'dest', 'origin')
    t.teleport('player1', (-150, 150, 0))
    t.fire('tp', use='on')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.4)
    t.snap('on', 'player1', 'origin')
    t.snap('on', 'dest', 'origin')
    t.expect.distance('off', 'player1', 'dest', 250, 500)
    t.expect.distance('on', 'player1', 'dest', 0, 20)


@S.test('trigger_changevalue "Clear bits" (15), "Min" (30), "Max" (31)', doc=LOGIC)
def changevalue_new_ops(t):
    info(t, 'bits', spawnflags='7')
    info(t, 'lo', health='50')
    info(t, 'hi', health='50')
    t.point('trigger_changevalue', targetname='clr', target='bits', m_iszValueName='spawnflags', m_iszNewValue='2',
            m_iszValueType='15')
    t.point('trigger_changevalue', targetname='mn', target='lo', m_iszValueName='health', m_iszNewValue='20',
            m_iszValueType='30')
    t.point('trigger_changevalue', targetname='mx', target='hi', m_iszValueName='health', m_iszNewValue='80',
            m_iszValueType='31')
    t.point('trigger_changevalue', targetname='mn2', target='hi', m_iszValueName='health', m_iszNewValue='99',
            m_iszValueType='30')
    for n in ('clr', 'mn', 'mx'):
        t.fire(n)
    t.wait(0.05)
    t.fire('mn2')
    t.wait(0.05)
    t.snap('s', 'bits', 'spawnflags')
    t.snap('s', 'lo', 'health')
    t.snap('s', 'hi', 'health')
    t.expect.value('s', 'bits', 'spawnflags', 5)
    t.expect.value('s', 'lo', 'health', 20)
    t.expect.value('s', 'hi', 'health', 80)


@S.test('trigger_changevalue "Treat new value as": Locus Position !activator moves a box to the player; Locus Ratio '
        'reads an env_global value; a failed Locus value changes nothing', players=1, doc=LOGIC)
def changevalue_value_type(t):
    info(t, 'box')
    info(t, 'num', health='5')
    t.point('env_global', targetname='counter', globalstate='parity_counter', triggermode='4', initialvalue='3')
    t.point('trigger_changevalue', targetname='topl', target='box', m_iszValueName='origin', m_iszNewValue='!activator',
            value_type='2')
    t.point('trigger_changevalue', targetname='rd', target='num', m_iszValueName='health', m_iszNewValue='counter',
            value_type='1', names=('target', 'm_iszNewValue'))
    t.point('trigger_changevalue', targetname='bad', target='num', m_iszValueName='health', m_iszNewValue='nosuchent',
            value_type='1')
    t.teleport('player1', (100, 120, 0))
    t.wait(0.2)
    t.fire('topl', activator='player1')
    t.fire('counter', use='on')                  # 3 + 1
    t.fire('rd')
    t.fire('bad')
    t.wait(0.05)
    t.snap('s', 'box', 'origin')
    t.snap('s', 'player1', 'origin')
    t.snap('s', 'num', 'health')
    t.expect.distance('s', 'box', 'player1', 0, 1)
    t.expect.value('s', 'num', 'health', 4)


@S.test('env_global "Modify / Set value" (4): On adds 1, Off takes 1, Set sets the fired value; read through a Locus '
        'ratio', doc=LOGIC)
def global_modify_value(t):
    info(t, 'num')
    t.point('env_global', targetname='g', globalstate='parity_value', triggermode='4', initialvalue='10')
    t.point('trigger_changevalue', targetname='rd', target='num', m_iszValueName='health', m_iszNewValue='g',
            value_type='1', names=('target', 'm_iszNewValue'))
    t.fire('g', use='on')
    t.fire('g', use='on')
    t.fire('g', use='off')
    t.fire('rd')
    t.wait(0.05)
    t.snap('a', 'num', 'health')
    t.fire('g', use='set', value=42)
    t.fire('rd')
    t.wait(0.05)
    t.snap('b', 'num', 'health')
    t.expect.value('a', 'num', 'health', 11)
    t.expect.value('b', 'num', 'health', 42)


@S.test('env_global "Set state: obey use type" (5) with Act as Master, as a game_counter\'s master: On opens it, Off '
        'closes it, Toggle flips it', doc=LOGIC)
def global_obey_use(t):
    t.point('env_global', targetname='g', globalstate='parity_obey', triggermode='5', initialstate='0', spawnflags='5')
    # a game_counter that fires at 1 and resets (2): it counts only while its master is open
    t.point('game_counter', targetname='rel', target='hit', health='1', frags='0', spawnflags='2', master='g',
            names=('master',))
    info(t, 'hit')
    t.fire('g', use='on')
    t.mark('on')
    t.fire('rel', use='on')
    t.wait(0.05)
    t.fire('g', use='off')
    t.mark('off')
    t.fire('rel', use='on')
    t.wait(0.05)
    t.fire('g', use='toggle')
    t.mark('tog')
    t.fire('rel', use='on')
    t.wait(0.05)
    t.expect.used('hit', count=2)
    t.expect.used('hit', count=1, after='on', within=(0, 0.04))
    t.expect.not_used('hit', after='off', within=(0, 0.04))
    t.expect.used('hit', count=1, after='tog')


# ---------------------------------------------------------------------- parity batch 2: effects and sound (A)
@S.test('env_explosion "Custom Fireball Sprite", "Custom Smoke Sprite", "Custom sprite scale" 2.5: the fireball and '
        'the smoke use them at scale 25', doc=LOGIC)
def explosion_custom_sprites(t):
    t.point('env_explosion', targetname='boom', at=(0, 0, 16), iMagnitude='50', spawnflags='3',
            model='sprites/fexplo.spr', smokesprite='sprites/steam1.spr', scale='2.5')
    t.wait(0.1)
    t.mark('go')
    t.fire('boom')
    t.wait(0.6)
    t.expect.msg('svc:23', fields={'te': 3, 'model': 'model:sprites/fexplo.spr', 'scale': 25}, after='go', count=1)
    t.expect.msg('svc:23', fields={'te': 5, 'model': 'model:sprites/steam1.spr', 'scale': 25}, after='go', count=1)


@S.test('env_spark "Spark Silent", "Spark Volume Factor" 2, "Sound radius" Enormous; "Cyclic" (16): one spark per '
        'trigger, a trigger during the wait sparks once at its end', doc=LOGIC)
def spark_options(t):
    t.point('env_spark', targetname='quiet', at=(-150, 0, 40), spawnflags='16', spark_silent='1', MaxDelay='0.5')
    t.point('env_spark', targetname='loud', at=(0, 0, 40), spawnflags='16', spark_volume='2', soundradius='2',
            MaxDelay='0.5')
    t.point('env_spark', targetname='cyc', at=(150, 0, 40), spawnflags='16', MaxDelay='1')
    t.wait(0.1)
    t.mark('go')
    t.fire('quiet')
    t.fire('loud')
    t.fire('cyc')
    t.wait(0.1)
    t.fire('cyc')
    t.fire('cyc')
    t.wait(1.4)
    t.expect.sound('buttons/spark', ent='quiet', count=0, min=0, after='go')
    t.expect.sound('buttons/spark', ent='loud', attn=0.25, vol=(0.2, 0.6), after='go', count=1)
    t.expect.sound('buttons/spark', ent='cyc', attn=0.8, after='go', count=2)
    for x, n in ((-150, 1), (0, 1), (150, 2)):
        t.expect.msg('svc:23', fields={'te': 9, 'pos': [float(v) for v in t._abs((x, 0, 40))]}, after='go', count=n)


@S.test('env_laser "Start Sprite": a sprite at the laser\'s start, shown with the laser', doc=LOGIC)
def laser_start_sprite(t):
    info(t, 'lend', at=(150, 0, 40))
    t.point('env_laser', targetname='las', at=(-150, 0, 40), LaserTarget='lend', texture='sprites/laserbeam.spr',
            StartSprite='sprites/flare3.spr', spawnflags='1', damage='0', names=('LaserTarget',))
    t.wait(0.5)
    t.expect.present('@env_sprite', at=(-150, 0, 40))
    t.expect.precached('sprites/flare3.spr')


@S.test('env_beam "No Camera Punch" (4096): the beam hurts player1 without kicking his view; player2 in a beam '
        'without the flag is kicked', players=2, doc=LOGIC)
def beam_no_punch(t):
    for side, x in (('a', -120), ('b', 120)):
        info(t, 's' + side, at=(x, -150, 36))
        info(t, 'e' + side, at=(x, 150, 36))
    t.point('env_beam', targetname='ba', LightningStart='sa', LightningEnd='ea', damage='20', spawnflags='4097',
            texture='sprites/laserbeam.spr', life='0', BoltWidth='10', names=('LightningStart', 'LightningEnd'))
    t.point('env_beam', targetname='bb', LightningStart='sb', LightningEnd='eb', damage='20', spawnflags='1',
            texture='sprites/laserbeam.spr', life='0', BoltWidth='10', names=('LightningStart', 'LightningEnd'))
    for p in ('player1', 'player2'):
        healthy(t, p, '1000')
    t.mark('in')
    t.teleport('player1', (-120, 0, 0))
    t.teleport('player2', (120, 0, 0))
    t.wait(0.25)
    t.snap('s', 'player1', 'punchangle')
    t.snap('s', 'player2', 'punchangle')
    t.teleport('player1', (-120, 220, 0))
    t.teleport('player2', (120, 220, 0))
    t.expect.damaged('player1', after='in')
    t.expect.damaged('player2', after='in')
    t.expect.value('s', 'player1', 'punchangle.x', 0, tol=0.01)
    t.expect.value('s', 'player2', 'punchangle.x', -0.5, op='<')


@S.test('env_shake "Position" [LP]: the shake is at the named point (player2 there is shaken), not at the entity '
        '(player1 beside it is not)', players=2, doc=LOGIC)
def shake_position(t):
    info(t, 'there', at=(180, 180, 0))
    t.point('env_shake', targetname='sh', at=(-180, -180, 16), message='there', radius='120', amplitude='8',
            duration='1', frequency='50', names=('message',))
    t.teleport('player1', (-180, -150, 0))
    t.teleport('player2', (180, 150, 0))
    t.wait(0.3)
    t.mark('go')
    t.fire('sh')
    t.wait(0.2)
    t.expect.msg('ScreenShake', to='player2', after='go', min=1)
    t.expect.no_msg('ScreenShake', to='player1', after='go')


@S.test('env_funnel "Particle sprite", "Position" [LP], "Repeatable" (2): two funnels at the point with the sprite',
        doc=LOGIC)
def funnel_options(t):
    info(t, 'fp', at=(100, 50, 64))
    t.point('env_funnel', targetname='fn', at=(-100, 0, 64), netname='sprites/flare3.spr', message='fp',
            spawnflags='2', names=('message',))
    t.wait(0.1)
    t.mark('go')
    t.fire('fn')
    t.wait(0.1)
    t.fire('fn')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': 100, 'model': 'model:sprites/flare3.spr', 'pos': [float(v) for v in t._abs((100, 50, 64))]},
                 after='go', count=2)


@S.test('env_fade "Directional blind" (8): player1 facing it gets the full fade, player2 with his back to it none',
        players=2, doc=LOGIC)
def fade_directional(t):
    t.point('env_fade', targetname='fd', at=(0, 0, 36), duration='1', holdtime='0', renderamt='200',
            rendercolor='255 255 255', spawnflags='8')
    t.teleport('player1', (-150, 0, 0), yaw=0)
    t.teleport('player2', (150, 0, 0), yaw=0)
    t.wait(0.3)
    t.mark('go')
    t.fire('fd')
    t.wait(0.1)
    t.expect.msg('ScreenFade', to='player1', fields={'a': 200}, after='go', count=1)
    t.expect.msg('ScreenFade', to='player2', fields={'a': 0}, after='go', count=1)


@S.test('env_blood "Initial position" [LP], "Direction" [LV], "Blood Color" 210 (blue): the drops are at the point '
        'in the palette color', doc=LOGIC)
def blood_options(t):
    info(t, 'bp', at=(80, 60, 50))
    t.point('env_blood', targetname='bl', at=(-80, 0, 50), target='bp', netname='0 0 1', color='210', amount='60',
            names=('target',))
    t.wait(0.1)
    t.mark('go')
    t.fire('bl')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': 115, 'color': 210, 'pos': [float(v) for v in t._abs((80, 60, 50))]},
                 after='go', count=1)


@S.test('env_bubbles "Custom Bubble Sprite": the fizz uses it', doc=LOGIC)
def bubbles_model(t):
    t.brush('env_bubbles', size=(64, 64, 64), at=(0, 0, 32), bubble_model='sprites/flare3.spr', density='2',
            frequency='10')
    t.wait(2.5)
    t.expect.msg('svc:23', fields={'te': 105, 'model': 'model:sprites/flare3.spr'}, min=1)


@S.test('env_sprite "Attached to entity" rides on its target (MOVETYPE_FOLLOW), "Start frame" 3, "Don\'t mess Yaw" '
        '(8) keeps yaw 90 (Half-Life moves it to roll)', doc=LOGIC)
def sprite_options(t):
    t.brush('func_wall', targetname='carrier', size=(32, 32, 32), at=(100, 0, 64))
    t.point('env_sprite', targetname='rider', at=(0, 0, 64), model='sprites/zerogxplode.spr', message='carrier',
            framerate='0', spawnflags='1', names=('message',))
    t.point('env_sprite', targetname='fr', at=(-100, 0, 64), model='sprites/zerogxplode.spr', impulse='3',
            framerate='0', spawnflags='1')
    t.point('env_sprite', targetname='keep', at=(-100, 100, 64), model='sprites/glow01.spr', angles='0 90 0',
            spawnflags='9')
    t.point('env_sprite', targetname='hl', at=(100, 100, 64), model='sprites/glow01.spr', angles='0 90 0',
            spawnflags='1')
    t.wait(0.3)
    t.snap('s', 'rider', 'movetype')
    t.snap('s', 'fr', 'frame')
    t.snap('s', 'keep', 'angles')
    t.snap('s', 'hl', 'angles')
    t.expect.value('s', 'rider', 'movetype', 12)
    t.expect.value('s', 'fr', 'frame', 3)
    t.expect.value('s', 'keep', 'angles.y', 90)
    t.expect.value('s', 'hl', 'angles.z', 90)


# ---------------------------------------------------------------------- parity batch 3: movers (A)
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3


@S.test('func_door "Fire On" targets: start / opening (On) / opened / stop going, closing (Off) / closed coming back, '
        'with the door\'s activator', players=1, doc=LOGIC)
def door_fire_on(t):
    names = ('fireonstart', 'fireonstop', 'fireonopening', 'fireonclosing', 'fireonopened', 'fireonclosed')
    for n in ('fstart', 'fstop', 'fopening', 'fclosing', 'fopened', 'fclosed'):
        info(t, n)
    t.brush('func_door', targetname='dr', size=(64, 16, 96), at=(0, 0, 0), angles='0 0 0', speed='400', wait='0.6',
            lip='8', fireonstart='fstart', fireonstart_triggerstate='2', fireonstop='fstop', fireonstop_triggerstate='2',
            fireonopening='fopening', fireonopening_triggerstate='1', fireonclosing='fclosing',
            fireonclosing_triggerstate='0', fireonopened='fopened', fireonopened_triggerstate='1',
            fireonclosed='fclosed', fireonclosed_triggerstate='0', names=names)
    t.wait(0.2)
    t.mark('go')
    t.fire('dr', activator='player1')
    t.wait(2.0)
    t.expect.used('fstart', use=USE_TOGGLE, count=2, after='go')
    t.expect.used('fstop', use=USE_TOGGLE, count=2, after='go')
    t.expect.used('fopening', use=USE_ON, activator='player1', count=1, after='go')
    t.expect.used('fopened', use=USE_ON, count=1, after='go')
    t.expect.used('fclosing', use=USE_OFF, count=1, after='go')
    t.expect.used('fclosed', use=USE_OFF, count=1, after='go')
    t.expect.order('fopened', 'fclosing')


@S.test('func_door custom sounds (noise1 move, noise2 stop, noise3 stop on the way back), "Sound radius" Enormous, '
        '"Return speed" 800 (closes in half the time of its speed 400)', doc=LOGIC)
def door_sounds_return(t):
    info(t, 'opened')
    info(t, 'closed')
    t.brush('func_door', targetname='dr', size=(64, 16, 96), at=(0, 0, 0), angles='0 0 0', speed='200', wait='0.2',
            lip='8', noise1='doors/doormove3.wav', noise2='doors/doorstop1.wav', noise3='doors/doorstop5.wav',
            soundradius='2', return_speed='800', fireonopened='opened', fireonclosed='closed',
            names=('fireonopened', 'fireonclosed'))
    t.wait(0.2)
    t.mark('go')
    t.fire('dr')
    t.wait(1.2)
    t.expect.sound('doors/doormove3.wav', attn=0.25, min=2, after='go')
    t.expect.sound('doors/doorstop1.wav', attn=0.25, count=1, after='go')
    t.expect.sound('doors/doorstop5.wav', attn=0.25, count=1, after='go')
    # 56 units each way: 0.28 s opening at 200, 0.07 s closing at 800 (after the 0.2 s wait)
    t.expect.used('opened', after='go', within=(0.2, 0.4))
    t.expect.used('closed', after='go', within=(0.5, 0.65))


# ---------------------------------------------------------------------------------------------- doors and buttons (batch 7)
SF_USE_ONLY = 256


def use_door(t, name, at, size=(16, 64, 32), **kv):
    """A Use Only func_door that fires `<name>_open` when it starts opening."""
    info(t, name + '_open')
    t.brush('func_door', targetname=name, size=size, at=at, angles='0 0 0', speed='100', wait='-1', lip='8',
            spawnflags=str(SF_USE_ONLY), fireonopening=name + '_open', fireonopening_triggerstate='1',
            names=('fireonopening',), **kv)


@S.test('func_door "directuse" 1 (Direct only): a low Use Only door in front of the player is not used while he looks '
        'over it (Half-Life picks it), and used once he looks down at it', players=1, doc=LOGIC)
def door_directuse_direct(t):
    use_door(t, 'dd', at=(44, 0, 0), directuse='1')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('over')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.mark('down')
    t.look('player1', 45, 0)
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('dd_open', within=(0, 0.45), after='over')
    t.expect.used('dd_open', after='down', count=1)


@S.test('func_door default "directuse": the same low door looked over is used (Half-Life\'s pick by view cone)',
        players=1, doc=LOGIC)
def door_directuse_default(t):
    use_door(t, 'dn', at=(44, 0, 0))
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('over')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.used('dn_open', after='over', count=1)


@S.test('func_door "directuse" 2 (Not through walls): a Use Only door behind a wall is not used; the same door without '
        'the key is used through the wall (Half-Life)', players=1, doc=LOGIC)
def door_directuse_visible(t):
    t.brush('func_wall', size=(4, 160, 128), at=(26, 0, 0))
    use_door(t, 'dv', at=(44, 30, 0), size=(16, 40, 64), directuse='2')
    use_door(t, 'dw', at=(44, -30, 0), size=(16, 40, 64))
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('dv_open', after='use')
    t.expect.used('dw_open', after='use', count=1)


@S.test('func_button "directuse" 1: a low button looked over is not pressed; looked at, it is', players=1, doc=LOGIC)
def button_directuse(t):
    info(t, 'bd_pressed')
    t.brush('func_button', targetname='bd', size=(16, 64, 32), at=(44, 0, 0), target='bd_pressed', spawnflags='1',
            wait='0.2', directuse='1')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('over')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.mark('down')
    t.look('player1', 45, 0)
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('bd_pressed', within=(0, 0.45), after='over')
    t.expect.used('bd_pressed', after='down', min=1)


@S.test('func_door "locked_play_on_use" 1: triggered while its master is off it plays its locked sound (button2); '
        'without the key it is silent', players=1, doc=LOGIC)
def door_locked_play_on_use(t):
    t.point('multisource', targetname='lockms')
    t.point('trigger_relay', targetname='lockin', target='lockms')     # a source never fired: locked (an empty multisource is open)
    t.brush('func_door', targetname='dlk', size=(64, 16, 96), at=(-100, 0, 0), angles='0 0 0', master='lockms',
            locked_sound='2', locked_play_on_use='1')
    t.brush('func_door', targetname='dlq', size=(64, 16, 96), at=(100, 0, 0), angles='0 0 0', master='lockms',
            locked_sound='3')
    t.wait(0.3)
    t.mark('go')
    t.fire('dlk', activator='player1')
    t.fire('dlq', activator='player1')
    t.wait(0.5)
    t.expect.sound('buttons/button2.wav', ent='dlk', count=1, after='go')
    t.expect.sound('buttons/button3.wav', count=0, min=0, after='go')


def door_lane(t, name, y, **kv):
    """A door moving 160 units +x down a lane at y, a dead grunt in its way 60 units ahead."""
    info(t, name + '_opened')
    t.brush('func_door', targetname=name, size=(16, 48, 72), at=(-120, y, 0), angles='0 0 0', speed='200', wait='-1',
            lip='-144', fireonopened=name + '_opened', fireonopened_triggerstate='1', names=('fireonopened',), **kv)
    t.point('monster_hgrunt_dead', targetname=name + '_c', at=(-60, y, 0), angles='0 0 0')


@S.test('func_door "ignore_corpses" 1: the door moves through a corpse (it stays put); a stock door pushes it along',
        players=1, doc=LOGIC)
def door_ignore_corpses(t):
    door_lane(t, 'dic', -60, ignore_corpses='1')
    door_lane(t, 'dpc', 60)
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.5)
    t.snap('a', 'dic_c', 'origin')
    t.snap('a', 'dpc_c', 'origin')
    t.fire('dic')
    t.fire('dpc')
    t.wait(1.5)
    t.snap('b', 'dic_c', 'origin')
    t.snap('b', 'dpc_c', 'origin')
    t.expect.moved('a', 'b', 'dic_c', delta=(0, 0, 0), tol=1.0)
    t.expect.moved('a', 'b', 'dpc_c', min_dist=20)
    t.expect.used('dic_opened', count=1)


@S.test('func_door "instant_gib_corpses" 1: a corpse pinned against a wall is gibbed at once and the door goes on to '
        'its end; with "blocker_recheck" 1 and Damage, a monster it kills does not turn it back either', players=1,
        doc=LOGIC)
def door_instant_gib_and_recheck(t):
    t.brush('func_wall', size=(16, 300, 128), at=(0, 0, 0))     # the corpses and the monster are pinned against it
    info(t, 'dg_opened')
    t.brush('func_door', targetname='dg', size=(16, 48, 72), at=(-120, -60, 0), angles='0 0 0', speed='200',
            wait='-1', lip='-84', instant_gib_corpses='1', fireonopened='dg_opened', fireonopened_triggerstate='1',
            names=('fireonopened',))
    t.point('monster_hgrunt_dead', targetname='dg_c', at=(-30, -60, 0), angles='0 0 0')
    info(t, 'dr_opened')
    info(t, 'dr_closing')
    t.brush('func_door', targetname='drc', size=(16, 48, 72), at=(-120, 60, 0), angles='0 0 0', speed='200',
            wait='5', lip='-84', dmg='500', blocker_recheck='1', fireonopened='dr_opened',
            fireonopened_triggerstate='1', fireonclosing='dr_closing', fireonclosing_triggerstate='1',
            names=('fireonopened', 'fireonclosing'))
    t.point('monster_cyborg', targetname='dr_m', at=(-30, 60, 0), angles='0 180 0')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (150, 0, 0), yaw=180)
    t.wait(0.5)
    t.kv('dr_m', 'health', '10')
    t.fire('dg')
    t.fire('drc')
    t.wait(2.0)
    t.expect.removed('dg_c')
    t.expect.used('dg_opened', count=1)
    t.expect.used('dr_opened', count=1)
    t.expect.not_used('dr_closing')


@S.test('func_recharge "CustomJuice" 5: a player draining it gets 5 armor; a stock charger gives more', players=1,
        doc=LOGIC)
def recharge_custom_juice(t):
    charger(t, 'func_recharge', CustomJuice='5')
    t.kv('player1', 'armorvalue', '0')
    t.wait(0.2)
    t.walk('player1', 0, 0, 4.0, '+use')
    t.wait(0.3)
    t.snap('e', 'player1', 'armorvalue')
    t.expect.value('e', 'player1', 'armorvalue', 5)


@S.test('monster_*_dead "effects" 16 (light from the ceiling) is kept; other effects bits are cleared as in Half-Life',
        players=1, doc=LOGIC)
def dead_monster_effects(t):
    t.point('monster_hgrunt_dead', targetname='de16', at=(-60, 0, 0), effects='16')
    t.point('monster_scientist_dead', targetname='de8', at=(60, 0, 0), effects='24')
    t.point('monster_barney_dead', targetname='de0', at=(0, 80, 0))
    t.wait(0.3)
    for n in ('de16', 'de8', 'de0'):
        t.snap('s', n, 'effects')
    t.expect.value('s', 'de16', 'effects', 16)
    t.expect.value('s', 'de8', 'effects', 16)
    t.expect.value('s', 'de0', 'effects', 0)


@S.test('func_wall flag 2 "Use Angles": it keeps the map\'s angles (0 90 0); without the flag they are 0', players=1,
        doc=LOGIC)
def wall_use_angles(t):
    t.brush('func_wall', targetname='wua', size=(16, 64, 64), at=(-60, 0, 0), angles='0 90 0', spawnflags='2')
    t.brush('func_wall', targetname='wno', size=(16, 64, 64), at=(60, 0, 0), angles='0 90 0')
    t.brush('func_wall_toggle', targetname='wtg', size=(16, 64, 64), at=(0, 80, 0), angles='0 90 0', spawnflags='2')
    t.wait(0.3)
    for n in ('wua', 'wno', 'wtg'):
        t.snap('s', n, 'angles')
    t.expect.value('s', 'wua', 'angles.y', 90, tol=0.5)
    t.expect.value('s', 'wtg', 'angles.y', 90, tol=0.5)
    t.expect.value('s', 'wno', 'angles.y', 0, tol=0.5)


@S.test('momentary_door "stopsnd" 2, Fire On start / opening / opened / closing / closed / stop, "Sound radius" Enormous: '
        'driven by Set uses (a momentary button\'s), the activator is the one who moved it', players=1, doc=LOGIC)
def momentary_door_options(t):
    names = ('fireonstart', 'fireonstop', 'fireonopening', 'fireonclosing', 'fireonopened', 'fireonclosed')
    for n in ('mstart', 'mstop', 'mopening', 'mclosing', 'mopened', 'mclosed'):
        info(t, n)
    t.brush('momentary_door', targetname='md', size=(64, 16, 96), at=(0, 0, 0), angles='0 0 0', lip='8', stopsnd='2',
            movesnd='3', soundradius='2', fireonstart='mstart', fireonstart_triggerstate='2', fireonstop='mstop',
            fireonstop_triggerstate='2', fireonopening='mopening', fireonopening_triggerstate='1',
            fireonclosing='mclosing', fireonclosing_triggerstate='0', fireonopened='mopened',
            fireonopened_triggerstate='1', fireonclosed='mclosed', fireonclosed_triggerstate='0', names=names)
    t.wait(0.2)
    t.mark('open')
    t.fire('md', activator='player1', use='set', value=1)
    t.wait(0.6)
    t.mark('close')
    t.fire('md', activator='player1', use='set', value=0)
    t.wait(0.6)
    t.expect.used('mstart', use=USE_TOGGLE, count=2)
    t.expect.used('mopening', use=USE_ON, activator='player1', count=1, after='open', within=(0, 0.05))
    t.expect.used('mopened', use=USE_ON, count=1, after='open', within=(0, 0.5))
    t.expect.used('mclosing', use=USE_OFF, count=1, after='close', within=(0, 0.05))
    t.expect.used('mclosed', use=USE_OFF, count=1, after='close')
    t.expect.used('mstop', use=USE_TOGGLE, count=2)
    t.expect.precached('doors/doorstop2.wav', kind='sound')
    t.expect.sound('doors/doorstop2.wav', attn=0.25, count=2)
    t.expect.sound('doors/doormove3.wav', attn=0.25, min=2)


@S.test('momentary_door without "stopsnd": no stop sound (as in 1.38)', doc=LOGIC)
def momentary_door_no_stopsnd(t):
    t.brush('momentary_door', targetname='md', size=(64, 16, 96), at=(0, 0, 0), angles='0 0 0', lip='8', movesnd='3')
    t.wait(0.2)
    t.fire('md', use='set', value=1)
    t.wait(0.6)
    t.expect.sound('doors/doorstop', count=0, min=0)
    t.expect.sound('doors/doormove3.wav', attn=0.8, min=1)


WEATHER = 'WEATHER.md#rain-and-snow-volumes'


@S.test('func_rainvolume: switched on, every client gets its volume (rain, brush flag, Featureful flags, intensity, '
        'period, drop size); switched off, a removal', players=1, doc=WEATHER)
def rain_volume_use(t):
    t.brush('func_rainvolume', targetname='rv', size=(256, 256, 128), at=(0, 0, 0), spawnflags=str(2 | 64),
            intensity='300', update_period='0.2', raindrop_width='3', raindrop_height='60', raindrop_min_speed='400',
            raindrop_max_speed='900', raindrop_life='0.8')
    t.wait(0.2)
    t.mark('on')
    t.fire('rv', use='on')
    t.wait(0.1)
    t.mark('off')
    t.fire('rv', use='off')
    t.wait(0.1)
    t.expect.msg('WeatherVol', after='on', within=(0, 0.05), count=1,
                 fields={'type': 1, 'flags': 4096 | 1 | 2 | 64, 'intensity': 300, 'period': 0.2, 'size1': 3,
                         'size2': 60, 'speed1': 400, 'speed2': 900, 'life': 0.8})
    t.expect.msg('WeatherVol', after='off', count=1, fields={'type': 0})


@S.test('func_snowvolume without a name: on from the start; defaults 150 per 0.5 s; a named one with Start Active '
        'toggles off and on again with its keys', players=1, doc=WEATHER)
def snow_volume_keys(t):
    t.brush('func_snowvolume', targetname='sv', size=(256, 256, 128), at=(0, 0, 0), spawnflags='1',
            snowflake_min_size='3', snowflake_max_size='4.5', snowflake_min_speed='60', snowflake_max_speed='90',
            snowflake_life='5')
    t.brush('func_snowvolume', size=(128, 128, 128), at=(200, 0, 0))
    t.wait(0.2)
    t.snap('s', 'sv', 'spawnflags', 'solid', 'effects')
    t.snap('s', '@func_snowvolume', 'spawnflags')
    t.mark('off')
    t.fire('sv')
    t.wait(0.1)
    t.mark('on')
    t.fire('sv')
    t.wait(0.1)
    t.expect.value('s', 'sv', 'spawnflags', 1, op='&')
    t.expect.value('s', 'sv', 'solid', 0)
    t.expect.value('s', 'sv', 'effects', 128, op='&')
    t.expect.value('s', '@func_snowvolume', 'spawnflags', 1, op='&')
    t.expect.msg('WeatherVol', after='off', within=(0, 0.05), count=1, fields={'type': 0})
    t.expect.msg('WeatherVol', after='on', count=1,
                 fields={'type': 2, 'flags': 4096 | 1, 'intensity': 150, 'period': 0.5, 'size1': 30, 'size2': 45,
                         'speed1': 60, 'speed2': 90, 'life': 5.0})


FOLLOW = 'FEATUREFUL_OPTIONS.md#following'


def use_on(t, name, at, label):
    """player1's +use on the monster (its Use with the player as caller, as PlayerUse calls it); snapshot after.
    Direct: a barney turned friendly walks up to greet, so a +use aimed at its spawn spot could miss it."""
    t.fire(name, activator='player1', caller='player1')
    t.wait(0.5)
    t.snap(label, name, 'follow')


@S.test('Follow rule: a barney does not follow +use (free, Prisoner, never having seen the player); a scientist does; '
        'a barney with is_player_ally 1 and followage_policy 0 does', players=1, doc=FOLLOW)
def follow_rule(t):
    t.flags('player1', '+FL_NOTARGET')     # no fight (SBO's barneys hate players): only the +use rule is tested
    spots = {'bfree': (-200, 150, 0), 'bpris': (-100, 150, 0), 'bblind': (0, 150, 0), 'sci': (100, 150, 0),
             'bally': (200, 150, 0)}
    t.point('monster_barney', targetname='bfree', at=spots['bfree'], angles='0 270 0')
    t.point('monster_barney', targetname='bpris', at=spots['bpris'], angles='0 270 0', spawnflags='16')
    t.point('monster_barney', targetname='bblind', at=spots['bblind'], angles='0 90 0')
    t.point('monster_scientist', targetname='sci', at=spots['sci'], angles='0 270 0')
    t.point('monster_barney', targetname='bally', at=spots['bally'], angles='0 270 0', is_player_ally='1',
            followage_policy='0')
    t.wait(0.5)
    for n in ('bfree', 'bpris', 'bblind', 'bally', 'sci'):     # the scientist last: once he follows he walks into the way
        use_on(t, n, spots[n], 'u_' + n)
    t.expect.value('u_bfree', 'bfree', 'follow', 0)
    t.expect.value('u_bpris', 'bpris', 'follow', 0)
    t.expect.value('u_bblind', 'bblind', 'follow', 0)
    t.expect.value('u_sci', 'sci', 'follow', 1)
    t.expect.value('u_bally', 'bally', 'follow', 1)


@S.test('Follow rule: classify 11 counts as a player ally; a player ally without followage_policy does not follow; '
        'a scientist with followage_policy 1 (scripted only) does not follow +use', players=1, doc=FOLLOW)
def follow_rule_keys(t):
    t.flags('player1', '+FL_NOTARGET')     # no fight (SBO's barneys hate players): only the +use rule is tested
    spots = {'bcls': (-150, 150, 0), 'bnopol': (0, 150, 0), 'sscr': (150, 150, 0)}
    t.point('monster_barney', targetname='bcls', at=spots['bcls'], angles='0 270 0', classify='11',
            followage_policy='0')
    t.point('monster_barney', targetname='bnopol', at=spots['bnopol'], angles='0 270 0', is_player_ally='1')
    t.point('monster_scientist', targetname='sscr', at=spots['sscr'], angles='0 270 0', followage_policy='1')
    t.wait(0.5)
    for n in ('bcls', 'bnopol', 'sscr'):
        use_on(t, n, spots[n], 'u_' + n)
    t.expect.value('u_bcls', 'bcls', 'follow', 1)
    t.expect.value('u_bnopol', 'bnopol', 'follow', 0)
    t.expect.value('u_sscr', 'sscr', 'follow', 0)


MONSTER = 'FEATUREFUL_OPTIONS.md#items-monsters-and-scripts'


@S.test('monster "gib_policy" Prefer not to gib: a zombie killed by a huge hit keeps its body; Default gibs it',
        doc=MONSTER)
def monster_gib_policy(t):
    t.point('monster_zombie', targetname='zn', at=(-100, 0, 0), spawnflags='16', gib_policy='2')
    t.point('monster_zombie', targetname='zd', at=(100, 0, 0), spawnflags='16')
    t.wait(0.3)
    t.damage('zn', 5000)
    t.damage('zd', 5000)
    t.wait(1.0)
    t.snap('s', 'zn', 'deadflag')
    t.expect.value('s', 'zn', 'deadflag', 0, op='>')
    t.expect.removed('zd')


@S.test('monster "gib_policy" Prefer gibbing: a zombie killed by a hit that leaves it above -40 health is gibbed; '
        'Default keeps the body', doc=MONSTER)
def monster_gib_policy_small(t):
    t.point('monster_zombie', targetname='zg', at=(-100, 0, 0), spawnflags='16', gib_policy='1')
    t.point('monster_zombie', targetname='zd', at=(100, 0, 0), spawnflags='16')
    t.wait(0.3)
    for n in ('zg', 'zd'):
        t.kv(n, 'health', '50')
        t.damage(n, 60)        # -10: above GIB_HEALTH_VALUE (-30), no gib by default
    t.wait(1.0)
    t.snap('s', 'zd', 'deadflag')
    t.expect.removed('zg')
    t.expect.value('s', 'zd', 'deadflag', 0, op='>')


@S.test('monster "field_of_view" and "soundmask": replace the monster\'s own (deaf: none; 1028: its own without '
        'Player)', doc=MONSTER)
def monster_fov_soundmask(t):
    t.point('monster_scientist', targetname='s0', at=(-150, 0, 0))
    t.point('monster_scientist', targetname='s1', at=(0, 0, 0), field_of_view='0.9', soundmask='-1')
    t.point('monster_scientist', targetname='s2', at=(150, 0, 0), soundmask='1028')
    t.wait(0.3)
    for n in ('s0', 's1', 's2'):
        t.snap('s', n, 'fov', 'soundmask')
    t.expect.value('s', 's1', 'fov', 0.9, tol=0.001)
    t.expect.value('s', 's1', 'soundmask', 0)
    t.expect.value('s', 's2', 'soundmask', 4, op='!&')
    t.expect.value('s', 's0', 'soundmask', 4, op='&')
    t.expect.value('s', 's0', 'fov', 0.9, op='<')


@S.test('monster "TriggerAltCondition": 50% Health as the second condition fires TriggerTarget once; 13 Got healed '
        'fires when healed; 12 Provoked fires when a player shoots a barney facing it', players=1, doc=MONSTER)
def monster_alt_conditions(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sa', at=(-150, 100, 0), TriggerTarget='alt', TriggerCondition='9',
            TriggerAltCondition='3', names=('TriggerTarget',))
    t.point('monster_scientist', targetname='sh', at=(0, 100, 0), TriggerTarget='healed', TriggerCondition='4',
            TriggerAltCondition='13',
            names=('TriggerTarget',))
    t.point('monster_barney', targetname='bp', at=(150, 100, 0), angles='0 270 0', TriggerTarget='provoked',
            TriggerAltCondition='12', names=('TriggerTarget',))
    t.point('trigger_hurt_remote', targetname='heal', target='sh', dmg='-10', names=('target',))
    for n in ('alt', 'healed', 'provoked'):
        info(t, n)
    t.wait(0.5)
    t.kv('sh', 'health', '5')
    t.kv('sa', 'health', '10')
    t.kv('sa', 'max_health', '10')
    t.damage('sa', 6)
    t.wait(0.5)
    t.damage('sa', 1)
    t.fire('heal')
    t.teleport('player1', (150, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.damage('bp', 2, attacker='player1')
    t.wait(1.0)
    t.expect.used('alt', count=1)
    t.expect.used('healed', count=1)
    t.expect.used('provoked', count=1)


@S.test('monster "classify" / "is_player_ally": a barney hates players; with is_player_ally it is their ally; classify '
        '-1 (none) ignores them; a scientist with is_player_ally becomes military (hates players)', players=1, doc=MONSTER)
def monster_classify(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='b0', at=(-150, 0, 0))
    t.point('monster_barney', targetname='b1', at=(-50, 0, 0), is_player_ally='1')
    t.point('monster_barney', targetname='b2', at=(50, 0, 0), classify='-1')
    t.point('monster_scientist', targetname='s1', at=(150, 0, 0), is_player_ally='1')
    t.wait(0.3)
    for n in ('b0', 'b1', 'b2', 's1'):
        t.snap('r', n, 'rel')
    t.expect.value('r', 'b0', 'rel', 2)        # R_HT
    t.expect.value('r', 'b1', 'rel', -2)       # R_AL
    t.expect.value('r', 'b2', 'rel', 0)        # R_NO
    t.expect.value('r', 's1', 'rel', 2)        # military: R_HT


@S.test('scientist "voicepitch" 150: his scripted sentence plays at pitch 150-153 (Half-Life: the head\'s pitch)', doc=MONSTER)
def talk_voicepitch(t):
    t.point('monster_scientist', targetname='sci', at=(0, 0, 0), voicepitch='150')
    t.point('scripted_sentence', targetname='say', at=(0, 0, 0), entity='sci', sentence='!SC_OK0', radius='512',
            duration='2', refire='1', spawnflags='4', names=('entity',))      # 4: interrupt (speaks whatever he does)
    t.wait(0.5)
    t.mark('go')
    t.fire('say')
    t.wait(4.0)        # scripted_sentence looks for its speaker every 0.5 s and waits for the talk pause
    t.expect.sound('!', ent='sci', pitch=(150, 153), min=1, after='go')     # a sentence (sent by its number)


@S.test('func_plat "Fire On" opening / opened / closing / closed, custom move and stop sounds, "Sound radius" Small',
        doc=LOGIC)
def plat_fire_on(t):
    for n in ('popening', 'popened', 'pclosing', 'pclosed'):
        info(t, n)
    t.brush('func_plat', targetname='pl', size=(64, 64, 16), at=(0, 0, 80), height='64', speed='400', spawnflags='1',
            custommovesnd='plats/elevmove2.wav', customstopsnd='plats/bigstop1.wav', soundradius='-2',
            fireonopening='popening', fireonopened='popened', fireonclosing='pclosing', fireonclosed='pclosed',
            fireonopening_triggerstate='2', fireonopened_triggerstate='2', fireonclosing_triggerstate='2',
            fireonclosed_triggerstate='2', names=('fireonopening', 'fireonopened', 'fireonclosing', 'fireonclosed'))
    t.wait(0.3)
    t.mark('go')
    t.fire('pl')
    t.wait(0.6)
    t.fire('pl')
    t.wait(0.6)
    t.expect.used('pclosing', count=1, after='go')
    t.expect.used('pclosed', count=1, after='go')
    t.expect.used('popening', count=1, after='go')
    t.expect.used('popened', count=1, after='go')
    t.expect.sound('plats/elevmove2.wav', attn=2.0, min=1, after='go')
    t.expect.sound('plats/bigstop1.wav', attn=2.0, min=1, after='go')


@S.test('func_train "No default damage" (64): Damage 0 stays 0 (Half-Life: 2); "Initially On" (4): a named train '
        'moves without a trigger; "soundpitch" 150 on its move sound', doc=LOGIC)
def train_flags(t):
    t.point('path_corner', targetname='ta', at=(-150, -150, 32), target='tb')
    t.point('path_corner', targetname='tb', at=(150, -150, 32), target='ta')
    t.point('path_corner', targetname='ua', at=(-150, 150, 32), target='ub')
    t.point('path_corner', targetname='ub', at=(150, 150, 32), target='ua')
    t.brush('func_train', targetname='tr', size=(32, 32, 32), at=(-150, -150, 32), target='ta', speed='600',
            spawnflags='68', dmg='0', movesnd='1', soundpitch='150')
    t.brush('func_train', targetname='tc', size=(32, 32, 32), at=(-150, 150, 32), target='ua', speed='600', dmg='0',
            movesnd='1')
    t.wait(0.2)
    t.snap('a', 'tr', 'origin', 'dmg')
    t.snap('a', 'tc', 'origin', 'dmg')
    t.wait(1.0)
    t.snap('b', 'tr', 'origin')
    t.snap('b', 'tc', 'origin')
    t.expect.value('a', 'tr', 'dmg', 0)
    t.expect.value('a', 'tc', 'dmg', 2)
    t.expect.moved('a', 'b', 'tc', (0, 0, 0), tol=1)
    # moving: the move sound at each leg (0.5 s each), at pitch 150; the train without the flag waits
    t.expect.sound('plats/bigmove1.wav', ent='tr', pitch=150, min=2)
    t.expect.sound('plats/bigmove1.wav', ent='tc', count=0, min=0)


@S.test('func_tracktrain custom move and brake sounds, "Sound radius" Huge', doc=LOGIC)
def tracktrain_sounds(t):
    t.point('path_track', targetname='p1', target='p2', at=(0, -180, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 180, 0))
    t.brush('func_tracktrain', size=(64, 64, 16), at=(0, -180, 0), origin=True, targetname='tt', target='p1',
            speed='200', height='8', wheels='32', startspeed='150', custommovesnd='plats/elevmove3.wav',
            customstopsnd='plats/bigstop2.wav', soundradius='1', sounds='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('tt', use='on')
    t.wait(1.0)
    t.fire('tt', use='off')
    t.wait(0.3)
    t.expect.sound('plats/elevmove3.wav', attn=0.5, min=1, after='go')
    t.expect.sound('plats/bigstop2.wav', attn=0.5, min=1, after='go')


@S.test('func_conveyor "Use Set Policy": Set 1 sets the speed to the fired value, 2 adds it; 0 reverses (Half-Life)',
        doc=LOGIC)
def conveyor_useset(t):
    t.brush('func_conveyor', targetname='cs', size=(64, 64, 16), at=(-150, 0, 0), speed='100', useset_policy='1')
    t.brush('func_conveyor', targetname='ca', size=(64, 64, 16), at=(0, 0, 0), speed='100', useset_policy='2')
    t.brush('func_conveyor', targetname='cr', size=(64, 64, 16), at=(150, 0, 0), speed='100')
    t.wait(0.1)
    for n in ('cs', 'ca', 'cr'):
        t.fire(n, use='set', value=40)
    t.wait(0.05)
    for n in ('cs', 'ca', 'cr'):
        t.snap('s', n, 'speed')
    t.expect.value('s', 'cs', 'speed', 40)
    t.expect.value('s', 'ca', 'speed', 140)
    t.expect.value('s', 'cr', 'speed', -100)


@S.test('func_button "trigger_before_move", "trigger_on_return", "usetype" On: its target gets On; locked (master closed): '
        '"target_on_locked" fires (at most every 2 s) and nothing else', players=1, doc=LOGIC)
def button_options(t):
    for n in ('bmove', 'bret', 'btgt', 'block', 'b2tgt'):
        info(t, n)
    t.point('env_global', targetname='closed', globalstate='parity_closed', initialstate='0', spawnflags='5')
    t.brush('func_button', targetname='bt', size=(16, 32, 32), at=(-100, 0, 40), target='btgt', wait='0.3',
            speed='400', trigger_before_move='bmove', trigger_on_return='bret', usetype='1',
            names=('trigger_before_move', 'trigger_on_return'))
    t.brush('func_button', targetname='bl', size=(16, 32, 32), at=(100, 0, 40), target='b2tgt', wait='0.3',
            master='closed', target_on_locked='block', names=('target_on_locked', 'master'))
    t.wait(0.2)
    t.mark('go')
    t.fire('bt', activator='player1')
    t.fire('bl', activator='player1')
    t.wait(0.1)
    t.fire('bl', activator='player1')
    t.wait(1.0)
    t.expect.used('bmove', count=1, after='go')
    t.expect.used('btgt', use=USE_ON, count=1, after='go')
    t.expect.used('bret', count=1, after='go')
    t.expect.order('bmove', 'bret')
    t.expect.used('block', count=1, after='go')
    t.expect.used('b2tgt', count=0, min=0, after='go')


@S.test('func_button toggle "usetype" On/Off (2): On going in, Off coming back; "wait_toggle" 1: a second use within '
        '1 s does nothing', players=1, doc=LOGIC)
def button_toggle_options(t):
    info(t, 'ttgt')
    t.brush('func_button', targetname='bt', size=(16, 32, 32), at=(0, 0, 40), target='ttgt', speed='400',
            spawnflags='32', usetype='2', wait_toggle='1')
    t.wait(0.2)
    t.mark('in')
    t.fire('bt', activator='player1')
    t.wait(0.3)
    t.fire('bt', activator='player1')        # within wait_toggle: ignored
    t.wait(1.0)
    t.mark('out')
    t.fire('bt', activator='player1')
    t.wait(0.3)
    t.expect.used('ttgt', use=USE_ON, count=1, after='in', within=(0, 0.3))
    t.expect.used('ttgt', count=0, min=0, after='in', within=(0.3, 1.2))
    t.expect.used('ttgt', use=USE_OFF, count=1, after='out')


@S.test('func_button "Player Can\'t Use" (16384): the player\'s +use does not press it; the same button without the flag '
        'is pressed', players=1, doc=LOGIC)
def button_player_cant_use(t):
    info(t, 'pa')
    info(t, 'pb')
    t.brush('func_button', targetname='ba', size=(48, 8, 48), at=(-80, 100, 40), target='pa', spawnflags='16384',
            wait='0.3', speed='400')
    t.brush('func_button', targetname='bb', size=(48, 8, 48), at=(80, 100, 40), target='pb', wait='0.3', speed='400')
    for x, n in ((-80, 'a'), (80, 'b')):
        t.teleport('player1', (x, 70, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait(0.3)
        t.mark('use' + n)
        t._step('test_move', t.ref('player1'), 0, 0, 3, '+use')
        t.wait(0.5)
    t.expect.used('pa', count=0, min=0, after='usea')
    t.expect.used('pb', count=1, after='useb')


@S.test('func_breakable "Custom number of gibs" 3, smoke trails (8192) and transparent gibs (16384), "Target\'s '
        'Activator" 1 (the breakable itself)', doc=LOGIC)
def breakable_gibs(t):
    info(t, 'broke')
    t.brush('func_breakable', targetname='cr', size=(48, 48, 48), at=(0, 0, 24), material='1', health='10',
            m_iGibs='3', spawnflags=str(8192 | 16384), target='broke', target_activator='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('cr')
    t.wait(0.3)
    t.expect.msg('svc:23', fields={'te': 108, 'count': 3, 'flags': 0x08 | 0x10 | 0x20}, after='go', count=1)
    t.expect.used('broke', activator='@func_breakable', count=1, after='go')   # its name is cleared as it breaks


@S.test('func_breakable "Not solid" (4096): not solid, still breaks when triggered; "switch_texture_when_damaged": frame '
        '1 at half health', doc=LOGIC)
def breakable_notsolid_texture(t):
    t.brush('func_breakable', targetname='ns', size=(48, 48, 48), at=(-100, 0, 24), material='1', health='10',
            spawnflags='4096')
    t.brush('func_breakable', targetname='tx', size=(48, 48, 48), at=(100, 0, 24), material='2', health='100',
            switch_texture_when_damaged='1')
    t.wait(0.2)
    t.snap('a', 'ns', 'solid')
    t.damage('tx', 30)
    t.wait(0.05)
    t.snap('b', 'tx', 'frame')
    t.damage('tx', 30)
    t.wait(0.05)
    t.snap('c', 'tx', 'frame')
    t.mark('go')
    t.fire('ns')
    t.wait(0.3)
    t.expect.value('a', 'ns', 'solid', 0)
    t.expect.value('b', 'tx', 'frame', 0)
    t.expect.value('c', 'tx', 'frame', 1)
    t.expect.msg('svc:23', fields={'te': 108}, after='go', count=1)


@S.test('func_breakable "whenhit": a laser hitting the crate fires it with a Locus point at the hit; "spark_when_hit" '
        'makes a wood crate spark', doc=LOGIC)
def breakable_whenhit(t):
    info(t, 'lend', at=(150, 0, 40))
    t.point('env_laser', targetname='las', at=(-150, 0, 40), LaserTarget='lend', texture='sprites/laserbeam.spr',
            damage='1', names=('LaserTarget',))
    t.brush('func_breakable', targetname='cr', size=(32, 64, 64), at=(60, 0, 8), material='1', health='100000',
            whenhit='hit', spark_when_hit='1', names=('whenhit',))
    info(t, 'hit')
    t.wait(0.2)
    t.mark('go')
    t.fire('las', use='on')
    t.wait(1.5)
    t.expect.used('hit', activator='@info_target', min=3, after='go')
    t.expect.msg('svc:23', fields={'te': 9}, min=1, after='go')


@S.test('func_tank "Trigger on firing (locus = barrel)": each shot fires it with a Locus point at the barrel\'s end',
        players=1, doc=LOGIC)
def tank_locus_fire(t):
    info(t, 'shot')
    t.point('monster_target', targetname='tgt', at=(0, 150, 32), frags='11')
    t.brush('func_tank', targetname='tk', size=(24, 24, 24), at=(0, -150, 32), origin=True, spawnflags='1',
            yawrate='300', yawrange='180', yawtolerance='15', pitchrate='300', pitchrange='60', pitchtolerance='15',
            firerate='5', bullet='1', barrel='16', persistence='1', m_iszLocusFire='shot', angles='0 90 0',
            names=('m_iszLocusFire',))
    t.teleport('player1', (-200, -200, 0))
    t.wait(3.0)
    t.expect.used('shot', activator='@info_target', min=2)


@S.test('trigger_camera "Alive player only" (1024): ends at once when its player dies; never starts for a dead one',
        players=2, doc=LOGIC)
def cam_alive_only(t):
    info(t, 'looka', at=(200, 200, 40))
    info(t, 'lookb', at=(200, 200, 40))
    t.point('trigger_camera', targetname='cama', at=(-150, -150, 100), target='looka', wait='10', spawnflags='1024')
    t.point('trigger_camera', targetname='camb', at=(-150, -150, 100), target='lookb', wait='10', spawnflags='1024')
    healthy(t, 'player1', '1000')
    t.fire('cama', activator='player2')
    t.wait(0.5)
    t.mark('kill')
    t.damage('player2', 10000)
    t.wait(0.5)
    t.mark('dead')
    t.fire('camb', activator='player2')
    t.wait(0.5)
    t.expect.setview('player2', 'cama', count=1)
    t.expect.used('looka', caller='cama', after='kill', within=(0, 0.3), count=1)
    t.expect.not_used('lookb')
    t.expect.setview('player2', 'camb', count=0)


# ---------------------------------------------------------------------- parity batch 4: items, chargers, monsters (A)
@S.test('item_battery "USE only" (256): touching it gives nothing, +use picks it up; with "TOUCH only" (128) as well '
        'touch picks it up', players=1, doc=LOGIC)
def item_use_only(t):
    t.point('item_battery', targetname='bat', at=(0, 0, 0), spawnflags='256')
    t.point('item_battery', targetname='bat2', at=(170, 170, 0), spawnflags='384')
    t.kv('player1', 'armorvalue', '0')
    t.mark('touch')
    t.teleport('player1', (0, 0, 0))
    t.wait(0.5)
    t.teleport('player1', (-48, 0, 0), yaw=0)
    t.look('player1', 45, 0)
    t.wait(0.1)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    # the +use pickup and its sound come in the frames after the key press: a loaded server takes longer, and a
    # sound still due then counted as the TOUCH + USE battery's (FLAKY 2026-11-02); 'both' starts once it is over
    t.wait(0.6)
    t.mark('both')
    t.teleport('player1', (170, 170, 0))
    t.wait(0.5)
    # the pickup sound (items.cpp CItemBattery::MyTouch) on each pickup
    t.expect.sound('items/gunpickup2.wav', count=0, min=0, after='touch', within=(0, 0.5))
    t.expect.used('bat', caller='player1', count=1, after='use', within=(0, 0.3))
    t.expect.sound('items/gunpickup2.wav', count=1, after='use', within=(0, 0.3))
    t.expect.sound('items/gunpickup2.wav', count=1, after='both')


def charger(t, classname, **kv):
    t.brush(classname, targetname='chg', size=(64, 8, 48), at=(0, 60, 24), **kv)
    for k in ('TriggerOnFirstUse', 'TriggerOnEmpty', 'TriggerOnRecharged'):
        if k in kv:
            info(t, kv[k])
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)


@S.test('func_recharge "Trigger On First Use", "Trigger On Empty", custom start, loop and denied sounds: a player '
        'drains it with +use', players=1, doc=LOGIC)
def recharge_options(t):
    charger(t, 'func_recharge', TriggerOnFirstUse='first', TriggerOnEmpty='empty',
            CustomStartSound='buttons/blip1.wav', CustomLoopSound='buttons/blip2.wav',
            CustomDeniedSound='buttons/button10.wav',
            names=('TriggerOnFirstUse', 'TriggerOnEmpty'))
    t.kv('player1', 'armorvalue', '0')
    t.wait(0.2)
    t.mark('go')
    t.walk('player1', 0, 0, 9.0, '+use')
    t.wait(0.3)
    t.expect.used('first', count=1, after='go')
    t.expect.used('empty', count=1, after='go')
    t.expect.order('first', 'empty')
    t.expect.sound('buttons/blip1.wav', min=1, after='go')
    t.expect.sound('buttons/blip2.wav', min=1, after='go')
    t.expect.sound('buttons/button10.wav', min=1, after='go')
    t.expect.sound('items/suitchargeok1.wav', count=0, min=0, after='go')
    t.expect.sound('items/suitchargeno1.wav', count=0, min=0, after='go')


@S.test('func_recharge "Trigger On Empty", "Trigger On Recharged", custom recharge sound: drained, it fills again after '
        '30 seconds (co-op), fires and plays the sound', players=1, doc=LOGIC)
def recharge_recharged(t):
    charger(t, 'func_recharge', TriggerOnEmpty='empty', TriggerOnRecharged='full',
            CustomRechargeSound='buttons/bell1.wav', names=('TriggerOnEmpty', 'TriggerOnRecharged'))
    t.kv('player1', 'armorvalue', '0')
    t.wait(0.2)
    t.walk('player1', 0, 0, 9.0, '+use')
    t.mark('drained')
    t.wait(31.0)
    t.expect.used('empty', count=1)
    t.expect.used('full', count=1, after='drained')
    t.expect.sound('buttons/bell1.wav', count=1, after='drained')


@S.test('monster "bloodcolor" 244 (green): a laser hitting a barney sprays green blood', doc=LOGIC)
def monster_bloodcolor(t):
    info(t, 'lend', at=(230, 0, 40))
    t.point('env_laser', targetname='las', at=(-200, 0, 40), LaserTarget='lend', texture='sprites/laserbeam.spr',
            damage='30', names=('LaserTarget',))
    t.point('monster_barney', targetname='bar', at=(100, 0, 0), angles='0 180 0', bloodcolor='244')
    t.flags('bar', '+FL_NOTARGET')
    t.wait(0.3)
    t.mark('go')
    t.fire('las', use='on')
    t.wait(1.0)
    t.expect.msg('svc:23', fields={'te': 115, 'color': 244}, min=1, after='go')
    t.expect.msg('svc:23', fields={'te': 115, 'color': 247}, count=0, min=0, after='go')


@S.test('monster "gibmodel" and "trigger_on_death": a gibbed barney throws gibs of that model (precached at spawn) and '
        'fires the target once', doc=LOGIC)
def monster_gibmodel_death(t):
    t.point('monster_barney', targetname='bar', at=(0, 0, 0), gibmodel='models/metalplategibs.mdl',
            trigger_on_death='dead', names=('trigger_on_death',))
    info(t, 'dead')
    t.wait(0.3)
    t.damage('bar', 10000, dmgtype=8192)
    t.wait(0.2)
    t.snap('gibs', '@gib', 'model')
    t.damage('bar', 10000, dmgtype=8192)
    t.wait(0.2)
    t.expect.precached('models/metalplategibs.mdl', kind='model')
    t.expect.value('gibs', '@gib', 'model', 'models/metalplategibs.mdl')
    t.expect.used('dead', count=1)


@S.test('monstermaker body and scale: its child gets them; a "skin" key is not passed on (no SBO monster shows one)',
        doc=LOGIC)
def maker_cosmetics(t):
    t.point('monstermaker', targetname='mk', at=(0, 0, 0), monstertype='monster_zombie', monstercount='1',
            netname='kid', body='1', skin='1', scale='1.5', names=('netname',))
    t.wait(0.2)
    t.fire('mk')
    t.wait(0.5)
    t.snap('kid', 'kid', 'body', 'skin', 'scale')
    t.expect.value('kid', 'kid', 'body', 1)
    t.expect.value('kid', 'kid', 'skin', 0)
    t.expect.value('kid', 'kid', 'scale', 1.5)


@S.test('scripted_sequence Move to 7 "Teleport": the scientist is on the mark at once, at its angle; "Fire on '
        'Possession" and "Fire on Animation start" fire once', doc=LOGIC)
def script_teleport_hooks(t):
    t.point('monster_scientist', at=(-170, -170, 0), targetname='sci', angles='0 0 0')
    t.point('scripted_sequence', at=(150, 150, 0), angles='0 90 0', targetname='ss', m_iszEntity='sci',
            m_iszPlay='studycart', m_fMoveTo='7', m_iszFireOnPossessed='poss', m_iszFireOnAnimStart='anim',
            names=('m_iszEntity', 'm_iszFireOnPossessed', 'm_iszFireOnAnimStart'))
    info(t, 'poss')
    info(t, 'anim')
    t.flags('sci', '+FL_NOTARGET')
    t.wait(0.3)
    t.fire('ss')
    t.wait(0.5)
    t.snap('there', 'sci', 'origin', 'angles')
    t.wait(1.0)
    t.expect.value('there', 'sci', 'origin.x', t._abs((150, 150, 0))[0], tol=2)
    t.expect.value('there', 'sci', 'origin.y', t._abs((150, 150, 0))[1], tol=2)
    t.expect.value('there', 'sci', 'angles.y', 90, tol=1)
    t.expect.used('poss', count=1)
    t.expect.used('anim', count=1)
    t.expect.order('poss', 'anim')


@S.test('scripted_sequence "Fire after moving" (m_iszFireOnBegin): fires when the walking scientist reaches the mark; '
        '"master": a locked script waits and plays once the master opens', doc=LOGIC)
def script_begin_master(t):
    t.point('env_global', targetname='g', globalstate='parity_script', triggermode='5', initialstate='0',
            spawnflags='5')
    t.point('monster_scientist', at=(-100, -150, 0), targetname='sci', angles='0 90 0')
    t.point('scripted_sequence', at=(-100, 50, 0), angles='0 90 0', targetname='ss', m_iszEntity='sci',
            m_iszPlay='studycart', m_fMoveTo='1', m_iszFireOnBegin='begin', m_iszFireOnPossessed='poss',
            master='g', names=('m_iszEntity', 'm_iszFireOnBegin', 'm_iszFireOnPossessed', 'master'))
    info(t, 'begin')
    info(t, 'poss')
    t.flags('sci', '+FL_NOTARGET')
    t.wait(0.3)
    t.fire('ss')
    t.wait(2.0)
    t.mark('open')
    t.fire('g', use='on')
    t.wait(6.0)
    t.expect.not_used('poss', within=(0, 2.0))
    t.expect.used('poss', count=1, after='open')
    t.expect.used('begin', count=1, after='open')
    t.expect.order('poss', 'begin')




# ---------------------------------------------------------------------------- monster AI keys
AI = 'FEATUREFUL_OPTIONS.md#monster-ai-keys'
WAKE = 6.0      # a named monster stands its first 5 s (SCHED_WAIT_TRIGGER, slIdleTrigger) before it looks around


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


def two_scientists(t, roam_kv, stay_kv):
    """'roam' and 'stay' (Gag: no chatter), each with a mark on its spawn spot; snapshot 'b' after 11 s."""
    node_grid(t)
    t.point('monster_scientist', targetname='roam', at=(-150, -150, 0), angles='0 90 0', spawnflags='2', **roam_kv)
    t.point('monster_scientist', targetname='stay', at=(150, 150, 0), angles='0 270 0', spawnflags='2', **stay_kv)
    info(t, 'roam0', at=(-150, -150, 0))
    info(t, 'stay0', at=(150, 150, 0))
    t.teleport('player1', (200, -200, 0))
    t.wait(WAKE + 5.0)
    for n in ('roam', 'stay', 'roam0', 'stay0'):
        t.snap('b', n, 'origin')
    t.expect.distance('b', 'roam', 'roam0', 48, 1000)
    t.expect.distance('b', 'stay', 'stay0', 0, 16)


@S.test('monster "freeroam" Always: an idle scientist walks from node to node; one with Map default (worldspawn '
        '"freeroam" off) stands', players=1, doc=AI)
def freeroam_always(t):
    two_scientists(t, dict(freeroam='2'), {})


@S.test('monster "prisonerto" Player: a grunt does not see the player (no enemy) while a grunt without it targets him',
        players=1, doc=AI)
def prisonerto_player(t):
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.point('monster_human_grunt', targetname='gpris', at=(-150, 150, 0), angles='0 270 0', prisonerto='2',
            netname='sq_a')
    t.point('monster_human_grunt', targetname='gfree', at=(150, 150, 0), angles='0 270 0', netname='sq_b')
    t.teleport('player1', (0, -150, 0))
    t.wait(WAKE + 1.0)
    t.snap('s', 'gpris', 'enemy')
    t.snap('s', 'gfree', 'enemy')
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('s', 'gpris', 'enemy', '')
    t.expect.value('s', 'gfree', 'enemy', 'player')


def grunt_and_zombie(t, **zkv):
    """A grunt and a zombie across the room (the player out of it: no target); snapshot 's' once both look around."""
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_human_grunt', targetname='grunt', at=(-180, 0, 0), angles='0 0 0')
    t.point('monster_zombie', targetname='zom', at=(180, 0, 0), angles='0 180 0', **zkv)
    t.wait(0.3)
    for n in ('grunt', 'zom'):
        t.kv(n, 'health', '100000')     # nobody dies before the snapshot
    t.teleport('player1', (0, -200, 0))
    t.wait(WAKE + 2.0)
    t.snap('s', 'grunt', 'enemy')
    t.snap('s', 'zom', 'enemy')
    t.flags('player1', '-FL_NOTARGET')


@S.test('monster "ignoredby" Human military on a zombie: the grunt does not see it, the zombie still goes for the grunt',
        players=1, doc=AI)
def ignoredby_monster(t):
    grunt_and_zombie(t, ignoredby='4')
    t.expect.value('s', 'grunt', 'enemy', 'monster_zombie', op='!=')
    t.expect.value('s', 'zom', 'enemy', 'monster_human_grunt')


@S.test('monster "prisonerto" Human military on a zombie: neither the grunt nor the zombie sees the other; without '
        'the key both fight', players=1, doc=AI)
def prisonerto_monster(t):
    grunt_and_zombie(t, prisonerto='4')
    t.expect.value('s', 'grunt', 'enemy', 'monster_zombie', op='!=')
    t.expect.value('s', 'zom', 'enemy', '')


@S.test('monster "prisonerto" / "ignoredby" unset: a grunt and a zombie see each other (the control)', players=1,
        doc=AI)
def prisoner_ignored_control(t):
    grunt_and_zombie(t)
    t.expect.value('s', 'grunt', 'enemy', 'monster_zombie')
    t.expect.value('s', 'zom', 'enemy', 'monster_human_grunt')


MIDS = 8
MID_LABELS = ('after',) + tuple('mid%d' % k for k in range(MIDS))


def alert_after_kill(t, **kv):
    """A grunt fights a zombie until the test kills it, then: snapshots 'after' (0.5 s on), one a second for 8 s
    ('mid0' ..) and 'later'."""
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    # prisonerto 2: the grunt never takes a player as his enemy (awake from the map start, he could see the test
    # player passing a neighbor room and keep him as his enemy: then the fight never ends)
    t.point('monster_human_grunt', targetname='grunt', at=(-150, 0, 0), angles='0 0 0', prisonerto='2', **kv)
    t.point('monster_zombie', targetname='victim', at=(150, 0, 0), angles='0 180 0')
    t.wait(0.3)
    for n in ('grunt', 'victim'):
        t.kv(n, 'health', '100000')     # nobody dies before the test kills the zombie
    t.teleport('player1', (200, -200, 0))
    t.wait(2.0)     # (awake since the map start) they fight; the zombie dies before it reaches the grunt
    t.kv('victim', 'health', '50')
    t.damage('victim', 1000)    # the fight is over: the zombie dies
    t.wait(0.5)
    t.snap('after', 'grunt', 'origin', 'state', 'schedule', 'enemy')
    # a grunt may first play his victory dance (a few seconds) before anything else: watch 8 s
    for k in range(MIDS):
        t.wait(1.0)
        t.snap('mid%d' % k, 'grunt', 'origin', 'state', 'schedule', 'enemy')
    t.snap('later', 'grunt', 'origin', 'state')
    t.flags('player1', '-FL_NOTARGET')


@S.test('monster "active_alert" Always: after killing its enemy a grunt goes to the gunfire he hears or walks around '
        'once (Featureful\'s InvestigateSound / Free Roaming (alert)); the Default one never roams', players=1, doc=AI)
def active_alert_roams(t):
    alert_after_kill(t, active_alert='2')
    t.expect.value_any(MID_LABELS, 'grunt', 'schedule', ['Free Roaming (alert)', 'InvestigateSound'])


@S.test('monster "active_alert" Default: after killing its enemy a grunt never roams (Half-Life\'s alert behavior: '
        'he may still go to a sound he hears)', players=1, doc=AI)
def active_alert_default(t):
    alert_after_kill(t)
    for label in MID_LABELS:
        t.expect.value(label, 'grunt', 'schedule', 'Free Roaming (alert)', op='!=')


def follow_then_hide(t, cls, kv, start=None, fence=False):
    """'mon' follows player1 (+use, or the scripted_following 'start'), then the player goes into a closed box
    nobody can reach (fence: walls 48 high, no roof: seen over, not climbed); snapshots 'on' (following) and 'off'
    (9 s in the box)."""
    t.flags('player1', '+FL_NOTARGET')
    t.point(cls, targetname='mon', at=(-150, -150, 0), angles='0 90 0', **kv)
    t.teleport('player1', (-150, -50, 0))
    t.wait(0.5)
    if start:
        t.fire(start, activator='player1')
    else:
        t.fire('mon', activator='player1', caller='player1')
    t.wait(0.5)
    t.snap('on', 'mon', 'follow')
    h = 48 if fence else 160
    walls = [((160, 8, h), (150, 70, 0)), ((160, 8, h), (150, 230, 0)), ((8, 160, h), (70, 150, 0)),
             ((8, 160, h), (230, 150, 0))]
    if not fence:
        walls.append(((160, 160, 8), (150, 150, 160)))
    for size, at in walls:
        t.brush('func_wall', size=size, at=at)
    t.teleport('player1', (150, 150, 0))
    t.wait(9.0)     # his +use answer first (Idle Speak Wait, a few seconds), then the follow
    t.snap('off', 'mon', 'follow')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('on', 'mon', 'follow', 1)


ALLY = dict(is_player_ally='1', followage_policy='0')


@S.test('monster "followfailpolicy" Default on a scientist: with the player out of reach he stops following (1.38)',
        players=1, doc=FOLLOW)
def follow_fail_sci_default(t):
    follow_then_hide(t, 'monster_scientist', {})
    t.expect.value('off', 'mon', 'follow', 0)


@S.test('monster "followfailpolicy" Keep trying on a scientist: with the player out of reach he keeps following',
        players=1, doc=FOLLOW)
def follow_fail_sci_keep(t):
    follow_then_hide(t, 'monster_scientist', dict(followfailpolicy='1'))
    t.expect.value('off', 'mon', 'follow', 1)


@S.test('monster "followfailpolicy" Default on a player-ally barney: with the player out of reach he keeps '
        'following (1.38)', players=1, doc=FOLLOW)
def follow_fail_barney_default(t):
    follow_then_hide(t, 'monster_barney', ALLY)
    t.expect.value('off', 'mon', 'follow', 1)


@S.test('monster "followfailpolicy" Stop following on a player-ally barney: with the player out of reach he stops',
        players=1, doc=FOLLOW)
def follow_fail_barney_stop(t):
    follow_then_hide(t, 'monster_barney', dict(followfailpolicy='2', **ALLY))
    t.expect.value('off', 'mon', 'follow', 0)


@S.test('monster "followfailpolicy" Stop following on a scientist with followage_policy Scripted only (started by '
        'scripted_following): he keeps trying, since +use could not start him again', players=1, doc=FOLLOW)
def follow_fail_policy_scripted(t):
    t.point('scripted_following', targetname='start', entity='mon', use_type='1', names=('entity',))
    follow_then_hide(t, 'monster_scientist', dict(followfailpolicy='2', followage_policy='1'), start='start')
    t.expect.value('off', 'mon', 'follow', 1)


@S.test('monster "followfailpolicy" Run to the nearest spot on a scientist: with the player out of reach he runs to '
        'the node nearest the fenced box he can see him from, and keeps following', players=1, doc=FOLLOW)
def follow_fail_nearest(t):
    for x in (-200, -100, 0):
        for y in (-200, -100, 0):
            t.point('info_node', at=(x, y, 16))
    info(t, 'boxside', at=(0, 0, 0))
    follow_then_hide(t, 'monster_scientist', dict(followfailpolicy='3'), fence=True)
    t.snap('off', 'mon', 'origin')
    t.snap('off', 'boxside', 'origin')
    t.expect.value('off', 'mon', 'follow', 1)
    t.expect.distance('off', 'mon', 'boxside', 0, 110)


# worldspawn "freeroam" makes every monster of the map with Map default roam while it is on: the last test but the
# camera one, so no later room's monsters walk off


@S.test('worldspawn "freeroam" 1: a scientist with Freeroam Map default walks around, one with Never stands',
        players=1, doc=AI)
def freeroam_world(t):
    t.kv('world', 'freeroam', '1')
    two_scientists(t, {}, dict(freeroam='1'))
    t.kv('world', 'freeroam', '0')


# ---------------------------------------------------------------------------------------------- colormap (f4bb3873)
@S.test('"colormap" on env_model, monster_generic, an item and a dead monster: the entity keeps the map\'s top and '
        'bottom colors (sent to clients like a player\'s)', doc=LOGIC)
def colormap_key(t):
    t.point('env_model', at=(-150, 0, 0), targetname='em', model='models/scientist.mdl', colormap='5140')
    t.point('monster_generic', at=(-50, 0, 0), targetname='mg', model='models/scientist.mdl', colormap='40960',
            spawnflags='16')                                       # Prisoner: stands still
    t.point('item_battery', at=(50, 0, 0), targetname='it', colormap='17920')
    t.point('monster_scientist_dead', at=(150, 0, 0), targetname='dm', colormap='57825')
    t.wait(0.5)
    for n in ('em', 'mg', 'it', 'dm'):
        t.snap('s', n, 'colormap')
    t.expect.value('s', 'em', 'colormap', 5140)      # orange top (20), orange bottom (20 << 8)
    t.expect.value('s', 'mg', 'colormap', 40960)     # red top (0), blue bottom (160 << 8)
    t.expect.value('s', 'it', 'colormap', 17920)
    t.expect.value('s', 'dm', 'colormap', 57825)


# ------------------------------------------------------------------------------------------ point_contents (7a20f521)
@S.test('server command point_contents <x> <y> <z>: prints the contents at the point (empty in the room, solid in '
        'its floor); without three numbers it prints the usage', doc=LOGIC)
def point_contents_command(t):
    air = t._abs((0, 0, 64))
    floor = t._abs((0, 0, -4))
    t.console('point_contents %g %g %g' % air)
    t.console('point_contents %g %g %g' % floor)
    t.console('point_contents 1')
    t.wait(0.1)
    t.expect.console('Point contents at (%g, %g, %g) is empty' % air)
    t.expect.console('Point contents at (%g, %g, %g) is solid' % floor)
    t.expect.console('Usage: point_contents <x> <y> <z>')


GIBSCALE = 'FEATUREFUL_OPTIONS.md#func_tank-keys-and-gib-scale-batch-8'


@S.test('func_breakable / func_pushable "gibscale" 0.5: the gibs come in the "BreakModel" message with scale 50 '
        '(scale x 100, after TE_BREAKMODEL\'s fields); a breakable without it sends TE_BREAKMODEL as before',
        players=1, doc=GIBSCALE)
def breakable_gibscale(t):
    t.brush('func_breakable', size=(48, 48, 48), at=(-100, 0, 0), targetname='bg', health='20', material='1',
            gibscale='0.5', m_iGibs='4')
    t.brush('func_pushable', size=(48, 48, 48), at=(100, 0, 0), targetname='pg', health='20', material='1',
            gibscale='2', spawnflags='128')
    t.brush('func_breakable', size=(48, 48, 48), at=(0, 150, 0), targetname='bs', health='20', material='1')
    t.wait(0.3)
    t.mark('break')
    t.damage('bg', 50, dmgtype=2)
    t.damage('pg', 50, dmgtype=2)
    t.wait(0.3)
    t.mark('stock')
    t.damage('bs', 50, dmgtype=2)
    t.wait(0.3)
    t.expect.removed('bg')
    t.expect.removed('pg')
    t.expect.msg('BreakModel', fields={'scale': 50, 'count': 4, 'life': 25}, after='break', count=1)
    t.expect.msg('BreakModel', fields={'scale': 200}, after='break', count=1)
    t.expect.no_msg('BreakModel', after='stock')
    t.expect.msg('svc:23', fields={'te': 108}, after='stock', count=1)
