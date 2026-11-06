"""Mapper entities that need players coming and going or a round restart (branch feat/mapper-entities):
trigger_inout (docs/STATE_LOGIC.md), round hooks game_round_start / game_round_end and round_reset
(docs/LOGIC_ENTITIES.md "Rounds"). Its own suite: a round restart respawns every player."""

from etest import Suite

SUITE = Suite('mapper_rounds', 'Mapper entities: trigger_inout, round hooks, round_reset (feat/mapper-entities)',
              requires=('RoundHooks_Reset', 'MapCompat_KeyValue'))
S = SUITE
USE_OFF, USE_ON = 0, 1


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


@S.test('trigger_inout: entering fires target and both ON, leaving fires the leave target and both OFF', players=3,
        doc='STATE_LOGIC.md')
def inout(t):
    t.trigger('trigger_inout', size=(128, 128, 96), at=(0, 120, 0), target='enter', m_iszAltTarget='leave',
              m_iszBothTarget='both')
    for n in ('enter', 'leave', 'both'):
        info(t, n)
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.5)
    t.mark('out')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.5)
    t.expect.fired('enter', activator='player1', count=1, after='in', within=(0, 0.2))
    t.expect.fired('both', activator='player1', use=USE_ON, count=1, after='in', within=(0, 0.2))
    t.expect.fired('leave', activator='player1', count=1, after='out', within=(0, 0.3))
    t.expect.fired('both', activator='player1', use=USE_OFF, count=1, after='out', within=(0, 0.3))


FROZEN = 4096


def frozen_camera(t, player, wait='3'):
    """A trigger_camera with Freeze Player (4) holding `player` for `wait` seconds; snapshot 'run' while it runs."""
    info(t, 'look', at=(200, 200, 40))
    t.point('trigger_camera', targetname='cam', at=(-150, -100, 100), target='look', wait=wait, spawnflags='4')
    t.fire('cam', activator=player)
    t.wait(0.2)
    t.snap('run', player, 'flags')
    t.expect.value('run', player, 'flags', FROZEN, op='&')


@S.test('trigger_camera Freeze Player: FL_FROZEN while it runs, cleared at its end', players=3, doc='LOGIC_ENTITIES.md')
def cam_end(t):
    frozen_camera(t, 'player1', wait='0.6')
    t.wait(0.8)
    t.snap('after', 'player1', 'flags')
    t.expect.value('after', 'player1', 'flags', FROZEN, op='!&')


@S.test('trigger_camera Freeze Player: death clears FL_FROZEN', players=3, doc='LOGIC_ENTITIES.md')
def cam_death(t):
    frozen_camera(t, 'player2')
    t.damage('player2', 5000)
    t.wait(0.3)
    t.snap('after', 'player2', 'flags', 'deadflag')
    t.expect.value('after', 'player2', 'flags', FROZEN, op='!&')


@S.test('trigger_inout: a player who dies inside counts as leaving', players=3, doc='STATE_LOGIC.md')
def inout_death(t):
    t.trigger('trigger_inout', size=(128, 128, 96), at=(0, 120, 0), target='enter', m_iszAltTarget='leave')
    info(t, 'enter')
    info(t, 'leave')
    t.wait(0.3)
    t.teleport('player3', (0, 120, 0))
    t.wait(0.4)
    t.mark('kill')
    t.damage('player3', 5000)
    t.wait(0.6)
    t.expect.fired('enter', activator='player3', count=1)
    t.expect.fired('leave', activator='player3', count=1, after='kill', within=(0, 0.35))


@S.test('round restart: game_round_end then game_round_start (world as activator), round_reset entities reset',
        players=3, doc='LOGIC_ENTITIES.md')
def round_hooks(t):
    t.point('trigger_relay', targetname='=game_round_end', target='ended', triggerstate='2')
    t.point('trigger_relay', targetname='=game_round_start', target='started', triggerstate='2')
    info(t, 'ended')
    info(t, 'started')
    t.point('game_number', targetname='num', frags='5', round_reset='1')
    t.point('game_number', targetname='keep', frags='5', round_reset='0')
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', target_count='2', round_reset='1',
            trigger_on_exhaust='empty')
    for n in ('a', 'b', 'empty'):
        info(t, n)
    for n in ('num', 'keep'):
        t.fire(n, use='on')
        t.fire(n, use='on')
    t.fire('rnd')
    t.fire('rnd')
    t.wait(0.1)
    t.snap('before', 'num', 'frags')
    t.snap('before', 'keep', 'frags')
    frozen_camera(t, 'player1', wait='20')       # still running when the round ends
    t.mark('restart')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.5)
    t.snap('after', 'num', 'frags')
    t.snap('after', 'keep', 'frags')
    t.snap('after', 'player1', 'flags')
    t.mark('again')
    t.fire('rnd')
    t.wait(0.1)
    t.expect.value('after', 'player1', 'flags', FROZEN, op='!&')    # the round end released the camera's player
    t.expect.value('before', 'num', 'frags', 7)
    t.expect.value('after', 'num', 'frags', 5)
    t.expect.value('after', 'keep', 'frags', 7)
    t.expect.fired('=game_round_end', activator='world', count=1, after='restart')
    t.expect.fired('=game_round_start', activator='world', count=1, after='restart')
    t.expect.order('=game_round_end', '=game_round_start')
    t.expect.used('ended', min=1, after='restart')
    t.expect.used('started', min=1, after='restart')
    t.expect.fired('empty', count=1)                                # exhausted before the restart
    t.expect.used('@info_target', caller='rnd', count=1, after='again')     # a full pool again after it


@S.test('trigger_camera Freeze Player: a respawn clears FL_FROZEN', players=3, doc='LOGIC_ENTITIES.md')
def cam_respawn(t):
    frozen_camera(t, 'player1')
    t.console('test_spawn player1')
    t.wait(0.3)
    t.snap('after', 'player1', 'flags')
    t.expect.value('after', 'player1', 'flags', FROZEN, op='!&')


@S.test('trigger_camera Freeze Player: going to spectator mode clears FL_FROZEN', players=3, doc='LOGIC_ENTITIES.md')
def cam_spectate(t):
    frozen_camera(t, 'player2')
    t.cmd('player2', 'spectate')
    t.wait(0.5)
    t.snap('after', 'player2', 'flags')
    t.expect.value('after', 'player2', 'flags', FROZEN, op='!&')
