"""Round hooks in Bounty Killer (docs/ROUNDS.md "Map hooks"): game_round_start when a hunted is picked and the round's
players spawn, game_round_end when the hunted leaves (disconnects or turns spectator; a hunted killed by a player
passes the hunt on, the round goes on); Bounty Killer counts no rounds and has no mission, so
game_round_<N>, game_round_failed and the mission hooks never fire. Bounty Killer writes no round log lines."""

from etest import Suite

SUITE = Suite('round_hooks_bk', 'Bounty Killer: game_round_start / end only', players=2)
S = SUITE
S.map_prefix = 'bk_'
S.server_args = ('+log', 'on')
DOC = 'ROUNDS.md'
HOOKS = ('game_round_start', 'game_round_end', 'game_round_1', 'game_round_2', 'game_round_failed',
         'game_mission_success', 'game_mission_failed')


@S.test('the hunted (player1, the first ready player) turns spectator: game_round_end, 5 s later game_round_start; '
        'no game_round_<N>, failed or mission hooks, no Round_Start line', players=2, doc=DOC, size=(512, 512, 256))
def bk_rounds(t):
    for n in HOOKS:
        t.point('info_target', targetname='=' + n)
    t.wait(1.0)
    t.mark('leave')
    t.cmd('player1', 'spectate')
    t.wait(7.0)                                     # the 5 s pause, then the next round
    t.expect.msg('TextMsg', fields={'text': 'The HUNTED IS DEAD !!!\n'}, min=1, after='leave', within=(0, 0.5))
    t.expect.fired('=game_round_end', activator='world', count=1, after='leave', within=(0, 0.5))
    t.expect.fired('=game_round_start', activator='world', count=1, after='leave', within=(4.0, 7.0))
    t.expect.not_fired('=game_round_1')
    t.expect.not_fired('=game_round_2')
    t.expect.not_fired('=game_round_failed')
    t.expect.not_fired('=game_mission_success')
    t.expect.not_fired('=game_mission_failed')
    t.expect.no_console('World triggered "Round_Start"')
