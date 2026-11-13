"""Tier 5, batch 3: FGD items of calc_position, calc_subvelocity and locus_beam that no 1.38 map uses and no other test
asserted (docs/LOCUS.md). Positions are read through a locus_variable, which moves to the position it records."""

from etest import Suite

SUITE = Suite('fgd_locus2', 'Tier 5: calc_position, calc_subvelocity, locus_beam FGD-only items', players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')
DOC = 'LOCUS.md'
TE_DECAL, TE_DECALHIGH, TE_WORLDDECAL, TE_WORLDDECALHIGH = 104, 105, 116, 117


def lv_pos(t, name, source):
    """A locus_variable that moves to the position [LP] `source` (a local entity name) when fired."""
    t.point('locus_variable', targetname=name, m_iszPosition=source, m_iszVelocity='0 0 0', m_iszRatio='0',
            names=('m_iszPosition',))


# ---------------------------------------------------------------------------------------------- calc_position
BOX_AT, BOX_SIZE = (0, 120, 0), (32, 32, 64)        # a func_wall: bottom center (0 120 0), 64 tall
POINTS = {2: ('top', (0, 120, 64)), 3: ('center', (0, 120, 32)), 4: ('bottom', (0, 120, 0))}


def box_case(impulse):
    label, ref = POINTS[impulse]

    @S.test('calc_position %d (%s) of a brush entity: the %s of its box' % (impulse, label, label), doc=DOC,
            covers=('calc_position.impulse=%d' % impulse, 'calc_position.netname'))
    def case(t):
        t.brush('func_wall', targetname='box', size=BOX_SIZE, at=BOX_AT)
        t.point('info_target', targetname='ref', at=ref)
        t.point('calc_position', targetname='cp', netname='box', impulse=str(impulse), message='0 0 0')
        lv_pos(t, 'lv', 'cp')
        t.fire('lv')
        t.wait(0.1)
        t.snap('s', 'lv', 'origin')
        t.snap('s', 'ref', 'origin')
        t.expect.distance('s', 'lv', 'ref', 0, 1.5, flat=False)
    case.__name__ = 'cp_%s' % label
    return case


for _i in sorted(POINTS):
    box_case(_i)


@S.test('calc_position 0 (origin) with Add offset 0 0 24: the entity\'s origin 24 units up', doc=DOC,
        covers=('calc_position.impulse=0', 'calc_position.message'))
def cp_origin_offset(t):
    t.point('info_target', targetname='spot', at=(-80, 80, 16))
    t.point('info_target', targetname='ref', at=(-80, 80, 40))
    t.point('calc_position', targetname='cp', netname='spot', impulse='0', message='0 0 24')
    lv_pos(t, 'lv', 'cp')
    t.fire('lv')
    t.wait(0.1)
    t.snap('s', 'lv', 'origin')
    t.snap('s', 'ref', 'origin')
    t.expect.distance('s', 'lv', 'ref', 0, 1, flat=False)


@S.test('calc_position 1 (eyes) of a scientist: straight above his origin, at eye height', doc=DOC,
        covers=('calc_position.impulse=1',))
def cp_eyes(t):
    t.point('monster_scientist', targetname='sci', at=(0, 120, 0))
    t.point('calc_position', targetname='cp', netname='sci', impulse='1', message='0 0 0')
    lv_pos(t, 'lv', 'cp')
    t.wait(0.3)
    t.fire('lv')
    t.wait(0.1)
    t.snap('s', 'lv', 'origin')
    t.snap('s', 'sci', 'origin', 'view_ofs')
    t.expect.distance('s', 'lv', 'sci', 0, 1, flat=True)
    t.expect.distance('s', 'lv', 'sci', 40, 80, flat=False)


@S.test('calc_position 9 (random in its box): every point is inside the brush entity\'s box', doc=DOC,
        covers=('calc_position.impulse=9',))
def cp_random(t):
    t.brush('func_wall', targetname='box', size=BOX_SIZE, at=BOX_AT)
    t.point('info_target', targetname='ref', at=(0, 120, 32))
    t.point('calc_position', targetname='cp', netname='box', impulse='9', message='0 0 0')
    for i in range(4):
        lv_pos(t, 'lv%d' % i, 'cp')
        t.fire('lv%d' % i)
    t.wait(0.1)
    t.snap('s', 'ref', 'origin')
    for i in range(4):
        t.snap('s', 'lv%d' % i, 'origin')
        t.expect.distance('s', 'lv%d' % i, 'ref', 0, 22.7, flat=True)       # within the 32 x 32 footprint
        t.expect.distance('s', 'lv%d' % i, 'ref', 0, 39.2, flat=False)


@S.test('calc_position 10 (Calc Position of the entity): another calc entity\'s position', doc=DOC,
        covers=('calc_position.impulse=10',))
def cp_calc(t):
    t.brush('func_wall', targetname='box', size=BOX_SIZE, at=BOX_AT)
    t.point('info_target', targetname='ref', at=(0, 120, 64))
    t.point('calc_position', targetname='top', netname='box', impulse='2', message='0 0 0')
    t.point('calc_position', targetname='cp', netname='top', impulse='10', message='0 0 0')
    lv_pos(t, 'lv', 'cp')
    t.fire('lv')
    t.wait(0.1)
    t.snap('s', 'lv', 'origin')
    t.snap('s', 'ref', 'origin')
    t.expect.distance('s', 'lv', 'ref', 0, 1, flat=False)


@S.test('calc_position 10 with Add offset 0 0 24: the other entity\'s position plus the offset', doc=DOC,
        covers=('calc_position.impulse=10', 'calc_position.message'))
def cp_calc_offset(t):
    t.brush('func_wall', targetname='box', size=BOX_SIZE, at=BOX_AT)
    t.point('info_target', targetname='ref', at=(0, 120, 88))
    t.point('calc_position', targetname='top', netname='box', impulse='2', message='0 0 0')
    t.point('calc_position', targetname='cp', netname='top', impulse='10', message='0 0 24')
    lv_pos(t, 'lv', 'cp')
    t.fire('lv')
    t.wait(0.1)
    t.snap('s', 'lv', 'origin')
    t.snap('s', 'ref', 'origin')
    t.expect.distance('s', 'lv', 'ref', 0, 1, flat=False)


CONTROLLER = 'models/controller.mdl'     # Half-Life's alien controller (valve/): 4 attachments


@S.test('calc_position 5-8 (attachment points 0-3) of a studio model: points on the model, each its own', doc=DOC,
        covers=tuple('calc_position.impulse=%d' % i for i in (5, 6, 7, 8)))
def cp_attachments(t):
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2',
            m_iAction_On='1')
    for i in (5, 6, 7, 8):
        t.point('calc_position', targetname='cp%d' % i, netname='mdl', impulse=str(i), message='0 0 0')
        lv_pos(t, 'lv%d' % i, 'cp%d' % i)
    t.wait(0.2)
    for i in (5, 6, 7, 8):
        t.fire('lv%d' % i)
    t.wait(0.1)
    t.snap('s', 'mdl', 'origin')
    for i in (5, 6, 7, 8):
        t.snap('s', 'lv%d' % i, 'origin')
        t.expect.distance('s', 'lv%d' % i, 'mdl', 1, 120, flat=False)
    t.expect.distance('s', 'lv5', 'lv6', 1, 300, flat=False)
    t.expect.distance('s', 'lv7', 'lv8', 1, 300, flat=False)


