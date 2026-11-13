"""The calc_* number and vector entities (docs/LOCUS.md, sdk/syndicate.fgd): every transformation, offset, scale,
range policy and cast of calc_ratio; calc_eval_number's operations, third operand, range, casts and Report as
vector; calc_numfromvec's components, length, pitch / yaw and vector kinds; calc_vecfromnums; calc_subvelocity's
sources and flags; calc_velocity_path's destination kinds, length calculations, factor and blocking; calc_velocity_polar.
Each value is read the way a map reads it: a calc_eval_number takes it as an [LR] operand (+ 0) and stores the result
in a game_number (Use Set), whose value is checked; vectors are read through calc_numfromvec."""

import math

from etest import Suite

SUITE = Suite('calc', 'Locus calc_* entities: numbers and vectors')
S = SUITE
SUITE.server_args = ('+developer', '1')    # the calc entities' console lines (ALERT at_console)
DOC = 'LOCUS.md'


def read(t, n, operand, activator=None, **ev):
    """a game_number <n> gets the [LR] value of `operand` (a calc entity's local name or a number) when <n>_ev fires
    (with `activator` as the chain's *locus); a failed calculation fires <n>_fail and leaves the number at -999"""
    t.point('game_number', targetname=n, frags='-999')
    t.point('calc_eval_number', targetname=n + '_ev', left_operand=operand, right_operand='0', operation='0',
            store_result=n, trigger_on_fail=n + '_fail', **ev)
    t.point('info_target', targetname=n + '_fail')
    t.fire(n + '_ev', activator=activator)


def check(t, n, expected, tol=0.01):
    t.snap(n, n, 'frags')
    if expected is None:
        t.expect.value(n, n, 'frags', -999)
        t.expect.used(n + '_fail', count=1)
    else:
        t.expect.value(n, n, 'frags', expected, tol=tol)
        t.expect.not_used(n + '_fail')


def vec(t, n, source, kind='0', activator=None):
    """three game_numbers <n>x / <n>y / <n>z with the components of the vector `source` ([LV] by default)"""
    for i, c in enumerate('xyz'):
        t.point('calc_numfromvec', targetname='%s%s_c' % (n, c), target=source, impulse=str(i), vector_type=kind)
        read(t, n + c, '%s%s_c' % (n, c), activator=activator)


def check_vec(t, n, expected, tol=0.01):
    for c, v in zip('xyz', expected):
        check(t, n + c, v, tol)


# -------------------------------------------------------------------------------------------------- calc_ratio
RATIO = [  # (impulse, base, result)
    (0, 4, 4), (1, 4, -3), (2, 4, -4), (3, 4, 0.25), (4, 4, 16), (5, 4, 0.0625), (6, 16, 4),
    (7, 60, 0.5), (8, 30, 0.5), (9, 45, 1), (10, 0.5, 60), (11, 0.5, 30), (12, 1, 45),
]


@S.test('calc_ratio: every Transformation (None, 1-X, -X, 1/X, X*X, 1/X*X, square root, cosine, sine, tangent and '
        'their inverses, in degrees)', doc=DOC,
        covers=('calc_ratio', 'calc_ratio.target') + tuple('calc_ratio.impulse=%d' % i for i, _b, _r in RATIO))
def ratio_transforms(t):
    for i, base, res in RATIO:
        t.point('calc_ratio', targetname='r%d' % i, target=str(base), impulse=str(i))
        read(t, 'n%d' % i, 'r%d' % i)
    t.wait(0.1)
    for i, base, res in RATIO:
        check(t, 'n%d' % i, res, tol=0.001)


@S.test('calc_ratio: Offset is added after the transformation, Scale multiplies after the offset; no value (1/0, '
        'square root of -1, inverse cosine of 2): the calculation fails', doc=DOC,
        covers=('calc_ratio.netname', 'calc_ratio.message', 'calc_ratio.impulse=3', 'calc_ratio.impulse=6',
                'calc_ratio.impulse=10'))
