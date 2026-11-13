"""Mover and trigger option tests generated from sdk/syndicate.fgd (suites fgd_gen_movers_*, fgd_gen_triggers_*).

Each group enumerates the FGD's own choice values and spawnflag bits for the classes that offer them, places the
class in a test room, drives it and checks what each value does:

- use types (`generate_use_types`): every "Fire On ... use type" choice (fireon*_triggerstate) reaches its target as
  USE_OFF / USE_ON / USE_TOGGLE when the mover starts, stops, opens and closes; every "Obey Trigger Mode" choice
  (m_iObeyTriggerMode) decides which use types start and stop the entity.
- sound choices (`generate_sounds`): every Move / Stop Sound and Sound radius of func_trackautochange (a train drives
  onto it) and func_tracktrain, every speaker preset, every Sound Radius of scripted_sentence and env_message, the
  ambient_generic play modes no other suite checks and every env_warpball AI sound type, each against the sound file,
  attenuation or AI reaction the code gives that value.
- trigger, game, path, button and train flags (`generate_flags`): every trigger_inout / trigger_jump flag with its kind
  of toucher (monster, player, func_pushable, gib) and their netname, the trigger_teleport, trigger_copyvalue,
  trigger_camera, trigger_sequence, game_score / game_team_* / game_player_hurt, path_track, func_pendulum, button and
  func_trackautochange / func_tracktrain flags, each next to a copy without the flag.

How a class is driven in a test room is the one thing the FGD cannot say: the RECIPES tables map a class to its
setup. A class with the keys but without a recipe is left out with the reason in SKIP (most are covered by a
hand-written suite, named there).

`python3 tests/entity/etest/gen_movers.py` prints the plan: every generated test and every class left out, with its
reason.
"""

import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
FGD_PATH = os.path.join(ROOT, 'sdk', 'syndicate.fgd')
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3
USE = {'0': USE_OFF, '1': USE_ON, '2': USE_TOGGLE}
FIREON = ('fireonstart', 'fireonstop', 'fireonopening', 'fireonclosing', 'fireonopened', 'fireonclosed')
DOC_USE = 'FGD use types'

_FGD = None


def _fgd():
    """(classes, effective_props) of the FGD, parsed once."""
    global _FGD
    if _FGD is None:
        sys.path.insert(0, os.path.join(ROOT, 'scripts'))
        import fgd
        classes, _ = fgd.parse_fgd(FGD_PATH)
        _FGD = (classes, fgd.effective_props)
    return _FGD


def _concrete():
    classes, props = _fgd()
    for cn, c in classes.items():
        if c['kind'] != 'baseclass':
            yield cn, props(classes, cn)


def _choices(p):
    return [str(v).strip('"') for v in p['choices']]


# ============================================================================================== G1: use types
# ---------------------------------------------------------------------------------------------- Fire On recipes
# A Fire On recipe: body(t, kv) places the class named 'm' with the Fire On keys kv (targets named after the keys)
# and drives it; it returns {key: (mark, (lo, hi), count)}: the window after a mark in which that key's target fires.

def _fireon_momentary_door(t, kv):
    t.brush('momentary_door', targetname='m', size=(64, 16, 96), at=(0, 100, 0), angles='0 0 0', lip='8', **kv)
    t.wait(0.2)
    t.mark('open')
    t.fire('m', activator='player1', use='set', value=1)
    t.wait(0.6)
    t.mark('close')
    t.fire('m', activator='player1', use='set', value=0)
    t.wait(0.6)
    # a Set use moves it in 0.1 s (speed = distance * 10)
    return {'fireonstart': ('open', (0, 0.05), 1), 'fireonopening': ('open', (0, 0.05), 1),
            'fireonstop': ('open', (0.05, 0.4), 1), 'fireonopened': ('open', (0.05, 0.4), 1),
            'fireonclosing': ('close', (0, 0.05), 1), 'fireonclosed': ('close', (0.05, 0.4), 1)}


def _fireon_func_train(t, kv):
    t.point('path_corner', targetname='p1', at=(-100, 100, 40), target='p2')
    t.point('path_corner', targetname='p2', at=(100, 100, 40))
    t.brush('func_train', targetname='m', size=(32, 32, 32), at=(-100, 100, 40), target='p1', speed='400', **kv)
    t.wait(0.3)
    t.mark('go')
    t.fire('m', activator='player1')
    t.wait(1.0)
    # 200 units at 400: it reaches the last stop 0.5 s later and stops there
    return {'fireonstart': ('go', (0, 0.1), 1), 'fireonstop': ('go', (0.4, 1.0), 1)}


def train_lane(t, p, y, bottom=False, **kv):
    """A func_trackautochange `p` at x 0 that a func_tracktrain `p`_tr drives onto, on its own lane at y.
    The train waits at x -200 on the platform's level (top, or bottom with flag 8 Start at Bottom); fired on, it
    rolls onto the platform's path, which is a dead end there (the auto change has disabled the next path), and
    the dead end (path_track Fire on dead end) uses the platform with the train as activator: the platform moves
    the train to the other level and the train drives on. Returns the name of the train."""
    z_top, z_bot = 72, 8
    z = z_bot if bottom else z_top
    t.point('path_track', targetname=p + '_a', target=p + ('_bot' if bottom else '_top'), at=(-200, y, z))
    t.point('path_track', targetname=p + '_top', target=p + '_topn', at=(0, y, z_top),
            **({} if bottom else {'netname': p}))
    t.point('path_track', targetname=p + '_topn', at=(200, y, z_top))
    t.point('path_track', targetname=p + '_bot', target=p + '_botn', at=(0, y, z_bot),
            **({'netname': p} if bottom else {}))
    t.point('path_track', targetname=p + '_botn', at=(200, y, z_bot))
    t.brush('func_tracktrain', targetname=p + '_tr', size=(32, 32, 16), at=(-200, y, z), target=p + '_a',
            wheels='48', height='0', speed='300', startspeed='0', origin=True)
    kv.setdefault('speed', '200')
    kv.setdefault('height', '64')
    if bottom:
        kv['spawnflags'] = str(int(kv.get('spawnflags', '0')) | 8)
    t.brush('func_trackautochange', targetname=p, size=(48, 48, 16), at=(0, y, 64), tex='666MURMETAL', origin=True,
            toptrack=p + '_top', bottomtrack=p + '_bot', train=p + '_tr', **kv)
    return p + '_tr'


def _fireon_func_trackautochange(t, kv):
    # lane d starts at the top (goes down: closing, closed), lane u at the bottom (goes up: opening, opened); each
    # lane's targets are its own (kv names 'x' become 'd_x' / 'u_x')
    for p, y, bottom in (('d', -60, False), ('u', 60, True)):
        lane = {k: (p + '_' + v if k in FIREON else v) for k, v in kv.items()}
        for k in FIREON:
            if k in lane:
                t.point('info_target', targetname=lane[k], at=(-220, y + 30, 0))
        train_lane(t, p, y, bottom, names=FIREON, **lane)
    t.wait(0.3)
    t.mark('go')
    t.fire('d_tr', use='on')
    t.fire('u_tr', use='on')
    t.wait(2.5)
    return {'d_fireonstart': ('go', (0.3, 1.2), 1), 'd_fireonclosing': ('go', (0.3, 1.2), 1),
            'd_fireonstop': ('go', (0.6, 2.0), 1), 'd_fireonclosed': ('go', (0.6, 2.0), 1),
            'u_fireonstart': ('go', (0.3, 1.2), 1), 'u_fireonopening': ('go', (0.3, 1.2), 1),
            'u_fireonstop': ('go', (0.6, 2.0), 1), 'u_fireonopened': ('go', (0.6, 2.0), 1)}


FIREON_RECIPES = {
    'momentary_door': _fireon_momentary_door,
    'func_train': _fireon_func_train,
    'func_trackautochange': _fireon_func_trackautochange,
}
FIREON_EXTRA_COVERS = {'func_trackautochange': ('func_trackautochange:8', 'path_track.netname')}

# ---------------------------------------------------------------------------------------------- Obey recipes
# An Obey recipe: (place(t, mode), probe(t, label), active(t, label, on)). place puts the class named 'o' with
# m_iObeyTriggerMode = mode, at rest and off; probe snapshots its state; active expects it running / open (on) or
# still / closed.


def _door_place(cls):
    def place(t, mode):
        kv = dict(m_iObeyTriggerMode=mode, wait='-1', lip='8', spawnflags='32')   # 32: Toggle (stays open)
        if cls == 'func_door_rotating':
            t.brush(cls, targetname='o', size=(64, 8, 96), at=(0, 100, 0), tex='666MURMETAL', origin=True,
                    distance='90', speed='180', **kv)
        else:
            t.brush(cls, targetname='o', size=(96, 96, 96), at=(0, 100, 0), angles='-90 0 0', speed='176',
                    **dict(kv, **({'skin': '-3'} if cls == 'func_water' else {})))
    return place


def _door_probe(cls):
    field = 'angles' if cls == 'func_door_rotating' else 'origin'
    return lambda t, label: t.snap(label, 'o', field)


def _door_active(cls):
    # rotating: 90 degrees open; sliding: up 96 - 8 (lip) = 88 (0.5 s each way at these speeds)
    def active(t, label, on):
        if cls == 'func_door_rotating':
            t.expect.value(label, 'o', 'angles.y', 90 if on else 0, tol=1)
        else:
            t.expect.value(label, 'o', 'origin.z', 88 if on else 0, tol=1)
    return active


