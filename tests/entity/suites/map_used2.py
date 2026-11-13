"""Tier 4 batch 2: map-used items that need a real server: turret Autostart / Start Inactive, monster Ff on the classes
the maps set it on, func_pushable Friction and Buoyancy, func_monsterclip, func_door / func_door_rotating 512
"Monsters Can't". Each test checks the FGD's description (sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('map_used2', 'Tier 4 batch 2: turret autostart, Ff, func_pushable friction / buoyancy, func_monsterclip, '
              'door Monsters Can\'t')
S = SUITE

SF_MONSTERCLIP = 4
SF_PRISONER = 16
CONTENTS_WATER = -3


def node_grid(t, xs, ys):
    for x in xs:
        for y in ys:
            t.point('info_node', at=(x, y, 16))


# ------------------------------------------------------------------------------------------------ turrets

TURRETS = ('monster_turret', 'monster_miniturret', 'monster_sentry')


def turret_in_view(t, cls, **kv):
    t.point(cls, targetname='tur', at=(0, 150, 0), angles='0 270 0', **kv)
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.flags('player1', '+FL_GODMODE')


def make_autostart_test(cls):
    def autostart(t):
        turret_in_view(t, cls, spawnflags='32')
        t.mark('start')
        t.wait(4.0)
        t.flags('player1', '-FL_GODMODE')
        t.expect.sound('turret/tu_deploy', ent='tur', after='start', within=(0, 4.0))

    autostart.__name__ = 'autostart_' + cls
    S.test('%s flag 32 Autostart: with a player in view it deploys by itself (no trigger)' % cls, players=1,
           covers=(cls + ':32',))(autostart)


def make_no_autostart_test(cls, flags, covers):
    def stays(t):
        turret_in_view(t, cls, spawnflags=str(flags))
        t.mark('start')
        t.wait(3.0)
        t.mark('fire')
        t.fire('tur')
        t.wait(3.0)
        t.flags('player1', '-FL_GODMODE')
        t.expect.sound('turret/tu_deploy', ent='tur', count=0, after='start', within=(0, 3.0))
        t.expect.sound('turret/tu_deploy', ent='tur', after='fire', within=(0, 3.0))

    stays.__name__ = 'no_autostart_%s_%d' % (cls, flags)
    what = 'flags 32 + 64 Start Inactive' if flags else 'without flag 32'
    S.test('%s %s: a player in view does not wake it; triggered, it deploys' % (cls, what), players=1,
           covers=covers)(stays)


for _c in TURRETS:
    make_autostart_test(_c)
make_no_autostart_test('monster_turret', 0, ('monster_turret',))
make_no_autostart_test('monster_turret', 96, ('monster_turret:64',))
make_no_autostart_test('monster_miniturret', 96, ('monster_miniturret:64',))


# ------------------------------------------------------------------------------------------------ Ff

def ff_test(t, cls):
    """As suite stock_keys: an ally's hit is ignored with Ff 1, taken with Ff 0 (health set to 100 first)."""
    t.point(cls, targetname='ff1', Ff='1', at=(-150, 0, 0), angles='0 0 0')
    t.point(cls, targetname='ff0', Ff='0', at=(0, 0, 0), angles='0 0 0')
    t.point(cls, targetname='ally', at=(150, 0, 0), angles='0 180 0')
    t.wait(0.5)
    t.kv('ff1', 'health', '100')
    t.kv('ff0', 'health', '100')
    t.snap('before', 'ff1', 'health')
    t.snap('before', 'ff0', 'health')
    t.damage('ff1', 10, 2, attacker='ally')        # DMG_BULLET
    t.damage('ff0', 10, 2, attacker='ally')
    t.wait(0.1)
    t.snap('after', 'ff1', 'health')
    t.snap('after', 'ff0', 'health')
    t.expect.same('before', 'after', 'ff1', 'health')
    t.expect.value('after', 'ff0', 'health', 100, op='<')     # turrets take a tenth of a bullet's damage


def make_ff_test(cls):
    def ff(t):
        ff_test(t, cls)

    ff.__name__ = 'ff_' + cls
    S.test('%s Ff 1 "Ignore Friendly Damage": a hit from another %s (an ally) takes no health; with Ff 0 the same hit '
           'takes health' % (cls, cls), covers=(cls + '.ff', cls + '.ff=0', cls + '.ff=1'))(ff)