def ratio_offset_scale_fail(t):
    t.point('calc_ratio', targetname='a', target='4', impulse='4', netname='1', message='3')     # (16 + 1) x 3
    read(t, 'na', 'a')
    t.point('calc_ratio', targetname='b', target='0', impulse='3')
    read(t, 'nb', 'b')
    t.point('calc_ratio', targetname='c', target='-1', impulse='6')
    read(t, 'nc', 'c')
    t.point('calc_ratio', targetname='d', target='2', impulse='10')
    read(t, 'nd', 'd')
    t.wait(0.1)
    check(t, 'na', 51)
    check(t, 'nb', None)
    check(t, 'nc', None)
    check(t, 'nd', None)


RANGE = [  # (frags policy, base, result) with Min 0, Max 10; bounce back below the Min: test ratio_bounce_below
    (0, 12, 10), (0, -3, 0), (1, 12, 2), (1, -3, 7), (2, 12, 8), (0, 5, 5),
]


@S.test('calc_ratio: Min / Max with each If-outside-range policy (nearest value, wrap around, bounce back), above '
        'and below the range; only a Min or only a Max crops that side', doc=DOC,
        covers=('calc_ratio.noise', 'calc_ratio.noise1', 'calc_ratio.frags=0', 'calc_ratio.frags=1',
                'calc_ratio.frags=2'))
def ratio_range(t):
    for k, (pol, base, res) in enumerate(RANGE):
        t.point('calc_ratio', targetname='r%d' % k, target=str(base), noise='0', noise1='10', frags=str(pol))
        read(t, 'n%d' % k, 'r%d' % k)
    t.point('calc_ratio', targetname='lo', target='-5', noise='2')
    read(t, 'nlo', 'lo')
    t.point('calc_ratio', targetname='hi', target='50', noise1='20')
    read(t, 'nhi', 'hi')
    t.point('calc_ratio', targetname='mid', target='-5', noise1='20')
    read(t, 'nmid', 'mid')
    t.wait(0.1)
    for k, (pol, base, res) in enumerate(RANGE):
        check(t, 'n%d' % k, res)
    check(t, 'nlo', 2)
    check(t, 'nhi', 20)
    check(t, 'nmid', -5)


@S.test('calc_ratio and calc_eval_number: "Bounce back" below the Min bounces off the Min (-3 in 0 .. 10 gives 3, as '
        '12 gives 8)', doc=DOC,
        covers=('calc_ratio.frags=2', 'calc_eval_number.clamp_policy=2'))
def ratio_bounce_below(t):
    t.point('calc_ratio', targetname='r', target='-3', noise='0', noise1='10', frags='2')
    read(t, 'nr', 'r')
    t.point('game_number', targetname='ne', frags='-999')
    t.point('calc_eval_number', targetname='ne_ev', left_operand='-1', right_operand='-2', operation='0',
            store_result='ne', min_value='0', max_value='10', clamp_policy='2')
    t.fire('ne_ev')
    t.wait(0.1)
    check(t, 'nr', 3)
    t.snap('ne', 'ne', 'frags')
    t.expect.value('ne', 'ne', 'frags', 3)


CASTS = [(1, 2.5, 3), (1, -2.4, -2), (2, -2.5, -3), (3, 2.1, 3), (4, -2.7, -2), (4, 2.7, 2), (0, 2.7, 2.7)]


@S.test('calc_ratio: Cast base and Cast result (round, floor, ceil, trunc, no cast); the base is cast before the '
        'transformation, the result after the range', doc=DOC,
        covers=tuple('calc_ratio.base_cast_mode=%d' % m for m in range(5)) +
        tuple('calc_ratio.result_cast_mode=%d' % m for m in range(5)))
def ratio_casts(t):
    for k, (mode, base, res) in enumerate(CASTS):
        t.point('calc_ratio', targetname='b%d' % k, target=str(base), base_cast_mode=str(mode))
        read(t, 'nb%d' % k, 'b%d' % k)
        t.point('calc_ratio', targetname='r%d' % k, target=str(base), result_cast_mode=str(mode))
        read(t, 'nr%d' % k, 'r%d' % k)
    # base cast first: floor(2.7) = 2, squared 4; result cast last: 2.7 squared 7.29, floor 7
    t.point('calc_ratio', targetname='bs', target='2.7', impulse='4', base_cast_mode='2')
    read(t, 'nbs', 'bs')
    t.point('calc_ratio', targetname='rs', target='2.7', impulse='4', result_cast_mode='2')
    read(t, 'nrs', 'rs')
    t.wait(0.1)
    for k, (mode, base, res) in enumerate(CASTS):
        check(t, 'nb%d' % k, res)
        check(t, 'nr%d' % k, res)
    check(t, 'nbs', 4)
    check(t, 'nrs', 7)


