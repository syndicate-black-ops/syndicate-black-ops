"""trigger_changevalue / trigger_copyvalue (docs/TRIGGER_CHANGEVALUE.md, sdk/syndicate.fgd) keys the suites sbo / matrix
do not check: every operation on integer, float, vector and string keys (custom keyvalues $i_ / $f_ / $v_ / $s_,
read back), the trigonometric behavior, Append spaces, the flags Don't use X / Z, Invert target / source, the Locus
value kinds; trigger_copyvalue's float conversions, Multiple destinations, Start on, and its flags."""

import math

from etest import Suite

SUITE = Suite('values', 'trigger_changevalue / trigger_copyvalue: operations, flags, conversions')
S = SUITE
SUITE.server_args = ('+developer', '1')
DOC = 'TRIGGER_CHANGEVALUE.md'


def var(t, name, key, value):
    t.point('info_target', targetname=name, **{key: value})


def change(t, name, target, key, new, op, **kw):
    t.point('trigger_changevalue', targetname=name, target=target, m_iszValueName=key, m_iszNewValue=new,
            m_iszValueType=str(op), **kw)


INT_OPS = [(0, 4), (1, 10), (2, 24), (3, 2), (4, 1), (16, 1296), (12, 2), (5, 4), (6, 6), (13, 2), (15, 2), (30, 4),
           (31, 6)]


@S.test('trigger_changevalue on an integer key (6, new 4): replace, add, mul, sub, div, pow, mod, AND, OR, XOR, clear '
        'bits, min, max', doc=DOC + '#trigger_changevalue',
        covers=('trigger_changevalue', 'trigger_changevalue.target', 'trigger_changevalue.m_iszValueName',
                'trigger_changevalue.m_iszNewValue', 'trigger_changevalue.m_iszValueType'))
def int_ops(t):
    for op, _r in INT_OPS:
        var(t, 'v%d' % op, '$i_v', '6')
        change(t, 'c%d' % op, 'v%d' % op, '$i_v', '4', op)
        t.fire('c%d' % op)
    t.wait(0.1)
    for op, r in INT_OPS:
        t.snap('s', 'v%d' % op, 'kv:$i_v')
        t.expect.value('s', 'v%d' % op, 'kv:$i_v', r)


FLOAT_OPS = [(0, '2', 2), (1, '2', 8.5), (2, '2', 13), (3, '2', 4.5), (4, '2', 3.25), (16, '2', 42.25), (12, '4', 2),
             (30, '2', 2), (31, '2', 6.5)]


@S.test('trigger_changevalue on a float key (6.5): replace, add, mul, sub, div, pow, mod (of the whole parts), min, max',
        doc=DOC + '#trigger_changevalue', covers=('trigger_changevalue.m_iszValueType',))
def float_ops(t):
    for op, new, _r in FLOAT_OPS:
        var(t, 'v%d' % op, '$f_v', '6.5')
        change(t, 'c%d' % op, 'v%d' % op, '$f_v', new, op)
        t.fire('c%d' % op)
    t.wait(0.1)
    for op, _n, r in FLOAT_OPS:
        t.snap('s', 'v%d' % op, 'kv:$f_v')
        t.expect.value('s', 'v%d' % op, 'kv:$f_v', r, tol=0.001)


@S.test('NAND / NOR / NXOR are bitwise, as the FGD writes them: ~(old & new), ~(old | new), ~(old ^ new) (6 and 4: '
        '-5, -7, -3)', doc=DOC + '#trigger_changevalue', covers=('trigger_changevalue.m_iszValueType',))
def nand_bitwise(t):
    for op in (7, 8, 14):
        var(t, 'v%d' % op, '$i_v', '6')
        change(t, 'c%d' % op, 'v%d' % op, '$i_v', '4', op)
        t.fire('c%d' % op)
    t.wait(0.1)
    for op, r in ((7, -5), (8, -7), (14, -3)):
        t.snap('s', 'v%d' % op, 'kv:$i_v')
        t.expect.value('s', 'v%d' % op, 'kv:$i_v', r)


