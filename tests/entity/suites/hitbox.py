"""env_model "Solid (model hitboxes)" (flag 8): the engine collides with the model's hitboxes (model flag 512),
docs/ENV_MODEL.md "Solid (model hitboxes)". The sample model models/crate_steps.mdl: two crates on the floor
(model Y -79 .. 79, Z 0 .. 42.4) and one on top of the -Y one (Y -79 .. 0, Z 42.4 .. 84.7), X -21.7 .. 21.7.
A player stands on the low step (Z 42.4), which a box around the whole model (top Z 84.7) would not allow.
Test-only copies of it (no flag 512, no hitboxes, 129 hitboxes) are written into the run's models folder."""

import os
import struct

from etest import Suite

SUITE = Suite('hitbox', 'env_model: collision with the model hitboxes', requires=('EnvModel_HitboxBounds',))
S = SUITE
S.server_args = ('+developer', '1')    # the warnings are developer messages (ALERT at_warning)
DOC = 'ENV_MODEL.md#solid-model-hitboxes'
MODEL = 'models/crate_steps.mdl'
TALL = (512, 512, 320)

STEP = 42.36            # top of the floor crates
TOP = 84.72             # top of the top crate
HALF_Y = 78.76          # floor crates: Y -78.76 .. 78.76
PLAYER_HALF = 16.0
PLAYER_STAND = 36.0     # a standing player's origin above the floor under him

_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
with open(os.path.join(_ROOT, 'gamedir', 'syndicate', MODEL), 'rb') as _f:
    _CRATE = _f.read()


def _patched(flags_off=False, numhitboxes=None):
    # a checkout without Git LFS objects (the CI coverage gate) has only the LFS pointer: the gate reads no model data
    if _CRATE.startswith(b'version https://git-lfs'):
        return _CRATE
    b = bytearray(_CRATE)
    if flags_off:
        struct.pack_into('<i', b, 136, struct.unpack_from('<i', b, 136)[0] & ~512)
    if numhitboxes is not None:
        struct.pack_into('<i', b, 156, numhitboxes)     # studiohdr_t.numhitboxes
    return bytes(b)


S.add_file('models/et_crate_noflag.mdl', _patched(flags_off=True))
S.add_file('models/et_crate_nohbox.mdl', _patched(numhitboxes=0))
S.add_file('models/et_crate_129.mdl', _patched(numhitboxes=129))


def crate(t, model=MODEL, yaw=0, spawnflags='8', **kv):
    t.point('env_model', at=(0, 0, 0), targetname='mdl', model=model, angles='0 %d 0' % yaw, spawnflags=spawnflags,
            m_iszSequence_On='idle', **kv)


def big_box(t, solid):
    """Probe-style control: the entity box around the whole model, set directly (2 SOLID_BBOX, 3 SOLID_SLIDEBOX)."""
    t.wait(0.3)
    t.kv('mdl', 'solid', solid)
    t.kv('mdl', 'mins', '-22 -79 0')
    t.kv('mdl', 'maxs', '22 79 85')
    t.kv('mdl', 'size', '44 158 85')
    t.teleport('mdl', 'mdl')        # relink with the new box


def fall_onto(t, at):
    t.teleport('player1', (at[0], at[1], 130), yaw=270)
    t.kv('player1', 'health', '100000')
    t.wait(2.0)
    t.snap('end', 'player1', 'origin', 'flags')
    t.snap('end', 'mdl', 'origin', 'solid', 'mins', 'maxs')


def standing_dist(x, y, z):
    return (x * x + y * y + z * z) ** 0.5


