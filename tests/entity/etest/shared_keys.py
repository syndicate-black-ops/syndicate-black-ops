"""Shared-key tests generated from sdk/syndicate.fgd (suites fgd_shared_*).

For every class the FGD gives one of the shared keys (target, delay, killtarget, master, triggerstate), one test
places the class with that key set and checks the key does what the FGD says:
- target: activated, it fires its target
- delay: its target fires `delay` seconds after the activation, not before
- killtarget: activated, it removes the named entity
- master: with a locked master it does nothing; with an unlocked one it works
- triggerstate: Off / On / Toggle reach the target as USE_OFF / USE_ON / USE_TOGGLE

Which classes get a test comes from the FGD: a class with the key, whose key label is the shared meaning (a key
named "target" that the FGD labels "First stop target" or "Destination entity" means something else and is left
out with that label as the reason). How a class is activated in a test room is the one thing the FGD cannot say:
RECIPES maps a class to its activation; a class without one is left out with the reason in SKIP.

`python3 tests/entity/etest/shared_keys.py` prints the plan: every generated test and every class left out, with
its reason.
"""

import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
FGD_PATH = os.path.join(ROOT, 'sdk', 'syndicate.fgd')
KEYS = ('target', 'delay', 'killtarget', 'master', 'triggerstate')
USE_OFF, USE_ON, USE_TOGGLE = 0, 1, 3

# FGD labels that mean the shared behavior; other labels name a different meaning for the same key
SHARED_LABELS = {
    'target': ('Target', 'Target (fired on look)', 'Fire on entering', 'Fire on state change (On / Off)',
               'Target when finished (time is up)', 'Fire on equip'),
    'delay': ('Delay before trigger', 'Delay before fire'),
    'killtarget': ('KillTarget', 'Kill target'),
    'master': ('Master', '(Team) Master', 'Team Master'),
    'triggerstate': ('Trigger State', 'Trigger state for each entity'),
}

# ---------------------------------------------------------------------------------------------- activations
# A recipe: kind (how it is activated), how it is placed, and extra keys. place(t, cls, name, x, keys) puts the class
# at x (copies of one test sit at x = -120 / 0 / 120); act(t, name, x) activates it.
TOUCH_Y = 120


def _fire(t, name, x):
    t.fire(name, activator='player1')


def _fire_twice(t, name, x):
    t.fire(name, activator='player1')
    t.wait(0.05)
    t.fire(name, activator='player1')


def _input(t, name, x):
    t.fire(name + '_in', activator='player1')


def _touch(t, name, x):
    t.teleport('player1', (x, TOUCH_Y, 0))
    t.wait(0.4)
    t.teleport('player1', (0, -200, 0))
    t.wait(0.1)