# -------------------------------------------------------------------------------------------- calc_eval_number
OPS = [(0, 7, 3, 10), (1, 7, 3, 4), (2, 7, 3, 21), (3, 7, 2, 3.5), (4, 7, 3, 1), (5, 7, 3, 3), (6, 7, 3, 7)]


@S.test('calc_eval_number: every Operation (add, subtract, multiply, divide, mod, min, max); a third operand applies '
        'the operation again; mod by 0 and a missing operand fail', doc=DOC,
        covers=('calc_eval_number', 'calc_eval_number.left_operand', 'calc_eval_number.right_operand',
                'calc_eval_number.third_operand', 'calc_eval_number.store_result',
                'calc_eval_number.trigger_on_fail') + tuple('calc_eval_number.operation=%d' % o for o, *_r in OPS))
def eval_ops(t):
    def ev(n, left, right, **kw):
        t.point('game_number', targetname=n, frags='-999')
        t.point('calc_eval_number', targetname=n + '_ev', left_operand=left, right_operand=right, store_result=n,
                trigger_on_fail=n + '_fail', **kw)
        t.point('info_target', targetname=n + '_fail')
        t.fire(n + '_ev')
    for op, a, b, res in OPS:
        ev('o%d' % op, str(a), str(b), operation=str(op))
    ev('third', '7', '3', third_operand='5', operation='1')          # 7 - 3 - 5
    ev('third_max', '7', '3', third_operand='9', operation='6')
    ev('mod0', '7', '0', operation='4')
    t.point('game_number', targetname='miss', frags='-999')
    t.point('calc_eval_number', targetname='miss_ev', left_operand='7', store_result='miss',
            trigger_on_fail='miss_fail')
    t.point('info_target', targetname='miss_fail')
    t.fire('miss_ev')
    t.wait(0.1)
    for op, a, b, res in OPS:
        check(t, 'o%d' % op, res)
    check(t, 'third', -1)
    check(t, 'third_max', 9)
    check(t, 'mod0', None)
    check(t, 'miss', None)
    t.expect.console('needs at least left and right operands defined')


@S.test('calc_eval_number: Min / Max with each range policy; the operand casts (first, second, third) and the result '
        'cast', doc=DOC,
        covers=('calc_eval_number.min_value', 'calc_eval_number.max_value') +
        tuple('calc_eval_number.clamp_policy=%d' % p for p in range(3)) +
        tuple('calc_eval_number.%s_cast_mode=%d' % (w, m) for w in ('first', 'second', 'third', 'result')
              for m in range(5)))
def eval_range_casts(t):
    def ev(n, left, right, **kw):
        t.point('game_number', targetname=n, frags='-999')
        t.point('calc_eval_number', targetname=n + '_ev', left_operand=left, right_operand=right, store_result=n,
                trigger_on_fail=n + '_fail', **kw)
        t.point('info_target', targetname=n + '_fail')
        t.fire(n + '_ev')
    rows = []
    for pol, res in ((0, 10), (1, 2), (2, 8)):                      # 6 + 6 = 12 into 0 .. 10
        ev('p%d' % pol, '6', '6', operation='0', min_value='0', max_value='10', clamp_policy=str(pol))
        rows.append(('p%d' % pol, res))
    # casts of 2.5 and -2.5: no cast 2.5, round 3, floor 2, ceil 3, trunc 2 (left); -2.5: round -2, floor -3, ceil -2,
    # trunc -2 (right): added
    left = {0: 2.5, 1: 3, 2: 2, 3: 3, 4: 2}
    right = {0: -2.5, 1: -2, 2: -3, 3: -2, 4: -2}
    for m in range(5):
        ev('f%d' % m, '2.5', '0', first_cast_mode=str(m))
        rows.append(('f%d' % m, left[m]))
        ev('s%d' % m, '0', '-2.5', second_cast_mode=str(m))
        rows.append(('s%d' % m, right[m]))
        ev('h%d' % m, '0', '0', third_operand='2.5', third_cast_mode=str(m))
        rows.append(('h%d' % m, left[m]))
        ev('r%d' % m, '1.2', '1.3', result_cast_mode=str(m))        # 2.5
        rows.append(('r%d' % m, left[m]))
    t.wait(0.1)
    for n, res in rows:
        check(t, n, res)


