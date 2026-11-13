"""max_round nbmaxround 1 (maxround.cpp uses 1 and 2 as written; 1.38 turned them into 5): with mp_easymode 0 the first
lost round fails the mission. Its own suite: the limit is the map's, a mission ends once per map."""

from etest import Suite

SUITE = Suite('round_limit_one', 'max_round nbmaxround 1: the first lost round fails the mission (mp_easymode 0)')
S = SUITE
S.server_args = ('+log', 'on')


@S.test('max_round nbmaxround 1: round 1 of 2 with mp_easymode 1 (the default); with mp_easymode 0 the first lost '
        'round is FAILED MISSION (Failed_Mission), no FAILED ROUND', players=1,
        covers=('max_round', 'max_round.nbmaxround'), size=(512, 512, 256))
def one_round(t):
    t.point('max_round', nbmaxround='1')
    t.console('mp_easymode 0')    # CoopRoundLimit reads it every time: round 1's start line was written with 1 (2)
    t.wait(2.3)                   # a player is protected for 2 s after he spawns
    t.mark('lost1')
    t.damage('player1', 5000)
    t.wait_for_round(5, 3)
    t.wait(0.3)
    t.expect.console('World triggered "Round_Start" (round "1" of "2")')
    t.expect.console('World triggered "Round_End" (round "1" of "1")')
    t.expect.no_console('(round "2" of')
    t.expect.msg('TextMsg', fields={'text': 'FAILED MISSION\n'}, count=1, after='lost1')
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=0, after='lost1')
    t.expect.console('World triggered "Failed_Mission"')
