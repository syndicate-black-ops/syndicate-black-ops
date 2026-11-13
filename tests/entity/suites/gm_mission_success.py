"""GM mode (docs/GM_MODE.md "Leaving"): the mission's end ends GM mode for everybody (CCoop::BeginMissionEnd ->
GmMode_StopAll). Its own suite: a mission end is once per map. Bug 2026-11-05 (dev): still GM after the
mission ended."""

from etest import Suite

SUITE = Suite('gm_mission_success', 'GM mode ends at the mission end (game_end)', players=2, requires=('GmMode_StopAll',))
S = SUITE


@S.test('a Game Master is out of GM mode after game_end', players=2, doc='GM_MODE.md', covers=('game_end',))
def gm_ends(t):
    t.console('mp_gmmode 1')
    t.point('game_end', targetname='outcome')
    t.wait(0.3)
    t.cmd('player1', 'gmon')
    t.wait(0.3)
    t.cmd('player1', 'gm')
    t.wait(0.3)
    t.snap('gm', 'player1', 'game_master', 'iuser1', 'iuser4')
    t.fire('outcome', activator='player2')
    t.wait(1.0)
    t.snap('after', 'player1', 'game_master', 'iuser4')
    t.expect.value('gm', 'player1', 'game_master', 1)
    t.expect.value('after', 'player1', 'game_master', 0)
    t.expect.value('after', 'player1', 'iuser4', 1, op='!&')
