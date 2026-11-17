"""Water splashes of server-fired bullets (water_hit.cpp): a FireBullets call of a monster, turret or mounted gun whose
bullets cross a water surface sends one "WaterHit" message (byte count, at most 4 entry points) to the PVS of the
first entry point; with no water on the way it sends nothing. test_bulletbench times FireBullets with the water check
off and on (its console line has the numbers)."""

from etest import Suite

SUITE = Suite('water_hit', 'Water splashes of server-fired bullets: the WaterHit message and its cost', players=1)
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
FEDS = 10


def pool(t, depth=96, **kv):
    # water over the south half of the room; the shooters stand on the dry north half
    t.brush('func_water', size=(512, 256, depth), at=(0, -128, 0), skin='-3', **kv)


def fight(t, water):
    # the shooters stand on a ledge 128 high and shoot down at a player who stands waist deep in the water: a monster
    # never sees a player whose head is under water (CBaseEntity::FVisible), so his head stays above it
    if water:
        pool(t, 48)
    t.brush('func_wall', size=(512, 160, 128), at=(0, 176, 0))
    t.flags('player1', '+FL_GODMODE')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=90)
    for i in range(FEDS):
        t.point('monster_human_grunt', targetname='fed%d' % i, at=(-225 + 50 * i, 220, 128), angles='0 270 0')
    t.point('monster_sentry', targetname='sentry', at=(0, 130, 128), angles='0 270 0', spawnflags='32')
    t.wait(2.5)     # past the round start's spawn protection
    t.snap('in', 'player1', 'waterlevel')
    t.flags('player1', '-FL_NOTARGET')
    t.mark('fire')
    t.wait(10.0)
    t.flags('player1', '+FL_NOTARGET')
    t.expect.damaged('player1', after='fire', min=1)     # they did shoot at him


@S.test('10 Feds and a sentry shoot a player standing in water: WaterHit messages, at most 4 points each',
        players=1, size=(512, 512, 256), doc=DOC)
def fight_water(t):
    fight(t, True)
    t.expect.value('in', 'player1', 'waterlevel', 1, op='>=')
    t.expect.msg('WaterHit', after='fire', min=1)


@S.test('the same fight with no water in the room: no WaterHit message', players=1, size=(512, 512, 256), doc=DOC)
def fight_dry(t):
    fight(t, False)
    t.expect.no_msg('WaterHit')


def bench(t, water, shots):
    if water:
        pool(t)
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (200, 200, 0), yaw=270)
    t.point('info_target', targetname='gun', at=(0, 200, 48))
    t.point('info_target', targetname='aim', at=(0, -150, 8))
    t.wait(0.5)
    t.mark('bench')
    t.console('test_bulletbench %s %s 2000 %d' % (t.name('gun'), t.name('aim'), shots))
    t.wait(0.2)
    t.expect.console('test_bulletbench: 2000 calls of %d shots' % shots)
    t.expect.console('(crosses water)' if water else '(no water)')


@S.test('test_bulletbench into water, 1 bullet per call', players=1, size=(512, 512, 256), doc=DOC)
def bench_water_1(t):
    bench(t, True, 1)
    t.expect.msg('WaterHit', after='bench', min=1000)


@S.test('test_bulletbench into water, 8 bullets per call', players=1, size=(512, 512, 256), doc=DOC)
def bench_water_8(t):
    bench(t, True, 8)
    t.expect.msg('WaterHit', after='bench', min=1000)


@S.test('test_bulletbench with no water, 8 bullets per call: no WaterHit message', players=1, size=(512, 512, 256),
        doc=DOC)
def bench_dry_8(t):
    bench(t, False, 8)
    t.expect.no_msg('WaterHit')


@S.test('a func_water that moved away: shots into the place it left make no splash; shots into its new place do',
        players=1, size=(512, 512, 256), doc=DOC)
def moved_water(t):
    # a func_water is a door: fired, it slides north (yaw 90) by its own length, under the shooter's ledge
    pool(t, 48, targetname='pool', angles='0 90 0', speed='10000', wait='-1')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (200, 200, 0), yaw=270)
    t.point('info_target', targetname='gun', at=(0, -50, 160))
    t.point('info_target', targetname='old', at=(0, -150, 8))
    t.point('info_target', targetname='new', at=(0, 150, 8))
    t.wait(0.5)
    t.mark('before')
    t.console('test_bulletbench %s %s 100 1' % (t.name('gun'), t.name('old')))
    t.wait(0.2)
    t.mark('fired')
    t.fire('pool')
    t.wait(1.0)
    t.snap('moved', 'pool', 'origin')
    t.mark('old')
    t.console('test_bulletbench %s %s 100 1' % (t.name('gun'), t.name('old')))
    t.wait(0.2)
    t.mark('new')
    t.console('test_bulletbench %s %s 100 1' % (t.name('gun'), t.name('new')))
    t.wait(0.2)
    t.expect.value('moved', 'pool', 'origin.y', 200, op='>')
    t.expect.msg('WaterHit', after='before', within=(0, 0.2), min=50)
    t.expect.no_msg('WaterHit', after='old', within=(0, 0.2))
    t.expect.msg('WaterHit', after='new', min=50)
