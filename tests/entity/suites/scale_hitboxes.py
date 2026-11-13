"""Hitboxes that follow the scale (branch feat/scale-hitboxes, docs/MODEL_SCALE.md "Changing it while the map runs"
and "Head scale", docs/ENV_RENDER.md "Scale and collision").

- An env_render scale fade: the box and the hitboxes follow the scale the clients draw at each moment.
- The engine's hitbox cache (r_cachestudio) does not keep the old hitboxes after a scale change on the same frame.
- A player's "headscale": his head hitbox (hit group 1) grows from the neck; traces above his box reach it.

Each check is a bullet's trace (test_hitscan): a line through the model, given relative to its origin. The grunt
cycler (models/hgrunt.mdl, box (-16 -16 0) (16 16 72) at scale 1) stands with its origin on the floor; a test
player's origin is 36 above his feet."""

from etest import Suite

SUITE = Suite('scale_hitboxes', 'Hitboxes follow scale fades and head scale (feat/scale-hitboxes)',
              requires=('UTIL_StudioHitboxesChanged',))
S = SUITE
ER_DOC = 'ENV_RENDER.md#scale-and-collision'
HEAD_DOC = 'MODEL_SCALE.md#head-scale'
TALL = (512, 512, 400)
HEAD = 1


def across(t, label, ent, z, axis='x'):
    """A horizontal trace through `ent`'s origin axis, `z` above it, from -100 to +100 along world `axis`. The
    cycler faces +x: along y the line crosses both of its legs; a test player faces -y here."""
    if axis == 'x':
        t.hitscan(label, ent, (-100, 0, z), (100, 0, z))
    else:
        t.hitscan(label, ent, (0, -100, z), (0, 100, z))


def grunt(t):
    t.point('cycler', at=(0, 100, 0), targetname='cyc', model='models/hgrunt.mdl')
    t.teleport('player1', (0, -200, 0), yaw=90)


@S.test('env_render fade 1 -> 3 over 2 s: half way the box is about 2x and bullets hit at 2x height', players=1,
        doc=ER_DOC, size=TALL, covers=('env_render', 'env_render.m_fScale', 'env_render.frags'))
def fade_half_way(t):
    grunt(t)
    t.point('env_render', targetname='grow', target='cyc', spawnflags='15', m_fScale='3', frags='2')
    t.wait(0.3)
    across(t, 'before', 'cyc', 110, 'y')
    t.fire('grow')
    t.wait(1.0)
    t.snap('half', 'cyc', 'mins', 'maxs')
    across(t, 'half_low', 'cyc', 110, 'y')       # inside a 2x grunt, above a 1x grunt
    across(t, 'half_high', 'cyc', 180, 'y')      # above a 2x grunt (about 145 high), inside a 3x one
    t.wait(1.5)
    across(t, 'after_high', 'cyc', 180, 'y')
    t.snap('after', 'cyc', 'maxs')
    t.expect.value('before', 'cyc', 'hit', 0)
    t.expect.value('half', 'cyc', 'maxs.z', 144, tol=20)
    t.expect.value('half', 'cyc', 'maxs.x', 32, tol=5)
    t.expect.value('half_low', 'cyc', 'hit', 1)
    t.expect.value('half_high', 'cyc', 'hit', 0)
    t.expect.value('after_high', 'cyc', 'hit', 1)
    t.expect.value('after', 'cyc', 'maxs', (48, 48, 216), tol=0.1)


@S.test('env_render fade 3 -> 1 over 2 s: half way the box is about 2x', players=1, doc=ER_DOC, size=TALL,
        covers=('env_render', 'env_render.m_fScale', 'env_render.frags'))
def fade_shrink(t):
    t.point('cycler', at=(0, 100, 0), targetname='cyc', model='models/hgrunt.mdl', scale='3')
    t.point('env_render', targetname='shrink', target='cyc', spawnflags='15', m_fScale='1', frags='2')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.3)
    t.fire('shrink')
    t.wait(1.0)
    t.snap('half', 'cyc', 'scale', 'maxs')
    across(t, 'half_high', 'cyc', 180, 'y')      # above a 2x grunt
    across(t, 'half_low', 'cyc', 110, 'y')
    t.wait(1.5)
    t.snap('after', 'cyc', 'maxs')
    t.expect.value('half', 'cyc', 'scale', 1)           # pev->scale holds the end scale from the start
    t.expect.value('half', 'cyc', 'maxs.z', 144, tol=20)
    t.expect.value('half_high', 'cyc', 'hit', 0)
    t.expect.value('half_low', 'cyc', 'hit', 1)
    t.expect.value('after', 'cyc', 'maxs', (16, 16, 72), tol=0.1)


