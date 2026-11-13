"""Tier 5: round_reset 0 / 1 on the logic entities whose round_reset no 1.38 map uses and no other test asserted
(docs/LOGIC_ENTITIES.md "Rounds", STATE_LOGIC.md, LIGHT_STYLES.md), and env_state's master, turn-off time and debug
flag. Its own suite: a round restart respawns every player and resets light styles map-wide."""

from etest import Suite

SUITE = Suite('fgd_rounds', 'Tier 5: round_reset on logic entities, env_state keys', players=3,
              requires=('RoundHooks_Reset',))
S = SUITE
SUITE.server_args = ('+developer', '1')      # env_state Debug Mode notes are developer messages
LOGIC = 'LOGIC_ENTITIES.md'
STATE = 'STATE_LOGIC.md'
USE_OFF, USE_ON = 0, 1


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


def restart(t):
    t.mark('restart')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.3)
    t.mark('after')


@S.test('env_state master: triggers are ignored while the master is locked, Debug Mode (2) says so', players=3,
        doc=STATE, covers=('env_state.master', 'env_state:2'))
def state_master(t):
    t.point('env_state', targetname='lock', spawnflags='0')            # off: a locked master
    t.point('env_state', targetname='st', master='lock', target='changed', spawnflags='2')
    info(t, 'changed')
    t.fire('st', use='on')
    t.wait(0.1)
    t.mark('unlocked')
    t.fire('lock', use='on')
    t.wait(0.1)
    t.fire('st', use='on')
    t.wait(0.1)
    t.expect.fired('changed', use=USE_ON, count=1, after='unlocked')
    t.expect.fired('changed', count=1)
    t.expect.console('ignored trigger USE_ON; locked by master')


@S.test('env_state Time taken to turn off: Off fires that long after the trigger, the env_state as activator; '
        'then it is a locked master', players=3, doc=STATE, covers=('env_state.turnofftime',))
def state_turnoff(t):
    t.point('env_state', targetname='st', target='changed', noise2='offnow', turnofftime='0.5', spawnflags='1')
    info(t, 'changed')
    info(t, 'offnow')
    t.point('env_state', targetname='probe', master='st', target='passed')     # ignores triggers while st fails
    info(t, 'passed')
    t.mark('off')
    t.fire('st', use='off')
    t.wait(0.8)
    t.fire('probe', use='on')
    t.wait(0.1)
    t.expect.fired('changed', use=USE_OFF, activator='st', count=1, after='off', within=(0.45, 0.55))
    t.expect.fired('offnow', count=1, after='off', within=(0.45, 0.55))
    t.expect.not_fired('passed')


@S.test('env_state Time taken to turn off: as a master it is off from the trigger on, for the whole time',
        players=3, doc=STATE, covers=('env_state.turnofftime',))
def state_turnoff_master(t):
    t.point('env_state', targetname='st', target='changed', turnofftime='0.5', spawnflags='1')
    info(t, 'changed')
    t.point('env_state', targetname='probe', master='st', target='passed')
    info(t, 'passed')
    t.fire('st', use='off')
    t.wait(0.2)
    t.mark('probe')
    t.fire('probe', use='on')
    t.wait(0.1)
    t.expect.not_fired('passed')


@S.test('round_reset 1 / 0 at a new round: trigger_random, trigger_random_unique, trigger_timer, trigger_condition, '
        'env_state back to the map-load state (1) or kept (0); trigger_lightstyle / global_light_control 0 leave '
        'the styles', players=3, doc=LOGIC,
        covers=('trigger_random.round_reset', 'trigger_random.round_reset=1', 'trigger_random.round_reset=0',
                'trigger_random_unique.round_reset', 'trigger_random_unique.round_reset=1',
                'trigger_random_unique.round_reset=0',
                'trigger_timer.round_reset', 'trigger_timer.round_reset=1', 'trigger_timer.round_reset=0',
                'trigger_condition.round_reset', 'trigger_condition.round_reset=1',
                'trigger_condition.round_reset=0',
                'env_state.round_reset', 'env_state.round_reset=1', 'env_state.round_reset=0',
                'trigger_lightstyle.round_reset=0', 'global_light_control.round_reset=0'))
