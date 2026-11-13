"""Tier 5, final pass: the flags scripted_sequence and aiscripted_sequence apply when the action ends
(CBaseMonster::CineCleanup, scripted.cpp) and while the script waits (schedule.cpp). The action is the barney's
beat_grunt (3.9 s): its root bone ends 26 units off his origin (to his left), 38 up, so the cleanup moves him there
and drops him to the floor. Each test snaps the barney the moment the script fires its target (SequenceDone fires it
right after the cleanup). The player is notarget; the barney is a prisoner (no fights)."""

from etest import Suite

SUITE = Suite('fgd_scripted_flags', 'Tier 5: scripted_sequence / aiscripted_sequence end-of-action and idle flags',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#scripted-scenes-batch-4'
CLASSES = ('scripted_sequence', 'aiscripted_sequence')
NAMES = ('m_iszEntity',)
# beat_grunt's root bone at its last frame, in the barney's frame: 26 to his left, 4 back, 38 up; he faces 270 (south),
# so in the world: +26 x, +4 y
SHIFT = (26, 4)


def setup(t, cls, flags, play='beat_grunt', at=(0, 0, 0)):
    t.point('monster_barney', targetname='b', at=at, angles='0 270 0', spawnflags='16')
    t.point(cls, targetname='ss', at=at, angles='0 270 0', m_iszEntity='b', m_iszPlay=play, m_iszIdle='idle1',
            m_fMoveTo='0', m_flRadius='512', target='done', spawnflags=str(flags), names=NAMES)
    t.point('info_target', targetname='done')


def play(t):
    t.wait(2.0)
    t.snap('pre', 'b', 'origin', 'angles')
    t.mark('go')
    t.fire('ss')
    t.wait_for_fire('done', 7.0)
    t.snap('end', 'b', 'origin', 'angles', 'flags')
    t.wait(0.5)


def cases(cls):
    short = 'ai' if cls.startswith('ai') else 'ss'

    @S.test('%s without the end flags: when the action ends the barney stands where its root bone ended (%d, %d), on '
            'the floor, facing as before' % ((cls,) + SHIFT), players=1, doc=DOC, covers=(cls + '.m_iszPlay',))
    def plain(t):
        t.flags('player1', '+FL_NOTARGET')
        setup(t, cls, 0)
        play(t)
        t.expect.fired('done', count=1, after='go')
        t.expect.moved('pre', 'end', 'b', delta=SHIFT + (0,), tol=6)
        t.expect.value('end', 'b', 'angles.y', 270, tol=2)
    plain.__name__ = short + '_plain'

    @S.test('%s flag 65536 "Don\'t Drop to Floor": when the action ends the barney is at its root bone, 38 up, '
            'not dropped to the floor' % cls, players=1, doc=DOC, covers=(cls + ':65536',))
    def no_drop(t):
        t.flags('player1', '+FL_NOTARGET')
        setup(t, cls, 65536)
        play(t)
        t.expect.moved('pre', 'end', 'b', delta=SHIFT + (38,), tol=6)
    no_drop.__name__ = short + '_no_drop'

    @S.test('%s flag 131072 "Allow stuck": where the action ends in a solid (a func_wall at his new place) the '
            'barney stays at the root bone; without it he goes back to where he stood' % cls, players=1, doc=DOC,
            covers=(cls + ':131072',))
    def stuck(t, flags=131072):
        t.flags('player1', '+FL_NOTARGET')
        setup(t, cls, flags)
        # a block inside the barney's box at his new place (x 10 .. 42), clear of his place before (x -16 .. 16)
        t.brush('func_wall', size=(8, 8, 8), at=(36, 4, 40), tex='666CAISSE')
        play(t)
        t.expect.moved('pre', 'end', 'b', delta=SHIFT + (38,) if flags else (0, 0, 0), tol=6)
    stuck.__name__ = short + '_stuck'

    @S.test('%s without "Allow stuck": where the action ends in a solid the barney goes back to where he stood' % cls,
            players=1, doc=DOC, covers=(cls + ':131072',))
    def not_stuck(t):
        stuck(t, flags=0)
    not_stuck.__name__ = short + '_not_stuck'


for _c in CLASSES:
    cases(_c)


@S.test('scripted_sequence flag 128 "No Script Movement": when the action ends the barney stays where he stood (even '
        'with Don\'t Drop to Floor: no move at all)', players=1, doc=DOC, covers=('scripted_sequence:128',))
def no_script_movement(t):
    t.flags('player1', '+FL_NOTARGET')
    setup(t, 'scripted_sequence', 128 | 65536)
    play(t)
    t.expect.moved('pre', 'end', 'b', delta=(0, 0, 0), tol=2)


@S.test('scripted_sequence flag 16384 "Force idle animation looping": waiting for its trigger, the barney plays the '
        'non-looping 2.5 s wave again and again; without it the wave stops on its last frame', players=1, doc=DOC,
        covers=('scripted_sequence:16384', 'scripted_sequence.m_iszIdle'))
def idle_looping(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('bl', -100, 16384), ('bs', 100, 0)):
        t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 0, 0), angles='0 270 0', m_iszEntity=n,
                m_iszIdle='barn_wave', m_iszPlay='idle1', m_fMoveTo='0', spawnflags=str(fl), names=NAMES)
    t.wait(4.0)
    labels = []
    for i in range(4):
        labels.append('f%d' % i)
        t.snap('f%d' % i, 'bl', 'frame', 'seqname')
        t.snap('f%d' % i, 'bs', 'frame', 'seqname')
        t.wait(0.4)
    for lab in labels:
        t.expect.value(lab, 'bs', 'frame', 250, op='>=')
        t.expect.value(lab, 'bl', 'seqname', 'barn_wave')
    t.expect.value_any(labels, 'bl', 'frame', 240, op='<')
