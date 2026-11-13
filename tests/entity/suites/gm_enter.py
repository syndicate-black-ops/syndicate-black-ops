"""GM mode entering (docs/GM_MODE.md "Turning it on"): a living or dead player becomes a Game Master (as SBO's gmon;
the "only while alive" rule is the GM mode plugin's), one Game Master at a time (by design). A dead Game
Master who leaves is a dead player again, as a living one who leaves."""

from etest import Suite

SUITE = Suite('gm_enter', 'GM mode: who can become a Game Master', players=3, requires=('GmMode_RoundStart',))
S = SUITE


def _dead_gm(t, flDeadFor):
    t.console('mp_gmmode 1')
    t.wait(4.0)
    t.damage('player3', 5000)
    t.wait(flDeadFor)
    t.snap('dead', 'player3', 'game_master', 'deadflag', 'health', 'iuser1')
    t.cmd('player3', 'gmon')
    t.wait(0.3)
    t.cmd('player3', 'gm')	# the intro closed
    t.wait(0.3)
    t.snap('gm', 'player3', 'game_master', 'iuser1', 'iuser4', 'flags')
    t.cmd('player3', 'gm_leave')
    t.wait(0.3)
    t.snap('left', 'player3', 'game_master', 'deadflag', 'health', 'iuser1', 'iuser4')
    t.wait(8.0)
    t.snap('later', 'player3', 'game_master', 'deadflag', 'health', 'iuser1')
    t.expect.value('dead', 'player3', 'deadflag', 0, op='!=')
    t.expect.value('gm', 'player3', 'game_master', 1)
    t.expect.value('gm', 'player3', 'iuser1', 3)			# OBS_ROAMING
    t.expect.value('left', 'player3', 'game_master', 0)
    t.expect.value('left', 'player3', 'deadflag', 0, op='!=')	# dead where the GM was, no kill
    t.expect.value('left', 'player3', 'iuser4', 1, op='!&')		# the GM roaming mark is gone (a dead observer again)
    t.expect.value('later', 'player3', 'game_master', 0)
    t.expect.value('later', 'player3', 'deadflag', 0, op='!=')	# still dead until the next round spawns him


@S.test('a dying player becomes a Game Master and leaves as a dead player', players=3, doc='GM_MODE.md')
def dying_gm(t):
    _dead_gm(t, 0.5)


@S.test('a dead observer becomes a Game Master and leaves as a dead player', players=3, doc='GM_MODE.md')
def dead_observer_gm(t):
    _dead_gm(t, 9.0)


@S.test('one Game Master at a time: a second player\'s gmon is refused', players=3, doc='GM_MODE.md')
def one_gm(t):
    t.console('mp_gmmode 1')
    t.cmd('player1', 'gmon')
    t.cmd('player2', 'gmon')
    t.wait(0.3)
    t.snap('gm', 'player1', 'game_master')
    t.snap('gm', 'player2', 'game_master')
    t.expect.value('gm', 'player1', 'game_master', 1)
    t.expect.value('gm', 'player2', 'game_master', 0)
