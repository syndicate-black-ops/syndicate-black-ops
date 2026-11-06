"""monster_cyberminotor (Minos) AI, src/dlls/minotaure.cpp (fix/minos-ai; the owner's minos_fix plugin ported):
- gun position: GetGunPosition is the model's muzzle (attachment 0) at any yaw (1.38: a world-axis offset, right
  only at yaw 90), so his shells, bullets and CheckRangeAttack1's trace start at his gun;
- targeting: BestVisibleEnemy picks among the entities he can see (FVisible); seeing nobody he keeps the last player
  he saw for 5 s, then the base choice stands (1.38: a grunt behind walls over a player in front of him);
- damage conditions: RunAI clears bits_COND_LIGHT_DAMAGE / HEAVY_DAMAGE after each think (1.38: never; they stayed
  set until the next schedule change).

Look() puts every monster and player within 12000 units in his list, through walls, so a Minos or grunt left alive
would see into the other tests' rooms: the targeting and damage tests make theirs with monstermakers at their start
and remove them at their end; the gun-position Minos are prisoners (nobody sees them, they see nobody)."""

from etest import Suite

SUITE = Suite('minos', 'monster_cyberminotor: gun position, targeting, shooting under fire (fix/minos-ai)')
S = SUITE
ROOM = (512, 512, 512)
SF_PRISONER = 16
DMG_BULLET = 2


def tough(t, player='player1'):
    """A test player Minos doesn't kill (a dead last player would end the co-op round)."""
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def maker(t, name, monster, at, yaw):
    """A monstermaker making one `monster` named `name` at `at`, facing `yaw`, when the test fires it; and the
    relay that removes it at the end (kill_<name>). A named monster waits for a trigger (StartMonster's
    SCHED_WAIT_TRIGGER), so it is used once to wake it; 100000 health, so nobody dies during the test."""
    t.point('monstermaker', at=at, targetname='make_' + name, monstertype=monster, monstercount='1',
            m_imaxlivechildren='1', netname=name, angles='0 %d 0' % yaw, names=('netname',))
    t.point('trigger_relay', targetname='kill_' + name, killtarget=name)
    t.fire('make_' + name)


def wake(t, *names):
    t.wait(0.3)
    for n in names:
        t.kv(n, 'health', '100000')
        t.fire(n, use='on')


def remove(t, *names):
    for n in names:
        t.fire('kill_' + n)
    t.wait(0.2)


@S.test('gun position: at yaw 0, 90, 180 and 270 his gun (shot, shell and trace start) is at the muzzle', size=ROOM)
def gun_at_muzzle(t):
    yaws = (0, 90, 180, 270)
    for i, yaw in enumerate(yaws):
        x, y = ((-150, -150), (150, -150), (-150, 150), (150, 150))[i]
        t.point('monster_cyberminotor', at=(x, y, 0), targetname='minos%d' % yaw, angles='0 %d 0' % yaw,
                spawnflags=str(SF_PRISONER))
    t.wait(1.0)
    for yaw in yaws:
        t.snap('s', 'minos%d' % yaw, 'origin', 'angles', 'gun', 'gun_dist', 'gun_muzzle')
    for yaw in yaws:
        # 1.38: about 0 at yaw 90, 260-380 at the other yaws
        t.expect.value('s', 'minos%d' % yaw, 'gun_muzzle', 16, op='<')
        t.expect.value('s', 'minos%d' % yaw, 'gun_dist', 100, op='>')
        t.expect.value('s', 'minos%d' % yaw, 'gun_dist', 400, op='<=')


def walled(t):
    """A floor-to-ceiling wall across the room's north part: a grunt behind it can't be seen from the south part."""
    t.brush('func_wall', size=(512, 16, 512), at=(0, 140, 0))


@S.test('targeting: a monster_human_grunt behind a wall, a player in view: Minos fights the player', players=1,
        size=ROOM)
def targets_player_in_view(t):
    walled(t)
    tough(t)
    t.teleport('player1', (150, -150, 0), yaw=180)
    maker(t, 'minos', 'monster_cyberminotor', (-120, -60, 0), 0)
    maker(t, 'grunt', 'monster_human_grunt', (0, 200, 0), 270)
    wake(t, 'minos', 'grunt')
    t.wait(2.0)
    t.snap('a', 'minos', 'enemy')
    t.wait(2.0)
    t.snap('b', 'minos', 'enemy')
    t.expect.value('a', 'minos', 'enemy', 'player')     # 1.38: monster_human_grunt (his nemesis, through the wall)
    t.expect.value('b', 'minos', 'enemy', 'player')
    remove(t, 'minos', 'grunt')