for _c in ('monster_grunt_hunter', 'monster_human_assassin', 'monster_sentry', 'monster_turret', 'monster_miniturret'):
    make_ff_test(_c)


# ------------------------------------------------------------------------------------------------ func_pushable

@S.test('func_pushable Friction (0-400): its top speed is 400 - friction: pushed by a walking player, friction 300 '
        'moves at most 100 units/s, friction 50 farther', players=1,
        covers=('func_pushable.friction',))
def pushable_friction(t):
    t.brush('func_pushable', size=(32, 32, 32), at=(-120, -100, 0), origin=True, targetname='slow', friction='300')
    t.brush('func_pushable', size=(32, 32, 32), at=(120, -100, 0), origin=True, targetname='fast', friction='50')
    t.point('info_target', targetname='slow0', at=(-120, -100, 16))
    t.point('info_target', targetname='fast0', at=(120, -100, 16))
    t.wait(1.0)
    for name, x in (('slow', -120), ('fast', 120)):
        t.teleport('player1', (x, -160, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait(0.3)
        t.walk('player1', 400, 0, 1.5)
        t.wait(0.1)
        t.snap('pushed', name, 'origin')
        t.snap('pushed', name + '0', 'origin')
    # friction 300: at most 100 units/s (pushed for 1.5 s: 150 at most); friction 50: it keeps up with the player
    t.expect.distance('pushed', 'slow', 'slow0', 20, 155)
    t.expect.distance('pushed', 'fast', 'fast0', 175, 500)


@S.test('func_pushable Buoyancy: in a pool, a pushable with buoyancy floats up (it ends higher than the same one '
        'with 0, which sinks to the bottom)', covers=('func_pushable.buoyancy',))
def pushable_buoyancy(t):
    t.brush('func_water', size=(400, 200, 300), at=(0, 0, 0), skin=str(CONTENTS_WATER))
    t.brush('func_pushable', size=(64, 64, 64), at=(-100, 0, 100), origin=True, targetname='floats', buoyancy='400')
    t.brush('func_pushable', size=(64, 64, 64), at=(100, 0, 100), origin=True, targetname='sinks', buoyancy='0')
    t.wait(4.0)
    t.snap('end', 'floats', 'origin')
    t.snap('end', 'sinks', 'origin')
    t.expect.value('end', 'sinks', 'origin.z', t._abs((0, 0, 40))[2], op='<=')
    t.expect.value('end', 'floats', 'origin.z', t._abs((0, 0, 150))[2], op='>=')


# ------------------------------------------------------------------------------------------------ func_monsterclip

def clip_room(t, monster_flags):
    """A func_monsterclip wall across the room; a scientist told to run to a mark on the other side."""
    t.brush('func_monsterclip', size=(16, 512, 200), at=(0, 0, 0))
    node_grid(t, (-180, -60, 60, 180), (-150, 0, 150))
    t.point('monster_scientist', targetname='sci', at=(-150, 0, 0), angles='0 0 0',
            spawnflags=str(SF_PRISONER | monster_flags))
    t.point('info_target', targetname='mark', at=(150, 0, 0))
    t.point('scripted_sequence', targetname='ss', at=(150, 0, 0), angles='0 0 0', m_iszEntity='sci',
            m_fMoveTo='2', m_iszPlay='idle1', names=('m_iszEntity',))
    t.teleport('player1', (-200, 200, 0))
    t.flags('player1', '+FL_NOTARGET')
    t.wait(1.0)
    t.fire('ss')
    t.wait(6.0)
    t.snap('end', 'sci', 'origin')
    t.snap('end', 'mark', 'origin')
    t.flags('player1', '-FL_NOTARGET')


@S.test('func_monsterclip: a monster with MonsterClip (4) cannot cross it: the scientist stays on his side',
        players=1, covers=('func_monsterclip',))
def monsterclip_blocks(t):
    clip_room(t, SF_MONSTERCLIP)
    t.expect.distance('end', 'sci', 'mark', 150, 1000)


@S.test('func_monsterclip: control: a monster without MonsterClip runs through it to the mark; a player walks '
        'through it', players=1, covers=('func_monsterclip',))
def monsterclip_passes(t):
    clip_room(t, 0)
    t.expect.distance('end', 'sci', 'mark', 0, 48)
    t.point('info_target', targetname='far', at=(150, 200, 0))
    t.teleport('player1', (-150, 200, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.walk('player1', 250, 0, 1.5)
    t.snap('walked', 'player1', 'origin')
    t.snap('walked', 'far', 'origin')
    t.expect.distance('walked', 'player1', 'far', 0, 100)


# ------------------------------------------------------------------------------------------------ doors: 512

def door_room(t, cls, flags):
    """Two halves of the room joined by a doorway with a door in it; a scientist told to run to the other half."""
    t.brush('func_wall', size=(16, 200, 200), at=(0, -156, 0))
    t.brush('func_wall', size=(16, 200, 200), at=(0, 156, 0))
    t.brush('func_wall', size=(16, 112, 72), at=(0, 0, 128))
    kv = dict(targetname='door', speed='200', wait='4', spawnflags=str(flags))
    if cls == 'func_door':
        kv['angles'] = '0 90 0'
        t.brush(cls, size=(8, 112, 128), at=(0, 0, 0), **kv)
    else:
        kv['distance'] = '90'
        t.brush(cls, size=(8, 112, 128), at=(0, 0, 0), origin=True, **kv)
    # a rotating door turns about its middle: the way past it is beside the middle
    node_grid(t, (-180, -60, 60, 180), (-60, 0, 60) if cls == 'func_door' else (-30, 30))
    t.point('monster_scientist', targetname='sci', at=(-150, 0, 0), angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('info_target', targetname='mark', at=(150, 0, 0))
    t.point('scripted_sequence', targetname='ss', at=(150, 0, 0), angles='0 0 0', m_iszEntity='sci',
            m_fMoveTo='2', m_iszPlay='idle1', names=('m_iszEntity',))
    t.teleport('player1', (-200, 200, 0))
    t.flags('player1', '+FL_NOTARGET')
    t.wait(1.0)
    t.mark('go')
    t.fire('ss')
    t.wait(10.0)
    t.snap('end', 'sci', 'origin')
    t.snap('end', 'mark', 'origin')
    t.flags('player1', '-FL_NOTARGET')


def make_door_tests(cls, unused_flags):
    def opens(t):
        door_room(t, cls, unused_flags)
        t.expect.used('door', caller='sci', min=1, after='go')
        t.expect.distance('end', 'sci', 'mark', 0, 48)

    def stays(t):
        door_room(t, cls, unused_flags | 512)
        t.expect.not_used('door', after='go')
        t.expect.distance('end', 'sci', 'mark', 150, 1000)

    opens.__name__ = 'door_opens_' + cls
    stays.__name__ = 'door_monsters_cant_' + cls
    S.test('%s: a monster whose route goes through it opens it and gets through' % cls, players=1,
           covers=(cls,))(opens)
    S.test('%s flag 512 Monsters Can\'t: the monster does not open it and stays on its side' % cls, players=1,
           covers=(cls + ':512',))(stays)


make_door_tests('func_door', 0)
make_door_tests('func_door_rotating', 0)


# ------------------------------------------------------------------------------------------------ func_tracktrain

@S.test('func_tracktrain Bank angle on turns 30: going round a left turn it leans (roll) towards it; Bank 0 stays '
        'level', covers=('func_tracktrain.bank',))
def tracktrain_bank(t):
    for y, name, bank in ((-120, 'banked', '30'), (120, 'level', '0')):
        t.point('path_track', at=(-200, y - 60, 40), targetname=name + '1', target=name + '2')
        t.point('path_track', at=(0, y - 60, 40), targetname=name + '2', target=name + '3')
        t.point('path_track', at=(100, y + 60, 40), targetname=name + '3')
        t.brush('func_tracktrain', size=(48, 24, 16), at=(-200, y - 60, 40), origin=True, targetname=name,
                target=name + '1', speed='120', height='0', wheels='16', bank=bank, spawnflags='2')
    t.wait(0.5)
    t.fire('banked', use='on')
    t.fire('level', use='on')
    t.wait(1.9)
    t.snap('turn', 'banked', 'angles')
    t.snap('turn', 'level', 'angles')
    t.expect.value('turn', 'banked', 'angles.z', 3, op='>=')
    t.expect.value('turn', 'level', 'angles.z', 0, tol=0.5)


@S.test('func_tracktrain Distance between the wheels: on a slope (200 up over 400) it pitches with the track (about '
        '26.6 degrees); flag 1 No Pitch stays level', covers=('func_tracktrain.wheels', 'func_tracktrain:1'))
def tracktrain_wheels(t):
    for y, name, flags in ((-100, 'pitched', '2'), (100, 'flat', '3')):
        t.point('path_track', at=(-200, y, 24), targetname=name + '1', target=name + '2')
        t.point('path_track', at=(200, y, 224), targetname=name + '2')
        t.brush('func_tracktrain', size=(48, 24, 16), at=(-200, y, 24), origin=True, targetname=name,
                target=name + '1', speed='100', height='0', wheels='40', bank='0', spawnflags=flags)
    t.wait(0.5)
    t.fire('pitched', use='on')
    t.fire('flat', use='on')
    t.wait(1.5)
    t.snap('slope', 'pitched', 'angles')
    t.snap('slope', 'flat', 'angles')
    t.expect.value('slope', 'pitched', 'angles.x', 26.6, tol=4)
    t.expect.value('slope', 'flat', 'angles.x', 0, tol=0.5)


# ------------------------------------------------------------------------------------------------ momentary_rot_button

def make_momentary_sound_test(value, sample):
    def momentary_sound(t):
        t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='sink',
                speed='90', distance='90', sounds=str(value))
        t.point('info_target', targetname='sink')
        t.teleport('player1', (0, 0, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait_frames(3)
        t.mark('hold')
        t.walk('player1', 0, 0, 0.5, '+use')
        t.wait(0.3)
        t.expect.sound(sample, ent='mb', after='hold')

    momentary_sound.__name__ = 'momentary_sound_%d' % value
    S.test('momentary_rot_button Sounds %d: turned by a player holding +use it plays %s' % (value, sample),
           players=1, covers=('momentary_rot_button.sounds', 'momentary_rot_button.sounds=%d' % value))(momentary_sound)


for _v, _s in ((9, 'buttons/button9.wav'), (21, 'buttons/lever1.wav'), (23, 'buttons/lever3.wav'),
               (24, 'buttons/lever4.wav')):
    make_momentary_sound_test(_v, _s)


# ------------------------------------------------------------------------------------------------ Squad Name (netname)

def make_help_test(cls):
    def help_call(t):
        # the caller sees the player; the two others stand behind a wall, 300 units from it: the one with the same
        # Squad Name is called (it takes the player as its enemy), the one without a name is not
        t.brush('func_wall', size=(512, 16, 200), at=(0, -60, 0))
        t.point(cls, targetname='caller', netname='pack', at=(0, 100, 0), angles='0 90 0')
        t.point(cls, targetname='mate', netname='pack', at=(-150, -200, 0), angles='0 270 0')
        t.point(cls, targetname='loner', at=(150, -200, 0), angles='0 270 0')
        t.kv('player1', 'health', '100000')
        t.teleport('player1', (0, 220, 0), yaw=270)
        t.wait(3.0)
        t.snap('s', 'caller', 'enemy')
        t.snap('s', 'mate', 'enemy')
        t.snap('s', 'loner', 'enemy')
        t.kv('player1', 'health', '100')
        t.teleport('player1', (-220, -220, 0))
        t.expect.value('s', 'caller', 'enemy', 'player')
        t.expect.value('s', 'mate', 'enemy', 'player')
        t.expect.value('s', 'loner', 'enemy', '')

    help_call.__name__ = 'help_' + cls
    S.test('%s Squad Name: the one that sees a player calls the others with its name within 512 units (they take him '
           'as their enemy behind a wall); one without the name is not called' % cls, players=1,
           covers=(cls + '.netname',))(help_call)


for _c in ('monster_dark', 'monster_alien_slave'):
    make_help_test(_c)
