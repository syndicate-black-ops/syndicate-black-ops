"""Tier 4 batch 5, real server: func_tankmortar keys (an Active mortar tank aiming at a test player), func_tankcontrols,
Prisoner (16) on monster_grunt_hunter / monster_dark, path_track 8 Disable train. Each
test checks the FGD's description (sdk/syndicate.fgd). Shots are counted by the tank's "Trigger on firing"."""

from etest import Suite

SUITE = Suite('map_used7', 'Tier 4 batch 5: func_tankmortar keys, func_tankcontrols, Prisoner, path_track Disable '
              'train')
S = SUITE

M = 'func_tankmortar'
TANK_AT = (0, 0, 48)
BASE = dict(yawrate='30', yawrange='180', yawtolerance='15', pitchrate='0', pitchrange='0', pitchtolerance='180',
            firerate='1', persistence='1', firespread='0', barrel='0', iMagnitude='1')
SF_PRISONER = 16
SF_TRACKTRAIN_NOCONTROL = 2


def mortar(t, facing=0, at=TANK_AT, **kv):
    keys = dict(BASE)
    keys.update(kv)
    t.brush(M, size=(32, 32, 32), at=at, origin=True, targetname='tk', spawnflags='1', angles='0 %d 0' % facing,
            m_iszLocusFire=t.name('shot'), **keys)


def player_at(t, at, yaw=180):
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', at, yaw=yaw)


def done(t):
    t.teleport('player1', (-200, -200, 0))
    t.flags('player1', '-FL_GODMODE')
    t.kv('player1', 'health', '100')
    t.kv('player1', 'max_health', '100')


def tracking_east(t):
    player_at(t, (200, 0, 0))
    t.wait(2.5)
    t.mark('in')


# ------------------------------------------------------------------------------------------------ func_tankmortar

@S.test('func_tankmortar Yaw rate 30 / Yaw range 30: facing east, it turns towards a player to the north at 30 degrees '
        'a second and stops at 30 degrees', players=1, covers=(M, M + '.yawrate', M + '.yawrange'))
def mortar_yaw(t):
    mortar(t, yawtolerance='5', yawrange='30')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.wait(0.5)
    t.snap('half', 'tk', 'angles')
    t.wait(2.0)
    t.snap('end', 'tk', 'angles')
    done(t)
    t.expect.value('half', 'tk', 'angles.y', 3, op='>=')
    t.expect.value('half', 'tk', 'angles.y', 22, op='<=')
    t.expect.value('end', 'tk', 'angles.y', 30, tol=1.5)


@S.test('func_tankmortar Pitch rate 20 / Pitch range 10: aiming down at a player below it pitches 10 degrees and no '
        'more', players=1, covers=(M + '.pitchrate', M + '.pitchrange'))
def mortar_pitch(t):
    mortar(t, at=(-50, 0, 160), pitchrate='20', pitchrange='10', firerate='0.2')
    player_at(t, (100, 0, 0))
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    done(t)
    t.expect.value('s', 'tk', 'angles.x', 10, tol=1.0)


@S.test('func_tankmortar Yaw tolerance 15 and Rate of Fire 2: turning from east to a player to the north it fires no '
        'shell until it is within 15 degrees of him (2.5 s), then about 2 a second', players=1,
        covers=(M + '.yawtolerance', M + '.firerate'))
