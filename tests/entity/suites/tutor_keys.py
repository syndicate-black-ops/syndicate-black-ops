"""env_tutor keys (docs/TUTOR_PORT.md "Keys", sdk/syndicate.fgd) the suites sbo / tutor do not check: the "TutorText"
message field by field (type, icon, sound and their defaults per type, colors, alignment, X / Y, custom icon and
sound files with their precache), holdtime and closetarget, target, priority / interrupt / lifetime in the queue,
Repeatable with maxshows, master. Each test closes what it opened (Off). The window itself needs the client layer."""

from etest import Suite

SUITE = Suite('tutor_keys', 'env_tutor: keys and the TutorText message', players=1)
S = SUITE
DOC = 'TUTOR_PORT.md'
MSG, FRIEND, ENEMY = 'events/tutor_msg.wav', 'events/friend_died.wav', 'events/enemy_died.wav'


def tutor(t, name, **kw):
    kw.setdefault('spawnflags', '9')        # activator only, repeatable
    kw.setdefault('message', name)
    return t.point('env_tutor', targetname=name, **kw)


@S.test('every display key in the message: type, icon, sound, text / icon / background / border colors (alpha 255 '
        'when left out), alignment, X / Y as parts of the screen x 10000, the text', players=1, doc=DOC,
        covers=('env_tutor', 'env_tutor.message', 'env_tutor.type=8', 'env_tutor.icon=3', 'env_tutor.sound=3',
                'env_tutor.color', 'env_tutor.iconcolor', 'env_tutor.bgcolor', 'env_tutor.bordercolor',
                'env_tutor.align=4', 'env_tutor.x', 'env_tutor.y'))
def keys(t):
    tutor(t, 'k', message='Find the key', type='8', icon='3', sound='3', color='1 2 3', iconcolor='4 5 6',
          bgcolor='7 8 9 10', bordercolor='11 12 13', align='4', x='0.25', y='0.5', holdtime='0')
    t.mark('go')
    t.fire('k', activator='player1', use='on')
    t.wait(0.2)
    t.fire('k', activator='player1', use='off')
    t.wait(0.1)
    t.expect.msg('TutorText', to='player1', count=1, after='go',
                 fields={'type': 8, 'flags': 2 | 4 | 8 | 16, 'align': 4, 'x': 2500, 'y': 5000, 'icon': 2,
                         'sound': FRIEND, 'color': [1, 2, 3], 'iconcolor': [4, 5, 6], 'bgcolor': [7, 8, 9, 10],
                         'bordercolor': [11, 12, 13, 255], 'text': 'Find the key'})


@S.test('keys left out: type 1, the type\'s icon (i) and sound (tutor_msg), no colors, top right, X / Y from the '
        'alignment (-1); an alignment out of range (12) is top right', players=1, doc=DOC,
        covers=('env_tutor.type=1', 'env_tutor.icon=0', 'env_tutor.sound=0', 'env_tutor.align=0'))
def defaults(t):
    tutor(t, 'd', holdtime='0')
    tutor(t, 'bad', holdtime='0', align='12')
    t.mark('go')
    t.fire('d', activator='player1', use='on')
    t.wait(0.2)
    t.fire('d', activator='player1', use='off')
    t.wait(0.1)
    t.mark('bad')
    t.fire('bad', activator='player1', use='on')
    t.wait(0.2)
    t.fire('bad', activator='player1', use='off')
    t.wait(0.1)
    t.expect.msg('TutorText', to='player1', count=1, after='go', within=(0, 0.25),
                 fields={'type': 1, 'flags': 0, 'align': 0, 'x': -1, 'y': -1, 'icon': 1, 'sound': MSG, 'text': 'd'})
    t.expect.msg('TutorText', to='player1', count=1, after='bad', fields={'align': 0, 'text': 'bad'})


TYPES = {1: (1, MSG), 2: (3, FRIEND), 4: (3, ENEMY), 8: (1, MSG), 16: (1, MSG), 32: (1, MSG), 64: (1, MSG),
         128: (1, MSG), 256: (1, MSG)}


@S.test('every Type: sent as given, with the type\'s icon (skull for FriendDeath / EnemyDeath, else i) and sound '
        '(friend_died / enemy_died for those, else tutor_msg)', players=1, doc=DOC,
        covers=tuple('env_tutor.type=%d' % k for k in TYPES))
def types(t):
    for k in TYPES:
        tutor(t, 'ty%d' % k, type=str(k), holdtime='0')
    for k, (icon, sound) in TYPES.items():
        t.mark('m%d' % k)
        t.fire('ty%d' % k, activator='player1', use='on')
        t.wait(0.1)
        t.fire('ty%d' % k, activator='player1', use='off')
        t.wait(0.05)
        t.expect.msg('TutorText', to='player1', count=1, after='m%d' % k, within=(0, 0.1),
                     fields={'type': k, 'icon': icon, 'sound': sound})