TRIG_RAD = [(17, math.pi / 6, 0.5), (18, math.pi / 3, 0.5), (19, math.pi / 4, 1), (23, math.pi / 4, 1),
            (20, 0.5, math.pi / 6), (21, 0.5, math.pi / 3), (22, 1, math.pi / 4), (24, 1, math.pi / 4)]


@S.test('trigonometric operations with behavior 1 Radians: sin, cos, tan, cot of "new" and their inverses', doc=DOC,
        covers=('trigger_changevalue.m_trigonometricBehaviour=1', 'trigger_changevalue.m_iszValueType'))
def trig_radians(t):
    for op, new, _r in TRIG_RAD:
        var(t, 'v%d' % op, '$f_v', '0')
        change(t, 'c%d' % op, 'v%d' % op, '$f_v', '%.7f' % new, op, m_trigonometricBehaviour='1')
        t.fire('c%d' % op)
    t.wait(0.1)
    for op, _n, r in TRIG_RAD:
        t.snap('s', 'v%d' % op, 'kv:$f_v')
        t.expect.value('s', 'v%d' % op, 'kv:$f_v', r, tol=0.001)


TRIG_DEG = [(17, 30, 0.5), (18, 60, 0.5), (19, 45, 1), (23, 45, 1), (20, 0.5, 30), (21, 0.5, 60), (22, 1, 45), (24, 1, 45)]


@S.test('trigonometric operations with behavior 0 Degrees (the default): sin(30) = 0.5, ..., arcsin(0.5) = 30', doc=DOC,
        covers=('trigger_changevalue.m_trigonometricBehaviour=0',))
def trig_degrees(t):
    for op, new, _r in TRIG_DEG:
        var(t, 'v%d' % op, '$f_v', '0')
        change(t, 'c%d' % op, 'v%d' % op, '$f_v', str(new), op, m_trigonometricBehaviour='0')
        t.fire('c%d' % op)
    t.wait(0.1)
    for op, _n, r in TRIG_DEG:
        t.snap('s', 'v%d' % op, 'kv:$f_v')
        t.expect.value('s', 'v%d' % op, 'kv:$f_v', r, tol=0.001)


@S.test('vectors: Direction to Angles ("0 1 0": 0 90 0), Angles to Direction ("0 90 0": 0 1 0); flags 1 / 4 Don\'t '
        'use X / Z leave that component', doc=DOC,
        covers=('trigger_changevalue:1', 'trigger_changevalue:4', 'trigger_changevalue.m_iszValueType'))
def vectors(t):
    var(t, 'd', '$v_v', '0 1 0')
    change(t, 'cd', 'd', '$v_v', '0', 9)
    var(t, 'a', '$v_v', '0 90 0')
    change(t, 'ca', 'a', '$v_v', '0', 10)
    var(t, 'x', '$v_v', '1 2 3')
    change(t, 'cx', 'x', '$v_v', '7 8 9', 0, spawnflags='1')
    var(t, 'z', '$v_v', '1 2 3')
    change(t, 'cz', 'z', '$v_v', '7 8 9', 0, spawnflags='4')
    for n in ('cd', 'ca', 'cx', 'cz'):
        t.fire(n)
    t.wait(0.1)
    for n, r in (('d', [0, 90, 0]), ('a', [0, 1, 0]), ('x', [1, 8, 9]), ('z', [7, 8, 3])):
        t.snap('s', n, 'kv:$v_v')
        t.expect.value('s', n, 'kv:$v_v', r, tol=0.001)


@S.test('a New value of three numbers is a vector also when y and z are zero: "64 0 0" with Replace, Add and Multiply '
        'on origin and on a $v_ key changes x only; one number ("2") still goes to every part; flag 2 Don\'t use Y leaves '
        'Y', doc=DOC, covers=('trigger_changevalue.m_iszNewValue', 'trigger_changevalue.m_iszValueType',
                              'trigger_changevalue:2'))
