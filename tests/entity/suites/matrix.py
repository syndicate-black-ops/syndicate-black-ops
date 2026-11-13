"""Combinations ("X with Y"): every value writer against every entity it is meant to work on.

Writers: trigger_changevalue and trigger_copyvalue changing the destination's `target` at run time
(docs/TRIGGER_CHANGEVALUE.md "Works: values read every time they are used: ... target"). Fixtures: entities that
fire their `target` when they act; each must then fire the new target and not the old one, and still work.
"""

from etest import Suite

SUITE = Suite('matrix', 'X with Y: value writers x the entities they retarget')
S = SUITE


def door(t):
    t.brush('func_door', size=(64, 16, 96), at=(0, 96, 0), tex='666MURMETAL', targetname='fx', angles='-90 0 0',
            speed='400', wait='-1', lip='8', target='old_out')
    return lambda: t.fire('fx'), 0.6, 'fx'


def breakable(t):
    t.brush('func_breakable', at=(0, 96, 0), targetname='fx', health='10', target='old_out', material='1')
    # it clears its own name before it fires (func_break.cpp Die): the caller is known by its class only
    return lambda: t.damage('fx', 50), 0.3, '@func_breakable'


def button(t):
    t.brush('func_button', size=(16, 32, 32), at=(0, 120, 40), tex='+0~ANZYG1', targetname='fx', target='old_out',
            angles='0 90 0', wait='1', speed='100', lip='12')
    return lambda: t.fire('fx'), 0.6, 'fx'


def relay(t):
    t.point('trigger_relay', targetname='fx', target='old_out', triggerstate='2')
    return lambda: t.fire('fx'), 0.2, 'fx'


def timer(t):
    t.point('game_timer', targetname='fx', timer_length='0.5', target='old_out', warn_time='0')
    return lambda: t.fire('fx', use='on'), 0.9, 'fx'


FIXTURES = {'func_door': door, 'func_breakable': breakable, 'func_button': button, 'trigger_relay': relay,
            'game_timer': timer}


def changevalue(t):
    t.point('trigger_changevalue', targetname='w', target='fx', m_iszValueName='target', m_iszNewValue=t.name('new_out'))


def copyvalue(t):
    t.point('info_target', targetname='src', target='new_out')
    t.point('trigger_copyvalue', targetname='w', netname='src', m_iszSrcValueName='target', target='fx',
            m_iszDstValueName='target', names=('netname',))


WRITERS = {'trigger_changevalue': changevalue, 'trigger_copyvalue': copyvalue}


def make(writer, fixture):
    def body(t):
        go, settle, caller = FIXTURES[fixture](t)
        WRITERS[writer](t)
        t.point('info_target', targetname='old_out')
        t.point('info_target', targetname='new_out')
        t.fire('w')
        t.wait(0.05)
        t.mark('act')
        go()
        t.wait(settle)
        t.expect.fired('new_out', caller=caller, min=1, after='act')
        t.expect.not_fired('old_out')
    body.__name__ = '%s_x_%s' % (writer, fixture)
    return body


for w in WRITERS:
    for f in FIXTURES:
        S.test('%s retargets %s' % (w, f), doc='TRIGGER_CHANGEVALUE.md#limits-and-what-is-not-safe',
               covers=(w, f, f + '.target') + ((w + '.target', w + '.m_iszValueName', w + '.m_iszNewValue')
                                               if w == 'trigger_changevalue' else
                                               (w + '.netname', w + '.m_iszSrcValueName', w + '.target',
                                                w + '.m_iszDstValueName')))(make(w, f))