# ---------------------------------------------------------------------------------------------- locus_beam
BEAM_FSINE, BEAM_FSOLID, BEAM_FSHADEIN, BEAM_FSHADEOUT = 0x10, 0x20, 0x40, 0x80
FLAG_BITS = {128: BEAM_FSHADEIN, 256: BEAM_FSHADEOUT, 512: BEAM_FSOLID, 1024: BEAM_FSINE}


@S.test('locus_beam Position & Entity (1): from a position to the activator; Color, Distortion, Start frame, Scroll '
        'rate and End attachment reach the beam', players=1, doc=DOC,
        covers=('locus_beam.impulse=1', 'locus_beam.rendercolor', 'locus_beam.m_iDistortion', 'locus_beam.m_fFrame',
                'locus_beam.m_iScrollRate', 'locus_beam.m_iEndAttachment'))
def beam_pos_ent(t):
    t.point('locus_beam', targetname='lb', m_iszStart='0 0 0', m_iszEnd='!activator', impulse='1',
            rendercolor='10 20 30', m_iDistortion='40', m_fFrame='5', m_iScrollRate='35', m_iEndAttachment='2',
            m_iszTargetName='bm', m_fDuration='1')
    t.fire('lb', activator='player1')
    t.wait(0.1)
    t.snap('b', 'bm', 'rendermode', 'skin', 'rendercolor', 'body', 'frame', 'kv:animtime')
    t.expect.spawned('bm', count=1)
    t.expect.value('b', 'bm', 'rendermode', 1)                     # BEAM_ENTPOINT
    t.expect.value('b', 'bm', 'skin', 1 | (2 << 12))               # player1, attachment 2
    t.expect.value('b', 'bm', 'rendercolor', [10, 20, 30])
    t.expect.value('b', 'bm', 'body', 40)                          # noise
    t.expect.value('b', 'bm', 'frame', 5)
    t.expect.value('b', 'bm', 'kv:animtime', 35)                      # scroll rate