def round_reset(t):
    # Unique pools, exhausted before the round restart
    t.point('trigger_random', targetname='r1', target1='r1a', target2='r1b', spawnflags='16', round_reset='1')
    t.point('trigger_random', targetname='r0', target1='r0a', target2='r0b', spawnflags='16', round_reset='0')
    t.point('trigger_random_unique', targetname='u1', target1='u1a', target2='u1b', round_reset='1')
    t.point('trigger_random_unique', targetname='u0', target1='u0a', target2='u0b', round_reset='0')
    for n in ('r1a', 'r1b', 'r0a', 'r0b', 'u1a', 'u1b', 'u0a', 'u0b'):
        info(t, n)
    # timers turned on before the restart (neither starts on)
    t.point('trigger_timer', targetname='tm1', target='tick1', min_delay='0.2', max_delay='0.2', round_reset='1')
    t.point('trigger_timer', targetname='tm0', target='tick0', min_delay='0.2', max_delay='0.2', round_reset='0')
    info(t, 'tick1')
    info(t, 'tick0')
    # constant conditions, Start Off, turned on before the restart, firing on every check (mode 3)
    info(t, 'mon', health='5')
    for n in ('1', '0'):
        t.point('trigger_condition', targetname='c' + n, target='mon', m_iszValueName='health', m_iszCheckValue='5',
                m_iCheckType='0', m_iCheckBehaviour='3', m_fCheckInterval='0.2', netname='true' + n,
                spawnflags='1', round_reset=n, names=('netname',))
        info(t, 'true' + n)
    # env_states, off at map load, turned on before the restart; an env_state with it as master probes each
    for n in ('1', '0'):
        t.point('env_state', targetname='s' + n, target='schanged' + n, round_reset=n)
        info(t, 'schanged' + n)
        t.point('env_state', targetname='probe' + n, master='s' + n, target='spass' + n)
        info(t, 'spass' + n)
    # light styles changed before the restart by entities with round_reset 0
    t.point('light', targetname='lamp', at=(0, 0, 100), _light='255 255 255 100', pattern='mmo')
    t.point('trigger_lightstyle', targetname='ls', target='lamp', pattern='zzz', m_iWait='-1', round_reset='0')
    t.point('global_light_control', targetname='dim', pattern='b', round_reset='0')

    for n in ('r1', 'r0', 'u1', 'u0'):
        t.fire(n)
        t.fire(n)
    t.fire('tm1', use='on')
    t.fire('tm0', use='on')
    t.fire('c1', use='on')
    t.fire('c0', use='on')
    t.fire('s1', use='on')
    t.fire('s0', use='on')
    t.fire('ls')
    t.fire('dim')
    t.wait(0.5)
    restart(t)
    for n in ('r1', 'r0', 'u1', 'u0'):
        t.fire(n)
    t.fire('probe1', use='on')
    t.fire('probe0', use='on')
    t.wait(1.0)

    # before the restart every pool was used up
    for n in ('r1a', 'r1b', 'r0a', 'r0b', 'u1a', 'u1b', 'u0a', 'u0b'):
        t.expect.fired(n, min=1)
    t.expect.used('@info_target', caller='r1', count=1, after='after')     # a full pool again
    t.expect.used('@info_target', caller='u1', count=1, after='after')
    t.expect.not_fired('r0a', after='after')                               # still used up
    t.expect.not_fired('r0b', after='after')
    t.expect.not_fired('u0a', after='after')
    t.expect.not_fired('u0b', after='after')
    t.expect.not_fired('tick1', after='after')                             # off, as at map load
    t.expect.fired('tick0', min=3, after='after')                          # still running
    t.expect.not_used('true1', after='after')                              # Start Off again
    t.expect.used('true0', caller='c0', min=3, after='after')
    t.expect.not_fired('spass1', after='after')                            # off again, as a master locked
    t.expect.fired('spass0', count=1, after='after')
    t.expect.not_fired('schanged1', after='restart')                       # the reset fires nothing
    t.expect.lightstyle('m', style=0, count=0, after='restart')            # 'b' stays on the world
    t.expect.lightstyle('mmo', style=range(32, 64), count=0, after='restart')


@S.test('round_reset 1 / 0 at a new round: trigger_entity_iterator (a toggle-mode run stops / goes on), '
        'locus_variable (values back to the map\'s / kept), env_model (on again as the map says / stays off)',
        players=3, doc=LOGIC,
        covers=('trigger_entity_iterator.round_reset', 'trigger_entity_iterator.round_reset=1',
                'trigger_entity_iterator.round_reset=0',
                'locus_variable.round_reset', 'locus_variable.round_reset=1', 'locus_variable.round_reset=0',
                'env_model.round_reset', 'env_model.round_reset=1', 'env_model.round_reset=0'))
def round_reset2(t):
    info(t, 'thing')
    for n in ('1', '0'):
        t.point('trigger_entity_iterator', targetname='it' + n, name_filter='thing', target='hit' + n,
                triggerstate='2', run_mode='2', delay_between_runs='0.2', round_reset=n)
        info(t, 'hit' + n)
        t.point('locus_variable', targetname='lv' + n, m_iszPosition='0 0 0', m_iszVelocity='0 0 0', m_iszRatio='7',
                round_reset=n)
        t.point('env_model', targetname='mdl' + n, at=(int(n) * 100 - 50, 100, 0), model='models/barney.mdl',
                m_iszSequence_On='idle1', m_iszSequence_Off='laflinch', m_iAction_On='1', m_iAction_Off='0',
                round_reset=n)
    for n in ('1', '0'):
        t.fire('it' + n)
        t.fire('lv' + n)
        t.fire('mdl' + n, use='off')
    t.wait(0.5)
    t.snap('before', 'lv1', 'frags')
    t.snap('before', 'mdl1', 'sequence')
    restart(t)
    t.wait(1.0)
    for n in ('1', '0'):
        t.snap('end', 'lv' + n, 'frags')
        t.snap('end', 'mdl' + n, 'sequence')
    t.expect.value('before', 'lv1', 'frags', 7)
    t.expect.value('before', 'mdl1', 'sequence', 13)
    t.expect.not_fired('hit1', after='after')                     # the run stopped
    t.expect.fired('hit0', min=3, after='after')                  # still repeating
    t.expect.value('end', 'lv1', 'frags', 0)
    t.expect.value('end', 'lv0', 'frags', 7)
    t.expect.value('end', 'mdl1', 'sequence', 0)                  # on again: idle1
    t.expect.value('end', 'mdl0', 'sequence', 13)                 # still off: laflinch