@S.test('calc_eval_number Report as vector: X 0 0, 0 Y 0, 0 0 Z (read as a vector by calc_numfromvec); No: not a '
        'vector (fails)', doc=DOC, covers=tuple('calc_eval_number.vector_mode=%d' % m for m in range(4)))
def eval_vector(t):
    for m in range(4):
        t.point('calc_eval_number', targetname='e%d' % m, left_operand='3', right_operand='4', operation='2',
                vector_mode=str(m))
        vec(t, 'v%d' % m, 'e%d' % m)
    t.wait(0.1)
    check_vec(t, 'v1', (12, 0, 0))
    check_vec(t, 'v2', (0, 12, 0))
    check_vec(t, 'v3', (0, 0, 12))
    check_vec(t, 'v0', (None, None, None))


# --------------------------------------------------------------------------------------------- calc_numfromvec
@S.test('calc_numfromvec: X, Y, Z, length, pitch and yaw of a vector; vector kinds: velocity [LV], position [LP] (an '
        'entity\'s origin), color (an entity\'s render color); no vector is *locus: without an activator it fails with a '
        'console line', doc=DOC,
        covers=('calc_numfromvec', 'calc_numfromvec.target') +
        tuple('calc_numfromvec.impulse=%d' % i for i in range(6)) +
        tuple('calc_numfromvec.vector_type=%d' % i for i in range(3)))
def numfromvec(t):
    for i, (v, res) in enumerate((('3 4 12', 3), ('3 4 12', 4), ('3 4 12', 12), ('3 4 12', 13), ('1 0 1', 45),
                                  ('0 10 0', 90))):
        t.point('calc_numfromvec', targetname='c%d' % i, target=v, impulse=str(i))
        read(t, 'n%d' % i, 'c%d' % i)
    t.point('info_target', targetname='spot', at=(30, -40, 50))
    t.point('calc_numfromvec', targetname='pz', target='spot', impulse='2', vector_type='1')
    read(t, 'npz', 'pz')
    t.point('env_sprite', targetname='spr', model='sprites/glow01.spr', rendercolor='10 20 30', spawnflags='0')
    t.point('calc_numfromvec', targetname='cg', target='spr', impulse='1', vector_type='2')
    read(t, 'ncg', 'cg')
    t.point('calc_numfromvec', targetname='none', impulse='0')
    read(t, 'nnone', 'none')
    t.wait(0.1)
    for i, res in enumerate((3, 4, 12, 13, 45, 90)):
        check(t, 'n%d' % i, res, tol=0.01)
    check(t, 'npz', 50)
    check(t, 'ncg', 20)
    check(t, 'nnone', None)
    t.expect.console("Couldn't get the vector value from *locus")


# --------------------------------------------------------------------------------------------- calc_vecfromnums
@S.test('calc_vecfromnums: X / Y / Z values replace the base vector\'s; blank keeps the base\'s; no base: 0 0 0; a '
        'color base (an entity\'s render color) and a position base', doc=DOC,
        covers=('calc_vecfromnums', 'calc_vecfromnums.base_vector', 'calc_vecfromnums.x_value',
                'calc_vecfromnums.y_value', 'calc_vecfromnums.z_value') +
        tuple('calc_vecfromnums.vector_type=%d' % i for i in range(3)))
def vecfromnums(t):
    t.point('calc_vecfromnums', targetname='a', base_vector='1 2 3', y_value='7', vector_type='0')
    vec(t, 'a', 'a')
    t.point('calc_vecfromnums', targetname='b', x_value='5')
    vec(t, 'b', 'b')
    t.point('env_sprite', targetname='spr', model='sprites/glow01.spr', rendercolor='10 20 30')
    t.point('calc_vecfromnums', targetname='c', base_vector='spr', vector_type='2', z_value='-4', names=('base_vector',))
    vec(t, 'c', 'c')
    t.point('info_target', targetname='spot', at=(0, 0, 50))
    t.point('calc_vecfromnums', targetname='d', base_vector='spot', vector_type='1', x_value='9', names=('base_vector',))
    vec(t, 'd', 'd')
    t.wait(0.1)
    check_vec(t, 'a', (1, 7, 3))
    check_vec(t, 'b', (5, 0, 0))
    check_vec(t, 'c', (10, 20, -4))
    check_vec(t, 'd', (9, t._abs((0, 0, 50))[1], 50))