@S.test('falls onto the low step: stands on the floor crates, below the top crate', players=1, doc=DOC, size=TALL,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def fall_step(t):
    crate(t)
    fall_onto(t, (0, 40))
    d = standing_dist(0, 40, STEP + PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)
    t.expect.value('end', 'mdl', 'solid', 2)        # SOLID_BBOX


@S.test('on the low step, walks into the top crate: stops at its face', players=1, doc=DOC, size=TALL,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def walk_on_step(t):
    crate(t)
    t.teleport('player1', (0, 60, STEP + 4), yaw=270)
    t.kv('player1', 'health', '100000')
    t.wait(1.0)
    t.walk('player1', 250, 0, 2.0)
    t.wait(0.5)
    t.snap('end', 'player1', 'origin')
    t.snap('end', 'mdl', 'origin')
    t.expect.distance('end', 'mdl', 'player1', PLAYER_HALF - 1, PLAYER_HALF + 3)


@S.test('walks in at the floor: stops at the floor crates', players=1, doc=DOC,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def walk_in(t):
    crate(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    t.kv('player1', 'health', '100000')
    t.wait(0.5)
    t.walk('player1', 250, 0, 2.0)
    t.wait(0.5)
    t.snap('end', 'player1', 'origin')
    t.snap('end', 'mdl', 'origin')
    t.expect.distance('end', 'mdl', 'player1', HALF_Y + PLAYER_HALF - 1, HALF_Y + PLAYER_HALF + 3)


@S.test('turned 90: the low step turns with it', players=1, doc=DOC, size=TALL,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def turned(t):
    crate(t, yaw=90)
    fall_onto(t, (-40, 0))          # model +Y turned 90 degrees: world -X
    d = standing_dist(40, 0, STEP + PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)


@S.test('the entity box covers the hitboxes (broad phase)', players=0, doc=DOC,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def cover_box(t):
    crate(t)
    t.wait(0.3)
    t.snap('s', 'mdl', 'mins', 'maxs', 'solid')
    t.expect.value('s', 'mdl', 'mins', (-22.65, -79.76, -1), tol=0.2)
    t.expect.value('s', 'mdl', 'maxs', (22.65, 79.76, TOP + 1), tol=0.2)


@S.test('control: the same model as a plain SOLID_BBOX box: stands on the box top', players=1, doc=DOC, size=TALL)
def box_control(t):
    crate(t, model='models/et_crate_noflag.mdl', spawnflags='0')
    big_box(t, '2')
    fall_onto(t, (0, 40))
    d = standing_dist(0, 40, 85 + PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)


@S.test('control: hitbox model as SOLID_SLIDEBOX: the server moves the player on the box', players=1, doc=DOC,
        size=TALL)
def slidebox_control(t):
    crate(t, spawnflags='0')
    big_box(t, '3')
    fall_onto(t, (0, 40))
    d = standing_dist(0, 40, 85 + PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)


@S.test('model without flag 512: warning, the 20-unit box', players=0, doc=DOC,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def missing_flag(t):
    crate(t, model='models/et_crate_noflag.mdl')
    t.wait(0.3)
    t.snap('s', 'mdl', 'solid', 'mins', 'maxs')
    t.expect.value('s', 'mdl', 'solid', 3)          # SOLID_SLIDEBOX
    t.expect.value('s', 'mdl', 'mins', (-10, -10, -10), tol=0.01)
    t.expect.value('s', 'mdl', 'maxs', (10, 10, 10), tol=0.01)
    t.expect.console('et_crate_noflag.mdl is not compiled for hitbox collision ($flags 512): the 20-unit box')


@S.test('model with flag 512 and 0 hitboxes: warning, not solid', players=0, doc=DOC,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def zero_hitboxes(t):
    crate(t, model='models/et_crate_nohbox.mdl')
    t.wait(0.3)
    t.snap('s', 'mdl', 'solid')
    t.expect.value('s', 'mdl', 'solid', 0)
    t.expect.console('et_crate_nohbox.mdl has 0 hitboxes (1 to 128 work): not solid')


@S.test('model with 129 hitboxes: warning, not solid', players=0, doc=DOC,
        covers=('env_model', 'env_model:8', 'env_model.model'))
def too_many_hitboxes(t):
    crate(t, model='models/et_crate_129.mdl')
    t.wait(0.3)
    t.snap('s', 'mdl', 'solid')
    t.expect.value('s', 'mdl', 'solid', 0)
    t.expect.console('et_crate_129.mdl has 129 hitboxes (1 to 128 work): not solid')


@S.test('initially off: not solid; on: SOLID_BBOX; off again: not solid', players=0, doc=DOC,
        covers=('env_model', 'env_model:1', 'env_model:8'))
def on_off(t):
    crate(t, spawnflags='9')
    t.wait(0.3)
    t.snap('off', 'mdl', 'solid')
    t.fire('mdl', use='on')
    t.wait(0.2)
    t.snap('on', 'mdl', 'solid')
    t.fire('mdl', use='off')
    t.wait(0.2)
    t.snap('off2', 'mdl', 'solid')
    t.expect.value('off', 'mdl', 'solid', 0)
    t.expect.value('on', 'mdl', 'solid', 2)
    t.expect.value('off2', 'mdl', 'solid', 0)


@S.test('initially off: a player falls through it', players=1, doc=DOC, size=TALL,
        covers=('env_model', 'env_model:1', 'env_model:8'))
def off_falls_through(t):
    crate(t, spawnflags='9')
    fall_onto(t, (0, 40))
    d = standing_dist(0, 40, PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)


@S.test('scale 2: collides at the unscaled size (warning)', players=1, doc=DOC, size=TALL,
        covers=('env_model', 'env_model:8', 'env_model.scale'))
def scaled(t):
    crate(t, scale='2')
    fall_onto(t, (0, 40))
    d = standing_dist(0, 40, STEP + PLAYER_STAND)
    t.expect.distance('end', 'mdl', 'player1', d - 2, d + 2, flat=False)
    t.expect.console('crate_steps.mdl collides at its unscaled size (scale 2 is drawn only)')


@S.test('material Wood: the clients get its letter (entity state endpos W + tag), its hit sounds are precached',
        players=0, doc=DOC, covers=('env_model', 'env_model:8', 'env_model.material=1'))
def material_wood(t):
    crate(t, material='1')
    t.wait(0.3)
    t.snap('s', 'mdl', 'endpos')
    t.expect.value('s', 'mdl', 'endpos', (ord('W'), 4001, 0), tol=0.01)
    t.expect.precached('debris/wood1.wav', kind='sound')


@S.test('no material key: Cinder Block (concrete, C)', players=0, doc=DOC, covers=('env_model', 'env_model:8'))
def material_default(t):
    crate(t)
    t.wait(0.3)
    t.snap('s', 'mdl', 'endpos')
    t.expect.value('s', 'mdl', 'endpos', (ord('C'), 4001, 0), tol=0.01)


@S.test('without "Solid (model hitboxes)": no material sent', players=0, doc=DOC,
        covers=('env_model', 'env_model:4', 'env_model.material'))
def material_not_solid(t):
    crate(t, spawnflags='4', material='2')
    t.wait(0.3)
    t.snap('s', 'mdl', 'endpos')
    t.expect.value('s', 'mdl', 'endpos', (0, 0, 0), tol=0.01)
