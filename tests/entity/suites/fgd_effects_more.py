"""Tier 5, final pass: more effect keys. env_smoker (made during the test: a placed one smokes at map load) puffs
Puffs times (one TE_SMOKE each, 0.1-0.2 s apart), at Smoke sprite scale, spread by Dispersion; env_render "FX Amount
factor" multiplies the FX Amount it gives; env_laser End Sprite puts that sprite at the laser's end."""

from etest import Suite

SUITE = Suite('fgd_effects_more', 'Tier 5: env_smoker, env_render factor, env_laser end sprite', players=1)
S = SUITE
DOC = 'FGD effect keys'
TE_SMOKE = 5


@S.test('env_smoker: Puffs 4 gives 4 smoke puffs (TE_SMOKE), Smoke sprite scale 3 their scale byte (3), '
        'Dispersion 0 all at its origin; then it removes itself', players=1, doc=DOC,
        covers=('env_smoker', 'env_smoker.health', 'env_smoker.scale', 'env_smoker.dmg'))
def smoker(t):
    p = t._abs((0, 100, 20))
    t.mark('go')
    t.console('test_create env_smoker %g %g %g 0 targetname %s health 4 scale 3 dmg 0' % (p[0], p[1], p[2],
                                                                                          t.name('sm')))
    t.wait(1.5)
    t.expect.msg('svc:23', fields={'te': TE_SMOKE, 'pos': [float(v) for v in p], 'scale': 3}, after='go', count=4)
    t.expect.removed('sm', after='go', count=1)


@S.test('env_render "FX Amount factor" 0.5: the target gets half its FX Amount (200 -> 100)', players=1, doc=DOC,
        covers=('env_render.message',))
def render_factor(t):
    t.brush('func_wall', targetname='w', size=(32, 32, 32), at=(0, 100, 0))
    t.point('env_render', targetname='r', target='w', renderamt='200', rendermode='2', message='0.5', spawnflags='0')
    t.wait(0.2)
    t.fire('r')
    t.wait(0.2)
    t.snap('s', 'w', 'renderamt')
    t.expect.value('s', 'w', 'renderamt', 100, tol=1)


@S.test('env_laser End Sprite: turned on, it shows that sprite at its end point', players=1, doc=DOC,
        covers=('env_laser.EndSprite',))
def laser_end_sprite(t):
    t.point('info_target', targetname='end', at=(0, 200, 60))
    t.point('env_laser', targetname='las', at=(0, -100, 60), LaserTarget='end', texture='sprites/laserbeam.spr',
            EndSprite='sprites/glow01.spr', renderamt='100', width='10', spawnflags='0', names=('LaserTarget',))
    t.wait(0.2)
    t.fire('las', use='on')
    t.wait(0.3)
    t.snap('s', '@env_sprite', 'model', 'origin', 'effects')
    t.expect.value('s', '@env_sprite', 'model', 'sprites/glow01.spr')
    t.expect.value('s', '@env_sprite', 'effects', 128, op='!&')
