"""Tier 4 batch 4: func_tank / func_tankrocket keys the 1.38 maps set, on a real server (an Active tank aiming at a test
player): barrel length / horizontal / vertical with the flash sprite and its scale, minimum / maximum target range,
pitch tolerance, and for func_tankrocket the turning, range, tolerance, rate of fire and persistence keys suite
map_used3 checks on func_tank. Each shot is counted by the tank's "Trigger on firing" (m_iszLocusFire), which every
tank kind fires per shot. Each test checks the FGD's description (sdk/syndicate.fgd, BaseTank)."""

from etest import Suite

SUITE = Suite('map_used5', 'Tier 4 batch 4: func_tank barrel / flash / ranges / pitch tolerance; func_tankrocket keys')
S = SUITE

TANK_AT = (0, 0, 48)
BASE = dict(yawrate='30', yawrange='180', yawtolerance='15', pitchrate='0', pitchrange='0', pitchtolerance='180',
            firerate='1', bullet='1', bullet_damage='1', persistence='1', firespread='0', barrel='0')


def tank(t, cls='func_tank', facing=0, at=TANK_AT, **kv):
    keys = dict(BASE)
    if cls != 'func_tank':
        keys.pop('bullet')
        keys.pop('bullet_damage')
    keys.update(kv)
    t.brush(cls, size=(32, 32, 32), at=at, origin=True, targetname='tk', spawnflags='1', angles='0 %d 0' % facing,
            m_iszLocusFire=t.name('shot'), **keys)


def player_at(t, at, yaw=180):
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', at, yaw=yaw)


def done(t):
    # the tank stops and the player stays in god mode until a rocket still in flight has gone off: one that hit him
    # after this left him dead, a spectator in the next test, whose tank then had no one to fire at
    t.fire('tk', use='off')
    t.teleport('player1', (-200, -200, 0))
    t.wait(1.5)
    t.flags('player1', '-FL_GODMODE')
    t.kv('player1', 'health', '100')
    t.kv('player1', 'max_health', '100')


def tracking_east(t):
    """The player stands east, in front of the tank (facing east), until it tracks him: mark 'in'."""
    player_at(t, (200, 0, 0))
    t.wait(2.5)
    t.mark('in')


# ------------------------------------------------------------------------------------------------ func_tank

@S.test('func_tank Barrel Length 40 / Horizontal 0 / Vertical 10 with a Flash Sprite at Sprite scale 2: each shot shows '
        'the flash at the barrel\'s end (40 ahead, 10 up) at scale 2',
        players=1, covers=('func_tank.barrel', 'func_tank.barrely', 'func_tank.barrelz', 'func_tank.spriteflash',
                'func_tank.spritescale'))
def barrel_flash(t):
    tank(t, firerate='10', barrel='40', barrely='0', barrelz='10', spriteflash='sprites/fire.spr',
         spritescale='2')
    t.point('info_target', targetname='muzzle', at=(TANK_AT[0] + 40, TANK_AT[1], TANK_AT[2] + 16 + 10))
    tracking_east(t)
    t.wait(0.5)
    t.snap('s', '@env_sprite', 'origin', 'scale', 'model')
    t.snap('s', 'muzzle', 'origin')
    done(t)
    t.expect.distance('s', '@env_sprite', 'muzzle', 0, 6, flat=False)
    t.expect.value('s', '@env_sprite', 'scale', 2, tol=0.01)
    t.expect.value('s', '@env_sprite', 'model', 'sprites/fire.spr')


@S.test('func_tank Maximum target range 150: a player 200 away is not fired at; Minimum target range 100: one 60 away '
        'is not either; one in between (120) is', players=1, covers=('func_tank.maxrange', 'func_tank.minrange'))
def ranges(t):
    tank(t, firerate='4', minRange='100', maxRange='150')
    player_at(t, (200, 0, 0))
    t.mark('far')
    t.wait(2.5)
    t.teleport('player1', (60, 0, 0), yaw=180)
    t.mark('near')
    t.wait(2.5)
    t.teleport('player1', (120, 0, 0), yaw=180)
    t.mark('mid')
    t.wait(2.5)
    done(t)
    t.expect.fired('shot', after='far', within=(0, 2.5), count=0)
    t.expect.fired('shot', after='near', within=(0, 2.5), count=0)
    t.expect.fired('shot', after='mid', within=(0.1, 2.5), min=1)


@S.test('func_tank Pitch tolerance 5: tilting down at 10 degrees a second towards a player below, it fires only once '
        'within 5 degrees of him (about 3 s once it has seen him)', players=1, covers=('func_tank.pitchtolerance',))
def pitch_tolerance(t):
    # the player 150 east and 112 below the barrel (about 37 degrees down): 37 - 5 = 32 degrees at 10 a second
    tank(t, at=(-50, 0, 160), firerate='4', pitchrate='10', pitchrange='60', pitchtolerance='5')
    player_at(t, (100, 0, 0))
    t.mark('in')
    t.wait(7.0)     # an idle tank looks for a target every 2 s
    done(t)
    t.expect.fired('shot', after='in', within=(0, 2.0), count=0)
    t.expect.fired('shot', after='in', within=(2.0, 7.0), min=1)


# ------------------------------------------------------------------------------------------------ func_tankrocket

R = 'func_tankrocket'


@S.test('func_tankrocket Yaw rate 30 / Yaw range 180: facing east, it turns towards a player to the north at 30 '
        'degrees a second', players=1, covers=(R, R + '.yawrate', R + '.yawrange'))
