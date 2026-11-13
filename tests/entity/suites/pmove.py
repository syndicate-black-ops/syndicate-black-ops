"""Player movement changes of SBO 2.0 on a real server (pm_shared; GAPS.md tier 2 "pm_shared"): the +use slowdown as
a third of maxspeed (461415bc, HL25), a func_friction zone's friction reset once the player leaves it (761c9a3c,
halflife #1542), trigger_gravity, trigger_push, sv_allowbunnyhopping (aeeded76)."""

from etest import Suite

SUITE = Suite('pmove', 'Player movement: +use slowdown, func_friction reset, trigger_gravity, trigger_push, '
              'sv_allowbunnyhopping')
S = SUITE


def hold(t, player, forward, *buttons):
    """Holds the input until release() (test_move with -1 frames, as suite tank)."""
    t.console('test_move %s %s 0 -1 %s' % (t.ref(player), forward, ' '.join(buttons)))


def release(t, player):
    t.console('test_move %s 0 0 0' % t.ref(player))


@S.test('+use held while walking: the speed on the ground is a third of maxspeed (HL25: maxspeed / 3; 1.38 scaled the '
        'velocity by 0.3 every frame, so it depended on the framerate); without +use the full maxspeed', players=1,
        covers=())
def use_slowdown(t):
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.5)
    hold(t, 'player1', 400, '+use')
    t.wait(0.8)
    t.snap('use', 'player1', 'velocity', 'maxspeed')
    release(t, 'player1')
    t.wait(0.5)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.3)
    hold(t, 'player1', 400)
    t.wait(0.8)
    t.snap('walk', 'player1', 'velocity', 'maxspeed')
    release(t, 'player1')
    t.wait(0.3)
    t.expect.ratio('walk', 'player1', 'use', 'player1', 'velocity.y', 1.0 / 3.0, tol=0.08)


@S.test('func_friction: the player inside has its friction (modifier 20: 0.2); once he leaves, his friction is 1 again '
        '(halflife #1542: in multiplayer 1.38 kept the zone\'s friction until he touched another zone)', players=1,
        covers=('func_friction', 'func_friction.modifier'))
def friction_reset(t):
    t.trigger('func_friction', size=(128, 128, 72), at=(0, 100, 0), modifier='20')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.5)
    t.teleport('player1', (0, 100, 0), yaw=90)
    t.wait(0.5)
    t.snap('in', 'player1', 'friction')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.5)
    t.snap('out', 'player1', 'friction')
    t.expect.value('in', 'player1', 'friction', 0.2, tol=0.01)
    t.expect.value('out', 'player1', 'friction', 1.0, tol=0.01)


@S.test('trigger_gravity: a player touching it gets its gravity (0.25)', players=1,
        covers=('trigger_gravity', 'trigger_gravity.gravity'))
def gravity(t):
    t.trigger('trigger_gravity', size=(128, 128, 72), at=(0, 100, 0), gravity='0.25')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.3)
    t.snap('before', 'player1', 'gravity')
    t.teleport('player1', (0, 100, 0), yaw=90)
    t.wait(0.3)
    t.snap('in', 'player1', 'gravity')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.kv('player1', 'gravity', '1')
    t.wait(0.2)
    t.expect.value('in', 'player1', 'gravity', 0.25, tol=0.01)


@S.test('trigger_push: a player inside is pushed at its speed in its direction (east, 600)', players=1,
        covers=('trigger_push', 'trigger_push.speed'))
def push_up(t):
    t.trigger('trigger_push', size=(128, 128, 72), at=(0, 100, 0), speed='600', angles='0 0 0')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.3)
    t.snap('start', 'player1', 'origin')
    t.teleport('player1', (0, 100, 0), yaw=90)
    t.wait(0.3)
    t.snap('pushed', 'player1', 'origin')
    t.wait(1.5)
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.5)
    # a push of 600 east for 0.3 s carries him well over 60 units (to the trigger's east edge at x 64 and on)
    t.expect.value('pushed', 'player1', 'origin.x', t._abs((60, 0, 0))[0], op='>')


@S.test('sv_allowbunnyhopping (halflife-updated) is registered, default 0 (SBO\'s bunny hop cap stays)',
        covers=())
def bhop_cvar(t):
    t.console('sv_allowbunnyhopping')
    t.wait(0.1)
    t.expect.console('"sv_allowbunnyhopping" is "0"')
