"""Entity triggering (docs/ENTITY_TRIGGERING.md, from the Half-Life Unified SDK): a target or killtarget ending in '*'
matches every entity whose name starts with the rest; "!activator" / "!caller" name the activator and the entity
firing (FireTargets, CBaseDelay::SUB_UseTargets killtarget, UTIL_FindEntityByTargetname). A selector that names the
firing entity itself is not triggered again (it would repeat without end), and neither is a wildcard match of
the firing entity. Players are never removed by a killtarget."""

from etest import Suite

SUITE = Suite('entity_triggering', 'Wildcard targets and killtargets, !activator / !caller target selectors', players=1)
S = SUITE

FORWARD_ACTIVATOR = '64'


def info(t, name, at=(0, 0, 0)):
    return t.point('info_target', targetname=name, at=at)


@S.test('Wildcard target: "grp_*" triggers every entity whose name starts with grp_, and no other',
        covers=('trigger_relay.target',))
def wildcard_target(t):
    t.point('trigger_relay', targetname='r', target='grp_*', triggerstate='1')
    info(t, 'grp_a', at=(-64, 0, 0))
    info(t, 'grp_b', at=(64, 0, 0))
    info(t, 'grpx', at=(0, 64, 0))       # starts with "grp" but not "grp_"
    info(t, 'other_grp_c', at=(0, -64, 0))
    t.mark('go')
    t.fire('r')
    t.wait(0.2)
    t.expect.used('grp_a', caller='r', count=1, after='go')
    t.expect.used('grp_b', caller='r', count=1, after='go')
    t.expect.not_used('grpx', after='go')
    t.expect.not_used('other_grp_c', after='go')


@S.test('Wildcard killtarget: "kill_*" removes every entity whose name starts with kill_, and no other',
        covers=('trigger_relay.killtarget',))
def wildcard_killtarget(t):
    t.point('trigger_relay', targetname='r', killtarget='kill_*', triggerstate='1')
    info(t, 'kill_a', at=(-64, 0, 0))
    info(t, 'kill_b', at=(64, 0, 0))
    info(t, 'keep', at=(0, 64, 0))
    t.mark('go')
    t.fire('r')
    t.wait(0.3)
    t.expect.removed('kill_a', count=1, after='go', within=(0, 0.3))
    t.expect.removed('kill_b', count=1, after='go', within=(0, 0.3))
    t.expect.not_removed('keep', after='go')


@S.test('Wildcard in a name lookup: trigger_changevalue target "crate_*" changes every crate',
        covers=('trigger_changevalue.target',))
def wildcard_lookup(t):
    t.brush('func_breakable', targetname='crate_1', health='50', at=(-96, 0, 0))
    t.brush('func_breakable', targetname='crate_2', health='50', at=(96, 0, 0))
    t.brush('func_breakable', targetname='box', health='50', at=(0, 128, 0))
    t.point('trigger_changevalue', targetname='cv', target='crate_*', m_iszValueName='health', m_iszNewValue='25',
            m_iszValueType='1')
    t.fire('cv')
    t.wait(0.1)
    t.snap('s', 'crate_1', 'health')
    t.snap('s2', 'crate_2', 'health')
    t.snap('s3', 'box', 'health')
    t.expect.value('s', 'crate_1', 'health', 75)
    t.expect.value('s2', 'crate_2', 'health', 75)
    t.expect.value('s3', 'box', 'health', 50)


@S.test('Target "!activator": the relay (Forward activator) triggers the entity that activated it',
        covers=('trigger_relay.target', 'trigger_relay:64'))
def activator_target(t):
    t.point('trigger_relay', targetname='r', target='!activator', triggerstate='1', spawnflags=FORWARD_ACTIVATOR)
    info(t, 'act')
    info(t, 'bystander', at=(64, 0, 0))
    t.mark('go')
    t.fire('r', activator='act')
    t.wait(0.2)
    t.expect.used('act', caller='r', activator='act', count=1, after='go')
    t.expect.not_used('bystander', after='go')


@S.test('Target "!activator" on a stock relay: the relay is its own activator, so it is not triggered again',
        covers=('trigger_relay.target',))
def activator_target_self(t):
    t.point('trigger_relay', targetname='r', target='!activator', triggerstate='1')
    info(t, 'act')
    t.mark('go')
    t.fire('r', activator='act')
    t.wait(0.2)
    t.expect.used('r', count=1, after='go')      # the test's fire only: no loop
    t.expect.not_used('act', after='go')


@S.test('Target "!caller": the caller is the firing entity itself, so it is not triggered again (no loop)',
        covers=('trigger_relay.target',))
def caller_target(t):
    t.point('trigger_relay', targetname='r', target='!caller', triggerstate='1')
    info(t, 'src')
    t.mark('go')
    t.fire('r', caller='src', activator='src')
    t.wait(0.2)
    t.expect.used('r', count=1, after='go')
    t.expect.not_used('src', after='go')


@S.test('Target "!caller" through a delay: the delayed use is the caller; nothing else is triggered',
        covers=('trigger_relay.target', 'trigger_relay.delay'))
def caller_target_delay(t):
    t.point('trigger_relay', targetname='r', target='!caller', triggerstate='1', delay='0.2')
    info(t, 'src')
    t.mark('go')
    t.fire('r', caller='src', activator='src')
    t.wait(0.5)
    t.expect.used('r', count=1, after='go')
    t.expect.not_used('src', after='go')


