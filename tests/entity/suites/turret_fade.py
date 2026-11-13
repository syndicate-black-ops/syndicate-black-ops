"""Monster flag 512 "Fade Corpse" on the turrets (monster_turret, monster_miniturret, monster_sentry): a destroyed
turret with the flag fades its wreck out and is removed once its death smoke ends, like other monsters' corpses; the
same turret without the flag keeps its wreck (1.38 kept every turret wreck). A monstermaker's turret fades too, as the
maker's other children do (Featureful)."""

from etest import Suite

SUITE = Suite('turret_fade', 'Fade Corpse (512) on monster_turret, monster_miniturret, monster_sentry')
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
SF_FADECORPSE = 512
# The death think runs about 5 s (smoke and sparks, the die sequence), then the fade takes about 4 s
DEATH_AND_FADE = 14.0


def fade_pair(t, classname, **kv):
    t.point(classname, targetname='fade', at=(-120, 0, 0), spawnflags=str(SF_FADECORPSE), **kv)
    t.point(classname, targetname='stay', at=(120, 0, 0), spawnflags='0', **kv)
    t.wait(0.5)
    t.mark('kill')
    t.damage('fade', 100000)
    t.damage('stay', 100000)
    t.wait(DEATH_AND_FADE)
    t.snap('end', 'stay', 'deadflag')
    t.expect.removed('fade', after='kill', within=(5.0, DEATH_AND_FADE), count=1)
    t.expect.not_removed('stay')
    t.expect.value('end', 'stay', 'deadflag', 2)


@S.test('monster_turret flag 512: the destroyed turret fades out and is removed; without the flag the wreck stays',
        doc=DOC, covers=('monster_turret', 'monster_turret:512'))
def turret(t):
    fade_pair(t, 'monster_turret')


@S.test('monster_miniturret flag 512: the destroyed turret fades out and is removed; without the flag the wreck '
        'stays', doc=DOC, covers=('monster_miniturret', 'monster_miniturret:512'))
def miniturret(t):
    fade_pair(t, 'monster_miniturret')


@S.test('monster_sentry flag 512: the destroyed sentry fades out and is removed; without the flag the wreck stays',
        doc=DOC, covers=('monster_sentry', 'monster_sentry:512'))
def sentry(t):
    fade_pair(t, 'monster_sentry')


@S.test('monstermaker of monster_sentry: the destroyed child of a maker of 2 fades out (a maker\'s children fade); '
        'the child of a maker of 1 (Number of Monsters 1: its children keep their corpse) keeps its wreck', doc=DOC,
        covers=('monstermaker.monstertype=monster_sentry', 'monstermaker.monstercount'))
def maker_child(t):
    t.point('monstermaker', targetname='mkf', at=(-120, 0, 0), monstertype='monster_sentry', monstercount='2',
            m_imaxlivechildren='1', netname='fade', names=('netname',))
    t.point('monstermaker', targetname='mks', at=(120, 0, 0), monstertype='monster_sentry', monstercount='1',
            netname='stay', names=('netname',))
    t.fire('mkf')
    t.fire('mks')
    t.wait(1.0)
    t.mark('kill')
    t.damage('fade', 100000)
    t.damage('stay', 100000)
    t.wait(DEATH_AND_FADE)
    t.expect.removed('fade', after='kill', within=(5.0, DEATH_AND_FADE), count=1)
    t.expect.not_removed('stay')
