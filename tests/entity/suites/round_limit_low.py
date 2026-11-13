"""max_round with nbmaxround 1 or 2 (GAPS.md tier 1 "max_round"): maxround.cpp uses the value as written (1.38 turned
2 or less into 5, the default; no map sets less than 4). Only 0 or less gives 5. With mp_easymode 1 (the default)
players get 1.5 times that (CoopRoundLimit). Its own suite: the limit is the map's."""

from etest import Suite

SUITE = Suite('round_limit_low', 'max_round nbmaxround 2: the limit is 2 (x 1.5 in mp_easymode 1: 3)')
S = SUITE
S.server_args = ('+log', 'on')


@S.test('max_round nbmaxround 2: the round limit is 2 (3 with mp_easymode 1, the default)', players=1,
        covers=('max_round.nbmaxround',))
def two(t):
    t.point('max_round', nbmaxround='2')
    t.wait(0.2)
    t.expect.console('World triggered "Round_Start" (round "1" of "3")')
