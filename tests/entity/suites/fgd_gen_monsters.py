"""Tier 5, final pass, generated: the look choices of monsters. Every Pose of the dead monsters plays the animation its
FGD label names (their m_szPoses tables), every Body value is the body they show, barney's Gun State (bodystate) and
the zombie grunt's Weapons. Checked through the snapshot (test layer field seqname: the sequence's name)."""

from etest import Suite

SUITE = Suite('fgd_gen_monsters', 'Tier 5: monster pose, body and gun choices (generated)', players=1)
S = SUITE
DOC = 'FGD monster looks'
# class -> pose value -> animation (scientist.cpp, player.cpp, barney.cpp, hgrunt.cpp CDead*::m_szPoses)
POSES = {
    'monster_scientist_dead': {0: 'lying_on_back', 1: 'lying_on_stomach', 2: 'dead_sitting', 4: 'dead_table1',
                               5: 'dead_table2', 6: 'dead_table3'},
    'monster_hevsuit_dead': {0: 'deadback', 1: 'deadsitting', 2: 'deadstomach', 3: 'deadtable'},
}
BODIES = {'monster_scientist_dead': (0, 1, 2, 3),   # monster_hevsuit_dead: always body 1 (no Body key in the FGD)
          'monster_sitting_scientist': (0, 1, 2, 3)}


def pose_case(cls):
    poses = POSES[cls]

    @S.test('%s Pose: each choice plays its animation (%s)' % (cls, ', '.join('%d %s' % kv for kv in poses.items())),
            players=1, doc=DOC, covers=tuple('%s.pose=%d' % (cls, p) for p in poses))
    def case(t):
        for i, p in enumerate(poses):
            t.point(cls, targetname='d%d' % p, at=(-210 + i * 70, 100, 0), angles='0 0 0', pose=str(p), body='0')
        t.wait(0.3)
        for p, seq in poses.items():
            t.snap('s', 'd%d' % p, 'seqname')
            t.expect.value('s', 'd%d' % p, 'seqname', seq)
    case.__name__ = 'pose_' + cls.replace('monster_', '')
    return case


def body_case(cls):
    bodies = BODIES[cls]

    @S.test('%s Body: each choice is the body it shows (%s)' % (cls, ', '.join(str(b) for b in bodies)), players=1,
            doc=DOC, covers=tuple('%s.body=%d' % (cls, b) for b in bodies))
    def case(t):
        for i, b in enumerate(bodies):
            t.point(cls, targetname='b%d' % b, at=(-150 + i * 100, 100, 0), angles='0 0 0', body=str(b))
        t.wait(0.3)
        for b in bodies:
            t.snap('s', 'b%d' % b, 'body')
            t.expect.value('s', 'b%d' % b, 'body', b)
    case.__name__ = 'body_' + cls.replace('monster_', '')
    return case


for _c in POSES:
    pose_case(_c)


@S.test('monster_scientist_dead Pose 3 (Hanging): scientist.mdl has no dead_hang, so it plays no pose animation (the '
        'FGD label says so)', players=1, doc=DOC, covers=('monster_scientist_dead.pose=3',))
def scientist_dead_hang(t):
    t.point('monster_scientist_dead', targetname='hang', at=(0, 100, 0), pose='3', body='0')
    t.wait(0.3)
    t.snap('s', 'hang', 'seqname')
    t.expect.value('s', 'hang', 'seqname', '')
for _c in BODIES:
    body_case(_c)


@S.test('monster_barney Gun State 0 (Gun Holstered): body 0; -1 (Random): holstered or drawn (0 or 1)', players=1,
        doc=DOC, covers=('monster_barney.bodystate=0', 'monster_barney.bodystate=-1'))
def barney_bodystate(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='hol', at=(-100, 100, 0), bodystate='0', spawnflags='16')
    t.point('monster_barney', targetname='rnd', at=(100, 100, 0), bodystate='-1', spawnflags='16')
    t.wait(0.3)
    t.snap('s', 'hol', 'body')
    t.snap('s', 'rnd', 'body')
    t.expect.value('s', 'hol', 'body', 0)
    t.expect.value('s', 'rnd', 'body', 1, op='<=')


@S.test('monster_zombie_grunt Weapons 8 (SPA): it carries the SPA (weapons 8)', players=1, doc=DOC,
        covers=('monster_zombie_grunt.weapons=8',))
def zombie_grunt_spa(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_zombie_grunt', targetname='zg', at=(0, 100, 0), weapons='8', spawnflags='16')
    t.wait(0.3)
    t.snap('s', 'zg', 'weapons')
    t.expect.value('s', 'zg', 'weapons', 8)
