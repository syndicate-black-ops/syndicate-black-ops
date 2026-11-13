"""Monster flags and the AI fixes of SBO 2.0 that need the real AI (sight, PVS, the sound list): Prisoner (16),
MonsterClip (4), Fade Corpse (512), SquadLeader (32); scientist voice pitch per head (b028c82e); func_tank picking the
best visible player among several (7cd935a0); hearing a player after other players died (03c2ff48, VS#1652)
(GAPS.md tier 2 "AI and monster fixes"). The stub-runner ones (sentry orientation, sentence on a button, maker slot,
dead pose clamp, triggered dying monster) are map-test cases 100l-100p and 102.

Monsters are referred to by class (@class[n], in map order: this suite's tests use each class in one test only, the
soundent test last): a named monster waits for its trigger (SCHED_WAIT_TRIGGER) and takes no enemy (HL SDK)."""

from etest import Suite

SUITE = Suite('monster_ai_fixes', 'Monster flags 4 / 16 / 32 / 512, scientist voice pitch, func_tank target choice, '
              'hearing after deaths', players=3)
S = SUITE

FL_MONSTERCLIP = 1 << 23
DMG_NEVERGIB = 4096
TANK_KEYS = dict(angles='0 90 0', yawrate='200', yawrange='90', pitchrate='100', pitchrange='30', barrel='24',
                 firerate='4', yawtolerance='15', pitchtolerance='5', persistence='1', bullet='1', bullet_damage='1')


def name(t, ref, local):
    """Names a monster after it spawned (a monster named in the map waits for its trigger)."""
    t.kv(ref, 'targetname', t.name(local))


def kill(t, *names):
    """The test's monsters die at its end, so they don't fight on while the next tests run. Not gibbed: a gibbed
    monster is removed, which would shift the @class[n] numbers of the later tests."""
    for n in names:
        t.damage(n, 5000, DMG_NEVERGIB)


@S.test('Prisoner (16): a grunt with the flag never takes the player in front of it as its enemy',
        players=1, covers=('monster_human_grunt:16',))
def prisoner(t):
    t.point('monster_human_grunt', at=(0, 150, 0), angles='0 270 0', spawnflags='16')
    name(t, '@monster_human_grunt[0]', 'pris')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(3.0)
    t.snap('s', 'pris', 'enemy')
    kill(t, 'pris')
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('s', 'pris', 'enemy', '')


@S.test('Prisoner control: the same grunt without the flag takes the player as its enemy (alone in its room: '
        'grunts side by side share what they see through their squad)', players=1, covers=('monster_human_grunt',))
def prisoner_control(t):
    t.point('monster_human_grunt', at=(0, 150, 0), angles='0 270 0', spawnflags='0')
    name(t, '@monster_human_grunt[1]', 'free')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(3.0)
    t.snap('s', 'free', 'enemy')
    kill(t, 'free')
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('s', 'free', 'enemy', 'player')


@S.test('MonsterClip (4): the monster gets FL_MONSTERCLIP (what func_monsterclip blocks); without the flag it does not',
        covers=('monster_alien_slave:4',))
def monsterclip(t):
    t.point('monster_alien_slave', at=(-100, 150, 0), angles='0 270 0', spawnflags='4')
    t.point('monster_alien_slave', at=(100, 150, 0), angles='0 270 0', spawnflags='0')
    name(t, '@monster_alien_slave[0]', 'clip')
    name(t, '@monster_alien_slave[1]', 'noclip')
    t.wait(0.5)
    t.snap('s', 'clip', 'flags')
    t.snap('s', 'noclip', 'flags')
    kill(t, 'clip', 'noclip')
    t.expect.value('s', 'clip', 'flags', FL_MONSTERCLIP, op='&')
    t.expect.value('s', 'noclip', 'flags', FL_MONSTERCLIP, op='!&')


@S.test('Fade Corpse (512): the killed monster fades out and is removed; without the flag its corpse stays',
        players=1, covers=('monster_zombie:512',))