# --------------------------------------------------------------------------------------------- calc_subvelocity
@S.test('calc_subvelocity: an entity\'s movement velocity x the scale + the offset; flag 1 normalizes before scaling; '
        'flags 4 / 8 / 16 discard X / Y / Z; flag 2 flips Z', doc=DOC,
        covers=('calc_subvelocity', 'calc_subvelocity.netname', 'calc_subvelocity.impulse=0', 'calc_subvelocity.noise',
                'calc_subvelocity.message', 'calc_subvelocity:1', 'calc_subvelocity:2', 'calc_subvelocity:4',
                'calc_subvelocity:8', 'calc_subvelocity:16'))
def subvelocity_flags(t):
    t.point('info_target', targetname='mover', velocity='30 40 -20')
    rows = [('a', {}, (30, 40, -20)), ('b', {'noise': '2', 'message': '1 1 1'}, (61, 81, -39)),
            ('c', {'spawnflags': '1', 'noise': '10'}, (30 / 5.385165, 40 / 5.385165, -20 / 5.385165)),
            ('d', {'spawnflags': '4'}, (0, 40, -20)), ('e', {'spawnflags': '8'}, (30, 0, -20)),
            ('f', {'spawnflags': '16'}, (30, 40, 0)), ('g', {'spawnflags': '2'}, (30, 40, 20))]
    for n, kw, _res in rows:
        t.point('calc_subvelocity', targetname=n, netname='mover', impulse='0', names=('netname',), **kw)
        vec(t, n, n)
    t.wait(0.1)
    for n, _kw, res in rows:
        check_vec(t, n, res, tol=0.02)


@S.test('calc_subvelocity: Angles (direction) and View angles give the forward vector; Calc Velocity of the entity '
        'reads another calc entity [LV]; Spirit\'s attachment directions 5-8 (left out of the FGD) fail with a console '
        'line, also on a model that has the attachments; a missing entity fails', doc=DOC, players=1,
        covers=('calc_subvelocity.impulse=1', 'calc_subvelocity.impulse=2', 'calc_subvelocity.impulse=10'))
def subvelocity_sources(t):
    t.point('info_target', targetname='turned', angles='0 90 0')
    t.point('calc_subvelocity', targetname='ang', netname='turned', impulse='1', noise='10', names=('netname',))
    vec(t, 'ang', 'ang')
    t.point('calc_vecfromnums', targetname='src', base_vector='3 4 0')
    t.point('calc_subvelocity', targetname='calc', netname='src', impulse='10', names=('netname',))
    vec(t, 'calc', 'calc')
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model='models/controller.mdl', m_iszSequence_On='idle2')
    for i in (5, 6, 7, 8):      # Spirit's attachment directions (left out of the FGD): no value
        t.point('calc_subvelocity', targetname='att%d' % i, netname='mdl', impulse=str(i), names=('netname',))
        vec(t, 'att%d' % i, 'att%d' % i)
    t.point('calc_subvelocity', targetname='miss', netname='nobody', impulse='0', names=('netname',))
    vec(t, 'miss', 'miss')
    # View angles: the activator (*locus) is the player, looking along yaw 180
    t.look('player1', 0, 180)
    t.wait(0.1)
    t.point('calc_subvelocity', targetname='view', netname='*locus', impulse='2', noise='10')
    for i, c in enumerate('xyz'):
        t.point('calc_numfromvec', targetname='view%s_c' % c, target='view', impulse=str(i))
        t.point('game_number', targetname='view' + c, frags='-999')
        t.point('calc_eval_number', targetname='view%s_ev' % c, left_operand='view%s_c' % c, right_operand='0',
                store_result='view' + c, trigger_on_fail='view%s_fail' % c)
        t.point('info_target', targetname='view%s_fail' % c)
        t.fire('view%s_ev' % c, activator='player1')
    t.wait(0.1)
    check_vec(t, 'ang', (0, 10, 0), tol=0.02)
    check_vec(t, 'calc', (3, 4, 0))
    for i in (5, 6, 7, 8):
        check_vec(t, 'att%d' % i, (None, None, None))
    check_vec(t, 'miss', (None, None, None))
    check_vec(t, 'view', (-10, 0, 0), tol=0.05)
    t.expect.console('no attachment direction (Value to calculate from 5)')
    t.expect.console('no attachment direction (Value to calculate from 8)')
    t.expect.console('failed to find target entity')


