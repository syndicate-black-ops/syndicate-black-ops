"""Model scale: scaled collision boxes, melee reach and follow range edge to edge (branch feat/model-scale-bounds,
docs/MODEL_SCALE.md "Building and testing" 4, 5b-5e). Runs only with an hl.so that has the branch's code."""

from etest import Suite

SUITE = Suite('model_scale', 'Model scale: boxes, reach, follow (feat/model-scale-bounds)',
              requires=('UTIL_ScaledHalfWidthExtra',))
S = SUITE
DOC = 'MODEL_SCALE.md#building-and-testing'
TALL = (512, 512, 400)


def tough(t, player='player1'):
    """A test player that survives a few claws (a dead last player would end the co-op round)."""
    t.kv(player, 'health', '1000')
    t.kv(player, 'max_health', '1000')


def zombie_claws_player(t, scale):
    t.point('monster_zombie', at=(0, 160, 0), targetname='zom', angles='0 270 0', scale=scale)
    t.teleport('player1', (0, -60, 0), yaw=90)
    tough(t)
    t.wait(8)
    t.snap('end', 'zom', 'origin')
    t.snap('end', 'player1', 'origin')
    t.expect.damaged('player1', attacker='@monster_zombie', min=1)
    # it stops at its scaled box: never inside the player (half widths 16 x scale + 16)
    t.expect.distance('end', 'zom', 'player1', 16 * float(scale) + 16 - 1, 1000)


@S.test('2x zombie: its claws reach and hurt a test player', players=1, doc=DOC, size=TALL)
def zombie2_player(t):
    zombie_claws_player(t, '2')


@S.test('4x zombie: its claws still reach and hurt a test player', players=1, doc=DOC, size=TALL)
def zombie4_player(t):
    zombie_claws_player(t, '4')


@S.test('1x zombie (control): its claws hurt a test player', players=1, doc=DOC)
def zombie1_player(t):
    zombie_claws_player(t, '1')


def notarget(t, player='player1'):
    """Monsters ignore the player (FL_NOTARGET), who stays in the room: monsters only think with a player in view."""
    t.flags(player, '+FL_NOTARGET')


def zombie_vs_scientist(t, zscale, sscale):
    t.point('monster_zombie', at=(0, 200, 0), targetname='zom', angles='0 270 0', scale=zscale)
    t.point('monster_scientist', at=(0, 20, 0), targetname='sci', angles='0 90 0', scale=sscale)
    t.teleport('player1', (-200, -200, 0), yaw=45)
    tough(t)
    notarget(t)
    t.wait(10)
    t.snap('end', 'sci', 'origin', 'health', 'deadflag', 'maxs')
    t.snap('end', 'zom', 'origin')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.damaged('sci', attacker='@monster_zombie', min=1)
    t.expect.not_damaged('player1')


@S.test('2x zombie vs 3x scientist: the claws reach the scientist edge to edge', players=1, doc=DOC, size=TALL)
def zombie2_scientist3(t):
    zombie_vs_scientist(t, '2', '3')


@S.test('1x zombie vs 1x scientist (control)', players=1, doc=DOC)
def zombie1_scientist1(t):
    zombie_vs_scientist(t, '1', '1')