def _train_place(t, mode):
    t.point('path_corner', targetname='op1', at=(-220, 100, 40), target='op2')
    t.point('path_corner', targetname='op2', at=(220, 100, 40), target='op1')
    t.brush('func_train', targetname='o', size=(32, 32, 32), at=(-220, 100, 40), target='op1', speed='50',
            m_iObeyTriggerMode=mode)


def _rotating_place(t, mode):
    t.brush('func_rotating', targetname='o', size=(64, 64, 16), at=(0, 100, 0), origin=True, speed='100',
            m_iObeyTriggerMode=mode)


def _monsterjump_place(t, mode):
    # named: it starts off (Half-Life)
    t.trigger('trigger_monsterjump', size=(64, 64, 72), at=(0, 100, 0), targetname='o', m_iObeyTriggerMode=mode)


OBEY_EXTRA_COVERS = {'func_door': ('func_door:32',), 'func_water': ('func_water:32',),
                     'func_door_rotating': ('func_door_rotating:32',), 'monster_apache': ('monster_apache:4',)}
OBEY_RECIPES = {
    'func_door': (_door_place('func_door'), _door_probe('func_door'), _door_active('func_door')),
    'func_water': (_door_place('func_water'), _door_probe('func_water'), _door_active('func_water')),
    'func_door_rotating': (_door_place('func_door_rotating'), _door_probe('func_door_rotating'),
                           _door_active('func_door_rotating')),
    'func_train': (_train_place, lambda t, label: t.snap(label, 'o', 'velocity'),
                   lambda t, label, on: t.expect.value(label, 'o', 'velocity.x', 50 if on else 0, tol=1)),
    'func_rotating': (_rotating_place, lambda t, label: t.snap(label, 'o', 'avelocity'),
                      lambda t, label, on: t.expect.value(label, 'o', 'avelocity.y', 100 if on else 0, tol=1)),
    # solid: SOLID_TRIGGER (1) on, SOLID_NOT (0) off; suite mapper shows an on trigger throwing a walking monster
    'trigger_monsterjump': (_monsterjump_place, lambda t, label: t.snap(label, 'o', 'solid'),
                            lambda t, label, on: t.expect.value(label, 'o', 'solid', 1 if on else 0)),
}

# Obey Trigger Mode sequences (mode value -> title, steps); a step: (use type, seconds to wait, expected on / off)
OBEY_SEQUENCES = {
    '0': ('No: Off starts / opens it, a second Off stops / closes it (every use type toggles)',
          (('off', 0.8, True), ('off', 0.8, False))),
    '1': ('Yes: Off when off does nothing, On starts / opens it, On again keeps it so, Off stops / closes it',
          (('off', 0.8, False), ('on', 0.8, True), ('on', 0.8, True), ('off', 0.8, False))),
    '2': ('Yes, even when moving: Off when closed does nothing; On opens; Off while it is still opening sends it '
          'back closed; On while it is closing opens it again',
          (('off', 0.8, False), ('on', 0.2, None), ('off', 0.8, False), ('on', 0.8, True), ('off', 0.2, None),
           ('on', 0.8, True))),
}


def _apache_obey(t, mode):
    """monster_apache flag 4 (waits for a trigger): mode 0: an Off starts it (rotor sound), a later Off does not stop
    it; mode 1: Off does not start it, On does, Off stops it (rotor sound stopped)."""
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_apache', targetname='o', at=(0, 0, 500), angles='0 90 0', spawnflags='4',
            m_iObeyTriggerMode=mode)
    t.wait(0.5)
    if mode == '0':
        t.mark('off1')
        t.fire('o', use='off')
        t.wait(1.5)
        t.mark('off2')
        t.fire('o', use='off')
        t.wait(1.0)
        t.expect.sound('ap_rotor2', ent='o', after='off1', within=(0, 1.5))
        t.expect.sound('ap_rotor2', ent='o', flags=32, count=0, after='off2')
    else:
        t.mark('off1')
        t.fire('o', use='off')
        t.wait(1.0)
        t.mark('on')
        t.fire('o', use='on')
        t.wait(1.5)
        t.mark('off2')
        t.fire('o', use='off')
        t.wait(1.0)
        t.expect.sound('ap_rotor2', ent='o', count=0, after='off1', within=(0, 1.0))
        t.expect.sound('ap_rotor2', ent='o', after='on', within=(0, 1.5))
        t.expect.sound('ap_rotor2', ent='o', flags=32, after='off2')


OBEY_BODIES = {'monster_apache': (_apache_obey, (512, 512, 1024), {
    '0': 'No (flag 4, waiting): an Off starts it (rotor sound), a later Off does not stop it',
    '1': 'Yes (flag 4, waiting): Off does not start it, On does, Off stops it in the air (rotor sound stopped)'})}

USE_SKIP = {
    'func_door': 'Fire On use types: suite fgd_movers',
    'func_door_rotating': 'Fire On use types: suite fgd_movers',
    'func_water': 'Fire On use types: suite fgd_movers',
    'func_plat': 'Fire On use types: suite fgd_movers',
    'func_platrot': 'Fire On use types: suite fgd_movers',
    'func_trackchange': 'Fire On use types: suite fgd_trackchange',
    'env_spritetrain': 'Obey Trigger Mode: suites fgd_misc and guide',
    'env_modeltrain': 'Obey Trigger Mode: suite fgd_misc',
    'trigger_hurt': 'Obey Trigger Mode: suites mapper and fgd_stock',
    'trigger_push': 'Obey Trigger Mode: suite push_keys',
}


def use_plan():
    """(cases, skipped): cases [(kind, class, values)] kind 'fireon' (values: {key: [choices]}) or 'obey'
    (values: [choices]); skipped [(class, what, reason)]."""
    cases, skipped = [], []
    for cn, props in _concrete():
        fire = {k: _choices(props[k]) for k in props
                if k.startswith('fireon') and k.endswith('_triggerstate') and props[k]['choices']}
        if fire:
            if cn in FIREON_RECIPES:
                cases.append(('fireon', cn, fire))
            else:
                skipped.append((cn, 'fireon', USE_SKIP.get(cn, 'no Fire On recipe in the harness')))
        p = props.get('m_iobeytriggermode')
        if p and p['choices']:
            if cn in OBEY_RECIPES or cn in OBEY_BODIES:
                cases.append(('obey', cn, _choices(p)))
            else:
                skipped.append((cn, 'obey', USE_SKIP.get(cn, 'no Obey Trigger Mode recipe in the harness')))
    return cases, skipped


def _make_fireon(suite, cn, values):
    keys = [k for k in (x + '_triggerstate' for x in FIREON) if k in values]
    n = max(len(v) for v in values.values())
    for shift in range(n):
        st = {k: values[k][(i + shift) % len(values[k])] for i, k in enumerate(keys)}
        covers = (cn,) + tuple('%s.%s' % (cn, k[:-len('_triggerstate')]) for k in keys) \
            + tuple('%s.%s=%s' % (cn, k, v) for k, v in st.items()) + FIREON_EXTRA_COVERS.get(cn, ())
        title = '%s Fire On use types %s: each target gets its use type when the mover %s' % (
            cn, ', '.join('%s %s' % (k[6:-13], v) for k, v in st.items()),
            'starts and stops' if len(keys) == 2 else 'starts, stops, opens and closes')

        def body(t, _cn=cn, _st=st):
            kv = {}
            for k, v in _st.items():
                tk = k[:-len('_triggerstate')]
                kv[tk] = tk
                kv[k] = v
                if _cn != 'func_trackautochange':
                    t.point('info_target', targetname=tk, at=(-200, -200, 0))
            windows = FIREON_RECIPES[_cn](t, kv)
            for name, (mark, within, count) in windows.items():
                tk = name.split('_', 1)[1] if name[:2] in ('d_', 'u_') else name
                t.expect.fired(name, use=USE[_st[tk + '_triggerstate']], count=count, after=mark, within=within)
        body.__name__ = '%s_fireon_%d' % (cn, shift)
        suite.test(title, doc=DOC_USE, players=1, covers=covers)(body)


def _make_obey(suite, cn, values):
    for v in values:
        covers = (cn, '%s.m_iobeytriggermode' % cn, '%s.m_iobeytriggermode=%s' % (cn, v)) \
            + OBEY_EXTRA_COVERS.get(cn, ())
        if cn in OBEY_BODIES:
            fn, size, titles = OBEY_BODIES[cn]
            title = '%s Obey Trigger Mode %s %s' % (cn, v, titles[v])

            def body(t, _fn=fn, _v=v):
                _fn(t, _v)
        else:
            if v not in OBEY_SEQUENCES:
                raise ValueError('%s Obey Trigger Mode %s: no sequence' % (cn, v))
            size = (512, 512, 256)
            seq_title, seq = OBEY_SEQUENCES[v]
            title = '%s Obey Trigger Mode %s %s' % (cn, v, seq_title)

            def body(t, _cn=cn, _v=v, _seq=seq):
                place, probe, active = OBEY_RECIPES[_cn]
                place(t, _v)
                t.wait(0.3)
                for i, (use, wait, on) in enumerate(_seq):
                    t.fire('o', use=use)
                    t.wait(wait)
                    if on is not None:
                        probe(t, 's%d' % i)
                        active(t, 's%d' % i, on)
        body.__name__ = '%s_obey_%s' % (cn, v)
        suite.test(title, doc=DOC_USE, players=1, size=size, covers=covers)(body)


