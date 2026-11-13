"""Sound files and sentence names instead of the numbered sound / sentence choices (Half-Life Unified SDK names
instead of indices, Featureful key names): func_button / func_rot_button / momentary_rot_button "noise" (the press
sound), the buttons' and doors' "locked_sentence_override" / "unlocked_sentence_override" (sentence group names),
the doors' "locked_sound_override" / "unlocked_sound_override", momentary_door "noise1" / "noise2". A set key wins
over the number; an empty one keeps the number."""

from etest import Suite

SUITE = Suite('sound_names', 'Sound files and sentence names instead of the numbered choices', players=1)
S = SUITE


def locks(t):
    """'lock': a locked master; 'open': a master unlocked at once (its one input fired)."""
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='never', target='lock', triggerstate='1')    # its one input, never fired
    t.point('multisource', targetname='open')
    t.point('trigger_relay', targetname='key', target='open', triggerstate='1')
    t.fire('key')
    t.wait(0.2)


@S.test('func_button "noise": the file plays when pressed instead of its numbered sound; empty: the number',
        players=1, covers=('func_button.noise', 'func_button.sounds'))
def button_noise(t):
    t.brush('func_button', targetname='b', size=(16, 32, 32), at=(-100, 0, 40), sounds='3', noise='buttons/blip1.wav',
            speed='100', wait='1')
    t.brush('func_button', targetname='n', size=(16, 32, 32), at=(100, 0, 40), sounds='3', speed='100', wait='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('b', activator='player1')
    t.fire('n', activator='player1')
    t.wait(0.3)
    t.expect.sound('buttons/blip1.wav', ent='b', after='go', count=1)
    t.expect.sound('buttons/button3.wav', ent='b', after='go', count=0)
    t.expect.sound('buttons/button3.wav', ent='n', after='go', count=1)


@S.test('func_rot_button "noise": the file plays when it turns instead of its numbered sound',
        players=1, covers=('func_rot_button.noise',))
def rot_button_noise(t):
    t.brush('func_rot_button', targetname='r', size=(16, 32, 32), at=(0, 0, 40), origin=True, sounds='3',
            noise='buttons/blip2.wav', distance='30', speed='100', wait='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('r', activator='player1')
    t.wait(0.3)
    t.expect.sound('buttons/blip2.wav', ent='r', after='go', count=1)
    t.expect.sound('buttons/button3.wav', ent='r', after='go', count=0)


@S.test('momentary_rot_button "noise": the file plays when it turns instead of its numbered sound',
        players=1, covers=('momentary_rot_button.noise',))
def momentary_noise(t):
    t.brush('momentary_rot_button', targetname='m', size=(8, 32, 32), at=(0, 40, 28), origin=True, target='sink',
            speed='90', distance='90', sounds='23', noise='buttons/bell1.wav')
    t.point('info_target', targetname='sink')
    t.wait(0.2)
    t.mark('go')
    t.fire('m', activator='player1')
    t.wait(0.3)
    t.expect.sound('buttons/bell1.wav', ent='m', after='go')
    t.expect.sound('lever3.wav', ent='m', after='go', count=0)


@S.test('func_button sentence names: locked, "locked_sentence_override" ND speaks instead of the numbered 1 (NA); '
        'unlocked, "unlocked_sentence_override" ED instead of 1 (EA); a button without them speaks the numbered ones',
        players=1, covers=('func_button.locked_sentence_override', 'func_button.unlocked_sentence_override',
                'func_button.locked_sentence', 'func_button.unlocked_sentence'))
def button_sentences(t):
    locks(t)
    t.brush('func_button', targetname='bl', size=(16, 32, 32), at=(-150, 0, 40), master='lock', locked_sentence='1',
            locked_sentence_override='ND')
    t.brush('func_button', targetname='bu', size=(16, 32, 32), at=(-50, 0, 40), master='open', unlocked_sentence='1',
            unlocked_sentence_override='ED')
    t.brush('func_button', targetname='nl', size=(16, 32, 32), at=(50, 0, 40), master='lock', locked_sentence='1')
    t.brush('func_button', targetname='nu', size=(16, 32, 32), at=(150, 0, 40), master='open', unlocked_sentence='1')
    t.wait(0.2)
    t.mark('go')
    for b in ('bl', 'bu', 'nl', 'nu'):
        t.fire(b, activator='player1')
    t.wait(0.5)
    t.expect.sound('ND', ent='bl', after='go', count=1)
    t.expect.sound('ED', ent='bu', after='go', count=1)
    t.expect.sound('NA', ent='nl', after='go', count=1)
    t.expect.sound('EA', ent='nu', after='go', count=1)


def door(t, name, x, **kv):
    return t.brush('func_door', targetname=name, size=(64, 16, 96), at=(x, 150, 0), tex='666MURMETAL',
                   angles='-90 0 0', speed='400', lip='8', wait='1', **kv)


@S.test('func_door sound files and sentence names: locked (Locked play on use), "locked_sound_override" and '
        '"locked_sentence_override" ND instead of the numbered 2 / 1; unlocked, "unlocked_sound_override" and '
        '"unlocked_sentence_override" ED instead of 3 / 1; a door without them plays the numbered ones',
        players=1, covers=('func_door.locked_sound_override', 'func_door.unlocked_sound_override',
                'func_door.locked_sentence_override', 'func_door.unlocked_sentence_override'))
def door_overrides(t):
    locks(t)
    door(t, 'dl', -180, master='lock', locked_play_on_use='1', locked_sound='2', locked_sentence='1',
         locked_sound_override='buttons/blip1.wav', locked_sentence_override='ND')
    door(t, 'du', -60, master='open', unlocked_sound='3', unlocked_sentence='1',
         unlocked_sound_override='buttons/blip2.wav', unlocked_sentence_override='ED')
    door(t, 'nl', 60, master='lock', locked_play_on_use='1', locked_sound='2', locked_sentence='1')
    door(t, 'nu', 180, master='open', unlocked_sound='3', unlocked_sentence='1')
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


@S.test('momentary_door "noise1" / "noise2": the files play as it moves and stops instead of the numbered '
        '"movesnd" / "stopsnd"', players=1, covers=('momentary_door.noise1', 'momentary_door.noise2'))
def momentary_door_noise(t):
    t.brush('momentary_door', targetname='md', size=(96, 16, 96), at=(0, 150, 0), angles='0 0 0', lip='6',
            speed='200', movesnd='1', stopsnd='1', noise1='buttons/bell1.wav', noise2='buttons/blip1.wav')
    t.wait(0.2)
    t.mark('go')
    t.fire('md', activator='player1', use='set', value=1)
    t.wait(1.5)
    t.expect.sound('buttons/bell1.wav', ent='md', after='go', min=1)
    t.expect.sound('buttons/blip1.wav', ent='md', after='go', min=1)
    t.expect.sound('doors/doormove1.wav', ent='md', after='go', count=0)