ICONS = {0: 1, 1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5}
SOUNDS = {0: MSG, 1: '', 2: MSG, 3: FRIEND, 4: ENEMY}


@S.test('every Icon choice (type\'s, none, i, !, skull, bigger i, bigger !) and every Sound choice (type\'s, none, '
        'tutor_msg, friend_died, enemy_died) in the message', players=1, doc=DOC,
        covers=tuple('env_tutor.icon=%d' % k for k in ICONS) + tuple('env_tutor.sound=%d' % k for k in SOUNDS))
def icons_sounds(t):
    for k in ICONS:
        tutor(t, 'ic%d' % k, icon=str(k), holdtime='0')
    for k in SOUNDS:
        tutor(t, 'so%d' % k, sound=str(k), holdtime='0')
    for k, icon in ICONS.items():
        t.mark('i%d' % k)
        t.fire('ic%d' % k, activator='player1', use='on')
        t.wait(0.1)
        t.fire('ic%d' % k, activator='player1', use='off')
        t.wait(0.05)
        t.expect.msg('TutorText', to='player1', count=1, after='i%d' % k, within=(0, 0.1),
                     fields={'icon': icon, 'text': 'ic%d' % k})
    for k, sound in SOUNDS.items():
        t.mark('s%d' % k)
        t.fire('so%d' % k, activator='player1', use='on')
        t.wait(0.1)
        t.fire('so%d' % k, activator='player1', use='off')
        t.wait(0.05)
        t.expect.msg('TutorText', to='player1', count=1, after='s%d' % k, within=(0, 0.1),
                     fields={'sound': sound, 'text': 'so%d' % k})


@S.test('custom icon .tga (sent as 255 + its path without .tga) overrides Icon, custom sound overrides Sound; both '
        'are precached for the download', players=1, doc=DOC,
        covers=('env_tutor.iconfile', 'env_tutor.soundfile'))
def custom_files(t):
    tutor(t, 'c', icon='4', iconfile='gfx/env/boatft.tga', sound='3', soundfile='buttons/blip1.wav', holdtime='0')
    t.mark('go')
    t.fire('c', activator='player1', use='on')
    t.wait(0.2)
    t.fire('c', activator='player1', use='off')
    t.wait(0.1)
    t.expect.msg('TutorText', to='player1', count=1, after='go',
                 fields={'icon': 255, 'iconfile': 'gfx/env/boatft', 'sound': 'buttons/blip1.wav'})
    t.expect.precached('gfx/env/boatft.tga', kind='generic')
    t.expect.precached('buttons/blip1.wav', kind='sound')


@S.test('holdtime: the window closes after it (TutorClose) and closetarget fires with the player; target fires when '
        'shown; holdtime 0 stays until Off, which fires closetarget too', players=1, doc=DOC,
        covers=('env_tutor.holdtime', 'env_tutor.closetarget', 'env_tutor.target'))
def holdtime(t):
    tutor(t, 'h', holdtime='0.5', target='shown', closetarget='closed')
    tutor(t, 'z', holdtime='0', closetarget='closed_z')
    for n in ('shown', 'closed', 'closed_z'):
        t.point('info_target', targetname=n)
    t.mark('go')
    t.fire('h', activator='player1', use='on')
    t.wait(1.0)
    t.mark('z')
    t.fire('z', activator='player1', use='on')
    t.wait(1.0)
    t.mark('off')
    t.fire('z', activator='player1', use='off')
    t.wait(0.1)
    t.expect.used('shown', activator='player1', count=1, after='go', within=(0, 0.1))
    t.expect.used('closed', activator='player1', count=1, after='go', within=(0.4, 0.75))
    t.expect.msg('TutorClose', to='player1', count=1, after='go', within=(0.4, 0.75))
    t.expect.not_used('closed_z', after='z', within=(0, 0.95))
    t.expect.used('closed_z', count=1, after='off')


@S.test('interrupt 1 with at least the shown priority replaces the shown message at once (its closetarget fires); '
        'interrupt 1 with a lower priority waits, and comes when the window frees', players=1, doc=DOC,
        covers=('env_tutor.priority', 'env_tutor.interrupt=1'))
def interrupt(t):
    tutor(t, 'a', holdtime='2', priority='2', closetarget='a_closed')
    tutor(t, 'low', holdtime='0.3', priority='1', interrupt='1')
    tutor(t, 'cut', holdtime='0.3', priority='2', interrupt='1')
    t.point('info_target', targetname='a_closed')
    t.mark('go')
    t.fire('a', activator='player1', use='on')
    t.wait(0.1)
    t.fire('low', activator='player1', use='on')       # interrupt, but a lower priority: waits
    t.wait(0.3)
    t.mark('cut')
    t.fire('cut', activator='player1', use='on')       # interrupt, same priority: at once
    t.wait(1.0)
    t.expect.msg('TutorText', to='player1', count=1, after='go', within=(0, 0.35), fields={'text': 'a'})
    t.expect.msg('TutorText', to='player1', count=0, after='go', within=(0, 0.35), fields={'text': 'low'})
    t.expect.msg('TutorText', to='player1', count=1, after='cut', within=(0, 0.1), fields={'text': 'cut'})
    t.expect.used('a_closed', count=1, after='cut', within=(0, 0.1))
    t.expect.msg('TutorText', to='player1', count=1, after='cut', within=(0.25, 0.5), fields={'text': 'low'})