def generate_use_types(suite):
    """G1: every Fire On use type and Obey Trigger Mode choice of the classes with a recipe."""
    cases, _ = use_plan()
    for kind, cn, values in cases:
        if kind == 'fireon':
            _make_fireon(suite, cn, values)
        else:
            _make_obey(suite, cn, values)


# ============================================================================================== G2: sound choices
# What the code plays for each choice value. A value the FGD offers that a table lacks fails the generation
# (_values), so a new choice lands with its expected sound.
RADII = {'-2': 2.0, '-1': 1.25, '0': 0.8, '1': 0.5, '2': 0.25, '3': 0.0}           # soundradius.h
PLAT_MOVE = {'0': None, '1': 'plats/bigmove1.wav', '2': 'plats/bigmove2.wav', '3': 'plats/elevmove1.wav',
             '4': 'plats/elevmove2.wav', '5': 'plats/elevmove3.wav', '6': 'plats/freightmove1.wav',
             '7': 'plats/freightmove2.wav', '8': 'plats/heavymove1.wav', '9': 'plats/rackmove1.wav',
             '10': 'plats/railmove1.wav', '11': 'plats/squeekmove1.wav', '12': 'plats/talkmove1.wav',
             '13': 'plats/talkmove2.wav'}                                            # plats.cpp CBasePlatTrain
PLAT_STOP = {'0': None, '1': 'plats/bigstop1.wav', '2': 'plats/bigstop2.wav', '3': 'plats/freightstop1.wav',
             '4': 'plats/heavystop2.wav', '5': 'plats/rackstop1.wav', '6': 'plats/railstop1.wav',
             '7': 'plats/squeekstop1.wav', '8': 'plats/talkstop1.wav'}
TRAIN_SOUND = {'0': None, '1': 'plats/ttrain1.wav', '2': 'plats/ttrain2.wav', '3': 'plats/ttrain3.wav',
               '4': 'plats/ttrain4.wav', '5': 'plats/ttrain6.wav', '6': 'plats/ttrain7.wav'}  # CFuncTrackTrain
SPEAKER_GROUP = {'0': None, '1': 'C1A0_', '2': 'C1A1_', '3': 'C1A2_', '4': 'C1A3_', '5': 'C1A4_', '6': 'C2A1_',
                 '7': 'C2A2_', '9': 'C2A4_', '11': 'C3A1_', '12': 'C3A2_'}           # sound.cpp CSpeaker
SENTENCE_ATTN = {'0': 2.0, '1': 1.25, '2': 0.8, '3': 0.0, '4': 0.5, '5': 0.25}     # scripted.cpp CScriptedSentence
MESSAGE_ATTN = {'0': 2.0, '1': 1.25, '2': 0.8, '3': 0.0}                            # effects.cpp CMessage
DOC_SOUND = 'FGD sound choices'


def _values(cn, key, table):
    """The FGD's choice values of cn.key, each with its entry in table."""
    classes, props = _fgd()
    vals = _choices(props(classes, cn)[key])
    missing = [v for v in vals if v not in table]
    if missing:
        raise KeyError('%s.%s: no expected sound for FGD value(s) %s' % (cn, key, ', '.join(missing)))
    return vals


def _lanes(n):
    """y positions of n train lanes, 60 apart, centered"""
    return [int((i - (n - 1) / 2.0) * 60) for i in range(n)]


def _trackauto_sounds(suite, part, parts=2):
    moves = _values('func_trackautochange', 'movesnd', PLAT_MOVE)
    stops = _values('func_trackautochange', 'stopsnd', PLAT_STOP)
    # every move value once; the stop values cycle over the same lanes, so each is used at least once
    rows = [(m, stops[k % len(stops)]) for k, m in enumerate(moves)][part::parts]
    covers = ('func_trackautochange', 'func_trackautochange.movesnd', 'func_trackautochange.stopsnd') \
        + tuple('func_trackautochange.movesnd=%s' % m for m, _ in rows) \
        + tuple(sorted(set('func_trackautochange.stopsnd=%s' % st for _, st in rows)))

    @suite.test('func_trackautochange Move Sound %s with Stop Sound %s: a train drives onto each platform, which '
                'plays them as it carries the train down and stops (0: none)' % (
                    ', '.join(m for m, _ in rows), ', '.join(st for _, st in rows)),
                doc=DOC_SOUND, covers=covers)
    def body(t):
        for i, ((m, st), y) in enumerate(zip(rows, _lanes(len(rows)))):
            train_lane(t, 'l%d' % i, y, movesnd=m, stopsnd=st)
        t.wait(0.3)
        t.mark('go')
        for i in range(len(rows)):
            t.fire('l%d_tr' % i, use='on')
        t.wait(2.5)
        for i, (m, st) in enumerate(rows):
            if PLAT_MOVE[m]:
                t.expect.sound(PLAT_MOVE[m], ent='l%d' % i, min=1, after='go')
            else:
                t.expect.sound('move', ent='l%d' % i, count=0, after='go')
            if PLAT_STOP[st]:
                t.expect.sound(PLAT_STOP[st], ent='l%d' % i, count=1, after='go')
            else:
                t.expect.sound('stop', ent='l%d' % i, count=0, after='go')
    body.__name__ = 'trackauto_sounds_%d' % part


def _trackauto_radius(suite):
    vals = _values('func_trackautochange', 'soundradius', RADII)

    @suite.test('func_trackautochange Sound radius %s: each platform a train drives onto plays its move and stop '
                'sounds at that attenuation (%s)' % (', '.join(vals), ', '.join('%g' % RADII[v] for v in vals)),
                doc=DOC_SOUND, covers=tuple('func_trackautochange.soundradius=%s' % v for v in vals))
    def body(t):
        for i, (v, y) in enumerate(zip(vals, _lanes(len(vals)))):
            train_lane(t, 'l%d' % i, y, movesnd='1', stopsnd='1', soundradius=v)
        t.wait(0.3)
        t.mark('go')
        for i in range(len(vals)):
            t.fire('l%d_tr' % i, use='on')
        t.wait(2.5)
        for i, v in enumerate(vals):
            t.expect.sound(PLAT_MOVE['1'], ent='l%d' % i, attn=RADII[v], min=1, after='go')
            t.expect.sound(PLAT_STOP['1'], ent='l%d' % i, attn=RADII[v], count=1, after='go')
    body.__name__ = 'trackauto_radius'


def _trackauto_custom(suite):
    @suite.test('func_trackautochange Custom move / stop sound: a train drives onto it, it plays those files instead '
                'of its Move / Stop Sound', doc=DOC_SOUND,
                covers=('func_trackautochange.custommovesnd', 'func_trackautochange.customstopsnd'))
    def trackauto_custom(t):
        train_lane(t, 'l', 0, movesnd='1', stopsnd='1', custommovesnd='doors/doormove3.wav',
                   customstopsnd='doors/doorstop5.wav')
        t.wait(0.3)
        t.mark('go')
        t.fire('l_tr', use='on')
        t.wait(2.5)
        t.expect.sound('doors/doormove3.wav', ent='l', min=1, after='go')
        t.expect.sound('doors/doorstop5.wav', ent='l', count=1, after='go')
        t.expect.sound('plats/', ent='l', count=0, after='go')


def _track_train(t, name, y, **kv):
    t.point('path_track', targetname=name + '_a', target=name + '_b', at=(-200, y, 8))
    t.point('path_track', targetname=name + '_b', at=(200, y, 8))
    t.brush('func_tracktrain', targetname=name, size=(32, 32, 16), at=(-200, y, 8), target=name + '_a',
            wheels='16', height='0', speed='100', startspeed='0', origin=True, **kv)


def _tracktrain_sounds(suite):
    vals = _values('func_tracktrain', 'sounds', TRAIN_SOUND)

    @suite.test('func_tracktrain Sound %s: each train plays its sound file when it starts (0: none, no start or brake '
                'sound either)' % ', '.join(vals), doc=DOC_SOUND,
                covers=('func_tracktrain', 'func_tracktrain.sounds')
                + tuple('func_tracktrain.sounds=%s' % v for v in vals))
    def tracktrain_sounds(t):
        for i, (v, y) in enumerate(zip(vals, _lanes(len(vals)))):
            _track_train(t, 'tr%d' % i, y, sounds=v)
        t.wait(0.3)
        t.mark('go')
        for i in range(len(vals)):
            t.fire('tr%d' % i, use='on')
        t.wait(1.0)
        for i, v in enumerate(vals):
            if TRAIN_SOUND[v]:
                t.expect.sound(TRAIN_SOUND[v], ent='tr%d' % i, min=1, after='go')
                t.expect.sound('plats/ttrain_start1.wav', ent='tr%d' % i, min=1, after='go')
            else:
                t.expect.sound('plats/', ent='tr%d' % i, count=0, after='go')


def _tracktrain_radius(suite):
    vals = _values('func_tracktrain', 'soundradius', RADII)

    @suite.test('func_tracktrain Sound radius %s: each train plays its start, move and brake sounds at that '
                'attenuation (%s)' % (', '.join(vals), ', '.join('%g' % RADII[v] for v in vals)), doc=DOC_SOUND,
                covers=tuple('func_tracktrain.soundradius=%s' % v for v in vals))
    def tracktrain_radius(t):
        for i, (v, y) in enumerate(zip(vals, _lanes(len(vals)))):
            _track_train(t, 'tr%d' % i, y, sounds='1', soundradius=v)
        t.wait(0.3)
        t.mark('go')
        for i in range(len(vals)):
            t.fire('tr%d' % i, use='on')
        t.wait(0.6)
        for i in range(len(vals)):
            t.fire('tr%d' % i, use='off')
        t.wait(0.3)
        for i, v in enumerate(vals):
            for snd in ('plats/ttrain1.wav', 'plats/ttrain_start1.wav', 'plats/ttrain_brake1.wav'):
                t.expect.sound(snd, ent='tr%d' % i, attn=RADII[v], min=1, after='go')


