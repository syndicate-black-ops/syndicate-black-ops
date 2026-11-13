"""Tier 5: FGD items of the logic entities that no 1.38 map uses and no other test asserted: trigger_random,
trigger_random_unique, trigger_timer, trigger_compare (docs/LOGIC_ENTITIES.md, docs/LOCUS.md). Each test checks what
the FGD and the docs say the key, flag or choice does. Round reset of these classes: suite fgd_rounds."""

from etest import Suite

SUITE = Suite('fgd_logic', 'Tier 5: logic entities, FGD-only keys and flags')
S = SUITE
LOGIC = 'LOGIC_ENTITIES.md'
LOCUS = 'LOCUS.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3

ALL_TARGETS = tuple('target%d' % i for i in range(1, 17))


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


def sixteen(t, cls, **kv):
    """A `cls` with all 16 targets t1..t16 (info_targets)."""
    for i in range(1, 17):
        kv['target%d' % i] = 't%d' % i
        info(t, 't%d' % i)
    return t.point(cls, targetname='rnd', **kv)


# ---------------------------------------------------------------------------------------------- trigger_random
@S.test('trigger_random Unique (16) with 16 targets: each target once, then Trigger when all fired, then nothing',
        doc=LOGIC, covers=('trigger_random', 'trigger_random:16', 'trigger_random.trigger_on_exhaust')
        + tuple('trigger_random.' + k for k in ALL_TARGETS))
def random_unique16(t):
    sixteen(t, 'trigger_random', spawnflags='16', trigger_on_exhaust='empty')
    info(t, 'empty')
    for i in range(16):
        t.fire('rnd')
    t.wait(0.1)
    t.mark('done')
    t.fire('rnd')
    t.wait(0.1)
    for i in range(1, 17):
        t.expect.fired('t%d' % i, caller='rnd', count=1)
        t.expect.not_fired('t%d' % i, after='done')
    t.expect.fired('empty', caller='rnd', count=1)


@S.test('trigger_random Unique + Reusable (4): the pool starts over after Trigger when all fired', doc=LOGIC,
        covers=('trigger_random:4', 'trigger_random:16', 'trigger_random.trigger_on_exhaust'))
def random_reusable(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', spawnflags=str(16 | 4),
            trigger_on_exhaust='empty')
    for n in ('a', 'b', 'empty'):
        info(t, n)
    for i in range(6):
        t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('a', count=3)
    t.expect.fired('b', count=3)
    t.expect.fired('empty', count=3)


@S.test('trigger_random first_target 3: the first trigger fires target 3', doc=LOGIC,
        covers=('trigger_random.first_target',))
def random_first(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', target3='c', target4='d',
            first_target='3')
    for n in 'abcd':
        info(t, n)
    t.mark('first')
    t.fire('rnd')
    t.wait(0.2)
    t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('c', count=1, after='first', within=(0, 0.1))
    t.expect.used('@info_target', caller='rnd', count=2)


@S.test('trigger_random Timed (8): a trigger turns it on, a target every Minimum..Maximum delay, the next trigger '
        'turns it off', doc=LOGIC,
        covers=('trigger_random:8', 'trigger_random.min_delay', 'trigger_random.max_delay'))
def random_timed(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', spawnflags='8', min_delay='0.3',
            max_delay='0.3')
    info(t, 'a')
    info(t, 'b')
    t.mark('on')
    t.fire('rnd', use='on')
    t.wait(1.05)
    t.fire('rnd', use='on')          # any use type toggles
    t.mark('off')
    t.wait(0.8)
    t.expect.used('@info_target', caller='rnd', activator='rnd', count=3, after='on')
    t.expect.used('@info_target', caller='rnd', count=1, after='on', within=(0.25, 0.35))
    t.expect.not_fired('a', after='off')
    t.expect.not_fired('b', after='off')


@S.test('trigger_random Timed: the delay is random between Minimum and Maximum delay', doc=LOGIC,
        covers=('trigger_random:8', 'trigger_random.min_delay', 'trigger_random.max_delay'))
def random_delay_range(t):
    t.point('trigger_random', targetname='rnd', target1='a', spawnflags=str(8 | 2), min_delay='0.4',
            max_delay='0.8')
    info(t, 'a')
    t.mark('on')
    t.fire('rnd')
    t.wait(1.2)
    t.expect.fired('a', count=1, after='on', within=(0.4, 0.8))


@S.test('trigger_random Start on (1) + Timed: fires without a trigger, itself the activator', doc=LOGIC,
        covers=('trigger_random:1', 'trigger_random:8'))