def fade_corpse(t):
    t.point('monster_zombie', at=(-100, 150, 0), angles='0 270 0', spawnflags='512')
    t.point('monster_zombie', at=(100, 150, 0), angles='0 270 0', spawnflags='0')
    name(t, '@monster_zombie[0]', 'fade')
    name(t, '@monster_zombie[1]', 'stay')
    t.wait(0.5)
    t.mark('kill')
    for n in ('fade', 'stay'):
        t.kv(n, 'health', '10')         # whatever the monster scaling gave them
        t.damage(n, 100, DMG_NEVERGIB)  # killed, not gibbed (a gibbed monster is removed at once)
    t.wait(15.0)    # a player in the room: the dying monsters think as in a game
    # by class: the trace knows an entity by the name it spawned with (these were named afterwards)
    t.expect.removed('@monster_zombie', count=1, after='kill', within=(1.0, 15.0))


@S.test('SquadLeader (32) with a squad name (netname): the leader and its member form one squad of 2',
        covers=('monster_grunt_hunter:32', 'monster_grunt_hunter.netname'))
def squad_leader(t):
    t.point('monster_grunt_hunter', at=(-60, 150, 0), angles='0 270 0', spawnflags='32', netname='sq', names=())
    t.point('monster_grunt_hunter', at=(60, 150, 0), angles='0 270 0', netname='sq', names=())
    name(t, '@monster_grunt_hunter[0]', 'lead')
    name(t, '@monster_grunt_hunter[1]', 'memb')
    t.wait(1.5)
    t.snap('s', 'lead', 'squad')
    t.snap('s', 'memb', 'squad')
    kill(t, 'lead', 'memb')
    t.expect.value('s', 'lead', 'squad', 2)
    t.expect.value('s', 'memb', 'squad', 2)


@S.test('monster_scientist voice pitch by head (halflife-updated #156: four heads): body 3 (Slick) speaks at 100-103, '
        'body 2 (Luther) at 95-98 (1.38 took body 3 as head 0, Glasses: 105)',
        covers=('monster_scientist.body',))
def scientist_pitch(t):
    t.point('monster_scientist', targetname='slick', at=(-100, 150, 0), angles='0 270 0', body='3')
    t.point('monster_scientist', targetname='luther', at=(100, 150, 0), angles='0 270 0', body='2')
    t.point('scripted_sentence', targetname='say3', entity='slick', sentence='!SC_HELLO0', duration='1', spawnflags='4',
            names=('entity',))
    t.point('scripted_sentence', targetname='say2', entity='luther', sentence='!SC_HELLO0', duration='1',
            spawnflags='4', names=('entity',))
    t.wait(1.0)
    t.mark('go')
    t.fire('say3')
    t.fire('say2')
    t.wait(1.0)
    kill(t, 'slick', 'luther')
    t.expect.sound('SC_HELLO0', ent='slick', after='go', pitch=(100, 103))
    t.expect.sound('SC_HELLO0', ent='luther', after='go', pitch=(95, 98))


@S.test('func_tank (Active, automatic) with two players: the nearer one is behind a wall (in the PVS, not visible), '
        'the tank shoots the one it can see (7cd935a0: 1.38 asked the engine for one PVS client per think)',
        players=2, covers=('func_tank:1',))
def tank_visible_player(t):
    t.brush('func_tank', size=(32, 32, 32), at=(0, -150, 40), origin=True, spawnflags='1', targetname='tk', target='shot', **TANK_KEYS)
    t.point('info_target', targetname='shot', at=(200, -200, 0))
    t.brush('func_wall', size=(128, 16, 128), at=(-120, -60, 0), tex='666CEMENT')
    for p in ('player1', 'player2'):
        t.flags(p, '+FL_GODMODE')
    t.teleport('player1', (-120, -20, 0), yaw=270)      # behind the wall, 140 units from the tank
    t.teleport('player2', (120, 200, 0), yaw=270)       # in the open, 380 units away
    t.wait(0.3)
    t.mark('go')
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    for p in ('player1', 'player2'):
        t.flags(p, '-FL_GODMODE')
    t.expect.used('shot', after='go', min=2)
    t.expect.value('s', 'tk', 'angles.y', 71.1, tol=15)    # towards player2 (player1: 132.7)
    t.expect.not_damaged('player1', after='go')


@S.test('func_tank (Active, automatic) with two visible players: it keeps shooting the nearer one', players=2,
        covers=('func_tank:1',))
