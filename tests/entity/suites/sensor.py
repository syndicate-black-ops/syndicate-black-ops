"""trigger_sensor (docs/LOGIC_ENTITIES.md#trigger_sensor, Featureful): fires its target for the entities near it that
pass its filters, each as activator. Its own suite: the last test leaves player2 in spectator mode.

Keys are always given explicitly (the code's defaults without a key differ from the FGD's). The kit puts an
info_target named "center" in every room: the tests filter by class or count it."""

from etest import Suite

SUITE = Suite('sensor', 'trigger_sensor (feat/trigger-sensor)', players=2)
S = SUITE
DOC = 'LOGIC_ENTITIES.md#trigger_sensor'


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', at=at, targetname=name, **kv)


def sensor(t, name, target, **kv):
    keys = dict(radius='128', line_blocked='0', trace_to='0', fire_trigger_policy='0', alive_filter='0',
                takedamage_filter='0', classify='0', relationship_filter='0')
    keys.update(kv)
    at = keys.pop('at', (0, 0, 32))
    return t.point('trigger_sensor', at=at, targetname=name, target=target, names=('position',), **keys)


@S.test('every entity in the radius: the target fires once for each, that entity as activator', doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.radius', 'trigger_sensor.target', 'trigger_sensor.sensed_classname',
                'trigger_sensor.fire_trigger_policy=0'))
def every(t):
    for at in ((-60, 0, 32), (0, 60, 32), (60, 0, 32)):
        info(t, 'item', at)
    info(t, 'far', (200, 0, 32))
    info(t, 'hit', (-200, -200, 32))
    sensor(t, 's', 'hit', sensed_classname='info_target')
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', caller='s', activator='item', count=3)
    t.expect.fired('hit', activator='far', count=0)


@S.test('closest / farthest entity: the target fires once, for the nearest / the farthest', doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.radius', 'trigger_sensor.fire_trigger_policy=1',
                'trigger_sensor.fire_trigger_policy=2'))
def closest_farthest(t):
    for n, at in (('near', (40, 0, 32)), ('mid', (0, 80, 32)), ('away', (-120, 0, 32))):
        t.point('path_corner', at=at, targetname=n)
    info(t, 'hitc', (-200, -200, 32))
    info(t, 'hitf', (200, -200, 32))
    sensor(t, 'sc', 'hitc', radius='160', sensed_classname='path_corner', fire_trigger_policy='1')
    sensor(t, 'sf', 'hitf', radius='160', sensed_classname='path_corner', fire_trigger_policy='2')
    t.fire('sc')
    t.fire('sf')
    t.wait(0.1)
    t.expect.fired('hitc', caller='sc', activator='near', count=1)
    t.expect.fired('hitc', count=1)
    t.expect.fired('hitf', caller='sf', activator='away', count=1)
    t.expect.fired('hitf', count=1)


@S.test('line blocked by walls: an entity behind a func_wall is not sensed; with "Nothing" it is', doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.line_blocked=0', 'trigger_sensor.line_blocked=1'))
def line_blocked(t):
    info(t, 'seen', (60, 0, 32))
    info(t, 'hidden', (0, 80, 32))
    t.brush('func_wall', size=(96, 16, 128), at=(0, 40, 0))
    info(t, 'hitw', (-200, -200, 32))
    info(t, 'hitn', (200, -200, 32))
    sensor(t, 'sw', 'hitw', sensed_classname='info_target', line_blocked='1')
    sensor(t, 'sn', 'hitn', sensed_classname='info_target', line_blocked='0')
    t.fire('sw')
    t.fire('sn')
    t.wait(0.1)
    t.expect.fired('hitw', activator='seen', count=1)
    t.expect.fired('hitw', activator='hidden', count=0)
    t.expect.fired('hitn', activator='hidden', count=1)


@S.test('damageable and alive filters: a func_breakable passes "Yes" and "Alive only", an info_target "No"', doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.takedamage_filter=1', 'trigger_sensor.takedamage_filter=-1',
                'trigger_sensor.alive_filter=1'))
