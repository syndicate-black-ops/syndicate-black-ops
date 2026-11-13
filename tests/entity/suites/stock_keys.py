"""Stock Half-Life keys, flags and choice values that need a real server (GAPS.md tier 3 part 3): what the engine does
with them (contents, sprites the engine animates) or what a player feels. Each test checks the FGD's description
(sdk/syndicate.fgd). The logic-only stock keys are map-test cases (tests/maps/cases 90-99, 1.38 and develop side by
side).

- env_sprite: Start on (1), Play Once (2), Framerate, Scale, the sprite (model);
- func_water Contents (skin): -3 Water (a player inside is under water, no damage), -4 Slime (acid damage), -5 Lava
  (burn damage); func_illusionary Contents -1 Empty: not solid, not water."""

from etest import Suite

SUITE = Suite('stock_keys', 'Stock HL keys: env_sprite, func_water / func_illusionary contents')
S = SUITE

EF_NODRAW = 128
DMG_BURN, DMG_ACID = 8, 1048576
CONTENTS_EMPTY, CONTENTS_WATER, CONTENTS_SLIME, CONTENTS_LAVA = -1, -3, -4, -5
FIRE = 'sprites/fire.spr'      # 15 frames


# ------------------------------------------------------------------------------------------------ env_sprite

@S.test('env_sprite: a named sprite without Start on is hidden until triggered, then animates at its Framerate (10 '
        'runs twice as fast as 5: frame about 6 after 0.6 s); Scale 2 and the sprite are kept',
        covers=('env_sprite', 'env_sprite.framerate', 'env_sprite.scale', 'env_sprite.model'))
def sprite_off_until_fired(t):
    t.point('env_sprite', targetname='spr', model=FIRE, framerate='10', scale='2', at=(0, 0, 64))
    t.point('env_sprite', targetname='slow', model=FIRE, framerate='5', at=(64, 0, 64))
    t.wait(0.2)
    t.snap('before', 'spr', 'effects', 'scale', 'model', 'frame')
    t.fire('spr', use='on')
    t.fire('slow', use='on')
    t.mark('on')
    t.wait(0.6)
    t.snap('after', 'spr', 'effects', 'frame')
    t.snap('after', 'slow', 'frame')
    t.expect.value('before', 'spr', 'effects', EF_NODRAW, op='&')
    t.expect.value('before', 'spr', 'scale', 2)
    t.expect.value('before', 'spr', 'model', FIRE)
    t.expect.value('after', 'spr', 'effects', EF_NODRAW, op='!&')
    # the server steps a sprite once per think (0.1 s): a loaded server runs fewer thinks, so the frame can be lower
    t.expect.value('after', 'spr', 'frame', 3, op='>=')
    t.expect.value('after', 'spr', 'frame', 7, op='<=')
    t.expect.ratio('after', 'slow', 'after', 'spr', 'frame', 2.0, tol=0.25)
    t.expect.precached(FIRE, kind='model')


@S.test('env_sprite flag 1 Start on: a named sprite is visible from the start and animates; triggered, it hides',
        covers=('env_sprite:1',))
def sprite_start_on(t):
    t.point('env_sprite', targetname='spr', model=FIRE, framerate='10', spawnflags='1', at=(0, 0, 64))
    t.wait(0.5)
    t.snap('start', 'spr', 'effects', 'frame')
    t.fire('spr')
    t.wait(0.2)
    t.snap('off', 'spr', 'effects')
    t.expect.value('start', 'spr', 'effects', EF_NODRAW, op='!&')
    t.expect.value('start', 'spr', 'frame', 0, op='>')
    t.expect.value('off', 'spr', 'effects', EF_NODRAW, op='&')


@S.test('env_sprite flag 2 Play Once: it plays its 15 frames once at Framerate 10 (1.5 s), then hides; without the '
        'flag it loops and stays visible', covers=('env_sprite:2',))
def sprite_play_once(t):
    t.point('env_sprite', targetname='once', model=FIRE, framerate='10', spawnflags='2', at=(0, 0, 64))
    t.point('env_sprite', targetname='loop', model=FIRE, framerate='10', at=(64, 0, 64))
    t.fire('once', use='on')
    t.fire('loop', use='on')
    t.wait(0.5)
    t.snap('mid', 'once', 'effects')
    t.wait(2.0)
    t.snap('end', 'once', 'effects')
    t.snap('end', 'loop', 'effects')
    t.expect.value('mid', 'once', 'effects', EF_NODRAW, op='!&')
    t.expect.value('end', 'once', 'effects', EF_NODRAW, op='&')
    t.expect.value('end', 'loop', 'effects', EF_NODRAW, op='!&')


