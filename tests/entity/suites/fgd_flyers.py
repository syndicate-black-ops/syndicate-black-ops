"""Tier 5, final pass: Monster keys on the Apache and the Osprey, which have their own Killed (apache.cpp,
osprey.cpp: CBaseMonster::Killed never runs for them). "Trigger on death" (trigger_on_death) fires from their Killed
too (CBaseMonster::FireTriggerOnDeath), once, with the killer as the activator. Both wait for a trigger (64), so they
hover where they are placed."""

from etest import Suite

SUITE = Suite('fgd_flyers', 'Tier 5: Apache and Osprey monster keys', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


def death_case(cls, z):
    @S.test('%s "trigger_on_death": killed by the player, it fires the target once, the player as activator; one '
            'without the key fires nothing' % cls, players=1, doc=DOC, covers=('%s.trigger_on_death' % cls,))
    def case(t):
        t.point('info_target', targetname='dead')
        t.point(cls, targetname='fly', at=(0, 0, z), angles='0 90 0', spawnflags='64', trigger_on_death='dead')
        t.point(cls, targetname='quiet', at=(0, 0, z + 120), angles='0 90 0', spawnflags='64')
        t.flags('player1', '+FL_NOTARGET')
        t.flags('player1', '+FL_GODMODE')         # the crash blasts
        t.wait(0.5)
        t.mark('kill')
        t.damage('fly', 100000, 0, 'player1')
        t.damage('quiet', 100000, 0, 'player1')
        t.wait_frames(1)
        t.snap('s', 'fly', 'deadflag')          # (the Osprey's crash removes it soon after)
        t.wait(0.5)
        t.flags('player1', '-FL_NOTARGET')
        t.flags('player1', '-FL_GODMODE')
        t.expect.used('dead', activator='player1', count=1, after='kill')
        t.expect.value('s', 'fly', 'deadflag', 0, op='>')
    case.__name__ = 'death_' + cls.replace('monster_', '')
    return case


death_case('monster_apache', 100)
death_case('monster_osprey', 100)
death_case('monster_snark', 0)          # CSqueakGrenade::Killed goes on to CBaseMonster::Killed


@S.test('monster_snark Trigger Condition Death (4) fires TriggerTarget when it is killed; No Trigger (0) fires '
        'nothing', players=1, doc=DOC,
        covers=('monster_snark.triggercondition=4', 'monster_snark.triggercondition=0', 'monster_snark.triggertarget'))
def snark_trigger_condition(t):
    t.point('info_target', targetname='died')
    t.point('info_target', targetname='never')
    t.point('monster_snark', targetname='pd', at=(-60, 0, 0), angles='0 90 0', TriggerCondition='4',
            TriggerTarget='died', names=('TriggerTarget',))
    t.point('monster_snark', targetname='pn', at=(60, 0, 0), angles='0 90 0', TriggerCondition='0',
            TriggerTarget='never', names=('TriggerTarget',))
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')         # the blasts
    t.wait(0.3)
    t.mark('kill')
    t.damage('pd', 1000, 0, 'player1')
    t.damage('pn', 1000, 0, 'player1')
    t.wait(0.5)
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')
    t.expect.used('died', count=1, after='kill')
    t.expect.not_used('never')
