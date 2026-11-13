"""Tier 5, final pass: Monster key choices no class tested, on a Fed (monster_human_grunt, CBaseMonster code paths
every class that runs MonsterInit shares). "bloodcolor": a bullet hit in the chest bleeds that color (TE_BLOODSPRITE,
CBaseMonster::TraceAttack -> SpawnBlood -> UTIL_BloodDrips); -1 none; 0 the Fed's own red (247)."""

from etest import Suite

SUITE = Suite('fgd_shared_values', 'Tier 5: Monster key choices on a Fed', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
TE_BLOODSPRITE = 115
BLOOD = (-1, 0, 247, 195, 70, 192, 244)
G = 'monster_human_grunt'


@S.test('%s "bloodcolor" each choice: a bullet in the chest bleeds that color; Don\'t bleed (-1): no blood; Default '
        '(0): its own red (247)' % G, players=1, doc=DOC,
        covers=('%s.bloodcolor' % G,) + tuple('%s.bloodcolor=%d' % (G, v) for v in BLOOD))
def bloodcolor(t):
    t.flags('player1', '+FL_NOTARGET')
    for i, v in enumerate(BLOOD):
        # Prisoners (16): they stand and nobody fights them
        t.point(G, targetname='g%d' % i, at=(-210 + i * 70, 150, 0), angles='0 270 0', spawnflags='16',
                bloodcolor=str(v))
    t.wait(0.5)
    for i, v in enumerate(BLOOD):
        t.kv('g%d' % i, 'health', '100000')
        t.mark('b%d' % i)
        t.damage('g%d' % i, 5, 2, 'player1', hitgroup=2)
        t.wait(0.3)
    t.flags('player1', '-FL_NOTARGET')
    for i, v in enumerate(BLOOD):
        if v == -1:
            t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE}, count=0, after='b%d' % i, within=(0, 0.25))
        else:
            t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE, 'color': 247 if v == 0 else v}, min=1,
                         after='b%d' % i, within=(0, 0.25))


# field_of_view: a scientist (unnamed: it looks around at once) faces north; the player stands 140 away at an angle
# from its nose whose cosine is just under the view (out of it), then just inside it; TriggerCondition 10 See Player
# Unconditional fires once it sees him (the player 140 away). 0: its own view (VIEW_FIELD_WIDE, -0.7)
import math

FOV = (0, -1, -0.7, -0.2, 0.2, 0.5, 0.7, 0.9)
SC = 'monster_scientist'


def fov_case(f):
    out_c, in_c = (-0.999, -0.95) if f == -1 else ((-0.85, -0.6) if f == 0 else (max(f - 0.15, -0.99), min(f + 0.08, 0.99)))

    @S.test('%s "field_of_view" %g: the player just outside that view is not seen (See Player Unconditional does not '
            'fire), just inside it he is%s' % (SC, f, ' (0: its own)' if f == 0 else ''), players=1, doc=DOC,
            covers=('%s.field_of_view' % SC, '%s.field_of_view=%g' % (SC, f)))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (0, -230, 0), yaw=90)
        t.point(SC, at=(0, -100, 0), angles='0 90 0', spawnflags='2', TriggerTarget='seen', TriggerCondition='10',
                field_of_view=str(f), names=('TriggerTarget',))
        t.point('info_target', targetname='seen')
        t.wait(1.5)
        t.flags('player1', '-FL_NOTARGET')
        for label, c in (('out', out_c), ('in', in_c)):
            a = math.acos(c)
            t.mark(label)
            t.teleport('player1', (140 * math.sin(a), -100 + 140 * math.cos(a), 0))
            t.wait(2.0)
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-230, -230, 0))
        t.flags('player1', '-FL_NOTARGET')
        if f == -1:
            t.expect.used('seen', count=1, after='out')
        else:
            t.expect.not_used('seen', after='out', within=(0, 1.9))
            t.expect.used('seen', count=1, after='in')
    case.__name__ = 'fov_%d' % FOV.index(f)
    return case


for _f in FOV:
    fov_case(_f)