def mortar_tolerance_rate(t):
    mortar(t, firerate='2')
    tracking_east(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.mark('north')
    t.wait(6.0)
    done(t)
    t.expect.fired('shot', after='north', within=(0.3, 2.0), count=0)
    t.expect.fired('shot', after='north', within=(3.5, 6.0), min=3)


@S.test('func_tankmortar Pitch tolerance 5: it fires only once within 5 degrees of a player below (about 3 s at 10 '
        'degrees a second, once it has seen him)', players=1, covers=(M + '.pitchtolerance',))
def mortar_pitch_tolerance(t):
    mortar(t, at=(-50, 0, 160), firerate='2', pitchrate='10', pitchrange='60', pitchtolerance='5')
    player_at(t, (100, 0, 0))
    t.mark('in')
    t.wait(9.0)
    done(t)
    t.expect.fired('shot', after='in', within=(0, 2.0), count=0)
    t.expect.fired('shot', after='in', within=(2.0, 9.0), min=1)


@S.test('func_tankmortar Firing persistence 3: when a wall hides the player it keeps firing for 3 s; 0.5 stops after '
        'half a second', players=1, covers=(M + '.persistence',))
def mortar_persistence(t):
    mortar(t, firerate='2', persistence='3')
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


@S.test('func_tankmortar Minimum / Maximum target range 100 / 150: no shell at a player 200 or 60 away, shells at 120',
        players=1, covers=(M + '.minrange', M + '.maxrange'))
def mortar_ranges(t):
    mortar(t, firerate='2', minRange='100', maxRange='150')
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


@S.test('func_tankmortar Barrel Length 40 / Vertical 10, Flash Sprite at scale 2: the flash shows at the barrel\'s end',
        players=1, covers=(M + '.barrel', M + '.barrely', M + '.barrelz', M + '.spriteflash', M + '.spritescale'))
def mortar_barrel(t):
    mortar(t, firerate='10', barrel='40', barrely='0', barrelz='10', spriteflash='sprites/fire.spr', spritescale='2')
    t.point('info_target', targetname='muzzle', at=(TANK_AT[0] + 40, TANK_AT[1], TANK_AT[2] + 16 + 10))
    tracking_east(t)
    t.wait(0.5)
    t.snap('s', '@env_sprite', 'origin', 'scale')
    t.snap('s', 'muzzle', 'origin')
    done(t)
    t.expect.distance('s', '@env_sprite', 'muzzle', 0, 6, flat=False)
    t.expect.value('s', '@env_sprite', 'scale', 2, tol=0.01)


@S.test('func_tankmortar Explosion Magnitude (iMagnitude 50): its shell blows up where it aims (an env_explosion), '
        'hurting the player with blast damage', players=1, covers=(M + '.imagnitude',))
def mortar_magnitude(t):
    mortar(t, firerate='1', iMagnitude='50')
    tracking_east(t)
    t.wait(2.0)
    done(t)
    t.expect.damaged('player1', attacker='@env_explosion', after='in', within=(0, 2.0), type=64)


# ------------------------------------------------------------------------------------------------ func_tankcontrols

@S.test('func_tankcontrols: a player who uses the controls (+use looking at them) takes the tank\'s controls',
        players=1, covers=('func_tankcontrols', 'func_tankcontrols.target'))
def tank_controls(t):
    t.brush('func_tank', size=(32, 32, 32), at=(0, 100, 48), origin=True, targetname='gun', angles='0 90 0',
            yawrate='30', yawrange='180', firerate='1', bullet='1', persistence='1', yawtolerance='15',
            pitchtolerance='5', spawnflags='32')
    t.brush('func_tankcontrols', size=(32, 32, 48), at=(0, 40, 0), target='gun')
    t.teleport('player1', (0, 0, 0), yaw=90)        # in front of the controls, looking at them
    t.look('player1', 0, 90)
    t.wait(1.0)
    t.move('player1', 0, 0, 2, '+use')
    t.wait(0.3)
    t.snap('s', 'player1', 'tank')
    t.move('player1', 0, 0, 2, '+use')      # let go
    t.wait(0.3)
    t.expect.value('s', 'player1', 'tank', 1)


# ------------------------------------------------------------------------------------------------ Prisoner

def make_prisoner_test(cls):
    def prisoner(t):
        # different squad names: no squad (a squad shares its enemies)
        t.point(cls, targetname='pris', at=(-100, 100, 0), angles='0 270 0', spawnflags=str(SF_PRISONER),
                netname=t.name('sq1'))
        t.point(cls, targetname='free', at=(100, 100, 0), angles='0 270 0', netname=t.name('sq2'))
        t.kv('player1', 'health', '100000')
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -150, 0), yaw=90)
        t.wait(2.0)
        t.snap('s', 'pris', 'enemy')
        t.snap('s', 'free', 'enemy')
        t.point('trigger_relay', targetname='rm', killtarget='free')
        t.fire('rm')
        t.flags('player1', '-FL_GODMODE')
        t.kv('player1', 'health', '100')
        t.teleport('player1', (-220, -220, 0))
        t.expect.value('s', 'pris', 'enemy', '')
        t.expect.value('s', 'free', 'enemy', 'player')

    prisoner.__name__ = 'prisoner_' + cls
    S.test('%s flag 16 Prisoner: it takes a player in front of it as no enemy; the same monster without the flag does'
           % cls, players=1, covers=(cls + ':16',))(prisoner)


for _c in ('monster_grunt_hunter', 'monster_dark'):
    make_prisoner_test(_c)


# ------------------------------------------------------------------------------------------------ path_track 8

@S.test('path_track flag 8 Disable train: a func_tracktrain passing it can no longer be driven by players (its flag 2 '
        'No User Control is set); before it, it could', covers=('path_track:8',))
def path_disable_train(t):
    t.point('path_track', at=(-200, 0, 32), targetname='p1', target='p2')
    t.point('path_track', at=(0, 0, 32), targetname='p2', target='p3', spawnflags='8')
    t.point('path_track', at=(200, 0, 32), targetname='p3')
    t.brush('func_tracktrain', size=(64, 32, 32), at=(-200, 0, 32), origin=True, targetname='tt', target='p1',
            speed='200', height='0', wheels='16')
    t.wait(0.3)
    t.fire('tt', use='on')
    t.wait(0.3)
    t.snap('before', 'tt', 'spawnflags')
    t.wait(2.5)
    t.snap('after', 'tt', 'spawnflags')
    t.expect.value('before', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='!&')
    t.expect.value('after', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='&')
