"""Tier 5, batch 2: FGD items of calc_state, trigger_condition and trigger_entity_iterator that no 1.38 map uses and no
other test asserted (docs/STATE_LOGIC.md, docs/LOGIC_ENTITIES.md). Each test checks what the FGD and the docs say."""

from etest import Suite

SUITE = Suite('fgd_logic2', 'Tier 5: calc_state, trigger_condition, trigger_entity_iterator FGD-only items')
S = SUITE
SUITE.server_args = ('+developer', '1')      # calc_state Debug Mode and error notes are developer messages
LOGIC = 'LOGIC_ENTITIES.md'
STATE = 'STATE_LOGIC.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3
OPS = ('left_operand', 'right_operand', 'fire_when_true', 'fire_when_false')


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


# ---------------------------------------------------------------------------------------------- calc_state
CALC_OPS = {2: ('NAND', lambda a, b: not (a and b)), 3: ('NOR', lambda a, b: not (a or b)),
            4: ('XOR', lambda a, b: a != b), 5: ('NXOR', lambda a, b: a == b)}
PAIRS = ((True, True), (True, False), (False, False))


def calc_case(op):
    label, fn = CALC_OPS[op]

    @S.test('calc_state operation %d (%s) of two env_states: on/on, on/off, off/off' % (op, label), doc=STATE,
            covers=('calc_state.operation=%d' % op, 'calc_state.target', 'calc_state.fire_when_true',
                    'calc_state.fire_when_false'))
    def case(t):
        t.point('env_state', targetname='on', spawnflags='1')
        t.point('env_state', targetname='off')
        for i, (a, b) in enumerate(PAIRS):
            t.point('calc_state', targetname='c%d' % i, left_operand='on' if a else 'off',
                    right_operand='on' if b else 'off', operation=str(op), target='o%d' % i,
                    fire_when_true='t%d' % i, fire_when_false='f%d' % i, names=OPS)
            for n in ('o', 't', 'f'):
                info(t, '%s%d' % (n, i))
            t.fire('c%d' % i)
        t.wait(0.1)
        for i, (a, b) in enumerate(PAIRS):
            r = fn(a, b)
            t.expect.fired('o%d' % i, use=USE_ON if r else USE_OFF, count=1)
            t.expect.fired('t%d' % i, count=1 if r else 0)
            t.expect.fired('f%d' % i, count=0 if r else 1)
    case.__name__ = 'calc_op%d' % op
    return case


for _op in sorted(CALC_OPS):
    calc_case(_op)


def fallback_case(side, fb):
    meaning = {0: 'an error: nothing fires, a console note', 1: 'Off', 2: 'On'}[fb]

    @S.test('calc_state %s_fallback %d: a missing operand %s is %s' % (side, fb, side, meaning), doc=STATE,
            covers=('calc_state.%s_fallback' % side, 'calc_state.%s_fallback=%d' % (side, fb)))
    def case(t):
        t.point('env_state', targetname='on', spawnflags='1')
        kv = {'left_operand': 'on', 'right_operand': 'on', side + '_operand': 'missing', side + '_fallback': str(fb)}
        # AND with an operand that is on: the result is the missing operand's value
        t.point('calc_state', targetname='cs', operation='0', target='out', fire_when_true='yes',
                fire_when_false='no', names=OPS, **kv)
        for n in ('out', 'yes', 'no'):
            info(t, n)
        t.fire('cs')
        t.wait(0.1)
        if fb == 0:
            t.expect.not_fired('out')
            t.expect.not_fired('yes')
            t.expect.not_fired('no')
            t.expect.console("%s operand '%s' doesn't exist" % (side, t.name('missing')))
        else:
            t.expect.fired('out', use=USE_ON if fb == 2 else USE_OFF, count=1)
            t.expect.fired('yes' if fb == 2 else 'no', count=1)
    case.__name__ = 'calc_fallback_%s%d' % (side, fb)
    return case


for _side in ('left', 'right'):
    for _fb in (0, 1, 2):
        fallback_case(_side, _fb)


@S.test('calc_state Debug Mode (2): console notes for each operand and the result', doc=STATE,
        covers=('calc_state:2',))
