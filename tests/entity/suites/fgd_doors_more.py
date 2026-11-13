"""Tier 5, final pass: base Door / LockedSounds / DoorBlockers items on func_door_rotating, func_water and
func_rot_button that other suites check on func_door or func_button only (doors.cpp CBaseDoor, CRotDoor;
buttons.cpp): the sound files and sentence names that replace the numbered locked / unlocked choices, Health to give on
open, Fire on Close, Passable, Starts Open, the player use policy, and the corpse / blocker options on a rotating
door. A func_water always has Contents (-3 water, -4 slime, -5 lava), so CBaseDoor::Spawn makes it SOLID_NOT and
silent (SF_DOOR_SILENT): its locked sounds still play (PlayLockSounds)."""

from etest import Suite

SUITE = Suite('fgd_doors_more', 'Tier 5: door keys on func_door_rotating, func_water, func_rot_button', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
SOLID_NOT, SOLID_BSP = 0, 4
SF_USE_ONLY = 256


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def locks(t):
    """'lock': a locked master; 'open': a master unlocked at once (its one input fired)."""
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='never', target='lock', triggerstate='1')    # its one input, never fired
    t.point('multisource', targetname='open')
    t.point('trigger_relay', targetname='key', target='open', triggerstate='1')
    t.fire('key')
    t.wait(0.2)


def door(t, cls, name, x, y=150, **kv):
    kv.setdefault('speed', '400')
    kv.setdefault('wait', '1')
    if cls == 'func_door_rotating':
        return t.brush(cls, targetname=name, size=(8, 64, 96), at=(x, y, 0), origin=True, distance='90', **kv)
    if cls == 'func_water':
        kv.setdefault('skin', '-3')
    return t.brush(cls, targetname=name, size=(64, 16, 96), at=(x, y, 0), tex='666MURMETAL', angles='-90 0 0',
                   lip='8', **kv)


# ---------------------------------------------------------------------------------------------- sound / sentence names
def overrides_case(cls):
    @S.test('%s sound files and sentence names: locked (Locked play on use), "locked_sound_override" and '
            '"locked_sentence_override" ND instead of the numbered 2 / 1; unlocked, "unlocked_sound_override" and '
            '"unlocked_sentence_override" ED instead of 3 / 1; a door without them plays the numbered ones' % cls,
            players=1, covers=tuple('%s.%s' % (cls, k) for k in (
                'locked_sound_override', 'unlocked_sound_override', 'locked_sentence_override',
                'unlocked_sentence_override')))
    def case(t):
        locks(t)
        door(t, cls, 'dl', -180, master='lock', locked_play_on_use='1', locked_sound='2', locked_sentence='1',
             locked_sound_override='buttons/blip1.wav', locked_sentence_override='ND')
        door(t, cls, 'du', -60, master='open', unlocked_sound='3', unlocked_sentence='1',
             unlocked_sound_override='buttons/blip2.wav', unlocked_sentence_override='ED')
        door(t, cls, 'nl', 60, master='lock', locked_play_on_use='1', locked_sound='2', locked_sentence='1')
        door(t, cls, 'nu', 180, master='open', unlocked_sound='3', unlocked_sentence='1')
        t.wait(0.2)
        t.mark('go')
        for d in ('dl', 'du', 'nl', 'nu'):
            t.fire(d, activator='player1')
        t.wait(0.5)
        t.expect.sound('buttons/blip1.wav', ent='dl', after='go', count=1)
        t.expect.sound('ND', ent='dl', after='go', count=1)
        t.expect.sound('buttons/button2.wav', ent='dl', after='go', count=0)
        t.expect.sound('buttons/blip2.wav', ent='du', after='go', count=1)
        t.expect.sound('ED', ent='du', after='go', count=1)
        t.expect.sound('buttons/button2.wav', ent='nl', after='go', count=1)
        t.expect.sound('NA', ent='nl', after='go', count=1)
        t.expect.sound('buttons/button3.wav', ent='nu', after='go', count=1)
        t.expect.sound('EA', ent='nu', after='go', count=1)
    case.__name__ = 'overrides_' + cls.replace('func_', '')
    return case


for _c in ('func_door_rotating', 'func_water'):
    overrides_case(_c)


@S.test('func_rot_button sentence names: locked, "locked_sentence_override" ND speaks instead of the numbered 1 (NA); '
        'unlocked, "unlocked_sentence_override" ED instead of 1 (EA); a button without them speaks the numbered ones',
        players=1, covers=('func_rot_button.locked_sentence_override', 'func_rot_button.unlocked_sentence_override',
                           'func_rot_button.locked_sentence', 'func_rot_button.unlocked_sentence'))
