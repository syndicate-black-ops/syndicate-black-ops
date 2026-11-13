"""Tier 5, final pass: monstermaker choices no test asserted (monstermaker.cpp): the Children's Trigger Condition
Hear World / Hear Player / Hear Combat (the child hears an env_soundmark of that type), and Children's Ignore
Friendly Damage No (0): the child takes a hit from an ally (a Fed's hit on a Fed child, CHGrunt's "Ff")."""

from etest import Suite

SUITE = Suite('fgd_maker_more', 'Tier 5: monstermaker hearing conditions, Ff No', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
HEAR = {7: ('Hear World', 2), 8: ('Hear Player', 4), 9: ('Hear Combat', 1)}


def hear_case(c):
    name, snd = HEAR[c]

    @S.test('monstermaker "childtriggercondition" %d (%s): its scientist child fires the Children\'s Trigger Target '
            'once it hears an env_soundmark of that type, not before' % (c, name), players=1, doc=DOC,
            covers=('monstermaker.childtriggercondition=%d' % c, 'monstermaker.childtriggertarget'))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.point('monstermaker', targetname='mk', at=(0, 150, 0), angles='0 270 0', monstertype='monster_scientist',
                monstercount='1', childtriggertarget='heard', childtriggercondition=str(c), spawnflags='0')
        t.point('info_target', targetname='heard')
        t.point('env_soundmark', targetname='snd', at=(0, 60, 36), type=str(snd), radius='500', duration='1')
        t.fire('mk')
        t.wait(1.5)
        t.mark('quiet')
        t.wait(1.0)
        t.mark('noise')
        t.fire('snd')
        t.wait(1.5)
        t.flags('player1', '-FL_NOTARGET')
        t.expect.not_fired('heard', after='quiet', within=(0, 0.9))
        t.expect.fired('heard', count=1, after='noise')
    case.__name__ = 'hear_%d' % c
    return case


for _c in HEAR:
    hear_case(_c)


@S.test('monstermaker "Ff" No (0): a hit from another Fed takes health from its Fed child', players=1, doc=DOC,
        covers=('monstermaker.ff=0',))
def ff_no(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monstermaker', targetname='mk', at=(-100, 150, 0), angles='0 270 0', monstertype='monster_human_grunt',
            monstercount='1', Ff='0', netname='kid', names=('netname',))
    t.point('monster_human_grunt', targetname='ally', at=(150, 150, 0), angles='0 180 0', spawnflags='16')
    t.fire('mk')
    t.wait(0.5)
    t.kv('kid', 'health', '100')
    t.snap('before', 'kid', 'health')
    t.damage('kid', 10, 2, attacker='ally')
    t.wait(0.1)
    t.snap('after', 'kid', 'health')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('before', 'kid', 'health', 100)
    t.expect.value('after', 'kid', 'health', 100, op='<')