def rocket_yaw_rate(t):
    tank(t, R, yawtolerance='5')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(1.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 18, op='>=')
    t.expect.value('s', 'tk', 'angles.y', 42, op='<=')


@S.test('func_tankrocket Yaw range 30: facing east, it turns towards a player to the north only 30 degrees',
        players=1, covers=(R + '.yawrange',))
def rocket_yaw_range(t):
    tank(t, R, yawrate='90', yawrange='30')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 30, tol=1.5)


@S.test('func_tankrocket Pitch rate 20 / Pitch range 10: aiming down at a player below it pitches 10 degrees and no '
        'more', players=1, covers=(R + '.pitchrate', R + '.pitchrange'))
def rocket_pitch(t):
    tank(t, R, at=(-50, 0, 160), facing=0, pitchrate='20', pitchrange='10', firerate='0.2')
    player_at(t, (100, 0, 0))
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.x', 10, tol=1.0)


@S.test('func_tankrocket Yaw tolerance 15: turning at 30 degrees a second from east to a player to the north it fires '
        'no rocket until it is within 15 degrees of him (2.5 s); Rate of Fire 2: about 2 rockets a second after that',
        players=1, covers=(R + '.yawtolerance', R + '.firerate'))
def rocket_tolerance_rate(t):
    tank(t, R, firerate='2')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(6.0)
    done(t)
    t.expect.fired('shot', after='north', within=(0.3, 2.0), count=0)
    t.expect.fired('shot', after='north', within=(3.5, 6.0), min=3)
    t.expect.spawned('@rpg_rocket', min=3)


@S.test('func_tankrocket Firing persistence 3: when a wall hides the player it keeps firing at where it saw him for '
        '3 s; persistence 0.5 stops after half a second', players=1, covers=(R + '.persistence',))
def rocket_persistence(t):
    tank(t, R, firerate='2', persistence='3')
    t.brush('func_wall_toggle', size=(16, 200, 200), at=(100, 0, 0), targetname='wall', spawnflags='1')
    tracking_east(t)
    t.wait(0.5)
    t.fire('wall')
    t.mark('hidden')
    t.wait(4.5)
    t.fire('wall')
    t.kv('tk', 'persistence', '0.5')
    t.wait(1.5)
    t.fire('wall')
    t.mark('hidden2')
    t.wait(3.0)
    t.fire('wall')
    done(t)
    t.expect.fired('shot', after='hidden', within=(1.0, 2.8), min=1)
    t.expect.fired('shot', after='hidden', within=(3.6, 4.5), count=0)
    t.expect.fired('shot', after='hidden2', within=(1.0, 3.0), count=0)


@S.test('func_tankrocket Pitch tolerance 5: it fires only once within 5 degrees of a player below (about 3 s at 10 '
        'degrees a second, once it has seen him)', players=1, covers=(R + '.pitchtolerance',))
def rocket_pitch_tolerance(t):
    tank(t, R, at=(-50, 0, 160), firerate='2', pitchrate='10', pitchrange='60', pitchtolerance='5')
    player_at(t, (100, 0, 0))
    t.mark('in')
    t.wait(7.0)     # an idle tank looks for a target every 2 s
    done(t)
    t.expect.fired('shot', after='in', within=(0, 2.0), count=0)
    t.expect.fired('shot', after='in', within=(2.0, 7.0), min=1)


@S.test('func_tankrocket Minimum / Maximum target range 100 / 150: no rocket at a player 200 or 60 away, rockets at '
        '120', players=1, covers=(R + '.minrange', R + '.maxrange'))
def rocket_ranges(t):
    tank(t, R, firerate='2', minRange='100', maxRange='150')
    player_at(t, (200, 0, 0))
    t.mark('far')
    t.wait(2.5)
    t.teleport('player1', (60, 0, 0), yaw=180)
    t.mark('near')
    t.wait(2.5)
    t.teleport('player1', (120, 0, 0), yaw=180)
    t.mark('mid')
    t.wait(2.5)
    done(t)
    t.expect.fired('shot', after='far', within=(0, 2.5), count=0)
    t.expect.fired('shot', after='near', within=(0, 2.5), count=0)
    t.expect.fired('shot', after='mid', within=(0.1, 2.5), min=1)


@S.test('func_tankrocket Barrel Length 40 / Vertical 10 with a Flash Sprite at scale 2: its rockets start and its flash '
        'shows at the barrel\'s end', players=1, covers=(R + '.barrel', R + '.barrely', R + '.barrelz', R + '.spritescale'))
def rocket_barrel(t):
    tank(t, R, firerate='4', barrel='40', barrely='0', barrelz='10', spriteflash='sprites/fire.spr',
         spritescale='2')
    t.point('info_target', targetname='muzzle', at=(TANK_AT[0] + 40, TANK_AT[1], TANK_AT[2] + 16 + 10))
    tracking_east(t)
    # the flash lives about 0.1 s and a shot comes every 0.25 s: snapshots over 0.4 s catch at least one
    labels = []
    for k in range(10):
        t.wait(0.04)
        t.snap('f%d' % k, '@env_sprite', 'origin', 'scale', 'model')
        t.snap('f%d' % k, 'muzzle', 'origin')
        labels.append('f%d' % k)
    done(t)
    t.expect.value_any(labels, '@env_sprite', 'model', 'sprites/fire.spr')
    t.expect.distance_any(labels, '@env_sprite', 'muzzle', 0, 6, flat=False)
    t.expect.value_any(labels, '@env_sprite', 'scale', 1.99, op='>=')
    t.expect.value_any(labels, '@env_sprite', 'scale', 2.01, op='<=')
