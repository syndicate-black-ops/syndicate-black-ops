"""Tier 5, final pass: the spawn spots' "master" and "target" on info_player_go2 .. go5 (subs.cpp CBaseDMStart:
a spot whose master is locked is not used, EntSelectSpawnPoint; the spot a player spawns at fires its target with
him as activator, CHalfLifeMultiplay::GetPlayerSpawnSpot). Each zone has a spot behind a locked master and a spot
with a target; multispawn moves the players on one zone each round. Its own suite: the spawn zone is the map's, the
tests build on each other, in order."""

from etest import Suite

SUITE = Suite('fgd_spawn_keys', 'Spawn spots go2-go5: master and target', players=1)
S = SUITE
ZONES = ('info_player_go2', 'info_player_go3', 'info_player_go4', 'info_player_go5')


def restart(t):
    t.console('test_round restart')
    t.wait_for_round(3, 15)
    t.wait(0.3)


def zone_case(i, cls):
    @S.test('%s: after multispawn moved the spawns here, the player spawns at the spot with the target (it fires '
            'with him as activator), never at the one whose master is locked' % cls, players=1,
            covers=('%s.master' % cls, '%s.target' % cls))
    def case(t):
        if i == 0:
            t.point('multispawn', targetname='=et_ms')
            t.point('multisource', targetname='=et_lock')
            t.point('trigger_relay', targetname='=et_key', target='=et_lock', triggerstate='1')   # never fired
        t.point(cls, targetname='locked', at=(-120, 0, 0), angles='0 90 0', master='=et_lock', target='lfired',
                names=('master',))
        t.point(cls, targetname='open', at=(120, 0, 0), angles='0 90 0', target='ofired')
        t.point('info_target', targetname='lfired')
        t.point('info_target', targetname='ofired')
        t.fire('=et_ms', activator='player1')
        t.mark('round')
        restart(t)
        t.snap('s', 'player1', 'origin')
        t.snap('s', 'open', 'origin')
        t.expect.distance('s', 'player1', 'open', 0, 2)
        t.expect.used('ofired', activator='player1', min=1, after='round')
        t.expect.not_used('lfired')
    case.__name__ = 'zone_' + cls.replace('info_player_', '')
    return case


for _i, _c in enumerate(ZONES):
    zone_case(_i, _c)
