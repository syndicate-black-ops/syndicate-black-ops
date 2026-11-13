"""Monster flag 512 "Fade Corpse" on monster_ichthyosaur: a killed ichthyosaur with the flag fades its corpse out and
is removed once its death animation ends, like other monsters' corpses; the same monster without the flag keeps its
corpse. Its own TASK_DIE skipped CBaseMonster's fade (1.38 kept every ichthyosaur corpse). A monstermaker's
ichthyosaur fades too, as the maker's other children do."""

from etest import Suite

SUITE = Suite('ichthyosaur_fade', 'Fade Corpse (512) on monster_ichthyosaur')
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
SF_FADECORPSE = 512
DMG_NEVERGIB = 4096
# the die sequence runs a few seconds, then the fade takes about 4 s
DEATH_AND_FADE = 14.0


@S.test('monster_ichthyosaur flag 512: the killed ichthyosaur fades out and is removed; without the flag the corpse '
        'stays (deadflag 2)', doc=DOC, covers=('monster_ichthyosaur', 'monster_ichthyosaur:512'))
def ichthyosaur(t):
    t.point('monster_ichthyosaur', targetname='fade', at=(-200, 0, 64), spawnflags=str(SF_FADECORPSE))
    t.point('monster_ichthyosaur', targetname='stay', at=(200, 0, 64), spawnflags='0')
    t.wait(0.5)
    t.mark('kill')
    t.damage('fade', 100000, dmgtype=DMG_NEVERGIB)
    t.damage('stay', 100000, dmgtype=DMG_NEVERGIB)
    t.wait(DEATH_AND_FADE)
    t.snap('end', 'stay', 'deadflag')
    t.expect.removed('fade', after='kill', within=(0.5, DEATH_AND_FADE), count=1)
    t.expect.not_removed('stay')
    t.expect.value('end', 'stay', 'deadflag', 2)


@S.test('monstermaker of monster_ichthyosaur: the killed child of a maker of 2 fades out (a maker\'s children fade); '
        'the child of a maker of 1 keeps its corpse', doc=DOC,
        covers=('monstermaker.monstertype=monster_ichthyosaur', 'monstermaker.monstercount'))
def maker_child(t):
    t.point('monstermaker', targetname='mkf', at=(-200, 0, 64), monstertype='monster_ichthyosaur', monstercount='2',
            m_imaxlivechildren='1', netname='fade', names=('netname',))
    t.point('monstermaker', targetname='mks', at=(200, 0, 64), monstertype='monster_ichthyosaur', monstercount='1',
            netname='stay', names=('netname',))
    t.fire('mkf')
    t.fire('mks')
    t.wait(1.0)
    t.mark('kill')
    t.damage('fade', 100000, dmgtype=DMG_NEVERGIB)
    t.damage('stay', 100000, dmgtype=DMG_NEVERGIB)
    t.wait(DEATH_AND_FADE)
    t.expect.removed('fade', after='kill', within=(0.5, DEATH_AND_FADE), count=1)
    t.expect.not_removed('stay')