def calc_debug(t):
    t.point('env_state', targetname='on', spawnflags='1')
    t.point('env_state', targetname='off')
    t.point('calc_state', targetname='dbg', left_operand='on', right_operand='off', operation='1', spawnflags='2',
            names=OPS)
    t.fire('dbg')
    t.wait(0.1)
    d = t.name('dbg')
    t.expect.console("calc_state '%s': left operand ('%s') evaluated to true" % (d, t.name('on')))
    t.expect.console("calc_state '%s': right operand ('%s') evaluated to false" % (d, t.name('off')))
    t.expect.console("calc_state '%s': operation OR evaluated to true" % d)


# ---------------------------------------------------------------------------------------------- trigger_condition
COND_NAMES = ('netname', 'message', 'm_iszSourceName')


def cond(t, name, mon, key, value, ctype, **kv):
    """A cyclic trigger_condition: each trigger checks once; netname yes_<name>, message no_<name>."""
    kv.setdefault('spawnflags', '32')
    t.point('trigger_condition', targetname=name, target=mon, m_iszValueName=key, m_iszCheckValue=value,
            m_iCheckType=str(ctype), netname='yes_' + name, message='no_' + name, names=COND_NAMES, **kv)
    info(t, 'yes_' + name)
    info(t, 'no_' + name)


COND_TYPES = {1: ('!=', (True, False, True)), 3: ('>', (True, False, False)), 4: ('<=', (False, True, True)),
              5: ('>=', (True, True, False))}


def cond_case(ctype):
    sign, results = COND_TYPES[ctype]

    @S.test('trigger_condition Comparator %d (%s): an integer key 5 against 4, 5 and 6' % (ctype, sign), doc=LOGIC,
            covers=('trigger_condition.m_iCheckType=%d' % ctype, 'trigger_condition:32'))
    def case(t):
        info(t, 'var', **{'$i_n': '5'})
        for v in (4, 5, 6):
            cond(t, 'c%d' % v, 'var', '$i_n', str(v), ctype)
            t.fire('c%d' % v)
        t.wait(0.1)
        for v, ok in zip((4, 5, 6), results):
            t.expect.used('yes_c%d' % v, count=1 if ok else 0)
            t.expect.used('no_c%d' % v, count=0 if ok else 1)
    case.__name__ = 'cond_type%d' % ctype
    return case


for _ct in sorted(COND_TYPES):
    cond_case(_ct)


@S.test('trigger_condition Comparator 6 (&, bits set): 6 & 2 is true, 6 & 1 false', doc=LOGIC,
        covers=('trigger_condition.m_iCheckType=6',))
def cond_bits(t):
    info(t, 'var', **{'$i_n': '6'})
    cond(t, 'b2', 'var', '$i_n', '2', 6)
    cond(t, 'b1', 'var', '$i_n', '1', 6)
    t.fire('b2')
    t.fire('b1')
    t.wait(0.1)
    t.expect.used('yes_b2', count=1)
    t.expect.used('no_b1', count=1)
    t.expect.not_used('yes_b1')


@S.test("trigger_condition Compare-entity / Compare-key: the monitored key against another entity's key", doc=LOGIC,
        covers=('trigger_condition.m_iszSourceName', 'trigger_condition.m_iszSourceKey'))
def cond_source(t):
    info(t, 'var', **{'$i_n': '5'})
    info(t, 'ref5', **{'$i_m': '5'})
    info(t, 'ref7', **{'$i_m': '7'})
    cond(t, 'same', 'var', '$i_n', '99', 0, m_iszSourceName='ref5', m_iszSourceKey='$i_m')
    cond(t, 'diff', 'var', '$i_n', '5', 0, m_iszSourceName='ref7', m_iszSourceKey='$i_m')
    t.fire('same')
    t.fire('diff')
    t.wait(0.1)
    t.expect.used('yes_same', count=1)      # 5 == ref5's 5 (the Compare-value 99 is not used)
    t.expect.used('no_diff', count=1)       # 5 != ref7's 7 (not the Compare-value 5)


def vec_flag_case(flag, color):
    axis = {2: 'X (R)', 4: 'Y (G)', 8: 'Z (B)'}[flag]

    @S.test("trigger_condition Don't use %s (%d): rendercolor %s == 0 0 0 with that component left out" % (
            axis, flag, color), doc=LOGIC, covers=('trigger_condition:%d' % flag,))
    def case(t):
        info(t, 'var', rendercolor=color)
        cond(t, 'with', 'var', 'rendercolor', '0 0 0', 0, spawnflags=str(32 | flag))
        cond(t, 'without', 'var', 'rendercolor', '0 0 0', 0)
        t.fire('with')
        t.fire('without')
        t.wait(0.1)
        t.expect.used('yes_with', count=1)
        t.expect.used('no_without', count=1)
    case.__name__ = 'cond_vec%d' % flag
    return case


