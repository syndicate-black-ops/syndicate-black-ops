"""Monster TriggerCondition through the real AI (GAPS.md tier 1 "Monster TriggerTarget / TriggerCondition per value",
the values the stub runner can't reach: sight and hearing need the engine's PVS, traces and the sound list). Latest
1.38 maps: 1 See Player, Mad at Player (193 entities / 7 maps), 4 Death (283 / 34), 10 See Player Unconditional
(19 / 2), 8 Hear Player (19 / 2), 3 50% Health (4 / 1), 11 See Player, Not In Combat (3 / 1), 9 Hear Combat (2 / 1),
2 Take Damage (1 / 1). TriggerTarget fires once, with the monster as activator and caller (monsters.cpp
FCheckAITriggerCondition); TriggerAltCondition is a second condition for the same target. What each checks is the
FGD's description (sdk/syndicate.fgd, base Monster)."""

from etest import Suite

SUITE = Suite('monster_conditions', 'Monster TriggerCondition / TriggerAltCondition through the real AI')
S = SUITE

NAMES = ('TriggerTarget',)


def monster(t, cls, name, at, yaw, cond, alt=None, **kv):
    if alt is not None:
        kv['TriggerAltCondition'] = str(alt)
    return t.point(cls, targetname=name, at=at, angles='0 %d 0' % yaw, TriggerTarget=name + '_fired',
                   TriggerCondition=str(cond), names=NAMES, **kv)


def sink(t, name):
    t.point('info_target', targetname=name + '_fired', at=(200, -200, 0))


def wall(t, y=0):
    """A wall across the room at y (blocks sight between the south and the north half)."""
    t.brush('func_wall', size=(512, 16, 256), at=(0, y, 0), tex='666CEMENT')


@S.test('TriggerCondition 1 See Player, Mad at Player: a grunt fires its TriggerTarget once (monster as activator and '
        'caller) when the player comes into its view and it takes him as its enemy, not before (a wall hides the room\'s '
        'south part)', players=1,
        covers=('monster_human_grunt.triggercondition=1', 'monster_human_grunt.triggertarget'))
def see_mad(t):
    # unnamed: a named monster waits in SCHED_WAIT_TRIGGER until it is triggered or hurt, and takes no enemy there
    # (HL SDK); condition 1 needs an enemy
    wall(t, y=0)
    t.point('monster_human_grunt', at=(150, 60, 0), angles='0 90 0', TriggerTarget='g_fired', TriggerCondition='1',
            names=NAMES)
    sink(t, 'g')
    t.flags('player1', '+FL_GODMODE')
    t.wait(1.0)
    t.mark('show')
    t.teleport('player1', (150, 220, 0), yaw=270)
    t.wait(2.0)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_fired('g_fired', within=(0, 1.0))
    t.expect.fired('g_fired', activator='@monster_human_grunt', caller='@monster_human_grunt', count=1, after='show')


@S.test('TriggerCondition 10 See Player Unconditional and 11 See Player, Not In Combat: scientists (not angry, not '
        'fighting) fire them when the player comes into view, not before', players=1,
        covers=('monster_scientist.triggercondition=10', 'monster_scientist.triggercondition=11'))
def see_unconditional(t):
    wall(t, y=0)
    monster(t, 'monster_scientist', 'sci10', (-100, 60, 0), 90, 10)
    monster(t, 'monster_scientist', 'sci11', (100, 60, 0), 90, 11)
    sink(t, 'sci10')
    sink(t, 'sci11')
    t.wait(1.0)
    t.mark('show')
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.wait(2.0)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.expect.not_fired('sci10_fired', within=(0, 1.0))
    t.expect.not_fired('sci11_fired', within=(0, 1.0))
    t.expect.fired('sci10_fired', activator='sci10', count=1, after='show')
    t.expect.fired('sci11_fired', activator='sci11', count=1, after='show')


@S.test('TriggerCondition 8 Hear Player: a grunt behind a wall hears the player firing (it can\'t see him); 9 Hear '
        'Combat: another hears the shots too', players=1,
        covers=('monster_human_grunt.triggercondition=8', 'monster_human_grunt.triggercondition=9'))