@S.test('priority: when the window frees, the waiting message with the highest priority comes first, then the next; '
        'interrupt 0 waits for the whole holdtime', players=1, doc=DOC,
        covers=('env_tutor.priority', 'env_tutor.interrupt=0'))
def priority(t):
    tutor(t, 'busy', holdtime='0.5', priority='9')
    tutor(t, 'p1', holdtime='0.3', priority='1')
    tutor(t, 'p5', holdtime='0.3', priority='5')
    t.mark('go')
    t.fire('busy', activator='player1', use='on')
    t.wait(0.1)
    t.fire('p1', activator='player1', use='on')
    t.fire('p5', activator='player1', use='on')
    t.wait(1.5)
    t.expect.msg('TutorText', to='player1', count=1, after='go', within=(0.4, 0.7), fields={'text': 'p5'})
    t.expect.msg('TutorText', to='player1', count=1, after='go', within=(0.75, 1.05), fields={'text': 'p1'})


@S.test('a waiting message with interrupt 1 and at least the shown priority replaces the shown one at once, also '
        'while a higher-priority message without interrupt waits too', players=1, doc=DOC,
        covers=('env_tutor.interrupt=1',))
def interrupt_behind_higher(t):
    tutor(t, 'a', holdtime='2', priority='2')
    tutor(t, 'high', holdtime='0.3', priority='5')
    tutor(t, 'cut', holdtime='0.3', priority='2', interrupt='1')
    t.fire('a', activator='player1', use='on')
    t.wait(0.1)
    t.fire('high', activator='player1', use='on')
    t.wait(0.2)
    t.mark('cut')
    t.fire('cut', activator='player1', use='on')
    t.wait(0.3)
    t.fire('a', activator='player1', use='off')
    t.fire('high', activator='player1', use='off')
    t.fire('cut', activator='player1', use='off')
    t.wait(0.2)
    t.expect.msg('TutorText', to='player1', count=1, after='cut', within=(0, 0.1), fields={'text': 'cut'})


@S.test('lifetime: a waiting message older than its lifetime is dropped; -1 never waits (not shown while the window '
        'is busy); 0 waits as long as needed', players=1, doc=DOC, covers=('env_tutor.lifetime',))
def lifetime(t):
    tutor(t, 'busy', holdtime='1')
    tutor(t, 'short', holdtime='0.2', lifetime='0.5')
    tutor(t, 'never', holdtime='0.2', lifetime='-1')
    tutor(t, 'patient', holdtime='0.2', lifetime='0')
    t.mark('go')
    t.fire('busy', activator='player1', use='on')
    t.wait(0.1)
    for n in ('short', 'never', 'patient'):
        t.fire(n, activator='player1', use='on')
    t.wait(1.8)
    t.expect.msg('TutorText', to='player1', count=0, after='go', fields={'text': 'short'})
    t.expect.msg('TutorText', to='player1', count=0, after='go', fields={'text': 'never'})
    t.expect.msg('TutorText', to='player1', count=1, after='go', within=(0.9, 1.3), fields={'text': 'patient'})


@S.test('Repeatable with maxshows 2: shown twice, the third trigger shows nothing; without Repeatable: once per player',
        players=1, doc=DOC, covers=('env_tutor.maxshows', 'env_tutor:8'))
def maxshows(t):
    tutor(t, 'two', holdtime='0.1', maxshows='2')
    tutor(t, 'once', holdtime='0.1', spawnflags='1')
    t.mark('go')
    for _ in range(3):
        t.fire('two', activator='player1', use='on')
        t.wait(0.3)
    for _ in range(2):
        t.fire('once', activator='player1', use='on')
        t.wait(0.3)
    t.expect.msg('TutorText', to='player1', count=2, after='go', fields={'text': 'two'})
    t.expect.msg('TutorText', to='player1', count=1, after='go', fields={'text': 'once'})


@S.test('master: while the env_state is off a trigger shows nothing; on: shown', players=1, doc=DOC,
        covers=('env_tutor.master',))
def master(t):
    t.point('env_state', targetname='st')
    tutor(t, 'm', holdtime='0.2', master='st')
    t.mark('locked')
    t.fire('m', activator='player1', use='on')
    t.wait(0.3)
    t.fire('st', use='on')
    t.wait(0.1)
    t.mark('open')
    t.fire('m', activator='player1', use='on')
    t.wait(0.3)
    t.expect.msg('TutorText', to='player1', count=0, after='locked', within=(0, 0.3), fields={'text': 'm'})
    t.expect.msg('TutorText', to='player1', count=1, after='open', fields={'text': 'm'})
