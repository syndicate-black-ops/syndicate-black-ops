"""Tier 5, final pass: the See Player AI trigger conditions (1, 10, 11, as TriggerCondition and TriggerAltCondition)
on the classes with their own Look (the hunting monsters, the Cyborg, the Cyber Minotaur: they look 12000 units
around, with no view cone or line of sight). Each test makes its monster (test_create, so no monster of another room
sees the player first), waits out the 5 s a named monster stands, shows the target is not fired while the player is
notarget, then lets the monster see him. 11 (Not In Combat): the monster is a player ally (is_player_ally), so seeing
him does not start a fight."""

from etest import Suite

SUITE = Suite('fgd_gen_hunter_ai', 'Tier 5: See Player trigger conditions on the hunting monsters', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
CLASSES = ('monster_hunter', 'monster_cyborg', 'monster_cyberminotor', 'monster_grunt_hunter', 'monster_imp_hunter',
           'monster_werewolf_hunter', 'monster_zombie_hunter')
NAMES = {1: 'See Player, Mad at Player', 10: 'See Player Unconditional', 11: 'See Player, Not In Combat'}


def case_for(cls, key, c):
    @S.test('%s %s %d (%s): its TriggerTarget fires once it sees the player, not while he is notarget' % (
                cls, key, c, NAMES[c]), players=1, doc=DOC,
            covers=('%s.%s=%d' % (cls, key.lower(), c), '%s.triggertarget' % cls))
    def case(t):
        t.point('info_target', targetname='sink')
        t.point(cls, targetname='pre', at=(200, 200, 0), angles='0 270 0', spawnflags='16')    # the class in the map
        t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
        t.point('trigger_relay', targetname='rm_m', killtarget='m')
        t.flags('player1', '+FL_NOTARGET')
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -200, 0), yaw=90)
        t.fire('rm_pre')
        p = t._abs((0, 150, 0))
        words = ['test_create', cls, '%g' % p[0], '%g' % p[1], '%g' % p[2], '270', 'targetname', t.name('m'),
                 'TriggerTarget', t.name('sink'), key, str(c), 'netname', t.name('sq')]
        if c == 11:
            words += ['is_player_ally', '1']
        t.console(' '.join(words))
        t.wait(7.0)                         # a named monster stands its first 5 s or more (SCHED_WAIT_TRIGGER)
        t.snap('quiet', 'm', 'state', 'schedule')
        t.mark('quiet')
        t.wait(1.5)
        t.mark('go')
        t.flags('player1', '-FL_NOTARGET')
        t.wait(4.0)
        t.snap('seen', 'm', 'state', 'schedule', 'enemy')
        t.flags('player1', '+FL_NOTARGET')
        t.fire('rm_m')
        t.wait(0.2)
        t.flags('player1', '-FL_GODMODE')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_used('sink', after='quiet', within=(0, 1.5))
        t.expect.used('sink', count=1, after='go')
    case.__name__ = '%s_%s_%d' % (cls.replace('monster_', ''), key.lower(), c)
    return case


for _cls in CLASSES:
    for _key in ('TriggerCondition', 'TriggerAltCondition'):
        for _c in (1, 10, 11):
            case_for(_cls, _key, _c)
