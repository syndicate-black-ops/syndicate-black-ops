"""Video (branch feat/video-player, docs/VIDEO_PLAYER.md): game_playvideo sends PlayVideo (Aura's message) to the
activator, or to everybody with All Players; its files are precached (FastDL); bad names make it do nothing; a
master blocks it. The server's side only (what is sent and precached); the client's playback is checked by
tests/video/run_test.sh (decoder) and in a scripted client test (co_video_test)."""

from etest import Suite

SUITE = Suite('video', 'Video: game_playvideo (feat/video-player)', requires=('PlayVideo',))
S = SUITE
S.server_args = ('+developer', '0')    # the refused names print at developer 0 (the mapper sees them)
DOC = 'VIDEO_PLAYER.md'
ALL_PLAYERS = 1


@S.test('game_playvideo: the activator gets PlayVideo with the names; the files are precached', players=2,
        doc=DOC + '#game_playvideo')
def video_activator(t):
    t.point('game_playvideo', targetname='vid', video='sbo_sample.ogv', audio='test/alt.ogg')
    t.wait(0.2)
    t.mark('go')
    t.fire('vid', activator='player1')
    t.wait(0.3)
    t.expect.msg('PlayVideo', to='player1', fields={'video': 'sbo_sample.ogv', 'audio': 'test/alt.ogg'}, after='go',
                 within=(0, 0.1), count=1)
    t.expect.no_msg('PlayVideo', to='player2', after='go')
    t.expect.precached('video/sbo_sample.ogv', kind='generic')
    t.expect.precached('video/test/alt.ogg', kind='generic')


@S.test('game_playvideo All Players: one message to everybody; without a player as activator too', players=2,
        doc=DOC + '#game_playvideo')
def video_all(t):
    t.point('game_playvideo', targetname='vid', video='intro.ogv', spawnflags=str(ALL_PLAYERS))
    t.wait(0.2)
    t.mark('go')
    t.fire('vid')
    t.wait(0.3)
    # count 1 with two players: one MSG_ALL, not one message per player
    t.expect.msg('PlayVideo', fields={'video': 'intro.ogv', 'audio': ''}, after='go', within=(0, 0.1), count=1)


@S.test('game_playvideo without All Players: nothing when the activator is not a player', players=1,
        doc=DOC + '#game_playvideo')
def video_no_player(t):
    t.point('game_playvideo', targetname='vid', video='intro.ogv')
    t.wait(0.2)
    t.mark('go')
    t.fire('vid')
    t.wait(0.3)
    t.expect.no_msg('PlayVideo', after='go')


@S.test('game_playvideo: names outside video/ or of another type are refused (nothing sent or precached)',
        players=1, doc=DOC + '#game_playvideo')
def video_bad_names(t):
    t.point('game_playvideo', targetname='v1', video='../liblist.gam', spawnflags=str(ALL_PLAYERS))
    t.point('game_playvideo', targetname='v2', video='intro.avi', spawnflags=str(ALL_PLAYERS))
    t.point('game_playvideo', targetname='v3', video='ok_video.ogv', audio='../../x.ogg', spawnflags=str(ALL_PLAYERS))
    t.point('game_playvideo', targetname='v4', spawnflags=str(ALL_PLAYERS))
    # a second dot: the engine never downloads such a file, so the game refuses it (VideoShared_NameError)
    t.point('game_playvideo', targetname='v5', video='a-b.c.ogv', spawnflags=str(ALL_PLAYERS))
    t.point('game_playvideo', targetname='v6', video='ok_video.ogv', audio='talk.v2.ogg', spawnflags=str(ALL_PLAYERS))
    t.wait(0.2)
    t.mark('go')
    for n in ('v1', 'v2', 'v3', 'v4', 'v5', 'v6'):
        t.fire(n)
    t.wait(0.3)
    t.expect.no_msg('PlayVideo', after='go')
    t.expect.not_precached('liblist.gam')
    t.expect.not_precached('intro.avi')
    t.expect.not_precached('x.ogg')
    t.expect.not_precached('video/a-b.c.ogv')
    t.expect.not_precached('video/talk.v2.ogg')
    t.expect.console('video "a-b.c.ogv" refused (more than one ".": the engine never sends such a file to players')
    t.expect.console('audio "talk.v2.ogg" refused (more than one "."')


@S.test('game_playvideo with a master: nothing while the master is off, sent once it is on', players=1,
        doc=DOC + '#game_playvideo')
def video_master(t):
    t.point('multisource', targetname='ms')
    t.point('trigger_relay', targetname='unlock', target='ms', triggerstate='1')
    t.point('game_playvideo', targetname='vid', video='intro.ogv', master='ms', spawnflags=str(ALL_PLAYERS))
    t.wait(0.2)
    t.mark('off')
    t.fire('vid')
    t.wait(0.2)
    t.fire('unlock')
    t.wait(0.2)
    t.mark('on')
    t.fire('vid')
    t.wait(0.3)
    t.expect.no_msg('PlayVideo', after='off', within=(0, 0.15))
    t.expect.msg('PlayVideo', after='on', within=(0, 0.1), count=1)