for _f, _c in ((2, '200 0 0'), (4, '0 200 0'), (8, '0 0 200')):
    vec_flag_case(_f, _c)


@S.test("trigger_condition Don't use X and Z (2 + 8): one component left, compared with < as a number", doc=LOGIC,
        covers=('trigger_condition:2', 'trigger_condition:8', 'trigger_condition.m_iCheckType=2'))
def cond_vec_one(t):
    info(t, 'var', rendercolor='200 10 0')
    cond(t, 'g', 'var', 'rendercolor', '50', 2, spawnflags=str(32 | 2 | 8))      # G 10 < 50 (the length 200 is not)
    t.fire('g')
    t.wait(0.1)
    t.expect.used('yes_g', count=1)


@S.test('trigger_condition Keep activator (64): the result target gets the activator the condition got', doc=LOGIC,
        covers=('trigger_condition:64',))
def cond_keep(t):
    info(t, 'var', **{'$i_n': '1'})
    info(t, 'src')
    cond(t, 'keep', 'var', '$i_n', '1', 0, spawnflags=str(32 | 64))
    cond(t, 'own', 'var', '$i_n', '1', 0)
    t.fire('keep', activator='src')
    t.fire('own', activator='src')
    t.wait(0.1)
    t.expect.used('yes_keep', activator='src', count=1)
    t.expect.used('yes_own', activator='own', count=1)


def constant(t, name, behaviour, flags=1, interval='0.2'):
    """A constant-mode trigger_condition on var's $i_n == 1, Start Off; netname yes_<name>, message no_<name>."""
    t.point('trigger_condition', targetname=name, target='var', m_iszValueName='$i_n', m_iszCheckValue='1',
            m_iCheckType='0', m_iCheckBehaviour=str(behaviour), m_fCheckInterval=interval, netname='yes_' + name,
            message='no_' + name, spawnflags=str(flags), names=COND_NAMES)
    info(t, 'yes_' + name)
    info(t, 'no_' + name)


def set_var(t, name, value):
    t.point('trigger_changevalue', targetname=name, target='var', m_iszValueName='$i_n', m_iszNewValue=value)


@S.test('trigger_condition Check interval left out: it checks every 0.1 s, the FGD default (Always: one true '
        'fire per 0.1 s)', doc=LOGIC, covers=('trigger_condition.m_fCheckInterval',))
def cond_interval_default(t):
    info(t, 'var', **{'$i_n': '1'})
    t.point('trigger_condition', targetname='ci', target='var', m_iszValueName='$i_n', m_iszCheckValue='1',
            m_iCheckType='0', m_iCheckBehaviour='3', netname='yes_ci', message='no_ci', spawnflags='1',
            names=COND_NAMES)
    info(t, 'yes_ci')
    info(t, 'no_ci')
    t.mark('on')
    t.fire('ci', use='on')
    t.wait(1.05)
    t.fire('ci', use='off')
    t.wait(0.2)
    t.expect.used('yes_ci', min=9, after='on', within=(0, 1.0))
    t.expect.used('yes_ci', count=0, after='on', within=(0.03, 0.07))
    t.expect.used('yes_ci', count=1, after='on', within=(0.07, 0.14))
    t.expect.not_used('no_ci')


@S.test('trigger_condition In constant mode fire when 1 (changed or keeps being true): true on every check, false '
        'once', doc=LOGIC, covers=('trigger_condition.m_iCheckBehaviour=1',))
def cond_mode1(t):
    info(t, 'var', **{'$i_n': '1'})
    constant(t, 'cm', 1)
    set_var(t, 'set0', '0')
    t.mark('on')
    t.fire('cm', use='on')
    t.wait(0.9)                      # checks at 0, 0.2, ... 0.8: true each time
    t.fire('set0')
    t.mark('false')
    t.wait(0.9)
    t.fire('cm', use='off')
    t.expect.used('yes_cm', min=4, after='on', within=(0, 0.85))
    t.expect.used('no_cm', count=1, after='false')
    t.expect.not_used('yes_cm', after='false')