def random_start_on(t):
    t.point('trigger_random', targetname='rnd', target1='a', spawnflags=str(8 | 1), min_delay='0.25',
            max_delay='0.25')
    info(t, 'a')
    t.mark('go')
    t.wait(1.1)
    t.expect.fired('a', caller='rnd', activator='rnd', min=4, after='go')


@S.test('trigger_random Trigger once (2) + Timed: one fire, then off', doc=LOGIC,
        covers=('trigger_random:2', 'trigger_random:8'))
def random_once(t):
    t.point('trigger_random', targetname='rnd', target1='a', spawnflags=str(8 | 2), min_delay='0.2',
            max_delay='0.2')
    info(t, 'a')
    t.fire('rnd')
    t.wait(1.2)
    t.expect.fired('a', count=1)


@S.test('trigger_random Trigger number limit 3 (Timed): three fires, then Trigger on limit, then off', doc=LOGIC,
        covers=('trigger_random.trigger_number', 'trigger_random.trigger_on_limit', 'trigger_random:8'))
def random_limit(t):
    t.point('trigger_random', targetname='rnd', target1='a', spawnflags='8', min_delay='0.2', max_delay='0.2',
            trigger_number='3', trigger_on_limit='limit')
    info(t, 'a')
    info(t, 'limit')
    t.mark('on')
    t.fire('rnd')
    t.wait(1.4)
    t.expect.fired('a', count=3)
    t.expect.fired('limit', caller='rnd', count=1, after='on', within=(0.5, 0.7))
    t.expect.order('a', 'limit')


@S.test('trigger_random Scheduled order (64): still one target per trigger', doc=LOGIC,
        covers=('trigger_random:64',))