# ------------------------------------------------------------------------------------------------ contents

def pool(t, cls, skin, **kv):
    return t.brush(cls, size=(256, 256, 160), at=(0, 0, 0), skin=str(skin), **kv)


@S.test('func_water Contents -3 Water: a player standing in it is fully under water (waterlevel 3, watertype -3) and '
        'takes no damage', players=1, covers=('func_water', 'func_water.skin', 'func_water.skin=-3'))
def water_contents_water(t):
    pool(t, 'func_water', CONTENTS_WATER)
    t.teleport('player1', (0, 0, 0))
    t.mark('in')
    t.wait(1.0)
    t.snap('in', 'player1', 'waterlevel', 'watertype')
    t.expect.value('in', 'player1', 'waterlevel', 3)
    t.expect.value('in', 'player1', 'watertype', CONTENTS_WATER)
    t.expect.not_damaged('player1', after='in', within=(0, 1.0))


@S.test('func_water Contents -4 Slime: a player in it takes acid damage (DMG_ACID)', players=1,
        covers=('func_water.skin=-4',))
def water_contents_slime(t):
    pool(t, 'func_water', CONTENTS_SLIME)
    t.flags('player1', '+FL_GODMODE')     # alive for the next tests
    t.teleport('player1', (0, 0, 0))
    t.mark('in')
    t.wait(0.5)
    t.snap('in', 'player1', 'watertype')
    t.teleport('player1', (200, 200, 0))
    t.wait(0.1)
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('in', 'player1', 'watertype', CONTENTS_SLIME)
    t.expect.damaged('player1', after='in', within=(0, 0.5), type=DMG_ACID)


@S.test('func_water Contents -5 Lava: a player in it takes burn damage (DMG_BURN)', players=1,
        covers=('func_water.skin=-5',))
def water_contents_lava(t):
    pool(t, 'func_water', CONTENTS_LAVA)
    t.flags('player1', '+FL_GODMODE')     # alive for the next tests
    t.teleport('player1', (0, 0, 0))
    t.mark('in')
    t.wait(0.5)
    t.snap('in', 'player1', 'watertype')
    t.teleport('player1', (200, 200, 0))
    t.wait(0.1)
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('in', 'player1', 'watertype', CONTENTS_LAVA)
    t.expect.damaged('player1', after='in', within=(0, 0.5), type=DMG_BURN)


@S.test('func_illusionary Contents -1 Empty: a player walks through it and is not in water', players=1,
        covers=('func_illusionary', 'func_illusionary.skin', 'func_illusionary.skin=-1'))
def illusionary_empty(t):
    t.brush('func_illusionary', size=(512, 32, 200), at=(0, 0, 0), skin='-1')
    t.point('info_target', targetname='start', at=(0, -120, 0))
    t.teleport('player1', (0, -120, 0), yaw=90)
    t.wait(0.2)
    t.walk('player1', 250, 0, 1.0)
    t.snap('after', 'player1', 'origin', 'waterlevel')
    t.snap('after', 'start', 'origin')
    t.expect.distance('after', 'player1', 'start', 150, 400)
    t.expect.value('after', 'player1', 'waterlevel', 0)


# ------------------------------------------------------------------------------------------------ monsters: Ff

def ff_test(t, cls):
    t.point(cls, targetname='ff1', Ff='1', at=(-120, 0, 0), angles='0 0 0')
    t.point(cls, targetname='ff0', Ff='0', at=(0, 0, 0), angles='0 0 0')
    t.point(cls, targetname='ally', at=(120, 0, 0), angles='0 180 0')
    t.wait(0.5)
    t.snap('before', 'ff1', 'health')
    t.snap('before', 'ff0', 'health')
    t.damage('ff1', 10, 2, attacker='ally')        # DMG_BULLET
    t.damage('ff0', 10, 2, attacker='ally')
    t.wait(0.1)
    t.snap('after', 'ff1', 'health')
    t.snap('after', 'ff0', 'health')
    t.expect.same('before', 'after', 'ff1', 'health')
    t.expect.ratio('before', 'ff0', 'after', 'ff0', 'health', 0.9, tol=0.05)   # 10 of its 100


