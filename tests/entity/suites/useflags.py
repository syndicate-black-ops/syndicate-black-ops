"""Player use policy ("directuse": 0 default, 1 direct only, 2 not through walls) on func_door / func_door_rotating
(with Use Only), func_button and func_rot_button, and the buttons' custom locked / unlocked sounds
(locked_sound_override, unlocked_sound_override), as the FGD describes them (Featureful keys)."""

from etest import Suite

SUITE = Suite('useflags', 'Player use policy and custom lock sounds')
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'

# the player stands at (-48, -208) (the room's south part); a small target at (0, -170) is within the use sphere
SPOT = (-48, -208, 0)


def stand(t, yaw):
    t.teleport('player1', SPOT, yaw=yaw)
    t.look('player1', 0, yaw)
    t.wait_frames(3)


def press(t, mark):
    t.mark(mark)
    t.move('player1', 0, 0, 3, '+use')
    t.wait_frames(6)


USERS = [  # (class, extra keys)
    ('func_door', {'spawnflags': '256', 'angles': '-90 0 0', 'speed': '100', 'lip': '8', 'wait': '-1'}),
    ('func_door_rotating', {'spawnflags': '256', 'speed': '100', 'distance': '30', 'wait': '-1', 'origin_brush': 1}),
    ('func_button', {'angles': '0 90 0', 'speed': '100', 'wait': '1'}),
    ('func_rot_button', {'speed': '100', 'distance': '30', 'wait': '1', 'origin_brush': 1}),
]


def user(t, cls, extra, **kv):
    extra = dict(extra)
    origin = bool(extra.pop('origin_brush', 0))
    kv.update(extra)
    return t.brush(cls, targetname='u', size=(32, 8, 96), at=(0, -170, 0), tex='666MURMETAL', origin=origin, **kv)


def policy_tests(cls, extra):
    def default(t):
        user(t, cls, extra, directuse='0')
        t.wait(1.0)     # right after the suite start a player's +use is not taken at once (ENTITY_TESTS / GAPS tier 1)
        stand(t, 60)
        press(t, 'off_axis')
        t.expect.used('u', activator='player1', min=1, after='off_axis')

    def direct(t):
        user(t, cls, extra, directuse='1')
        stand(t, 60)
        press(t, 'off_axis')
        t.wait(0.3)
        stand(t, 35)
        press(t, 'straight')
        t.expect.not_used('u', after='off_axis', within=(0, 0.25))
        t.expect.used('u', activator='player1', min=1, after='straight')

    def visible(t):
        t.brush('func_wall', size=(160, 8, 128), at=(0, -188, 0))
        user(t, cls, extra, directuse='2')
        stand(t, 35)
        press(t, 'go')
        t.expect.not_used('u', after='go')

    def visible_control(t):
        t.brush('func_wall', size=(160, 8, 128), at=(0, -188, 0))
        user(t, cls, extra)
        stand(t, 35)
        press(t, 'go')
        t.expect.used('u', activator='player1', min=1, after='go')

    default.__name__ = '%s_default' % cls
    S.test('%s directuse 0 (default): used when the player looks past it (it is in the use sphere and cone)' % cls,
            players=1, doc=DOC, covers=(cls, '%s.directuse=0' % cls))(default)

    direct.__name__ = '%s_direct' % cls
    S.test('%s directuse 1 (direct only): not used when the player looks past it; used when he looks straight at it'
            % cls, players=1, doc=DOC, covers=(cls, '%s.directuse=1' % cls))(direct)

    visible.__name__ = '%s_visible' % cls
    S.test('%s directuse 2 (not through walls): behind a wall it is not used' % cls,
            players=1, doc=DOC, covers=(cls, '%s.directuse=2' % cls))(visible)

    visible_control.__name__ = '%s_visible_control' % cls
    S.test('%s directuse 0 behind a wall (control): the use sphere reaches through it' % cls, players=1, doc=DOC,
            covers=(cls, '%s.directuse=0' % cls))(visible_control)


for _cls, _extra in USERS:
    policy_tests(_cls, _extra)


@S.test('func_button / func_rot_button custom locked and unlocked sounds replace the chosen ones and are precached',
        players=1, doc=DOC,
        covers=('func_button.locked_sound_override', 'func_button.unlocked_sound_override',
                'func_rot_button.locked_sound_override', 'func_rot_button.unlocked_sound_override'))
def lock_sounds(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')
    t.brush('func_button', targetname='b', size=(16, 32, 32), at=(-100, 0, 40), master='lock', locked_sound='2',
            unlocked_sound='3', locked_sound_override='buttons/blip1.wav', unlocked_sound_override='buttons/blip2.wav')
    t.brush('func_rot_button', targetname='r', size=(16, 32, 32), at=(100, 0, 40), origin=True, master='lock',
            locked_sound='2', unlocked_sound='3', locked_sound_override='buttons/bell1.wav',
            unlocked_sound_override='buttons/button3.wav', distance='30')
    t.mark('locked')
    t.fire('b', activator='player1')
    t.fire('r', activator='player1')
    t.wait(0.8)
    t.fire('key')
    t.wait(0.1)
    t.mark('open')
    t.fire('b', activator='player1')
    t.fire('r', activator='player1')
    t.wait(0.5)
    t.expect.sound('buttons/blip1.wav', ent='b', after='locked', within=(0, 0.2), count=1)
    t.expect.sound('buttons/bell1.wav', ent='r', after='locked', within=(0, 0.2), count=1)
    t.expect.sound('buttons/button2.wav', after='locked', count=0)
    t.expect.sound('buttons/blip2.wav', ent='b', after='open', count=1)
    t.expect.sound('buttons/button3.wav', ent='r', after='open', count=1)
    t.expect.precached('buttons/blip1.wav', kind='sound')
    t.expect.precached('buttons/bell1.wav', kind='sound')
