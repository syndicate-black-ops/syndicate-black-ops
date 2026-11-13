"""mp_hardcore through real co-op rounds (docs/ROUNDS.md test list item 11, CCoop::CheckRoundOver): a round is lost when
the players alive drop to (alive at the start) - mp_hardcore % of them. 100 (the default): when the last one dies;
50 with two players: at the first death. Its own suite: it loses rounds."""

from etest import Suite

SUITE = Suite('round_hardcore', 'mp_hardcore: the share of deaths that loses a co-op round', players=2)
S = SUITE
DOC = 'ROUNDS.md'


@S.test('mp_hardcore 100: one of two dies, the round goes on; the second dies: FAILED ROUND. mp_hardcore 50: the '
        'first of two dies: FAILED ROUND at once', players=2, doc=DOC, size=(512, 512, 256))
def hardcore(t):
    t.console('mp_hardcore 100')
    t.wait(2.5)                   # a player is protected for 2 s after he spawns
    t.mark('first')
    t.damage('player1', 5000)
    t.wait(1.0)
    t.mark('second')
    t.damage('player2', 5000)
    t.wait_for_round(0, 2)        # lost: the pause between rounds
    t.wait_for_round(3, 20)       # the next round in play
    t.console('mp_hardcore 50')
    t.wait(2.5)
    t.mark('half')
    t.damage('player1', 5000)
    t.wait_for_round(0, 2)
    t.wait(0.3)
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=0, after='first', within=(0, 0.9))
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=1, after='second', within=(0, 0.5))
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=1, after='half', within=(0, 0.5))
