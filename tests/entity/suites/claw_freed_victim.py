"""A claw hit whose victim is freed inside its own TakeDamage. CheckTraceHullAttack (combat.cpp) and the
Gargantua's GargantuaCheckTraceHullAttack damage the entity their hull trace hits and return it; the claw code
then reads it (punchangle on FL_MONSTER / FL_CLIENT victims). A plugin's TakeDamage hook can remove the victim
with REMOVE_ENTITY, which frees its private data at once (the Redeemer's warhead hitbox, shot down by an Alien
Slave's claw). The attack must then count as a miss, not read the freed entity.

test_remove_on_damage (test layer) is a damageable box that frees itself in TakeDamage and hands the freed block
to a new allocation filled with 0xFF, as the next allocation of that size reuses it in the game; test_animevent
makes the monster handle its claw event at once."""

from etest import Suite

SUITE = Suite('claw_freed_victim', 'claws: a victim freed inside its own TakeDamage is a miss', players=0)
S = SUITE
ROOM = (512, 512, 256)


def claw_hits_self_removing_box(t, classname, event, box_at):
    t.point(classname, at=(0, 0, 0), targetname='npc', angles='0 0 0')
    t.point('test_remove_on_damage', at=box_at, targetname='box')
    t.wait(0.5)
    t.console('test_animevent %s %d' % (t.ref('npc'), event))
    t.wait(0.5)
    t.expect.damaged('box', attacker='npc', min=1)
    t.expect.removed('box')
    # the freed block holds other data, as in the game
    t.expect.console('test_remove_on_damage: block reused (hit by %s)' % classname)
    t.expect.console('test_animevent: %s event %d handled' % (t.ref('npc'), event))  # the claw code returned
    t.expect.sound('miss', ent='npc')                             # nothing left to hit: the miss sound


@S.test('monster_alien_slave claw (CheckTraceHullAttack) on an entity freed in TakeDamage: a miss, no crash',
        size=ROOM, covers=('monster_alien_slave',))
def slave_claw(t):
    claw_hits_self_removing_box(t, 'monster_alien_slave', 1, (50, 0, 32))       # ISLAVE_AE_CLAW


@S.test('monster_gargantua slash (GargantuaCheckTraceHullAttack) on an entity freed in TakeDamage: a miss, no crash',
        size=ROOM, covers=('monster_gargantua',))
def garg_slash(t):
    claw_hits_self_removing_box(t, 'monster_gargantua', 1, (70, 0, 48))         # GARG_AE_SLASH_LEFT
