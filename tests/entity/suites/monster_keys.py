"""The Featureful monster keys of base Monster (sdk/syndicate.fgd): classify, is_player_ally, field_of_view,
soundmask, gib_policy, each checked through the real AI (TriggerCondition 1 "See Player, Mad at Player" and 8 "Hear
Player" as the observers, the same setups as suite monster_conditions) or through the kill (gibbed: the monster is
removed with a gib sound; not gibbed: the corpse stays)."""

from etest import Suite

SUITE = Suite('monster_keys', 'Monster keys: classify, is_player_ally, field_of_view, soundmask, gib_policy')
S = SUITE
SUITE.server_args = ('+developer', '1')
NAMES = ('TriggerTarget',)
DMG_BULLET, DMG_ALWAYSGIB = 2, 8192


def grunt(t, name, at=(150, 60, 0), yaw=90, cond=1, **kv):
    # unnamed in the map (a named monster waits for a trigger and takes no enemy): found as @monster_human_grunt
    return t.point('monster_human_grunt', at=at, angles='0 %d 0' % yaw, TriggerTarget=name + '_fired',
                   TriggerCondition=str(cond), names=NAMES, **kv)


def sink(t, name):
    t.point('info_target', targetname=name + '_fired', at=(200, -200, 0))


def wall(t, y=0):
    t.brush('func_wall', size=(512, 16, 256), at=(0, y, 0), tex='666CEMENT')


def show(t, at=(150, 220, 0), yaw=270, seconds=2.0):
    t.flags('player1', '+FL_GODMODE')
    t.wait(1.0)
    t.mark('show')
    t.teleport('player1', at, yaw=yaw)
    t.wait(seconds)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.flags('player1', '-FL_GODMODE')


def sight_test(title, name, covers, fires, **kv):
    def f(t):
        wall(t, y=0)
        grunt(t, name, **kv)
        sink(t, name)
        show(t)
        if fires:
            t.expect.fired(name + '_fired', count=1, after='show')
        else:
            t.expect.not_fired(name + '_fired', after='show')
    f.__name__ = name
    S.test(title, players=1, covers=covers)(f)


sight_test('control: a grunt (keys left out) takes the player as its enemy when he comes into view (TriggerCondition 1 '
           'fires)', 'ctl', ('monster_human_grunt', 'monster_human_grunt.classify=0',
                             'monster_human_grunt.is_player_ally=0'), True, classify='0')
sight_test('is_player_ally 1: the grunt changes sides, the player is no enemy (TriggerCondition 1 never fires)',
           'ally', ('monster_human_grunt.is_player_ally=1',), False, is_player_ally='1')
sight_test('classify 11 Player ally: the player is no enemy', 'pally', ('monster_human_grunt.classify=11',), False,
           classify='11')
sight_test('classify -1 None: it fights nobody', 'none', ('monster_human_grunt.classify=-1',), False, classify='-1')
sight_test('classify 5 Alien military: still the players\' enemy', 'alien', ('monster_human_grunt.classify=5',), True,
           classify='5')
# field of view: the grunt faces east (yaw 0); the player appears straight north of it (90 degrees off its axis, dot 0)
sight_test('field_of_view default (humans: wide): a player 90 degrees to the side is seen', 'fovdef',
           ('monster_human_grunt.field_of_view=0',), True, yaw=0, field_of_view='0')
sight_test('field_of_view 0.5 Tunnel: a player 90 degrees to the side is not seen', 'fovtun',
           ('monster_human_grunt.field_of_view=0.5',), False, yaw=0, field_of_view='0.5')
sight_test('field_of_view -1 Full: a player 90 degrees to the side is seen', 'fovfull',
           ('monster_human_grunt.field_of_view=-1',), True, yaw=0, field_of_view='-1')


def hear_test(title, name, covers, fires, **kv):
    def f(t):
        wall(t, y=0)
        grunt(t, name, at=(0, 150, 0), cond=8, **kv)
        sink(t, name)
        t.flags('player1', '+FL_GODMODE')
        t.wait(0.5)
        t.mark('noise')
        t.teleport('player1', (0, -150, 0), yaw=270)
        t.look('player1', 0, 270)
        t.wait_frames(3)
        for _ in range(5):
            t.move('player1', 0, 0, 3, '+attack')
            t.wait(0.4)
        t.wait(2.5)
        t.flags('player1', '-FL_GODMODE')
        if fires:
            t.expect.fired(name + '_fired', count=1, after='noise')
        else:
            t.expect.not_fired(name + '_fired', after='noise')
    f.__name__ = name
    S.test(title, players=1, covers=covers)(f)


hear_test('soundmask default: a grunt behind a wall hears the player fire (TriggerCondition 8)', 'hdef',
          ('monster_human_grunt.soundmask=0',), True, soundmask='0')
hear_test('soundmask -1 No sounds (deaf): it does not hear the shots', 'hdeaf', ('monster_human_grunt.soundmask=-1',),
          False, soundmask='-1')