def _look(t, name, x):
    t.teleport('player1', (x, TOUCH_Y, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.8)
    t.teleport('player1', (0, -200, 0))
    t.wait(0.1)


def _zone(t, name, x):
    t.teleport('player1', (x, TOUCH_Y, 0))
    t.wait(0.1)
    t.fire(name, activator='player1')
    t.wait(0.1)
    t.teleport('player1', (0, -200, 0))
    t.wait(0.1)


def _airtank(t, name, x):
    """a player takes the air; the tank fires its target when it refills, 30 s later"""
    t.teleport('player1', (x, 60, 0))
    t.wait(0.3)
    t.teleport('player1', (0, -200, 0))
    t.wait(30.5)


def _damage(t, name, x):
    t.damage(name, 1000, attacker='player1')


def _point(cls, **extra):
    def place(t, name, x, keys):
        t.point(cls, targetname=name, at=(x, 60, 0), **dict(extra, **keys))
    return place


def _brush(cls, size=(32, 32, 32), y=60, z=0, **extra):
    def place(t, name, x, keys):
        t.brush(cls, size=size, at=(x, y, z), targetname=name, **dict(extra, **keys))
    return place


def _trigger(cls, size=(64, 64, 72), **extra):
    def place(t, name, x, keys):
        t.trigger(cls, size=size, at=(x, TOUCH_Y, 0), targetname=name, **dict(extra, **keys))
    return place


def _multisource(t, name, x, keys):
    t.point('multisource', targetname=name, at=(x, 60, 0), **keys)
    t.point('trigger_relay', targetname=name + '_in', target=name, at=(x, 40, 0), triggerstate='1')


def _look_trigger(t, name, x, keys):
    t.point('info_target', targetname=name + '_spot', at=(x, 240, 64))
    t.trigger('trigger_look', size=(64, 64, 72), at=(x, TOUCH_Y, 0), targetname=name, netname=name + '_spot',
              look_time='0.2', field_of_view='0.7', names=('netname',), **keys)


def _zone_trigger(t, name, x, keys):
    keys = dict(keys)
    if 'target' in keys:                     # game_zone_player fires intarget for each player in the zone
        keys['intarget'] = keys.pop('target')
    t.trigger('game_zone_player', size=(64, 64, 72), at=(x, TOUCH_Y, 0), targetname=name, **keys)


def _iterator(t, name, x, keys):
    t.point('info_target', targetname=name + '_thing', at=(x, 200, 0))
    t.point('trigger_entity_iterator', targetname=name, at=(x, 60, 0), name_filter=name + '_thing',
            names=('name_filter',), **keys)


def _script(cls, monster='monster_scientist', **extra):
    """a script class with its own monster (name_m) next to it; the player is notarget"""
    def place(t, name, x, keys):
        t.flags('player1', '+FL_NOTARGET')
        t.point(monster, targetname=name + '_m', at=(x, 100, 0), angles='0 270 0', spawnflags='16')
        kv = dict(extra)
        kv[SCRIPT_ENTITY_KEY.get(cls, 'm_iszEntity')] = name + '_m'
        t.point(cls, targetname=name, at=(x, 100, 0), angles='0 270 0',
                names=(SCRIPT_ENTITY_KEY.get(cls, 'm_iszEntity'),), **dict(kv, **keys))
    return place


SCRIPT_ENTITY_KEY = {'scripted_sentence': 'entity'}


def _plantlight(t, name, x, keys):
    """a player walking into it makes it duck and turn its light off (fires its target with Off)"""
    t.point('xen_plantlight', targetname=name, at=(x, TOUCH_Y, 0), **keys)


DOOR = dict(speed='2000', wait='-1', lip='8')
RECIPES = {
    'trigger_relay': (_point('trigger_relay'), _fire),
    'trigger_counter': (_point('trigger_counter', count='2'), _fire_twice),
    'trigger_changetarget': (_point('trigger_changetarget', m_iszNewTarget='x'), _fire),
    'multisource': (_multisource, _input),
    'button_target': (_brush('button_target', size=(16, 32, 32), z=40), _fire),
    'func_button': (_brush('func_button', size=(16, 32, 32), z=40, speed='2000', wait='1', angles='0 90 0'), _fire),
    'func_rot_button': (_brush('func_rot_button', size=(8, 32, 32), z=28, origin=True, speed='2000',
                               distance='45', wait='1'), _fire),
    'momentary_rot_button': (_brush('momentary_rot_button', size=(8, 32, 32), z=28, origin=True, speed='90',
                                    distance='90'), _fire),
    'func_door': (_brush('func_door', size=(32, 16, 64), angles='-90 0 0', **DOOR), _fire),
    'func_door_rotating': (_brush('func_door_rotating', size=(16, 32, 64), origin=True, distance='45', speed='900',
                                  wait='-1'), _fire),
    'func_water': (_brush('func_water', size=(32, 16, 64), angles='-90 0 0', skin='-3', **DOOR), _fire),
    'func_breakable': (_brush('func_breakable', health='1', material='1'), _damage),
    'func_pushable': (_brush('func_pushable', health='1', material='1', spawnflags='128'), _damage),
    'item_airtank': (_point('item_airtank'), _airtank),
    'item_battery': (_point('item_battery'), lambda t, name, x: (t.teleport('player1', (x, 60, 0)), t.wait(0.3),
                                                                  t.teleport('player1', (0, -200, 0)))),
    'game_counter': (_point('game_counter', health='1', frags='0'), _fire),
    'game_counter_set': (_point('game_counter_set', frags='1'), _fire),
    'game_team_master': (_point('game_team_master', teamindex='-1'), _fire),
    'game_team_set': (_point('game_team_set'), _fire),
    'game_player_settings': (_point('game_player_settings'), _fire),
    'env_state': (_point('env_state'), _fire),
    'env_message': (_point('env_message', message='shared key test'), _fire),
    'env_warpball': (_point('env_warpball', spawnflags='4', beamcount='2'), _fire),
    'env_tutor': (_point('env_tutor', message='shared key test', spawnflags='1'), _fire),
    'light': (_point('light', style='32'), _fire),
    'light_spot': (_point('light_spot', style='32'), _fire),
    'trigger_entity_iterator': (_iterator, _fire),
    'trigger_camera': (_point('trigger_camera', wait='0.3'), _fire),
    'scripted_sequence': (_script('scripted_sequence', m_iszPlay='console', m_fMoveTo='0'), _fire, 10.0),
    'aiscripted_sequence': (_script('aiscripted_sequence', m_iszPlay='console', m_fMoveTo='0'), _fire, 10.0),
    'scripted_action': (_script('scripted_action', monster='monster_human_grunt', m_fAction='5', m_fMoveTo='0',
                                spawnflags='64'), _fire, 2.0),
    'scripted_sentence': (_script('scripted_sentence', sentence='!SC_HELLO0', duration='1', radius='512',
                                  spawnflags='4'), _fire, 3.0),
    'xen_plantlight': (_plantlight, _touch, 2.0),
    'trigger_once': (_trigger('trigger_once'), _touch),
    'trigger_multiple': (_trigger('trigger_multiple', wait='1'), _touch),
    'trigger_hurt': (_trigger('trigger_hurt', dmg='1'), _touch),
    'trigger_inout': (_trigger('trigger_inout'), _touch),
    'trigger_look': (_look_trigger, _look),
    'game_zone_player': (_zone_trigger, _zone),
}


# Classes whose master locks an effect other than a target: the whole master test body. The locked copy runs first,
# then the unlocked one, each in its own time window.
def _master_hurt(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    t.point('game_player_hurt', targetname='e_l', dmg='5', master='lk')
    t.point('game_player_hurt', targetname='e_o', dmg='5', master='op')
    t.kv('player1', 'health', '100')
    t.wait(0.3)
    t.mark('l')
    t.fire('e_l', activator='player1')
    t.wait(0.5)
    t.mark('o')
    t.fire('e_o', activator='player1')
    t.wait(0.5)
    t.expect.not_damaged('player1', after='l', within=(0, 0.5))
    t.expect.damaged('player1', after='o', min=1)


def _master_score(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    t.point('game_score', targetname='e_l', points='5', master='lk')
    t.point('game_score', targetname='e_o', points='5', master='op')
    t.kv('player1', 'frags', '0')
    t.wait(0.3)
    t.fire('e_l', activator='player1')
    t.wait(0.2)
    t.snap('l', 'player1', 'frags')
    t.fire('e_o', activator='player1')
    t.wait(0.2)
    t.snap('o', 'player1', 'frags')
    t.expect.value('l', 'player1', 'frags', 0)
    t.expect.value('o', 'player1', 'frags', 5)


def _master_jump(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    _info(t, 'land', at=(0, -200, 200))
    for n, x, m in (('e_l', -120, 'lk'), ('e_o', 120, 'op')):
        t.trigger('trigger_jump', size=(64, 64, 72), at=(x, TOUCH_Y, 0), targetname=n, target='land', master=m)
    z0 = t._abs((0, 0, 0))[2]
    t.wait(0.3)
    t.teleport('player1', (-120, TOUCH_Y, 0))
    t.wait(0.3)
    t.snap('l', 'player1', 'origin', 'velocity')
    t.teleport('player1', (120, TOUCH_Y, 0))
    t.wait(0.3)
    t.snap('o', 'player1', 'origin', 'velocity')
    t.teleport('player1', (0, -200, 0))
    t.wait(0.5)
    t.expect.value('l', 'player1', 'origin.z', z0 + 40, op='<')
    t.expect.value('o', 'player1', 'origin.z', z0 + 60, op='>')


def _delay_changetarget(t):
    """trigger_changetarget's delay delays the change itself, and its kill target with it"""
    t.point('trigger_relay', targetname='rl', target='old', at=(0, 100, 0))
    t.point('trigger_changetarget', targetname='e', target='rl', m_iszNewTarget='new', delay='1', killtarget='victim',
            at=(0, 60, 0), names=('m_iszNewTarget',))
    _info(t, 'victim')
    t.wait(0.3)
    t.mark('go')
    t.fire('e', activator='player1')
    t.wait(0.5)
    t.snap('early', 'rl', 'target')
    t.wait(1.0)
    t.snap('late', 'rl', 'target')
    t.expect.value('early', 'rl', 'target', t.name('old'))
    t.expect.value('late', 'rl', 'target', t.name('new'))
    t.expect.not_removed('victim', after='go', within=(0, 0.7))
    t.expect.removed('victim', after='go', within=(0.7, 1.5), count=1)


def _master_push(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    for n, x, m in (('e_l', -120, 'lk'), ('e_o', 120, 'op')):
        t.trigger('trigger_push', size=(64, 64, 72), at=(x, TOUCH_Y, 0), targetname=n, speed='400', angles='0 0 0',
                  master=m)
    t.wait(0.3)
    for lab, x in (('l', -120), ('o', 120)):
        t.teleport('player1', (x, TOUCH_Y, 0))
        t.wait_frames(1)
        t.snap(lab + '0', 'player1', 'origin')
        t.wait(0.3)
        t.snap(lab + '1', 'player1', 'origin')
    t.teleport('player1', (0, -200, 0))
    t.wait(0.3)
    t.expect.moved('l0', 'l1', 'player1', delta=(0, 0, 0), tol=5, flat=True)
    t.expect.moved('o0', 'o1', 'player1', min_dist=30, flat=True)


def _master_gravity(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    for n, x, m in (('e_l', -120, 'lk'), ('e_o', 120, 'op')):
        t.trigger('trigger_gravity', size=(64, 64, 72), at=(x, TOUCH_Y, 0), targetname=n, gravity='0.25', master=m)
    t.wait(0.3)
    t.teleport('player1', (-120, TOUCH_Y, 0))
    t.wait(0.3)
    t.snap('l', 'player1', 'gravity')
    t.teleport('player1', (120, TOUCH_Y, 0))
    t.wait(0.3)
    t.snap('o', 'player1', 'gravity')
    t.teleport('player1', (0, -200, 0))
    t.kv('player1', 'gravity', '1')
    t.wait(0.1)
    t.expect.value('l', 'player1', 'gravity', 1.0, tol=0.01)
    t.expect.value('o', 'player1', 'gravity', 0.25, tol=0.01)


def _master_safe_fall(t):
    _lock(t, 'lk', False)
    _lock(t, 'op', True)
    for n, x, m in (('e_l', -120, 'lk'), ('e_o', 120, 'op')):
        t.trigger('trigger_safe_fall', size=(128, 128, 200), at=(x, 0, 0), targetname=n, master=m)
    t.kv('player1', 'health', '1000')
    t.wait(0.3)
    t.mark('l')
    t.teleport('player1', (-120, 0, 700))
    t.wait(2.0)
    t.mark('o')
    t.teleport('player1', (120, 0, 700))
    t.wait(2.0)
    t.expect.damaged('player1', min=1, after='l', within=(0, 2.0))
    t.expect.not_damaged('player1', after='o')


DELAY_BODY = {'trigger_changetarget': _delay_changetarget}

MASTER_SIZE = {'trigger_safe_fall': (512, 512, 1024)}
MASTER_BODY = {'trigger_safe_fall': _master_safe_fall, 'trigger_push': _master_push, 'game_player_hurt': _master_hurt,
               'game_score': _master_score, 'trigger_jump': _master_jump, 'trigger_gravity': _master_gravity}


# Classes with a shared key that the generated tests leave out, and why (keys: 'class' or 'class.key')
SKIP = {
    'trigger_auto': 'fires once at map start, before any test room is entered; the harness only checks inside a '
                    "test's own time",
    'info_player_deathmatch': 'a spawn point: the test players spawn before the tests run',
    'info_player_go2': 'a spawn point: the test players spawn before the tests run',
    'info_player_go3': 'a spawn point: the test players spawn before the tests run',
    'info_player_go4': 'a spawn point: the test players spawn before the tests run',
    'info_player_go5': 'a spawn point: the test players spawn before the tests run',
    'multispawn': 'a spawn point: the test players spawn before the tests run',
    'game_end': 'ends the map (and the suite)',
    'mission_failed': 'ends the mission (and the suite)',
    'nuclear_explosion': 'kills every player on the map',
    'game_playvideo': 'plays a video on the client; the test players are fake clients',
    'vgui_screen': 'a client screen: its target is the screen input, fired by a real client',
    'func_tank': 'mounted through func_tankcontrols by a real +use; master tested in suite fixes',
    'func_tanklaser': 'mounted through func_tankcontrols; see func_tank',
    'func_tankmortar': 'mounted through func_tankcontrols; see func_tank',
    'func_tankrocket': 'mounted through func_tankcontrols; see func_tank',
    'game_player_equip': 'Team Master: co-op has no teams',
    'game_player_team': 'changes teams: co-op has no teams',
    'game_text': 'master: tested in suite gametext',
    'trigger_teleport.target': 'target is the destination (suites triggers, options)',
    'trigger_teleport.master': 'tested in suites stock and triggers',
    'trigger_jump.target': 'target is the landing point (suite mapper)',
    'trigger_monsterjump': 'master: needs a walking monster; tested in suite mapper (fixed on fix/docs-findings)',
    'env_render': 'target is the entities it changes, not a Use',
    'env_shooter.delay': 'delay is the time between shots (suite fgd_shooter)',
    'gibshooter.delay': 'delay is the time between shots (suite fgd_shooter)',
    'monstermaker.delay': 'delay is the time between monsters (monstermaker suites)',
    'trigger_hurt_remote.delay': 'delay is the time between hurts (Constant mode)',
    'trigger_changetarget.target': 'target is the entity whose target it changes (the FGD label is the plain '
                                   '"Target" of base Targetx)',
    'light_spot.target': 'the compiler aims the spot at it (compile-time only)',
    'light.target': 'the compiler turns the light into a spotlight aimed at it (compile-time only; the game never '
                    'fires it)',
}
for _m in ('dark_grunt', 'monster_alien_slave', 'monster_apache', 'monster_barney', 'monster_cyberminotor',
           'monster_cyborg', 'monster_dark', 'monster_furniture', 'monster_gargantua', 'monster_generic',
           'monster_grunt_hunter', 'monster_grunt_repel', 'monster_grunt_hunter_repel', 'monster_human_assassin',
           'monster_human_grunt', 'monster_hunter', 'monster_ichthyosaur', 'monster_imp', 'monster_imp_hunter',
           'monster_miniturret', 'monster_osprey', 'monster_scientist', 'monster_sentry', 'monster_sitting_scientist',
           'monster_snark', 'monster_tripmine', 'monster_turret', 'monster_werewolf', 'monster_werewolf_hunter',
           'monster_zombie', 'monster_zombie_grunt', 'monster_zombie_hunter'):
    SKIP.setdefault(_m, "a monster's target is the first path_corner it walks to")

# Known results: (class, key) -> xfail reason (plain facts; findings of this batch)
XFAIL = {}

# Classes that do nothing at all without a target: the killtarget test gives them one
NEEDS_TARGET = {'trigger_camera': 'a camera without a target to look at does not turn on'}


# ---------------------------------------------------------------------------------------------- plan
def _labels():
    """(class, key) -> the FGD label of that key on the class (through its bases)."""
    sys.path.insert(0, os.path.join(ROOT, 'scripts'))
    import fgd
    classes, _ = fgd.parse_fgd(FGD_PATH)
    lines = open(FGD_PATH, encoding='latin-1').read().split('\n')
    out = {}
    for cn, c in classes.items():
        if c['kind'] == 'baseclass':
            continue
        props = fgd.effective_props(classes, cn)
        for key in KEYS:
            p = props.get(key)
            if p:
                m = re.search(r':\s*"([^"]*)"', lines[p['line'] - 1])
                out[(cn, key)] = m.group(1) if m else ''
    return out


def plan():
    """(cases, skipped): cases [(class, key)], skipped [(class, key, reason)], in FGD order."""
    cases, skipped = [], []
    for (cn, key), label in _labels().items():
        if cn in SKIP or '%s.%s' % (cn, key) in SKIP:
            skipped.append((cn, key, SKIP.get('%s.%s' % (cn, key)) or SKIP[cn]))
        elif label not in SHARED_LABELS[key]:
            skipped.append((cn, key, 'the FGD gives this key another meaning here: "%s"' % label))
        elif key == 'master' and cn in MASTER_BODY:
            cases.append((cn, key))
        elif cn not in RECIPES:
            skipped.append((cn, key, 'no activation recipe in the harness'))
        else:
            cases.append((cn, key))
    return cases, skipped


# ---------------------------------------------------------------------------------------------- tests
def _lock(t, name, unlocked):
    """A multisource with one input (locked until that input is fired); unlocked: the input is fired now."""
    t.point('multisource', targetname=name, at=(200, -200, 0))
    t.point('trigger_relay', targetname=name + '_key', target=name, at=(180, -200, 0), triggerstate='1')
    if unlocked:
        t.fire(name + '_key')


def _info(t, name, at=(-200, -200, 0)):
    t.point('info_target', targetname=name, at=at)


def _make(suite, cn, key):
    recipe = RECIPES.get(cn, (None, None))
    place, act = recipe[:2]
    settle = recipe[2] if len(recipe) > 2 else 1.0
    doc = 'FGD %s, key %s' % (cn, key)
    xfail = XFAIL.get((cn, key))
    covers = ('%s.%s' % (cn, key),)

    if key == 'master' and cn in MASTER_BODY:
        title = '%s master: with a locked master it does nothing; with an unlocked one it works' % cn

        def body(t, _f=MASTER_BODY[cn]):
            _f(t)
    elif key == 'delay' and cn in DELAY_BODY:
        title = '%s delay 1: it acts 1 s after the activation, not before' % cn

        def body(t, _f=DELAY_BODY[cn]):
            _f(t)
    elif key == 'target':
        title = '%s target: activated, it fires its target' % cn

        def body(t):
            place(t, 'e', 0, {'target': 'out'})
            _info(t, 'out')
            t.wait(0.3)
            t.mark('go')
            act(t, 'e', 0)
            t.wait(settle)
            t.expect.used('out', after='go', min=1)
    elif key == 'delay' and settle > 1.0:
        title = '%s delay 1: its target fires 1 s later than the same setup with delay 0' % cn

        def body(t):
            place(t, 'e0', -120, {'target': 'out0', 'delay': '0'})
            place(t, 'e1', 120, {'target': 'out1', 'delay': '1'})
            _info(t, 'out0')
            _info(t, 'out1', at=(-160, -200, 0))
            t.wait(0.3)
            act(t, 'e0', -120)
            act(t, 'e1', 120)
            t.wait(settle + 1.5)
            t.expect.order('out0', 'out1', gap=(0.8, 1.4))
    elif key == 'delay':
        title = '%s delay 1: its target fires 1 s after the activation, not before' % cn

        def body(t):
            place(t, 'e', 0, {'target': 'out', 'delay': '1'})
            _info(t, 'out')
            t.wait(0.3)
            t.mark('go')
            act(t, 'e', 0)
            t.wait(2.5)
            t.expect.not_used('out', after='go', within=(0, 0.7))
            t.expect.used('out', after='go', within=(0.7, 2.5), min=1)
    elif key == 'killtarget':
        title = '%s killtarget: activated, it removes the named entity' % cn

        def body(t):
            place(t, 'e', 0, dict({'killtarget': 'victim'}, **({'target': 'out'} if cn in NEEDS_TARGET else {})))
            _info(t, 'victim')
            _info(t, 'out', at=(-160, -200, 0))
            t.wait(0.3)
            t.mark('go')
            act(t, 'e', 0)
            t.wait(settle)
            t.expect.removed('victim', after='go', count=1)
    elif key == 'master':
        title = '%s master: with a locked master it does not fire its target; with an unlocked one it does' % cn

        def body(t):
            _lock(t, 'lk', False)
            _lock(t, 'op', True)
            place(t, 'e_l', -120, {'target': 'out_l', 'master': 'lk'})
            place(t, 'e_o', 120, {'target': 'out_o', 'master': 'op'})
            _info(t, 'out_l')
            _info(t, 'out_o', at=(-160, -200, 0))
            t.wait(0.3)
            t.mark('go')
            act(t, 'e_l', -120)
            act(t, 'e_o', 120)
            t.wait(settle)
            t.expect.not_used('out_l', after='go')
            t.expect.used('out_o', after='go', min=1)
    else:   # triggerstate
        title = '%s triggerstate 0 / 1 / 2: the target gets USE_OFF / USE_ON / USE_TOGGLE' % cn
        covers = covers + tuple('%s.triggerstate=%d' % (cn, v) for v in (0, 1, 2))

        def body(t):
            for v, x in ((0, -120), (1, 0), (2, 120)):
                place(t, 'e%d' % v, x, {'target': 'out%d' % v, 'triggerstate': str(v)})
                _info(t, 'out%d' % v, at=(-200 + 40 * v, -200, 0))
            t.wait(0.3)
            t.mark('go')
            for v, x in ((0, -120), (1, 0), (2, 120)):
                act(t, 'e%d' % v, x)
            t.wait(1.0)
            for v, use in ((0, USE_OFF), (1, USE_ON), (2, USE_TOGGLE)):
                t.expect.used('out%d' % v, use=use, after='go', min=1)

    body.__name__ = '%s_%s' % (cn, key)
    size = MASTER_SIZE.get(cn, (512, 512, 256)) if key == 'master' else (512, 512, 256)
    suite.test(title, doc=doc, players=1, size=size, xfail=xfail, covers=covers)(body)


def generate(suite, keys):
    """Adds the generated tests for `keys` to `suite`."""
    cases, _ = plan()
    for cn, key in cases:
        if key in keys:
            _make(suite, cn, key)


if __name__ == '__main__':
    cases, skipped = plan()
    print('generated %d tests' % len(cases))
    for key in KEYS:
        print('  %-12s %s' % (key, ' '.join(cn for cn, k in cases if k == key)))
    print('left out %d (class, key):' % len(skipped))
    for cn, key, why in skipped:
        print('  %s.%s: %s' % (cn, key, why))
