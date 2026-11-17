"""The cloak in Bounty Killer (CBasePlayer::CloakUpdate, player.cpp): the render amount follows what the player does,
as in co-op, but never drops below 50, so a still Assassin stays a faint shape to the other players. The map is
bk_et_cloak_bk (Bounty Killer game rules); suite cloak: co-op."""

from etest import Suite

SUITE = Suite('cloak_bk', 'Bounty Killer: a still, cloaked player is drawn at render amount 50 (co-op: 30)')
S = SUITE
S.map_prefix = 'bk_'
DOC = 'SBO-2.0-CHANGES.md'
RENDER_TRANSALPHA = 4


@S.test('a cloaked player standing still in Bounty Killer: render mode 4, amount 50', players=1, doc=DOC)
def bk_still(t):
    t.cmd('player1', 'cyberware', '5')     # cyberware 5: the cloak
    t.console('test_spawn player1')
    t.console('wait_for alive player1 20')
    t.wait(1.5)
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.wait(0.5)
    t.cmd('player1', 'cloak')
    t.wait(1.0)
    t.snap('on', 'player1', 'rendermode', 'renderamt')
    t.expect.value('on', 'player1', 'rendermode', RENDER_TRANSALPHA)
    t.expect.value('on', 'player1', 'renderamt', 50)