hear_test('soundmask 32 Only Danger: it does not hear the player\'s shots (combat / player sounds)', 'hdanger',
          ('monster_human_grunt.soundmask=32',), False, soundmask='32')


@S.test('soundmask as the monster listens (its sound mask read back): 1028 Default, except Player is the default '
        'without the player bit (4); 33 Combat and Danger; 37 Combat, Danger and Player; 127 every sound; -1 none',
        covers=('monster_human_grunt.soundmask=1028', 'monster_human_grunt.soundmask=33',
                'monster_human_grunt.soundmask=37', 'monster_human_grunt.soundmask=127',
                'monster_human_grunt.soundmask=-1'))
def soundmask_values(t):
    # named: they wait for a trigger, which doesn't change what they listen for
    rows = (('d', '0'), ('np', '1028'), ('c', '33'), ('cp', '37'), ('all', '127'), ('deaf', '-1'))
    for i, (n, m) in enumerate(rows):
        t.point('monster_human_grunt', targetname=n, at=(-200 + 80 * i, 150, 0), soundmask=m)
    t.wait(0.5)
    for n, _m in rows:
        t.snap('s', n, 'soundmask')
    t.expect.value('s', 'd', 'soundmask', 4, op='&')
    t.expect.value('s', 'np', 'soundmask', 4, op='!&')
    t.expect.value('s', 'np', 'soundmask', 1, op='&')          # combat kept
    t.expect.value('s', 'c', 'soundmask', 33)
    t.expect.value('s', 'cp', 'soundmask', 37)
    t.expect.value('s', 'all', 'soundmask', 127)
    t.expect.value('s', 'deaf', 'soundmask', 0)


@S.test('gib_policy: 1 Prefer gibbing gibs a scientist killed by a small bullet hit; 0 (control) leaves a corpse; 2 '
        'Prefer not to gib keeps the corpse after a huge hit; an always-gib hit still gibs it', doc=None,
        covers=('monster_scientist', 'monster_scientist.gib_policy=0', 'monster_scientist.gib_policy=1',
                'monster_scientist.gib_policy=2'))
def gib_policy(t):
    for n, x, pol in (('g0', -180, '0'), ('g1', -60, '1'), ('g2', 60, '2'), ('g2a', 180, '2')):
        t.point('monster_scientist', targetname=n, at=(x, 150, 0), angles='0 270 0', gib_policy=pol)
    t.wait(0.5)
    for n in ('g0', 'g1', 'g2', 'g2a'):
        t.kv(n, 'health', '20')         # the skill's health is set at spawn: 25 damage kills, -5 does not gib
    t.mark('hit')
    t.damage('g0', 25, DMG_BULLET)
    t.damage('g1', 25, DMG_BULLET)
    t.damage('g2', 5000, DMG_BULLET)
    t.damage('g2a', 25, DMG_BULLET | DMG_ALWAYSGIB)
    t.wait(0.5)
    for n in ('g0', 'g2'):
        t.snap('s', n, 'effects', 'deadflag')
    # gibbed: the gib sound and the body removed at once; not gibbed: a corpse (dead, drawn) stays
    t.expect.value('s', 'g0', 'deadflag', 0, op='>')
    t.expect.value('s', 'g0', 'effects', 128, op='!&')
    t.expect.value('s', 'g2', 'deadflag', 0, op='>')
    t.expect.value('s', 'g2', 'effects', 128, op='!&')
    t.expect.removed('g1', after='hit', within=(0, 0.3))
    t.expect.removed('g2a', after='hit', within=(0, 0.3))
    t.expect.sound('common/bodysplat.wav', ent='g1', after='hit', min=1)


# ---------------------------------------------------------------------------------------------- monstermaker type
@S.test('monstermaker without a monster type (no key, or an empty one): one console warning with its name and origin, '
        'the maker removes itself, the map runs (1.38 stopped the server: "Spawned a NULL entity!")',
        doc='FEATUREFUL_OPTIONS.md', covers=('monstermaker.monstertype',))
def maker_no_monstertype(t):
    for n, x, mt in (('mk_none', -120, None), ('mk_empty', 120, '')):
        t.point('monstermaker', targetname=n, at=(x, 0, 0), monstertype=mt, monstercount='1',
                m_imaxlivechildren='1', delay='0.1', netname='kid_' + n, names=('netname',))
    t.wait(0.5)
    t.fire('mk_none')
    t.fire('mk_empty')
    t.wait(0.5)
    for n, x in (('mk_none', -120), ('mk_empty', 120)):
        o = t._abs((x, 0, 0))
        t.expect.console('monstermaker "%s" at %.0f %.0f %.0f has no monster type (key "monstertype"): removed'
                         % (t.name(n), o[0], o[1], o[2]))
        t.expect.absent(n)
        t.expect.absent('kid_' + n)
