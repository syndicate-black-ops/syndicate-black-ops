"""Tier 5, final pass: the AI trigger conditions and the sound mask on the Apache (apache.cpp). It flies its own
think, not the schedule AI, but HuntThink looks, listens and checks the conditions every think (halflife-updated
VS#3262), and its TakeDamage sets the damage conditions. Generated per key and choice: each test first shows the
TriggerTarget is not fired before the condition. The Apache is made once the player is notarget (one placed in the
map could see him as he enters the room); a placed copy (a Prisoner that waits for a trigger) precaches the class."""

from etest import Suite

SUITE = Suite('fgd_apache_ai', 'Tier 5: Apache AI trigger conditions and sound mask', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
A = 'monster_apache'
KEYS = ('TriggerCondition', 'TriggerAltCondition')
NAMES = {0: 'No Trigger', 1: 'See Player, Mad at Player', 2: 'Take Damage', 3: '50% Health Remaining', 4: 'Death',
         7: 'Hear World', 8: 'Hear Player', 9: 'Hear Combat', 10: 'See Player Unconditional',
         11: 'See Player, Not In Combat', 13: 'Got healed'}
SOUND_TYPE = {7: 2, 8: 4, 9: 1}         # env_soundmark type: World, Player, Combat


def apache(t, keys):
    t.point('info_target', targetname='sink')
    t.point(A, targetname='pre', at=(200, -200, 120), angles='0 90 0', spawnflags='80')    # Prisoner + Wait (64)
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.point('trigger_relay', targetname='rm_a', killtarget='a')
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.fire('rm_pre')
    p = t._abs((0, 100, 200))
    words = ['test_create', A, '%g' % p[0], '%g' % p[1], '%g' % p[2], '270', 'targetname', t.name('a'),
             'TriggerTarget', t.name('sink')]
    for k, v in keys.items():
        words += [k, str(v)]
    t.console(' '.join(words))


def finish(t):
    t.fire('rm_a')
    t.wait(0.2)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')


def case_for(key, c):
    @S.test('%s %s %d (%s): its TriggerTarget fires once the condition happens, not before' % (A, key, c, NAMES[c]),
            players=1, doc=DOC, covers=('%s.%s=%d' % (A, key.lower(), c), '%s.triggertarget' % A))
    def case(t):
        keys = {key: c}
        if c == 11:
            keys['is_player_ally'] = 1          # it sees him and does not fight him: not in combat
        apache(t, keys)
        t.wait(0.5)
        t.mark('quiet')
        t.wait(1.5)
        t.mark('go')
        if c in (1, 10, 11):
            t.flags('player1', '-FL_NOTARGET')
            t.wait(3.0)
            t.flags('player1', '+FL_NOTARGET')
        elif c in SOUND_TYPE:
            t.point('env_soundmark', targetname='snd', at=(0, 100, 150), type=str(SOUND_TYPE[c]), radius='600',
                    duration='1')
            t.fire('snd')
            t.wait(1.5)
        elif c == 2:
            t.damage('a', 1, 2)
            t.wait(0.5)
        elif c == 3:
            t.damage('a', 1, 2)                 # a scratch: well over half its health
            t.wait(0.5)
            t.mark('half')
            t.kv('a', 'health', '5')            # under half its max_health (250)
            t.damage('a', 1, 2)
            t.wait(0.5)
        elif c == 13:
            t.kv('a', 'health', '50')
            t.trigger('trigger_hurt', size=(256, 256, 160), at=(0, 100, 80), targetname='heal', dmg='-20',
                      spawnflags='2')           # Start Off; the Apache flies through it
            t.mark('hit')
            t.fire('heal', use='on')
            t.wait(1.5)
            t.fire('heal', use='off')
        elif c == 4:
            t.damage('a', 100000, 2)
            t.wait(1.0)
        else:                                   # 0: nothing fires it
            t.flags('player1', '-FL_NOTARGET')
            t.wait(2.0)
            t.flags('player1', '+FL_NOTARGET')
            t.damage('a', 1, 2)
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
    case.__name__ = 'apache_%s_%d' % (key.lower(), c)
    return case


for _key in KEYS:
    for _c in (0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 13):
        case_for(_key, _c)

# soundmask: does it hear a World sound (TriggerCondition 7)? Its own mask is World, Combat, Player (ISoundMask)
MASKS = {-1: False, 0: True, 32: False, 33: False, 37: False, 127: True, 1028: True}


def mask_case(m, heard):
    @S.test('%s "soundmask" %d: it %s an env_soundmark of type World (TriggerCondition 7 Hear World %s)' % (
                A, m, 'hears' if heard else 'does not hear', 'fires' if heard else 'never fires'),
            players=1, doc=DOC, covers=('%s.soundmask' % A, '%s.soundmask=%d' % (A, m)))
    def case(t):
        apache(t, {'TriggerCondition': 7, 'soundmask': m})
        t.wait(1.0)
        t.mark('go')
        t.point('env_soundmark', targetname='snd', at=(0, 100, 150), type='2', radius='600', duration='1')
        t.fire('snd')
        t.wait(1.5)
        finish(t)
        if heard:
            t.expect.used('sink', count=1, after='go')
        else:
            t.expect.not_used('sink')
    case.__name__ = 'apache_soundmask_%d' % m if m >= 0 else 'apache_soundmask_deaf'
    return case


for _m, _h in MASKS.items():
    mask_case(_m, _h)


@S.test('%s "target" (a path_corner 300 units away): it flies toward it; one without a target hovers where it is' % A,
        players=1, size=(512, 512, 400), doc=DOC, covers=(A + '.target',))
def apache_target(t):
    t.point(A, targetname='pre', at=(200, -200, 120), angles='0 90 0', spawnflags='80')
    t.point('trigger_relay', targetname='rm_pre', killtarget='pre')
    t.point('trigger_relay', targetname='rm_a', killtarget='a')
    t.point('trigger_relay', targetname='rm_b', killtarget='b')
    t.point('path_corner', targetname='pc', at=(-150, -150, 250))
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.fire('rm_pre')
    for name, x, kv in (('a', -150, ['target', t.name('pc')]), ('b', 150, [])):
        p = t._abs((x, 150, 250))
        t.console(' '.join(['test_create', A, '%g' % p[0], '%g' % p[1], '%g' % p[2], '270', 'targetname',
                            t.name(name)] + kv))
    t.wait(0.3)
    t.snap('pre', 'a', 'origin')
    t.snap('pre', 'b', 'origin')
    t.snap('pre', 'pc', 'origin')
    t.wait(4.0)
    t.snap('end', 'a', 'origin')
    t.snap('end', 'b', 'origin')
    t.snap('end', 'pc', 'origin')
    t.fire('rm_a')
    t.fire('rm_b')
    t.wait(0.2)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.distance('pre', 'a', 'pc', 280, 320)
    t.expect.distance('end', 'a', 'pc', 0, 200)
    t.expect.moved('pre', 'end', 'b', delta=(0, 0, 0), tol=60)