def _speaker(suite):
    vals = _values('speaker', 'preset', SPEAKER_GROUP)
    presets = [v for v in vals if SPEAKER_GROUP[v]]
    gap = 10.0    # a speaker that talks holds every other speaker (and talking monster) for 5 s (g_talkWaitTime)

    @suite.test('speaker Announcement Presets %s, and a Sentence Group Name with preset 0 (None): Start Silent (1) '
                'keeps each one quiet until it is fired; fired, it says a sentence of its group (one every %g s: a '
                'speaker holds the others for 5 s)' % (', '.join(presets), gap), doc=DOC_SOUND,
                covers=('speaker', 'speaker.message', 'speaker:1') + tuple('speaker.preset=%s' % v for v in vals))
    def speaker_presets(t):
        rows = [(v, SPEAKER_GROUP[v], {'preset': v}) for v in presets] + [('0', 'C1A2_', {'message': 'C1A2_'})]
        for i, (_v, _g, kv) in enumerate(rows):
            t.point('speaker', targetname='sp%d' % i, at=(-220 + 36 * i, 100, 64), spawnflags='1', health='10', **kv)
        t.mark('silent')
        t.wait(gap)
        for i in range(len(rows)):
            t.mark('f%d' % i)
            t.fire('sp%d' % i, use='on')
            t.wait(1.5)
            t.fire('sp%d' % i, use='off')      # no later announcements (one every 15 s to 2.25 min)
            t.wait(gap - 1.5)
        t.expect.sound('!', ent='sp0', count=0, after='silent', within=(0, gap - 0.5))
        for i, (_v, group, _kv) in enumerate(rows):
            # the trace names the sentence ('C1A0_8') next to its sample ('!104')
            t.expect.sound(group, ent='sp%d' % i, count=1, attn=0.3, after='f%d' % i, within=(0, 1.0))
    return speaker_presets


def _sentence(suite):
    vals = _values('scripted_sentence', 'attenuation', SENTENCE_ATTN)

    @suite.test('scripted_sentence Sound Radius %s: each scientist says its sentence at that attenuation (%s)' % (
                ', '.join(vals), ', '.join('%g' % SENTENCE_ATTN[v] for v in vals)), doc=DOC_SOUND, players=1,
                covers=('scripted_sentence', 'scripted_sentence.attenuation')
                + tuple('scripted_sentence.attenuation=%s' % v for v in vals))
    def sentence_attenuation(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (0, -200, 0))
        for i, v in enumerate(vals):
            x = -200 + 80 * i
            t.point('monster_scientist', targetname='sc%d' % i, at=(x, 100, 0), angles='0 270 0', spawnflags='2')
            t.point('scripted_sentence', targetname='ss%d' % i, at=(x, 100, 0), sentence='!SC_OK0',
                    entity='sc%d' % i, duration='1', radius='64', spawnflags='4', attenuation=v,
                    names=('entity',))
        t.wait(1.0)
        for i in range(len(vals)):
            t.mark('s%d' % i)
            t.fire('ss%d' % i)
            t.wait(1.5)
        for i, v in enumerate(vals):
            t.expect.sound('!', ent='sc%d' % i, attn=SENTENCE_ATTN[v], min=1, after='s%d' % i, within=(0, 1.0))


def _message(suite):
    vals = _values('env_message', 'messageattenuation', MESSAGE_ATTN)

    @suite.test('env_message Sound Radius %s: its Sound Effect plays at that attenuation (%s); Play Once (1): '
                'removed after its first use' % (', '.join(vals), ', '.join('%g' % MESSAGE_ATTN[v] for v in vals)),
                doc=DOC_SOUND, players=1,
                covers=('env_message', 'env_message.messagesound', 'env_message:1')
                + tuple('env_message.messageattenuation=%s' % v for v in vals))
    def message_attenuation(t):
        for i, v in enumerate(vals):
            t.point('env_message', targetname='m%d' % i, at=(-150 + 60 * i, 100, 32), message='GEN_TEST',
                    messagesound='buttons/blip1.wav', messageattenuation=v)
        t.point('env_message', targetname='once', at=(150, 100, 32), message='GEN_TEST',
                messagesound='buttons/blip2.wav', spawnflags='1')
        t.wait(0.2)
        t.mark('go')
        for i in range(len(vals)):
            t.fire('m%d' % i, activator='player1')
        t.fire('once', activator='player1')
        t.wait(0.3)
        t.fire('once', activator='player1')
        t.wait(0.3)
        for i, v in enumerate(vals):
            t.expect.sound('buttons/blip1.wav', ent='m%d' % i, attn=MESSAGE_ATTN[v], count=1, after='go')
        t.expect.sound('buttons/blip2.wav', count=1, after='go')
        t.expect.removed('once', count=1, after='go')


def _ambient_playmode(suite):
    # anzym/cricket1.wav: 1.88 s with cue points (a looping WAV)
    @suite.test('ambient_generic Play Mode on a looping WAV (cue points): 0 Default plays it and lets it loop (no stop '
                'from the server); 1 Play once stops it when it ends; 5 Play once, linear falloff: no sound to the '
                'whole server, each client gets its own volume (a player 128 units away with Linear Min 0 / Max 1, '
                'in steps of 256 units: half volume, no engine falloff), and it stops when it ends', doc=DOC_SOUND,
                players=1,
                covers=('ambient_generic', 'ambient_generic.playmode=0', 'ambient_generic.playmode=1',
                        'ambient_generic.playmode=5', 'ambient_generic.linearmin', 'ambient_generic.linearmax'))
    def ambient_playmode(t):
        t.point('ambient_generic', targetname='a0', at=(-150, 150, 64), message='anzym/cricket1.wav', health='10',
                playmode='0', spawnflags='20')                          # 16 Start Silent | 4 Medium Radius
        t.point('ambient_generic', targetname='a1', at=(0, 150, 64), message='anzym/cricket1.wav', health='10',
                playmode='1', spawnflags='20')
        t.point('ambient_generic', targetname='a5', at=(150, -100, 64), message='anzym/cricket1.wav', health='10',
                playmode='5', linearmin='0', linearmax='1', spawnflags='20')   # radii in steps of 256 units
        t.teleport('player1', (22, -100, 0), yaw=0)                     # eyes 64 up: 128 units from a5
        t.wait(1.5)
        t.mark('go')
        for n in ('a0', 'a1', 'a5'):
            t.fire(n)
        t.wait(3.5)
        t.expect.sound('anzym/cricket1.wav', ent='a0', count=1, after='go')
        t.expect.sound('anzym/cricket1.wav', ent='a0', flags=32, count=0, after='go')
        t.expect.sound('anzym/cricket1.wav', ent='a1', vol=(0.9, 1.0), count=1, after='go', within=(0, 0.3))
        # the end of the sound is timed on the engine's clock (Time()), which runs ahead of game time on a busy
        # machine: a wide window around its 1.88 s
        t.expect.sound('anzym/cricket1.wav', ent='a1', flags=32, count=1, after='go', within=(0.5, 3.3))
        # linear: no sound to everyone (only the stop when the once-played sound ends)
        t.expect.sound('anzym/cricket1.wav', ent='a5', vol=(0.01, 1.0), count=0, after='go')
        t.expect.sound('anzym/cricket1.wav', ent='a5', flags=32, count=1, after='go', within=(0.5, 3.3))
        t.expect.msg('svc:29', to='player1', after='go', min=1,
                     fields={'vol': {'range': (115, 140)}, 'attn': 0})
    return ambient_playmode


def _warpball_aisound(suite):
    @suite.test('env_warpball AI Sound type 1 Combat: a Fed behind a wall hears it (its Hear Combat condition fires); '
                '0 No sound: not heard', doc=DOC_SOUND, players=1,
                covers=('env_warpball.aisound=0', 'env_warpball.aisound=1', 'env_warpball.aisound'))
    def warpball_combat(t):
        t.brush('func_wall', size=(512, 16, 256), at=(0, 0, 0), tex='666CEMENT')
        for n, x, v in (('w0', -150, '0'), ('w1', 150, '1')):
            t.point('monster_human_grunt', at=(x, 150, 0), angles='0 270 0', TriggerTarget=n + '_heard',
                    TriggerCondition='9', names=('TriggerTarget',))
            t.point('info_target', targetname=n + '_heard', at=(x, -200, 0))
            t.point('env_warpball', targetname=n, at=(x, 60, 32), spawnflags='4', beamcount='2', radius='192',
                    aisound=v)
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -150, 0), yaw=270)
        t.wait(1.0)
        t.mark('go')
        t.fire('w0')
        t.fire('w1')
        t.wait(2.0)
        t.flags('player1', '-FL_GODMODE')
        t.expect.fired('w1_heard', min=1, after='go')
        t.expect.not_fired('w0_heard', after='go')

    @suite.test('env_warpball AI Sound type 32 Danger: an idle Fed next to it runs away from it (a node grid gives him '
                'cover)', doc=DOC_SOUND, players=1, covers=('env_warpball.aisound=32',))
    def warpball_danger(t):
        for x in (-200, -100, 0, 100, 200):
            for y in (-200, -100, 0, 100, 200):
                t.point('info_node', at=(x, y, 16))
        t.brush('func_wall', size=(32, 160, 120), at=(150, 0, 0))
        t.brush('func_wall', size=(32, 160, 120), at=(-150, 0, 0))
        t.point('monster_human_grunt', targetname='sci', at=(30, 0, 0), angles='0 0 0')
        t.point('info_target', targetname='mark', at=(0, 0, 16))
        t.point('env_warpball', targetname='w', at=(0, 0, 16), spawnflags='4', beamcount='2', radius='160',
                aisound='32', aisound_duration='3')
        # notarget: the Fed idles (a scientist greeting a player, Idle Hello, does not listen for danger)
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-220, -220, 0))
        t.wait(2.0)
        t.snap('before', 'sci', 'origin')
        t.snap('before', 'mark', 'origin')
        t.fire('w')
        t.wait(3.0)
        t.snap('after', 'sci', 'origin')
        t.snap('after', 'mark', 'origin')
        t.expect.distance('before', 'sci', 'mark', 0, 60)
        t.expect.distance('after', 'sci', 'mark', 100, 600)


