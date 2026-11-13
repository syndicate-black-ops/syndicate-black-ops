"""Bounty hunt (bk_) scoring after 0eb55ce9 (docs/ROUNDS.md "Suicides cost no points in BR and bounty hunt"): a
suicide (the player as his own attacker: "kill", his own explosive) and a world death (trigger_hurt) take no frags
(1.38: -10 and -5). GAPS.md tier 2 "Round, mission and rules commits". The BR side (suicide -5 removed) has no test:
the test players can't join a BR round (test_round is co-op only; they stay observers without a loadout).
"""

from etest import Suite

SUITE = Suite('bk_scoring', 'Bounty hunt: suicides and world deaths cost no frags (0eb55ce9)')
SUITE.map_prefix = 'bk_'
S = SUITE


def respawn(t, player='player1'):
    t.console('wait_for alive %s 20' % t.ref(player))
    t.wait(1.0)


@S.test('bk_: a suicide (the player as his own attacker) costs no frags (1.38: -10)', players=1,
        covers=('info_player_deathmatch',))
def bk_suicide(t):
    t.wait(1.0)
    t.snap('before', 'player1', 'frags')
    t.damage('player1', 5000, 0, 'player1')     # his own attacker: a suicide (as his own grenade)
    t.wait(0.5)
    t.snap('after', 'player1', 'frags', 'deadflag')
    respawn(t)
    t.expect.value('after', 'player1', 'deadflag', 0, op='>')
    t.expect.same('before', 'after', 'player1', 'frags')


@S.test('bk_: a world death (trigger_hurt) costs no frags (1.38: -5)', players=1, covers=('trigger_hurt',))
def bk_world(t):
    t.trigger('trigger_hurt', size=(96, 96, 72), at=(0, 100, 0), dmg='5000')
    t.teleport('player1', (0, -150, 0))
    t.wait(1.0)
    t.snap('before', 'player1', 'frags')
    t.teleport('player1', (0, 100, 0))
    t.wait(0.5)
    t.snap('after', 'player1', 'frags', 'deadflag')
    respawn(t)
    t.expect.value('after', 'player1', 'deadflag', 0, op='>')
    t.expect.same('before', 'after', 'player1', 'frags')
