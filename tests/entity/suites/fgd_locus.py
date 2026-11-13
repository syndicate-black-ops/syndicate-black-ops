"""Tier 5, batch 2: FGD items of locus_variable, player_calc_ratio (with game_player_settings' ammo keys) and
trigger_hurt_remote that no 1.38 map uses and no other test asserted (docs/LOCUS.md, docs/LOGIC_ENTITIES.md,
docs/FEATUREFUL_GUIDE_ENTITIES.md). Each test checks what the FGD and the docs say."""

from etest import Suite

SUITE = Suite('fgd_locus', 'Tier 5: locus_variable, player_calc_ratio ammo, trigger_hurt_remote', players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')
LOCUS = 'LOCUS.md'
LOGIC = 'LOGIC_ENTITIES.md'
GUIDE = 'FEATUREFUL_GUIDE_ENTITIES.md'
DMG_NEVERGIB, DMG_ALWAYSGIB = 4096, 8192


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


# ---------------------------------------------------------------------------------------------- locus_variable
@S.test('locus_variable Position [LP] and Vector [LV]: it moves to the recorded position and reports the vector',
        doc=LOCUS, covers=('locus_variable.m_iszPosition', 'locus_variable.m_iszVelocity'))
def lv_position(t):
    info(t, 'spot', at=(120, 60, 40))
    t.point('locus_variable', targetname='lv', at=(-100, -100, 10), m_iszPosition='spot', m_iszVelocity='10 20 30',
            m_iszRatio='7', names=('m_iszPosition',))
    t.fire('lv')
    t.wait(0.1)
    t.snap('s', 'lv', 'origin', 'movedir', 'frags')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'lv', 'spot', 0, 1, flat=False)
    t.expect.value('s', 'lv', 'movedir', [10, 20, 30])
    t.expect.value('s', 'lv', 'frags', 7)


@S.test("locus_variable Child's name, Fire on spawn, Child removed after: each trigger makes a named marker holding "
        "the values, fired as activator, removed after the time", doc=LOCUS,
        covers=('locus_variable.m_iszTargetName', 'locus_variable.m_iszFireOnSpawn', 'locus_variable.m_fDuration'))
def lv_child(t):
    t.point('locus_variable', targetname='lv', m_iszPosition='10 20 30', m_iszVelocity='1 2 3', m_iszRatio='5',
            m_iszTargetName='child', m_iszFireOnSpawn='spawned', m_fDuration='0.6', names=('m_iszTargetName',))
    info(t, 'spawned')
    t.mark('go')
    t.fire('lv')
    t.wait(0.2)
    t.snap('alive', 'child', 'frags', 'movedir')
    t.snap('alive', 'lv', 'frags')
    t.wait(0.7)
    t.expect.spawned('child', count=1)
    t.expect.used('spawned', activator='child', caller='lv', count=1)
    t.expect.value('alive', 'child', 'frags', 5)
    t.expect.value('alive', 'child', 'movedir', [1, 2, 3])
    t.expect.value('alive', 'lv', 'frags', 0)                  # with a child the variable itself keeps nothing
    t.expect.removed('child', count=1, after='go', within=(0.5, 0.7))


@S.test('locus_variable: a Position naming a missing entity gives no value, the variable keeps its position '
        '(LOCUS.md: "the entity that asked does nothing this time")', doc=LOCUS,
        covers=('locus_variable.m_iszPosition',))
def lv_missing(t):
    info(t, 'spot', at=(120, 60, 40))
    t.point('locus_variable', targetname='lv', m_iszPosition='spot', m_iszVelocity='0 0 0', m_iszRatio='0',
            names=('m_iszPosition',))
    t.fire('lv')
    t.wait(0.1)
    t.snap('before', 'lv', 'origin')
    t.kv('lv', 'm_iszPosition', 'no_such_entity_t5')
    t.fire('lv')
    t.wait(0.1)
    t.snap('after', 'lv', 'origin')
    t.expect.same('before', 'after', 'lv', 'origin')


@S.test('locus_variable: Position, Vector and Number left out or blank are *locus (the FGD default): the activator\'s '
        'position, velocity and number; fired without an activator it stores nothing', doc=LOCUS,
        covers=('locus_variable.m_iszPosition', 'locus_variable.m_iszVelocity', 'locus_variable.m_iszRatio'))
def lv_blank_is_locus(t):
    t.point('game_number', targetname='src', at=(120, 60, 40), frags='6', velocity='30 40 0')
    t.point('locus_variable', targetname='lva', at=(-100, -100, 10))
    t.point('locus_variable', targetname='lvb', at=(-100, 100, 10), m_iszPosition='', m_iszVelocity='',
            m_iszRatio='')
    t.point('locus_variable', targetname='lvn', at=(100, -100, 10))
    t.fire('lva', activator='src')
    t.fire('lvb', activator='src')
    t.snap('before', 'lvn', 'origin')
    t.fire('lvn')
    t.wait(0.1)
    t.snap('s', 'src', 'origin')
    for n in ('lva', 'lvb'):
        t.snap('s', n, 'origin', 'movedir', 'frags')
        t.expect.distance('s', n, 'src', 0, 1, flat=False)
        t.expect.value('s', n, 'movedir', [30, 40, 0])
        t.expect.value('s', n, 'frags', 6)
    t.snap('after', 'lvn', 'origin', 'frags')
    t.expect.same('before', 'after', 'lvn', 'origin')
    t.expect.value('after', 'lvn', 'frags', 0)


