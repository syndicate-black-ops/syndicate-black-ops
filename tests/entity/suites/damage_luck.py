"""SBO's random hits (combat.cpp CBaseMonster::TakeDamage, 1.38's): each hit has a 1 in 100 chance each of 2x
(perfect), 1.5x (critical), 0.5x (fail) and 0.25x (very bad). The test layer's test_shotluck 0 (t.exact_damage) turns
them off; every test starts with them off (its begin mark), and t.random_hits (test_shotluck 1) turns them back on.
This suite shows they are there when on and gone when off."""

from etest import Suite

SUITE = Suite('damage_luck', 'Random perfect / failed hits, and the test layer switch', players=1)
S = SUITE
DOC = 'dev/ENTITY_TESTS.md'
HITS = 300


def grunt(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_human_grunt', targetname='g', at=(0, 150, 0), angles='0 270 0', spawnflags='16')
    t.wait(0.5)
    t.kv('g', 'health', '100000')


@S.test('of 300 hits of 10 on a Fed, some take 20, 15, 5 or 2.5 (perfect, critical, failed, very bad: about 12 of '
        '300; none at all: about 1 in 200000)', players=1, doc=DOC, covers=('monster_human_grunt',))
def luck_on(t):
    grunt(t)
    t.random_hits()
    labels = []
    for k in range(HITS):
        t.kv('g', 'health', '1000')
        t.damage('g', 10)
        t.snap('h%d' % k, 'g', 'health')
        labels.append('h%d' % k)
    t.flags('player1', '-FL_NOTARGET')
    # (their sum can still come to exactly 3000: the health after each hit shows them)
    t.expect.value_any(labels, 'g', 'health', [980, 985, 995, 997.5])


@S.test('with test_shotluck 0 (every test\'s start) the same 300 hits take exactly 3000', players=1, doc=DOC, covers=('monster_human_grunt',))
def luck_off(t):
    grunt(t)
    t.exact_damage()
    for _ in range(HITS):
        t.damage('g', 10)
    t.wait(0.1)
    t.snap('s', 'g', 'health')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('s', 'g', 'health', 100000 - 10 * HITS)
