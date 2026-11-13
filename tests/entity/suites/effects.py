"""Hit effects with a stored sprite or model index (fix/stale-effect-index): the index a temp entity carries is the
one precached on the current map. weapon_sw's zoomed shots and dark_grunt's bullets (BULLET_PLAYER_SW2) send
TE_GLOWSPRITE / TE_SPRITETRAIL with sprites/xflare1.spr (the same two globals; dark_grunt fires them only with 9 or
more players in the game, so the player's weapon is the one tested); monster_cyberminotor ejects models/shell.mdl (TE_MODEL).
The last test changes the map: it must stay last (the suite's map is gone after it)."""

from etest import Suite

SUITE = Suite('effects', 'Hit effects: temp entity sprite and model indexes of the current map')
S = SUITE
XFLARE = 'model:sprites/xflare1.spr'    # its index on the map the message was sent on (check.py)
SHELL = 'model:models/shell.mdl'
TE_SPRITETRAIL, TE_GLOWSPRITE, TE_MODEL = 15, 23, 106
SECOND_MAP = 'co_carnage'               # a stock map: another precache order than the suite's map


def tough(t, player='player1'):
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def sw_ready(t, player='player1', at=(0, -150, 0)):
    """`player` picks up a weapon_sw where it stands and selects it."""
    t.point('weapon_sw', at=at)
    t.teleport(player, at, yaw=90)
    tough(t, player)
    t.wait(0.5)
    t.cmd(player, 'weapon_sw')
    t.wait(1.5)                 # deploy


def sw_hits(t, after):
    """The zoomed shot's two hit effects carry this map's xflare1 index, and none carries 0."""
    t.expect.msg('svc:23', fields={'te': TE_GLOWSPRITE, 'model': XFLARE}, after=after)
    t.expect.msg('svc:23', fields={'te': TE_SPRITETRAIL, 'model': XFLARE}, after=after)
    t.expect.msg('svc:23', fields={'te': TE_GLOWSPRITE, 'model': 0}, count=0, min=0, after=after)
    t.expect.msg('svc:23', fields={'te': TE_SPRITETRAIL, 'model': 0}, count=0, min=0, after=after)


@S.test('weapon_sw zoomed, before any plasma ball on the map: the hit sends this map\'s xflare1 (glow + trail)',
        players=1)
def sw_zoomed_fresh_map(t):
    sw_ready(t)
    t.look('player1', 0, 90)    # the room's north wall
    t.move('player1', 0, 0, 3, '+attack2')      # zoom (CSw::SecondaryAttack: state 2 -> 30 degrees, m_fInZoom)
    t.wait(0.5)
    t.mark('fire')
    t.move('player1', 0, 0, 3, '+attack')       # zoomed: BULLET_PLAYER_SW2, no plasma ball
    t.wait(0.5)
    t.expect.spawned('@plasma', count=0, min=0)
    sw_hits(t, 'fire')


@S.test('monster_cyberminotor ejects shell casings: TE_MODEL with models/shell.mdl, never index 0', players=1,
        size=(512, 512, 512), covers=('monster_cyberminotor',))
def minos_shells(t):
    # its gun is the model's muzzle, about 200 units ahead of it (CMinotaure::GetGunPosition): it stands in the
    # room's middle facing south, the player beyond its melee range (200) to the south
    t.point('monster_cyberminotor', at=(0, -10, 0), targetname='minos', angles='0 270 0')
    tough(t)
    t.teleport('player1', (0, -230, 0), yaw=90)
    t.mark('seen')
    t.wait(10.0)
    t.expect.damaged('player1', attacker='minos', after='seen')
    t.expect.msg('svc:23', fields={'te': TE_MODEL, 'model': SHELL}, after='seen')
    t.expect.msg('svc:23', fields={'te': TE_MODEL, 'model': 0}, count=0, min=0, after='seen')


@S.test('weapon_sw zoomed after a plasma hit and a changelevel: the new map\'s xflare1, not the last map\'s',
        players=1)
def sw_zoomed_after_changelevel(t):
    sw_ready(t)
    t.cmd('player1', 'weapon_fa18')     # away and back: CSw::Deploy ends the zoom of the first test
    t.wait(1.0)
    t.cmd('player1', 'weapon_sw')
    t.wait(1.5)
    t.look('player1', 0, 90)
    t.mark('plasma')
    t.move('player1', 0, 0, 3, '+attack')       # unzoomed: a plasma ball, CPlasma::Touch on the north wall
    t.wait(1.0)
    t.expect.spawned('@plasma', min=1)
    t.expect.msg('svc:23', fields={'te': TE_GLOWSPRITE, 'model': XFLARE}, after='plasma')
    # another map, whose model table starts with three models more (as a server plugin's precaches do): its
    # xflare1 index is not this map's. The test player is dropped (fake clients are) and added again there
    t.console('test_precache_first models/hgrunt.mdl models/osprey.mdl sprites/xspark1.spr')
    t.console('changelevel %s' % SECOND_MAP)
    t.wait_frames(20)
    t.console('test_player add et1')
    t.wait_frames(2)
    t.console('test_join player1 1 1 3')       # heavy weapon 3: weapon_sw
    t.wait(0.5)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.flags('player1', '+FL_NOTARGET')          # the map's monsters leave it alone
    tough(t)
    t.cmd('player1', 'weapon_sw')
    t.wait(1.5)
    t.look('player1', 89, 0)    # the floor at its feet
    t.move('player1', 0, 0, 3, '+attack2')
    t.wait(0.5)
    t.mark('fire')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.5)
    sw_hits(t, 'fire')
