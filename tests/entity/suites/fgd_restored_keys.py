"""Tier 5: keys and flags the FGD offers again after a check of the code paths (each was taken out with a wrong
reason). Turrets, the Osprey and the Pitbull call CBaseMonster::Look from their own think, which reads WaitTillSeen
(1) and Prisoner (16); monster_furniture runs the full monster AI; the ichthyosaur walks (swims) a path_corner route
from StartMonster; a tripmine and a sitting scientist carry a Classification that a trigger_sensor's relationship
filter reads (CBaseMonster::KeyedClass); a tripmine's TakeDamage is CBaseMonster's (Unkillable); the Pitbull checks its
trigger conditions when it dies."""

from etest import Suite

SUITE = Suite('fgd_restored_keys', 'Tier 5: keys and flags offered again after the code-path check', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
TURRETS = ('monster_turret', 'monster_miniturret', 'monster_sentry')

# CBaseMonster::IDefaultRelationship rows / columns for class 4 (human military, the sensors' class), from
# suite fgd_guide2: an entity of class c is an enemy of the sensor when REL4[c] >= 1 or REL_TO4[c] >= 1
REL4 = [0, -2, 2, 0, -2, 2, 1, 1, 1, 1, 2, 2, 0, 0]
REL_TO4 = [0, -2, 1, 0, -2, 2, 0, 1, 1, 2, -1, 1, 1, 1]
SENSOR_CLASS = 4


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def enemy_of_sensor(c):
    return REL4[c] >= 1 or REL_TO4[c] >= 1


def sensor(t, name, at, cls, hit):
    t.point('trigger_sensor', targetname=name, at=at, target=hit, radius='30', sensed_classname=cls,
            line_blocked='0', trace_to='0', fire_trigger_policy='0', alive_filter='0', takedamage_filter='0',
            classify=str(SENSOR_CLASS), relationship_filter='2')
    info(t, hit, at=(at[0], at[1] + 60, at[2]))


# ---------------------------------------------------------------------------------------------- classification
def classify_case(cls, values, own, z):
    @S.test('%s Classification: a trigger_sensor acting as human military with an Enemy relationship filter senses '
            'it exactly when the class table makes that class its enemy (%s)' % (cls, ', '.join(map(str, values))),
            doc=DOC, covers=tuple('%s.classify=%s' % (cls, v) for v in values) + ('%s.classify' % cls,))
    def case(t):
        for i, v in enumerate(values):
            x, y = -220 + (i % 6) * 88, -150 + (i // 6) * 150
            t.point(cls, targetname='c%d' % i, at=(x, y, z), angles='0 90 0', classify=str(v))
            if cls == 'monster_tripmine':       # a mine needs a wall behind it
                t.brush('func_wall', size=(32, 16, 64), at=(x, y - 16, 32), names=())
            sensor(t, 's%d' % i, (x, y, z), cls, 'h%d' % i)
        t.wait(0.3)
        for i in range(len(values)):
            t.fire('s%d' % i)
        t.wait(0.3)
        for i, v in enumerate(values):
            eff = 0 if v == -1 else (own if v == 0 else v)
            t.expect.fired('h%d' % i, count=1 if enemy_of_sensor(eff) else 0)
    case.__name__ = '%s_classify' % cls.replace('monster_', '')
    return case


classify_case('monster_sitting_scientist', [-1, 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13], 4, 16)
classify_case('monster_tripmine', [0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13], 0, 32)


@S.test('monster_sitting_scientist Is player ally: a trigger_sensor acting as human military with an Enemy filter '
        'senses it as a player ally with Yes, not with No (Human military)', doc=DOC,
        covers=('monster_sitting_scientist.is_player_ally', 'monster_sitting_scientist.is_player_ally=0',
                'monster_sitting_scientist.is_player_ally=1'))
def sitting_ally(t):
    for i, v in enumerate((0, 1)):
        x = -100 + 200 * i
        t.point('monster_sitting_scientist', targetname='a%d' % i, at=(x, 0, 16), angles='0 90 0',
                is_player_ally=str(v))
        sensor(t, 's%d' % i, (x, 0, 16), 'monster_sitting_scientist', 'h%d' % i)
    t.wait(0.3)
    t.fire('s0')
    t.fire('s1')
    t.wait(0.3)
    t.expect.fired('h0', count=0)
    t.expect.fired('h1', count=1)


@S.test('monster_tripmine Unkillable Yes: a hit of 500 does not set it off; No: the hit sets it off', players=1,
        doc=DOC,
        covers=('monster_tripmine.unkillable', 'monster_tripmine.unkillable=0', 'monster_tripmine.unkillable=1'))
def tripmine_unkillable(t):
    t.flags('player1', '+FL_GODMODE')
    t.brush('func_wall', size=(16, 64, 64), at=(-150, 100, 32), names=())
    t.brush('func_wall', size=(16, 64, 64), at=(150, 100, 32), names=())
    t.point('monster_tripmine', targetname='tu', at=(-140, 100, 32), angles='0 0 0', unkillable='1', spawnflags='1')
    t.point('monster_tripmine', targetname='tk', at=(160, 100, 32), angles='0 0 0', unkillable='0', spawnflags='1')
    t.wait(2.0)
    t.mark('hit')
    t.damage('tu', 500)
    t.damage('tk', 500)
    t.wait(1.5)
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_removed('tu', after='hit')
    t.expect.removed('tk', after='hit')


# ---------------------------------------------------------------------------------------------- turrets 1 / 16
def turret_room(t, cls, name, x, flags):
    t.point(cls, targetname='pre_' + name, at=(x, -200, 0), angles='0 90 0', spawnflags='16')
    t.point('trigger_relay', targetname='rm_pre_' + name, killtarget='pre_' + name)
    t.fire('rm_pre_' + name)
    p = t._abs((x, 150, 0))
    t.console('test_create %s %g %g %g 270 targetname %s spawnflags %d TriggerTarget %s TriggerCondition 10' % (
        cls, p[0], p[1], p[2], t.name(name), 32 | flags, t.name('saw_' + name)))
    info(t, 'saw_' + name)


def turret_flag_case(cls):
    @S.test('%s Prisoner (16): it sees nobody, so a visible player is never its enemy (See Player Unconditional does '
            'not fire); WaitTillSeen (1): it takes a player who looks away for nothing, and takes him once he faces '
            'it; one with no flag takes him at once' % cls, players=1, doc=DOC, covers=('%s:16' % cls, '%s:1' % cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -150, 0), yaw=270)     # facing away from the turrets
        turret_room(t, cls, 'tp', -150, 16)
        turret_room(t, cls, 'tw', 0, 1)
        turret_room(t, cls, 'tn', 150, 0)
        t.wait(0.5)
        t.mark('show')
        t.flags('player1', '-FL_NOTARGET')
        t.wait(3.0)
        t.mark('face')
        t.look('player1', 0, 90)
        t.wait(3.0)
        t.flags('player1', '+FL_NOTARGET')
        for n in ('tp', 'tw', 'tn'):
            t.point('trigger_relay', targetname='rm_' + n, killtarget=n)
            t.fire('rm_' + n)
        t.wait(0.2)
        t.flags('player1', '-FL_GODMODE')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_used('saw_tp')
        t.expect.used('saw_tn', count=1, after='show', within=(0, 2.0))
        t.expect.not_used('saw_tw', after='show', within=(0, 3.0))
        t.expect.used('saw_tw', count=1, after='face')
    case.__name__ = '%s_flags' % cls.replace('monster_', '')
    return case


for _c in TURRETS:
    turret_flag_case(_c)


# ---------------------------------------------------------------------------------------------- Pitbull
def snark_room(t, specs):
    t.flags('player1', '+FL_GODMODE')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=270)
    # made now: a Pitbull placed in the map explodes 30 s after the map starts
    for n, x, fl in specs:
        p = t._abs((x, 150, 40))     # dropped: a Pitbull starts to hunt at its first bounce, then every 2 s
        # Custom Field Of View Full: it hops about at random, so with its own view cone it might face away
        t.console('test_create monster_snark %g %g %g 270 targetname %s spawnflags %d field_of_view -1' % (
            p[0], p[1], p[2], t.name(n), fl))
    t.wait(1.0)
    t.flags('player1', '-FL_NOTARGET')
    t.wait(4.5)                             # two hunts


def snark_end(t):
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.2)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')