def random_scheduled_order(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', target3='c', spawnflags='64')
    for n in 'abc':
        info(t, n)
    for i in range(9):
        t.fire('rnd')
    t.wait(0.1)
    t.expect.used('@info_target', caller='rnd', count=9)


@S.test('trigger_random Scheduled delays (128) + Timed: delays stay between Minimum and Maximum', doc=LOGIC,
        covers=('trigger_random:128', 'trigger_random:8'))
def random_scheduled_delays(t):
    t.point('trigger_random', targetname='rnd', target1='a', spawnflags=str(128 | 8 | 2), min_delay='0.4',
            max_delay='0.8')
    info(t, 'a')
    t.mark('on')
    t.fire('rnd')
    t.wait(1.2)
    t.expect.fired('a', count=1, after='on', within=(0.4, 0.8))


# ---------------------------------------------------------------------------------------------- trigger_random_unique
@S.test('trigger_random_unique: each target once, Trigger when all fired, then nothing (not Reusable)', doc=LOGIC,
        covers=('trigger_random_unique', 'trigger_random_unique.target1', 'trigger_random_unique.target2',
                'trigger_random_unique.target3', 'trigger_random_unique.target_count',
                'trigger_random_unique.trigger_on_exhaust'))
def unique_once(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', target3='c', target4='x',
            target_count='3', trigger_on_exhaust='empty')
    for n in ('a', 'b', 'c', 'x', 'empty'):
        info(t, n)
    for i in range(3):
        t.fire('rnd')
    t.wait(0.1)
    t.fire('rnd')
    t.fire('rnd')
    t.wait(0.1)
    for n in 'abc':
        t.expect.fired(n, count=1)
    t.expect.not_fired('x')                     # target_count 3: slot 4 does not count
    t.expect.fired('empty', caller='rnd', count=1)


@S.test('trigger_random_unique with 16 targets: each once', doc=LOGIC,
        covers=tuple('trigger_random_unique.' + k for k in ALL_TARGETS))
def unique16(t):
    sixteen(t, 'trigger_random_unique')
    for i in range(16):
        t.fire('rnd')
    t.wait(0.1)
    for i in range(1, 17):
        t.expect.fired('t%d' % i, caller='rnd', count=1)


@S.test('trigger_random_unique Reusable (1): the pool starts over', doc=LOGIC,
        covers=('trigger_random_unique:1', 'trigger_random_unique.trigger_on_exhaust'))
def unique_reusable(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', spawnflags='1',
            trigger_on_exhaust='empty')
    for n in ('a', 'b', 'empty'):
        info(t, n)
    for i in range(6):
        t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('a', count=3)
    t.expect.fired('b', count=3)
    t.expect.fired('empty', count=3)


@S.test('trigger_random_unique Reusable + Not repetitive (32): never the same target twice in a row, also across '
        'pools', doc=LOGIC, covers=('trigger_random_unique:32', 'trigger_random_unique:1'))
def unique_not_repetitive(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', spawnflags=str(32 | 1))
    info(t, 'a')
    info(t, 'b')
    n = 8
    for i in range(n):
        t.mark('m%d' % i)
        t.fire('rnd')
        t.wait(0.5)
    for i in range(n - 1):
        # fires i and i+1 (0.5 s apart): one of each
        t.expect.fired('a', count=1, after='m%d' % i, within=(0, 0.7))
        t.expect.fired('b', count=1, after='m%d' % i, within=(0, 0.7))


@S.test('trigger_random_unique first_target 2: the first trigger fires target 2', doc=LOGIC,
        covers=('trigger_random_unique.first_target',))
def unique_first(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', target3='c', first_target='2')
    for n in 'abc':
        info(t, n)
    t.mark('first')
    t.fire('rnd')
    t.wait(0.2)
    t.fire('rnd')
    t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('b', count=1, after='first', within=(0, 0.1))
    for n in 'abc':
        t.expect.fired(n, count=1)


@S.test('trigger_random_unique Scheduled order (64): each target once all the same', doc=LOGIC,
        covers=('trigger_random_unique:64',))
def unique_scheduled(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', target3='c', spawnflags='64')
    for n in 'abc':
        info(t, n)
    for i in range(4):
        t.fire('rnd')
    t.wait(0.1)
    for n in 'abc':
        t.expect.fired(n, count=1)


# ---------------------------------------------------------------------------------------------- trigger_timer
@S.test('trigger_timer Start on (1): fires without a trigger', doc=LOGIC, covers=('trigger_timer:1',))
def timer_start_on(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.25', max_delay='0.25', spawnflags='1')
    info(t, 'tick')
    t.mark('go')
    t.wait(1.1)
    t.expect.fired('tick', caller='tm', activator='tm', min=4, after='go')


@S.test("trigger_timer Don't delay first fire (32): fires at once when turned on", doc=LOGIC,
        covers=('trigger_timer:32',))
def timer_no_first_delay(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.5', max_delay='0.5', spawnflags='32')
    info(t, 'tick')
    t.mark('on')
    t.fire('tm', use='on')
    t.wait(0.7)
    t.expect.fired('tick', count=1, after='on', within=(0, 0.1))
    t.expect.fired('tick', count=2, after='on')


@S.test('trigger_timer without 32: the first fire waits the delay', doc=LOGIC, covers=('trigger_timer',))
def timer_first_delay(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.5', max_delay='0.5')
    info(t, 'tick')
    t.mark('on')
    t.fire('tm', use='on')
    t.wait(0.7)
    t.expect.fired('tick', count=1, after='on', within=(0.45, 0.55))


@S.test('trigger_timer Forward activator (64): the target gets the activator that turned it on', doc=LOGIC,
        covers=('trigger_timer:64',))
def timer_forward(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.2', max_delay='0.2', spawnflags='64')
    info(t, 'tick')
    info(t, 'src')
    t.fire('tm', activator='src', use='on')
    t.wait(0.5)
    t.expect.fired('tick', caller='tm', activator='src', min=2)
    t.expect.fired('tick', activator='tm', count=0)


@S.test('trigger_timer without 64: the timer is the activator', doc=LOGIC, covers=('trigger_timer',))
def timer_no_forward(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.2', max_delay='0.2')
    info(t, 'tick')
    info(t, 'src')
    t.fire('tm', activator='src', use='on')
    t.wait(0.5)
    t.expect.fired('tick', caller='tm', activator='tm', min=2)


@S.test('trigger_timer Scheduled delays (128): delays stay between Minimum and Maximum', doc=LOGIC,
        covers=('trigger_timer:128',))
def timer_scheduled(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.4', max_delay='0.8', spawnflags='128',
            trigger_number='1')
    info(t, 'tick')
    t.mark('on')
    t.fire('tm', use='on')
    t.wait(1.2)
    t.expect.fired('tick', count=1, after='on', within=(0.4, 0.8))


@S.test("trigger_timer Don't reset trigger counter (256): the count goes on after Off / On", doc=LOGIC,
        covers=('trigger_timer:256', 'trigger_timer.trigger_number', 'trigger_timer.trigger_on_limit'))
def timer_keep_count(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.3', max_delay='0.3', spawnflags='256',
            trigger_number='3', trigger_on_limit='limit')
    info(t, 'tick')
    info(t, 'limit')
    t.fire('tm', use='on')
    t.wait(0.75)                     # two fires (0.3, 0.6)
    t.fire('tm', use='off')
    t.wait(0.2)
    t.mark('again')
    t.fire('tm', use='on')
    t.wait(1.2)
    t.expect.fired('tick', count=3)
    t.expect.fired('limit', count=1, after='again', within=(0.25, 0.35))


@S.test('trigger_timer without 256: Off resets the count', doc=LOGIC,
        covers=('trigger_timer.trigger_number', 'trigger_timer.trigger_on_limit'))
def timer_reset_count(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.3', max_delay='0.3', trigger_number='3',
            trigger_on_limit='limit')
    info(t, 'tick')
    info(t, 'limit')
    t.fire('tm', use='on')
    t.wait(0.75)
    t.fire('tm', use='off')
    t.wait(0.2)
    t.mark('again')
    t.fire('tm', use='on')
    t.wait(1.2)
    t.expect.fired('tick', count=5)
    t.expect.fired('limit', count=1, after='again', within=(0.85, 0.95))


# ---------------------------------------------------------------------------------------------- trigger_compare
CMP_TYPES = {1: ('>=', (False, True, True)), 2: ('>', (False, False, True)), 3: ('<=', (True, True, False)),
             4: ('<', (True, False, False)), 5: ('!=', (True, False, True))}


def compare_case(testtype):
    sign, results = CMP_TYPES[testtype]

    @S.test('trigger_compare testtype %d (%s): 1, 2 and 3 against 2 fire pass / fail' % (testtype, sign), doc=LOCUS,
            covers=('trigger_compare.testtype=%d' % testtype, 'trigger_compare.pass_target',
                    'trigger_compare.fail_target'))
    def case(t):
        for v in (1, 2, 3):
            t.point('trigger_compare', targetname='c%d' % v, initial_value=str(v), compare_value='2',
                    testtype=str(testtype), pass_target='pass%d' % v, fail_target='fail%d' % v)
            info(t, 'pass%d' % v)
            info(t, 'fail%d' % v)
            t.fire('c%d' % v)
        t.wait(0.1)
        for v, ok in zip((1, 2, 3), results):
            t.expect.fired('pass%d' % v, count=1 if ok else 0)
            t.expect.fired('fail%d' % v, count=0 if ok else 1)
    case.__name__ = 'compare_type%d' % testtype
    return case


for _tt in sorted(CMP_TYPES):
    compare_case(_tt)


@S.test('trigger_compare Fire on equal / not equal / less / greater: by the result alone', doc=LOCUS,
        covers=('trigger_compare.trigger_on_equal', 'trigger_compare.trigger_on_not_equal',
                'trigger_compare.trigger_on_less', 'trigger_compare.trigger_on_greater'))
def compare_results(t):
    for v in (1, 2, 3):
        t.point('trigger_compare', targetname='c%d' % v, initial_value=str(v), compare_value='2', testtype='0',
                trigger_on_equal='eq%d' % v, trigger_on_not_equal='ne%d' % v, trigger_on_less='lt%d' % v,
                trigger_on_greater='gt%d' % v)
        for k in ('eq', 'ne', 'lt', 'gt'):
            info(t, '%s%d' % (k, v))
        t.fire('c%d' % v)
    t.wait(0.1)
    expect = {1: ('ne', 'lt'), 2: ('eq',), 3: ('ne', 'gt')}
    for v, fired in expect.items():
        for k in ('eq', 'ne', 'lt', 'gt'):
            t.expect.fired('%s%d' % (k, v), caller='c%d' % v, count=1 if k in fired else 0)


@S.test('trigger_compare Fire on calc fail: a value that cannot be computed fires it, nothing else', doc=LOCUS,
        covers=('trigger_compare.trigger_on_fail',))
def compare_fail(t):
    t.point('trigger_compare', targetname='cmp', initial_value='no_such_entity_t5', compare_value='2', testtype='0',
            pass_target='pass', fail_target='fail', trigger_on_fail='calcfail', trigger_on_equal='eq',
            trigger_on_not_equal='ne')
    for n in ('pass', 'fail', 'calcfail', 'eq', 'ne'):
        info(t, n)
    t.fire('cmp')
    t.wait(0.1)
    t.expect.fired('calcfail', caller='cmp', count=1)
    for n in ('pass', 'fail', 'eq', 'ne'):
        t.expect.not_fired(n)
