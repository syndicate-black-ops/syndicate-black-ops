"""Tier 5, final pass: func_trackchange (FGD base Trackchange: PlatSounds and Fire On keys).
Each platform has a top and a bottom path_track and a train far away on its own track (Find needs all three); fired,
it goes down (or up with Start at Bottom), plays its Move / Stop Sound with its Sound radius and fires its Fire On
targets with their use types (plats.cpp CFuncTrackChange, CFuncPlat, CBasePlatTrain)."""

from etest import Suite

SUITE = Suite('fgd_trackchange', 'Tier 5: func_trackchange / func_trackautochange keys, sounds, flags')
S = SUITE
DOC = 'FGD Trackchange'
CLASSES = ('func_trackchange',)   # func_trackautochange moves only when its train drives onto it: another suite
MOVE = {1: 'plats/bigmove1.wav', 2: 'plats/bigmove2.wav', 3: 'plats/elevmove1.wav', 4: 'plats/elevmove2.wav',
        5: 'plats/elevmove3.wav', 6: 'plats/freightmove1.wav', 7: 'plats/freightmove2.wav',
        8: 'plats/heavymove1.wav', 9: 'plats/rackmove1.wav', 10: 'plats/railmove1.wav',
        11: 'plats/squeekmove1.wav', 12: 'plats/talkmove1.wav', 13: 'plats/talkmove2.wav'}
STOP = {1: 'plats/bigstop1.wav', 2: 'plats/bigstop2.wav', 3: 'plats/freightstop1.wav', 4: 'plats/heavystop2.wav',
        5: 'plats/rackstop1.wav', 6: 'plats/railstop1.wav', 7: 'plats/squeekstop1.wav', 8: 'plats/talkstop1.wav'}
RADII = {-2: 2.0, -1: 1.25, 0: 0.8, 1: 0.5, 2: 0.25, 3: 0.0}
USE = {0: 0, 1: 1, 2: 3}
FIREON = ('fireonstart', 'fireonstop', 'fireonopening', 'fireonopened', 'fireonclosing', 'fireonclosed')


def train(t):
    """the train every platform names, far from them on a track of its own"""
    t.point('path_track', targetname='far1', target='far2', at=(-220, -220, 0))
    t.point('path_track', targetname='far2', at=(-120, -220, 0))
    t.brush('func_tracktrain', targetname='tr', size=(32, 32, 16), at=(-220, -220, 0), target='far1', wheels='8',
            height='0', speed='0')


def platform(t, cls, name, x, y=60, **kv):
    t.point('path_track', targetname=name + '_top', at=(x, y, 72))
    t.point('path_track', targetname=name + '_bot', at=(x, y, 8))
    kv.setdefault('speed', '200')
    kv.setdefault('height', '64')
    return t.brush(cls, targetname=name, size=(48, 48, 16), at=(x, y, 64), tex='666MURMETAL', origin=True,
                   toptrack=name + '_top', bottomtrack=name + '_bot', train='tr', **kv)