@S.test('monster_snark Prisoner (16): it takes no enemy; one without the flag takes the player', players=1, doc=DOC,
        covers=('monster_snark:16',))
def snark_prisoner(t):
    snark_room(t, (('sp', -100, 16), ('sn', 100, 0)))
    t.snap('s', 'sp', 'enemy')
    t.snap('s', 'sn', 'enemy')
    snark_end(t)
    t.expect.value('s', 'sp', 'enemy', '')
    t.expect.value('s', 'sn', 'enemy', 'player')


@S.test('monster_snark WaitTillSeen (1): it takes the player only once he faces it', players=1, doc=DOC,
        covers=('monster_snark:1',))
def snark_wait_till_seen(t):
    snark_room(t, (('sw', 0, 1),))           # alone: another Pitbull's bites would kick the player's view
    t.snap('away', 'sw', 'enemy')
    t.look('player1', 0, 90)
    t.wait(4.5)
    t.snap('faced', 'sw', 'enemy')
    snark_end(t)
    t.expect.value('away', 'sw', 'enemy', '')
    t.expect.value('faced', 'sw', 'enemy', 'player')


@S.test('monster_snark trigger conditions are checked when it dies: Take Damage (2) and Death (4) fire then, as the '
        'alternative condition too; No Trigger (0) fires nothing', players=1, doc=DOC,
        covers=('monster_snark.triggercondition=2', 'monster_snark.triggeraltcondition',
                'monster_snark.triggeraltcondition=0', 'monster_snark.triggeraltcondition=2',
                'monster_snark.triggeraltcondition=4'))