def generate_sounds(suite, part):
    """G2: the sound choice values. part 0: movers (func_trackautochange, func_tracktrain); part 1: scripted_sentence,
    env_message, ambient_generic, env_warpball; part 2: speaker (a suite of its own: a talking speaker or monster holds
    every speaker for 5 s, CTalkMonster::g_talkWaitTime, so no monsters may share its map)."""
    if part == 0:
        _trackauto_sounds(suite, 0)
        _trackauto_sounds(suite, 1)
        _trackauto_radius(suite)
        _trackauto_custom(suite)
        _tracktrain_sounds(suite)
        _tracktrain_radius(suite)
    elif part == 1:
        _sentence(suite)
        _message(suite)
        _ambient_playmode(suite)
        _warpball_aisound(suite)
    else:
        _speaker(suite)


# ============================================================================================== G7: trigger, game, path
# and button flags. FLAG_TESTS maps (class, bit) or (class, 'key[=value]') to the test that checks it; flags_plan()
# lists every spawnflag bit of the FLAG_CLASSES the FGD offers and says which have a test, which are exempted
# (tests/fgd_exemptions.json) and which are left.
DOC_FLAGS = 'FGD flags'
FL_ONGROUND = 512
CANTOUCH = {1: 'a monster', 2: 'a player', 4: 'a func_pushable', 8: 'a gib'}   # Featureful_CanTouch (triggers.cpp)


def _touch_trigger(t, cls, name, x, out, **kv):
    """trigger_inout fires `out` (target) when something enters; trigger_jump fires it (trigger_on_jump) when it
    launches something at its landing point"""
    if cls == 'trigger_inout':
        kv['target'] = out
    else:
        kv.update(target=name + '_land', trigger_on_jump=out)
        t.point('info_target', targetname=name + '_land', at=(x, -200, 0))
    t.trigger(cls, size=(64, 64, 72), at=(x, 0, 0), targetname=name, **kv)
    t.point('info_target', targetname=out, at=(x, 220, 0))


def _drop(t, ent, x):
    """moves `ent` (with an origin at its center) above the trigger at x and lets it fall in"""
    t.teleport(ent, (x, 0, 48))
    t.flags(ent, '-%d' % FL_ONGROUND)


def _cantouch_case(suite, cls, bit):
    what = CANTOUCH[bit]
    inverse = bit == 2

    @suite.test('%s flag %d: %s %s it off (it fires its %s); the same trigger without the flag %s' % (
                cls, bit, what, 'does not set' if inverse else 'sets',
                'target' if cls == 'trigger_inout' else 'trigger_on_jump',
                'does' if inverse else 'does not'), doc=DOC_FLAGS, players=1, covers=('%s:%d' % (cls, bit),))
    def body(t):
        t.flags('player1', '+FL_NOTARGET')
        _touch_trigger(t, cls, 'w', -120, 'w_out', spawnflags=str(bit))
        _touch_trigger(t, cls, 'c', 120, 'c_out')
        if bit == 1:
            for n, x in (('m_w', -120), ('m_c', 120)):
                t.point('monster_scientist', targetname=n, at=(x, -150, 0), angles='0 90 0', spawnflags='16')
        elif bit == 4:
            for n, x in (('p_w', -120), ('p_c', 120)):
                t.brush('func_pushable', targetname=n, size=(32, 32, 32), at=(x, -150, 0), origin=True)
        elif bit == 8:
            for n, x in (('g_w', -120), ('g_c', 120)):
                t.point('gibshooter', targetname=n, at=(x, 0, 150), m_iGibs='1', m_flGibLife='3',
                        m_iszVelocity='0 0 -1', m_flVelocity='100', m_flVariance='0', names=('m_iszVelocity',))
        t.teleport('player1', (0, -220, 0))
        t.wait(0.5)
        t.mark('go')
        for side, x in (('w', -120), ('c', 120)):
            if bit == 1:
                _drop(t, 'm_' + side, x)
            elif bit == 2:
                t.teleport('player1', (x, 0, 0))
                t.wait(0.3)
                t.teleport('player1', (0, -220, 0))
            elif bit == 4:
                _drop(t, 'p_' + side, x)
            else:
                t.fire('g_' + side)
            t.wait(1.0)
        t.flags('player1', '-FL_NOTARGET')
        if inverse:
            t.expect.not_fired('w_out', after='go')
            t.expect.fired('c_out', activator='player1', min=1, after='go')
        else:
            t.expect.fired('w_out', min=1, after='go')
            t.expect.not_fired('c_out', after='go')
    body.__name__ = '%s_flag_%d' % (cls, bit)


def _cantouch_netname(suite, cls):
    @suite.test('%s Triggered only by entity (netname) "player", with flag 2 No Clients: a player sets it off (the '
                'name or class name decides, not the flags); netname "nobody": a player does not' % cls,
                doc=DOC_FLAGS, players=1, covers=('%s.netname' % cls,))
    def body(t):
        _touch_trigger(t, cls, 'w', -120, 'w_out', spawnflags='2', netname='player')
        _touch_trigger(t, cls, 'c', 120, 'c_out', netname='nobody')
        t.wait(0.3)
        t.mark('go')
        for x in (-120, 120):
            t.teleport('player1', (x, 0, 0))
            t.wait(0.3)
            t.teleport('player1', (0, -220, 0))
            t.wait(1.0)
        t.expect.fired('w_out', activator='player1', min=1, after='go')
        t.expect.not_fired('c_out', after='go')
    body.__name__ = '%s_netname' % cls


