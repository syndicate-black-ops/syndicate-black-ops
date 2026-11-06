"""GM mode entering (docs/GM_MODE.md "Turning it on"): only a living player becomes a Game Master, as the owner's GM
mode plugin ("Unable to enter Game Master Mode while dead"), and one Game Master at a time (the owner)."""

from etest import Suite

SUITE = Suite('gm_enter', 'GM mode: who can become a Game Master', players=3, requires=('GmMode_RoundStart',))
S = SUITE


@S.test('a dead player does not become a Game Master', players=3, doc='GM_MODE.md')
def dead_refused(t):
    t.console('mp_gmmode 1')
    t.wait(4.0)
    t.damage('player3', 5000)
    t.wait(0.5)
    t.cmd('player3', 'gmon')
    t.wait(0.3)
    t.snap('after', 'player3', 'game_master', 'deadflag', 'health')
    t.expect.value('after', 'player3', 'game_master', 0)


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