@S.test('locus_beam Start attachment between two studio models (Entity & Entity)', doc=DOC,
        covers=('locus_beam.m_iStartAttachment', 'locus_beam.m_iEndAttachment'))
def beam_attachments(t):
    t.point('env_model', targetname='ma', at=(-100, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('env_model', targetname='mb', at=(100, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('locus_beam', targetname='lb', m_iszStart='ma', m_iszEnd='mb', impulse='0', m_iStartAttachment='3',
            m_iEndAttachment='1', m_iszTargetName='bm', m_fDuration='1', names=('m_iszStart', 'm_iszEnd'))
    t.fire('lb')
    t.wait(0.1)
    t.snap('b', 'bm', 'sequence', 'skin')
    t.expect.value('b', 'bm', 'sequence', 3 << 12, op='&')
    t.expect.value('b', 'bm', 'skin', 1 << 12, op='&')


@S.test('locus_beam flags 128 / 256 / 512 / 1024: Fade start, Fade end, Draw solid, Draw sine on the beam', doc=DOC,
        covers=tuple('locus_beam:%d' % f for f in FLAG_BITS))
def beam_flags(t):
    for i, (flag, bit) in enumerate(sorted(FLAG_BITS.items())):
        t.point('locus_beam', targetname='lb%d' % flag, m_iszStart='-100 %d 48' % (i * 40),
                m_iszEnd='100 %d 48' % (i * 40), impulse='2', spawnflags=str(flag), m_iszTargetName='bm%d' % flag,
                m_fDuration='1')
        t.fire('lb%d' % flag)
    t.wait(0.1)
    for flag, bit in FLAG_BITS.items():
        t.snap('b', 'bm%d' % flag, 'rendermode')
        t.expect.value('b', 'bm%d' % flag, 'rendermode', bit, op='&')
        for other in FLAG_BITS.values():
            if other != bit:
                t.expect.value('b', 'bm%d' % flag, 'rendermode', other, op='!&')


@S.test('locus_beam Decal end (64): scorch marks where the beam hits a wall', doc=DOC, covers=('locus_beam:64',))
def beam_decal(t):
    t.brush('func_wall', targetname='wall', size=(64, 16, 96), at=(0, 120, 0))
    t.point('info_target', targetname='src', at=(0, 0, 48))
    t.point('locus_beam', targetname='lb', m_iszStart='src', m_iszEnd='0 300 0', impulse='3', spawnflags='64',
            m_iszTargetName='bm', m_fDuration='0.6', names=('m_iszStart',))
    t.mark('on')
    t.fire('lb')
    t.wait(0.8)
    t.expect.msg('svc:23', min=1, fields={'te': TE_DECAL}, after='on')


LBEAM_DMG = (0, 1, 2, 4, 8, 16, 64, 128, 256, 512, 1024, 16384, 65536, 131072, 262144, 1048576)


@S.test('locus_beam Damage type: every choice reaches what the beam hits with that type', doc=DOC,
        covers=('locus_beam.m_iDamageType',) + tuple('locus_beam.m_iDamageType=%d' % d for d in LBEAM_DMG))
def beam_damage_types(t):
    for i, d in enumerate(LBEAM_DMG):
        x, y = -210 + (i % 8) * 60, -100 + (i // 8) * 160
        t.brush('func_breakable', targetname='b%d' % i, health='100000', size=(24, 24, 48), at=(x, y + 60, 0))
        t.point('info_target', targetname='src%d' % i, at=(x, y, 24))
        t.point('locus_beam', targetname='lb%d' % i, m_iszStart='src%d' % i, m_iszEnd='0 120 0', impulse='3',
                m_fDamage='50', m_iDamageType=str(d), m_iszTargetName='bm%d' % i, m_fDuration='0.5',
                names=('m_iszStart',))
        t.fire('lb%d' % i)
    t.wait(0.7)
    for i, d in enumerate(LBEAM_DMG):
        t.expect.damaged('b%d' % i, min=1, type=d if d else None)