@S.test('trigger_condition In constant mode fire when 2 (changed or keeps being false): false on every check, true '
        'once', doc=LOGIC, covers=('trigger_condition.m_iCheckBehaviour=2',))
def cond_mode2(t):
    info(t, 'var', **{'$i_n': '0'})
    constant(t, 'cm', 2)
    set_var(t, 'set1', '1')
    t.mark('on')
    t.fire('cm', use='on')
    t.wait(0.9)
    t.fire('set1')
    t.mark('true')
    t.wait(0.9)
    t.fire('cm', use='off')
    t.expect.used('no_cm', min=4, after='on', within=(0, 0.85))
    t.expect.used('yes_cm', count=1, after='true')
    t.expect.not_used('no_cm', after='true')


@S.test('trigger_condition In constant mode fire when 3 (always): true and false on every check', doc=LOGIC,
        covers=('trigger_condition.m_iCheckBehaviour=3',))
def cond_mode3(t):
    info(t, 'var', **{'$i_n': '1'})
    constant(t, 'cm', 3)
    set_var(t, 'set0', '0')
    t.mark('on')
    t.fire('cm', use='on')
    t.wait(0.9)
    t.fire('set0')
    t.mark('false')
    t.wait(0.9)
    t.fire('cm', use='off')
    t.expect.used('yes_cm', min=4, after='on', within=(0, 0.85))
    t.expect.used('no_cm', min=4, after='false')


@S.test('trigger_condition Ignore initial result (128): the first check after turning on fires nothing', doc=LOGIC,
        covers=('trigger_condition:128',))
def cond_ignore_first(t):
    info(t, 'var', **{'$i_n': '1'})
    constant(t, 'ign', 3, flags=1 | 128, interval='0.6')
    constant(t, 'ctl', 3, flags=1, interval='0.6')
    t.mark('on')
    t.fire('ign', use='on')
    t.fire('ctl', use='on')
    t.wait(0.8)
    t.fire('ign', use='off')
    t.fire('ctl', use='off')
    t.expect.used('yes_ctl', count=1, after='on', within=(0, 0.2))
    t.expect.not_used('yes_ign', after='on', within=(0, 0.2))
    t.expect.used('yes_ign', count=1, after='on', within=(0.5, 0.7))
    t.expect.used('yes_ctl', count=2)


# ---------------------------------------------------------------------------------------------- trigger_entity_iterator
def iterator(t, name='it', **kv):
    t.point('trigger_entity_iterator', targetname=name, **kv)


@S.test('trigger_entity_iterator Filter by class name: every entity of that class, each as activator', doc=LOGIC,
        covers=('trigger_entity_iterator.classname_filter',))
def it_class(t):
    for n in ('d1', 'd2', 'd3'):
        t.point('info_teleport_destination', targetname=n)
    info(t, 'other')
    info(t, 'hit')
    iterator(t, classname_filter='info_teleport_destination', target='hit', triggerstate='2')
    t.fire('it')
    t.wait(0.1)
    for n in ('d1', 'd2', 'd3'):
        t.expect.used('hit', activator=n, caller='it', count=1)
    t.expect.used('hit', count=3)


def status_case(status):
    what = {0: 'both', 1: 'only the living info_target', 2: 'only the corpse'}[status]

    @S.test('trigger_entity_iterator Filter by status %d: of an info_target and a monster_scientist_dead, %s' % (
            status, what), doc=LOGIC, covers=('trigger_entity_iterator.status_filter',
                                              'trigger_entity_iterator.status_filter=%d' % status))
    def case(t):
        info(t, 'thing', at=(-100, 0, 0))
        t.point('monster_scientist_dead', targetname='thing', at=(100, 0, 0))
        info(t, 'hit')
        iterator(t, name_filter='thing', status_filter=str(status), target='hit', triggerstate='2')
        t.fire('it')
        t.wait(0.1)
        t.expect.used('hit', activator='@info_target', count=0 if status == 2 else 1)
        t.expect.used('hit', activator='@monster_scientist_dead', count=0 if status == 1 else 1)
    case.__name__ = 'it_status%d' % status
    return case


for _st in (0, 1, 2):
    status_case(_st)


def state_case(ts):
    @S.test('trigger_entity_iterator Trigger state %d: the target is used with %s' % (ts, ('Off', 'On')[ts]),
            doc=LOGIC, covers=('trigger_entity_iterator.triggerstate=%d' % ts,))
    def case(t):
        info(t, 'thing')
        info(t, 'hit')
        iterator(t, name_filter='thing', target='hit', triggerstate=str(ts))
        t.fire('it')
        t.wait(0.1)
        t.expect.used('hit', use=ts, count=1)
    case.__name__ = 'it_state%d' % ts
    return case