def _teleport_cases(suite):
    def tele(t, name, x, size=(96, 96, 72), **kv):
        t.trigger('trigger_teleport', size=size, at=(x, 0, 0), targetname=name, target=name + '_d', origin=True,
                  **kv)

    @suite.test('trigger_teleport flag 128 Relative: a player 30 units east of the trigger\'s center arrives 30 units '
                'east of the destination; without it (Start Inactive 0: on from the start) he arrives on it',
                doc=DOC_FLAGS, players=1,
                covers=('trigger_teleport:128', 'trigger_teleport.teleport_start_inactive=0'))
    def teleport_relative(t):
        tele(t, 'w', -120, spawnflags='128')
        tele(t, 'c', 120, teleport_start_inactive='0')
        t.point('info_teleport_destination', targetname='w_d', at=(-120, 180, 64))
        t.point('info_teleport_destination', targetname='c_d', at=(120, 180, 64))
        t.wait(0.3)
        for side, x in (('w', -120), ('c', 120)):
            t.teleport('player1', (x + 30, 0, 0))
            t.wait(0.3)
            t.snap(side, 'player1', 'origin')
            t.snap(side, side + '_d', 'origin')
            t.teleport('player1', (0, -220, 0))
            t.wait(0.2)
        t.expect.distance('w', 'player1', 'w_d', 25, 35)
        t.expect.distance('c', 'player1', 'c_d', 0, 3)

    @suite.test('trigger_teleport flag 256 Keep angles: the player keeps his facing (yaw 0); without it he takes the '
                'destination\'s (yaw 90)', doc=DOC_FLAGS, players=1, covers=('trigger_teleport:256',))
    def teleport_keep_angles(t):
        tele(t, 'w', -120, spawnflags='256')
        tele(t, 'c', 120)
        t.point('info_teleport_destination', targetname='w_d', at=(-120, 180, 0), angles='0 90 0')
        t.point('info_teleport_destination', targetname='c_d', at=(120, 180, 0), angles='0 90 0')
        t.wait(0.3)
        for side, x in (('w', -120), ('c', 120)):
            t.teleport('player1', (x, 0, 0), yaw=0)
            t.wait(0.3)
            t.snap(side, 'player1', 'origin', 'angles')
            t.snap(side, side + '_d', 'origin')
            t.teleport('player1', (0, -220, 0), yaw=0)
            t.wait(0.2)
        t.expect.distance('w', 'player1', 'w_d', 0, 3)
        t.expect.distance('c', 'player1', 'c_d', 0, 3)
        t.expect.value('w', 'player1', 'angles.y', 0, tol=2)
        t.expect.value('c', 'player1', 'angles.y', 90, tol=2)

    @suite.test('trigger_teleport flag 512 Keep velocity: a player falling into it at 300 east arrives in the air '
                'still moving east; without it he arrives still', doc=DOC_FLAGS, players=1,
                covers=('trigger_teleport:512',))
    def teleport_keep_velocity(t):
        tele(t, 'w', -120, spawnflags='512')
        tele(t, 'c', 120)
        t.point('info_teleport_destination', targetname='w_d', at=(-180, 160, 180))
        t.point('info_teleport_destination', targetname='c_d', at=(60, 160, 180))
        t.wait(0.3)
        for side, x in (('w', -120), ('c', 120)):
            t.teleport('player1', (x - 20, 0, 90))
            t.kv('player1', 'velocity', '300 0 -300')
            t.wait(0.25)
            t.snap(side, 'player1', 'origin', 'velocity')
            t.snap(side, side + '_d', 'origin')
            t.teleport('player1', (0, -220, 0))
            t.wait(0.3)
        t.expect.distance('w', 'player1', 'w_d', 0, 120)
        t.expect.distance('c', 'player1', 'c_d', 0, 20)
        t.expect.value('w', 'player1', 'velocity.x', 200, op='>')
        t.expect.value('c', 'player1', 'velocity.x', 0, tol=5)

    @suite.test('trigger_teleport flag 1024 Grenades, items (with a Landmark): a gib falling through it comes out at '
                'the destination; without the flag the gib falls through', doc=DOC_FLAGS, players=1,
                covers=('trigger_teleport:1024', 'trigger_teleport.message'))
    def teleport_objects(t):
        for side, x, sf in (('w', -120, '1024'), ('c', 120, '0')):
            t.trigger('trigger_teleport', size=(96, 96, 72), at=(x, 0, 0), targetname=side, target=side + '_d',
                      message=side + '_lm', spawnflags=sf)
            t.point('info_target', targetname=side + '_lm', at=(x, 0, 36))
            t.point('info_teleport_destination', targetname=side + '_d', at=(x, 180, 120))
            t.point('gibshooter', targetname=side + '_g', at=(x, 0, 150), m_iGibs='1', m_flGibLife='3',
                    m_iszVelocity='0 0 -1', m_flVelocity='100', m_flVariance='0', m_iszSpawnTarget=side + '_tag',
                    names=('m_iszVelocity', 'm_iszSpawnTarget'))
            # names the new gib side_p, so the test snapshots it
            t.point('trigger_changevalue', targetname=side + '_tag', target='!activator', m_iszValueName='targetname',
                    m_iszNewValue=t.name(side + '_p'), m_iszValueType='0')
        t.teleport('player1', (0, -220, 0))
        t.wait(0.3)
        t.fire('w_g')
        t.fire('c_g')
        t.wait(0.8)
        for side in ('w', 'c'):
            t.snap('s', side + '_p', 'origin')
            t.snap('s', side + '_d', 'origin')
            t.snap('s', side + '_lm', 'origin')
        t.expect.distance('s', 'w_p', 'w_d', 0, 60)
        t.expect.distance('s', 'c_p', 'c_lm', 0, 60)


def _copyvalue_cases(suite):
    @suite.test('trigger_copyvalue flag 2 Don\'t use Y / 4 Don\'t use Z: a copied vector keeps the destination\'s Y / '
                'Z; Trigger after operation fires after the copy; trigonometric behavior 0 Degrees: Sin of 30 is '
                '0.5, 1 Radians: Sin of 30 is -0.988', doc=DOC_FLAGS,
                covers=('trigger_copyvalue:2', 'trigger_copyvalue:4', 'trigger_copyvalue.message',
                        'trigger_copyvalue.m_trigonometricbehaviour=0', 'trigger_copyvalue.m_trigonometricbehaviour=1'))
    def copyvalue_flags(t):
        t.point('info_target', targetname='vs', **{'$v_s': '7 8 9', '$f_s': '30'})
        for n in ('dy', 'dz'):
            t.point('info_target', targetname=n, **{'$v_d': '1 2 3'})
        for n in ('deg', 'rad'):
            t.point('info_target', targetname=n, **{'$f_d': '0'})
        t.point('info_target', targetname='after')
        t.point('trigger_copyvalue', targetname='cy', netname='vs', m_iszSrcValueName='$v_s', target='dy',
                m_iszDstValueName='$v_d', spawnflags='2', message='after')
        t.point('trigger_copyvalue', targetname='cz', netname='vs', m_iszSrcValueName='$v_s', target='dz',
                m_iszDstValueName='$v_d', spawnflags='4')
        for n, b in (('deg', '0'), ('rad', '1')):
            t.point('trigger_copyvalue', targetname='c' + n, netname='vs', m_iszSrcValueName='$f_s', target=n,
                    m_iszDstValueName='$f_d', m_iszValueType='17', m_trigonometricBehaviour=b)
        t.wait(0.1)
        t.mark('go')
        for n in ('cy', 'cz', 'cdeg', 'crad'):
            t.fire(n)
        t.wait(0.2)
        t.snap('s', 'dy', 'kv:$v_d')
        t.snap('s', 'dz', 'kv:$v_d')
        t.snap('s', 'deg', 'kv:$f_d')
        t.snap('s', 'rad', 'kv:$f_d')
        t.expect.value('s', 'dy', 'kv:$v_d', [7, 2, 9], tol=0.001)
        t.expect.value('s', 'dz', 'kv:$v_d', [7, 8, 3], tol=0.001)
        t.expect.value('s', 'deg', 'kv:$f_d', 0.5, tol=0.001)
        t.expect.value('s', 'rad', 'kv:$f_d', -0.988, tol=0.001)
        t.expect.used('after', count=1, after='go')


def _camera_cases(suite):
    @suite.test('trigger_camera flag 1 Start At Player: fired by a player, it starts at his eyes; without it, at its '
                'own place', doc=DOC_FLAGS, players=1, covers=('trigger_camera:1',))
    def camera_start_at_player(t):
        t.point('info_target', targetname='look', at=(0, 200, 40))
        t.point('trigger_camera', targetname='cw', at=(200, 200, 100), target='look', wait='0.5', spawnflags='1')
        t.point('trigger_camera', targetname='cc', at=(200, 200, 100), target='look', wait='0.5', spawnflags='0')
        t.teleport('player1', (-150, -150, 0))
        t.wait(0.3)
        t.fire('cw', activator='player1')
        t.wait_frames(2)
        t.snap('w', 'cw', 'origin')
        t.snap('w', 'player1', 'origin')
        t.wait(1.0)
        t.fire('cc', activator='player1')
        t.wait_frames(2)
        t.snap('c', 'cc', 'origin')
        t.snap('c', 'player1', 'origin')
        t.wait(1.0)
        t.expect.distance('w', 'cw', 'player1', 0, 2)
        t.expect.distance('c', 'cc', 'player1', 300, 600)

    @suite.test('trigger_camera flag 2 Follow Player: with no target it looks at the player who fired it (he gets its '
                'view); without the flag and target it has nothing to look at and does not start',
                doc=DOC_FLAGS, players=1, covers=('trigger_camera:2',))
    def camera_follow_player(t):
        t.point('trigger_camera', targetname='fw', at=(200, 200, 100), wait='0.5', spawnflags='2')
        t.point('trigger_camera', targetname='fc', at=(-200, 200, 100), wait='0.5', spawnflags='0')
        t.wait(0.3)
        t.mark('w')
        t.fire('fw', activator='player1')
        t.wait(1.0)
        t.mark('c')
        t.fire('fc', activator='player1')
        t.wait(1.0)
        t.expect.setview('player1', 'fw', min=1, after='w', within=(0, 0.3))
        t.expect.setview('player1', 'fc', count=0, after='c')


def _once_cases(suite):
    @suite.test('trigger_sequence flag 1 Fire once: fired twice, it runs its entry once; without it, twice',
                doc=DOC_FLAGS, covers=('trigger_sequence:1',))
    def sequence_once(t):
        seq = t.sequence('once', '    #firetargets = "%s"' % t.name('a'))
        seq2 = t.sequence('twice', '    #firetargets = "%s"' % t.name('b'))
        t.point('trigger_sequence', targetname='sw', sequence_id=seq, spawnflags='1')
        t.point('trigger_sequence', targetname='sc', sequence_id=seq2)
        t.point('info_target', targetname='a')
        t.point('info_target', targetname='b')
        t.mark('go')
        for _ in range(2):
            t.fire('sw', use='on')
            t.fire('sc', use='on')
            t.wait(0.3)
        t.expect.fired('a', count=1, after='go')
        t.expect.fired('b', count=2, after='go')

    @suite.test('game_score flag 1 Allow Negative: -5 points take a player with 0 to -5; without it he stays at 0',
                doc=DOC_FLAGS, players=1, covers=('game_score:1', 'game_score.points'))
    def score_negative(t):
        t.point('game_score', targetname='sw', points='-5', spawnflags='1')
        t.point('game_score', targetname='sc', points='-5')
        t.kv('player1', 'frags', '0')
        t.wait(0.1)
        t.fire('sw', activator='player1')
        t.wait(0.1)
        t.snap('w', 'player1', 'frags')
        t.kv('player1', 'frags', '0')
        t.fire('sc', activator='player1')
        t.wait(0.1)
        t.snap('c', 'player1', 'frags')
        t.kv('player1', 'frags', '0')
        t.expect.value('w', 'player1', 'frags', -5)
        t.expect.value('c', 'player1', 'frags', 0)

    @suite.test('Remove On fire (1) on game_team_master (Team index -1: any team), game_team_set and '
                'game_player_hurt: fired twice, each acts once and is removed; the copies without the flag act twice '
                'and stay', doc=DOC_FLAGS, players=1,
                covers=('game_team_master:1', 'game_team_set:1', 'game_player_hurt:1'))
    def remove_on_fire(t):
        for side, sf in (('w', '1'), ('c', '0')):
            t.point('game_team_master', targetname='tm_' + side, teamindex='-1', target='tm_out_' + side,
                    triggerstate='2', spawnflags=sf)
            t.point('game_team_set', targetname='ts_' + side, target='ts_out_' + side, spawnflags=sf)
            t.point('game_player_hurt', targetname='ph_' + side, dmg='1', spawnflags=sf)
            for n in ('tm_out_', 'ts_out_'):
                t.point('info_target', targetname=n + side)
        t.flags('player1', '+FL_GODMODE')
        t.wait(0.1)
        t.mark('go')
        for _ in range(2):
            for side in ('w', 'c'):
                for n in ('tm_', 'ts_', 'ph_'):
                    t.fire(n + side, activator='player1')
            t.wait(0.2)
        t.flags('player1', '-FL_GODMODE')
        for n in ('tm_', 'ts_', 'ph_'):
            t.expect.removed(n + 'w', count=1, after='go')
            t.expect.not_removed(n + 'c', after='go')
        t.expect.used('tm_out_w', count=1, after='go')
        t.expect.used('tm_out_c', count=2, after='go')
        t.expect.used('ts_out_w', count=1, after='go')
        t.expect.used('ts_out_c', count=2, after='go')
        t.expect.damaged('player1', attacker='ph_w', count=1, after='go')
        t.expect.damaged('player1', attacker='ph_c', count=2, after='go')


