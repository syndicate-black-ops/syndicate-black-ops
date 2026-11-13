"""Tier 5, batch 2: env_model FGD items that no 1.38 map uses and no other test asserted (docs/ENV_MODEL.md): on / off
sequences, behavior when a sequence ends, framerates, starting frame, skin, body, Drop to Floor, Client-side
animation. Model: models/barney.mdl (sequence 13 "laflinch" and 14 "raflinch": 9 frames at 16 fps, about 0.5 s, not
looping). Round reset: suite fgd_rounds."""

from etest import Suite

SUITE = Suite('fgd_model', 'Tier 5: env_model FGD-only keys and flags')
S = SUITE
DOC = 'ENV_MODEL.md'
MODEL = 'models/barney.mdl'
SEQ_A, SEQ_B = 13, 14           # laflinch, raflinch


def model(t, name, at=(0, 60, 0), **kv):
    kv.setdefault('m_iszSequence_On', 'laflinch')
    kv.setdefault('m_iszSequence_Off', 'raflinch')
    return t.point('env_model', targetname=name, at=at, model=MODEL, **kv)


@S.test('env_model Sequence when on / off: on plays the on sequence, Off switches to the off sequence', doc=DOC,
        covers=('env_model.m_iszSequence_On', 'env_model.m_iszSequence_Off', 'env_model.m_iAction_Off=0',
                'env_model.m_iAction_On=0'))
def sequences(t):
    model(t, 'm', m_iAction_On='0', m_iAction_Off='0')
    t.wait(0.2)
    t.snap('on', 'm', 'sequence')
    t.fire('m', use='off')
    t.wait(0.2)
    t.snap('off', 'm', 'sequence')
    t.expect.value('on', 'm', 'sequence', SEQ_A)
    t.expect.value('off', 'm', 'sequence', SEQ_B)


@S.test('env_model Behavior when on 0 (freeze): the sequence ends on its last frame and stays', doc=DOC,
        covers=('env_model.m_iAction_On=0',))
def freeze(t):
    model(t, 'm', m_iAction_On='0', m_iAction_Off='0', spawnflags='1')
    t.fire('m', use='on')
    t.wait(1.2)
    t.snap('end', 'm', 'sequence', 'frame')
    t.expect.value('end', 'm', 'sequence', SEQ_A)
    t.expect.value('end', 'm', 'frame', 255, tol=1)


@S.test('env_model Behavior when on 1 (loop): the sequence starts over and never freezes', doc=DOC,
        covers=('env_model.m_iAction_On=1',))
def loop(t):
    model(t, 'm', m_iAction_On='1', m_iAction_Off='0', spawnflags='1')
    t.fire('m', use='on')
    t.wait(1.25)
    t.snap('a', 'm', 'sequence', 'frame')
    t.wait(0.2)
    t.snap('b', 'm', 'sequence', 'frame')
    t.expect.value('a', 'm', 'sequence', SEQ_A)
    t.expect.value('b', 'm', 'sequence', SEQ_A)
    t.expect.value('b', 'm', 'frame', 250, op='<')


@S.test('env_model Behavior when on / off 2 (change state): on ends -> off sequence; off ends -> on again', doc=DOC,
        covers=('env_model.m_iAction_On=2', 'env_model.m_iAction_Off=2'))
def change_state(t):
    model(t, 'm', m_iAction_On='2', m_iAction_Off='2', spawnflags='1')
    t.fire('m', use='on')
    t.wait(0.25)
    t.snap('on', 'm', 'sequence')
    t.wait(0.6)                      # the on sequence (0.5 s) ended: off
    t.snap('off', 'm', 'sequence')
    t.wait(0.6)                      # the off sequence ended: on again
    t.snap('again', 'm', 'sequence')
    t.expect.value('on', 'm', 'sequence', SEQ_A)
    t.expect.value('off', 'm', 'sequence', SEQ_B)
    t.expect.value('again', 'm', 'sequence', SEQ_A)