def snark_conditions(t):
    t.flags('player1', '+FL_GODMODE')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=90)
    # made now: a Pitbull placed in the map explodes 30 s after the map starts
    cases = [('c2', 'TriggerCondition', 2), ('a2', 'TriggerAltCondition', 2), ('a4', 'TriggerAltCondition', 4),
             ('a0', 'TriggerAltCondition', 0)]
    for i, (n, key, c) in enumerate(cases):
        p = t._abs((-150 + i * 100, 150, 0))
        t.console('test_create monster_snark %g %g %g 270 targetname %s TriggerTarget %s %s %d' % (
            p[0], p[1], p[2], t.name(n), t.name('hit_' + n), key, c))
        info(t, 'hit_' + n, at=(-150 + i * 100, 220, 0))
    t.wait(0.5)
    t.mark('kill')
    for n, _k, _c in cases:
        t.damage(n, 1000, 0, 'player1')
    t.wait(0.5)
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')
    for n, _k, c in cases:
        t.expect.used('hit_' + n, count=0 if c == 0 else 1, after='kill')


# ---------------------------------------------------------------------------------------------- Osprey
OSPREY_ROOM = (512, 512, 1024)
OZ = 600


def osprey(t, name, x, flags):
    p = t._abs((x, -150, OZ))
    t.console('test_create monster_osprey %g %g %g 90 targetname %s TriggerTarget %s TriggerCondition 10 '
              'target %s spawnflags %d' % (p[0], p[1], p[2], t.name(name), t.name('saw_' + name),
                                           t.name('op_' + name), 64 | flags))
    t.point('path_corner', targetname='op_' + name, at=(x, 150, OZ), target='op2_' + name, speed='300')
    t.point('path_corner', targetname='op2_' + name, at=(x, 0, OZ), target='op_' + name, speed='0')
    info(t, 'saw_' + name)


@S.test('monster_osprey Prisoner (16): it sees nobody, so See Player Unconditional never fires; WaitTillSeen (1): it '
        'fires only once the player faces it; one with no flag fires when he is in sight', size=OSPREY_ROOM,
        players=1, doc=DOC, covers=('monster_osprey:16', 'monster_osprey:1'))
def osprey_flags(t):
    for k in range(4):
        t.point('monster_human_grunt', targetname='og%d' % k, at=(-200 + 40 * k, -200, 0), angles='0 90 0',
                spawnflags='16')
    t.point('monster_osprey', targetname='pre', at=(0, 0, 900), angles='0 90 0', spawnflags='64')
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -220, 0), yaw=270)     # below them, facing away
    t.fire('rm_pre')
    variants = (('op', 16), ('ow', 1), ('on', 0))
    # one at a time: each Osprey in its own run of the room (a second one would block the first's path)
    for n, fl in variants:
        osprey(t, n, 0, fl)
        t.wait(0.2)
        t.fire(n, use='on')
        t.wait(1.0)
        t.mark('show_' + n)
        t.flags('player1', '-FL_NOTARGET')
        t.look('player1', 0, 270)
        t.wait(3.0)
        t.mark('face_' + n)
        t.look('player1', 0, 90)
        t.wait(3.0)
        t.flags('player1', '+FL_NOTARGET')
        t.point('trigger_relay', targetname='rm_' + n, killtarget=n)
        t.fire('rm_' + n)
        t.wait(0.5)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.not_used('saw_op')
    t.expect.used('saw_on', count=1, after='show_on', within=(0, 3.0))
    t.expect.not_used('saw_ow', after='show_ow', within=(0, 3.0))
    t.expect.used('saw_ow', count=1, after='face_ow', within=(0, 3.0))


