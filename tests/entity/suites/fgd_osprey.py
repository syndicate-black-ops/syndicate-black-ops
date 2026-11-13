"""Tier 5, final pass: Monster keys on the Osprey (osprey.cpp). It flies its own think, not the schedule AI; while
it flies, COsprey::Update looks, listens and checks the AI trigger conditions every think (halflife-updated VS#3261),
and its TakeDamage makes the attacker its enemy. Each test makes an Osprey (test_create, once the player is notarget)
that flies a path at the top of a tall room; four Prisoner Feds in a corner are the soldiers it needs (FindAllThink
removes an Osprey with none). The TriggerTarget is not fired before its condition."""


from etest import Suite

SUITE = Suite('fgd_osprey', 'Tier 5: Osprey keys: target, Start Inactive, AI trigger conditions, sound mask, view',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
O = 'monster_osprey'
ROOM = (512, 512, 1024)
Z = 600
NAMES = {0: 'No Trigger', 1: 'See Player, Mad at Player', 2: 'Take Damage', 3: '50% Health Remaining', 4: 'Death',
         7: 'Hear World', 8: 'Hear Player', 9: 'Hear Combat', 10: 'See Player Unconditional',
         11: 'See Player, Not In Combat', 13: 'Got healed'}
SOUND_TYPE = {7: 2, 8: 4, 9: 1}


def setup(t, keys, start=True, path=True):
    t.point('info_target', targetname='sink')
    for k in range(4):
        t.point('monster_human_grunt', targetname='og%d' % k, at=(-200 + 40 * k, -200, 0), angles='0 90 0',
                spawnflags='16')
    t.point(O, targetname='pre', at=(0, 0, 900), angles='0 90 0', spawnflags='64')     # the class in the map
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.point('trigger_relay', targetname='rm_o', killtarget='o')
    if path:
        # the closed path of suite hlu_fixes_2: it flies a loop between op1 and op2 inside the room
        t.point('path_corner', targetname='op1', at=(0, 150, Z), target='op2', speed='300')
        t.point('path_corner', targetname='op2', at=(0, 0, Z), target='op3', speed='0')
        t.point('path_corner', targetname='op3', at=(0, 150, Z), target='op1', speed='300')
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.fire('rm_pre')
    p = t._abs((0, -150, Z))
    words = ['test_create', O, '%g' % p[0], '%g' % p[1], '%g' % p[2], '90', 'targetname', t.name('o'),
             'TriggerTarget', t.name('sink'), 'spawnflags', '64']
    if path:
        words += ['target', t.name('op1')]
    for k, v in keys.items():
        words += [k, str(v)]
    t.console(' '.join(words))
    t.wait(0.2)
    if start:
        t.fire('o', use='on')


def finish(t):
    t.fire('rm_o')
    t.wait(0.2)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')


@S.test('%s "target" (first path_corner) and Start Inactive (64): it hangs still until triggered, then flies to the '
        'path_corner' % O, size=ROOM, players=1, doc=DOC, covers=(O + '.target', O + ':64'))
def target_inactive(t):
    setup(t, {}, start=False)
    t.wait(2.0)
    t.snap('still', 'o', 'origin')
    t.snap('still', 'op1', 'origin')
    t.fire('o', use='on')
    t.wait(5.0)
    t.snap('there', 'o', 'origin')
    t.snap('there', 'op1', 'origin')
    finish(t)
    t.expect.distance('still', 'o', 'op1', 290, 310)
    t.expect.distance('there', 'o', 'op1', 0, 200)     # on its loop between op1 and op2, not off elsewhere


def case_for(key, c):
    @S.test('%s %s %d (%s): its TriggerTarget fires once the condition happens, not before' % (O, key, c, NAMES[c]),
            size=ROOM, players=1, doc=DOC, covers=('%s.%s=%d' % (O, key.lower(), c), '%s.triggertarget' % O))
    def case(t):
        keys = {key: c}
        if c == 11:
            keys['is_player_ally'] = 1
        setup(t, keys)
        t.wait(0.5)
        t.mark('quiet')
        t.wait(1.5)
        t.mark('go')
        if c in (10, 11):
            t.flags('player1', '-FL_NOTARGET')
            t.teleport('player1', (0, -220, 0), yaw=90)     # below, ahead of it (it faces south at op2)
            t.wait(3.0)
            t.flags('player1', '+FL_NOTARGET')
        elif c == 1:
            t.flags('player1', '-FL_NOTARGET')
            t.teleport('player1', (0, -220, 0), yaw=90)
            t.damage('o', 1, 2, 'player1')                  # its attacker becomes its enemy
            t.wait(3.0)
            t.flags('player1', '+FL_NOTARGET')
        elif c in SOUND_TYPE:
            t.point('env_soundmark', targetname='snd', at=(0, 100, Z - 50), type=str(SOUND_TYPE[c]), radius='800',
                    duration='1')
            t.fire('snd')
            t.wait(1.5)
        elif c == 2:
            t.damage('o', 1, 2, 'player1')     # the damage conditions come from a monster's or a player's hit
            t.wait(0.5)
        elif c == 3:
            t.damage('o', 1, 2)
            t.wait(0.5)
            t.mark('half')
            t.kv('o', 'health', '5')            # under half its max_health (400)
            t.damage('o', 1, 2)
            t.wait(0.5)
        elif c == 13:
            t.kv('o', 'health', '50')
            # Start Off (2), Affect non-moving NPCs (64): it hovers
            t.trigger('trigger_hurt', size=(512, 512, 300), at=(0, 0, Z - 150), targetname='heal', dmg='-20',
                      spawnflags='66')
            t.mark('hit')
            t.fire('heal', use='on')
            t.wait(1.5)
            t.fire('heal', use='off')
        elif c == 4:
            t.damage('o', 100000, 2)
            t.wait(1.5)
        else:
            t.flags('player1', '-FL_NOTARGET')
            t.teleport('player1', (0, -220, 0), yaw=90)
            t.wait(2.0)
            t.flags('player1', '+FL_NOTARGET')
            t.damage('o', 1, 2)
            t.wait(0.5)
        finish(t)
        t.expect.not_used('sink', after='quiet', within=(0, 1.5))
        if c == 0:
            t.expect.not_used('sink', after='go')
        elif c == 3:
            t.expect.not_used('sink', after='go', within=(0, 0.4))
            t.expect.used('sink', count=1, after='half')
        elif c == 13:
            t.expect.used('sink', count=1, after='hit')
        else:
            t.expect.used('sink', count=1, after='go')
    case.__name__ = 'osprey_%s_%d' % (key.lower(), c)
    return case


for _key in ('TriggerCondition', 'TriggerAltCondition'):
    for _c in (0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 13):
        case_for(_key, _c)

MASKS = {-1: False, 0: True, 32: False, 33: False, 37: False, 127: True, 1028: True}


def mask_case(m, heard):
    @S.test('%s "soundmask" %d: it %s an env_soundmark of type World (TriggerCondition 7 Hear World)' % (
                O, m, 'hears' if heard else 'does not hear'), size=ROOM, players=1, doc=DOC,
            covers=('%s.soundmask' % O, '%s.soundmask=%d' % (O, m)))
    def case(t):
        setup(t, {'TriggerCondition': 7, 'soundmask': m})
        t.wait(1.5)
        t.mark('go')
        t.point('env_soundmark', targetname='snd', at=(0, 100, Z - 50), type='2', radius='800', duration='1')
        t.fire('snd')
        t.wait(1.5)
        finish(t)
        if heard:
            t.expect.used('sink', count=1, after='go')
        else:
            t.expect.not_used('sink')
    case.__name__ = 'osprey_soundmask_%d' % m if m >= 0 else 'osprey_soundmask_deaf'
    return case


for _m, _h in MASKS.items():
    mask_case(_m, _h)

FOV = (0, -1, -0.7, -0.2, 0.2, 0.5, 0.7, 0.9)


def fov_case(f):
    # it flies its loop between op1 and op2 (x within about 140 of the middle, y 0 to 150) facing east (yaw 0); the
    # player on the floor west of the loop (250, 75) is always behind it, east of it (250, 75) always within about 35
    # degrees of its nose. TriggerCondition 10 See Player Unconditional. -1 (all round): seen from behind; any other
    # view (0: its own, 180 degrees): not from behind, and from ahead for every view wider than 35 degrees
    ahead = f <= 0.7

    @S.test('%s "field_of_view" %g: the player behind it is %s%s' % (
                O, f, 'seen' if f == -1 else 'not seen', ', ahead of it he is' if f != -1 and ahead else ''),
            size=ROOM, players=1, doc=DOC, covers=('%s.field_of_view' % O, '%s.field_of_view=%g' % (O, f)))
    def case(t):
        setup(t, {'TriggerCondition': 10, 'field_of_view': f})
        t.wait(3.0)                             # on its loop, facing east
        t.flags('player1', '-FL_NOTARGET')
        t.mark('behind')
        t.teleport('player1', (-250, 75, 0))
        t.wait(2.0)
        t.mark('ahead')
        if ahead:
            t.teleport('player1', (250, 75, 0))
            t.wait(2.0)
        finish(t)
        if f == -1:
            t.expect.used('sink', count=1, after='behind', within=(0, 2.0))
        else:
            t.expect.not_used('sink', after='behind', within=(0, 1.9))
            if ahead:
                t.expect.used('sink', count=1, after='ahead')
    case.__name__ = 'osprey_fov_%d' % FOV.index(f)
    return case


for _f in FOV:
    fov_case(_f)
