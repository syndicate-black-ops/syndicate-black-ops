"""Tier 5, final pass: the Scripted base keys on aiscripted_sequence (CCineAI, scripted.cpp: PossessEntity moves the
monster without taking its AI state away). Each test shows the visible effect on a prisoner barney (no fights; the
player is notarget). A named script with an idle animation takes its barney at map start and holds him until fired."""

from etest import Suite

SUITE = Suite('fgd_aiscripted', 'Tier 5: aiscripted_sequence keys (Scripted base, AI Schedule when done)', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#scripted-scenes-batch-4'
C = 'aiscripted_sequence'
NAMES = ('m_iszEntity',)


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def held(t, name, at, script_kv=None, play='barn_wave', script_at=None):
    """A prisoner barney `name` and an aiscripted_sequence 'name_ai' (the 2.55 s wave) that takes him at map start and
    holds him until fired."""
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16')
    kv = dict(m_iszEntity=name, m_iszPlay=play, m_fMoveTo='0', m_flRadius='512', m_iszIdle='idle1')
    kv.update(script_kv or {})
    t.point(C, targetname=name + '_ai', at=script_at or at, angles='0 270 0', names=NAMES, **kv)


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


@S.test('%s "Search Radius" 100 with a class name as Target Monster: it takes the barney 60 away, not the one 300 '
        'away' % C, players=1, doc=DOC, covers=(C + '.m_flRadius',))
def radius(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, d in (('bnear', -150, 60), ('bfar', 150, 300)):
        t.point('monster_barney', targetname=n, at=(x, -150, 0), angles='0 270 0', spawnflags='16')
        t.point(C, targetname=n + '_ai', at=(x, -150 + d, 0), m_iszEntity='monster_barney', m_iszIdle='idle1',
                m_iszPlay='barn_wave', m_flRadius='100', m_fMoveTo='0')
    t.wait(2.5)
    t.snap('s', 'bnear', 'cine')
    t.snap('s', 'bfar', 'cine')
    t.expect.value('s', 'bnear', 'cine', 1)
    t.expect.value('s', 'bfar', 'cine', 0)


@S.test('%s Move to 0 (No): fired, the barney plays the action where he stands, then the script fires its target' % C,
        players=1, doc=DOC, covers=(C + '.m_fMoveTo', C + '.m_fMoveTo=0', C + '.m_iszPlay'))
def move_none(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bn', (0, -100, 0), {'target': 'done'}, script_at=(0, 150, 0))
    info(t, 'done')
    info(t, 'start', at=(0, -100, 0))
    t.wait(2.0)
    t.mark('go')
    t.fire('bn_ai')
    t.wait(1.0)
    t.snap('mid', 'bn', 'cine', 'origin')
    t.snap('mid', 'start', 'origin')
    t.wait(3.0)
    t.expect.value('mid', 'bn', 'cine', 1)
    t.expect.distance('mid', 'bn', 'start', 0, 8, flat=True)
    t.expect.used('done', count=1, after='go', within=(2.0, 3.8))


@S.test('%s Move to 4 (Instantaneous) and 7 (Teleport): the barney is at the script at once; Turn mode 1 / 2 / 0 '
        'with Move to 5 (No - Turn to Face): he turns to it, does not turn, takes its angle (90)' % C, players=1,
        doc=DOC, covers=(C + '.m_fMoveTo=4', C + '.m_fMoveTo=7', C + '.m_fMoveTo=5', C + '.m_fTurnType',
                         C + '.m_fTurnType=0', C + '.m_fTurnType=1', C + '.m_fTurnType=2'))
def move_turn(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, mt in (('bi', -150, '4'), ('bt', 150, '7')):
        t.point('monster_barney', targetname=n, at=(x, -150, 0), angles='0 90 0', spawnflags='16')
        t.point(C, targetname=n + '_ai', at=(x, 150, 0), angles='0 90 0', m_iszEntity=n, m_iszPlay='barn_wave',
                m_fMoveTo=mt, names=NAMES)
        info(t, n + '_mark', at=(x, 150, 0))
    for n, x, tt in (('btf', -60, '1'), ('btn', 0, '2'), ('btm', 60, '0')):
        t.point('monster_barney', targetname=n, at=(x, -100, 0), angles='0 0 0', spawnflags='16')
        t.point(C, targetname=n + '_ai', at=(x, 100, 0), angles='0 90 0', m_iszEntity=n, m_iszPlay='idle1',
                m_fMoveTo='5', m_fTurnType=tt, names=NAMES)
    t.wait(1.5)
    for n in ('bi', 'bt', 'btf', 'btn', 'btm'):
        t.fire(n + '_ai')
    t.wait(1.0)
    for n in ('bi', 'bt', 'bi_mark', 'bt_mark'):
        t.snap('at', n, 'origin')
    t.wait(2.0)                  # the turn takes longer under load
    for n in ('btf', 'btn', 'btm'):
        t.snap('s', n, 'angles')
    t.expect.distance('at', 'bi', 'bi_mark', 0, 20)
    t.expect.distance('at', 'bt', 'bt_mark', 0, 20)
    t.expect.value('s', 'btf', 'angles.y', 90, tol=12)
    t.expect.value('s', 'btn', 'angles.y', 0, tol=12)
    t.expect.value('s', 'btm', 'angles.y', 90, tol=12)


@S.test('%s Move to 1 (Walk) and 2 (Run): the barney goes to the script, Run faster (after 2 s the runner is nearer '
        'it); Fire after moving fires once he is there; Move to target\'s radius 150 stops him short' % C,
        players=1, doc=DOC,
        covers=(C + '.m_fMoveTo=1', C + '.m_fMoveTo=2', C + '.m_iszFireOnBegin', C + '.moveto_radius'))
def move_walk_run(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, x, mt, kv in (('bw', -150, '1', {}), ('br', 0, '2', {}), ('bm', 150, '1', {'moveto_radius': '150'})):
        t.point('monster_barney', targetname=n, at=(x, -200, 0), angles='0 90 0', spawnflags='16')
        t.point(C, targetname=n + '_ai', at=(x, 200, 0), angles='0 90 0', m_iszEntity=n, m_iszPlay='barn_wave',
                m_fMoveTo=mt, m_iszFireOnBegin=n + '_begin', names=NAMES, **kv)
        info(t, n + '_mark', at=(x, 200, 0))
        info(t, n + '_begin')
    t.wait(1.5)
    t.mark('go')
    for n in ('bw', 'br', 'bm'):
        t.fire(n + '_ai')
    t.wait(2.5)
    for n in ('bw', 'br', 'bw_mark', 'br_mark'):
        t.snap('mid', n, 'origin')
    t.wait(6.0)
    for n in ('bw', 'br', 'bm', 'bw_mark', 'br_mark', 'bm_mark'):
        t.snap('s', n, 'origin')
    t.expect.distance('mid', 'bw', 'bw_mark', 150, 400)
    t.expect.distance('mid', 'br', 'br_mark', 0, 120)
    t.expect.distance('s', 'bw', 'bw_mark', 0, 20)
    t.expect.distance('s', 'br', 'br_mark', 0, 20)
    t.expect.distance('s', 'bm', 'bm_mark', 110, 165)
    for n in ('bw', 'br'):
        t.expect.fired(n + '_begin', count=1, after='go', within=(1.0, 8.5))


@S.test('%s "Repeat action X more times" 2: the 2.55 s wave plays three times (still in the script at 4 s, out by '
        '9 s); without it out at 4 s' % C, players=1, doc=DOC, covers=(C + '.m_iRepeats',))
def repeats(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'brp', (-100, 0, 0), {'m_iRepeats': '2'})
    held(t, 'bon', (100, 0, 0))
    t.wait(2.0)
    t.fire('brp_ai')
    t.fire('bon_ai')
    t.wait(4.0)
    t.snap('a', 'brp', 'cine')
    t.snap('a', 'bon', 'cine')
    t.wait(5.0)
    t.snap('b', 'brp', 'cine')
    t.expect.value('a', 'brp', 'cine', 1)
    t.expect.value('a', 'bon', 'cine', 0)
    t.expect.value('b', 'brp', 'cine', 0)


@S.test('%s "AI Schedule when done" 1 (Ambush): after the action the barney waits in the Ambush schedule; 0 '
        '(Default AI): he picks his own (seen the moment the script ends: the player in view soon interrupts an '
        'Ambush)' % C, players=1, doc=DOC,
        covers=(C + '.m_iFinishSchedule', C + '.m_iFinishSchedule=0', C + '.m_iFinishSchedule=1'))
def finish_schedule(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bam', (-100, 0, 0), {'m_iFinishSchedule': '1', 'target': 'bam_done'})
    held(t, 'bdf', (100, 0, 0), {'m_iFinishSchedule': '0', 'target': 'bdf_done'})
    info(t, 'bam_done')
    info(t, 'bdf_done')
    t.wait(2.0)
    t.fire('bam_ai')
    t.wait_for_fire('bam_done', 5.0)     # the target fires right after the clean-up sets the schedule
    t.snap('s', 'bam', 'cine', 'schedule')
    t.wait(1.0)
    t.fire('bdf_ai')
    t.wait_for_fire('bdf_done', 5.0)
    t.wait(0.3)                          # its next think picks a schedule
    t.snap('s2', 'bdf', 'cine', 'schedule')
    t.expect.fired('bam_done', count=1)
    t.expect.fired('bdf_done', count=1)
    t.expect.value('s', 'bam', 'cine', 0)
    t.expect.value('s', 'bam', 'schedule', 'Ambush')
    t.expect.value('s2', 'bdf', 'schedule', 'Ambush', op='!=')


@S.test('%s flag 32768 "Remove on interruption": a hit breaks the barney out and the script is removed; without the '
        'flag it stays' % C, players=1, doc=DOC, covers=(C + ':32768',))
def remove_on_interruption(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'brm', (-100, 0, 0), {'spawnflags': '32768'})
    held(t, 'bkp', (100, 0, 0))
    t.wait(2.0)
    t.mark('hit')
    t.damage('brm', 10, attacker='player1')
    t.damage('bkp', 10, attacker='player1')
    t.wait(1.0)
    t.snap('s', 'brm', 'cine')
    t.snap('s', 'bkp', 'cine')
    t.expect.value('s', 'brm', 'cine', 0)
    t.expect.value('s', 'bkp', 'cine', 0)
    t.expect.removed('brm_ai', after='hit')
    t.expect.not_removed('bkp_ai', after='hit')