@S.test('targeting: the player hides: Minos keeps him about 5 s, then the game\'s own choice (the grunt)', players=1,
        size=ROOM)
def keeps_last_player(t):
    walled(t)
    # the hiding place: east of a second wall, sealed by the first
    t.brush('func_wall', size=(16, 396, 512), at=(100, -58, 0))
    tough(t)
    t.teleport('player1', (-150, -230, 0), yaw=90)
    maker(t, 'minos', 'monster_cyberminotor', (-150, 20, 0), 270)
    maker(t, 'grunt', 'monster_human_grunt', (0, 200, 0), 270)
    wake(t, 'minos', 'grunt')
    t.wait(2.0)
    t.snap('seen', 'minos', 'enemy')
    t.teleport('player1', (180, -150, 0), yaw=180)
    t.wait(3.0)
    t.snap('kept', 'minos', 'enemy')
    t.wait(6.0)
    t.snap('after', 'minos', 'enemy')
    t.expect.value('seen', 'minos', 'enemy', 'player')  # 1.38: monster_human_grunt from the start
    t.expect.value('kept', 'minos', 'enemy', 'player')
    t.expect.value('after', 'minos', 'enemy', 'monster_human_grunt')
    remove(t, 'minos', 'grunt')


@S.test('targeting (control): a grunt in view and no player in the room: Minos fights the grunt', size=ROOM)
def targets_grunt_in_view(t):
    maker(t, 'minos', 'monster_cyberminotor', (-150, -150, 0), 45)
    maker(t, 'grunt', 'monster_human_grunt', (150, 150, 0), 225)
    wake(t, 'minos', 'grunt')
    t.wait(2.0)
    t.snap('a', 'minos', 'enemy')
    t.expect.value('a', 'minos', 'enemy', 'monster_human_grunt')
    remove(t, 'minos', 'grunt')


@S.test('under fire: hit every 0.1 s (one hit per think), Minos still fires at the player within 8 s', players=1,
        size=ROOM)
def fires_under_fire(t):
    tough(t)
    # yaw 90 toward the player: the 1.38 gun offset is right at this heading. A regression guard, not a test of the
    # 1.38 bug: ChangeSchedule clears all conditions, so the damage bits rarely outlive a schedule change, and the
    # line-removed build fired here too in most runs (once none in 8 s; 28 shots from 5.5 s against 51 from 3.2 s
    # with the line). damage_conditions_cleared tests the bits themselves
    t.teleport('player1', (0, 150, 0), yaw=270)
    maker(t, 'minos', 'monster_cyberminotor', (0, -150, 0), 90)
    wake(t, 'minos')
    t.mark('fire')
    for _ in range(80):
        t.damage('minos', 5, DMG_BULLET, 'player1')
        t.wait(0.1)
    t.expect.sound('gat_fire1.wav', ent='minos', min=1, after='fire')
    t.expect.damaged('player1', attacker='minos', after='fire')
    remove(t, 'minos')




@S.test('damage conditions: cleared after each think, also while he flinches (CBaseMonster::RunAI)', players=1,
        size=ROOM)
def damage_conditions_cleared(t):
    # the first hit interrupts his range attack and he flinches (SCHED_SMALL_FLINCH, which damage does not
    # interrupt, about 3 s); the hits during the flinch set the bits again. 1.38 left them set until the next
    # schedule change (ChangeSchedule clears all conditions); each snapshot is two thinks after the last hit
    tough(t)
    t.teleport('player1', (0, 150, 0), yaw=270)
    maker(t, 'minos', 'monster_cyberminotor', (0, -150, 0), 90)
    wake(t, 'minos')
    t.wait(0.2)
    for i in range(12):
        t.damage('minos', 5, DMG_BULLET, 'player1')
        t.wait(0.25)
        t.snap('s%d' % i, 'minos', 'schedule', 'damage_cond')
    for i in range(12):
        t.expect.value('s%d' % i, 'minos', 'damage_cond', 0)
    remove(t, 'minos')
