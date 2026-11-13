"""Tier 5, batch 8: gibshooter and env_shooter FGD items that no 1.38 map uses and no other test asserted (Featureful /
Spirit keys, docs/LOCUS.md for [LP] / [LV] / [LR]): number of gibs, delay, life, position, direction and speed, course
variance, Fire on spawn, Repeatable, Start instantly; env_shooter shot physics, frame, friction, size, Fire on collision,
material sounds, start gib body. Each test waits for the gibs of the test before it to be gone."""

from etest import Suite

SUITE = Suite('fgd_shooter', 'Tier 5: gibshooter / env_shooter FGD-only keys', players=1)
S = SUITE
DOC = 'LOCUS.md'
MOVETYPE_TOSS, MOVETYPE_FLYMISSILE, MOVETYPE_BOUNCE, MOVETYPE_NOCLIP, MOVETYPE_BOUNCEMISSILE = 6, 9, 10, 8, 11
NAMES = ('m_iszPosition', 'm_iszVelocity', 'm_iszSpawnTarget', 'm_iszTouchOther')


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def tagger(t, name='tag', proj='proj'):
    """Fired on spawn: names the new gib / shot `proj`, so the test snapshots its own (others may still fly)."""
    t.point('trigger_changevalue', targetname=name, target='!activator', m_iszValueName='targetname',
            m_iszNewValue=t.name(proj), m_iszValueType='0')


# ---------------------------------------------------------------------------------------------- gibshooter
@S.test('gibshooter Number of Gibs 3, Delay between shots 0.2, Fire on spawn: three gibs 0.2 s apart, each fires the '
        'target as its activator', doc=DOC,
        covers=('gibshooter.m_iGibs', 'gibshooter.delay', 'gibshooter.m_iszSpawnTarget'))
def gib_count(t):
    t.point('gibshooter', targetname='gs', at=(0, 0, 100), m_iGibs='3', delay='0.2', m_flGibLife='0.5',
            m_iszSpawnTarget='sp', names=NAMES)
    info(t, 'sp')
    t.wait(1.0)
    t.mark('go')
    t.fire('gs')
    t.wait(0.9)
    t.expect.used('sp', activator='@gib', count=3, after='go')
    t.expect.used('sp', count=1, after='go', within=(0.15, 0.3))
    t.expect.used('sp', count=1, after='go', within=(0.35, 0.5))


@S.test('gibshooter Position [LP], Direction [LV] and Gib Velocity [LR], Course Variance 0: the gib starts at the '
        'spot and flies straight at velocity x direction', doc=DOC,
        covers=('gibshooter.m_iszPosition', 'gibshooter.m_iszVelocity', 'gibshooter.m_flVelocity',
                'gibshooter.m_flVariance'))
def gib_direction(t):
    info(t, 'spot', at=(-100, 50, 120))
    t.point('gibshooter', targetname='gs', at=(0, 0, 100), m_iGibs='1', m_flGibLife='0.5', m_iszPosition='spot',
            m_iszVelocity='0 1 0', m_flVelocity='300', m_flVariance='0', m_iszSpawnTarget='tag', names=NAMES)
    tagger(t)
    t.wait(0.5)
    t.fire('gs')
    t.wait_frames(1)
    t.snap('s', 'proj', 'origin', 'velocity')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'proj', 'spot', 0, 20, flat=False)
    t.expect.value('s', 'proj', 'velocity.x', 0, tol=5)
    t.expect.value('s', 'proj', 'velocity.y', 290, op='>')


@S.test('gibshooter Gib Life 0.4: the gib is removed about 0.4 s after it lands', doc=DOC,
        covers=('gibshooter.m_flGibLife',))
def gib_life(t):
    t.point('gibshooter', targetname='gs', at=(0, 0, 20), m_iGibs='1', m_flGibLife='0.4', m_flVelocity='10',
            m_iszSpawnTarget='sp', names=NAMES, angles='90 0 0')
    info(t, 'sp')
    t.wait(1.5)
    t.mark('go')
    t.fire('gs')
    t.wait(2.5)
    t.expect.removed('@gib', after='go', within=(0.2, 2.0), min=1)


@S.test('gibshooter Repeatable (1): a second trigger shoots again; without it the shooter is removed after its gibs',
        doc=DOC, covers=('gibshooter:1',))
def gib_repeat(t):
    t.point('gibshooter', targetname='rep', at=(-80, 0, 100), m_iGibs='1', m_flGibLife='0.3', spawnflags='1',
            m_iszSpawnTarget='sp_rep', names=NAMES)
    t.point('gibshooter', targetname='one', at=(80, 0, 100), m_iGibs='1', m_flGibLife='0.3',
            m_iszSpawnTarget='sp_one', names=NAMES)
    info(t, 'sp_rep')
    info(t, 'sp_one')
    t.wait(1.0)
    t.fire('rep')
    t.fire('one')
    t.wait(0.5)
    t.fire('rep')
    t.fire('one')
    t.wait(0.5)
    t.expect.used('sp_rep', count=2)
    t.expect.used('sp_one', count=1)
    t.expect.removed('one')


