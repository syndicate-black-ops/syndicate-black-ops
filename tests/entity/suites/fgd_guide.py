"""Tier 5, batch 4: FGD items of the Featureful guide entities that no 1.38 map uses and no other test asserted
(docs/FEATUREFUL_GUIDE_ENTITIES.md, docs/FEATUREFUL_OPTIONS.md): player_capabilities' choices (seen in the
"Capability" message, the bits the client predicts with), ambient_extraspeaker's channel, volume, pitch and flags,
env_spark's sound radius and Spark Silent No, env_shockwave's cylinder choices. ambient_random: map-test cases 200 /
200b."""

from etest import Suite

SUITE = Suite('fgd_guide', 'Tier 5: Featureful guide entities, FGD-only choices', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
OPTIONS = 'FEATUREFUL_OPTIONS.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3
TE_BEAMCYLINDER = 21

# capability key -> PLAYER_SUPPRESS_* bit (pm_shared.h)
CAPS = {'attack_capability': 1, 'jump_capability': 2, 'duck_capability': 4, 'use_capability': 8,
        'stepsound_capability': 16, 'movement_capability': 32}


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def caps_case(key, bit):
    @S.test('player_capabilities %s: Off sets the bit, No change leaves it, Toggle flips it, Copy use input follows '
            'the use type (On / Off / Toggle), On clears it' % key, players=1, doc=DOC,
            covers=('player_capabilities.%s' % key,) + tuple('player_capabilities.%s=%d' % (key, v)
                                                             for v in (-1, 0, 1, 2, 3)))
    def case(t):
        for name, v in (('off', '-1'), ('none', '0'), ('on', '1'), ('tog', '2'), ('copy', '3')):
            t.point('player_capabilities', targetname=name, **{key: v})
        steps = []

        def step(label, target, use, bits):
            t.mark(label)
            t.fire(target, activator='player1', use=use)
            t.wait(1.0)     # apart by more than a loaded frame (the windows widen by the frame length)
            steps.append((label, bits))

        t.fire('on', activator='player1')
        t.wait(0.3)
        step('off', 'off', 'toggle', bit)
        step('none', 'none', 'toggle', None)            # no change: no message
        step('tog1', 'tog', 'toggle', 0)
        step('tog2', 'tog', 'toggle', bit)
        step('copy_on', 'copy', 'on', 0)
        step('copy_off', 'copy', 'off', bit)
        step('copy_tog', 'copy', 'toggle', 0)
        step('off2', 'off', 'toggle', bit)
        step('on', 'on', 'toggle', 0)
        for label, bits in steps:
            if bits is None:
                t.expect.no_msg('Capability', to='player1', after=label, within=(0, 0.5))
            else:
                t.expect.msg('Capability', to='player1', fields={'bits': bits}, after=label, within=(0, 0.5),
                             count=1)
    case.__name__ = 'caps_' + key.split('_')[0]
    return case


for _k, _b in CAPS.items():
    caps_case(_k, _b)


# ---------------------------------------------------------------------------------------------- ambient_extraspeaker
CHANNELS = {0: 6, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6}      # 0 Default: Static


@S.test('ambient_extraspeaker Channel: every choice plays on that channel (0 Default: Static); Volume and Pitch',
        players=1, doc=DOC,
        covers=('ambient_extraspeaker.channel', 'ambient_extraspeaker.health', 'ambient_extraspeaker.pitch')
        + tuple('ambient_extraspeaker.channel=%d' % c for c in CHANNELS))
def es_channels(t):
    for c in CHANNELS:
        info(t, 'host%d' % c, at=(-180 + c * 60, 100, 32))
        t.point('ambient_extraspeaker', targetname='es%d' % c, message='buttons/blip1.wav', target='host%d' % c,
                channel=str(c), health='5', pitch='150', spawnflags=str(16 | 32))
        t.fire('es%d' % c)
    t.wait(0.2)
    t.expect.sound('buttons/blip1.wav', count=len(CHANNELS))
    for chan in sorted(set(CHANNELS.values())):
        n = sum(1 for v in CHANNELS.values() if v == chan)
        t.expect.sound('buttons/blip1.wav', chan=chan, vol=(0.45, 0.55), pitch=150, count=n)


RADIUS = {1: 0.0, 2: 2.0, 4: 1.25, 8: 0.8}


@S.test('ambient_extraspeaker radius flags 1 / 2 / 4 / 8: attenuation 0 / 2 / 1.25 / 0.8', players=1, doc=DOC,
        covers=tuple('ambient_extraspeaker:%d' % f for f in RADIUS) + ('ambient_extraspeaker:32',))
def es_radius(t):
    for i, f in enumerate(RADIUS):
        info(t, 'host%d' % f, at=(-150 + i * 100, 100, 32))
        t.point('ambient_extraspeaker', targetname='es%d' % f, message='buttons/blip%d.wav' % (1 if i % 2 == 0 else 2),
                target='host%d' % f, spawnflags=str(f | 16 | 32))
        t.mark('m%d' % f)
        t.fire('es%d' % f)
        t.wait(0.2)
    for f, attn in RADIUS.items():
        t.expect.sound('buttons/blip', attn=attn, count=1, after='m%d' % f, within=(0, 0.1))


@S.test('ambient_extraspeaker without Start Silent (16) on a named entity: plays from the map start; a trigger stops '
        'it, the next starts it again', players=1, doc=DOC,
        covers=('ambient_extraspeaker:16',))
def es_not_silent(t):
    info(t, 'host', at=(0, 100, 32))
    t.point('ambient_extraspeaker', targetname='es', message='ambience/alien_zonerator.wav', target='host',
            spawnflags='4')
    t.mark('stop')
    t.fire('es')
    t.wait(0.2)
    t.mark('start')
    t.fire('es')
    t.wait(0.2)
    t.expect.sound('alien_zonerator', after='stop', within=(0, 0.1), flags=32, count=1)
    t.expect.sound('alien_zonerator', after='start', within=(0, 0.1), min=1)   # (Play stops it first: SND_STOP + play)
    t.expect.sound('alien_zonerator', min=3)       # the map start's, the stop, the restart


@S.test('ambient_extraspeaker without Start Silent and no target (the activator): the first trigger plays it on '
        'the player ("a trigger starts it and the next stops it")', players=1, doc=DOC,
        covers=('ambient_extraspeaker:16',))
def es_not_silent_player(t):
    t.point('ambient_extraspeaker', targetname='es', message='ambience/alien_zonerator.wav', spawnflags='4')
    t.wait(0.2)
    t.mark('first')
    t.fire('es', activator='player1')
    t.wait(1.0)     # the second trigger more than a loaded frame later (the window widens by the frame length)
    t.fire('es', activator='player1')
    t.expect.sound('alien_zonerator', after='first', within=(0, 0.5), count=1, flags=None)


# ---------------------------------------------------------------------------------------------- env_spark
SPARK_RADIUS = {-2: 2.0, -1: 1.25, 0: 0.8, 1: 0.5, 3: 0.0}


@S.test('env_spark Sound radius Small / Medium / Default / Huge / Everywhere: the spark sound\'s attenuation; Spark '
        'Silent No: it sounds', doc=OPTIONS,
        covers=tuple('env_spark.soundradius=%d' % r for r in SPARK_RADIUS) + ('env_spark.spark_silent=0',))
def spark_radius(t):
    for i, r in enumerate(SPARK_RADIUS):
        t.point('env_spark', targetname='sp%d' % i, at=(-200 + i * 100, 0, 40), spawnflags='16', MaxDelay='0.5',
                soundradius=str(r), spark_silent='0')
    t.wait(0.1)
    t.mark('go')
    for i in range(len(SPARK_RADIUS)):
        t.fire('sp%d' % i)
    t.wait(0.3)
    for i, (r, attn) in enumerate(SPARK_RADIUS.items()):
        t.expect.sound('buttons/spark', ent='sp%d' % i, attn=attn, after='go', count=1)


# ---------------------------------------------------------------------------------------------- env_shockwave
@S.test('env_shockwave Type 0 (Default) and 21 (Cylinder): a beam cylinder', doc=DOC,
        covers=('env_shockwave.m_cType=0', 'env_shockwave.m_cType=21'))
def shockwave_cylinder(t):
    t.point('env_shockwave', targetname='s0', at=(-100, 0, 16), m_cType='0', m_iHeight='32', m_iRadius='200',
            m_iTime='5')
    t.point('env_shockwave', targetname='s21', at=(100, 0, 16), m_cType='21', m_iHeight='32', m_iRadius='200',
            m_iTime='5')
    t.fire('s0')
    t.fire('s21')
    t.wait(0.2)
    t.expect.msg('svc:23', fields={'te': TE_BEAMCYLINDER, 'pos': [float(v) for v in t._abs((-100, 0, 32))]}, count=1)
    t.expect.msg('svc:23', fields={'te': TE_BEAMCYLINDER, 'pos': [float(v) for v in t._abs((100, 0, 32))]}, count=1)