def three_numbers(t):
    for op in (0, 1, 2):
        var(t, 'v%d' % op, '$v_v', '1 2 3')
        change(t, 'cv%d' % op, 'v%d' % op, '$v_v', '64 0 0', op)
        t.point('info_target', targetname='o%d' % op, at=(-64 + 64 * op, 32, 16))
        change(t, 'co%d' % op, 'o%d' % op, 'origin', '64 0 0', op)
    var(t, 's', '$v_v', '1 2 3')
    change(t, 'cs', 's', '$v_v', '2', 2)
    var(t, 'y', '$v_v', '1 2 3')
    change(t, 'cy', 'y', '$v_v', '64 0 0', 0, spawnflags='2')
    for op in (0, 1, 2):
        t.snap('a', 'o%d' % op, 'origin')
    for n in ('cv0', 'cv1', 'cv2', 'co0', 'co1', 'co2', 'cs', 'cy'):
        t.fire(n)
    t.wait(0.1)
    for n, r in (('v0', [64, 0, 0]), ('v1', [65, 2, 3]), ('v2', [64, 0, 0]), ('s', [2, 4, 6]), ('y', [64, 2, 0])):
        t.snap('b', n, 'kv:$v_v')
        t.expect.value('b', n, 'kv:$v_v', r, tol=0.001)
    for op in (0, 1, 2):
        t.snap('b', 'o%d' % op, 'origin')
    ox, _oy, _oz = t._abs((64, 32, 16))
    t.expect.value('b', 'o0', 'origin', [64, 0, 0], tol=0.01)
    t.expect.moved('a', 'b', 'o1', delta=(64, 0, 0), tol=0.01)
    t.expect.value('b', 'o2', 'origin', [ox * 64, 0, 0], tol=0.5)


@S.test('strings: Append and Replace, each followed by Append spaces', doc=DOC,
        covers=('trigger_changevalue.m_iAppendSpaces', 'trigger_changevalue.m_iszValueType'))
def strings(t):
    var(t, 'a', '$s_v', 'ab')
    change(t, 'ca', 'a', '$s_v', 'cd', 11, m_iAppendSpaces='2')
    var(t, 'r', '$s_v', 'ab')
    change(t, 'cr', 'r', '$s_v', 'xy', 0, m_iAppendSpaces='1')
    t.fire('ca')
    t.fire('cr')
    t.wait(0.1)
    t.snap('s', 'a', 'kv:$s_v')
    t.snap('s', 'r', 'kv:$s_v')
    t.expect.value('s', 'a', 'kv:$s_v', 'abcd  ')
    t.expect.value('s', 'r', 'kv:$s_v', 'xy ')


@S.test('flag 32 Invert target value (6 add 4: -6 + 4) and 64 Invert source value (6 + -4)', doc=DOC,
        covers=('trigger_changevalue:32', 'trigger_changevalue:64'))
def invert(t):
    var(t, 'a', '$i_v', '6')
    change(t, 'ca', 'a', '$i_v', '4', 1, spawnflags='32')
    var(t, 'b', '$f_v', '6')
    change(t, 'cb', 'b', '$f_v', '4', 1, spawnflags='64')
    t.fire('ca')
    t.fire('cb')
    t.wait(0.1)
    t.snap('s', 'a', 'kv:$i_v')
    t.snap('s', 'b', 'kv:$f_v')
    t.expect.value('s', 'a', 'kv:$i_v', -2)
    t.expect.value('s', 'b', 'kv:$f_v', 2)


@S.test('Treat new value as: Literal (0), Locus Ratio (a calc_ratio), Locus Position (an entity\'s origin), Locus '
        'Velocity (a calc_vecfromnums), computed when it fires', doc=DOC,
        covers=tuple('trigger_changevalue.value_type=%d' % v for v in range(4)))