def damage_alive(t):
    t.brush('func_breakable', targetname='crate', health='50', at=(40, 0, 16))
    info(t, 'plain', (-40, 0, 32))
    for n, x in (('hy', -200), ('hn', 0), ('ha', 200)):
        info(t, n, (x, -200, 32))
    sensor(t, 'sy', 'hy', takedamage_filter='1')
    sensor(t, 'sn', 'hn', takedamage_filter='-1', sensed_classname='info_target')
    sensor(t, 'sa', 'ha', alive_filter='1')
    t.fire('sy')
    t.fire('sn')
    t.fire('sa')
    t.wait(0.1)
    t.expect.fired('hy', activator='crate', count=1)
    t.expect.fired('hy', count=1)
    t.expect.fired('hn', activator='plain', count=1)
    t.expect.fired('hn', activator='crate', count=0)
    t.expect.fired('ha', activator='crate', count=1)
    t.expect.fired('ha', activator='plain', count=0)


@S.test('Source Position [LP]: an entity name moves the search center there', doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.position'))
def position(t):
    info(t, 'center', (150, 150, 32))
    info(t, 'item', (180, 150, 32))
    info(t, 'item', (0, 40, 32))            # next to the sensor itself: outside the moved sphere
    info(t, 'hit', (-200, -200, 32))
    sensor(t, 's', 'hit', radius='64', sensed_classname='info_target', position='center', at=(0, 0, 32))
    t.fire('s')
    t.wait(0.1)
    t.expect.fired('hit', activator='center', count=1)
    t.expect.fired('hit', activator='item', count=1)
    t.expect.fired('hit', count=2)


@S.test('a target that triggers the sensor again while it fires: ignored, no recursion', doc=DOC,
        covers=('trigger_sensor',))
def recursion(t):
    info(t, 'item', (40, 0, 32))
    t.point('trigger_relay', at=(-200, -200, 32), targetname='back', target='s', triggerstate='2')
    sensor(t, 's', 'back', sensed_classname='info_target')
    t.fire('s')
    t.wait(0.1)
    # two sensed (item and the room's "center"): two re-triggers, each ignored, so "back" fires only twice
    t.expect.fired('back', activator='item', count=1)
    t.expect.fired('back', count=2)
    t.expect.used('s', caller='back', count=2)


@S.test('relationship: as human military players are enemies, as player ally allies, as human passive neither; '
        'a spectator is never sensed', players=2, doc=DOC,
        covers=('trigger_sensor', 'trigger_sensor.classify=3', 'trigger_sensor.classify=4',
                'trigger_sensor.classify=11', 'trigger_sensor.relationship_filter=2',
                'trigger_sensor.relationship_filter=4'))
def players(t):
    for n, x in (('he', -200), ('ha', 0), ('hp', 200)):
        info(t, n, (x, 200, 32))
    kv = dict(at=(0, -120, 40), radius='200', sensed_classname='player', line_blocked='0')
    sensor(t, 'se', 'he', classify='4', relationship_filter='2', **kv)
    sensor(t, 'sa', 'ha', classify='11', relationship_filter='4', **kv)
    sensor(t, 'sp', 'hp', classify='3', relationship_filter='2', **kv)
    t.fire('se')
    t.fire('sa')
    t.fire('sp')
    t.wait(0.2)
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.mark('spec')
    t.fire('se')
    t.wait(0.2)
    t.expect.fired('he', activator='player1', count=2)
    t.expect.fired('he', activator='player2', count=1)
    t.expect.fired('he', activator='player2', count=0, after='spec')
    t.expect.fired('ha', activator='player1', count=1)
    t.expect.fired('ha', activator='player2', count=1)
    t.expect.fired('hp', count=0)