def _path_lane(t, p, y, x0=-200, x1=200, xm=0, mid=None, alt=None, start='a', **train):
    """path_tracks p_a (x0) -> p_x (xm) -> p_b (x1) on lane y, p_x with keys mid and a branch p_alt (alt: its
    position) if given; a func_tracktrain p_tr at `start`"""
    mid = dict(mid or {})
    if alt:
        mid['altpath'] = p + '_alt'
        t.point('path_track', targetname=p + '_alt', at=alt)
    t.point('path_track', targetname=p + '_a', target=p + '_x', at=(x0, y, 8))
    t.point('path_track', targetname=p + '_x', target=p + '_b', at=(xm, y, 8), **mid)
    t.point('path_track', targetname=p + '_b', at=(x1, y, 8))
    train.setdefault('speed', '400')
    t.brush('func_tracktrain', targetname=p + '_tr', size=(32, 32, 16), at=(x0 if start == 'a' else x1, y, 8),
            target=p + '_' + start, wheels='16', height='0', startspeed='0', origin=True, **train)


def _path_cases(suite):
    @suite.test('path_track flag 1 Disabled: a train cannot drive past it (stays at the path before it); without it '
                'the train drives on to the end', doc=DOC_FLAGS, covers=('path_track:1',))
    def path_disabled(t):
        _path_lane(t, 'w', -60, mid={'spawnflags': '1'})
        _path_lane(t, 'c', 60)
        t.wait(0.3)
        t.fire('w_tr', use='on')
        t.fire('c_tr', use='on')
        t.wait(2.0)
        t.snap('s', 'w_tr', 'origin')
        t.snap('s', 'w_a', 'origin')
        t.snap('s', 'c_tr', 'origin')
        t.snap('s', 'c_b', 'origin')
        t.expect.distance('s', 'w_tr', 'w_a', 0, 40)
        t.expect.distance('s', 'c_tr', 'c_b', 0, 40)

    @suite.test('path_track flag 2 Fire once: a train going round a loop twice fires its Fire On Pass once; a path '
                'without the flag fires each lap', doc=DOC_FLAGS, covers=('path_track:2',))
    def path_fire_once(t):
        t.point('path_track', targetname='l1', target='l2', at=(-180, -150, 8))
        t.point('path_track', targetname='l2', target='l3', at=(180, -150, 8), message='once', spawnflags='2')
        t.point('path_track', targetname='l3', target='l1', at=(0, 180, 8), message='each')
        t.point('info_target', targetname='once')
        t.point('info_target', targetname='each')
        t.brush('func_tracktrain', targetname='tr', size=(32, 32, 16), at=(-180, -150, 8), target='l1', wheels='16',
                height='0', speed='500', startspeed='0', origin=True)
        t.wait(0.3)
        t.mark('go')
        t.fire('tr', use='on')
        t.wait(5.0)
        t.fire('tr', use='off')
        t.expect.fired('once', count=1, after='go')
        t.expect.fired('each', min=2, after='go')

    @suite.test('path_track Branch Path: switched on (fired), a train takes the branch; flag 4 Branch Reverse: the '
                'branch is for trains going backwards, a train going forwards keeps to the main path; a branch not '
                'switched on is not taken', doc=DOC_FLAGS,
                covers=('path_track.altpath', 'path_track:4'))
    def path_branch(t):
        _path_lane(t, 'br', -150, alt=(0, -50, 8))
        _path_lane(t, 'rv', 0, mid={'spawnflags': '4'}, alt=(0, 100, 8))
        _path_lane(t, 'off', 150, alt=(0, 220, 8))
        t.wait(0.3)
        t.fire('br_x')          # a path with a branch: a use switches between its next path and the branch
        t.fire('rv_x')
        t.wait(0.1)
        for p in ('br', 'rv', 'off'):
            t.fire(p + '_tr', use='on')
        t.wait(2.0)
        for p in ('br', 'rv', 'off'):
            t.snap('s', p + '_tr', 'origin')
            t.snap('s', p + '_b', 'origin')
            t.snap('s', p + '_alt', 'origin')
        t.expect.distance('s', 'br_tr', 'br_alt', 0, 40, flat=False)
        t.expect.distance('s', 'rv_tr', 'rv_b', 0, 40, flat=False)
        t.expect.distance('s', 'off_tr', 'off_b', 0, 40, flat=False)


