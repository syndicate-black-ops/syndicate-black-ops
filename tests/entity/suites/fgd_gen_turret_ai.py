"""Tier 5, final pass: the AI trigger conditions (Monster TriggerCondition / TriggerAltCondition) on the turrets
(monster_turret, monster_miniturret, monster_sentry), generated per class and choice. Turrets run their own think,
never the schedule AI that checks these conditions; they check them when they take a new enemy (See Player ...),
when a hit leaves them alive (Take Damage, 50% Health Remaining) and when their death ends (Death)
(turret.cpp CBaseTurret::AcquiredEnemyTrigger / DamageTrigger / TurretDeath, as Featureful). Each test first shows
the TriggerTarget is not fired before its condition, then makes the condition happen."""

from etest import Suite

SUITE = Suite('fgd_gen_turret_ai', 'Tier 5: turret AI trigger conditions, every choice a turret can meet', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
TURRETS = ('monster_turret', 'monster_miniturret', 'monster_sentry')
KEYS = ('TriggerCondition', 'TriggerAltCondition')
NAMES = {0: 'No Trigger', 1: 'See Player, Mad at Player', 2: 'Take Damage', 3: '50% Health Remaining', 4: 'Death',
         10: 'See Player Unconditional', 11: 'See Player, Not In Combat', 13: 'Got healed'}


def turret(t, cls, key, c):
    t.point('info_target', targetname='sink')
    # a copy so the map precaches the class (a Prisoner, removed at once); the turret is made once the player is
    # notarget: one placed in the map could see him as he enters the room and fire its See Player target then
    t.point(cls, targetname='pre', at=(200, -200, 0), angles='0 90 0', spawnflags='16')
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.point('trigger_relay', targetname='rm_tur', killtarget='tur')
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')       # all through the test: a turret that took him keeps firing at him
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.fire('rm_pre')
    p = t._abs((0, 150, 0))
    # Autostart (32): it deploys by itself once it sees the player
    t.console('test_create %s %g %g %g 270 targetname %s spawnflags 32 TriggerTarget %s %s %d' % (
        cls, p[0], p[1], p[2], t.name('tur'), t.name('sink'), key, c))


def see(t):
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.flags('player1', '-FL_NOTARGET')      # in its view: it takes him as its enemy
    t.snap('seen', 'player1', 'health', 'deadflag', 'origin')
    t.wait(5.0)
    t.flags('player1', '+FL_NOTARGET')


def case_for(cls, key, c):
    @S.test('%s %s %d (%s): its TriggerTarget fires once the condition happens, not before' % (cls, key, c, NAMES[c]),
            players=1, doc=DOC, covers=('%s.%s=%d' % (cls, key.lower(), c), '%s.triggertarget' % cls))
    def case(t):
        turret(t, cls, key, c)
        t.wait(0.5)
        t.mark('quiet')
        t.wait(1.5)
        # what the turret holds before the player is shown (a fire before 'go' names its enemy here)
        t.snap('pre_go', 'tur', 'enemy', 'enemy_name', 'conditions')
        t.snap('pre_go', 'player1', 'flags', 'origin')
        t.mark('go')
        if c in (1, 10, 11):
            see(t)
        elif c == 2:
            t.damage('tur', 1, 2)
            t.wait(0.3)
        elif c == 3:
            t.damage('tur', 1, 2)           # a scratch: well over half its health
            t.wait(0.3)
            t.mark('half')
            t.kv('tur', 'health', '5')      # under half its health (max_health is its spawn health)
            t.damage('tur', 1, 2)
            t.wait(0.3)
        elif c == 13:
            t.kv('tur', 'health', '50')
            # a trigger_hurt with negative damage heals it (CBaseMonster::TakeHealth remembers it); the next hit
            # checks the conditions
            t.trigger('trigger_hurt', size=(96, 96, 96), at=(0, 150, -16), targetname='heal', dmg='-20',
                      spawnflags='2')      # Start Off
            t.fire('heal', use='on')
            t.wait(0.6)
            t.fire('heal', use='off')
            t.mark('hit')
            t.damage('tur', 1, 2)
            t.wait(0.3)
        elif c == 4:
            if cls != 'monster_sentry':
                t.fire('tur', use='on')     # on and turning: its death ends once the barrel has dropped
                t.wait(2.5)
            t.mark('kill')
            t.damage('tur', 100000)
            t.wait(9.0)
        else:                               # 0: none of the conditions fires it
            see(t)
            t.damage('tur', 1, 2)
            t.wait(0.3)
        t.fire('rm_tur')                    # gone before the player leaves god mode
        t.wait(0.2)
        t.flags('player1', '-FL_GODMODE')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_used('sink', after='quiet', within=(0, 1.5))
        if c == 0:
            t.expect.not_used('sink', after='go')
        elif c == 13:
            t.expect.not_used('sink', after='go', within=(0, 0.5))
            t.expect.used('sink', caller='tur', count=1, after='hit')
        elif c == 3:
            t.expect.not_used('sink', after='go', within=(0, 0.25))
            t.expect.used('sink', caller='tur', count=1, after='half')
        else:
            t.expect.used('sink', caller='tur', count=1, after='go')
    case.__name__ = '%s_%s_%d' % (cls.replace('monster_', ''), key.lower(), c)
    return case


for _cls in TURRETS:
    for _key in KEYS:
        for _c in (0, 1, 2, 3, 10, 11, 13) + ((4,) if _key == 'TriggerAltCondition' else ()):
            case_for(_cls, _key, _c)