def locus_values(t):
    t.point('calc_ratio', targetname='r', target='5', impulse='4')
    t.point('info_target', targetname='spot', at=(10, 20, 30))
    t.point('calc_vecfromnums', targetname='vel', base_vector='1 2 3')
    var(t, 'lit', '$f_v', '0')
    change(t, 'c0', 'lit', '$f_v', '7', 0, value_type='0')
    var(t, 'rat', '$f_v', '0')
    change(t, 'c1', 'rat', '$f_v', 'r', 0, value_type='1', names=('m_iszNewValue',))
    var(t, 'pos', '$v_v', '0 0 0')
    change(t, 'c2', 'pos', '$v_v', 'spot', 0, value_type='2', names=('m_iszNewValue',))
    var(t, 'vec', '$v_v', '0 0 0')
    change(t, 'c3', 'vec', '$v_v', 'vel', 0, value_type='3', names=('m_iszNewValue',))
    for n in ('c0', 'c1', 'c2', 'c3'):
        t.fire(n)
    t.wait(0.1)
    for n, k, r in (('lit', '$f_v', 7), ('rat', '$f_v', 25), ('pos', '$v_v', list(t._abs((10, 20, 30)))),
                    ('vec', '$v_v', [1, 2, 3])):
        t.snap('s', n, 'kv:' + k)
        t.expect.value('s', n, 'kv:' + k, r, tol=0.01)


CONV = [(0, '2.718280'), (1, '2.71828'), (4, '2.7183'), (7, '2.718'), (10, '2.72'), (13, '2.7'), (16, '3'), (17, '3'),
        (18, '2')]


@S.test('trigger_copyvalue Float conversion: a float copied into a string key with each choice (6 .. 1 decimals, '
        'round, round up, round down)', doc=DOC + '#trigger_copyvalue',
        covers=('trigger_copyvalue', 'trigger_copyvalue.netname', 'trigger_copyvalue.m_iszSrcValueName',
                'trigger_copyvalue.target', 'trigger_copyvalue.m_iszDstValueName') +
        tuple('trigger_copyvalue.m_iFloatConversion=%d' % c for c, _r in CONV))
def float_conversion(t):
    var(t, 'src', '$f_s', '2.71828')
    for c, _r in CONV:
        var(t, 'd%d' % c, '$s_d', 'x')
        t.point('trigger_copyvalue', targetname='c%d' % c, netname='src', m_iszSrcValueName='$f_s',
                target='d%d' % c, m_iszDstValueName='$s_d', m_iFloatConversion=str(c))
        t.fire('c%d' % c)
    t.wait(0.1)
    for c, r in CONV:
        t.snap('s', 'd%d' % c, 'kv:$s_d')
        t.expect.value('s', 'd%d' % c, 'kv:$s_d', r)


NEG = [('m17', '-1.7', {16: '-2', 17: '-1', 18: '-2'}), ('m15', '-1.5', {16: '-1', 17: '-1', 18: '-2'}),
       ('p15', '1.5', {16: '2', 17: '2', 18: '1'})]


@S.test('trigger_copyvalue Float conversion on negative values: Round to nearest (halves up), Round up and Round '
        'down go to the nearer, the higher and the lower whole number, into a string key and an integer key',
        doc=DOC + '#trigger_copyvalue',
        covers=tuple('trigger_copyvalue.m_iFloatConversion=%d' % c for c in (16, 17, 18)))
def float_conversion_negative(t):
    for n, v, _r in NEG:
        var(t, 'src_' + n, '$f_s', v)
        for c in (16, 17, 18):
            for kind in ('s', 'i'):
                d = 'd_%s_%d_%s' % (n, c, kind)
                var(t, d, '$%s_d' % kind, 'x' if kind == 's' else '99')
                t.point('trigger_copyvalue', targetname='c' + d[1:], netname='src_' + n, m_iszSrcValueName='$f_s',
                        target=d, m_iszDstValueName='$%s_d' % kind, m_iFloatConversion=str(c))
                t.fire('c' + d[1:])
    t.wait(0.1)
    for n, _v, r in NEG:
        for c in (16, 17, 18):
            for kind in ('s', 'i'):
                d = 'd_%s_%d_%s' % (n, c, kind)
                t.snap('s', d, 'kv:$%s_d' % kind)
                t.expect.value('s', d, 'kv:$%s_d' % kind, r[c] if kind == 's' else int(r[c]))