@S.test('dark_grunt Ff 1 "Ignore Friendly Damage": a hit from another dark_grunt (an ally) takes no health; with Ff 0 '
        'the same hit does', covers=('dark_grunt.ff', 'dark_grunt.ff=0', 'dark_grunt.ff=1'))
def ff_dark_grunt(t):
    ff_test(t, 'dark_grunt')


@S.test('monster_human_grunt Ff 1 "Ignore Friendly Damage": a hit from another grunt takes no health; with Ff 0 the '
        'same hit does', covers=('monster_human_grunt.ff', 'monster_human_grunt.ff=0', 'monster_human_grunt.ff=1'))
def ff_human_grunt(t):
    ff_test(t, 'monster_human_grunt')


# ------------------------------------------------------------------------------------------------ env_shake

@S.test('env_shake: a player within Effect radius gets a ScreenShake with Amplitude 8, Duration 2, Frequency 40; one '
        'outside the radius gets none, unless GlobalShake (1)', players=1,
        covers=('env_shake', 'env_shake.amplitude', 'env_shake.radius', 'env_shake.duration', 'env_shake.frequency',
                'env_shake:1'))
def shake(t):
    t.point('env_shake', targetname='near', amplitude='8', radius='300', duration='2', frequency='40', at=(0, 0, 0))
    t.point('env_shake', targetname='far', amplitude='4', radius='100', duration='1', frequency='2', at=(200, 200, 0))
    t.point('env_shake', targetname='glob', amplitude='2', radius='100', duration='1', frequency='4', spawnflags='1',
            at=(200, 200, 0))
    t.teleport('player1', (0, -100, 0))
    t.wait(0.3)
    t.mark('go')
    t.fire('near')
    t.fire('far')
    t.wait(0.1)
    t.mark('glob')
    t.fire('glob')
    t.wait(0.1)
    t.expect.msg('ScreenShake', to='player1', count=1, after='go', within=(0, 0.1),
                 fields={'amplitude': 8, 'duration': 2, 'frequency': 40})
    t.expect.msg('ScreenShake', to='player1', count=1, after='glob', fields={'amplitude': 2, 'duration': 1,
                                                                              'frequency': 4})


# ------------------------------------------------------------------------------------------------ pushers

@S.test('trigger_push Speed of push 300 along its angles (+x): a player inside is pushed at about 300 units/s (the '
        'push sets the velocity: base velocity for a player on the ground)', players=1,
        covers=('trigger_push', 'trigger_push.speed'))
def push_speed(t):
    t.trigger('trigger_push', size=(400, 400, 100), at=(0, 0, 0), speed='300', angles='0 0 0')
    t.point('info_target', targetname='start', at=(-150, 0, 0))
    t.teleport('player1', (-150, 0, 0))
    t.wait(0.5)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'start', 'origin')
    t.expect.distance('after', 'player1', 'start', 80, 400)


@S.test('func_conveyor Conveyor Speed 200 (angles +x): a player standing on it is carried along; on one with flag 1 No '
        'Push he stays', players=1, covers=('func_conveyor', 'func_conveyor.speed', 'func_conveyor:1'))
def conveyor_speed(t):
    t.brush('func_conveyor', size=(400, 128, 16), at=(0, -100, 0), speed='200', angles='0 0 0')
    t.brush('func_conveyor', size=(400, 128, 16), at=(0, 100, 0), speed='200', angles='0 0 0', spawnflags='1')
    t.point('info_target', targetname='a', at=(-150, -100, 16))
    t.point('info_target', targetname='b', at=(-150, 100, 16))
    t.teleport('player1', (-150, -100, 16))
    t.wait(1.0)
    t.snap('moving', 'player1', 'origin')
    t.snap('moving', 'a', 'origin')
    t.teleport('player1', (-150, 100, 16))
    t.wait(0.3)
    t.snap('still0', 'player1', 'origin')
    t.wait(1.0)
    t.snap('still', 'player1', 'origin')
    t.expect.distance('moving', 'player1', 'a', 100, 400)
    t.expect.moved('still0', 'still', 'player1', delta=(0, 0, 0), tol=8)
