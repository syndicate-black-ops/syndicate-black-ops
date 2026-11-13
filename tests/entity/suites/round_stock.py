"""Stock entities across a real co-op round restart (GAPS.md tier 1 "Round reset of stock logic"): SBO's rounds respawn
the players, not the map's entities (docs/LOGIC_ENTITIES.md "Rounds": a door opened in round 1 is still open in round 2,
a counter keeps its count; only "round_reset" entities go back). A fired trigger_once stays gone, an unlocked
multisource stays unlocked, a game_counter keeps its count, an open toggle door stays open, a broken func_breakable
stays broken; the restart leaks no entities. Its own suite: it restarts the round."""

from etest import Suite

SUITE = Suite('round_stock', 'Stock entities keep their state across a round restart; no leaked entities')
S = SUITE


@S.test('round restart: trigger_once, multisource, game_counter, func_door, func_breakable keep their state; the '
        'number of entities is the same after the restart', players=2,
        covers=('trigger_once', 'multisource', 'game_counter', 'func_door', 'func_breakable'))
def keep_state(t):
    t.trigger('trigger_once', size=(96, 96, 72), at=(-150, 150, 0), target='once_out')
    t.point('info_target', targetname='once_out')
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')
    t.trigger('trigger_multiple', size=(96, 96, 72), at=(150, 150, 0), target='locked_out', master='lock', wait='5')
    t.point('info_target', targetname='locked_out')
    t.point('game_counter', targetname='count', health='3', target='count_out')
    t.point('info_target', targetname='count_out')
    t.brush('func_door', size=(64, 16, 96), at=(0, 220, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='400', lip='8', spawnflags='32')
    t.brush('func_breakable', size=(32, 32, 32), at=(150, -150, 0), targetname='crate', health='10')
    t.wait(1.0)     # first test: touches and +use right after the suite start were not taken (seen 2026-11-07)
    # round 1: fire the trigger_once, unlock the master, count twice, open the door, break the crate
    t.teleport('player1', (-150, 150, 0))
    t.wait(0.3)
    t.teleport('player1', (0, -150, 0))
    t.fire('key')
    t.fire('count')
    t.fire('count')
    t.fire('door')
    t.damage('crate', 50)
    t.wait(0.6)
    t.snap('before', 'door', 'origin')
    t.mark('before')
    t.console('test_round restart')
    t.wait_for_round(3, 15)
    t.wait(0.5)
    t.mark('after')
    t.snap('after', 'door', 'origin')
    # round 2: the trigger_once is gone, the master still open, one more count reaches 3
    t.teleport('player1', (-150, 150, 0))
    t.wait(0.3)
    t.teleport('player2', (150, 150, 0))
    t.wait(0.3)
    t.fire('count')
    t.wait(0.2)
    t.expect.fired('once_out', count=1)
    t.expect.not_fired('once_out', after='after')
    t.expect.fired('locked_out', activator='player2', count=1, after='after')
    t.expect.fired('count_out', count=1, after='after')
    t.expect.value('before', 'door', 'origin', [0, 0, 88], tol=0.5)
    t.expect.value('after', 'door', 'origin', [0, 0, 88], tol=0.5)
    t.expect.removed('crate', count=1)
    t.expect.edicts('before', 'after', 0, 0)