@S.test('gibshooter / env_shooter Start instantly (8): with Delay between shots 1, the first gib comes at once (in '
        'the trigger\'s own frame; without the flag at the next think, also before the delay)', doc=DOC,
        covers=('gibshooter:8', 'env_shooter:8'))
def start_instantly(t):
    for n, x, cls, fl in (('gi', -150, 'gibshooter', '8'), ('gd', -50, 'gibshooter', '0'),
                          ('ei', 50, 'env_shooter', '8'), ('ed', 150, 'env_shooter', '0')):
        kv = {'shootmodel': 'models/hgibs.mdl'} if cls == 'env_shooter' else {}
        t.point(cls, targetname=n, at=(x, 0, 100), m_iGibs='1', delay='1', m_flGibLife='0.3', spawnflags=fl,
                m_iszSpawnTarget='sp_' + n, names=NAMES, **kv)
        info(t, 'sp_' + n)
    t.wait(0.2)
    t.mark('go')
    for n in ('gi', 'gd', 'ei', 'ed'):
        t.fire(n)
    t.wait(1.3)
    t.expect.used('sp_gi', count=1, after='go', within=(0, 0.1))
    t.expect.used('sp_ei', count=1, after='go', within=(0, 0.1))
    t.expect.used('sp_gd', count=1, after='go', within=(0, 0.2))
    t.expect.used('sp_ed', count=1, after='go', within=(0, 0.2))


# ---------------------------------------------------------------------------------------------- env_shooter
PHYSICS = {0: ('gib', MOVETYPE_BOUNCE), 1: ('gib', MOVETYPE_TOSS), 2: ('shot', MOVETYPE_NOCLIP),
           3: ('shot', MOVETYPE_FLYMISSILE), 4: ('shot', MOVETYPE_BOUNCEMISSILE), 6: ('shot', MOVETYPE_BOUNCE)}


def physics_case(p):
    cls, mt = PHYSICS[p]

    @S.test('env_shooter Gib/Shot physics %d: a %s with movetype %d' % (p, cls, mt), doc=DOC,
            covers=('env_shooter.m_iPhysics=%d' % p,))
    def case(t):
        t.point('env_shooter', targetname='es', at=(0, 0, 100), m_iGibs='1', m_flGibLife='0.4',
                shootmodel='sprites/glow01.spr' if p >= 2 else 'models/hgibs.mdl', m_iPhysics=str(p),
                m_flVelocity='100', m_iszSpawnTarget='tag', names=NAMES)
        tagger(t)
        t.wait(0.5)
        t.fire('es')
        t.wait_frames(2)
        t.snap('s', 'proj', 'movetype', 'classname')
        t.expect.value('s', 'proj', 'movetype', mt)
        t.expect.value('s', 'proj', 'classname', cls)
    case.__name__ = 'physics_%d' % p
    return case


for _p in sorted(PHYSICS):
    physics_case(_p)


@S.test('env_shooter shot keys (Fly shot): Start frame 2, Bounce friction 0.3, Shot size 4 4 4 (a box of 4: -2 to 2); '
        'Fire on collision (locus = wall) when it hits a func_wall', doc=DOC,
        covers=('env_shooter.frame', 'env_shooter.m_fFriction', 'env_shooter.m_vecSize', 'env_shooter.m_iszTouchOther',
                'env_shooter.m_iszVelocity', 'env_shooter.m_iszPosition'))
def shot_keys(t):
    info(t, 'spot', at=(0, 0, 80))
    t.brush('func_wall', targetname='wall', size=(128, 16, 160), at=(0, 150, 0))
    t.point('env_shooter', targetname='es', at=(0, -100, 80), m_iGibs='1', m_flGibLife='3',
            shootmodel='sprites/glow01.spr', m_iPhysics='3', frame='2', framerate='0', m_fFriction='0.3',
            m_vecSize='4 4 4', m_iszTouchOther='hitwall', m_iszPosition='spot', m_iszVelocity='0 1 0',
            m_flVelocity='400', m_flVariance='0', m_iszSpawnTarget='tag', names=NAMES)
    tagger(t)
    info(t, 'hitwall')
    t.wait(0.5)
    t.fire('es')
    t.wait_frames(1)
    t.snap('s', 'proj', 'frame', 'friction', 'mins', 'maxs', 'origin')
    t.snap('s', 'spot', 'origin')
    t.wait(1.0)
    t.expect.value('s', 'proj', 'frame', 2)
    t.expect.value('s', 'proj', 'friction', 0.3)
    t.expect.value('s', 'proj', 'mins', [-2, -2, -2])
    t.expect.value('s', 'proj', 'maxs', [2, 2, 2])
    t.expect.distance('s', 'proj', 'spot', 0, 60)
    t.expect.used('hitwall', activator='wall', min=1)