# ------------------------------------------------------------------------------------------- calc_velocity_path
LENGTH = [(0, (0, 30, 40)), (1, (0, 0.6, 0.8)), (2, (0, 0.6 / 50, 0.8 / 50)), (3, (0, 0.6 / 2500, 0.8 / 2500)),
          (4, (0, 1500, 2000))]


@S.test('calc_velocity_path: from the start position to the destination position (or by an offset); each Length '
        'calculation (none, normalize, reciprocal, inverse square, square) and the length factor', doc=DOC,
        covers=('calc_velocity_path', 'calc_velocity_path.target', 'calc_velocity_path.netname',
                'calc_velocity_path.armorvalue=0', 'calc_velocity_path.armorvalue=1', 'calc_velocity_path.noise') +
        tuple('calc_velocity_path.health=%d' % h for h, _r in LENGTH))
def velocity_path(t):
    for h, res in LENGTH:
        # factor 1000 for the tiny ones, so the 0.01 tolerance means something
        f = 1000 if h in (2, 3) else 1
        t.point('calc_velocity_path', targetname='p%d' % h, target='100 0 0', netname='100 30 40', health=str(h),
                noise=str(f))
        vec(t, 'p%d' % h, 'p%d' % h)
    t.point('calc_velocity_path', targetname='off', target='100 0 0', netname='5 6 7', armorvalue='1', noise='2')
    vec(t, 'off', 'off')
    t.wait(0.1)
    for h, res in LENGTH:
        f = 1000 if h in (2, 3) else 1
        check_vec(t, 'p%d' % h, tuple(c * f for c in res), tol=0.01)
    check_vec(t, 'off', (10, 12, 14))


BLOCK = [  # (frags, length): the line from the room's center east: glass at +40, a monster at +100, the wall at +256
    (0, 400), (1, 256), (2, 36), (3, None), (4, 36),
]


@S.test('calc_velocity_path "Line is blocked by": nothing, walls, walls & glass, walls & monsters, walls, monsters & '
        'glass: the vector ends where the line is stopped', doc=DOC, size=(512, 512, 256),
        covers=tuple('calc_velocity_path.frags=%d' % f for f, _l in BLOCK))
def velocity_path_block(t):
    x, y, _z = t._abs((0, 0, 0))
    # glass: a see-through breakable (a func_wall is a world brush, never glass to a trace): 36 .. 44
    t.brush('func_breakable', size=(8, 64, 128), at=(40, 0, 0), rendermode='2', renderamt='120', material='0',
            spawnflags='1')
    t.point('monster_scientist', targetname='sci', at=(100, 0, 0), angles='0 180 0')
    start = '%g %g 50' % (x, y)
    end = '%g %g 50' % (x + 400, y)
    for f, _l in BLOCK:
        t.point('calc_velocity_path', targetname='b%d' % f, target=start, netname=end, frags=str(f))
        t.point('calc_numfromvec', targetname='b%d_len' % f, target='b%d' % f, impulse='3')
        read(t, 'n%d' % f, 'b%d_len' % f)
    t.wait(0.1)
    for f, l in BLOCK:
        if l is not None:
            check(t, 'n%d' % f, l, tol=2)
    # walls & monsters: the line ends on the scientist's box, in front of its origin (+100)
    t.snap('n3', 'n3', 'frags')
    t.expect.value('n3', 'n3', 'frags', 60, op='>')
    t.expect.value('n3', 'n3', 'frags', 100, op='<')


# ------------------------------------------------------------------------------------------ calc_velocity_polar
@S.test('calc_velocity_polar: the vector turned by its angles, x the length factor (times the vector\'s length; '
        'flag 1: the factor is the exact length), + the offset', doc=DOC,
        covers=('calc_velocity_polar', 'calc_velocity_polar.netname', 'calc_velocity_polar.angles',
                'calc_velocity_polar.noise', 'calc_velocity_polar.message', 'calc_velocity_polar:1'))
