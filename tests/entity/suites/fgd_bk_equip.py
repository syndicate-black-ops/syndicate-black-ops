"""game_player_equip in Bounty Killer: without Use Only every player gets its items at each spawn, as in the other
modes, instead of the Wong (dm.cpp CDm::PlayerSpawn); with Use Only only the player who fires it."""

from etest import Suite

SUITE = Suite('fgd_bk_equip', 'Bounty Killer: game_player_equip at spawn', players=2)
S = SUITE
S.map_prefix = 'bk_'
DOC = 'FGD game_player_equip'


def has(t, name, weapon):
    t.point('player_hasweapon', targetname=name, weapon_name=weapon, pass_target=name + '_yes',
            fail_target=name + '_no')
    t.point('info_target', targetname=name + '_yes')
    t.point('info_target', targetname=name + '_no')


@S.test('Bounty Killer: a game_player_equip without Use Only gives its sword to each player at spawn, and no Wong; '
        'one with Use Only (its pm66) gives nothing at spawn', players=2, doc=DOC, size=(512, 512, 256),
        covers=('game_player_equip:1',))
def bk_equip_spawn(t):
    t.point('game_player_equip', targetname='eq', weapon_sword='1')
    t.point('game_player_equip', targetname='eq_use', spawnflags='1', weapon_pm66='1')
    for p in ('1', '2'):
        has(t, 'sw' + p, 'weapon_sword')
        has(t, 'pm' + p, 'weapon_pm66')
        has(t, 'wo' + p, 'weapon_wong')
    t.wait(1.5)
    t.mark('check')
    for p in ('1', '2'):
        for w in ('sw', 'pm', 'wo'):
            t.fire(w + p, activator='player' + p)
    t.wait(0.3)
    for p in ('1', '2'):
        t.expect.fired('sw%s_yes' % p, count=1, after='check')
        t.expect.fired('pm%s_no' % p, count=1, after='check')
        t.expect.fired('wo%s_no' % p, count=1, after='check')