# ---------------------------------------------------------------------------------------------- furniture
FURN = 'models/barney.mdl'      # a model with walk and death animations, so the furniture can walk and die


@S.test('monster_furniture with a Classification that hates players (Alien military): Prisoner (16) sees nobody '
        '(See Player Unconditional never fires); WaitTillSeen (1) fires only once the player faces it; with no flag '
        'it fires when he is in sight', players=1, doc=DOC, covers=('monster_furniture:16', 'monster_furniture:1'))
def furniture_sight(t):
    t.flags('player1', '+FL_GODMODE')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=270)
    # made now: the harness brings the player into the room before the test's first step, while he can still be
    # seen, so a furniture placed in the map would see him then
    t.point('monster_furniture', targetname='fpre', at=(220, -220, 0), model=FURN)   # the model in the map
    for n, x, fl in (('fp', -150, 16), ('fw', 0, 1), ('fn', 150, 0)):
        p = t._abs((x, 150, 0))
        t.console('test_create monster_furniture %g %g %g 270 targetname %s model %s classify 5 TriggerCondition 10 '
                  'TriggerTarget %s spawnflags %d' % (p[0], p[1], p[2], t.name(n), FURN, t.name('saw_' + n), fl))
        info(t, 'saw_' + n, at=(x, 220, 0))
    t.wait(1.0)
    t.mark('show')
    t.flags('player1', '-FL_NOTARGET')
    t.wait(3.0)
    t.mark('face')
    t.look('player1', 0, 90)
    t.wait(3.0)
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.2)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.not_used('saw_fp')
    t.expect.used('saw_fn', count=1, after='show', within=(0, 3.0))
    t.expect.not_used('saw_fw', after='show', within=(0, 3.0))
    t.expect.used('saw_fw', count=1, after='face')


@S.test('monster_furniture Non-solid corpse (65536): killed without gibbing, its corpse is not solid; without the '
        'flag it stays solid', players=1, doc=DOC, covers=('monster_furniture:65536',))
def furniture_corpse(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('dn', -120, 65536), ('ds', 120, 0)):
        t.point('monster_furniture', targetname=n, at=(x, 100, 0), angles='0 270 0', model=FURN, spawnflags=str(fl))
    t.wait(0.5)
    t.damage('dn', 80010, 2)                 # 80000 health: ends at -10, no gibs
    t.damage('ds', 80010, 2)
    t.wait(5.0)
    t.snap('dead', 'dn', 'deadflag', 'solid')
    t.snap('dead', 'ds', 'deadflag', 'solid')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('dead', 'dn', 'deadflag', 0, op='>')
    t.expect.value('dead', 'dn', 'solid', 0)
    t.expect.value('dead', 'ds', 'solid', 0, op='>')


# ---------------------------------------------------------------------------------------------- ichthyosaur
@S.test('monster_ichthyosaur Target: it swims to the first path_corner (a monster trigger at the corner, 380 away, fires within 8 s)', players=1,
        doc=DOC, covers=('monster_ichthyosaur.target',))
def ichthyosaur_path(t):
    t.flags('player1', '+FL_NOTARGET')
    t.brush('func_water', size=(480, 480, 240), at=(0, 0, 120), skin='-3', names=())
    t.point('path_corner', targetname='ip', at=(200, -100, 120))
    # Monsters (1): the ichthyosaur fires it when it gets to the corner
    t.trigger('trigger_multiple', size=(96, 96, 96), at=(200, -100, 72), target='reached', spawnflags='1', wait='0.2')
    info(t, 'reached')
    # made now (a placed one swims its path from the map start, long before the test)
    p = t._abs((-180, -100, 120))
    t.mark('made')
    t.console('test_create monster_ichthyosaur %g %g %g 0 targetname %s target %s' % (
        p[0], p[1], p[2], t.name('ip_i'), t.name('ip')))
    t.wait(8.0)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.used('reached', activator='ip_i', min=1, after='made', within=(1.0, 8.0))