for _ts in (0, 1):
    state_case(_ts)


@S.test('trigger_entity_iterator Delay between entities: one entity every 0.2 s', doc=LOGIC,
        covers=('trigger_entity_iterator.delay_between_triggers',))
def it_delay(t):
    for i in range(3):
        info(t, 'thing', at=(-100 + 100 * i, 0, 0))
    info(t, 'hit')
    info(t, 'done')
    iterator(t, name_filter='thing', target='hit', triggerstate='2', delay_between_triggers='0.2',
             trigger_after_run='done')
    t.mark('go')
    t.fire('it')
    t.wait(0.7)
    t.expect.used('hit', count=1, after='go', within=(0, 0.05))
    t.expect.used('hit', count=1, after='go', within=(0.15, 0.25))
    t.expect.used('hit', count=1, after='go', within=(0.35, 0.45))
    t.expect.used('hit', count=3)
    t.expect.used('done', count=1, after='go', within=(0.35, 0.45))


@S.test('trigger_entity_iterator Run mode 0 (Run once): a trigger during a run stops it', doc=LOGIC,
        covers=('trigger_entity_iterator.run_mode=0', 'trigger_entity_iterator.delay_between_triggers'))
def it_mode0(t):
    for i in range(3):
        info(t, 'thing', at=(-100 + 100 * i, 0, 0))
    info(t, 'hit')
    iterator(t, name_filter='thing', target='hit', triggerstate='2', delay_between_triggers='0.2', run_mode='0')
    t.fire('it')
    t.wait(0.1)
    t.fire('it')
    t.wait(0.6)
    t.expect.used('hit', count=1)


@S.test('trigger_entity_iterator Run mode 1: two triggers run two full runs side by side', doc=LOGIC,
        covers=('trigger_entity_iterator.run_mode=1', 'trigger_entity_iterator.delay_between_triggers'))
def it_mode1(t):
    for i in range(3):
        info(t, 'thing', at=(-100 + 100 * i, 0, 0))
    info(t, 'hit')
    info(t, 'done')
    iterator(t, name_filter='thing', target='hit', triggerstate='2', delay_between_triggers='0.2', run_mode='1',
             trigger_after_run='done')
    t.fire('it')
    t.wait(0.1)
    t.fire('it')
    t.wait(0.7)
    t.expect.used('hit', count=6)
    t.expect.used('done', count=2)


@S.test('trigger_entity_iterator Run mode 2 (toggle): runs repeat every Delay between runs, up to Maximum runs',
        doc=LOGIC, covers=('trigger_entity_iterator.run_mode=2', 'trigger_entity_iterator.delay_between_runs',
                           'trigger_entity_iterator.maximum_runs'))
def it_mode2_max(t):
    info(t, 'thing', at=(-50, 0, 0))
    info(t, 'thing', at=(50, 0, 0))
    info(t, 'hit')
    info(t, 'done')
    iterator(t, name_filter='thing', target='hit', triggerstate='2', run_mode='2', delay_between_runs='0.3',
             maximum_runs='3', trigger_after_run='done')
    t.mark('go')
    t.fire('it')
    t.wait(1.3)
    t.expect.used('hit', count=6)
    t.expect.used('done', count=3)
    t.expect.used('done', count=1, after='go', within=(0.25, 0.35))
    t.expect.used('done', count=1, after='go', within=(0.55, 0.65))


@S.test('trigger_entity_iterator Run mode 2 without a maximum: repeats until triggered again', doc=LOGIC,
        covers=('trigger_entity_iterator.run_mode=2', 'trigger_entity_iterator.delay_between_runs',
                'trigger_entity_iterator.maximum_runs'))
def it_mode2_toggle(t):
    info(t, 'thing')
    info(t, 'hit')
    iterator(t, name_filter='thing', target='hit', triggerstate='2', run_mode='2', delay_between_runs='0.2',
             maximum_runs='0')
    t.fire('it')
    t.wait(1.05)
    t.fire('it')
    t.mark('off')
    t.wait(0.6)
    t.expect.used('hit', min=5)
    t.expect.not_used('hit', after='off')
