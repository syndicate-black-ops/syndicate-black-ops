"""Tier 5, final pass: env_model "material" with every choice (env_model.cpp EnvModel_MaterialLetter): with "Solid
(model hitboxes)" (8) the model's material letter goes to the clients in its entity state (endpos: letter, tag 4001),
so a bullet on its hitboxes sounds and sparks as that material (the client's material table; suite hitbox shows the
precache for Wood)."""

from etest import Suite

SUITE = Suite('fgd_model_material', 'Tier 5: env_model material, every choice', requires=('EnvModel_HitboxBounds',))
S = SUITE
DOC = 'ENV_MODEL.md#solid-model-hitboxes'
MODEL = 'models/crate_steps.mdl'
# choice -> its letter (pm_shared/pm_materials.h CHAR_TEX_*)
MATERIALS = {0: 'Y', 2: 'M', 3: 'F', 4: 'C', 5: 'T', 6: 'P', 7: 'Y', 8: 'R'}


def material_case(v):
    letter = MATERIALS[v]

    @S.test('env_model "material" %d: the clients get its letter %s (entity state endpos letter + tag 4001)' % (
                v, letter),
            players=0, doc=DOC, covers=('env_model.material=%d' % v,))
    def case(t):
        t.point('env_model', at=(0, 0, 0), targetname='mdl', model=MODEL, angles='0 0 0', spawnflags='8',
                m_iszSequence_On='idle', material=str(v))
        t.wait(0.3)
        t.snap('s', 'mdl', 'endpos')
        t.expect.value('s', 'mdl', 'endpos', (ord(letter), 4001, 0), tol=0.01)
    case.__name__ = 'material_%d' % v
    return case


for _v in MATERIALS:
    material_case(_v)