@S.test('env_model Behavior when off 1 (loop): the off sequence starts over', doc=DOC,
        covers=('env_model.m_iAction_Off=1',))
def loop_off(t):
    model(t, 'm', m_iAction_On='0', m_iAction_Off='1')
    t.fire('m', use='off')
    t.wait(1.45)
    t.snap('b', 'm', 'sequence', 'frame')
    t.expect.value('b', 'm', 'sequence', SEQ_B)
    t.expect.value('b', 'm', 'frame', 250, op='<')


@S.test('env_model Framerate when on 2 / when off 0.5: the animation speed of each state', doc=DOC,
        covers=('env_model.m_flFramerate_On', 'env_model.m_flFramerate_Off'))
def framerates(t):
    model(t, 'm', m_iAction_On='1', m_iAction_Off='1', m_flFramerate_On='2', m_flFramerate_Off='0.5')
    t.wait(0.2)
    t.snap('on', 'm', 'framerate')
    t.fire('m', use='off')
    t.wait(0.2)
    t.snap('off', 'm', 'framerate')
    t.expect.value('on', 'm', 'framerate', 2)
    t.expect.value('off', 'm', 'framerate', 0.5)


@S.test('env_model Framerate when off blank: the off sequence plays at 1 ("1 is default")', doc=DOC,
        covers=('env_model.m_flFramerate_Off', 'env_model.m_flFramerate_On'))
def framerate_off_default(t):
    model(t, 'm', m_iAction_On='1', m_iAction_Off='1', m_flFramerate_On='2')
    t.fire('m', use='off')
    t.wait(0.2)
    t.snap('off', 'm', 'framerate')
    t.expect.value('off', 'm', 'framerate', 1)


@S.test('env_model Skin 1, Body 1', doc=DOC, covers=('env_model.skin', 'env_model.body'))
def skin_body(t):
    model(t, 'm', skin='1', body='1')
    t.wait(0.1)
    t.snap('s', 'm', 'skin', 'body')
    t.expect.value('s', 'm', 'skin', 1)
    t.expect.value('s', 'm', 'body', 1)


@S.test('env_model Starting frame 128 with Client-side animation (16): the model starts at frame 128 and the server '
        'leaves the frame there', doc=DOC, covers=('env_model.frame', 'env_model:16'))
def start_frame(t):
    model(t, 'm', frame='128', m_iAction_On='1', spawnflags='16')
    t.wait(0.3)
    t.snap('s', 'm', 'frame')
    t.expect.value('s', 'm', 'frame', 128, tol=1)


@S.test('env_model Client-side animation (16): the server leaves the frame at the start (0) while the sequence plays '
        'out; without it the server advances it to the end (255)', doc=DOC, covers=('env_model:16',))
def client_anim(t):
    model(t, 'cl', at=(-60, 60, 0), m_iAction_On='0', spawnflags='16')
    model(t, 'sv', at=(60, 60, 0), m_iAction_On='0')
    t.wait(0.1)
    t.snap('s', 'cl', 'frame')
    t.snap('s', 'sv', 'frame')
    t.expect.value('s', 'cl', 'frame', 0, tol=0.5)
    t.expect.value('s', 'sv', 'frame', 255, tol=1)


@S.test('env_model Drop to Floor (2): placed 64 units up it lands on the floor; without the flag it stays', doc=DOC,
        covers=('env_model:2',))
def drop(t):
    model(t, 'fall', at=(-60, 60, 64), spawnflags='2')
    model(t, 'stay', at=(60, 60, 64))
    t.point('info_target', targetname='ref_fall', at=(-60, 60, 64))
    t.point('info_target', targetname='ref_stay', at=(60, 60, 64))
    t.wait(0.1)
    t.snap('s', 'fall', 'origin')
    t.snap('s', 'stay', 'origin')
    t.snap('s', 'ref_fall', 'origin')
    t.snap('s', 'ref_stay', 'origin')
    t.expect.distance('s', 'fall', 'ref_fall', 40, 70, flat=False)
    t.expect.distance('s', 'stay', 'ref_stay', 0, 1, flat=False)