@S.test('trigger_copyvalue: flag 128 Multiple destinations writes every entity of the name, without it only the '
        'first; an operation (Add) and flags 32 / 64 apply to the copied value as "new"', doc=DOC + '#trigger_copyvalue',
        covers=('trigger_copyvalue:128', 'trigger_copyvalue:32', 'trigger_copyvalue:64',
                'trigger_copyvalue.m_iszValueType'))
def copy_multi(t):
    # two entities per name, of classes no other test here uses, so '@class' reads each one
    var(t, 'src', '$i_s', '4')
    t.point('path_corner', targetname='all', **{'$i_d': '1'})
    t.point('info_teleport_destination', targetname='all', **{'$i_d': '1'})
    t.point('trigger_relay', targetname='one', **{'$i_d': '1'})
    t.point('env_state', targetname='one', **{'$i_d': '1'})
    var(t, 'inv', '$i_d', '6')
    t.point('trigger_copyvalue', targetname='cm', netname='src', m_iszSrcValueName='$i_s', target='all',
            m_iszDstValueName='$i_d', spawnflags='128')
    t.point('trigger_copyvalue', targetname='c1', netname='src', m_iszSrcValueName='$i_s', target='one',
            m_iszDstValueName='$i_d')
    t.point('trigger_copyvalue', targetname='ci', netname='src', m_iszSrcValueName='$i_s', target='inv',
            m_iszDstValueName='$i_d', m_iszValueType='1', spawnflags='96')
    for n in ('cm', 'c1', 'ci'):
        t.fire(n)
    t.wait(0.1)
    for n in ('@path_corner', '@info_teleport_destination', '@trigger_relay', '@env_state', 'inv'):
        t.snap('s', n, 'kv:$i_d')
    t.expect.value('s', 'inv', 'kv:$i_d', -10)       # -6 + -4
    t.expect.value('s', '@path_corner', 'kv:$i_d', 4)
    t.expect.value('s', '@info_teleport_destination', 'kv:$i_d', 4)
    t.expect.value('s', '@trigger_relay', 'kv:$i_d', 4)
    t.expect.value('s', '@env_state', 'kv:$i_d', 1)


@S.test('trigger_copyvalue flag 16 Start on (with 8 Constant): it copies every dmg seconds from the map start without '
        'a trigger; flag 1 Don\'t use X leaves a vector\'s X', doc=DOC + '#trigger_copyvalue',
        covers=('trigger_copyvalue:16', 'trigger_copyvalue:8', 'trigger_copyvalue.dmg', 'trigger_copyvalue:1'))
def copy_start_on(t):
    var(t, 'src', '$i_s', '1')
    var(t, 'dst', '$i_d', '0')
    t.point('trigger_copyvalue', targetname='cc', netname='src', m_iszSrcValueName='$i_s', target='dst',
            m_iszDstValueName='$i_d', spawnflags='24', dmg='0.2')
    change(t, 'set', 'src', '$i_s', '9', 0)
    var(t, 'vs', '$v_s', '7 8 9')
    var(t, 'vd', '$v_d', '1 2 3')
    t.point('trigger_copyvalue', targetname='cv', netname='vs', m_iszSrcValueName='$v_s', target='vd',
            m_iszDstValueName='$v_d', spawnflags='1')
    t.snap('a', 'dst', 'kv:$i_d')
    t.fire('set')
    t.fire('cv')
    t.wait(0.5)
    t.snap('b', 'dst', 'kv:$i_d')
    t.snap('b', 'vd', 'kv:$v_d')
    t.expect.value('a', 'dst', 'kv:$i_d', 1)
    t.expect.value('b', 'dst', 'kv:$i_d', 9)
    t.expect.value('b', 'vd', 'kv:$v_d', [1, 8, 9], tol=0.001)