@S.test('env_shooter Fire on collision (locus = wall) when the shot hits a wall of the map (the world): fired with '
        'the shot as activator (the world is no locus), at the hit point', doc=DOC,
        covers=('env_shooter.m_iszTouchOther',))
def shot_world(t):
    info(t, 'spot', at=(0, 0, 80))
    t.point('env_shooter', targetname='es', at=(0, -100, 80), m_iGibs='1', m_flGibLife='3',
            shootmodel='sprites/glow01.spr', m_iPhysics='3', m_iszTouchOther='hitwall', m_iszPosition='spot',
            m_iszVelocity='0 1 0', m_flVelocity='400', m_flVariance='0', names=NAMES)
    info(t, 'hitwall')
    t.wait(1.0)
    t.fire('es')
    t.wait(1.5)
    t.expect.used('hitwall', activator='@shot', min=1)


SOUNDS = {0: 'debris/glass', 1: 'debris/wood', 4: 'debris/concrete'}


def sound_case(m):
    @S.test('env_shooter Material Sound %d: a gib bouncing on the floor plays %s*' % (m, SOUNDS[m]), doc=DOC,
            covers=('env_shooter.shootsounds=%d' % m,))
    def case(t):
        t.point('env_shooter', targetname='es', at=(0, 0, 150), angles='-60 0 0', m_iGibs='3', m_flGibLife='1',
                shootmodel='models/hgibs.mdl', shootsounds=str(m), m_flVelocity='300')
        t.wait(1.5)
        t.mark('go')
        t.fire('es')
        t.wait(2.0)
        t.expect.sound(SOUNDS[m], after='go', min=1)
    case.__name__ = 'sound_%d' % m
    return case


for _m in sorted(SOUNDS):
    sound_case(_m)


@S.test('env_shooter Start gib body 3: the gib\'s submodel is 3 or later (hgibs.mdl)', doc=DOC,
        covers=('env_shooter.start_gib_body',))
def gib_body(t):
    t.point('env_shooter', targetname='es', at=(0, 0, 100), m_iGibs='1', m_flGibLife='0.5',
            shootmodel='models/hgibs.mdl', body='4', start_gib_body='3', m_flVelocity='50', m_iszSpawnTarget='tag',
            names=NAMES)
    tagger(t)
    t.wait(0.5)
    t.fire('es')
    t.wait_frames(2)
    t.snap('s', 'proj', 'body')
    t.expect.value('s', 'proj', 'body', 3, op='>=')


@S.test('env_shooter "Framerate (shot)" 15: the shot (Fly shot of a sprite) plays at framerate 15', doc=DOC,
        covers=('env_shooter.framerate',))
def shot_framerate(t):
    t.point('env_shooter', targetname='es', at=(0, -100, 80), m_iGibs='1', m_flGibLife='2',
            shootmodel='sprites/fire.spr', m_iPhysics='2', framerate='15', m_iszVelocity='0 1 0', m_flVelocity='50',
            m_flVariance='0', m_iszSpawnTarget='tag', names=NAMES)
    tagger(t)
    t.wait(0.5)
    t.fire('es')
    t.wait_frames(1)
    t.snap('s', 'proj', 'framerate')
    t.expect.value('s', 'proj', 'framerate', 15)


@S.test('env_shooter "Don\'t wait till land" (4): a gib thrown up still flying starts fading at its life (0.3 s); '
        'without the flag the flying gib waits to land', size=(512, 512, 1024), doc=DOC, covers=('env_shooter:4',))
def dont_wait_land(t):
    for name, x, fl in (('wait', -100, '0'), ('nowait', 100, '4')):
        t.point('env_shooter', targetname='es_' + name, at=(x, 0, 100), m_iGibs='1', m_flGibLife='0.3',
                shootmodel='models/hgibs.mdl', m_iszVelocity='0 0 1', m_flVelocity='600', m_flVariance='0',
                spawnflags=fl, m_iszSpawnTarget='tag_' + name, names=NAMES)
        tagger(t, 'tag_' + name, 'gib_' + name)
    t.wait(0.5)
    t.fire('es_wait')
    t.fire('es_nowait')
    t.wait(0.6)
    t.snap('s', 'gib_wait', 'rendermode', 'velocity')
    t.snap('s', 'gib_nowait', 'rendermode', 'velocity')
    t.expect.value('s', 'gib_nowait', 'velocity.z', 0, op='!=')       # still in the air
    t.expect.value('s', 'gib_nowait', 'rendermode', 2)                # kRenderTransTexture: fading
    t.expect.value('s', 'gib_wait', 'velocity.z', 0, op='!=')
    t.expect.value('s', 'gib_wait', 'rendermode', 0)                  # not yet
