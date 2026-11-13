"""The SDK update's map entity fixes (74c5ad52, halflife-updated / HL25) that need a real server: momentary_door value
clamp, arrival and repeated moves; func_button sparks at the button; func_breakable "explosion" 1 (directed) and 0
(random) gibs; a func_plat's trigger after the platform is removed; env_sound's room type; func_rotating
"spawnorigin" (GAPS.md tier 2 "74c5ad52 map entity fixes, hunk by hunk"). The stub-runner hunks are the map-test cases
100-100c and 102b (tests/maps/cases)."""

from etest import Suite

SUITE = Suite('sdk_entity_fixes', 'SDK update map entity fixes (74c5ad52): momentary_door, button sparks, breakable '
              'gibs, plat trigger, env_sound, func_rotating')
S = SUITE

USE_SET = 'set'
SND_STOP = 32
TE_SPARKS, TE_BREAKMODEL = 9, 108
MOVE1, STOP1 = 'doors/doormove1.wav', 'doors/doorstop1.wav'


def mdoor(t, **kv):
    """A momentary_door 64 high moving up 64 (lip 0): value 1 = 64 up. It always moves in 0.1 s (speed = 10 x the
    distance, HL SDK)."""
    return t.brush('momentary_door', size=(64, 16, 64), at=(0, 120, 0), tex='666MURMETAL', angles='-90 0 0',
                   lip='0', **kv)


# ------------------------------------------------------------------------------------------------ momentary_door

@S.test('momentary_door: a negative value is used as 0 (the closed position), not past it (halflife #3265 hunk)',
        covers=('momentary_door',))
def mdoor_negative(t):
    mdoor(t, targetname='md')
    t.wait(1.0)
    t.snap('closed', 'md', 'origin')
    t.fire('md', use=USE_SET, value=0.5)
    t.wait(0.5)
    t.snap('half', 'md', 'origin')
    t.fire('md', use=USE_SET, value=-0.5)
    t.wait(0.5)
    t.snap('neg', 'md', 'origin')
    t.expect.value('half', 'md', 'origin', [0, 0, 32.5], tol=1.0)
    t.expect.value('neg', 'md', 'origin', [0, 0, 0], tol=0.5)


@S.test('momentary_door: once it arrives its move sound is stopped and its stop sound (stopsnd) plays '
        '(halflife #3265: 1.38 left the move sound running)',
        covers=('momentary_door.movesnd', 'momentary_door.stopsnd'))
def mdoor_arrival_sounds(t):
    mdoor(t, targetname='md', movesnd='1', stopsnd='1')
    t.wait(0.3)
    t.mark('go')
    t.fire('md', use=USE_SET, value=1)
    t.wait(0.6)
    t.snap('open', 'md', 'origin')
    t.expect.value('open', 'md', 'origin', [0, 0, 65], tol=1.0)
    # a think runs with its own think time, which under load can be close to the mark: no lower bound
    t.expect.sound(MOVE1, ent='md', vol=(0.5, 1.0), count=1, after='go', within=(0, 0.1))
    t.expect.sound(MOVE1, ent='md', flags=SND_STOP, count=1, after='go', within=(0, 0.4))
    t.expect.sound(STOP1, ent='md', count=1, after='go', within=(0, 0.4))


@S.test('momentary_door: Used again and again with the value it is already moving to (a held momentary_rot_button '
        'at its end), it arrives once in 0.1 s; 1.38 restarted the move every time and arrived only after the last',
        covers=('momentary_door',))
def mdoor_repeated_value(t):
    mdoor(t, targetname='md', movesnd='1', stopsnd='1')
    t.wait(0.3)
    t.mark('go')
    for _ in range(12):
        t.fire('md', use=USE_SET, value=1)
        t.wait(0.04)
    t.wait(0.4)
    # it arrives (stop sound) long before the last Use (0.48 s); the move sound starts once (the SND_STOP of the
    # move sound has volume 0: left out by vol)
    t.expect.sound(STOP1, ent='md', count=1, after='go', within=(0, 0.3))
    t.expect.sound(MOVE1, ent='md', vol=(0.5, 1.0), count=1, after='go')


# ------------------------------------------------------------------------------------------------ func_button sparks

@S.test('func_button flag 64 Sparks with an ORIGIN brush: the sparks come from the button (its absmin + half its '
        'size), not from its model\'s local bounds around the map origin (halflife #3269)',
        covers=('func_button:64',))