def sound_case(cls, half):
    moves = [m for m in MOVE if (m <= 7) == (half == 0)]

    @S.test('%s Move Sound %s and Stop Sound: each platform plays them when it moves and stops; 0: none' % (
            cls, ', '.join(str(m) for m in moves)), doc=DOC, players=1,
            covers=(cls, '%s.toptrack' % cls, '%s.bottomtrack' % cls, '%s.train' % cls, '%s.movesnd' % cls,
                    '%s.stopsnd' % cls) + tuple('%s.movesnd=%d' % (cls, m) for m in moves)
            + tuple('%s.stopsnd=%d' % (cls, (m - 1) % 8 + 1) for m in moves)
            + (('%s.movesnd=0' % cls, '%s.stopsnd=0' % cls) if half == 0 else ()))
    def case(t):
        train(t)
        plats = [(m, (m - 1) % 8 + 1) for m in moves] + ([(0, 0)] if half == 0 else [])
        for i, (m, s) in enumerate(plats):
            platform(t, cls, 'p%d' % i, -150 + (i % 4) * 100, -60 + (i // 4) * 120, movesnd=str(m), stopsnd=str(s))
        t.wait(0.5)
        t.mark('go')
        for i in range(len(plats)):
            t.fire('p%d' % i, activator='player1')
        t.wait(1.5)
        for i, (m, s) in enumerate(plats):
            if m:
                t.expect.sound(MOVE[m], ent='p%d' % i, after='go', min=1)
                t.expect.sound(STOP[s], ent='p%d' % i, after='go', min=1)
            else:
                t.expect.sound('plats/', ent='p%d' % i, after='go', count=0)
    case.__name__ = '%s_sounds_%d' % (cls.replace('func_', ''), half)
    return case


def radius_case(cls):
    @S.test('%s Sound radius: each choice is the attenuation of its move sound' % cls, doc=DOC, players=1,
            covers=tuple('%s.soundradius=%d' % (cls, k) for k in RADII))
    def case(t):
        train(t)
        for i, k in enumerate(RADII):
            platform(t, cls, 's%d' % i, -150 + (i % 3) * 120, -40 + (i // 3) * 140, movesnd='1', stopsnd='1',
                     soundradius=str(k))
        t.wait(0.5)
        t.mark('go')
        for i in range(len(RADII)):
            t.fire('s%d' % i, activator='player1')
        t.wait(0.5)
        for i, (k, attn) in enumerate(RADII.items()):
            t.expect.sound('plats/bigmove1.wav', ent='s%d' % i, attn=attn, min=1, after='go')
    case.__name__ = '%s_radius' % cls.replace('func_', '')
    return case


def custom_case(cls):
    @S.test('%s Custom move / stop sound: those files instead of the numbered sounds' % cls, doc=DOC, players=1,
            covers=('%s.custommovesnd' % cls, '%s.customstopsnd' % cls))
    def case(t):
        train(t)
        platform(t, cls, 'c', 0, movesnd='1', stopsnd='1', custommovesnd='doors/doormove3.wav',
                 customstopsnd='doors/doorstop5.wav')
        t.wait(0.5)
        t.mark('go')
        t.fire('c', activator='player1')
        t.wait(1.0)
        t.expect.sound('doors/doormove3.wav', ent='c', after='go', min=1)
        t.expect.sound('doors/doorstop5.wav', ent='c', after='go', min=1)
        t.expect.sound('plats/', ent='c', after='go', count=0)
    case.__name__ = '%s_custom' % cls.replace('func_', '')
    return case


def fireon_case(cls, shift):
    st = {k: (i + shift) % 3 for i, k in enumerate(FIREON)}

    @S.test('%s Fire On targets with use types shifted by %d: going down (closing) then up (opening)' % (cls, shift),
            doc=DOC, players=1, covers=tuple('%s.%s' % (cls, k) for k in FIREON)
            + tuple('%s.%s_triggerstate=%d' % (cls, k, v) for k, v in st.items()))
    def case(t):
        train(t)
        kv = {}
        for k, v in st.items():
            kv[k] = k
            kv[k + '_triggerstate'] = str(v)
            t.point('info_target', targetname=k)
        platform(t, cls, 'f', 0, names=FIREON, **kv)
        t.wait(0.5)
        t.mark('go')
        t.fire('f', activator='player1')
        t.wait(1.0)
        t.mark('up')
        t.fire('f', activator='player1')
        t.wait(1.0)
        x = t.expect
        x.fired('fireonstart', use=USE[st['fireonstart']], min=2, after='go')
        x.fired('fireonstop', use=USE[st['fireonstop']], min=2, after='go')
        x.fired('fireonclosing', use=USE[st['fireonclosing']], count=1, after='go', within=(0, 0.5))
        x.fired('fireonclosed', use=USE[st['fireonclosed']], count=1, after='go', within=(0, 1.0))
        x.fired('fireonopening', use=USE[st['fireonopening']], count=1, after='up', within=(0, 0.5))
        x.fired('fireonopened', use=USE[st['fireonopened']], count=1, after='up', within=(0, 1.0))
    case.__name__ = '%s_fireon_%d' % (cls.replace('func_', ''), shift)
    return case


def flags_case(cls):
    @S.test('%s Start at Bottom (8): the first trigger takes it up 64; Rotate Only (16) with Spin amount 90: it turns '
            'and stays at its height; X Axis (64) / Y Axis (128): it turns about that axis' % cls, doc=DOC, players=1,
            covers=('%s:8' % cls, '%s:16' % cls, '%s:64' % cls, '%s:128' % cls, '%s.rotation' % cls,
                    '%s.height' % cls, '%s.speed' % cls))
    def case(t):
        train(t)
        platform(t, cls, 'bot', -150, spawnflags='8')
        platform(t, cls, 'rot', -50, spawnflags='16', rotation='90', speed='90')
        platform(t, cls, 'ax', 50, spawnflags='16|64'.replace('|', '') if False else str(16 | 64), rotation='90',
                 speed='90')
        platform(t, cls, 'ay', 150, spawnflags=str(16 | 128), rotation='90', speed='90')
        t.wait(0.5)
        t.snap('a', 'bot', 'origin')
        t.snap('a', 'rot', 'origin', 'angles')
        t.snap('a', 'ax', 'angles')
        t.snap('a', 'ay', 'angles')
        for n in ('bot', 'rot', 'ax', 'ay'):
            t.fire(n, activator='player1')
        t.wait(2.0)
        t.snap('b', 'bot', 'origin')
        t.snap('b', 'rot', 'origin', 'angles')
        t.snap('b', 'ax', 'angles')
        t.snap('b', 'ay', 'angles')
        t.expect.moved('a', 'b', 'bot', delta=(0, 0, 64), tol=2)
        t.expect.moved('a', 'b', 'rot', delta=(0, 0, 0), tol=1)
        # at the top it is turned by Spin amount; going down it turns back to 0
        t.expect.value('a', 'rot', 'angles.y', 90, tol=2)
        t.expect.value('b', 'rot', 'angles.y', 0, tol=2)
        t.expect.value('a', 'ax', 'angles.z', 90, tol=2)
        t.expect.value('b', 'ax', 'angles.z', 0, tol=2)
        t.expect.value('a', 'ay', 'angles.x', 90, tol=2)
        t.expect.value('b', 'ay', 'angles.x', 0, tol=2)
    case.__name__ = '%s_flags' % cls.replace('func_', '')
    return case


for _cls in CLASSES:
    sound_case(_cls, 0)
    sound_case(_cls, 1)
    radius_case(_cls)
    custom_case(_cls)
    for _shift in (0, 1, 2):
        fireon_case(_cls, _shift)
    flags_case(_cls)


@S.test('func_trackautochange fired with no activator (trigger_auto, timers): not the train, so it does not move and '
        'the server keeps running (Use read the activator\'s classname without a NULL check and crashed; Half-Life '
        'and 1.38 the same)', doc='SBO-2.0-CHANGES.md',
        covers=('func_trackautochange', 'func_trackautochange.toptrack', 'func_trackautochange.bottomtrack',
                'func_trackautochange.train'))
def trackauto_no_activator(t):
    train(t)
    platform(t, 'func_trackautochange', 'auto', 0)
    t.point('trigger_relay', targetname='after', target='alive', triggerstate='2')
    t.point('info_target', targetname='alive', at=(0, -150, 0))
    t.wait(0.5)
    t.snap('a', 'auto', 'origin')
    t.mark('go')
    t.fire('auto')
    t.fire('auto', use='on')
    t.wait(1.0)
    t.fire('after')
    t.wait(0.5)
    t.snap('b', 'auto', 'origin')
    t.expect.moved('a', 'b', 'auto', delta=(0, 0, 0), tol=1)
    t.expect.fired('alive', after='go', min=1)


@S.test('func_trackchange / func_trackautochange whose Train is never found (no entity of that name): fired by a '
        'player and with no activator, the server keeps running (func_trackautochange read the missing train\'s path)',
        doc='SBO-2.0-CHANGES.md', players=1,
        covers=('func_trackautochange', 'func_trackchange.train', 'func_trackautochange.train'))
def track_no_train(t):
    for i, cls in enumerate(('func_trackautochange', 'func_trackchange')):
        n = 'nt%d' % i
        x = -100 + i * 200
        # the top path has a next path, so the auto change's not-the-train branch looks at the train's path
        t.point('path_track', targetname=n + '_top', target=n + '_next', at=(x, 60, 72))
        t.point('path_track', targetname=n + '_next', at=(x, 160, 72))
        t.point('path_track', targetname=n + '_bot', at=(x, 60, 8))
        t.brush(cls, targetname=n, size=(48, 48, 16), at=(x, 60, 64), tex='666MURMETAL', origin=True,
                toptrack=n + '_top', bottomtrack=n + '_bot', train='no_such_train', speed='200', height='64')
    t.point('trigger_relay', targetname='after', target='alive', triggerstate='2')
    t.point('info_target', targetname='alive', at=(0, -150, 0))
    t.wait(0.5)
    t.mark('go')
    for n in ('nt0', 'nt1'):
        t.fire(n)
        t.fire(n, activator='player1')
        t.fire(n, use='on')
    t.wait(1.5)
    t.fire('after')
    t.wait(0.5)
    t.expect.fired('alive', after='go', min=1)


@S.test('func_trackchange / func_trackautochange whose train is removed (killtarget) after it was found: fired with no '
        'activator and by a player, each acts as with no train (the track change moves, the auto change toggles its '
        'paths) and the server keeps running (both kept a raw pointer to the removed train)',
        doc='SBO-2.0-CHANGES.md', players=1,
        covers=('func_trackchange.train', 'func_trackautochange.train'))
def track_train_killed(t):
    train(t)
    for i, cls in enumerate(('func_trackautochange', 'func_trackchange')):
        n = 'kt%d' % i
        x = -100 + i * 200
        # the top path has a next path, so the auto change's not-the-train branch looks at the train's path
        t.point('path_track', targetname=n + '_top', target=n + '_next', at=(x, 60, 72))
        t.point('path_track', targetname=n + '_next', at=(x, 160, 72))
        t.point('path_track', targetname=n + '_bot', at=(x, 60, 8))
        t.brush(cls, targetname=n, size=(48, 48, 16), at=(x, 60, 64), tex='666MURMETAL', origin=True,
                toptrack=n + '_top', bottomtrack=n + '_bot', train='tr', speed='200', height='64')
    t.point('trigger_relay', targetname='kill_tr', killtarget='tr')
    t.point('trigger_relay', targetname='after', target='alive', triggerstate='2')
    t.point('info_target', targetname='alive', at=(0, -150, 0))
    # Find runs 2 s after spawn and takes the train
    t.wait(2.5)
    t.fire('kill_tr')
    t.wait(0.5)
    t.snap('a', 'kt1', 'origin')
    t.mark('go')
    for n in ('kt0', 'kt1'):
        t.fire(n)
        t.fire(n, activator='player1')
        t.fire(n, use='on')
    t.wait(1.5)
    t.snap('b', 'kt1', 'origin')
    t.fire('after')
    t.wait(0.5)
    t.expect.moved('a', 'b', 'kt1', delta=(0, 0, -64), tol=2)
    t.expect.fired('alive', after='go', min=1)
