"""Map music (branch feat/cs-ambient-sound, docs/MAP_MUSIC.md): ambient_music and an ambient_generic with an .mp3
send the track state in the message MapMusic (the client library runs the engine's "mp3" command), to everybody
on a start / stop and to a client that joins; ambient_generic play modes. Its own suite: music is global state (one
track at a time). Nothing here plays sound: the server's side only (what is sent, how often)."""

from etest import Suite

SUITE = Suite('music', 'Map music: ambient_music, MP3 ambient_generic, play modes (feat/cs-ambient-sound)',
              requires=('MapMusic',))
S = SUITE
DOC = 'MAP_MUSIC.md'
STOP, PLAY, LOOP = 0, 1, 2
# ambient_music flags
START_SILENT, MUSIC_LOOP = 1, 2
# ambient_generic flags
AG_EVERYWHERE, AG_START_SILENT, AG_NOT_TOGGLED = 1, 16, 32


@S.test('ambient_music Start Silent + Loop: the first trigger sends loop for everybody, the second a stop', players=1,
        doc=DOC + '#ambient_music')
def music_toggle(t):
    t.point('ambient_music', targetname='mus', message='test_music/loop_a.mp3', spawnflags=str(START_SILENT | MUSIC_LOOP))
    t.wait(0.2)
    t.mark('on')
    t.fire('mus')
    t.wait(0.3)
    t.mark('off')
    t.fire('mus')
    t.wait(0.3)
    t.expect.msg('MapMusic', fields={'state': LOOP, 'path': 'sound/test_music/loop_a.mp3'}, after='on', within=(0, 0.1),
                 count=1)
    t.expect.msg('MapMusic', fields={'state': STOP, 'path': ''}, after='off', within=(0, 0.1), count=1)


@S.test('ambient_music: a second track replaces the first; stopping the replaced one does not stop it', players=1,
        doc=DOC + '#ambient_music')
def music_replace(t):
    t.point('ambient_music', targetname='m1', message='test_music/one.mp3', spawnflags=str(START_SILENT))
    t.point('ambient_music', targetname='m2', message='test_music/two.mp3', spawnflags=str(START_SILENT | MUSIC_LOOP))
    t.wait(0.2)
    t.mark('go')
    t.fire('m1')
    t.wait(0.2)
    t.fire('m2')
    t.wait(0.2)
    t.mark('old_off')
    t.fire('m1')            # m1 still counts as on: this turns it off, but m2 started last
    t.wait(0.3)
    t.mark('m2_off')
    t.fire('m2', use='off')
    t.wait(0.3)
    t.expect.msg('MapMusic', fields={'state': PLAY, 'path': 'sound/test_music/one.mp3'}, after='go', count=1)
    t.expect.msg('MapMusic', fields={'state': LOOP, 'path': 'sound/test_music/two.mp3'}, after='go', count=1)
    t.expect.no_msg('MapMusic', after='old_off', within=(0, 0.25))
    t.expect.msg('MapMusic', fields={'state': STOP}, after='m2_off', within=(0, 0.1), count=1)


@S.test('ambient_music: a joining player gets the current track (MSG_ONE at its HUD init)', players=1,
        doc=DOC + '#what-players-get')
def music_late_join(t):
    t.point('ambient_music', targetname='mus', message='test_music/late.mp3', spawnflags=str(START_SILENT | MUSIC_LOOP))
    t.wait(0.2)
    t.fire('mus')
    t.wait(0.3)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.5)
    t.mark('done')
    t.fire('mus', use='off')
    t.wait(0.2)
    t.expect.msg('MapMusic', fields={'state': LOOP, 'path': 'sound/test_music/late.mp3'}, after='join', within=(0, 1.4),
                 count=1)
    t.console('test_player remove latejoin')


@S.test('ambient_music: a path that is not a plain sound/....mp3 name is refused (entity removed, nothing sent)',
        players=1, doc=DOC + '#ambient_music')
def music_bad_path(t):
    t.point('ambient_music', targetname='bad1', message='test_music/two words.mp3', spawnflags=str(START_SILENT))
    t.point('ambient_music', targetname='bad2', message='../../valve/media/Half-Life01.mp3', spawnflags=str(START_SILENT))
    t.point('ambient_music', targetname='bad3', message='test_music/x;quit.mp3', spawnflags=str(START_SILENT))
    t.point('ambient_music', targetname='bad4', message='test_music/' + 'a' * 60 + '.mp3', spawnflags=str(START_SILENT))
    t.wait(0.3)
    t.mark('go')
    for n in ('bad1', 'bad2', 'bad3', 'bad4'):
        t.fire(n)
    t.wait(0.3)
    t.expect.no_msg('MapMusic', after='go')


@S.test('ambient_generic with an .mp3 plays it as music; Play Mode Loop loops it', players=1, doc=DOC + '#mp3-as-the-sound')
def generic_mp3(t):
    t.point('ambient_generic', targetname='gen', message='test_music/generic.mp3', health='10', playmode='2',
            spawnflags=str(AG_START_SILENT))
    t.wait(0.2)
    t.mark('on')
    t.fire('gen')
    t.wait(0.3)
    t.mark('off')
    t.fire('gen')
    t.wait(0.3)
    t.expect.msg('MapMusic', fields={'state': LOOP, 'path': 'sound/test_music/generic.mp3'}, after='on', within=(0, 0.1),
                 count=1)
    t.expect.msg('MapMusic', fields={'state': STOP}, after='off', within=(0, 0.1), count=1)
    t.expect.sound('generic.mp3', count=0)


@S.test('ambient_generic Play Mode Loop on a WAV without cue points: the server restarts it once per length', players=1,
        doc=DOC + '#play-mode-playmode')
def playmode_loop(t):
    # anzym/gun2.wav: 1.04 s, no cue points
    t.point('ambient_generic', targetname='lp', message='anzym/gun2.wav', health='10', playmode='2',
            spawnflags=str(AG_START_SILENT | AG_EVERYWHERE))
    t.wait(0.2)
    t.mark('on')
    t.fire('lp')
    t.wait(3.4)
    t.mark('off')
    t.fire('lp')
    t.wait(0.2)
    t.expect.sound('anzym/gun2.wav', after='on', within=(0, 3.3), min=3)
    # forced loop: one restart per length (about 1 s), never one per frame
    t.expect.sound('anzym/gun2.wav', after='on', within=(0.2, 3.3), min=2)


@S.test('ambient_generic Play Mode on a WAV the game cannot read: the default behavior, no sound every frame',
        players=1, doc=DOC + '#play-mode-playmode')
def playmode_missing_wav(t):
    # Before the fix the forced loop of a file without a length re-armed its think every frame and sent a sound
    # message each time.
    t.point('ambient_generic', targetname='miss', message='test_music/no_such_file.wav', health='10', playmode='2',
            spawnflags=str(AG_EVERYWHERE))
    t.point('ambient_generic', targetname='missl', message='test_music/no_such_file2.wav', health='10', playmode='6',
            linearmin='1', linearmax='4', spawnflags='0')
    t.mark('run')
    t.wait(2.0)
    t.expect.sound('no_such_file.wav', after='run', within=(0, 2.0), count=0)
    t.expect.sound('no_such_file2.wav', after='run', within=(0, 2.0), count=0)
