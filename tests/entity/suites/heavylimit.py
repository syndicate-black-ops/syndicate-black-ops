"""Co-op heavy weapon team limits for the new loadout menu (docs/LOADOUT_MENU.md "HeavyLimit"): coop.cpp
CanTakeHeavyWeapon allows 3 grenade launchers (heavy 7) and 2 nuclear rocket launchers (heavy 9) on the team, and
HeavyLimit_Update tells every player how many of them the OTHER players hold (byte grenade launchers, byte 3, byte
nuclear launchers, byte 2), when that changes. A full one is refused with "3 grenade launchers max / team" and the
loadout stays as it was; a place freed by another pick reaches the others within the 0.25 s check."""

from etest import Suite

SUITE = Suite('heavylimit', 'Co-op heavy weapon team limits and the HeavyLimit message', players=4)
S = SUITE
DOC = 'LOADOUT_MENU.md'


@S.test('three grenade launchers taken: the 4th player is told full and refused; a freed one is sent at once',
        players=4, doc=DOC)
def glauncher_full(t):
    for p in ('player2', 'player3', 'player4'):
        t.cmd(p, 'heavy', '7')
    t.wait(1.0)
    t.mark('full')
    # player1 sees all three taken (the counts without player1); player2 sees the two others
    t.expect.msg('HeavyLimit', to='player1', fields={'gl': 3, 'gl_max': 3, 'nuke': 0, 'nuke_max': 2})
    t.expect.msg('HeavyLimit', to='player2', fields={'gl': 2, 'gl_max': 3})
    t.cmd('player1', 'heavy', '7')
    t.wait(0.5)
    t.expect.msg('TextMsg', to='player1', fields={'text': '3 grenade launchers max / team'}, after='full')
    # refused: player1 holds none, so nothing changed for the others
    t.expect.no_msg('HeavyLimit', to='player2', after='full', within=(0, 0.5))
    t.mark('freed')
    t.cmd('player4', 'heavy', '1')
    t.wait(1.0)
    t.expect.msg('HeavyLimit', to='player1', fields={'gl': 2, 'gl_max': 3}, after='freed', within=(0, 0.6))
    t.cmd('player1', 'heavy', '7')     # now allowed: player1 takes the freed place
    t.wait(1.0)
    t.expect.msg('HeavyLimit', to='player4', fields={'gl': 3, 'gl_max': 3}, after='freed')
    for p in ('player1', 'player2', 'player3', 'player4'):
        t.cmd(p, 'heavy', '1')


@S.test('two nuclear launchers taken: full for the others, never for a holder', players=4, doc=DOC)
def nuke_full(t):
    t.cmd('player2', 'heavy', '9')
    t.cmd('player3', 'heavy', '9')
    t.wait(1.0)
    t.expect.msg('HeavyLimit', to='player1', fields={'nuke': 2, 'nuke_max': 2})
    t.expect.msg('HeavyLimit', to='player2', fields={'nuke': 1, 'nuke_max': 2})
    for p in ('player2', 'player3'):
        t.cmd(p, 'heavy', '1')
