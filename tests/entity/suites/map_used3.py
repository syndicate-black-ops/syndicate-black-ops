"""Tier 4 batch 2: func_tank keys the 1.38 maps set (13 maps), on a real server: an Active tank (flag 1) aiming at a
test player. Yaw rate / range, pitch rate / range, yaw tolerance, rate of fire, bullet type, persistence. Each test
checks the FGD's description (sdk/syndicate.fgd, BaseTank)."""

from etest import Suite

SUITE = Suite('map_used3', 'Tier 4 batch 2: func_tank yaw / pitch rate and range, tolerance, rate of fire, bullet, '
              'persistence', players=1)
S = SUITE

DMG_BULLET = 2
TANK_AT = (0, 0, 48)
BASE = dict(yawrate='30', yawrange='180', yawtolerance='15', pitchrate='0', pitchrange='0', pitchtolerance='180',
            firerate='1', bullet='1', bullet_damage='1', persistence='1', firespread='0', barrel='0')


def tank(t, facing=90, **kv):
    keys = dict(BASE)
    keys.update(kv)
    t.brush('func_tank', size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags='1',
            angles='0 %d 0' % facing, **keys)


def player_north(t, at=(0, 200, 0)):
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.teleport('player1', at, yaw=270)
    t.mark('in')


def tracking_then_north(t):
    """The player first stands east, in front of the tank (facing east), until it tracks him (an idle tank looks for a
    target every 2 s), then is put to the north: mark 'in'."""
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.teleport('player1', (200, 0, 0), yaw=180)
    t.wait(2.5)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('in')


def tracking_east(t):
    """The player stands east, in front of the tank (facing east), until it tracks him: mark 'in'."""
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.teleport('player1', (200, 0, 0), yaw=180)
    t.wait(2.5)
    t.mark('in')


def done(t):
    t.teleport('player1', (-200, -200, 0))
    t.kv('player1', 'health', '100')
    t.kv('player1', 'max_health', '100')


@S.test('func_tank Yaw rate 30: facing east, it turns towards a player to the north at 30 degrees a second (about 30 '
        'after 1 s)', players=1, covers=('func_tank', 'func_tank:1', 'func_tank.yawrate'))
def yaw_rate(t):
    tank(t, facing=0, yawtolerance='5')
    tracking_then_north(t)
    t.wait(1.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 18, op='>=')
    t.expect.value('s', 'tk', 'angles.y', 42, op='<=')


@S.test('func_tank Yaw range 30: facing east, it turns towards a player to the north only 30 degrees',
        players=1, covers=('func_tank.yawrange',))
def yaw_range(t):
    tank(t, facing=0, yawrate='90', yawrange='30')
    player_north(t)
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 30, tol=1.5)


@S.test('func_tank Pitch rate 20 / Pitch range 10: aiming down at a player below, it pitches 10 degrees and no more; '
        'with Pitch range 0 it stays level', players=1, covers=('func_tank.pitchrate', 'func_tank.pitchrange'))
def pitch_range(t):
    t.brush('func_tank', size=(32, 32, 32), at=(-100, 0, 160), origin=True, targetname='tk', spawnflags='1',
            angles='0 90 0', **dict(BASE, pitchrate='20', pitchrange='10'))
    t.brush('func_tank', size=(32, 32, 32), at=(100, 0, 160), origin=True, targetname='flat', spawnflags='1',
            angles='0 90 0', **dict(BASE, pitchrate='20', pitchrange='0'))
    player_north(t, at=(0, 150, 0))
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    t.snap('s', 'flat', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.x', 10, tol=1.0)
    t.expect.value('s', 'flat', 'angles.x', 0, tol=0.5)


@S.test('func_tank Yaw tolerance 15: facing east and turning at 30 degrees a second towards a player to the north, it '
        'does not fire until it is within 15 degrees of him (75 degrees: 2.5 s)',
        players=1, covers=('func_tank.yawtolerance',))
def yaw_tolerance(t):
    tank(t, facing=0, firerate='4')
    tracking_then_north(t)
    t.wait(6.0)
    done(t)
    t.expect.not_damaged('player1', attacker='tk', after='in', within=(0.3, 2.0))
    t.expect.damaged('player1', attacker='tk', after='in', within=(2.0, 6.0), type=DMG_BULLET)


def make_rate_test(rate, lo, hi, covers):
    def rate_test(t):
        tank(t, facing=0, firerate=str(rate))
        tracking_east(t)
        t.wait(2.0)
        done(t)
        # it aims at the eyes: each hit is a head hit, 10 health for Damage Per Bullet 1
        t.expect.damaged('player1', attacker='tk', after='in', within=(0, 2.0), min=lo)
        t.expect.damaged('player1', attacker='tk', after='in', within=(0, 2.0), total=(lo * 10, hi * 10))

    rate_test.__name__ = 'fire_rate_%d' % rate
    S.test('func_tank Rate of Fire %d (Damage Per Bullet 1, Bullet accuracy 0 Perfect Shot): facing the player it hits '
           'him %d times a second (%d to %d hits in 2 s)' % (rate, rate, lo, hi), players=1, covers=covers)(rate_test)


make_rate_test(4, 5, 10, ('func_tank.firerate', 'func_tank.firespread', 'func_tank.firespread=0'))
make_rate_test(1, 1, 3, ('func_tank.firerate',))


def make_bullet_test(value, name):
    def bullet(t):
        tank(t, facing=0, bullet=str(value), bullet_damage='0', firerate='2')
        tracking_east(t)
        t.wait(2.0)
        done(t)
        if value:
            t.expect.damaged('player1', attacker='tk', after='in', within=(0, 2.0), type=DMG_BULLET)
        else:
            t.expect.not_damaged('player1', attacker='tk', after='in', within=(0, 2.0))

    bullet.__name__ = 'bullet_%d' % value
    S.test('func_tank Bullets %d %s (Damage Per Bullet 0: the type\'s own damage): %s' % (
        value, name, 'it hits the player with bullets' if value else 'it fires nothing'),
        players=1, covers=('func_tank.bullet', 'func_tank.bullet=%d' % value))(bullet)


for _v, _n in ((0, 'None'), (1, '9mm'), (2, 'MP5'), (3, '12mm')):
    make_bullet_test(_v, _n)


@S.test('func_tank Firing persistence 3: when a wall hides the player it keeps firing at where it saw him for 3 s '
        '(each shot fires its Trigger on firing); persistence 0.5 stops after half a second', players=1,
        covers=('func_tank.persistence',))
def persistence(t):
    tank(t, facing=0, firerate='4', persistence='3', m_iszLocusFire='shot')     # a global name (the key is a string: not prefixed)
    t.brush('func_wall_toggle', size=(16, 200, 200), at=(100, 0, 0), targetname='wall', spawnflags='1')
    tracking_east(t)
    t.wait(0.5)
    t.fire('wall')      # on: the player is hidden
    t.mark('hidden')
    t.wait(4.5)
    t.fire('wall')      # off
    t.kv('tk', 'persistence', '0.5')
    t.wait(1.5)
    t.fire('wall')
    t.mark('hidden2')
    t.wait(3.0)
    t.fire('wall')
    done(t)
    t.expect.fired('=shot', after='hidden', within=(1.5, 2.5), min=1)
    t.expect.fired('=shot', after='hidden', within=(3.6, 4.5), count=0)
    t.expect.fired('=shot', after='hidden2', within=(1.0, 3.0), count=0)
