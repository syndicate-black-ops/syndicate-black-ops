"""Tier 5, final pass: BaseTank items generated over the four tank classes (func_tank.cpp), for the classes where no
other suite asserts them: Bullet accuracy (firespread: the shots' spread at a wall, from their temp entities),
(Team) Master (a refused player does not take the controls), Only Direct (16), Laser Spot (64), Match Target (128),
Player weapon volume, Rotate Sound, and for func_tanklaser the turning, range, tolerance, rate, persistence and barrel
keys suite map_used5 checks on func_tankrocket. Each shot is counted by the tank's "Trigger on firing"
(m_iszLocusFire), which every tank kind fires per shot. A func_tanklaser fires through its env_laser (laserentity),
which needs to exist (else the tank removes itself)."""

from etest import Suite

SUITE = Suite('fgd_gen_tanks', 'Tier 5: tank keys and flags generated over the tank classes', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
CLASSES = ('func_tank', 'func_tanklaser', 'func_tankmortar', 'func_tankrocket')
EF_NODRAW = 128
TE_SPARKS, TE_EXPLOSION, TE_GUNSHOTDECAL = 9, 3, 109
# each class's shot mark at the wall: bullet decals; the laser's sparks at its end (env_laser Spark at End, 32);
# the mortar's explosion
SHOT_TE = {'func_tank': TE_GUNSHOTDECAL, 'func_tanklaser': TE_SPARKS, 'func_tankmortar': TE_EXPLOSION}
KIND_KEYS = {
    'func_tank': dict(bullet='1', bullet_damage='1'),
    'func_tanklaser': dict(laserentity='beam'),
    'func_tankrocket': {},
    'func_tankmortar': dict(iMagnitude='60'),
}
TANK_AT = (0, 0, 48)
BASE = dict(yawrate='30', yawrange='180', yawtolerance='15', pitchrate='0', pitchrange='0', pitchtolerance='180',
            firerate='1', persistence='1', barrel='0')


def short(cls):
    return cls.replace('func_', '')


def tank(t, cls, facing=0, at=TANK_AT, flags=1, name='tk', **kv):
    keys = dict(BASE)
    keys.update(KIND_KEYS[cls])
    keys.update(kv)
    t.brush(cls, size=(32, 32, 32), at=at, origin=True, targetname=name, spawnflags=str(flags),
            angles='0 %d 0' % facing, m_iszLocusFire=t.name('shot'), **keys)
    if cls == 'func_tanklaser':
        # Spark at End (32): TE_SPARKS where the beam ends; no damage
        t.point('env_laser', at=(at[0], at[1], at[2] + 16), targetname='beam', texture='sprites/laserbeam.spr',
                damage='0', width='10', spawnflags='32')


def player_at(t, at, yaw=180):
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', at, yaw=yaw)


def done(t):
    t.teleport('player1', (-200, -200, 0))
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')
    t.kv('player1', 'health', '100')
    t.kv('player1', 'max_health', '100')


def tracking_east(t):
    """The player stands east, in front of the tank (facing east), until it tracks him: mark 'in'."""
    player_at(t, (200, 0, 0))
    t.wait(2.5)
    t.mark('in')


# ---------------------------------------------------------------------------------------------- Bullet accuracy
# A tank at the west wall faces east at a monster_target (not solid: the shots pass it) 356 units away at its own
# height, half way up the room, with the east wall 456 units away: the shots land on that wall around (256, 0, 128). The cone is the table
# gTankSpread (0, 0.025, 0.05, 0.1, 0.25); a shot's offset is up to that times the distance (TankTrace /
# FireBullets: x and y each the sum of two random -0.5..0.5), so about 11, 23, 46 and 114 units at most.
SPREAD_RANGE = {0: (0, 2), 1: (2, 17), 2: (5, 34), 3: (10, 68), 4: (25, 170)}
SPREAD_CASES = {'func_tank': (1, 2, 3, 4), 'func_tanklaser': (0, 1, 2, 3, 4), 'func_tankmortar': (0, 1, 2, 3, 4)}


def spread_case(cls, v):
    lo, hi = SPREAD_RANGE[v]

    @S.test('%s Bullet accuracy %d: shots at a target 456 units from the wall behind it land within %g units of the '
            'aim point (farthest %g to %g)' % (cls, v, hi, lo, hi), players=1, doc=DOC,
            covers=('%s.firespread' % cls, '%s.firespread=%d' % (cls, v)))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        # off until its test (else every room's tank fires all through the run)
        tank(t, cls, at=(-200, 0, 112), flags=0, firerate='4', firespread=str(v))
        t.point('monster_target', targetname='foe', at=(156, 0, 128), frags='11')    # a player ally: Behavior 0 shoots it
        t.fire('tk', use='on')
        t.wait(1.0)
        t.mark('go')
        t.wait(3.0)
        t.fire('tk', use='off')
        t.expect.fired('shot', after='go', min=6)
        t.expect.spread('svc:23', t._abs((256, 0, 128)), lo, hi, fields={'te': SHOT_TE[cls]}, min=6, after='go')
        done(t)
    case.__name__ = 'spread_%s_%d' % (short(cls), v)
    return case


for _c, _vs in SPREAD_CASES.items():
    for _v in _vs:
        spread_case(_c, _v)


# ---------------------------------------------------------------------------------------------- (Team) Master
CONTROL_AT, CONTROLLER_AT = (0, -120, 40), (0, -200, 0)


def controls(t, cls, flags=32, **kv):
    tank(t, cls, facing=90, at=CONTROL_AT, flags=flags, **kv)


def at_controls(t):
    """player1 stands where the tank lets him take its controls (it lets go of one 30 units away)."""
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', CONTROLLER_AT, yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)


def master_case(cls):
    @S.test('%s (Team) Master: a player used on it while its master is locked does not take the controls; once the '
            'master is on he does' % cls, players=1, doc=DOC, covers=('%s.master' % cls,))
    def case(t):
        t.point('multisource', targetname='lock')
        t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')
        controls(t, cls, master='lock')
        at_controls(t)
        t.fire('tk', activator='player1')
        t.wait(0.2)
        t.snap('locked', 'player1', 'tank')
        t.fire('key')
        t.wait(0.2)
        t.fire('tk', activator='player1')
        t.wait(0.2)
        t.snap('open', 'player1', 'tank')
        t.fire('tk', activator='player1', use='off')
        t.wait(0.2)
        done(t)
        t.expect.value('locked', 'player1', 'tank', 0)
        t.expect.value('open', 'player1', 'tank', 1)
    case.__name__ = 'master_' + short(cls)
    return case


# ---------------------------------------------------------------------------------------------- Only Direct (16)
def direct_case(cls):
    @S.test('%s Only Direct (16) with Yaw tolerance 180: turning at 30 degrees a second from east to a player to the '
            'north it fires only once the barrel points at him (about 3 s); without the flag it fires at once' % cls,
            players=1, doc=DOC, covers=('%s:16' % cls,))
    def case(t):
        tank(t, cls, at=(-120, 0, 48), flags=1 | 16, yawtolerance='180', firerate='2', name='tk')
        t.point('info_target', targetname='shot')
        player_at(t, (80, 0, 0))
        t.wait(2.5)                         # it tracks him: facing east
        t.teleport('player1', (-120, 200, 0), yaw=270)
        t.mark('north')
        t.wait(5.0)
        done(t)
        t.expect.fired('shot', after='north', within=(0.3, 2.0), count=0)
        t.expect.fired('shot', after='north', within=(2.5, 5.0), min=2)
    case.__name__ = 'direct_' + short(cls)
    return case


def direct_off_case(cls):
    @S.test('%s without Only Direct, Yaw tolerance 180: turning from east to a player to the north it fires at once '
            '(control for the flag)' % cls, players=1, doc=DOC, covers=(cls,))
    def case(t):
        tank(t, cls, at=(-120, 0, 48), flags=1, yawtolerance='180', firerate='2')
        player_at(t, (80, 0, 0))
        t.wait(2.5)
        t.teleport('player1', (-120, 200, 0), yaw=270)
        t.mark('north')
        t.wait(2.0)
        done(t)
        t.expect.fired('shot', after='north', within=(0.0, 2.0), min=2)
    case.__name__ = 'direct_off_' + short(cls)
    return case


# ---------------------------------------------------------------------------------------------- Laser Spot (64)
def spot_case(cls):
    @S.test('%s Laser Spot (64): turned on it shows a laser dot on the wall the barrel points at (east wall, 256 '
            'east of the room center); turned off the dot is hidden' % cls, players=1, doc=DOC,
            covers=('%s:64' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        tank(t, cls, flags=64, at=(0, 0, 48))
        t.point('info_target', targetname='wallpt', at=(255, 0, 64))
        t.wait(0.3)
        t.fire('tk', use='on')
        t.wait(0.5)
        t.snap('on', '@laser_spot', 'origin', 'effects')
        t.snap('on', 'wallpt', 'origin')
        t.fire('tk', use='off')
        t.wait(0.3)
        t.snap('off', '@laser_spot', 'effects')
        t.point('trigger_relay', targetname='rm', killtarget='tk')
        t.fire('rm')                        # its spot goes with it: the next test's spot is the only one
        t.wait(0.2)
        done(t)
        t.expect.spawned('@laser_spot', count=1)
        t.expect.distance('on', '@laser_spot', 'wallpt', 0, 4, flat=False)
        t.expect.value('on', '@laser_spot', 'effects', EF_NODRAW, op='!&')
        t.expect.value('off', '@laser_spot', 'effects', EF_NODRAW, op='&')
    case.__name__ = 'spot_' + short(cls)
    return case


# ---------------------------------------------------------------------------------------------- Match Target (128)
# The controller stands 80 units behind the tank (south of it, both facing north) and looks 30 degrees down and 30
# degrees left (yaw 120): his look meets the floor about 111 units away, at (-55, -104). Match Target aims the tank
# from its own origin at that point (yaw about 164); without it the tank takes his yaw (120).
def match_case(cls, flag):
    want = 164 if flag else 120

    @S.test('%s %s: the controller looks 30 degrees down and to the left; the tank turns to yaw %d (%s)' % (
                cls, 'Match Target (128)' if flag else 'without Match Target', want,
                'the point he looks at' if flag else 'his own yaw'),
            players=1, doc=DOC, covers=(('%s:128' % cls,) if flag else (cls,)))
    def case(t):
        controls(t, cls, flags=32 | (128 if flag else 0), yawrate='200', yawrange='180', pitchrate='200',
                 pitchrange='60')
        at_controls(t)
        t.fire('tk', activator='player1')
        t.wait(0.2)
        t.look('player1', 30, 120)
        t.wait(1.5)
        t.snap('s', 'tk', 'angles')
        t.fire('tk', activator='player1', use='off')
        t.wait(0.2)
        done(t)
        t.expect.value('s', 'tk', 'angles.y', want, tol=6)
    case.__name__ = 'match_%s_%d' % (short(cls), flag)
    return case


for _c in CLASSES:
    if _c != 'func_tank':
        master_case(_c)
    direct_case(_c)
    direct_off_case(_c)
    if _c != 'func_tank':
        spot_case(_c)
    match_case(_c, 1)
    match_case(_c, 0)


# ---------------------------------------------------------------------------------------------- Rotate Sound
@S.test('func_tankrocket Rotate Sound: it plays while the tank turns towards a player and stops once it faces him',
        players=1, doc=DOC, covers=('func_tankrocket.rotatesound',))
def rocket_rotatesound(t):
    tank(t, 'func_tankrocket', rotatesound='plats/rackmove1.wav')
    player_at(t, (200, 0, 0))
    t.wait(2.5)                         # facing him
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(5.0)
    done(t)
    t.expect.sound('plats/rackmove1.wav', ent='tk', after='north', within=(0, 1.0), min=1)
    t.expect.sound('plats/rackmove1.wav', ent='tk', flags=32, after='north', within=(2.0, 5.0), min=1)


# ---------------------------------------------------------------------------------------------- Player weapon volume
def volume_case(cls):
    @S.test('%s Player weapon volume -1: a player firing it at the controls makes no weapon sound for the monsters; '
            'with 0 he is loud (the combat sound of his shots)' % cls, players=1, doc=DOC,
            covers=('%s.player_weapon_volume' % cls,))
    def case(t):
        controls(t, cls, player_weapon_volume='-1', firerate='4')
        at_controls(t)
        t.wait(4.0)                     # a weapon volume of the test before (1000, 250 less a second) has run out
        t.fire('tk', activator='player1')
        t.wait(0.5)
        t.mark('quiet')
        t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
        t.wait(1.0)
        t.console('test_move %s 0 0 0' % t.ref('player1'))
        t.snap('q', 'player1', 'origin', 'aisounds')
        t.kv('tk', 'player_weapon_volume', '0')
        t.wait(0.3)
        t.mark('loud')
        t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
        t.wait(1.0)
        t.console('test_move %s 0 0 0' % t.ref('player1'))
        t.snap('l', 'player1', 'origin', 'aisounds')
        t.fire('tk', activator='player1', use='off')
        t.wait(0.2)
        done(t)
        t.expect.fired('shot', after='quiet', within=(0, 1.0), min=2)
        # the player's own AI sound (CBasePlayer::UpdatePlayerSound): bits_SOUND_PLAYER (4), with bits_SOUND_COMBAT
        # (1) while his weapon volume is above his body's
        t.expect.aisound('q', 'player1', 5, pos=(CONTROLLER_AT[0], CONTROLLER_AT[1], 36), tol=20, count=0)
        t.expect.aisound('l', 'player1', 5, pos=(CONTROLLER_AT[0], CONTROLLER_AT[1], 36), tol=20, min=1)
    case.__name__ = 'volume_' + short(cls)
    return case


for _c in CLASSES:
    volume_case(_c)


# ---------------------------------------------------------------------------------------------- func_tanklaser keys
L = 'func_tanklaser'


@S.test('func_tanklaser Yaw rate 30 / Yaw range 180: facing east, it turns towards a player to the north at 30 '
        'degrees a second', players=1, covers=(L + '.yawrate', L + '.yawrange'))
def laser_yaw_rate(t):
    tank(t, L, yawtolerance='5')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(1.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 18, op='>=')
    t.expect.value('s', 'tk', 'angles.y', 42, op='<=')


@S.test('func_tanklaser Yaw range 30: facing east, it turns towards a player to the north only 30 degrees',
        players=1, covers=(L + '.yawrange',))
def laser_yaw_range(t):
    tank(t, L, yawrate='90', yawrange='30')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.y', 30, tol=1.5)


@S.test('func_tanklaser Pitch rate 20 / Pitch range 10: aiming down at a player below it pitches 10 degrees and no '
        'more', players=1, covers=(L + '.pitchrate', L + '.pitchrange'))
def laser_pitch(t):
    tank(t, L, at=(-50, 0, 160), facing=0, pitchrate='20', pitchrange='10', firerate='0.2')
    player_at(t, (100, 0, 0))
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.x', 10, tol=1.0)


@S.test('func_tanklaser Yaw tolerance 15: turning at 30 degrees a second from east to a player to the north it fires '
        'no shot until it is within 15 degrees of him (2.5 s); Rate of Fire 2: about 2 shots a second after that',
        players=1, covers=(L + '.yawtolerance', L + '.firerate'))
def laser_tolerance_rate(t):
    tank(t, L, firerate='2')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(6.0)
    done(t)
    t.expect.fired('shot', after='north', within=(0.3, 2.0), count=0)
    t.expect.fired('shot', after='north', within=(3.5, 6.0), min=3)


@S.test('func_tanklaser Firing persistence 3: when a wall hides the player it keeps firing at where it saw him for '
        '3 s; persistence 0.5 stops after half a second', players=1, covers=(L + '.persistence',))
def laser_persistence(t):
    tank(t, L, firerate='2', persistence='3')
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


@S.test('func_tanklaser Pitch tolerance 5: it fires only once within 5 degrees of a player below (about 3 s at 10 '
        'degrees a second, once it has seen him)', players=1, covers=(L + '.pitchtolerance',))
def laser_pitch_tolerance(t):
    tank(t, L, at=(-50, 0, 160), firerate='2', pitchrate='10', pitchrange='60', pitchtolerance='5')
    player_at(t, (100, 0, 0))
    t.mark('in')
    t.wait(7.0)     # an idle tank looks for a target every 2 s
    done(t)
    t.expect.fired('shot', after='in', within=(0, 2.0), count=0)
    t.expect.fired('shot', after='in', within=(2.0, 7.0), min=1)


@S.test('func_tanklaser Minimum / Maximum target range 100 / 150: no shot at a player 200 or 60 away, shots at 120',
        players=1, covers=(L + '.minrange', L + '.maxrange'))
def laser_ranges(t):
    tank(t, L, firerate='2', minRange='100', maxRange='150')
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


@S.test('func_tanklaser Barrel Length 40 / Vertical 10 with a Flash Sprite at scale 2: its flash shows at the '
        'barrel\'s end, at scale 2', players=1,
        covers=(L + '.barrel', L + '.barrely', L + '.barrelz', L + '.spritescale'))
def laser_barrel(t):
    tank(t, L, firerate='4', barrel='40', barrely='0', barrelz='10', spriteflash='sprites/fire.spr', spritescale='2')
    t.point('info_target', targetname='muzzle', at=(TANK_AT[0] + 40, TANK_AT[1], TANK_AT[2] + 16 + 10))
    tracking_east(t)
    t.wait_for_fire('shot', 2.0)       # the flash is made with the shot
    t.snap('s', '@env_sprite', 'origin', 'scale')
    t.snap('s', 'muzzle', 'origin')
    done(t)
    t.expect.distance('s', '@env_sprite', 'muzzle', 0, 6, flat=False)
    t.expect.value('s', '@env_sprite', 'scale', 2, tol=0.01)


@S.test('func_tanklaser Barrel Horizontal 12: its flash shows 12 units to the side of the barrel\'s end', players=1,
        covers=(L + '.barrely',))
def laser_barrel_side(t):
    tank(t, L, firerate='4', barrel='40', barrely='12', barrelz='0', spriteflash='sprites/fire.spr')
    tracking_east(t)
    t.wait_for_fire('shot', 2.0)       # the flash is made with the shot
    t.snap('s', '@env_sprite', 'origin')
    t.snap('s', 'tk', 'origin', 'angles')
    done(t)
    # the barrel's end: 40 ahead and 12 to the side of the tank's origin (sqrt(40^2 + 12^2) = 41.8), wherever it points
    t.expect.distance('s', '@env_sprite', 'tk', 41, 43, flat=True)