def velocity_polar(t):
    t.point('calc_velocity_polar', targetname='a', netname='10 0 0', angles='0 90 0')
    vec(t, 'a', 'a')
    t.point('calc_velocity_polar', targetname='b', netname='10 0 0', angles='0 90 0', noise='2', message='1 1 1')
    vec(t, 'b', 'b')
    t.point('calc_velocity_polar', targetname='c', netname='10 0 0', angles='0 180 0', noise='5', spawnflags='1')
    vec(t, 'c', 'c')
    t.wait(0.1)
    check_vec(t, 'a', (0, 10, 0), tol=0.02)
    check_vec(t, 'b', (1, 21, 1), tol=0.02)
    check_vec(t, 'c', (-5, 0, 0), tol=0.02)


@S.test('calc_velocity_polar: the up / down part is kept (Spirit 1.8): an upward vector stays upward with angles '
        '0 0 0, a turned one keeps its slope, a positive pitch turns a flat vector up', doc=DOC,
        covers=('calc_velocity_polar', 'calc_velocity_polar.netname', 'calc_velocity_polar.angles'))
def velocity_polar_vertical(t):
    t.point('calc_velocity_polar', targetname='u', netname='0 0 10', angles='0 0 0')
    vec(t, 'u', 'u')
    t.point('calc_velocity_polar', targetname='s', netname='10 0 10', angles='0 90 0')
    vec(t, 's', 's')
    t.point('calc_velocity_polar', targetname='p', netname='10 0 0', angles='30 0 0')
    vec(t, 'p', 'p')
    t.wait(0.1)
    check_vec(t, 'u', (0, 0, 10), tol=0.02)
    check_vec(t, 's', (0, 10, 10), tol=0.02)
    check_vec(t, 'p', (8.660, 0, 5), tol=0.02)


# ------------------------------------------------------------------------------------------- blank = *locus
@S.test('A blank or left-out "what to work from" key is *locus, its FGD default: calc_position Entity to use, '
        'calc_ratio Based on number, calc_numfromvec Vector to use, calc_subvelocity Entity to use, calc_velocity_path '
        'Start position, calc_velocity_polar Based on vector, each read with a game_number as activator', doc=DOC,
        covers=('calc_position.netname', 'calc_ratio.target', 'calc_numfromvec.target', 'calc_subvelocity.netname',
                'calc_velocity_path.target', 'calc_velocity_polar.netname'))
def blank_is_locus(t):
    # the activator: number 6, velocity 30 40 0, at (40, 20, 16) in the room
    t.point('game_number', targetname='src', at=(40, 20, 16), frags='6', velocity='30 40 0')
    sx, sy, sz = t._abs((40, 20, 16))
    dest = '%g %g %g' % t._abs((140, 20, 16))
    for v, blank in (('a', None), ('b', '')):       # a: key left out; b: key blank
        t.point('calc_position', targetname='pos' + v, impulse='0', netname=blank)
        vec(t, 'pos' + v, 'pos' + v, kind='1', activator='src')
        t.point('calc_ratio', targetname='r' + v, target=blank)
        read(t, 'nr' + v, 'r' + v, activator='src')
        t.point('calc_numfromvec', targetname='nv' + v, impulse='0', target=blank)
        read(t, 'nnv' + v, 'nv' + v, activator='src')
        t.point('calc_subvelocity', targetname='sv' + v, impulse='0', netname=blank)
        vec(t, 'sv' + v, 'sv' + v, activator='src')
        # from the activator's origin (not this entity's) to 100 units east of it
        t.point('calc_velocity_path', targetname='vp' + v, at=(-100, -100, 0), netname=dest, target=blank)
        vec(t, 'vp' + v, 'vp' + v, activator='src')
        t.point('calc_velocity_polar', targetname='vq' + v, netname=blank)
        vec(t, 'vq' + v, 'vq' + v, activator='src')
    t.wait(0.1)
    for v in 'ab':
        check_vec(t, 'pos' + v, (sx, sy, sz), tol=0.1)
        check(t, 'nr' + v, 6)
        check(t, 'nnv' + v, 30)
        check_vec(t, 'sv' + v, (30, 40, 0))
        check_vec(t, 'vp' + v, (100, 0, 0), tol=0.1)
        check_vec(t, 'vq' + v, (30, 40, 0), tol=0.02)