# ---------------------------------------------------------------------------------------------- player_calc_ratio ammo
# player_calc_ratio ammo_name choice -> game_player_settings key that gives it
AMMO = {'ARgrenades': 'ARgrenades', 'rockets': 'rockets', 'Hand Grenade': 'Hand_Grenade',
        'Satchel Charge': 'Satchel_Charge', 'Trip Mine': 'Trip_Mine', 'Snarks': 'Snarks', 'K444m': 'K444m',
        'benm': 'benm', 'canonm': 'canonm', 'chainm': 'chainm', 'fa18m': 'fa18m', 'federalm': 'federalm',
        'nm33m': 'nm33m', 'pm66m': 'pm66m', 'rc28m': 'rc28m', 'spam': 'spam', 'swm': 'swm'}
# FGD choices without a weapon in SBO that uses them (W_Precache never registers these ammo types)
HL_AMMO = ('9mm', '357', 'buckshot', 'bolts', 'uranium', 'Hornets')


def ammo_probe(t, idx, ammo, value='1'):
    """player_calc_ratio p<idx> (Ammo, absolute) read by trigger_compare c<idx> against `value`: eq<idx> / ne<idx> /
    fail<idx>."""
    t.point('player_calc_ratio', targetname='p%d' % idx, calc_param='2', value_notion='0', ammo_name=ammo)
    t.point('trigger_compare', targetname='c%d' % idx, initial_value=t.name('p%d' % idx), compare_value=value,
            testtype='0', trigger_on_equal='eq%d' % idx, trigger_on_not_equal='ne%d' % idx,
            trigger_on_fail='fail%d' % idx)
    for n in ('eq', 'ne', 'fail'):
        info(t, '%s%d' % (n, idx))


@S.test('player_calc_ratio Ammo type: every SBO ammo choice counts what game_player_settings gave (all taken, '
        'then 1 given; rockets: SBO carries none in reserve, so 0)', players=1, doc=LOCUS,
        covers=tuple('player_calc_ratio.ammo_name=%s' % a for a in AMMO)
        + tuple('game_player_settings.%s' % k for k in AMMO.values() if k != 'rockets')
        + ('player_calc_ratio.calc_param=2',))
def pcr_ammo(t):
    take = {k: '-999' for k in AMMO.values()}
    give = {k: '1' for k in AMMO.values()}
    t.point('game_player_settings', targetname='take', **take)
    t.point('game_player_settings', targetname='give', **give)
    for i, ammo in enumerate(AMMO):
        ammo_probe(t, i, ammo, '0' if ammo == 'rockets' else '1')
    t.fire('take', activator='player1')
    t.wait(0.1)
    t.fire('give', activator='player1')
    t.wait(0.1)
    for i in range(len(AMMO)):
        t.fire('c%d' % i, activator='player1')
    t.wait(0.1)
    for i, ammo in enumerate(AMMO):
        t.expect.fired('eq%d' % i, count=1)
        t.expect.not_fired('fail%d' % i)


@S.test('player_calc_ratio Ammo type blank: no number (calc fail)', players=1, doc=LOCUS,
        covers=('player_calc_ratio.ammo_name=',))
def pcr_blank(t):
    ammo_probe(t, 0, '')
    t.fire('c0', activator='player1')
    t.wait(0.1)
    t.expect.fired('fail0', count=1)
    t.expect.console('the ammo name is not specified')


# ---------------------------------------------------------------------------------------------- trigger_hurt_remote
DMG_TYPES = (0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 16384, 32768, 65536, 131072, 262144, 524288, 1048576,
             2097152, 4194304)


@S.test('trigger_hurt_remote Damage type: every choice reaches the target with that type', doc=LOGIC,
        size=(512, 512, 256),
        covers=('trigger_hurt_remote.damagetype',) + tuple('trigger_hurt_remote.damagetype=%d' % d
                                                           for d in DMG_TYPES))
def hurt_types(t):
    for i, d in enumerate(DMG_TYPES):
        x, y = -210 + (i % 7) * 70, -150 + (i // 7) * 100
        t.brush('func_breakable', targetname='b%d' % i, health='100000', size=(24, 24, 24), at=(x, y, 0))
        t.point('trigger_hurt_remote', targetname='h%d' % i, target='b%d' % i, dmg='5', damagetype=str(d))
        t.fire('h%d' % i)
    t.wait(0.1)
    for i, d in enumerate(DMG_TYPES):
        t.expect.damaged('b%d' % i, count=1, total=(4.9, 5.1), type=d if d else None)


def gib_case(policy):
    @S.test('trigger_hurt_remote Gibbing policy %d: a scientist killed with little overkill (health 10, 20 damage) %s; '
            '1000 damage gibs it either way' % (policy, 'stays a corpse' if policy == 0 else 'is gibbed (removed)'),
            doc=LOGIC, covers=('trigger_hurt_remote.gib_policy=%d' % policy,))
    def case(t):
        t.point('monster_scientist', targetname='small', at=(-100, 100, 0))
        t.point('monster_scientist', targetname='big', at=(100, 100, 0))
        t.point('trigger_hurt_remote', targetname='hs', target='small', dmg='20', damagetype='0',
                gib_policy=str(policy))
        t.point('trigger_hurt_remote', targetname='hb', target='big', dmg='1000', damagetype='0',
                gib_policy=str(policy))
        t.kv('small', 'health', '10')
        t.mark('hit')
        t.fire('hs')
        t.fire('hb')
        t.wait(1.5)
        t.expect.removed('big', after='hit', within=(0, 0.5))
        if policy == 1:
            t.expect.damaged('small', type=DMG_ALWAYSGIB, min=1)
            t.expect.removed('small', after='hit', within=(0, 0.5))
        else:
            t.snap('end', 'small', 'deadflag')
            t.expect.value('end', 'small', 'deadflag', 0, op='>')       # dead, a corpse
            t.expect.not_removed('small', after='hit')
    case.__name__ = 'hurt_gib%d' % policy
    return case


for _p in (0, 1):
    gib_case(_p)
