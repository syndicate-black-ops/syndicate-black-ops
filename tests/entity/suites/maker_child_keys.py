"""monstermaker "#<key>" keys (Half-Life Unified SDK): each child gets "<key>" before it spawns; the model, health and
name keys are not passed on (a developer 2 note)."""

from etest import Suite

SUITE = Suite('maker_child_keys', 'monstermaker #<key>: keys passed on to the children', players=0)
SUITE.server_args = ('+developer', '2')
S = SUITE


def maker(t, **keys):
    t.point('monstermaker', at=(0, 0, 0), targetname='mk', monstertype='monster_zombie', monstercount='1',
            netname='child', spawnflags='0', childspawnflags='16', names=('netname',), **keys)
    t.wait(0.3)
    t.fire('mk')
    t.wait(1.0)


@S.test('"#unkillable" 1 on a monstermaker: its zombie survives a lethal hit with 1 health',
        covers=('monstermaker', 'monster_zombie.unkillable'))
def child_unkillable(t):
    maker(t, **{'#unkillable': '1'})
    t.kv('child', 'health', '10')
    t.damage('child', 1000, 4096, 'world')
    t.wait(0.3)
    t.snap('after', 'child', 'health', 'deadflag')
    t.expect.value('after', 'child', 'health', 1)
    t.expect.value('after', 'child', 'deadflag', 0)


@S.test('"#model" and "#health" on a monstermaker are not passed on: the zombie keeps its own model and health',
        covers=('monstermaker',))
def child_refused(t):
    maker(t, **{'#model': 'models/scientist.mdl', '#health': '777'})
    t.snap('made', 'child', 'model', 'health')
    t.expect.value('made', 'child', 'model', 'models/zombie.mdl')
    t.expect.value('made', 'child', 'health', 777, op='<')
    t.expect.console('key "#model" is not passed on to its monsters')
    t.expect.console('key "#health" is not passed on to its monsters')
