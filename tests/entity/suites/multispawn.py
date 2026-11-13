"""multispawn through real round restarts (GAPS.md tier 1 "multispawn (+master)", 29 maps): "each trigger moves the
player spawns to the next zone, from the next round": info_player_deathmatch, then info_player_go2 .. go5 (go5 stays).
Its own suite: the spawn zone is the map's; the tests build on each other, in order."""

from etest import Suite

SUITE = Suite('multispawn', 'multispawn: spawn zones go2..go5 from the next round, master')
S = SUITE


def restart(t):
    t.console('test_round restart')
    t.wait_for_round(3, 15)
    t.wait(0.3)


@S.test('multispawn: nothing while its master is locked (the next round spawns in the lobby)', players=1,
        covers=('multispawn.master',))
def locked(t):
    t.point('multispawn', targetname='=et_ms', master='=et_lock')
    t.point('multisource', targetname='=et_lock')
    t.point('trigger_relay', targetname='=et_key', target='=et_lock', triggerstate='1')
    t.point('info_player_go2', targetname='go2', at=(0, 100, 0), angles='0 90 0')
    t.point('info_player_go3', targetname='go3', at=(0, -100, 0), angles='0 90 0')
    t.point('info_player_go5', targetname='go5', at=(150, 0, 0), angles='0 90 0')
    t.fire('=et_ms', activator='player1')
    restart(t)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'go2', 'origin')
    t.expect.distance('after', 'player1', 'go2', 300, 100000)


@S.test('multispawn: fired (master unlocked), the players spawn at info_player_go2 from the next round; again: go3; '
        'three times more (go4, go5, go5) in one round: go5, it stays there', players=1,
        covers=('multispawn', 'info_player_go2', 'info_player_go3', 'info_player_go5'))
def zones(t):
    t.fire('=et_key')
    t.wait(0.1)
    t.fire('=et_ms', activator='player1')
    restart(t)
    t.snap('r2', 'player1', 'origin')
    t.fire('=et_ms', activator='player1')
    restart(t)
    t.snap('r3', 'player1', 'origin')
    t.fire('=et_ms', activator='player1')
    t.fire('=et_ms', activator='player1')
    t.fire('=et_ms', activator='player1')
    restart(t)
    t.snap('r5', 'player1', 'origin')
    for lab in ('r2', 'r3', 'r5'):
        for n in ('=t01_go2', '=t01_go3', '=t01_go5'):
            t.snap(lab, n, 'origin')
    t.expect.distance('r2', 'player1', '=t01_go2', 0, 2)
    t.expect.distance('r3', 'player1', '=t01_go3', 0, 2)
    t.expect.distance('r5', 'player1', '=t01_go5', 0, 2)
