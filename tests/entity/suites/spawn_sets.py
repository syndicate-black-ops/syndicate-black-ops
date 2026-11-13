"""multispawn "Spawn set" (spawnset): named spawn sets (maprules.cpp CMultiSpawn, player.cpp EntSelectSpawnPoint). A
multispawn with a Spawn set makes the Player Start 1 to 5 points with that name the active set, in any order; an empty
key moves the go2..go5 rotation on and leaves the named set; a name with no spawn points falls back to the rotation
with a developer message; a named Player Start 1 is used only by its set while the rotation is on Player Start 1;
trigger_changevalue changes the key while the map runs. The active set lasts through round restarts, as the rotation
does (both reset at map start, in the game rules). Its own suite: the spawn set is the map's, the tests build on each
other, in order."""

from etest import Suite

SUITE = Suite('spawn_sets', 'multispawn Spawn set: named spawn sets, any order, fallback, run-time changes', players=1)
SUITE.server_args = ('+developer', '1')     # the missing-set message is a developer message
S = SUITE


def restart(t):
    t.console('test_round restart')
    t.wait_for_round(3, 15)
    t.wait(0.3)


def at(t, label, spot):
    """the player spawned at spot in the round just started"""
    t.snap(label, 'player1', 'origin')
    t.snap(label, spot, 'origin')
    t.expect.distance(label, 'player1', spot, 0, 2)


@S.test('Player Start 1: while no set is active, a named info_player_deathmatch is not used by the rotation (it '
        'belongs to its set): with every unnamed spot locked, the round still does not spawn the player at the '
        'named one', players=1, covers=('info_player_deathmatch.targetname',))
def dm_named_skipped(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')   # never fired: the lock stays off
    t.point('info_player_deathmatch', targetname='solo', at=(0, 100, 0), angles='0 90 0')
    t.wait(0.2)
    t.kv('@info_player_deathmatch', 'master', t.name('lock'))     # every spot, the lobby's too, locked
    t.kv('solo', 'master', '')                                   # only the named one open
    restart(t)
    t.snap('s', 'player1', 'origin')
    t.snap('s', 'solo', 'origin')
    t.kv('@info_player_deathmatch', 'master', '')                # the spots as they were
    t.expect.distance('s', 'player1', 'solo', 300, 100000)


@S.test('multispawn with an empty Spawn set: the rotation moves on to Player Start 2 as before; a named '
        'info_player_go2 is still used in its zone', players=1,
        covers=('multispawn', 'info_player_go2.targetname'))
def rotation(t):
    t.point('multispawn', targetname='=ms_rot')
    t.point('info_player_go2', targetname='=g2', at=(0, 100, 0), angles='0 90 0')
    t.fire('=ms_rot', activator='player1')
    restart(t)
    at(t, 'r', '=g2')


@S.test('multispawn with a Spawn set: the round spawns the player at the Player Start 1 named so (rotation on '
        'zone 2)', players=1, covers=('multispawn.spawnset',))
def named_set(t):
    t.point('multispawn', targetname='=ms_a', spawnset='=setA')
    t.point('info_player_deathmatch', targetname='=setA', at=(0, 100, 0), angles='0 90 0')
    t.fire('=ms_a', activator='player1')
    restart(t)
    at(t, 'a', '=setA')


@S.test('Spawn sets switch in any order: set B (a Player Start 4), back to set A, B again; a round restart keeps '
        'the active set', players=1, covers=('info_player_go4.targetname',))
def switch(t):
    t.point('multispawn', targetname='=ms_b', spawnset='=setB')
    t.point('info_player_go4', targetname='=setB', at=(0, 100, 0), angles='0 90 0')
    t.fire('=ms_b', activator='player1')
    restart(t)
    at(t, 'b1', '=setB')
    t.fire('=ms_a', activator='player1')
    restart(t)
    at(t, 'a1', '=setA')
    t.fire('=ms_b', activator='player1')
    restart(t)
    at(t, 'b2', '=setB')
    restart(t)                                                  # no multispawn: the same set
    at(t, 'b3', '=setB')


@S.test('A Spawn set no spawn point has: the round uses the rotation (Player Start 2) and a developer message names '
        'the set', players=1)
def missing_set(t):
    t.point('multispawn', targetname='=ms_none', spawnset='=nosuchset')
    t.fire('=ms_none', activator='player1')
    restart(t)
    at(t, 'm', '=g2')
    t.expect.console('no spawn point is named "nosuchset"')


@S.test('trigger_changevalue sets Spawn set while the map runs: the multispawn then makes set C (a Player Start 5) '
        'active; set to empty, it moves the rotation on to Player Start 3 and no named set is used',
        players=1, covers=('info_player_go5.targetname', 'info_player_go3.targetname'))
def changevalue(t):
    t.point('multispawn', targetname='=ms_cv', spawnset='=setA')
    t.point('info_player_go5', targetname='=setC', at=(-100, 100, 0), angles='0 90 0')
    t.point('info_player_go3', targetname='=g3', at=(100, 100, 0), angles='0 90 0')
    t.point('trigger_changevalue', targetname='cv_c', target='=ms_cv', m_iszValueName='spawnset',
            m_iszNewValue='setC', m_iszValueType='0')
    t.point('trigger_changevalue', targetname='cv_empty', target='=ms_cv', m_iszValueName='spawnset',
            m_iszValueType='0')
    t.fire('cv_c')
    t.wait(0.1)
    t.fire('=ms_cv', activator='player1')
    restart(t)
    at(t, 'c', '=setC')
    t.fire('cv_empty')
    t.wait(0.1)
    t.fire('=ms_cv', activator='player1')
    restart(t)
    at(t, 'e', '=g3')
