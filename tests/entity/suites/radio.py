"""Radio menu (coop.cpp CCoop::ClientCommand, "radio" + "menuselect"): one radio call is one sound, whatever the
player count, as production's syndicate_radio_commands plugin played it (playtest 2026-10-17: the game emitted it
once per connected player, so N players heard N copies). The chat line goes to every player."""

from etest import Suite

SUITE = Suite('radio', 'Radio: one sound per call', players=3)
S = SUITE


@S.test('3 players, one radio call: one radio sound, three chat lines', players=3)
def one_sound(t):
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(0.5)
    t.mark('radio')
    t.cmd('player1', 'radio')
    t.wait(0.2)
    t.cmd('player1', 'menuselect', '1')
    t.wait(0.5)
    t.expect.sound('radio/', count=1, after='radio')
    t.expect.msg('SayText', count=3, after='radio')
