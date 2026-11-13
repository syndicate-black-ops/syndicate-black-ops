"""func_button, func_rot_button, momentary_rot_button and button_target on a real server, pressed by a test player
(GAPS.md tier 1 "func_door / door_rotating / button / rot_button / momentary / button_target", "Masters on the
classes"): func_button flags 1 Don't move, 32 Toggle, 64 Sparks, 256 Touch Activates; master with the locked /
unlocked sounds; wait, delay, sounds, health (shootable); func_rot_button distance, axes, flag 1 Not solid, master;
momentary_rot_button held +use, flag 16 Auto Return with returnspeed, master; button_target flag 1 Use Activates, shot.
What each checks is the FGD's description (sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('buttons', 'func_button / func_rot_button / momentary_rot_button / button_target: flags, master, '
              'wait, delay, sparks')
S = SUITE

SOLID_NOT = 0
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def lock(t, name='lock', key='key'):
    t.point('multisource', targetname=name)
    t.point('trigger_relay', targetname=key, target=name, triggerstate='1')


def button(t, **kv):
    """A button on the north side at eye height, moving north 12 units (lip 4) at 100 u/s."""
    kv.setdefault('speed', '100')
    kv.setdefault('wait', '1')
    return t.brush('func_button', size=(16, 32, 32), at=(0, 120, 40), tex='+0~ANZYG1', angles='0 90 0', **kv)


def stand(t, player='player1'):
    """The player in front of the button, looking at it."""
    t.teleport(player, (0, 70, 0), yaw=90)
    t.look(player, 0, 90)
    t.wait_frames(3)


def press(t, player='player1', mark=None):
    if mark:
        t.mark(mark)
    t.move(player, 0, 0, 3, '+use')
    t.wait_frames(4)


# ------------------------------------------------------------------------------------------------ func_button

@S.test('func_button flag 1 Don\'t move: pressed, it fires its target and stays where it is', players=1,
        covers=('func_button:1',))
def button_dont_move(t):
    button(t, targetname='btn', target='out', spawnflags='1')
    info(t, 'out')
    t.wait(1.0)     # first test: touches and +use right after the suite start were not taken (seen 2026-11-07)
    t.snap('before', 'btn', 'origin')
    stand(t)
    press(t, mark='go')
    t.wait(0.3)
    t.snap('after', 'btn', 'origin')
    t.expect.fired('out', activator='player1', count=1, after='go', within=(0, 0.1))
    t.expect.same('before', 'after', 'btn', 'origin')


@S.test('func_button: it moves in (speed, lip), fires at the end of the move, comes back after its wait and can be '
        'pressed again; sounds 3 is played when pressed', players=1,
        covers=('func_button', 'func_button.wait', 'func_button.speed', 'func_button.lip', 'func_button.sounds=3'))
def button_wait(t):
    button(t, targetname='btn', target='out', wait='0.5', lip='4', sounds='3')
    info(t, 'out')
    stand(t)
    press(t, mark='go')
    t.wait(0.45)
    t.snap('in', 'btn', 'origin')
    press(t)                                    # pushed in and waiting: ignored
    t.wait(0.9)
    t.snap('back', 'btn', 'origin')
    press(t, mark='again')
    t.wait(0.3)
    # the size along its angle (32, plus 1 each side the engine adds) - 2 - lip 4 = 28 units north in 0.28 s
    t.expect.value('in', 'btn', 'origin', [0, 28, 0], tol=0.5)
    t.expect.value('back', 'btn', 'origin', [0, 0, 0], tol=0.5)
    t.expect.fired('out', activator='player1', count=1, after='go', within=(0.2, 0.4))
    t.expect.fired('out', count=2)
    t.expect.fired('out', count=1, after='again')
    t.expect.sound('buttons/button3.wav', ent='btn', after='go', min=1, within=(0, 0.1))


@S.test('func_button wait -1: it stays pressed in', players=1, covers=('func_button.wait',))
def button_stay(t):
    button(t, targetname='btn', target='out', wait='-1')
    info(t, 'out')
    stand(t)
    press(t)
    t.wait(1.5)
    t.snap('in', 'btn', 'origin')
    press(t)
    t.wait(0.3)
    t.expect.value('in', 'btn', 'origin', [0, 28, 0], tol=0.5)
    t.expect.fired('out', count=1)


@S.test('func_button flag 32 Toggle: pressed it stays in and fires its target; pressed again it comes back out and '
        'fires it again', players=1, covers=('func_button:32',))
def button_toggle(t):
    button(t, targetname='btn', target='out', spawnflags='32')
    info(t, 'out')
    stand(t)
    press(t, mark='on')
    t.wait(1.5)
    t.snap('in', 'btn', 'origin')
    press(t, mark='off')
    t.wait(0.5)
    t.snap('out', 'btn', 'origin')
    t.expect.value('in', 'btn', 'origin', [0, 28, 0], tol=0.5)
    t.expect.value('out', 'btn', 'origin', [0, 0, 0], tol=0.5)
    t.expect.fired('out', count=1, after='on', within=(0, 0.3))
    t.expect.fired('out', count=1, after='off', within=(0, 0.3))


@S.test('func_button flag 256 Touch Activates: a player walking into it presses it; a button without the flag is not '
        'pressed by a touch', players=1, covers=('func_button:256',))
def button_touch(t):
    t.brush('func_button', size=(32, 16, 64), at=(-120, 200, 0), tex='+0~ANZYG1', angles='0 90 0', target='out_t',
            spawnflags='256', speed='100')
    t.brush('func_button', size=(32, 16, 64), at=(120, 200, 0), tex='+0~ANZYG1', angles='0 90 0', target='out_n',
            speed='100')
    info(t, 'out_t')
    info(t, 'out_n')
    t.mark('go')
    t.teleport('player1', (-120, 150, 0), yaw=90)
    t.walk('player1', 200, 0, 0.5)
    t.teleport('player1', (120, 150, 0), yaw=90)
    t.walk('player1', 200, 0, 0.5)
    t.wait(0.3)
    t.expect.fired('out_t', activator='player1', count=1, after='go')
    t.expect.not_fired('out_n', after='go')


@S.test('func_button master: used while locked it plays its locked sound and does not fire; unlocked, it fires with '
        'its unlocked sound', players=1,
        covers=('func_button.master', 'func_button.locked_sound=2', 'func_button.unlocked_sound=3'))
def button_master(t):
    lock(t)
    button(t, target='out', master='lock', locked_sound='2', unlocked_sound='3')
    info(t, 'out')
    t.wait(0.3)
    stand(t)
    press(t, mark='locked')
    t.wait(0.8)                                 # the lock sounds of a button share a 0.5 s wait
    t.mark('unlock')
    t.fire('key')
    press(t)
    t.wait(0.4)
    t.expect.sound('buttons/button2.wav', ent='@func_button', after='locked', within=(0, 0.2), count=1)
    t.expect.not_fired('out', after='locked', within=(0, 0.8))
    t.expect.sound('buttons/button3.wav', ent='@func_button', after='unlock', count=1)
    t.expect.fired('out', activator='player1', count=1, after='unlock')


@S.test('func_button delay: its target fires "delay" seconds after the button is in', players=1,
        covers=('func_button.delay',))
def button_delay(t):
    button(t, target='out', delay='0.6', speed='280')     # in after 0.1 s
    info(t, 'out')
    stand(t)
    press(t, mark='go')
    t.wait(1.0)
    t.expect.fired('out', activator='player1', count=1, after='go', within=(0.65, 0.85))


@S.test('func_button health: shootable; damage presses it with the attacker as activator', players=1,
        covers=('func_button.health',))
def button_health(t):
    button(t, targetname='btn', target='out', health='5')
    info(t, 'out')
    t.mark('go')
    t.damage('btn', 10, 2, 'player1')           # DMG_BULLET from player1
    t.wait(0.4)
    t.expect.fired('out', activator='player1', count=1, after='go')


@S.test('func_button flag 64 Sparks: it sparks again and again while out (not only once), none while pressed in',
        players=1, covers=('func_button:64',))
def button_sparks(t):
    button(t, targetname='btn', target='out', spawnflags='96', speed='200')   # Sparks + Toggle (stays in)
    info(t, 'out')
    t.mark('out1')
    t.wait(4.0)
    stand(t)
    press(t, mark='in')
    t.wait(3.0)
    t.expect.sound('buttons/spark', ent='btn', after='out1', within=(0, 4.0), min=3)
    t.expect.msg('svc:23', fields={'te': 9}, after='out1', within=(0, 4.0), min=3)      # TE_SPARKS
    t.expect.sound('buttons/spark', ent='btn', after='in', within=(0.3, 3.0), count=0)


# ------------------------------------------------------------------------------------------------ func_rot_button

def rot_button(t, name, at, **kv):
    kv.setdefault('speed', '360')
    kv.setdefault('distance', '90')
    kv.setdefault('wait', '-1')
    return t.brush('func_rot_button', size=(16, 48, 48), at=at, origin=True, targetname=name, tex='+0~ANZYG1', **kv)


@S.test('func_rot_button: +use turns it "distance" degrees about Z and fires its target; flags 64 X Axis and 128 Y '
        'Axis turn about those axes', players=1,
        covers=('func_rot_button', 'func_rot_button.distance', 'func_rot_button.target', 'func_rot_button:64',
                'func_rot_button:128'))
def rot_button_axes(t):
    rot_button(t, 'rz', (0, 120, 20), distance='45', target='out')
    rot_button(t, 'rx', (-150, 150, 20), spawnflags='64')
    rot_button(t, 'ry', (150, 150, 20), spawnflags='128')
    info(t, 'out')
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.look('player1', 20, 90)
    t.wait_frames(3)
    press(t, mark='go')
    t.fire('rx')
    t.fire('ry')
    t.wait(0.6)
    for n in ('rz', 'rx', 'ry'):
        t.snap('after', n, 'angles')
    t.expect.fired('out', activator='player1', count=1, after='go')
    t.expect.value('after', 'rz', 'angles', [0, 45, 0], tol=0.5)
    t.expect.value('after', 'rx', 'angles', [0, 0, 90], tol=0.5)
    t.expect.value('after', 'ry', 'angles', [90, 0, 0], tol=0.5)


@S.test('func_rot_button flag 1 Not solid: a player walks through it; without the flag he can\'t', players=1,
        covers=('func_rot_button:1',))
def rot_button_not_solid(t):
    t.brush('func_rot_button', size=(512, 16, 256), at=(0, 0, 0), origin=True, targetname='ns', tex='+0~ANZYG1',
            spawnflags='1')
    info(t, 'start', at=(0, -100, 0))
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'start', 'origin')
    t.snap('after', 'ns', 'solid')
    t.expect.distance('after', 'player1', 'start', 150, 400)
    t.expect.value('after', 'ns', 'solid', SOLID_NOT)


@S.test('func_rot_button master: fired while locked it does not turn; unlocked, it turns and fires',
        covers=('func_rot_button.master',))
def rot_button_master(t):
    lock(t)
    rot_button(t, 'rb', (0, 120, 20), master='lock', target='out')
    info(t, 'out')
    t.wait(0.3)
    t.fire('rb')
    t.wait(0.5)
    t.snap('locked', 'rb', 'angles')
    t.fire('key')
    t.mark('unlock')
    t.fire('rb')
    t.wait(0.6)
    t.snap('open', 'rb', 'angles')
    t.expect.value('locked', 'rb', 'angles', [0, 0, 0], tol=0.5)
    t.expect.value('open', 'rb', 'angles', [0, 90, 0], tol=0.5)
    t.expect.not_fired('out', within=(0, 0.8))
    t.expect.fired('out', count=1, after='unlock')


# ------------------------------------------------------------------------------------------------ momentary_rot_button

@S.test('momentary_rot_button: held +use turns it at its speed and sends its target its position (USE_SET, 0..1); '
        'flag 16 Auto Return: released, it turns back at returnspeed', players=1,
        covers=('momentary_rot_button', 'momentary_rot_button.speed', 'momentary_rot_button.distance',
                'momentary_rot_button:16', 'momentary_rot_button.returnspeed'))
def momentary_auto_return(t):
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='md',
            speed='90', distance='90', returnspeed='45', spawnflags='16')
    t.brush('momentary_door', size=(96, 16, 96), at=(-100, -150, 0), targetname='md', angles='0 0 0', lip='6')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.mark('hold')
    t.walk('player1', 0, 0, 0.5, '+use')
    t.snap('held', 'mb', 'angles')
    t.wait(0.3)
    t.snap('released', 'mb', 'angles')
    t.wait(2.5)
    t.snap('back', 'mb', 'angles')
    t.expect.used('md', caller='mb', use=USE_SET, after='hold', min=3)
    t.expect.value('held', 'mb', 'angles.y', 10, op='>')
    t.expect.value('back', 'mb', 'angles', [0, 0, 0], tol=0.5)
    t.expect.ratio('held', 'mb', 'released', 'mb', 'angles.y', 1.0, tol=0.6)


@S.test('momentary_rot_button: its turning sound (sounds 23, lever3.wav, 2 s long) starts with +use and is stopped '
        'when the player lets go (Half-Life Unified SDK)', players=1,
        covers=('momentary_rot_button', 'momentary_rot_button.sounds=23'))
def momentary_sound_stops(t):
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='sink',
            speed='90', distance='90', sounds='23')
    info(t, 'sink')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.mark('hold')
    t.walk('player1', 0, 0, 0.3, '+use')
    t.mark('released')
    t.wait(0.5)
    t.expect.sound('lever3.wav', ent='mb', after='hold')
    t.expect.sound('lever3.wav', ent='mb', flags=32, after='released', within=(0, 0.5))


@S.test('momentary_rot_button without Auto Return: released, it does not turn back (it may coast on: each held frame '
        'pushes its stop 0.1 s later, HL SDK)', players=1,
        covers=('momentary_rot_button',))
def momentary_stays(t):
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', speed='90',
            distance='90', returnspeed='45', target='sink')    # without a target it does not turn (HL SDK)
    info(t, 'sink')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.walk('player1', 0, 0, 0.6, '+use')
    t.snap('released', 'mb', 'angles')
    t.wait(1.5)
    t.snap('later', 'mb', 'angles')
    t.expect.value('released', 'mb', 'angles.y', 10, op='>')
    t.expect.value('later', 'mb', 'angles.y', 10, op='>')
    t.expect.ratio('released', 'mb', 'later', 'mb', 'angles.y', 1.5, tol=0.5)    # not back towards 0


@S.test('momentary_rot_button master: while the master is locked, held +use does not turn it and its target gets '
        'nothing; unlocked, it turns and sends its target its position', players=1,
        covers=('momentary_rot_button.master',))
def momentary_master(t):
    lock(t)
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', speed='90',
            distance='90', master='lock', target='sink')
    info(t, 'sink')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.mark('locked')
    t.walk('player1', 0, 0, 0.6, '+use')
    t.snap('after', 'mb', 'angles')
    t.wait(0.3)
    t.mark('unlock')
    t.fire('key')
    t.walk('player1', 0, 0, 0.6, '+use')
    t.snap('open', 'mb', 'angles')
    t.expect.value('after', 'mb', 'angles', [0, 0, 0], tol=0.5)
    t.expect.not_used('sink', after='locked', within=(0, 0.9))
    t.expect.value('open', 'mb', 'angles.y', 10, op='>')
    t.expect.used('sink', caller='mb', use=USE_SET, after='unlock', min=3)


# ------------------------------------------------------------------------------------------------ button_target

@S.test('button_target: shot it turns on (frame 1, target fired On) and shot again off (target Off); flag 1 Use '
        'Activates: +use works too, without the flag it does not', players=1,
        covers=('button_target', 'button_target:1', 'button_target.target'))
def button_target(t):
    t.brush('button_target', size=(16, 32, 32), at=(0, 120, 40), targetname='bt', target='out', spawnflags='1')
    t.brush('button_target', size=(16, 32, 32), at=(150, 120, 40), targetname='bn', target='out_n', spawnflags='0')
    info(t, 'out')
    info(t, 'out_n')
    t.mark('shot1')
    t.damage('bt', 5, 2, 'player1')
    t.wait(0.1)
    t.snap('on', 'bt', 'frame')
    t.mark('shot2')
    t.damage('bt', 5, 2, 'player1')
    t.wait(0.1)
    t.snap('off', 'bt', 'frame')
    stand(t)
    press(t, mark='use')
    t.wait(0.2)
    t.teleport('player1', (150, 70, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    press(t, mark='use_n')
    t.wait(0.2)
    t.expect.value('on', 'bt', 'frame', 1)
    t.expect.value('off', 'bt', 'frame', 0)
    t.expect.fired('out', activator='player1', use=USE_ON, count=1, after='shot1', within=(0, 0.05))
    t.expect.fired('out', activator='player1', use=USE_OFF, count=1, after='shot2', within=(0, 0.05))
    t.expect.fired('out', activator='player1', use=USE_ON, count=1, after='use')
    t.expect.not_fired('out_n', after='use_n')


@S.test('button_target master: a locked master keeps a shot from turning it on (HL SDK and 1.38 ignored it; develop '
        '9f1d84a4)', players=1, covers=('button_target.master',))
def button_target_master(t):
    lock(t)
    t.brush('button_target', size=(16, 32, 32), at=(0, 120, 40), targetname='bt', target='out', master='lock')
    info(t, 'out')
    t.wait(0.3)
    t.damage('bt', 5, 2, 'player1')
    t.wait(0.2)
    t.expect.not_fired('out')