@S.test('Scale changed on the same frame: the next bullet meets the new hitboxes (engine hitbox cache)', players=1,
        doc='MODEL_SCALE.md#changing-it-while-the-map-runs', size=TALL, covers=('env_render', 'env_render.m_fScale'))
def cache_same_frame(t):
    grunt(t)
    t.point('env_render', targetname='big', target='cyc', spawnflags='15', m_fScale='3')
    t.point('env_render', targetname='normal', target='cyc', spawnflags='15', m_fScale='1')
    t.wait(0.3)
    # no wait between the steps: one server frame, the cycler's animation frame unchanged. A trace straight down
    # through his middle meets the top of his head: about 72 up at scale 1, 216 at scale 3
    t.hitscan('s1', 'cyc', (0, 0, 300), (0, 0, -10))
    t.fire('big')
    t.hitscan('s3', 'cyc', (0, 0, 300), (0, 0, -10))
    t.fire('normal')
    t.hitscan('s1b', 'cyc', (0, 0, 300), (0, 0, -10))
    t.expect.value('s1', 'cyc', 'hit', 1)
    t.expect.value('s1', 'cyc', 'fraction', 0.6, op='>')     # (300 - 72) / 310 = 0.74
    t.expect.value('s3', 'cyc', 'hit', 1)
    t.expect.value('s3', 'cyc', 'fraction', 0.4, op='<')     # (300 - 216) / 310 = 0.27
    t.expect.value('s1b', 'cyc', 'fraction', 0.6, op='>')


@S.test('headscale 2.5: a bullet above the player\'s box hits his head; at 1 it misses', players=2, doc=HEAD_DOC)
def head_big(t):
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.teleport('player2', (0, 100, 0), yaw=270)
    t.wait(0.5)
    across(t, 'normal_top', 'player2', 44)              # above his head (top about 39) and his box (37)
    t.kv('player2', 'headscale', '2.5')
    t.wait(0.2)
    across(t, 'big_top', 'player2', 44)
    t.snap('big', 'player2', 'absmax', 'maxs')
    t.kv('player2', 'headscale', '1')
    t.wait(0.2)
    across(t, 'back_top', 'player2', 44)
    t.snap('back', 'player2', 'absmax', 'maxs')
    t.expect.value('normal_top', 'player2', 'hit', 0)
    t.expect.value('big_top', 'player2', 'hit', 1)
    t.expect.value('big_top', 'player2', 'hitgroup', HEAD)
    t.expect.value('big', 'player2', 'maxs.z', 36, tol=0.1)      # his movement box stays
    t.expect.value('back_top', 'player2', 'hit', 0)
    t.expect.msg('HeadScale', min=2)


@S.test('headscale 0.5: the head hitbox is smaller (a trace near the normal head\'s top misses)', players=2,
        doc=HEAD_DOC)
def head_small(t):
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.teleport('player2', (0, 100, 0), yaw=270)
    t.wait(0.5)
    across(t, 'normal', 'player2', 36)              # near the top of a normal head
    t.kv('player2', 'headscale', '0.5')
    t.wait(0.2)
    across(t, 'small', 'player2', 36)
    t.expect.value('normal', 'player2', 'hitgroup', HEAD)
    t.expect.value('small', 'player2', 'hit', 0)


@S.test('headscale is for server plugins only: trigger_changevalue / trigger_copyvalue on !activator can\'t set it, '
        'a direct DispatchKeyValue (test_kv, as a plugin) can', players=2, doc=HEAD_DOC)
def head_plugin_only(t):
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.teleport('player2', (0, 100, 0), yaw=270)
    t.point('trigger_changevalue', targetname='cv', target='!activator', m_iszValueName='headscale',
            m_iszNewValue='2.5')
    t.point('info_target', targetname='src', **{'$f_big': '2.5'})
    t.point('trigger_copyvalue', targetname='copy', netname='src', m_iszSrcValueName='$f_big', target='!activator',
            m_iszDstValueName='headscale', names=('netname',))
    t.wait(0.5)
    t.fire('cv', activator='player2')
    t.fire('copy', activator='player2')
    t.wait(0.2)
    across(t, 'map_top', 'player2', 44)                 # still a normal head: above it nothing is hit
    t.kv('player2', 'headscale', '2.5')                 # the plugin's path: the engine's pfnKeyValue
    t.wait(0.2)
    across(t, 'plugin_top', 'player2', 44)
    t.kv('player2', 'headscale', '1')
    t.expect.used('cv', count=1)                        # both map triggers ran on player2
    t.expect.used('copy', count=1)
    t.expect.value('map_top', 'player2', 'hit', 0)
    t.expect.value('plugin_top', 'player2', 'hit', 1)
    t.expect.value('plugin_top', 'player2', 'hitgroup', HEAD)
    t.expect.msg('HeadScale', min=2, max=2)            # the plugin's two changes only