# ---- movie screens: vgui_screen "moviefilename" (docs/WORLD_SCREENS.md#movie-screens) ----
WS_DEFINE, WS_STATE = 0, 1
F_ACTIVE, F_NOINPUT, F_MOVIE, F_LOOP, F_SOUND2D, F_NOSOUND = 1, 8, 16, 32, 64, 128
START_INACTIVE, MOVIE_SOUND2D, MOVIE_NOSOUND = 1, 4, 8
WSDOC = 'WORLD_SCREENS.md#movie-screens'


def movie_screen(t, name, **kv):
    t.point('vgui_screen', targetname=name, angles='0 90 0', width='128', height='72', **kv)


@S.test('vgui_screen movie: a player who joins later gets the movie with the seconds it has played', players=1,
        doc=WSDOC)
def movie_late_join(t):
    movie_screen(t, 'scr', moviefilename='sbo_sample.ogv', looping='1', health='7', radius='800',
                 spawnflags=str(START_INACTIVE))
    t.wait(0.2)
    t.fire('scr', use='on')
    t.wait(2.0)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.0)
    t.expect.msg('WScreen', fields={'op': WS_DEFINE, 'movie': 'sbo_sample.ogv', 'flags': F_ACTIVE | F_NOINPUT | F_MOVIE | F_LOOP,
                                    'volume': 7, 'radius': 800, 'elapsed_ms': {'range': (1900, 3300)}},
                 after='join', within=(0, 0.9), min=1)
    t.expect.precached('video/sbo_sample.ogv', kind='generic')
    t.console('test_player remove latejoin')


@S.test('vgui_screen movie: toggling sends the state (on: it starts over, off: stopped); no panel layout needed',
        players=1, doc=WSDOC)
def movie_toggle(t):
    movie_screen(t, 'scr', moviefilename='sbo_sample.ogv')
    t.wait(0.2)
    t.mark('off')
    t.fire('scr')
    t.wait(0.2)
    t.mark('on')
    t.fire('scr')
    t.wait(0.2)
    t.expect.msg('WScreen', fields={'op': WS_STATE, 'flags': F_NOINPUT | F_MOVIE}, after='off', within=(0, 0.15), count=1)
    t.expect.msg('WScreen', fields={'op': WS_STATE, 'flags': F_ACTIVE | F_NOINPUT | F_MOVIE}, after='on',
                 within=(0, 0.15), count=1)
    t.expect.not_precached('example_screen.res')


@S.test('vgui_screen movie: sound everywhere / no sound flags, default volume and radius', players=1, doc=WSDOC)
def movie_sound_flags(t):
    movie_screen(t, 's2d', moviefilename='mv_2d.ogv', spawnflags=str(MOVIE_SOUND2D | START_INACTIVE))
    movie_screen(t, 'snone', moviefilename='mv_none.ogv', spawnflags=str(MOVIE_NOSOUND | START_INACTIVE))
    t.wait(0.2)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.0)
    t.expect.msg('WScreen', fields={'op': WS_DEFINE, 'movie': 'mv_2d.ogv', 'flags': F_NOINPUT | F_MOVIE | F_SOUND2D,
                                    'volume': 10, 'radius': 1250}, after='join', within=(0, 0.9), min=1)
    t.expect.msg('WScreen', fields={'op': WS_DEFINE, 'movie': 'mv_none.ogv', 'flags': F_NOINPUT | F_MOVIE | F_NOSOUND},
                 after='join', within=(0, 0.9), min=1)
    t.console('test_player remove latejoin')


@S.test('vgui_screen movie: a name outside video/ or not .ogv removes the screen (nothing sent or precached)',
        players=1, doc=WSDOC)
def movie_bad_name(t):
    movie_screen(t, 'b1', moviefilename='../maps/x.ogv')
    movie_screen(t, 'b2', moviefilename='clip.webm')
    movie_screen(t, 'b3', moviefilename='news.part2.ogv')    # a second dot: never downloaded, refused
    t.wait(0.2)
    t.mark('join')
    t.console('test_player add latejoin')
    t.wait(1.0)
    # (the other tests' screens are in the same map: they are sent to the new player)
    t.expect.msg('WScreen', fields={'movie': '../maps/x.ogv'}, after='join', min=0, count=0)
    t.expect.msg('WScreen', fields={'movie': 'clip.webm'}, after='join', min=0, count=0)
    t.expect.msg('WScreen', fields={'op': WS_DEFINE, 'movie': 'sbo_sample.ogv'}, after='join', within=(0, 0.9), min=1)
    t.expect.not_precached('x.ogv')
    t.expect.not_precached('clip.webm')
    t.expect.msg('WScreen', fields={'movie': 'news.part2.ogv'}, after='join', min=0, count=0)
    t.expect.not_precached('video/news.part2.ogv')
    t.expect.console('"moviefilename" "news.part2.ogv" refused (more than one "."')
    t.console('test_player remove latejoin')
