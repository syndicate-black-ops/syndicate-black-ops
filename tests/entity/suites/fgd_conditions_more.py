"""Tier 5, final pass: Monster TriggerCondition / TriggerAltCondition choices no class tested (monsters.cpp
FCheckAITriggerCondition, through the schedule AI): every TriggerAltCondition the stock AI reaches, with no
TriggerCondition (No Trigger), and TriggerCondition 7 Hear World (an env_soundmark of type World), 12 Provoked by a
player (a player-ally Cop hit by a player looking at it), 13 Got healed (a trigger_hurt with negative damage). Each
shows the target is not fired before its condition."""

from etest import Suite

SUITE = Suite('fgd_conditions_more', 'Tier 5: AI trigger conditions, the choices no class tested', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
NAMES = ('TriggerTarget',)


def sink(t, name):
    t.point('info_target', targetname=name + '_fired', at=(200, -200, 0))


def wall(t, y=0):
    t.brush('func_wall', size=(512, 16, 256), at=(0, y, 0), tex='666CEMENT')


def mon(t, cls, name, at, yaw, key, value, named=False, **kv):
    """named: targetname `name` (a named monster stands its first 5 s in SCHED_WAIT_TRIGGER and takes no enemy:
    the sight and hearing cases use unnamed ones)"""
    sink(t, name)
    if named:
        kv['targetname'] = name
    return t.point(cls, at=at, angles='0 %d 0' % yaw, TriggerTarget=name + '_fired', names=NAMES,
                   **dict({key: str(value)}, **kv))


def see_case(key, v, cls):
    @S.test('%s %s %d: it fires its TriggerTarget when the player comes into its view, not before' % (cls, key, v),
            players=1, doc=DOC, covers=('%s.%s=%d' % (cls, key.lower(), v), '%s.triggertarget' % cls))
    def case(t):
        wall(t, y=0)
        mon(t, cls, 'm', (150, 60, 0), 90, key, v)
        t.flags('player1', '+FL_GODMODE')
        t.wait(1.0)
        t.mark('show')
        t.teleport('player1', (150, 220, 0), yaw=270)
        t.wait(2.0)
        t.teleport('player1', (0, -200, 0), yaw=90)
        t.flags('player1', '-FL_GODMODE')
        t.expect.not_fired('m_fired', within=(0, 1.0))
        t.expect.fired('m_fired', count=1, after='show')
    case.__name__ = 'see_%s_%d' % (key.lower(), v)
    return case


see_case('TriggerAltCondition', 1, 'monster_human_grunt')
see_case('TriggerAltCondition', 10, 'monster_scientist')
see_case('TriggerAltCondition', 11, 'monster_scientist')


@S.test('TriggerAltCondition 8 Hear Player / 9 Hear Combat: Feds behind a wall hear the player firing', players=1,
        doc=DOC, covers=('monster_human_grunt.triggeraltcondition=8', 'monster_human_grunt.triggeraltcondition=9'))
def hear_alt(t):
    wall(t, y=0)
    mon(t, 'monster_human_grunt', 'g8', (-120, 150, 0), 90, 'TriggerAltCondition', 8)
    mon(t, 'monster_human_grunt', 'g9', (120, 150, 0), 90, 'TriggerAltCondition', 9)
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
    t.expect.not_fired('g8_fired', within=(0, 0.4))
    t.expect.fired('g8_fired', count=1, after='noise')
    t.expect.fired('g9_fired', count=1, after='noise')


def world_case(key):
    @S.test('%s 7 Hear World: a scientist behind a wall hears an env_soundmark of type World' % key, players=1, doc=DOC,
            covers=('monster_scientist.%s=7' % key.lower(), 'env_soundmark.type=2'))
    def case(t):
        wall(t, y=0)
        mon(t, 'monster_scientist', 's', (0, 150, 0), 270, key, 7, spawnflags='2')
        t.point('env_soundmark', targetname='snd', at=(0, 60, 36), type='2', radius='500', duration='1')
        t.wait(1.5)
        t.mark('noise')
        t.fire('snd')
        t.wait(1.5)
        t.expect.not_fired('s_fired', within=(0, 1.4))
        t.expect.fired('s_fired', count=1, after='noise')
    case.__name__ = 'hear_world_' + key.lower()
    return case


world_case('TriggerCondition')
world_case('TriggerAltCondition')


@S.test('TriggerAltCondition 2 Take Damage: an alien slave fires it when hit', players=1, doc=DOC,
        covers=('monster_alien_slave.triggeraltcondition=2',))
def damage_alt(t):
    mon(t, 'monster_alien_slave', 'hurt', (0, 100, 0), 270, 'TriggerAltCondition', 2, named=True)
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.mark('hit')
    t.damage('hurt', 10, 2, 'player1')
    t.wait(0.5)
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_fired('hurt_fired', within=(0, 0.4))
    t.expect.fired('hurt_fired', count=1, after='hit')


def provoked_case(key):
    @S.test('%s 12 Provoked by a player: a player-ally Cop fires it once a player hit while looking at it makes it '
            'hostile' % key, players=1, doc=DOC, covers=('monster_barney.%s=12' % key.lower(),))
    def case(t):
        mon(t, 'monster_barney', 'b', (0, 150, 0), 270, key, 12, named=True, is_player_ally='1', spawnflags='2')
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (0, 0, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait(1.0)
        t.mark('hit')
        t.damage('b', 5, attacker='player1')
        t.wait(1.5)
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_fired('b_fired', within=(0, 0.9))
        t.expect.fired('b_fired', count=1, after='hit')
    case.__name__ = 'provoked_' + key.lower()
    return case


provoked_case('TriggerCondition')


def healed_case(key):
    @S.test('%s 13 Got healed: a hurt scientist fires it when a trigger_hurt with negative damage heals him' % key,
            players=1, doc=DOC, covers=('monster_scientist.%s=13' % key.lower(),))
    def case(t):
        mon(t, 'monster_scientist', 's', (0, 150, 0), 270, key, 13, named=True, spawnflags='2')
        t.trigger('trigger_hurt', size=(96, 96, 96), at=(0, 150, 0), targetname='heal', dmg='-20', spawnflags='2')
        t.flags('player1', '+FL_NOTARGET')
        t.wait(1.0)
        t.kv('s', 'health', '5')
        t.wait(1.0)
        t.mark('heal')
        t.fire('heal', use='on')
        t.wait(1.0)
        t.fire('heal', use='off')
        t.wait(0.5)
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_fired('s_fired', within=(0, 1.9))
        t.expect.fired('s_fired', count=1, after='heal')
    case.__name__ = 'healed_' + key.lower()
    return case


healed_case('TriggerCondition')


@S.test('TriggerAltCondition 0 No Trigger (with TriggerCondition 0): seeing, hearing and hurting a Fed fire nothing',
        players=1, doc=DOC, covers=('monster_human_grunt.triggeraltcondition=0',))
def alt_none(t):
    mon(t, 'monster_human_grunt', 'g', (0, 150, 0), 270, 'TriggerAltCondition', 0, named=True, TriggerCondition='0')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(2.0)
    t.damage('g', 10, 2, 'player1')
    t.wait(1.0)
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_fired('g_fired')
