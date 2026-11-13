"""Tier 5, batch 8: monstermaker FGD items that no 1.38 map uses and no other test asserted (docs/MONSTERMAKER.md,
Featureful's maker keys): Target's Activator, Children's classification (seen through a trigger_sensor's
relationship), weapons / gag / render override left at No, children's path, face position, warpball position, make
blocker move away. The player is notarget so the children stay idle."""

from etest import Suite

SUITE = Suite('fgd_maker', 'Tier 5: monstermaker Featureful keys', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
TE_BEAMPOINTS = 0
NAMES = ('netname', 'target', 'face_position')

# CBaseMonster::IDefaultRelationship rows / columns (R_AL -2, R_FR -1, R_NO 0, R_DL 1, R_HT 2, R_NM 3)
REL = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, -2, 1, 1, -2, 1, 1, 1, 1, 1, 2, 1, 1, 1],
    [0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1],
    [0, -1, -1, -2, 0, -1, 0, 2, 1, -1, 2, -2, 0, 0],
    [0, -2, 2, 0, -2, 2, 1, 1, 1, 1, 2, 2, 0, 0],
    [0, 1, 2, 1, 2, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, 1, 3, 1, 1, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 1, 1, 1, 1, 0, 0, 0, 0, -1, 2, 1, 0, 0],
    [0, 1, 3, 2, 2, 0, 0, 0, 2, 1, 2, 1, 0, 0],
    [2, 2, 2, 2, -1, 2, 2, 2, 2, 2, 2, 2, 2, 2],
    [0, 1, -2, -2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 1],
    [0, 0, 1, 1, 1, -2, 0, 1, 1, 0, 2, 1, 1, 0],
]
SCIENTIST = 3                # monster_scientist's own class (human passive)
SENSOR = 4                   # the sensor acts as human military


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def maker(t, name, at=(0, 0, 0), kid='kid', monstertype='monster_scientist', names=(), **kv):
    keys = dict(monstertype=monstertype, monstercount='1', m_imaxlivechildren='1', delay='0.1', netname=kid)
    keys.update(kv)
    t.point('monstermaker', targetname=name, at=at, angles='0 0 0', names=NAMES + tuple(names), **keys)


# ---------------------------------------------------------------------------------------------- target activator
@S.test('monstermaker Target\'s Activator -1 (none), 0 (default) and 1 (the maker): Target On Release fires with no '
        'activator / the maker / the maker', players=1, doc=DOC,
        covers=('monstermaker.target_activator=-1', 'monstermaker.target_activator=0',
                'monstermaker.target_activator=1'))
def target_activator(t):
    t.flags('player1', '+FL_NOTARGET')
    for i, ta in enumerate(('-1', '0', '1')):
        maker(t, 'mk%d' % i, at=(-150 + i * 150, 100, 0), kid='kid%d' % i, target='rel%d' % i, target_activator=ta)
        info(t, 'rel%d' % i)
    t.wait(0.2)
    for i in range(3):
        t.fire('mk%d' % i)
    t.wait(0.5)
    t.expect.fired('rel0', count=1)
    t.expect.fired('rel0', activator='mk0', count=0)
    t.expect.fired('rel1', activator='mk1', caller='mk1', count=1)
    t.expect.fired('rel2', activator='mk2', caller='mk2', count=1)


# ---------------------------------------------------------------------------------------------- classify
CLASSES = (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13)
TANK = 4                     # an Active func_tank with Behavior human military shoots what that class dislikes or worse


def classify_case(c):
    cls = {-1: 0, 0: SCIENTIST}.get(c, c)
    hated = REL[TANK][cls] >= 1

    @S.test('monstermaker Children\'s classification %d: the scientist child fights as that class (an Active func_tank '
            'with Behavior human military %s it)' % (c, 'shoots at' if hated else 'leaves'), players=1, doc=DOC,
            covers=('monstermaker.classify', 'monstermaker.classify=%d' % c))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        maker(t, 'mk', at=(0, 100, 0), classify=str(c), spawnflags='16')
        t.brush('func_tank', size=(32, 32, 32), at=(0, -40, 40), origin=True, targetname='tk', target='shot',
                spawnflags='1', angles='0 90 0', yawrate='200', yawrange='60', pitchrate='100', pitchrange='30',
                barrel='24', firerate='4', yawtolerance='15', pitchtolerance='15', persistence='1', maxRange='200',
                bullet='1', bullet_damage='0', m_iClass=str(TANK))
        info(t, 'shot')
        t.wait(0.2)
        t.fire('mk')
        t.mark('made')
        t.wait(2.5)
        if hated:
            t.expect.used('shot', after='made', min=1)
        else:
            t.expect.not_used('shot', after='made')
    case.__name__ = 'classify_%d' % (c + 1)
    return case


for _c in CLASSES:
    classify_case(_c)


@S.test('trigger_sensor relationship with a monster\'s "classify" key: a scientist keyed Alien military (5) is an '
        'enemy of a sensor acting as human military', players=1, doc=DOC, covers=('trigger_sensor.relationship_filter=2',))