@S.test('Killtarget "!activator": removes the entity that activated the relay (Forward activator)',
        covers=('trigger_relay.killtarget', 'trigger_relay:64'))
def activator_killtarget(t):
    t.point('trigger_relay', targetname='r', killtarget='!activator', triggerstate='1', spawnflags=FORWARD_ACTIVATOR)
    info(t, 'victim')
    info(t, 'bystander', at=(64, 0, 0))
    t.mark('go')
    t.fire('r', activator='victim')
    t.wait(0.3)
    t.expect.removed('victim', count=1, after='go', within=(0, 0.3))
    t.expect.not_removed('bystander', after='go')


@S.test('Killtarget "!activator" with a player activator: players are never removed; the target still fires',
        players=1, covers=('trigger_relay.killtarget',))
def activator_killtarget_player(t):
    t.point('trigger_relay', targetname='r', killtarget='!activator', target='out', triggerstate='1',
            spawnflags=FORWARD_ACTIVATOR)
    info(t, 'out')
    t.mark('go')
    t.fire('r', activator='player1')
    t.wait(0.3)
    t.expect.used('out', activator='player1', count=1, after='go')
    t.expect.not_removed('player1', after='go')


@S.test('Killtarget "!caller": the caller of a killtarget is the firing entity, so the relay removes itself',
        covers=('trigger_relay.killtarget',))
def caller_killtarget(t):
    t.point('trigger_relay', targetname='r', killtarget='!caller', target='out', triggerstate='1')
    info(t, 'src')
    info(t, 'out', at=(64, 0, 0))
    t.mark('go')
    t.fire('r', caller='src')
    t.wait(0.3)
    t.expect.removed('r', count=1, after='go', within=(0, 0.3))
    t.expect.not_removed('src', after='go')
    t.expect.used('out', count=1, after='go')     # the removed relay has no name any more (UTIL_Remove), so no caller=


@S.test('Wildcard target matching the firing entity: "room1_relay" with target "room1_*" triggers the other room1_ '
        'entities once and not itself (no loop)', covers=('trigger_relay.target',))
def wildcard_target_self(t):
    t.point('trigger_relay', targetname='room1_relay', target='room1_*', triggerstate='1')
    info(t, 'room1_a', at=(-64, 0, 0))
    info(t, 'room1_b', at=(64, 0, 0))
    t.mark('go')
    t.fire('room1_relay')
    t.wait(0.3)
    t.expect.used('room1_a', caller='room1_relay', count=1, after='go')
    t.expect.used('room1_b', caller='room1_relay', count=1, after='go')
    t.expect.used('room1_relay', count=1, after='go')     # the test's own fire only


@S.test('Wildcard killtarget matching the firing entity: "room2_relay" with killtarget "room2_*" removes the others '
        'and not itself', covers=('trigger_relay.killtarget',))
def wildcard_killtarget_self(t):
    t.point('trigger_relay', targetname='room2_relay', killtarget='room2_*', triggerstate='1')
    info(t, 'room2_a', at=(-64, 0, 0))
    t.mark('go')
    t.fire('room2_relay')
    t.wait(0.3)
    t.expect.removed('room2_a', after='go')
    t.expect.not_removed('room2_relay', after='go')


@S.test('trigger_changetarget KillTarget: "!activator" removes the entity that fired it, "!caller" the changetarget '
        'itself; a wildcard never removes a player or the changetarget; the target change still happens',
        players=1, covers=('trigger_changetarget.killtarget',))
def changetarget_killtarget(t):
    for n in ('rl_a', 'rl_c', 'rl_w', 'rl_p'):
        t.point('trigger_relay', targetname=n, target='old', triggerstate='1')
    t.point('trigger_changetarget', targetname='ct_a', target='rl_a', m_iszNewTarget='new', killtarget='!activator',
            names=('m_iszNewTarget',))
    t.point('trigger_changetarget', targetname='ct_c', target='rl_c', m_iszNewTarget='new', killtarget='!caller',
            names=('m_iszNewTarget',))
    t.point('trigger_changetarget', targetname='ct_w', target='rl_w', m_iszNewTarget='new', killtarget='ct_*',
            names=('m_iszNewTarget',))
    t.point('trigger_changetarget', targetname='ct_p', target='rl_p', m_iszNewTarget='new', killtarget='!activator',
            names=('m_iszNewTarget',))
    info(t, 'victim')
    info(t, 'ct_other', at=(64, 0, 0))
    t.wait(0.2)
    t.mark('go')
    t.fire('ct_a', activator='victim')
    t.fire('ct_c')
    t.fire('ct_p', activator='player1')
    t.fire('ct_w')
    t.wait(0.3)
    t.snap('s', 'rl_a', 'target')
    t.snap('s', 'rl_c', 'target')
    t.snap('s', 'rl_w', 'target')
    t.snap('s', 'rl_p', 'target')
    x = t.expect
    x.removed('victim', count=1, after='go')
    x.removed('ct_c', count=1, after='go')
    x.removed('ct_other', count=1, after='go')
    x.not_removed('ct_w', after='go')
    x.not_removed('player1', after='go')
    for n in ('rl_a', 'rl_c', 'rl_w', 'rl_p'):
        x.value('s', n, 'target', t.name('new'))
