"""Tier 4 batch 3: map-used items that need a real server: env_beam Radius (random strikes need walls), the
nuclear_explosion Shake radius (a player on the ground), scripted_sequence flag 64 Override AI, func_train Contents,
env_shooter keys. Each test checks the FGD's description (sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('map_used4', 'Tier 4 batch 3: env_beam radius, nuke shake radius, scripted Override AI, func_train '
              'contents, env_shooter')
S = SUITE

TE_BEAMPOINTS = 0
SOLID_BSP = 4


# ------------------------------------------------------------------------------------------------ env_beam Radius

@S.test('env_beam with only a Start Entity strikes from it to a random point up to Radius away where it meets a wall: '
        'Radius 400 reaches the room\'s walls (strikes), Radius 100 reaches none (no strike)',
        covers=('env_beam.radius',))
def beam_radius(t):
    t.point('info_target', targetname='far_src', at=(0, 0, 100))
    t.point('env_beam', targetname='far', LightningStart='far_src', Radius='400', life='0.5', StrikeTime='0.2',
            BoltWidth='10', renderamt='100', texture='sprites/laserbeam.spr', spawnflags='1', at=(0, 0, 100))
    t.mark('on')
    t.wait(3.0)
    t.fire('far')       # off
    t.point('info_target', targetname='near_src', at=(0, 0, 100))
    t.point('env_beam', targetname='near', LightningStart='near_src', Radius='100', life='0.5', StrikeTime='0.2',
            BoltWidth='10', renderamt='100', texture='sprites/laserbeam.spr', at=(0, 0, 100))
    t.mark('near')
    t.fire('near')
    t.wait(3.0)
    t.fire('near')
    t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS}, after='on', within=(0, 3.0), min=2)
    t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS}, after='near', within=(0.1, 3.0), count=0)


# ------------------------------------------------------------------------------------------------ nuclear_explosion v13

@S.test('nuclear_explosion Shake radius (v13): a player on the ground within it gets a ScreenShake; one outside it '
        'does not', players=1, covers=('nuclear_explosion.v13',))
def nuke_shake(t):
    t.point('nuclear_explosion', targetname='wide', at=(0, 200, 0), v11='0', v12='0', v13='400', v8='10')
    t.point('nuclear_explosion', targetname='narrow', at=(0, 200, 0), v11='0', v12='0', v13='100', v8='10')
    t.teleport('player1', (0, -100, 0))
    t.wait(0.5)
    t.mark('wide')
    t.fire('wide')
    t.wait(0.5)
    t.mark('narrow')
    t.fire('narrow')
    t.wait(0.5)
    t.expect.msg('ScreenShake', to='player1', count=1, after='wide', within=(0, 0.3))
    t.expect.msg('ScreenShake', to='player1', count=0, after='narrow', within=(0, 0.4))


# ------------------------------------------------------------------------------------------------ scripted 64

def grunt_in_combat(t, flags):
    t.point('monster_human_grunt', targetname='g', at=(0, 150, 0), angles='0 270 0')
    t.point('scripted_sequence', targetname='ss', at=(0, 150, 0), angles='0 270 0', m_iszEntity='g',
            m_iszPlay='idle1', m_fMoveTo='0', spawnflags=str(flags), names=('m_iszEntity',))
    t.kv('player1', 'health', '100000')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(2.0)                     # it sees him and fights
    t.snap('fight', 'g', 'state', 'enemy')
    t.fire('ss')
    t.wait(0.5)
    t.snap('script', 'g', 'cine')
    t.point('trigger_relay', targetname='rm', killtarget='g')
    t.fire('rm')
    t.kv('player1', 'health', '100')
    t.teleport('player1', (-200, -200, 0))


@S.test('scripted_sequence flag 64 Override AI: a grunt fighting a player is taken by the script at once', players=1,
        covers=('scripted_sequence:64',))
def script_override(t):
    grunt_in_combat(t, 64)
    t.expect.value('fight', 'g', 'enemy', 'player')
    t.expect.value('script', 'g', 'cine', 1)


@S.test('scripted_sequence without flag 64: a grunt fighting a player is not taken by the script', players=1,
        covers=('scripted_sequence',))
def script_no_override(t):
    grunt_in_combat(t, 0)
    t.expect.value('fight', 'g', 'enemy', 'player')
    t.expect.value('script', 'g', 'cine', 0)


# ------------------------------------------------------------------------------------------------ func_train Contents

@S.test('func_train Contents 0 and -1 (Empty: the values the maps use): the train is a solid mover, a player walking '
        'into it is stopped', players=1, covers=('func_train.skin',))
def train_contents(t):
    for x, name, skin in ((-120, 'c0', '0'), (120, 'c1', '-1')):
        t.point('path_corner', targetname=name + 'p', at=(x, 0, 0))
        t.brush('func_train', size=(96, 32, 96), at=(x, 0, 0), origin=True, targetname=name, target=name + 'p',
                speed='50', skin=skin)
        t.point('info_target', targetname=name + 'beyond', at=(x, 150, 0))
    t.wait(0.5)
    t.snap('s', 'c0', 'solid')
    t.snap('s', 'c1', 'solid')
    for name, x in (('c0', -120), ('c1', 120)):
        t.teleport('player1', (x, -100, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait(0.3)
        t.walk('player1', 250, 0, 1.5)
        t.snap('walked_' + name, 'player1', 'origin')
        t.snap('walked_' + name, name + 'beyond', 'origin')
    t.expect.value('s', 'c0', 'solid', SOLID_BSP)
    t.expect.value('s', 'c1', 'solid', SOLID_BSP)
    t.expect.distance('walked_c0', 'player1', 'c0beyond', 150, 400)
    t.expect.distance('walked_c1', 'player1', 'c1beyond', 150, 400)


# ------------------------------------------------------------------------------------------------ env_shooter

@S.test('env_shooter: triggered it shoots Number of Gibs (3) of its model, one every Delay between shots (0.5 s), at '
        'Gib Velocity (300, Course Variance 0) with its Gib Skin and scale', covers=(
            'env_shooter.shootmodel', 'env_shooter.delay', 'env_shooter.m_flvelocity', 'env_shooter.m_flvariance',
            'env_shooter.skin', 'env_shooter.scale', 'env_shooter.m_igibs'))
def shooter_keys(t):
    t.point('env_shooter', at=(0, 0, 150), targetname='shoot', m_iGibs='3', delay='0.5', m_flVelocity='300',
            m_flVariance='0', m_flGibLife='5', shootmodel='models/hgibs.mdl', skin='1', scale='2', angles='0 0 0',
            shootsounds='-1')
    t.wait(0.2)
    t.mark('go')
    t.fire('shoot')
    t.wait(0.05)
    t.snap('first', '@gib', 'velocity', 'skin', 'scale')
    t.wait(0.25)
    t.wait(1.0)
    t.wait(1.0)         # silent gibs (Material Sound -1): they do not disturb the next tests
    t.expect.spawned('@gib', count=3)
    t.expect.spawned('@gib', count=1, after='go', within=(0, 0.3))
    t.expect.spawned('@gib', count=2, after='go', within=(0.3, 1.3))
    t.expect.value('first', '@gib', 'velocity.x', 300, tol=40)
    t.expect.value('first', '@gib', 'skin', 1)
    t.expect.value('first', '@gib', 'scale', 2, tol=0.01)


def make_shooter_sound_test(value, sample):
    def shooter_sound(t):
        t.point('env_shooter', at=(0, 0, 100), targetname='shoot', m_iGibs='4', delay='0.1', m_flVelocity='100',
                m_flVariance='0.2', m_flGibLife='1', shootmodel='models/hgibs.mdl', shootsounds=str(value),
                angles='0 0 0')
        t.wait(0.2)
        t.mark('go')
        t.fire('shoot')
        t.wait(6.0)         # they land and fade (life 1 s) before the next test
        if sample:
            t.expect.sound(sample, after='go', within=(0, 4.0))
        else:
            for s in ('debris/glass', 'debris/wood', 'debris/metal', 'debris/flesh', 'debris/concrete'):
                t.expect.sound(s, count=0, after='go', within=(0, 4.0))

    shooter_sound.__name__ = 'shooter_sound_%d' % (value + 1)
    S.test('env_shooter Material Sound %d: its gibs bounce on the floor %s' % (
        value, 'with %s sounds' % sample if sample else 'without a sound'),
        covers=('env_shooter.shootsounds', 'env_shooter.shootsounds=%d' % value))(shooter_sound)


for _v, _s in ((-1, None), (2, 'debris/metal'), (3, 'debris/flesh')):
    make_shooter_sound_test(_v, _s)


# ------------------------------------------------------------------------------------------------ env_laser

@S.test('env_laser (Start On, EndSparks): a beam to its Target of Laser with its Width, Amount of noise, Brightness, '
        'Beam Color and Sprite Name; it hurts a player in it by Damage / second (20; the beam crosses his stomach: '
        'times 3, as any hit there) and sparks at its end (TE_SPARKS)', players=1,
        covers=('env_laser', 'env_laser:1', 'env_laser:32', 'env_laser.lasertarget', 'env_laser.width',
                'env_laser.noiseamplitude', 'env_laser.renderamt', 'env_laser.rendercolor', 'env_laser.texture',
                'env_laser.damage'))
def laser(t):
    t.point('info_target', targetname='lt', at=(200, 0, 40))
    t.point('env_laser', targetname='las', at=(-200, 0, 40), LaserTarget='lt', width='30', NoiseAmplitude='5',
            renderamt='150', rendercolor='255 0 0', texture='sprites/laserbeam.spr', TextureScroll='40',
            damage='20', spawnflags='33')
    t.kv('player1', 'health', '100000')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, 0, 4))
    t.mark('in')
    t.wait(1.0)
    t.snap('s', 'las', 'scale', 'body', 'renderamt', 'rendercolor', 'model')
    t.teleport('player1', (-200, -200, 0))
    t.flags('player1', '-FL_GODMODE')
    t.kv('player1', 'health', '100')
    t.fire('las')       # off
    t.expect.damaged('player1', attacker='las', after='in', within=(0, 1.0), total=(40, 80))
    t.expect.msg('svc:23', fields={'te': 9}, after='in', within=(0, 1.0), min=1)
    t.expect.value('s', 'las', 'scale', 30)
    t.expect.value('s', 'las', 'body', 5)
    t.expect.value('s', 'las', 'renderamt', 150)
    t.expect.value('s', 'las', 'rendercolor', [255, 0, 0])
    t.expect.value('s', 'las', 'model', 'sprites/laserbeam.spr')