def rot_button_sentences(t):
    locks(t)
    for name, x, kv in (('bl', -150, dict(master='lock', locked_sentence='1', locked_sentence_override='ND')),
                        ('bu', -50, dict(master='open', unlocked_sentence='1', unlocked_sentence_override='ED')),
                        ('nl', 50, dict(master='lock', locked_sentence='1')),
                        ('nu', 150, dict(master='open', unlocked_sentence='1'))):
        t.brush('func_rot_button', targetname=name, size=(16, 32, 32), at=(x, 0, 40), origin=True, distance='30',
                speed='100', wait='1', **kv)
    t.wait(0.2)
    t.mark('go')
    for b in ('bl', 'bu', 'nl', 'nu'):
        t.fire(b, activator='player1')
    t.wait(0.5)
    t.expect.sound('ND', ent='bl', after='go', count=1)
    t.expect.sound('ED', ent='bu', after='go', count=1)
    t.expect.sound('NA', ent='nl', after='go', count=1)
    t.expect.sound('EA', ent='nu', after='go', count=1)


# ---------------------------------------------------------------------------------------------- health, close, flags
@S.test('func_door_rotating Health to give on open 25: a player at 50 health who opens it has 75; one opened by a '
        'trigger_relay (no player) gives nothing', players=1, covers=('func_door_rotating.healthvalue',))
def rot_healthvalue(t):
    door(t, 'func_door_rotating', 'd', -100, healthvalue='25', wait='-1')
    door(t, 'func_door_rotating', 'n', 100, healthvalue='25', wait='-1')
    t.point('trigger_relay', targetname='relay', target='n')
    t.kv('player1', 'health', '50')
    t.wait(0.1)
    t.snap('before', 'player1', 'health')
    t.fire('d', activator='player1')
    t.wait(0.3)
    t.snap('after', 'player1', 'health')
    t.fire('relay')
    t.wait(0.3)
    t.snap('relay', 'player1', 'health')
    t.kv('player1', 'health', '100')
    t.expect.value('before', 'player1', 'health', 50)
    t.expect.value('after', 'player1', 'health', 75)
    t.expect.value('relay', 'player1', 'health', 75)


def close_case(cls):
    @S.test('%s Starts Open with "Fire on Close" (netname): it spawns open; fired, it closes and fires the target '
            'once it is shut' % cls, players=1, covers=('%s:1' % cls, '%s.netname' % cls))
    def case(t):
        d = door(t, cls, 'door', 0, spawnflags='1', wait='-1', netname='closed', names=('netname',))
        info(t, 'closed')
        t.wait(0.3)
        field = 'angles' if cls == 'func_door_rotating' else 'origin'
        t.snap('spawn', 'door', field)
        t.mark('go')
        t.fire('door')
        t.wait(0.6)
        t.snap('after', 'door', field)
        if cls == 'func_door_rotating':
            t.expect.value('spawn', 'door', 'angles', [0, 90, 0], tol=0.5)
            t.expect.value('after', 'door', 'angles', [0, 0, 0], tol=0.5)
        else:
            t.expect.value('spawn', 'door', 'origin', [0, 0, 88], tol=0.5)
            t.expect.value('after', 'door', 'origin', [0, 0, 0], tol=0.5)
        t.expect.fired('closed', count=1, after='go', within=(0.1, 0.5))
    case.__name__ = 'close_' + cls.replace('func_', '')
    return case


for _c in ('func_door_rotating', 'func_water'):
    close_case(_c)


@S.test('func_door_rotating flag 8 Passable: a player walks through the closed door (not solid); through the same '
        'door without the flag he can\'t', players=1, covers=('func_door_rotating:8',))
