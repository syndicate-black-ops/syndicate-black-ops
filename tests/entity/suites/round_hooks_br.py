"""Round hooks in Battle Royale (docs/ROUNDS.md "Map hooks"): game_round_start and game_round_<N> with each round of two
players or more (N as the Round_Start log line counts them), game_round_end with its Round_End line; BR has no lost
rounds and no mission, so game_round_failed / game_mission_success / game_mission_failed never fire. The players are
added by the test (edicts 1-2, =player1 / =player2) and answer the go menu themselves (observer_movetype_br)."""

from etest import Suite

SUITE = Suite('round_hooks_br', 'Battle Royale: game_round_<N> per round, no failed / mission hooks', players=0)
S = SUITE
S.map_prefix = 'br_'
S.server_args = ('+log', 'on')
DOC = 'ROUNDS.md'
HOOKS = ('game_round_start', 'game_round_end', 'game_round_1', 'game_round_2', 'game_round_3', 'game_round_failed',
         'game_mission_success', 'game_mission_failed')


@S.test('two players ready: round 1 (game_round_start, game_round_1, Round_Start of "infinite"); one killed: '
        'BR_Round_Winner, Round_End, game_round_end; next round: game_round_2; no failed or mission hooks', doc=DOC,
        size=(512, 512, 256))
def br_rounds(t):
    for n in HOOKS:
        t.point('info_target', targetname='=' + n)
    for name in ('br1', 'br2'):
        t.console('test_player add %s' % name)
        t.wait_frames(2)
    t.console('test_cmd player1 menuselect 1')     # the go menu: ready (two ready players start a round)
    t.console('test_cmd player2 menuselect 1')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.duration += 40
    t.wait(2.5)                                     # spawn protection
    t.mark('kill')
    t.damage('=player2', 5000)
    t.wait(1.0)
    t.console('wait_for alive player2 20')          # the next round respawns both
    t.duration += 20
    t.wait(1.0)
    t.expect.fired('=game_round_1', activator='world', count=1)
    t.expect.order('=game_round_start', '=game_round_1')
    t.expect.fired('=game_round_end', activator='world', count=1, after='kill', within=(0, 0.5))
    t.expect.fired('=game_round_2', activator='world', count=1, after='kill')
    t.expect.not_fired('=game_round_3')
    t.expect.not_fired('=game_round_failed')
    t.expect.not_fired('=game_mission_success')
    t.expect.not_fired('=game_mission_failed')
    t.expect.console('World triggered "Round_Start" (round "1" of "infinite")')
    t.expect.console('World triggered "Round_Start" (round "2" of "infinite")')
    t.expect.console('World triggered "BR_Round_Winner" (round "1", winner "br1"')
    t.expect.console('World triggered "Round_End" (round "1" of "infinite")')
