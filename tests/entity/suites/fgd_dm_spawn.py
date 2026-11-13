"""Tier 5, final pass: "master" and "target" on info_player_deathmatch, the first spawn zone (subs.cpp CBaseDMStart:
a spot whose master is locked is not used, EntSelectSpawnPoint; the spot a player spawns at fires its target with
him as activator, CHalfLifeMultiplay::GetPlayerSpawnSpot). The lobby's spawn spots are info_player_deathmatch too:
the test locks them all with the same master, then opens only its own spot without a lock, so the round restart
has to use it (a multisource with no input counts as on, so a never-fired trigger_relay is its input). A named
info_player_deathmatch belongs to its spawn set only (multispawn "Spawn set"), so the test
clears its two spots' names before the round and finds the open spot by an info_target at the same place. Its own
suite: it changes the map's spawn spots (and puts them back)."""

from etest import Suite

SUITE = Suite('fgd_dm_spawn', 'Spawn spots info_player_deathmatch: master and target', players=1)
S = SUITE
C = 'info_player_deathmatch'


@S.test('%s: the player spawns at the spot with the target (it fires with him as activator), never at a spot whose '
        'master is locked' % C, players=1, covers=(C + '.master', C + '.target'))
def dm_spawn(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')   # never fired: the lock stays off
    t.point(C, targetname='locked', at=(-120, 0, 0), angles='0 90 0', master='lock', target='lfired',
            names=('master',))
    t.point(C, targetname='open', at=(120, 0, 0), angles='0 90 0', target='ofired')
    t.point('info_target', targetname='lfired')
    t.point('info_target', targetname='ofired')
    t.point('info_target', targetname='openpos', at=(120, 0, 0))   # where the open spot is
    t.wait(0.2)
    t.kv('@' + C, 'master', t.name('lock'))     # every spot, the lobby's too, behind the locked master
    t.kv('open', 'master', '')                  # an empty master: no lock
    t.kv('open', 'targetname', '')              # unnamed: in the first spawn zone, not in a spawn set
    t.kv('locked', 'targetname', '')
    t.mark('round')
    t.console('test_round restart')
    t.wait_for_round(3, 15)
    t.wait(0.3)
    t.snap('s', 'player1', 'origin')
    t.snap('s', 'openpos', 'origin')
    t.kv('@' + C, 'master', '')                 # the spots as they were
    t.expect.distance('s', 'player1', 'openpos', 0, 2)
    t.expect.used('ofired', activator='player1', min=1, after='round')
    t.expect.not_used('lfired')