def rot_passable(t):
    t.brush('func_door_rotating', targetname='pass', size=(256, 16, 128), at=(-128, 0, 0), origin=True,
            distance='90', spawnflags='8')
    t.brush('func_door_rotating', targetname='wall', size=(256, 16, 128), at=(128, 0, 0), origin=True,
            distance='90')
    info(t, 'start_p', at=(-128, -100, 0))
    info(t, 'start_w', at=(128, -100, 0))
    t.teleport('player1', (-128, -100, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('p', 'player1', 'origin')
    t.snap('p', 'start_p', 'origin')
    t.teleport('player1', (128, -100, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('w', 'player1', 'origin')
    t.snap('w', 'start_w', 'origin')
    t.snap('w', 'pass', 'solid')
    t.snap('w', 'wall', 'solid')
    t.expect.distance('p', 'player1', 'start_p', 150, 400)          # past the door at 100
    t.expect.distance('w', 'player1', 'start_w', 0, 90)             # stopped at the door
    t.expect.value('w', 'pass', 'solid', SOLID_NOT)
    t.expect.value('w', 'wall', 'solid', SOLID_BSP)


# ---------------------------------------------------------------------------------------------- func_water use policy
def use_water(t, name, at, size=(16, 64, 32), **kv):
    """A Use Only func_water that fires `<name>_open` when it starts opening."""
    info(t, name + '_open')
    t.brush('func_water', targetname=name, size=size, at=at, angles='0 0 0', speed='100', wait='-1', lip='8',
            skin='-3', spawnflags=str(SF_USE_ONLY), fireonopening=name + '_open', fireonopening_triggerstate='1',
            names=('fireonopening',), **kv)


@S.test('func_water "directuse" 0 (Default): a low Use Only water in front of the player is used while he looks over '
        'it (Half-Life\'s pick by view cone)', players=1, covers=('func_water:256', 'func_water.directuse=0'))
def water_directuse_default(t):
    use_water(t, 'wn', at=(44, 0, 0), directuse='0')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('over')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.used('wn_open', after='over', count=1)


@S.test('func_water "directuse" 1 (Direct only): the look is a trace and a liquid is not solid, so the player never '
        'uses it, looking over it or straight at it', players=1, covers=('func_water.directuse=1',))
def water_directuse_direct(t):
    use_water(t, 'wd', at=(44, 0, 0), directuse='1')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('over')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.look('player1', 45, 0)
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('wd_open', after='over')


@S.test('func_water "directuse" 2 (Not through walls): a Use Only water behind a wall is not used; the same water '
        'without the key is used through the wall (Half-Life)', players=1, covers=('func_water.directuse=2',))
def water_directuse_visible(t):
    t.brush('func_wall', size=(4, 160, 128), at=(26, 0, 0))
    use_water(t, 'wv', at=(44, 30, 0), size=(16, 40, 64), directuse='2')
    use_water(t, 'ww', at=(44, -30, 0), size=(16, 40, 64))
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('wv_open', after='use')
    t.expect.used('ww_open', after='use', count=1)


# ---------------------------------------------------------------------------------------------- rotating door blockers
# A door 128 long (an arm of 64 each side of its origin) along y, turning +90 (yaw) about its center: the +y arm
# sweeps from (0, 64) to (-64, 0). The blocker stands at (-30, 40) on that sweep; a func_wall at (-56, 20) behind it
# pins it (a pusher moves through BSP entities, the blocker can't).
def rdoor(t, name, x, **kv):
    info(t, name + '_opened')
    info(t, name + '_closing')
    t.brush('func_door_rotating', targetname=name, size=(8, 128, 72), at=(x, 0, 0), origin=True, distance='90',
            speed='90', fireonopened=name + '_opened', fireonopened_triggerstate='1', fireonclosing=name + '_closing',
            fireonclosing_triggerstate='1', names=('fireonopened', 'fireonclosing'), **kv)


def pin(t, x):
    t.brush('func_wall', size=(16, 16, 128), at=(x - 56, 20, 0))


R = 'func_door_rotating'


@S.test('func_door_rotating ignore_corpses 0 pushes a corpse in its way along, 1 swings through it', players=1, doc=DOC,
        covers=(R + '.ignore_corpses=0', R + '.ignore_corpses=1'))
def rot_ignore_corpses(t):
    for v, x in (('0', -120), ('1', 120)):
        rdoor(t, 'd' + v, x, wait='-1', ignore_corpses=v)
        t.point('monster_hgrunt_dead', targetname='c' + v, at=(x - 30, 40, 0), angles='0 0 0')
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.5)
    t.snap('a', 'c0', 'origin')
    t.snap('a', 'c1', 'origin')
    t.fire('d0')
    t.fire('d1')
    t.wait(1.5)
    t.snap('b', 'c0', 'origin')
    t.snap('b', 'c1', 'origin')
    t.expect.moved('a', 'b', 'c0', min_dist=10)
    t.expect.moved('a', 'b', 'c1', delta=(0, 0, 0), tol=1.0)


@S.test('func_door_rotating instant_gib_corpses 1 gibs a corpse pinned against a wall and swings on; 0 is blocked by '
        'it', players=1, doc=DOC, covers=(R + '.instant_gib_corpses=0', R + '.instant_gib_corpses=1'))
def rot_instant_gib(t):
    for v, x in (('0', -120), ('1', 120)):
        rdoor(t, 'g' + v, x, wait='-1', instant_gib_corpses=v)
        pin(t, x)
        t.point('monster_hgrunt_dead', targetname='c' + v, at=(x - 30, 40, 0), angles='0 0 0')
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.5)
    t.fire('g0')
    t.fire('g1')
    t.wait(2.0)
    t.expect.removed('c1')
    t.expect.used('g1_opened', count=1)
    t.expect.not_removed('c0')
    t.expect.not_used('g0_opened')


@S.test('func_door_rotating blocker_recheck 1: a monster its damage kills does not turn it back; 0 and 2 go back',
        players=1, doc=DOC, covers=tuple('%s.blocker_recheck=%d' % (R, v) for v in (0, 1, 2)))
def rot_blocker_recheck(t):
    for v, x in (('0', -160), ('1', 0), ('2', 160)):
        rdoor(t, 'r' + v, x, wait='5', dmg='500', blocker_recheck=v)
        pin(t, x)
        t.point('monster_cyborg', targetname='m' + v, at=(x - 30, 40, 0), angles='0 180 0')
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(0.5)
    for v in '012':
        t.kv('m' + v, 'health', '10')
    for v in '012':
        t.fire('r' + v)
    t.wait(2.0)
    t.expect.used('r1_opened', count=1)
    t.expect.not_used('r1_closing')
    for v in '02':
        t.expect.used('r%s_closing' % v, min=1)