def _button_cases(suite):
    @suite.test('func_pendulum flag 16 Auto-return: fired again while swinging, it swings back to where it started; '
                'without it, it stops where it is', doc=DOC_FLAGS, covers=('func_pendulum:16',))
    def pendulum_return(t):
        for n, x, sf in (('pw', -120, '16'), ('pc', 120, '0')):
            t.brush('func_pendulum', size=(16, 16, 64), at=(x, 0, 32), targetname=n, speed='120', distance='90',
                    spawnflags=sf, origin=True)
        t.wait(0.3)
        t.fire('pw')
        t.fire('pc')
        t.wait(0.8)
        t.fire('pw')
        t.fire('pc')
        t.wait(2.0)
        t.snap('s', 'pw', 'angles', 'avelocity')
        t.snap('s', 'pc', 'angles', 'avelocity')
        t.expect.value('s', 'pw', 'angles.y', 0, tol=0.5)
        t.expect.value('s', 'pw', 'avelocity', [0, 0, 0], tol=0.01)
        t.expect.value('s', 'pc', 'angles.y', 10, op='>')

    @suite.test('func_pendulum flag 64 X Axis: it swings about X (roll); without an axis flag about Z (yaw)',
                doc=DOC_FLAGS, covers=('func_pendulum:64',))
    def pendulum_x(t):
        for n, x, sf in (('px', -120, '64'), ('pz', 120, '0')):
            t.brush('func_pendulum', size=(16, 16, 64), at=(x, 0, 32), targetname=n, speed='120', distance='90',
                    spawnflags=sf, origin=True)
        t.wait(0.3)
        t.fire('px')
        t.fire('pz')
        t.wait(0.8)
        t.snap('s', 'px', 'angles')
        t.snap('s', 'pz', 'angles')
        t.expect.value('s', 'px', 'angles.z', 10, op='>')
        t.expect.value('s', 'px', 'angles.y', 0, tol=0.5)
        t.expect.value('s', 'pz', 'angles.y', 10, op='>')
        t.expect.value('s', 'pz', 'angles.z', 0, tol=0.5)

    def rot_button(t, n, x, **kv):
        kv.setdefault('speed', '2000')
        kv.setdefault('distance', '45')
        kv.setdefault('wait', '1')
        t.brush('func_rot_button', size=(32, 8, 40), at=(x, 60, 0), targetname=n, origin=True, target=n + '_out',
                **kv)
        t.point('info_target', targetname=n + '_out', at=(x, 200, 0))

    @suite.test('func_rot_button flag 2 Reverse Dir: pressed, it turns the other way (-45); Delay before reset -1 '
                'Stays pressed: still turned 2 s later; the plain button (wait 1) is back', doc=DOC_FLAGS, players=1,
                covers=('func_rot_button:2', 'func_rot_button.wait=-1'))
    def rot_button_reverse_stay(t):
        rot_button(t, 'rv', -150, spawnflags='2')
        rot_button(t, 'st', 0, wait='-1')
        rot_button(t, 'pl', 150)
        t.wait(0.3)
        for n in ('rv', 'st', 'pl'):
            t.fire(n, activator='player1')
        t.wait(0.3)
        t.snap('a', 'rv', 'angles')
        t.snap('a', 'pl', 'angles')
        t.wait(2.0)
        t.snap('b', 'st', 'angles')
        t.snap('b', 'pl', 'angles')
        t.expect.value('a', 'rv', 'angles.y', -45, tol=1)
        t.expect.value('a', 'pl', 'angles.y', 45, tol=1)
        t.expect.value('b', 'st', 'angles.y', 45, tol=1)
        t.expect.value('b', 'pl', 'angles.y', 0, tol=1)

    @suite.test('func_rot_button flag 256 Touch Activates: a player walking into it presses it; without the flag '
                'walking into it does nothing', doc=DOC_FLAGS, players=1, covers=('func_rot_button:256',))
    def rot_button_touch(t):
        rot_button(t, 'tw', -120, spawnflags='256')
        rot_button(t, 'tc', 120)
        t.wait(0.3)
        t.mark('go')
        for x in (-120, 120):
            t.teleport('player1', (x, 0, 0), yaw=90)
            t.look('player1', 0, 90)
            t.walk('player1', 300, 0, 0.6)
            t.teleport('player1', (0, -200, 0))
            t.wait(0.3)
        t.expect.fired('tw_out', min=1, after='go')
        t.expect.not_fired('tc_out', after='go')

    @suite.test('momentary_rot_button flag 1 Door Hack: solid (a player cannot walk through it); without it not '
                'solid', doc=DOC_FLAGS, players=1, covers=('momentary_rot_button:1',))
    def momentary_door_hack(t):
        for n, x, sf in (('dh', -120, '1'), ('nd', 120, '0')):
            t.brush('momentary_rot_button', size=(64, 16, 72), at=(x, 60, 0), targetname=n, origin=True,
                    speed='90', distance='90', spawnflags=sf)
        t.wait(0.3)
        for n, x in (('dh', -120), ('nd', 120)):
            t.teleport('player1', (x, 0, 0), yaw=90)
            t.look('player1', 0, 90)
            t.walk('player1', 300, 0, 0.8)
            t.snap(n, 'player1', 'origin')
            t.snap(n, n, 'origin', 'solid')
            t.teleport('player1', (0, -200, 0))
            t.wait(0.2)
        t.expect.value('dh', 'dh', 'solid', 4)
        t.expect.value('nd', 'nd', 'solid', 0)
        # the player's center: stopped before the handle (y 60 - 8 - 16) or walked past it
        t.expect.value('dh', 'player1', 'origin.y', t._abs((0, 40, 0))[1], op='<')
        t.expect.value('nd', 'player1', 'origin.y', t._abs((0, 80, 0))[1], op='>')

    @suite.test('momentary_rot_button flag 64 X Axis / 128 Y Axis: used, it turns about X (roll) / Y (pitch); without '
                'an axis flag about Z (yaw)', doc=DOC_FLAGS, players=1,
                covers=('momentary_rot_button:64', 'momentary_rot_button:128'))
    def momentary_axes(t):
        for n, x, sf in (('mx', -120, '64'), ('my', 0, '128'), ('mz', 120, '0')):
            t.brush('momentary_rot_button', size=(16, 16, 32), at=(x, 60, 20), targetname=n, origin=True,
                    speed='100', distance='90', spawnflags=sf, target=n + '_out')
            t.point('info_target', targetname=n + '_out', at=(x, 200, 0))
        t.wait(0.3)
        for n in ('mx', 'my', 'mz'):
            t.fire(n, activator='player1')
        t.wait(0.05)
        for n in ('mx', 'my', 'mz'):
            t.snap('s', n, 'angles')
        t.expect.value('s', 'mx', 'angles.z', 1, op='>')
        t.expect.value('s', 'mx', 'angles.y', 0, tol=0.01)
        t.expect.value('s', 'my', 'angles.x', 1, op='>')
        t.expect.value('s', 'my', 'angles.y', 0, tol=0.01)
        t.expect.value('s', 'mz', 'angles.y', 1, op='>')


def _train_flag_cases(suite):
    @suite.test('func_trackautochange flag 16 Rotate Only (Spin amount 90) and 64 X Axis / 128 Y Axis: a train drives '
                'onto each; it turns back from 90 to 0 about Z / X / Y and stays at its height', doc=DOC_FLAGS,
                covers=('func_trackautochange:16', 'func_trackautochange:64', 'func_trackautochange:128',
                        'func_trackautochange.rotation'))
    def trackauto_rotate(t):
        rows = (('rz', 16, 'angles.y'), ('rx', 16 | 64, 'angles.z'), ('ry', 16 | 128, 'angles.x'))
        for (p, sf, _f), y in zip(rows, _lanes(len(rows))):
            train_lane(t, p, y, spawnflags=str(sf), rotation='90', speed='90')
        t.wait(0.3)
        for p, _sf, _f in rows:
            t.snap('a', p, 'origin', 'angles')
            t.fire(p + '_tr', use='on')
        t.wait(3.0)
        for p, _sf, _f in rows:
            t.snap('b', p, 'origin', 'angles')
        for p, _sf, f in rows:
            t.expect.value('a', p, f, 90, tol=2)
            t.expect.value('b', p, f, 0, tol=2)
            t.expect.moved('a', 'b', p, delta=(0, 0, 0), tol=1)

    @suite.test('func_tracktrain flag 4 Forwards Only: a Set use backwards (-1) leaves it still; without the flag it '
                'drives backwards', doc=DOC_FLAGS, covers=('func_tracktrain:4',))
    def tracktrain_forward_only(t):
        for n, y, sf in (('fo', -60, '4'), ('bk', 60, '0')):
            t.point('path_track', targetname=n + '_a', target=n + '_b', at=(-200, y, 8))
            t.point('path_track', targetname=n + '_b', at=(200, y, 8))
            t.brush('func_tracktrain', targetname=n, size=(32, 32, 16), at=(200, y, 8), target=n + '_b',
                    wheels='16', height='0', speed='200', startspeed='0', spawnflags=sf, origin=True)
        t.wait(0.3)
        t.snap('a', 'fo', 'origin')
        t.snap('a', 'bk', 'origin')
        t.fire('fo', use='set', value=-1)
        t.fire('bk', use='set', value=-1)
        t.wait(1.0)
        t.snap('b', 'fo', 'origin', 'speed')
        t.snap('b', 'bk', 'origin', 'speed')
        t.expect.moved('a', 'b', 'fo', delta=(0, 0, 0), tol=1)
        t.expect.value('b', 'fo', 'speed', 0)
        t.expect.value('b', 'bk', 'speed', 0, op='<')

    @suite.test('func_tracktrain flag 8 Passable: it drives through a player standing on its track (not solid); '
                'without it the player is pushed along', doc=DOC_FLAGS, players=1, covers=('func_tracktrain:8',))
    def tracktrain_passable(t):
        for n, y, sf in (('ps', -100, '8'), ('so', 100, '0')):
            t.point('path_track', targetname=n + '_a', target=n + '_b', at=(-200, y, 36))
            t.point('path_track', targetname=n + '_b', at=(200, y, 36))
            t.brush('func_tracktrain', targetname=n, size=(48, 48, 48), at=(-200, y, 36), target=n + '_a',
                    wheels='16', height='0', speed='200', startspeed='0', spawnflags=sf, dmg='0',
                    origin=True)
        t.wait(0.3)
        t.snap('s', 'ps', 'solid')
        t.snap('s', 'so', 'solid')
        t.expect.value('s', 'ps', 'solid', 0)
        t.expect.value('s', 'so', 'solid', 4)
        for n, y in (('ps', -100), ('so', 100)):
            t.teleport('player1', (0, y, 0))
            t.wait(0.2)
            t.snap(n + '0', 'player1', 'origin')
            t.fire(n, use='on')
            t.wait(1.6)
            t.fire(n, use='off')
            t.snap(n + '1', 'player1', 'origin')
            t.teleport('player1', (0, -220, 0))
            t.wait(0.2)
        t.expect.moved('ps0', 'ps1', 'player1', delta=(0, 0, 0), tol=2, flat=True)
        t.expect.moved('so0', 'so1', 'player1', min_dist=40, flat=True)


FLAG_GENERATORS = (_teleport_cases, _copyvalue_cases, _camera_cases, _once_cases, _path_cases, _button_cases,
                   _train_flag_cases)
CANTOUCH_CLASSES = ('trigger_inout', 'trigger_jump')


def flags_plan():
    """The trigger_inout / trigger_jump flag bits read from the FGD (Featureful_CanTouch's table must cover them)."""
    classes, props = _fgd()
    out = []
    for cn in CANTOUCH_CLASSES:
        bits = [int(v) for v in props(classes, cn)['spawnflags']['choices']]
        missing = [b for b in bits if b not in CANTOUCH]
        if missing:
            raise KeyError('%s: no toucher for FGD flag(s) %s' % (cn, missing))
        out.append((cn, bits))
    return out


def generate_flags(suite, part):
    """G7. part 0: trigger_inout / trigger_jump, every flag (each with its kind of toucher) and netname; part 1: the
    trigger_teleport, trigger_copyvalue, trigger_camera, trigger_sequence, game_*, path_track, pendulum, button and
    train flags."""
    if part == 0:
        for cn, bits in flags_plan():
            for b in bits:
                _cantouch_case(suite, cn, b)
            _cantouch_netname(suite, cn)
    else:
        for gen in FLAG_GENERATORS:
            gen(suite)


if __name__ == '__main__':
    cases, skipped = use_plan()
    print('use types: %d generated groups' % len(cases))
    for kind, cn, values in cases:
        print('  %-7s %-24s %s' % (kind, cn, values))
    print('left out:')
    for cn, what, why in skipped:
        print('  %-7s %-24s %s' % (what, cn, why))
