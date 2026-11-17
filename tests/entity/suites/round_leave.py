"""A player who leaves a co-op round in play (docs/ROUNDS.md "Leaving the round", CCoop::PlayerLeavesRound): spectator
mode, Game Master mode and a disconnect all take the player out of the round's count and out of mp_hardcore's base
(the players alive at the round start), so leaving is no death. mp_hardcore 50: the round is lost when the players
alive drop to base - 50 % of base (rounded down). With 5 at the start and one gone, the base is 4: one death leaves
3 (goes on), a second leaves 2 (FAILED ROUND). Its own suite: it loses rounds; the disconnect tests come last."""

from etest import Suite

SUITE = Suite('round_leave', 'Leaving a co-op round: spectator, Game Master, disconnect', players=5,
              requires=('GmMode_RoundStart',))
S = SUITE
DOC = 'ROUNDS.md'
FAILED = {'text': 'FAILED ROUND\n'}


def _leave_then_two_deaths(t, leave):
    t.console('mp_hardcore 50')
    t.wait(2.5)                   # a player is protected for 2 s after he spawns
    leave(t)
    t.wait(0.5)
    t.mark('left')
    t.damage('player1', 5000)
    t.wait(1.0)
    t.mark('second')
    t.damage('player2', 5000)
    t.wait_for_round(0, 2)        # lost: the pause between rounds
    t.expect.msg('TextMsg', fields=FAILED, count=0, after='left', within=(0, 0.9))
    t.expect.msg('TextMsg', fields=FAILED, count=1, after='second', within=(0, 0.5))


@S.test('mp_hardcore 50, 5 players: one goes to spectator mode, one dies: the round goes on; a second death: '
        'FAILED ROUND', players=5, doc=DOC, size=(512, 512, 256))
def spectate(t):
    _leave_then_two_deaths(t, lambda t: t.cmd('player5', 'spectate'))
    t.wait(3.5)                   # the spectate toggle's own delay
    t.cmd('player5', 'spectate')  # back: joins the next round
    t.wait_for_round(3, 25)


@S.test('mp_hardcore 50, 5 players: one becomes Game Master, one dies: the round goes on; a second death: '
        'FAILED ROUND', players=5, doc=DOC, size=(512, 512, 256))
def game_master(t):
    t.console('mp_gmmode 1')

    def gm(t):
        t.cmd('player5', 'gmon')
        t.wait(0.3)
        t.cmd('player5', 'gm')    # the intro closed

    _leave_then_two_deaths(t, gm)
    t.snap('gm', 'player5', 'game_master')
    t.cmd('player5', 'gm_leave')  # a dead player again: the next round spawns him
    t.wait_for_round(3, 25)
    t.expect.value('gm', 'player5', 'game_master', 1)


@S.test('mp_hardcore 50, 5 players: one disconnects, one dies: the round goes on; a second death: FAILED ROUND',
        players=5, doc=DOC, size=(512, 512, 256))
def disconnect(t):
    _leave_then_two_deaths(t, lambda t: t.console('test_player remove player5'))
    t.wait_for_round(3, 25)


@S.test('mp_hardcore 50, 4 players: one dies, then he and one more disconnect: the round goes on (his death no '
        'longer counts, nobody else died); then one of the 2 left dies: FAILED ROUND', players=4, doc=DOC,
        size=(512, 512, 256))
def dead_and_alive_disconnect(t):
    t.console('mp_hardcore 50')
    t.wait(2.5)
    t.mark('dead')
    t.damage('player4', 5000)
    t.wait(1.0)
    t.console('test_player remove player4')
    t.wait(0.3)
    t.console('test_player remove player3')
    t.wait(1.0)
    t.mark('last')
    t.damage('player1', 5000)
    t.wait_for_round(0, 2)
    t.expect.msg('TextMsg', fields=FAILED, count=0, after='dead', within=(0, 2.0))
    t.expect.msg('TextMsg', fields=FAILED, count=1, after='last', within=(0, 0.5))