def button_sparks_at_button(t):
    t.brush('func_button', size=(16, 32, 32), at=(0, 120, 40), tex='+0~ANZYG1', origin=True, angles='0 90 0',
            targetname='btn', spawnflags='64', speed='100', wait='1')
    t.wait(2.5)
    x, y, z = t._abs((0, 120, 56))
    t.expect.msg('svc:23', fields={'te': TE_SPARKS, 'pos': [{'range': (x - 12, x + 12)}, {'range': (y - 20, y + 20)},
                                                           {'range': (z - 20, z + 20)}]})
    t.expect.msg('svc:23', fields={'te': TE_SPARKS, 'pos': [{'range': (-64, 64)}, {'range': (-64, 64)},
                                                           {'range': (-64, 64)}]}, count=0)


# ------------------------------------------------------------------------------------------------ func_breakable gibs

def breakable(t, **kv):
    return t.brush('func_breakable', size=(32, 32, 32), at=(0, 120, 16), tex='+0~ANZYG1', health='10', material='1',
                   **kv)


@S.test('func_breakable "explosion" 1 (Relative to Attack): the gibs fly away from the attacker (the FGD writes "1"; '
        'halflife-updated #24: g_vecAttackDir points at the attacker, so the velocity is minus it, 200 u/s)',
        players=1, covers=('func_breakable.explosion=1',))
def breakable_directed(t):
    breakable(t, targetname='br', explosion='1')
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.wait(0.3)
    t.mark('go')
    t.damage('br', 50, 0, 'player1')
    t.wait(0.3)
    t.expect.msg('svc:23', after='go', count=1,
                 fields={'te': TE_BREAKMODEL, 'velocity': [{'range': (-40, 40)}, {'range': (150, 201)},
                                                           {'range': (-60, 60)}]})


@S.test('func_breakable "explosion" 0 (Random): the gibs get a random velocity of at most 100 sideways '
        '(the control of the directed test)', players=1, covers=('func_breakable.explosion=0',))
def breakable_random(t):
    breakable(t, targetname='br', explosion='0')
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.wait(0.3)
    t.mark('go')
    t.damage('br', 50, 0, 'player1')
    t.wait(0.3)
    t.expect.msg('svc:23', after='go', count=1,
                 fields={'te': TE_BREAKMODEL, 'velocity': [{'range': (-100, 100)}, {'range': (-100, 100)},
                                                           {'range': (-200, 200)}]})


# ------------------------------------------------------------------------------------------------ func_plat trigger

@S.test('func_plat without a name, removed by a killtarget: when a player then stands where its trigger is, the '
        'trigger removes itself (halflife #1725: 1.38 read the removed platform through a raw pointer)',
        players=1, covers=('func_plat',))
def plat_removed_trigger(t):
    t.brush('func_plat', size=(96, 96, 16), at=(0, 0, 0), tex='666MURMETAL', height='64', speed='100')
    t.point('trigger_relay', targetname='kill', killtarget='plat')
    t.teleport('player1', (0, -200, 0))
    t.wait(0.3)
    t.kv('@func_plat', 'targetname', t.name('plat'))
    t.fire('kill')
    t.wait(0.3)
    t.mark('gone')
    t.teleport('player1', (0, 0, 16))
    t.wait(0.5)
    t.mark('touched')
    t.teleport('player1', (0, -200, 0))
    t.wait(0.3)
    t.expect.removed('@func_plat')
    t.expect.edicts('gone', 'touched', -1, -1)


# ------------------------------------------------------------------------------------------------ func_rotating

@S.test('func_rotating takes "spawnorigin" (halflife-updated #72: the key is handled)', covers=('func_rotating',))
def rotating_spawnorigin(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(0, 120, 0), origin=True, targetname='rot', speed='0')
    t.wait(0.2)
    x, y, z = t._abs((0, 120, 16))
    t.kv('rot', 'spawnorigin', '%d %d %d' % (x, y, z))
    t.wait(0.1)
    t.expect.kv_handled('rot', 'spawnorigin')


# ------------------------------------------------------------------------------------------------ env_sound (last: the room type stays with the player)

@S.test('env_sound: a player in its radius gets its room type (SVC_ROOMTYPE, sent from UpdateClientData); leaving '
        'the radius does not reset it to 0 (halflife #1144 / #2936)', players=1,
        covers=('env_sound', 'env_sound.roomtype', 'env_sound.radius'))
def env_sound_roomtype(t):
    t.point('env_sound', at=(0, 150, 40), roomtype='14', radius='128')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(1.0)
    t.mark('in')
    t.teleport('player1', (0, 100, 0), yaw=90)
    t.wait(2.0)
    t.mark('out')
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(2.0)
    t.expect.msg('svc:37', to='player1', fields={'room': 14}, after='in', within=(0, 1.8))
    t.expect.no_msg('svc:37', to='player1', after='out')
