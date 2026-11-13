"""Half-Life Unified SDK ports (fix/hlu-batch-1): trigger_relay "master", func_breakable "spawnobject" by class name,
monster "unkillable", multi_manager with more than 16 targets."""

from etest import Suite

SUITE = Suite('hlu_fixes', 'Half-Life Unified SDK ports: trigger_relay master, spawnobject by class name, unkillable', players=0)
SUITE.server_args = ('+developer', '1')    # the refusal notes are developer messages
S = SUITE


@S.test('trigger_relay with a locked master does not fire; once the master is unlocked it fires',
        covers=('trigger_relay', 'trigger_relay.master'))
def relay_master(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock')      # the multisource's one input
    t.point('trigger_relay', targetname='relay', target='out', master='lock', triggerstate='1')
    t.point('info_target', targetname='out')
    t.wait(0.2)
    t.mark('locked')
    t.fire('relay')
    t.wait(0.3)
    t.fire('key')                                                   # unlocks the multisource
    t.wait(0.3)
    t.mark('open')
    t.fire('relay')
    t.wait(0.3)
    t.expect.not_used('out', within=(0, 0.5))
    t.expect.used('out', caller='relay', count=1, after='open')


def crate(t, spawnobject, name='crate'):
    t.brush('func_breakable', size=(32, 32, 32), at=(0, 0, 16), targetname=name, health='1', material='1',
            spawnobject=spawnobject)
    t.wait(0.2)
    t.mark('break')
    t.damage(name, 50, 0, 'world')
    t.wait(0.5)


@S.test('func_breakable "spawnobject" as a class name (weapon_k444): the weapon spawns when it breaks',
        covers=('func_breakable', 'func_breakable.spawnobject'))
def spawnobject_classname(t):
    crate(t, 'weapon_k444')
    t.expect.removed('crate', after='break')
    t.expect.spawned('@weapon_k444', count=1, after='break')


@S.test('func_breakable "spawnobject" as a number of the list (1, battery) still works',
        covers=('func_breakable.spawnobject=1',))
def spawnobject_number(t):
    crate(t, '1')
    t.expect.spawned('@item_battery', count=1, after='break')


@S.test('func_breakable "spawnobject" naming a monster (monster_imp) spawns nothing and says why',
        covers=('func_breakable.spawnobject',))
def spawnobject_monster_refused(t):
    crate(t, 'monster_imp')
    t.expect.removed('crate', after='break')
    t.expect.absent('@monster_imp')
    t.expect.console('Cannot spawn entity "monster_imp": not an item')


def lethal(t, name='z'):
    """A hit far past its health (10 health, 1000 damage, DMG_NEVERGIB: some hits are scaled down): it dies without gibbing, so the body is still there."""
    t.kv(name, 'health', '10')
    t.damage(name, 1000, 4096, 'world')


def zombie(t, name, unkillable=None, at=(0, 0, 0)):
    keys = {} if unkillable is None else {'unkillable': unkillable}
    t.point('monster_zombie', targetname=name, at=at, angles='0 90 0', spawnflags='16', **keys)


@S.test('monster "unkillable" 1: lethal damage leaves it alive with 1 health', covers=('monster_zombie.unkillable',
        'monster_zombie.unkillable=1'))
def unkillable_survives(t):
    zombie(t, 'z', '1')
    t.wait(0.5)
    lethal(t)
    t.wait(0.3)
    t.snap('after', 'z', 'health', 'deadflag')
    t.expect.value('after', 'z', 'health', 1)
    t.expect.value('after', 'z', 'deadflag', 0)


@S.test('monster "unkillable" 0: lethal damage kills it', covers=('monster_zombie.unkillable=0',))
def unkillable_off_dies(t):
    zombie(t, 'z', '0')
    t.wait(0.5)
    lethal(t)
    t.wait(0.3)
    t.snap('after', 'z', 'deadflag')
    t.expect.value('after', 'z', 'deadflag', 0, op='>')


@S.test('monster "unkillable" switched at run time with trigger_changevalue: 1 survives a lethal hit, then 0 and '
        'the next lethal hit kills it', covers=('monster_zombie.unkillable', 'trigger_changevalue'))
def unkillable_runtime(t):
    zombie(t, 'z')
    t.point('trigger_changevalue', targetname='on', target='z', m_iszValueName='unkillable', m_iszNewValue='1')
    t.point('trigger_changevalue', targetname='off', target='z', m_iszValueName='unkillable', m_iszNewValue='0')
    t.wait(0.5)
    t.fire('on')
    t.wait(0.1)
    lethal(t)
    t.wait(0.3)
    t.snap('on', 'z', 'health', 'deadflag')
    t.fire('off')
    t.wait(0.1)
    lethal(t)
    t.wait(0.3)
    t.snap('off', 'z', 'deadflag')
    t.expect.value('on', 'z', 'health', 1)
    t.expect.value('on', 'z', 'deadflag', 0)
    t.expect.value('off', 'z', 'deadflag', 0, op='>')


@S.test('multi_manager with 20 targets fires all 20 (the limit is 64, Half-Life Unified SDK; it was 16)',
        covers=('multi_manager',))
def multi_manager_20(t):
    t.point('multi_manager', targetname='mm', **{'o%02d' % i: '0' for i in range(1, 21)})
    for i in range(1, 21):
        t.point('info_target', targetname='o%02d' % i, at=(-200 + 20 * i, 0, 0))
    t.wait(0.2)
    t.mark('go')
    t.fire('mm')
    t.wait(0.3)
    for i in range(1, 21):
        t.expect.used('o%02d' % i, caller='mm', count=1, after='go')
