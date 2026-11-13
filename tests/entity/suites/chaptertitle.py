"""worldspawn "chaptertitle" (Chapter Title Message): the title goes to each player once per map, when the player first
enters the game (spawned in play, spectating or Game Master; CBasePlayer::UpdateClientData), players who join later
too; not again on a respawn or a new round. Half-Life and 1.38 showed it to player 1 only (an env_message with no
activator 0.3 s into the map). The suite's map sets the key; its players join inside the test, so the messages fall
in the test's window."""

from etest import Suite

SUITE = Suite('chaptertitle', 'worldspawn chaptertitle: every player once, late joiners too', players=0)
SUITE.world_keys = {'chaptertitle': 'ET_CHAPTER'}
S = SUITE
TITLE = {'text': 'ET_CHAPTER'}


def join(t, k):
    t.console('test_player add ct%d' % k)
    t.wait_frames(2)
    t.console('test_join player%d' % k)


def wait_alive(t, *players):
    for k in players:
        t.console('wait_for alive player%d 20' % k)
        t.duration += 20


@S.test('two players who join get the title once each; a player who joins during the round gets it when '
        'the next round spawns him; a new round sends it to nobody again', covers=('worldspawn', 'worldspawn.chaptertitle'))
def all_players(t):
    t.mark('join')
    join(t, 1)
    join(t, 2)
    t.wait(1.0)
    t.console('test_round start')
    wait_alive(t, 1, 2)
    t.wait(0.5)
    t.mark('late')
    join(t, 3)
    t.wait(1.0)
    t.mark('restart')
    t.console('test_round restart')
    wait_alive(t, 1, 2, 3)
    t.wait(1.0)
    t.expect.msg('HudText', to='=player1', count=1, fields=TITLE, after='join')
    t.expect.msg('HudText', to='=player2', count=1, fields=TITLE, after='join')
    t.expect.msg('HudText', to='=player3', count=1, fields=TITLE, after='late')
    t.expect.msg('HudText', to='=player1', count=0, fields=TITLE, after='restart')
    t.expect.msg('HudText', to='=player2', count=0, fields=TITLE, after='restart')
