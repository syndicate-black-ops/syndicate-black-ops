"""Damage whose victim is freed inside its own TakeDamage. A plugin's TakeDamage hook can remove the victim with
REMOVE_ENTITY, which frees its private data at once (the sentry plugin's unbuilt base, before Sentry guns 1.0.8).
::RadiusDamage (env_explosion, grenades, rockets) continued its sphere search from the freed entity, and env_beam's
damage read the freed entity again (Decal End, No Camera Punch): both read freed memory. The search now goes on from
the edict, and the beam ends its hit there.

test_remove_on_damage (test layer) is a damageable box that frees itself in TakeDamage and hands the freed block
to a new allocation filled with 0xFF, as the next allocation of that size reuses it in the game."""

from etest import Suite

SUITE = Suite('damage_freed_victim', 'damage: a victim freed inside its own TakeDamage is not read again', players=0)
S = SUITE
ROOM = (512, 512, 256)


@S.test('env_explosion (::RadiusDamage) on two entities that free themselves in TakeDamage: both hit, no crash',
        size=ROOM, covers=('env_explosion',))
def explosion(t):
    t.point('test_remove_on_damage', at=(48, 0, 16), targetname='near')     # found first (lower edict)
    t.point('test_remove_on_damage', at=(96, 0, 16), targetname='far')
    t.point('env_explosion', targetname='boom', at=(0, 0, 16), iMagnitude='200')
    t.wait(0.5)
    t.fire('boom')
    t.wait(0.5)
    t.expect.removed('near')
    t.expect.removed('far')                     # the search went on past the freed entity
    t.expect.console('test_remove_on_damage: freed')


@S.test('env_beam Decal End (64) damage on an entity that frees itself in TakeDamage: the hit ends there, no crash',
        size=ROOM, covers=('env_beam:64',))
def beam(t):
    t.point('info_target', targetname='beam_s', at=(-120, 0, 16))
    t.point('test_remove_on_damage', at=(0, 0, 16), targetname='box')
    t.point('info_target', targetname='beam_e', at=(120, 0, 16))
    t.point('env_beam', targetname='beam', LightningStart='beam_s', LightningEnd='beam_e', renderamt='100',
            BoltWidth='10', texture='sprites/laserbeam.spr', life='0', damage='50', spawnflags='64',
            names=('LightningStart', 'LightningEnd'))
    t.wait(0.3)
    t.fire('beam', use='on')
    t.wait(1.0)
    t.fire('beam', use='off')
    t.wait(0.3)
    t.expect.removed('box')
    t.expect.console('test_remove_on_damage: freed')