def tank_nearest_player(t):
    t.brush('func_tank', size=(32, 32, 32), at=(0, -150, 40), origin=True, spawnflags='1', targetname='tk', target='shot', **TANK_KEYS)
    t.point('info_target', targetname='shot', at=(200, -200, 0))
    for p in ('player1', 'player2'):
        t.flags(p, '+FL_GODMODE')
    t.teleport('player1', (-150, 200, 0), yaw=270)      # 400 units
    t.teleport('player2', (60, 0, 0), yaw=270)          # 160 units
    t.wait(0.3)
    t.mark('go')
    t.wait(3.0)
    t.snap('s', 'tk', 'angles')
    for p in ('player1', 'player2'):
        t.flags(p, '-FL_GODMODE')
    t.expect.used('shot', after='go', min=3)
    t.expect.value('s', 'tk', 'angles.y', 68.2, tol=15)    # towards player2 (player1: 113.2)
    t.expect.not_damaged('player1', after='go')


@S.test('Hearing after deaths (VS#1652): player2 and player3 die; a grunt behind a wall still hears player1 fire '
        '(1.38 cut the sound list at a dead player\'s slot, so NPCs stopped hearing the players of lower slots)',
        players=3, covers=('monster_human_grunt.triggercondition=8',))
def hear_after_deaths(t):
    t.brush('func_wall', size=(512, 16, 256), at=(0, 0, 0), tex='666CEMENT')
    t.point('monster_human_grunt', targetname='g8', at=(-120, 150, 0), angles='0 90 0', TriggerTarget='g8_fired',
            TriggerCondition='8', names=('TriggerTarget',))
    t.point('info_target', targetname='g8_fired', at=(200, -200, 0))
    # all three alive at the start, whatever the earlier tests did to them
    t.console('test_round restart')
    for p in ('player1', 'player2', 'player3'):
        t.console('wait_for alive %s 20' % t.ref(p))
    t.wait(2.5)                         # past the spawn protection
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player2', (100, -150, 0))
    t.teleport('player3', (150, -150, 0))
    t.wait(0.5)
    t.damage('player2', 1000, 0)
    t.damage('player3', 1000, 0)
    t.wait(1.0)
    t.mark('noise')
    t.teleport('player1', (0, -150, 0), yaw=270)
    t.look('player1', 0, 270)
    t.wait_frames(3)
    for _ in range(3):
        t.move('player1', 0, 0, 3, '+attack')
        t.wait(0.4)
    t.wait(1.5)
    t.flags('player1', '-FL_GODMODE')
    t.expect.fired('g8_fired', activator='g8', count=1, after='noise')


@S.test('"Ff" 1 (Ignore Friendly Damage) as the first key of a grunt and a dark_grunt: an ally\'s damage is ignored; '
        'without it the same damage hurts (64bc41c3: 1.38 reset Ff to 0 at every later key, so it worked only as the '
        'last key)', covers=('monster_human_grunt.Ff=1', 'dark_grunt.Ff=1'))
def ff_first_key(t):
    # the keys are written in this order: Ff first, the others after it
    for cls, x in (('monster_human_grunt', -150), ('dark_grunt', 150)):
        t.point(cls, Ff='1', targetname=cls[:4] + '_ff', at=(x, 150, 0), angles='0 270 0')
        t.point(cls, Ff='0', targetname=cls[:4] + '_no', at=(x, 50, 0), angles='0 270 0')
        t.point(cls, targetname=cls[:4] + '_ally', at=(x, -100, 0), angles='0 90 0')
    t.wait(0.5)
    t.snap('before', 'mons_ff', 'health')
    t.snap('before', 'mons_no', 'health')
    t.snap('before', 'dark_ff', 'health')
    t.snap('before', 'dark_no', 'health')
    for p in ('mons', 'dark'):
        t.damage(p + '_ff', 10, 2, p + '_ally')
        t.damage(p + '_no', 10, 2, p + '_ally')
    t.wait(0.2)
    t.snap('after', 'mons_ff', 'health')
    t.snap('after', 'mons_no', 'health')
    t.snap('after', 'dark_ff', 'health')
    t.snap('after', 'dark_no', 'health')
    for p in ('mons', 'dark'):
        kill(t, p + '_ff', p + '_no', p + '_ally')
    t.expect.same('before', 'after', 'mons_ff', 'health')
    t.expect.same('before', 'after', 'dark_ff', 'health')
    t.expect.value('after', 'mons_no', 'health', 0, op='>')
    t.expect.damaged('mons_no', attacker='mons_ally')
    t.expect.damaged('dark_no', attacker='dark_ally')