def sensor_keyed(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sci', at=(0, 60, 0), classify='5', spawnflags='16')
    info(t, 'hit', at=(0, 160, 40))
    t.point('trigger_sensor', targetname='s', at=(0, 60, 40), target='hit', radius='30',
            sensed_classname='monster_scientist', line_blocked='0', trace_to='0', fire_trigger_policy='0',
            alive_filter='0', takedamage_filter='0', classify='4', relationship_filter='2')
    t.wait(0.3)
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', activator='sci', count=1)


@S.test('trigger_sensor relationship with a monstermaker\'s Children\'s classification: a scientist child made as Alien '
        'military (5) is an enemy of a sensor acting as human military; one made as Human passive (3) is not',
        players=1, doc=DOC, covers=('trigger_sensor.relationship_filter=2',))
def sensor_maker_child(t):
    t.flags('player1', '+FL_NOTARGET')
    for i, c in enumerate(('5', '3')):
        x = -100 + i * 200
        maker(t, 'mk%d' % i, at=(x, 60, 0), kid='kid%d' % i, classify=c, spawnflags='16')
        info(t, 'hit%d' % i, at=(x, 160, 40))
        t.point('trigger_sensor', targetname='s%d' % i, at=(x, 60, 40), target='hit%d' % i, radius='30',
                sensed_classname='monster_scientist', line_blocked='0', trace_to='0', fire_trigger_policy='0',
                alive_filter='0', takedamage_filter='0', classify='4', relationship_filter='2')
    t.wait(0.2)
    t.fire('mk0')
    t.fire('mk1')
    t.wait(0.5)
    t.fire('s0')
    t.fire('s1')
    t.wait(0.1)
    t.expect.fired('hit0', activator='kid0', count=1)
    t.expect.not_fired('hit1')


# ---------------------------------------------------------------------------------------------- left at No / own
@S.test('monstermaker Children\'s weapons 0 (the monster\'s own), Gag 0, Override Render Mode 0, Reverse Relationship '
        '0: a grunt child keeps its own weapons (9mmAR + HG: 3), no Gag flag, its own render mode, its own side',
        players=1, doc=DOC,
        covers=('monstermaker.weapons=0', 'monstermaker.gag=0', 'monstermaker.change_rendermode=0',
                'monstermaker.respawn_as_playerally=0'))
def left_at_no(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mk', monstertype='monster_human_grunt', weapons='0', gag='0', change_rendermode='0',
          respawn_as_playerally='0', rendermode='2', renderamt='50', spawnflags='16')
    t.wait(0.2)
    t.fire('mk')
    t.wait(0.5)
    t.snap('s', 'kid', 'weapons', 'spawnflags', 'rendermode', 'renderamt')
    t.expect.value('s', 'kid', 'weapons', 3)
    t.expect.value('s', 'kid', 'spawnflags', 2, op='!&')
    t.expect.value('s', 'kid', 'rendermode', 0)


# ---------------------------------------------------------------------------------------------- path, facing, warpball
@S.test('monstermaker Children\'s Target (path) and Face position [LP]: the child\'s target is the path; it faces the '
        'point (north: yaw 90)', players=1, doc=DOC,
        covers=('monstermaker.childtarget', 'monstermaker.face_position'))
def path_face(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('path_corner', targetname='path', at=(0, 200, 0))
    info(t, 'north', at=(0, 200, 0))
    maker(t, 'mk', childtarget='path', face_position='north', spawnflags='16', names=('childtarget',))
    t.wait(0.2)
    t.fire('mk')
    t.wait(0.3)
    t.snap('s', 'kid', 'target', 'angles')
    t.expect.value('s', 'kid', 'target', t.name('path'))
    t.expect.value('s', 'kid', 'angles.y', 90, tol=2)


def warp_case(impulse):
    @S.test('monstermaker Warp at monster\'s %s (%d): the warpball plays at the child\'s %s' % (
            {-1: 'Origin', 0: 'Default (Center)', 1: 'Center'}[impulse], impulse,
            'origin' if impulse < 0 else 'center (36 up for a scientist)'), players=1, doc=DOC,
            covers=('monstermaker.impulse', 'monstermaker.impulse=%d' % impulse, 'monstermaker.warpball'))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.point('env_warpball', targetname='wb', at=(0, 0, 200), spawnflags=str(4 | 16), beamcount='4')
        maker(t, 'mk', at=(0, 0, 0), warpball='wb', impulse=str(impulse), spawnflags='16')
        t.wait(0.2)
        t.fire('mk')
        t.wait(0.3)
        z = 0 if impulse < 0 else 36
        t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS, 'pos': [float(v) for v in t._abs((0, 0, z))]}, min=1)
    case.__name__ = 'warp_%d' % (impulse + 1)
    return case


for _imp in (-1, 0, 1):
    warp_case(_imp)


# ---------------------------------------------------------------------------------------------- blocker
@S.test('monstermaker Make blocker move away 1 (walking): a scientist on the spot is asked to move and the child '
        'appears; 0: the spot stays taken, no child', players=1, doc=DOC, size=(512, 512, 256),
        covers=('monstermaker.make_blocker_move_away=1', 'monstermaker.make_blocker_move_away=0'))
def blocker(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, v in (('mv', -120, '1'), ('st', 120, '0')):
        for gx in range(x - 96, x + 97, 48):
            for gy in (-96, 0, 96):
                t.point('info_node', at=(gx, gy, 16))
        t.point('monster_scientist', targetname='blk_' + n, at=(x, 0, 0), angles='0 0 0')
        maker(t, 'mk_' + n, at=(x, 0, 0), kid='kid_' + n, make_blocker_move_away=v, delay='0.5')
    t.wait(1.0)
    t.fire('mk_mv')
    t.fire('mk_st')
    t.wait(6.0)
    t.expect.spawned('kid_mv', count=1)
    t.expect.absent('kid_st')