def hear(t):
    wall(t, y=0)
    monster(t, 'monster_human_grunt', 'g8', (-120, 150, 0), 90, 8)
    monster(t, 'monster_human_grunt', 'g9', (120, 150, 0), 90, 9)
    sink(t, 'g8')
    sink(t, 'g9')
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.mark('noise')
    t.teleport('player1', (0, -150, 0), yaw=270)
    t.look('player1', 0, 270)
    t.wait_frames(3)
    for _ in range(3):
        t.move('player1', 0, 0, 3, '+attack')
        t.wait(0.4)
    t.wait(1.5)
    t.flags('player1', '-FL_GODMODE')
    t.expect.fired('g8_fired', activator='g8', count=1, after='noise')
    t.expect.fired('g9_fired', activator='g9', count=1, after='noise')


@S.test('TriggerCondition 2 Take Damage, 3 50% Health Remaining, 4 Death: hurt, below half its health, killed; each '
        'fires once', players=1,
        covers=('monster_alien_slave.triggercondition=2', 'monster_alien_slave.triggercondition=3',
                'monster_alien_slave.triggercondition=4'))
def damage_health_death(t):
    # alien slaves, not scientists: a talk monster hit by a player turns its nearest friend to "stop shooting", and
    # that schedule change clears the friend's fresh damage conditions (HL SDK CTalkMonster::TakeDamage), so three
    # scientists hit in one frame lose the first hit's Take Damage
    monster(t, 'monster_alien_slave', 'hurt', (-150, 100, 0), 270, 2)
    monster(t, 'monster_alien_slave', 'half', (0, 100, 0), 270, 3)
    monster(t, 'monster_alien_slave', 'dies', (150, 100, 0), 270, 4)
    for n in ('hurt', 'half', 'dies'):
        sink(t, n)
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.snap('spawned', 'half', 'health')
    t.mark('hit')
    for n in ('hurt', 'half', 'dies'):
        t.damage(n, 10, 2, 'player1')
    t.wait(0.5)
    t.mark('below')
    t.damage('half', 300, 2, 'player1')         # 500 - 10 - 300 = 190: below half
    t.damage('hurt', 10, 2, 'player1')
    t.wait(0.5)
    t.mark('kill')
    t.damage('dies', 5000, 2, 'player1')
    t.wait(0.5)
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('spawned', 'half', 'health', 500)       # an alien slave's health (skill); "health" is not a key of it
    t.expect.fired('hurt_fired', activator='hurt', count=1, after='hit', within=(0, 0.5))
    t.expect.not_fired('half_fired', after='hit', within=(0, 0.5))
    t.expect.fired('half_fired', activator='half', count=1, after='below')
    t.expect.not_fired('dies_fired', after='hit', within=(0, 0.9))
    t.expect.fired('dies_fired', activator='dies', count=1, after='kill')


@S.test('TriggerAltCondition: either condition fires the TriggerTarget, once (here the alternative, Death, comes first)',
        players=1, covers=('monster_scientist.triggeraltcondition=4',))
def alt_condition(t):
    monster(t, 'monster_scientist', 'sci', (0, 150, 0), 270, 8, alt=4, health='50')     # Hear Player / Death
    sink(t, 'sci')
    t.wait(0.5)
    t.mark('kill')
    t.damage('sci', 500, 2, 'player1')
    t.wait(1.0)
    t.expect.fired('sci_fired', activator='sci', count=1, after='kill', within=(0, 0.3))


def turret_death_case(cls):
    @S.test('TriggerCondition 4 Death on %s: destroyed, it fires its TriggerTarget once when its death ends '
            '(Half-Life Unified SDK; turrets die in TakeDamage, not Killed)' % cls,
            covers=('%s.TriggerCondition=4' % cls, '%s.TriggerTarget' % cls))
    def case(t):
        monster(t, cls, 'tur', (0, 100, 0), 270, 4)
        sink(t, 'tur')
        t.wait(0.5)
        if cls != 'monster_sentry':
            # on and turning: a turret's death ends once its barrel has dropped, which takes a turn rate (a turret
            # that never turned keeps its death think going, as in HL)
            t.fire('tur', use='on')
            t.wait(2.5)
        t.mark('kill')
        t.damage('tur', 100000)
        t.wait(9.0)                     # the death ends 5 s after the hit, once the death animation is done
        t.expect.used('tur_fired', caller='tur', count=1, after='kill')
    case.__name__ = 'death_' + cls.replace('monster_', '')
    return case


for _c in ('monster_turret', 'monster_miniturret', 'monster_sentry'):
    turret_death_case(_c)