def item_walk(t, scale, offset, picked):
    t.point('ammo_reserve', at=(0, 40, 0), targetname='box', scale=scale)
    t.teleport('player1', (offset, -60, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.mark('walk')
    t.walk('player1', 200, 0, 1.2)      # walks north over / past the box (about 200 units)
    t.wait(0.3)
    if picked:
        t.expect.sound('9mmclip1', ent='@ammo_reserve', min=1, after='walk')
    else:
        t.expect.sound('9mmclip1', ent='@ammo_reserve', min=0, count=0, after='walk')


@S.test('0.5x ammo_reserve: picked up when the player walks over it', players=1, doc=DOC)
def item_half_touch(t):
    item_walk(t, '0.5', 0, True)


# The engine links every box 1 unit larger on each side (SV_LinkEdict), so a player (16) touches an item whose half
# width is h while the centers are less than 16 + h + 2 apart: 26 for the 0.5x box (8), 34 for the 1x box (16).
@S.test('0.5x ammo_reserve: not picked up passing 28 units to the side (its box is 16 wide)', players=1, doc=DOC)
def item_half_miss(t):
    item_walk(t, '0.5', 28, False)


@S.test('1x ammo_reserve (control): picked up passing 28 units to the side (box 32 wide)', players=1, doc=DOC)
def item_one_side(t):
    item_walk(t, '1', 28, True)


def follow(t, scale, use_key):
    t.point('monster_scientist', at=(0, 150, 0), targetname='sci', angles='0 270 0', scale=scale)
    gap = 16 * float(scale) + 16 + 8
    t.teleport('player1', (0, 150 - gap, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.5)
    if use_key:
        t.move('player1', 0, 0, 5, '+use')
    else:
        t.fire('sci', activator='player1', caller='player1')     # FollowerUse wants a player as the caller
    t.wait(0.5)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(6)
    t.snap('end', 'sci', 'origin')
    t.snap('end', 'player1', 'origin')


@S.test('3x scientist: +use makes it follow (it comes to the player)', players=1, doc=DOC, size=TALL)
def sci3_use(t):
    follow(t, '3', True)
    t.expect.used('sci', activator='player1', min=1)
    t.expect.distance('end', 'sci', 'player1', 0, 260)       # it came: it started 350 away


@S.test('3x scientist following: stops about 160 from the player (128 edge to edge)', players=1, doc=DOC, size=TALL)
def sci3_follow(t):
    follow(t, '3', False)
    t.expect.distance('end', 'sci', 'player1', 145, 200)


@S.test('1x scientist following (control): stops about 128 from the player', players=1, doc=DOC)
def sci1_follow(t):
    follow(t, '1', False)
    t.expect.distance('end', 'sci', 'player1', 100, 150)


def walk_into(t, scale):
    t.point('cycler', at=(0, 100, 0), targetname='box', model='models/zombie.mdl', scale=scale)
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.2)
    t.walk('player1', 300, 0, 2.0)
    t.wait(0.5)
    t.snap('end', 'box', 'origin')
    t.snap('end', 'player1', 'origin')
    half = 16 * float(scale)
    t.expect.distance('end', 'box', 'player1', half + 16 - 0.5, half + 16 + 2)


@S.test('2x cycler: a walking player stops at its 64-wide box (48 from its origin)', players=1, doc=DOC, size=TALL)
def box2(t):
    walk_into(t, '2')


@S.test('0.5x cycler: a walking player stops at its 16-wide box (24 from its origin)', players=1, doc=DOC)
def box_half(t):
    walk_into(t, '0.5')


@S.test('1x cycler (control): a walking player stops at its 32-wide box (32 from its origin)', players=1, doc=DOC)
def box1(t):
    walk_into(t, '1')


# ---------------------------------------------------------------------------------------------- env_render scale
# docs/ENV_RENDER.md "Scale and collision": env_render's m_fScale scales the box too (playtest 2026-10-17: a 3x
# grunt on co_sbo_showcase kept its normal-size box). Spawnflags 15: only the scale changes (FX, amount, mode, color
# masked). A cycler's box at scale 1 is (-16 -16 0) (16 16 72).
ER_DOC = 'ENV_RENDER.md#scale-and-collision'


@S.test('env_render m_fScale 3, no fade: the cycler\'s box is 3x at once', players=1, doc=ER_DOC, size=TALL)
def env_render_scale_now(t):
    t.point('cycler', at=(0, 100, 0), targetname='cyc', model='models/hgrunt.mdl')
    t.point('env_render', targetname='grow', target='cyc', spawnflags='15', m_fScale='3')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.2)
    t.snap('before', 'cyc', 'mins', 'maxs')
    t.fire('grow')
    t.wait(0.2)
    t.snap('after', 'cyc', 'scale', 'mins', 'maxs')
    t.expect.value('before', 'cyc', 'maxs', (16, 16, 72), tol=0.1)
    t.expect.value('after', 'cyc', 'scale', 3)
    t.expect.value('after', 'cyc', 'mins', (-48, -48, 0), tol=0.1)
    t.expect.value('after', 'cyc', 'maxs', (48, 48, 216), tol=0.1)


@S.test('env_render m_fScale 3 with Fade Time 2: the box grows with the drawn scale (2x half way), 3x at its end',
        players=1, doc=ER_DOC, size=TALL)
def env_render_scale_fade(t):
    t.point('cycler', at=(0, 100, 0), targetname='cyc', model='models/hgrunt.mdl')
    t.point('env_render', targetname='grow', target='cyc', spawnflags='15', m_fScale='3', frags='2')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.2)
    t.fire('grow')
    t.wait(1.0)
    t.snap('during', 'cyc', 'scale', 'maxs')
    t.wait(1.5)
    t.snap('after', 'cyc', 'maxs')
    t.expect.value('during', 'cyc', 'scale', 3)       # pev->scale holds the end scale from the start
    t.expect.value('during', 'cyc', 'maxs', (32, 32, 144), tol=20)  # scale 2 half way (frame times: +- a few units)
    t.expect.value('after', 'cyc', 'maxs', (48, 48, 216), tol=0.1)


@S.test('env_render m_fScale 0.5, no fade: the cycler\'s box is half size', players=1, doc=ER_DOC)
def env_render_scale_half(t):
    t.point('cycler', at=(0, 100, 0), targetname='cyc', model='models/hgrunt.mdl')
    t.point('env_render', targetname='shrink', target='cyc', spawnflags='15', m_fScale='0.5')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.2)
    t.fire('shrink')
    t.wait(0.2)
    t.snap('after', 'cyc', 'maxs')
    t.expect.value('after', 'cyc', 'maxs', (8, 8, 36), tol=0.1)
