"""Entity states (docs/STATE_LOGIC.md "Entity states"): each class's state in each of its states, read through the test
layer's snapshot field ent_state (CBaseEntity::GetState); multi_watcher's logical tests and flags, watcher_count's
comparisons and targets (sdk/syndicate.fgd text). Branch feat/entity-states."""

from etest import Suite

SUITE = Suite('entity_states', 'Entity states: every class, multi_watcher, watcher_count', players=1,
              requires=('GetStringForState',))
S = SUITE
SUITE.server_args = ('+developer', '1')      # the watchers' loop note is a console message
DOC = 'STATE_LOGIC.md#entity-states'
WDOC = 'STATE_LOGIC.md#multi_watcher'
CDOC = 'STATE_LOGIC.md#watcher_count'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3
ON, OFF, TURN_ON, TURN_OFF, IN_USE = 'On', 'Off', 'Turn On', 'Turn Off', 'In Use'


def info(t, name, at=(0, 0, 0)):
    return t.point('info_target', targetname=name, at=at)


def state(t, label, ent, expected):
    """snapshot `label` of `ent`'s state, expected to be `expected`"""
    t.snap(label, ent, 'ent_state')
    t.expect.value(label, ent, 'ent_state', expected)


# ------------------------------------------------------------------------------------------------ movers

@S.test('func_door "Toggle" (32): closed Off, opening Turn On, open On, closing Turn Off, closed Off again', players=1, doc=DOC,
        covers=('func_door', 'func_door:32'))
def door(t):
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='50', wait='-1', lip='8', spawnflags='32')
    t.wait(0.2)
    state(t, 'closed', 'door', OFF)
    t.fire('door')
    t.wait(0.4)
    state(t, 'opening', 'door', TURN_ON)
    t.wait(2.5)
    state(t, 'open', 'door', ON)
    t.fire('door')
    t.wait(0.4)
    state(t, 'closing', 'door', TURN_OFF)
    t.wait(2.5)
    state(t, 'closed2', 'door', OFF)


@S.test('func_door "Starts Open" (1): open On at the start, closing Turn Off, closed Off, opening Turn On', players=1, doc=DOC,
        covers=('func_door', 'func_door:1'))
def door_start_open(t):
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='50', wait='-1', lip='8', spawnflags='33')
    t.wait(0.2)
    state(t, 'open', 'door', ON)
    t.fire('door')
    t.wait(0.4)
    state(t, 'closing', 'door', TURN_OFF)
    t.wait(2.5)
    state(t, 'closed', 'door', OFF)
    t.fire('door')
    t.wait(0.4)
    state(t, 'opening', 'door', TURN_ON)


@S.test('func_door_rotating: closed Off, opening Turn On, open On', players=1, doc=DOC, covers=('func_door_rotating',))
def door_rotating(t):
    t.brush('func_door_rotating', size=(64, 8, 96), at=(32, 64, 0), tex='666MURMETAL', origin=True, targetname='rd',
            distance='90', speed='45', wait='-1')
    t.wait(0.2)
    state(t, 'closed', 'rd', OFF)
    t.fire('rd')
    t.wait(0.4)
    state(t, 'opening', 'rd', TURN_ON)
    t.wait(2.5)
    state(t, 'open', 'rd', ON)


@S.test('momentary_door: closed Off, moving open Turn On, open On, moving closed Turn Off, closed Off', players=1, doc=DOC,
        covers=('momentary_door',))
def momentary_door(t):
    t.brush('momentary_door', size=(96, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='md', angles='-90 0 0',
            lip='8')
    t.wait(0.2)
    state(t, 'closed', 'md', OFF)
    t.fire('md', use='set', value='1')
    state(t, 'opening', 'md', TURN_ON)      # the same frame: it moves for 0.1 s
    t.wait(0.5)
    state(t, 'open', 'md', ON)
    t.fire('md', use='set', value='0')
    state(t, 'closing', 'md', TURN_OFF)
    t.wait(0.5)
    state(t, 'closed2', 'md', OFF)


def button(t, classname='func_button', **kv):
    kv.setdefault('speed', '20')
    kv.setdefault('wait', '3')
    return t.brush(classname, size=(16, 32, 32), at=(0, 120, 40), tex='+0~ANZYG1', angles='0 90 0', targetname='btn',
                   **kv)


@S.test('func_button: out Off, going in Turn On, pressed On, coming back Turn Off, out Off', players=1, doc=DOC,
        covers=('func_button',))
def func_button(t):
    button(t)
    t.wait(0.2)
    state(t, 'out', 'btn', OFF)
    t.fire('btn')
    t.wait(0.3)
    state(t, 'in', 'btn', TURN_ON)
    t.wait(1.7)
    state(t, 'pressed', 'btn', ON)
    t.wait(2.6)
    state(t, 'back', 'btn', TURN_OFF)
    t.wait(1.9)
    state(t, 'out2', 'btn', OFF)


@S.test('func_rot_button: out Off, turning in Turn On, pressed On', players=1, doc=DOC, covers=('func_rot_button',))
def rot_button(t):
    t.brush('func_rot_button', size=(8, 32, 32), at=(0, 120, 40), origin=True, targetname='btn', distance='90',
            speed='45', wait='3')
    t.wait(0.2)
    state(t, 'out', 'btn', OFF)
    t.fire('btn')
    t.wait(0.4)
    state(t, 'in', 'btn', TURN_ON)
    t.wait(2.4)
    state(t, 'pressed', 'btn', ON)


@S.test('momentary_rot_button: at its start Off, held In Use, returning by itself (Auto Return) Turn Off', players=1, doc=DOC,
        covers=('momentary_rot_button',))
def momentary_return(t):
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='md',
            speed='90', distance='90', returnspeed='20', spawnflags='16')
    t.brush('momentary_door', size=(96, 16, 96), at=(-100, -150, 0), targetname='md', angles='0 0 0', lip='6')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    state(t, 'start', 'mb', OFF)
    t.console('test_move %s 0 0 -1 +use' % t.ref('player1'))
    t.wait(0.4)
    state(t, 'held', 'mb', IN_USE)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.wait(0.5)
    state(t, 'returning', 'mb', TURN_OFF)


@S.test('momentary_rot_button: let go away from its start (no Auto Return) On', players=1, doc=DOC,
        covers=('momentary_rot_button',))
def momentary_on(t):
    t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='md',
            speed='90', distance='90')
    t.brush('momentary_door', size=(96, 16, 96), at=(-100, -150, 0), targetname='md', angles='0 0 0', lip='6')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.walk('player1', 0, 0, 0.4, '+use')
    t.wait(0.5)
    state(t, 'turned', 'mb', ON)


@S.test('func_plat "Toggle" (1, named: starts at the top): top On, going down Turn Off, bottom Off, going up Turn On', players=1, doc=DOC,
        covers=('func_plat', 'func_plat:1'))
def plat(t):
    t.brush('func_plat', size=(64, 64, 16), at=(0, 100, 80), tex='666MURMETAL', targetname='plat', height='64',
            speed='64', spawnflags='1')
    t.wait(0.3)
    state(t, 'top', 'plat', ON)
    t.fire('plat')
    t.wait(0.4)
    state(t, 'down', 'plat', TURN_OFF)
    t.wait(1.5)
    state(t, 'bottom', 'plat', OFF)
    t.fire('plat')
    t.wait(0.4)
    state(t, 'up', 'plat', TURN_ON)


@S.test('func_train: standing Off, moving On, waiting at its path_corner Off', players=1, doc=DOC, covers=('func_train',))
def train(t):
    t.point('path_corner', targetname='p1', target='p2', at=(-150, 100, 40))
    t.point('path_corner', targetname='p2', target='p1', at=(150, 100, 40), wait='-1')
    t.brush('func_train', size=(32, 32, 32), at=(-150, 100, 40), targetname='tr', target='p1', speed='100')
    t.wait(0.5)
    state(t, 'standing', 'tr', OFF)
    t.fire('tr')
    t.wait(1.0)
    state(t, 'moving', 'tr', ON)
    t.wait(3.0)
    state(t, 'waiting', 'tr', OFF)


@S.test('func_tracktrain: standing Off, moving On, stopped Off', players=1, doc=DOC, covers=('func_tracktrain',))
def tracktrain(t):
    t.point('path_track', targetname='t1', target='t2', at=(-200, 100, 0))
    t.point('path_track', targetname='t2', at=(200, 100, 0))
    t.brush('func_tracktrain', targetname='tt', size=(32, 32, 16), at=(-200, 100, 0), target='t1', wheels='8',
            height='0', speed='100', startspeed='0')
    t.wait(0.5)
    state(t, 'standing', 'tt', OFF)
    t.fire('tt', use='on')
    t.wait(0.5)
    state(t, 'moving', 'tt', ON)
    t.fire('tt', use='off')
    t.wait(0.2)
    state(t, 'stopped', 'tt', OFF)


@S.test('func_rotating: standing Off, turning On; with Acc/Dcc (16) spinning up Turn On, spinning down Turn Off',
        players=1, doc=DOC, covers=('func_rotating', 'func_rotating:16'))
def rotating(t):
    t.brush('func_rotating', size=(32, 32, 32), at=(-80, 100, 32), origin=True, targetname='rot', speed='90')
    t.brush('func_rotating', size=(32, 32, 32), at=(80, 100, 32), origin=True, targetname='fan', speed='200',
            fanfriction='5', spawnflags='16')
    t.wait(0.2)
    state(t, 'still', 'rot', OFF)
    t.fire('rot')
    t.fire('fan')
    t.wait(0.3)
    state(t, 'turning', 'rot', ON)
    state(t, 'up', 'fan', TURN_ON)
    t.wait(2.5)
    state(t, 'full', 'fan', ON)
    t.fire('fan')
    t.wait(0.3)
    state(t, 'down', 'fan', TURN_OFF)


@S.test('func_pendulum: standing Off, swinging On; "Auto-return" (16) going back to its start Turn Off', players=1, doc=DOC,
        covers=('func_pendulum', 'func_pendulum:16'))
def pendulum(t):
    t.brush('func_pendulum', size=(8, 8, 64), at=(0, 100, 40), origin=True, targetname='pd', distance='60',
            speed='60', spawnflags='16')
    t.wait(0.2)
    state(t, 'still', 'pd', OFF)
    t.fire('pd')
    t.wait(0.5)
    state(t, 'swinging', 'pd', ON)
    t.fire('pd')
    state(t, 'returning', 'pd', TURN_OFF)


@S.test('func_wall: first texture Off, second texture (triggered) On', players=1, doc=DOC, covers=('func_wall',))
def wall(t):
    t.brush('func_wall', size=(64, 16, 64), at=(0, 100, 0), targetname='w')
    t.wait(0.2)
    state(t, 'a', 'w', OFF)
    t.fire('w')
    state(t, 'b', 'w', ON)


@S.test('func_wall_toggle and func_illusionary_toggle: there On, gone Off; "Starts Invisible" (1) Off', players=1, doc=DOC,
        covers=('func_wall_toggle', 'func_wall_toggle:1', 'func_illusionary_toggle', 'func_illusionary_toggle:1'))
def wall_toggle(t):
    t.brush('func_wall_toggle', size=(32, 16, 64), at=(-100, 100, 0), targetname='wt')
    t.brush('func_wall_toggle', size=(32, 16, 64), at=(-40, 100, 0), targetname='wt_off', spawnflags='1')
    t.brush('func_illusionary_toggle', size=(32, 16, 64), at=(40, 100, 0), targetname='it')
    t.brush('func_illusionary_toggle', size=(32, 16, 64), at=(100, 100, 0), targetname='it_off', spawnflags='1')
    t.wait(0.2)
    state(t, 'a', 'wt', ON)
    state(t, 'a', 'wt_off', OFF)
    state(t, 'a', 'it', ON)
    state(t, 'a', 'it_off', OFF)
    t.fire('wt')
    t.fire('it')
    state(t, 'b', 'wt', OFF)
    state(t, 'b', 'it', OFF)


@S.test('func_breakable: whole On, broken Off; func_pushable whole On', players=1, doc=DOC,
        covers=('func_breakable', 'func_pushable'))
def breakable(t):
    t.brush('func_breakable', size=(32, 32, 32), at=(-60, 100, 0), targetname='crate', health='30')
    t.brush('func_pushable', size=(32, 32, 32), at=(60, 100, 0), targetname='push', health='0')
    t.wait(0.2)
    state(t, 'whole', 'crate', ON)
    state(t, 'whole', 'push', ON)
    t.damage('crate', 100, 2, 'player1')
    state(t, 'broken', '@func_breakable', OFF)     # a broken breakable loses its name until it is removed


# ------------------------------------------------------------------------------------------------ lights, tanks, chargers

@S.test('light and light_spot: lit On, switched off Off; "Initially dark" (1) Off', players=1, doc=DOC,
        covers=('light', 'light:1', 'light_spot'))
def lights(t):
    t.point('light', targetname='l1', at=(-100, 0, 64))
    t.point('light', targetname='l2', at=(0, 0, 64), spawnflags='1')
    t.point('light_spot', targetname='l3', at=(100, 0, 64))
    t.wait(0.2)
    state(t, 'a', 'l1', ON)
    state(t, 'a', 'l2', OFF)
    state(t, 'a', 'l3', ON)
    t.fire('l1')
    t.fire('l2')
    state(t, 'b', 'l1', OFF)
    state(t, 'b', 'l2', ON)


def tank(t, **kv):
    t.brush('func_tank', size=(32, 32, 32), at=(0, 100, 40), origin=True, targetname='gun', yawrate='30',
            yawrange='180', pitchrate='30', pitchrange='30', barrel='16', firerate='2', bullet='1', **kv)


@S.test('func_tank: inactive Off, "Active" (1) On, controlled by a player In Use; func_tankcontrols in use On, '
        'else Off', players=1, doc=DOC, covers=('func_tank', 'func_tank:1', 'func_tankcontrols'))
def tanks(t):
    tank(t, spawnflags='32')
    t.brush('func_tank', size=(32, 32, 32), at=(150, 100, 40), origin=True, targetname='active', yawrate='30',
            yawrange='180', pitchrate='30', pitchrange='30', barrel='16', firerate='2', bullet='1', spawnflags='33')
    t.brush('func_tankcontrols', size=(64, 64, 96), at=(0, 0, 0), target='gun', targetname='tc')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(1.0)
    state(t, 'a', 'gun', OFF)
    state(t, 'a', 'active', ON)
    state(t, 'a', 'tc', OFF)
    t.fire('tc', activator='player1')
    t.wait(0.3)
    state(t, 'b', 'gun', IN_USE)
    state(t, 'b', 'tc', ON)
    t.fire('tc', activator='player1', use='off')


def charger(t, classname, **kv):
    t.brush(classname, targetname='chg', size=(64, 8, 48), at=(0, 60, 24), **kv)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)


@S.test('func_recharge: charge left On, charging a player In Use, empty Off', players=1, doc=DOC, covers=('func_recharge',))
def recharge(t):
    charger(t, 'func_recharge', CustomJuice='10')
    t.kv('player1', 'armorvalue', '0')
    t.wait(0.2)
    state(t, 'full', 'chg', ON)
    t.console('test_move %s 0 0 -1 +use' % t.ref('player1'))
    t.wait(0.3)
    state(t, 'using', 'chg', IN_USE)
    t.wait(2.0)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.wait(0.3)
    state(t, 'empty', 'chg', OFF)


# ------------------------------------------------------------------------------------------------ triggers and logic

@S.test('trigger_multiple: ready On, waiting after it fired Off, ready again On; trigger_hurt "Start Off" (2) Off, '
        'switched on On', players=1, doc=DOC, covers=('trigger_multiple', 'trigger_hurt', 'trigger_hurt:2'))
def triggers(t):
    t.trigger('trigger_multiple', at=(0, 150, 0), targetname='tm', target='hit', wait='1.5')
    info(t, 'hit')
    t.trigger('trigger_hurt', at=(150, 150, 0), targetname='th', spawnflags='2', dmg='0')
    t.wait(0.2)
    state(t, 'ready', 'tm', ON)
    state(t, 'off', 'th', OFF)
    t.teleport('player1', (0, 150, 0))
    t.wait(0.3)
    t.teleport('player1', (0, -150, 0))
    state(t, 'waiting', 'tm', OFF)
    t.wait(1.8)
    state(t, 'again', 'tm', ON)
    t.fire('th')
    state(t, 'on', 'th', ON)
    t.expect.fired('hit', count=1)


@S.test('trigger_inout: empty Off, a player inside On, empty again Off', players=1, doc=DOC, covers=('trigger_inout',))
def inout(t):
    t.trigger('trigger_inout', at=(0, 150, 0), targetname='io')
    t.wait(0.2)
    state(t, 'empty', 'io', OFF)
    t.teleport('player1', (0, 150, 0))
    t.wait(0.4)
    state(t, 'inside', 'io', ON)
    t.teleport('player1', (0, -150, 0))
    t.wait(0.4)
    state(t, 'left', 'io', OFF)


@S.test('multi_manager: waiting Off, running its targets On, done Off; "multithreaded" (1) On while a copy runs',
        players=1, doc=DOC, covers=('multi_manager', 'multi_manager:1'))
def multi_manager(t):
    t.point('multi_manager', targetname='mm', a='0', b='1')
    t.point('multi_manager', targetname='mt', c='0', d='1', spawnflags='1')
    for n in 'abcd':
        info(t, n)
    t.wait(0.2)
    state(t, 'idle', 'mm', OFF)
    state(t, 'idle', 'mt', OFF)
    t.fire('mm')
    t.fire('mt')
    t.wait(0.4)
    state(t, 'running', 'mm', ON)
    state(t, 'running', 'mt', ON)
    t.wait(1.0)
    state(t, 'done', 'mm', OFF)
    state(t, 'done', 'mt', OFF)


@S.test('env_state: Off, turning on (Time taken to turn on) Turn On, On, turning off Turn Off', players=1, doc=DOC,
        covers=('env_state',))
def env_state(t):
    t.point('env_state', targetname='es', turnontime='1', turnofftime='1')
    t.wait(0.2)
    state(t, 'off', 'es', OFF)
    t.fire('es')
    t.wait(0.3)
    state(t, 'turning_on', 'es', TURN_ON)
    t.wait(1.0)
    state(t, 'on', 'es', ON)
    t.fire('es')
    t.wait(0.3)
    state(t, 'turning_off', 'es', TURN_OFF)


@S.test('Masters read as their master answer: multisource with its input on On, else Off; other entities Off',
        players=1, doc=DOC, covers=('multisource', 'info_target'))
def masters(t):
    t.point('multisource', targetname='ms')
    t.point('trigger_relay', targetname='in', target='ms', triggerstate='1')
    info(t, 'plain')
    t.wait(0.3)
    state(t, 'a', 'ms', OFF)
    state(t, 'a', 'plain', OFF)
    t.fire('in')
    t.wait(0.1)
    state(t, 'b', 'ms', ON)


# ------------------------------------------------------------------------------------------------ monsters, players

@S.test('monsters: alive On, dying Turn Off, dead Off; a player in play On, dead Off', players=1, doc=DOC,
        covers=('monster_scientist',))
def monsters(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sci', at=(0, 150, 0), angles='0 270 0', spawnflags='16')
    t.wait(0.5)
    state(t, 'alive', 'sci', ON)
    state(t, 'alive', 'player1', ON)
    t.kv('sci', 'health', '10')
    t.damage('sci', 20, 2, 'player1')
    t.wait(0.3)                                 # the death animation starts with its next think
    state(t, 'dying', 'sci', TURN_OFF)
    t.wait(2.5)
    state(t, 'dead', 'sci', OFF)
    t.damage('player1', 1000, 2)
    state(t, 'dead', 'player1', OFF)


@S.test('monster_target: active On, "Start inactive" (1) Off, toggled On', players=1, doc=DOC,
        covers=('monster_target', 'monster_target:1'))
def monster_target(t):
    t.point('monster_target', targetname='mt_on', at=(-60, 100, 0))
    t.point('monster_target', targetname='mt_off', at=(60, 100, 0), spawnflags='1')
    t.wait(0.2)
    state(t, 'a', 'mt_on', ON)
    state(t, 'a', 'mt_off', OFF)
    t.fire('mt_off')
    state(t, 'b', 'mt_off', ON)


@S.test('func_guntarget: Off until triggered, On from its start (also waiting at a corner); Off stops it, Toggle stops '
        'a moving one and starts a stopped one, On starts it again; shot down Off', players=1, doc=DOC,
        covers=('func_guntarget',))
def guntarget(t):
    t.point('path_corner', targetname='g1', at=(0, 150, 40), target='g2')
    t.point('path_corner', targetname='g2', at=(0, -150, 40), target='g1')
    t.brush('func_guntarget', targetname='gt', size=(32, 8, 32), at=(0, 150, 40), origin=True, target='g1',
            speed='50', health='100')     # 50 u/s: all its moving here stays on the first leg
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.3)
    state(t, 'still', 'gt', OFF)
    t.fire('gt', use='on')
    t.wait(0.5)
    t.snap('on_a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('on_b', 'gt', 'origin')
    state(t, 'moving', 'gt', ON)
    t.fire('gt', use='off')                     # Off stops it where it is
    t.wait(0.2)
    t.snap('off_a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('off_b', 'gt', 'origin')
    state(t, 'stopped', 'gt', OFF)
    t.fire('gt', use='toggle')                  # Toggle starts a stopped one
    t.wait(0.5)
    t.snap('tog_a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('tog_b', 'gt', 'origin')
    state(t, 'toggled_on', 'gt', ON)
    t.fire('gt', use='toggle')                  # and stops a moving one
    t.wait(0.2)
    t.snap('tog_c', 'gt', 'origin')
    t.wait(0.5)
    t.snap('tog_d', 'gt', 'origin')
    state(t, 'toggled_off', 'gt', OFF)
    t.fire('gt', use='on')                      # On starts it again
    t.wait(0.5)
    t.snap('re_a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('re_b', 'gt', 'origin')
    state(t, 'restarted', 'gt', ON)
    t.fire('gt', use='on')                      # On while on: nothing changes, it keeps moving
    t.wait(0.5)
    t.snap('re_c', 'gt', 'origin')
    state(t, 'still_on', 'gt', ON)
    t.damage('gt', 200, 2, 'player1')           # shot down: it stops, Off
    t.wait(0.2)
    t.snap('dead_a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('dead_b', 'gt', 'origin')
    state(t, 'shot_down', 'gt', OFF)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.moved('on_a', 'on_b', 'gt', min_dist=15)
    t.expect.moved('off_a', 'off_b', 'gt', delta=(0, 0, 0), tol=1)
    t.expect.moved('tog_a', 'tog_b', 'gt', min_dist=15)
    t.expect.moved('tog_c', 'tog_d', 'gt', delta=(0, 0, 0), tol=1)
    t.expect.moved('re_a', 're_b', 'gt', min_dist=15)
    t.expect.moved('re_b', 're_c', 'gt', min_dist=15)
    t.expect.moved('dead_a', 'dead_b', 'gt', delta=(0, 0, 0), tol=1)


@S.test('scripted_sequence: idle Off, its monster walking to it Turn On; one whose monster stands on it plays its '
        'action On', players=1, doc=DOC, covers=('scripted_sequence',))
def scripted(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_cyborg', targetname='npc', at=(-150, 150, 0), angles='0 0 0', spawnflags='16')
    t.point('scripted_sequence', targetname='ss', at=(100, 150, 0), m_iszEntity='npc', m_fMoveTo='1',
            m_iszPlay='idle1', names=('m_iszEntity',))
    t.point('monster_cyborg', targetname='npc2', at=(0, -150, 0), angles='0 0 0', spawnflags='16')
    t.point('scripted_sequence', targetname='ss2', at=(0, -150, 0), m_iszEntity='npc2', m_fMoveTo='0',
            m_iszPlay='idle1', names=('m_iszEntity',))
    t.wait(0.5)
    state(t, 'idle', 'ss', OFF)
    t.fire('ss')
    t.fire('ss2')
    t.wait(0.5)
    state(t, 'walking', 'ss', TURN_ON)
    state(t, 'playing', 'ss2', ON)


@S.test('scripted_sentence: waiting Off, its speaker saying the sentence On, after it Off', players=1, doc=DOC,
        covers=('scripted_sentence',))
def sentence(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sa', at=(0, 150, 0), angles='0 270 0', spawnflags='16')
    t.point('scripted_sentence', targetname='say', entity='sa', sentence='!SC_HELLO0', duration='2', refire='1',
            names=('entity',))
    t.wait(0.5)
    state(t, 'idle', 'say', OFF)
    t.fire('say')
    t.wait(0.5)
    state(t, 'speaking', 'say', ON)
    t.wait(2.0)
    state(t, 'after', 'say', OFF)


# ------------------------------------------------------------------------------------------------ effects

@S.test('effects: env_beam, env_sprite, env_bubbles, env_spark, env_model, env_beamtrail on On and switched off Off',
        players=1, doc=DOC, covers=('env_beam', 'env_sprite', 'env_bubbles', 'env_spark', 'env_model', 'env_beamtrail'))
def effects(t):
    info(t, 'b_a', at=(-150, 100, 20))
    info(t, 'b_b', at=(-150, 100, 120))
    t.point('env_beam', targetname='beam', LightningStart='b_a', LightningEnd='b_b', texture='sprites/laserbeam.spr',
            renderamt='255', life='0', BoltWidth='10', spawnflags='1', names=('LightningStart', 'LightningEnd'))
    t.point('env_sprite', targetname='spr', at=(-90, 100, 40), model='sprites/glow01.spr', spawnflags='1')
    t.brush('env_bubbles', size=(32, 32, 32), at=(-30, 100, 0), targetname='bub', density='2', frequency='2')
    t.point('env_spark', targetname='spk', at=(30, 100, 40), MaxDelay='1', spawnflags='96')
    t.point('env_model', targetname='mdl', at=(90, 100, 0), model='models/can.mdl')
    t.point('env_beamtrail', targetname='bt', at=(150, 100, 40))
    t.wait(0.5)
    ents = ('beam', 'spr', 'bub', 'spk', 'mdl', 'bt')
    for e in ents:
        state(t, 'on', e, ON)
    for e in ents:
        t.fire(e, use='off')
    t.wait(0.1)
    for e in ents:
        state(t, 'off', e, OFF)


@S.test('env_beverage: cans left On, empty Off; env_dlight on On, off Off', players=1, doc=DOC,
        covers=('env_beverage', 'env_dlight'))
def beverage_dlight(t):
    t.point('env_beverage', targetname='bev', at=(-60, 100, 40), health='1')
    t.point('env_dlight', targetname='dl', at=(60, 100, 40), spawnflags='2', renderamt='200')
    t.wait(0.2)
    state(t, 'a', 'bev', ON)
    state(t, 'a', 'dl', ON)
    t.fire('bev')
    t.fire('dl', use='off')
    state(t, 'b', 'bev', OFF)
    state(t, 'b', 'dl', OFF)


# ------------------------------------------------------------------------------------------------ multi_watcher

LOGIC = {0: ('AND', lambda v: all(v)), 1: ('OR', lambda v: any(v)), 2: ('NAND', lambda v: not all(v)),
         3: ('NOR', lambda v: not any(v)), 4: ('XOR', lambda v: sum(v) == 1), 5: ('XNOR', lambda v: sum(v) != 1)}
COMBOS = ((False, False), (True, False), (True, True))


def logic_case(op):
    label, fn = LOGIC[op]

    @S.test('multi_watcher Logical test %d (%s) of two env_states: off/off, on/off, on/on' % (op, label), players=1, doc=WDOC,
            covers=('multi_watcher', 'multi_watcher.m_fLogic', 'multi_watcher.m_fLogic=%d' % op))
    def case(t):
        t.point('env_state', targetname='on', spawnflags='1')
        t.point('env_state', targetname='off')
        for i, (a, b) in enumerate(COMBOS):
            kv = {('on' if a else 'off'): '0', ('on#2' if b else 'off#2'): '0'}
            t.point('multi_watcher', targetname='w%d' % i, m_fLogic=str(op), **kv)
        t.wait(0.2)
        for i, combo in enumerate(COMBOS):
            state(t, 's', 'w%d' % i, ON if fn(combo) else OFF)
    case.__name__ = 'watcher_logic%d' % op
    return case


for _op in sorted(LOGIC):
    logic_case(_op)


@S.test('multi_watcher Entity to notify: fired with On when the test turns true and Off when it turns false',
        players=1, doc=WDOC, covers=('multi_watcher.target', 'multi_watcher.m_iszWatch'))
def watcher_notify(t):
    t.point('env_state', targetname='es')
    t.point('multi_watcher', targetname='w', m_iszWatch='es', target='out')
    info(t, 'out')
    t.wait(1.0)
    t.mark('on')
    t.fire('es', use='on')
    t.wait(0.5)
    t.mark('off')
    t.fire('es', use='off')
    t.wait(0.5)
    t.expect.fired('out', use=USE_ON, count=1, caller='w', after='on')
    t.expect.fired('out', use=USE_OFF, count=1, caller='w', after='off')
    t.expect.fired('out', use=USE_OFF, count=0, min=0, within=(0, 0.9))   # it starts Off: nothing at the first check


def flag_notify_case(flag, name, expect_on, expect_off):
    @S.test('multi_watcher flag %d %s: on %s, off %s' % (flag, name, expect_on, expect_off), players=1, doc=WDOC,
            covers=('multi_watcher:%d' % flag,))
    def case(t):
        t.point('env_state', targetname='es')
        t.point('multi_watcher', targetname='w', es='0', target='out', spawnflags=str(flag))
        info(t, 'out')
        t.wait(1.0)
        t.mark('on')
        t.fire('es', use='on')
        t.wait(0.5)
        t.mark('off')
        t.fire('es', use='off')
        t.wait(0.5)
        for mark, exp in (('on', expect_on), ('off', expect_off)):
            if exp is None:
                t.expect.not_fired('out', after=mark, within=(0, 0.45))
            else:
                t.expect.fired('out', use=exp, count=1, after=mark, within=(0, 0.45))
    case.__name__ = 'watcher_flag%d' % flag
    return case


flag_notify_case(1, "Send 'Toggle'", USE_TOGGLE, USE_TOGGLE)
flag_notify_case(2, "Don't send On", None, USE_OFF)
flag_notify_case(4, "Don't send Off", USE_ON, None)


@S.test("multi_watcher state flags: NOT 'On' (8), 'Off' (16), 'Turn On' (32), 'Turn Off' (64) pick the states that "
        'count as true', players=1, doc=WDOC,
        covers=('multi_watcher:8', 'multi_watcher:16', 'multi_watcher:32', 'multi_watcher:64'))
def watcher_state_flags(t):
    t.point('env_state', targetname='on', spawnflags='1')
    t.point('env_state', targetname='off')
    t.point('env_state', targetname='slow', turnontime='5', turnofftime='5')
    t.point('env_state', targetname='slow_off', spawnflags='1', turnofftime='5')
    t.point('multi_watcher', targetname='w_noton', on='0', spawnflags='8')         # On no longer counts
    t.point('multi_watcher', targetname='w_off', off='0', spawnflags='24')         # Off counts
    t.point('multi_watcher', targetname='w_ton', slow='0', spawnflags='40')        # Turn On counts
    t.point('multi_watcher', targetname='w_toff', slow_off='0', spawnflags='72')   # Turn Off counts
    t.point('multi_watcher', targetname='w_plain', slow='0')                       # Turn On doesn't count
    t.wait(0.2)
    t.fire('slow')
    t.fire('slow_off')
    t.wait(0.3)
    state(t, 's', 'w_noton', OFF)
    state(t, 's', 'w_off', ON)
    state(t, 's', 'w_ton', ON)
    state(t, 's', 'w_toff', ON)
    state(t, 's', 'w_plain', OFF)


@S.test("multi_watcher 'In Use' (128): a charger a player uses counts as true", players=1, doc=WDOC,
        covers=('multi_watcher:128',))
def watcher_in_use(t):
    charger(t, 'func_recharge')
    t.kv('player1', 'armorvalue', '0')
    t.point('multi_watcher', targetname='w', chg='0', spawnflags='136')
    t.wait(0.2)
    state(t, 'idle', 'w', OFF)
    t.console('test_move %s 0 0 -1 +use' % t.ref('player1'))
    t.wait(0.3)
    state(t, 'used', 'w', ON)
    t.console('test_move %s 0 0 0' % t.ref('player1'))


@S.test('multi_watcher as a master: a door with it as Master opens only while the watched door is open; a missing '
        'entity is not tested', players=1, doc=WDOC, covers=('multi_watcher',))
def watcher_master(t):
    t.brush('func_door', size=(64, 16, 96), at=(-100, 64, 0), tex='666MURMETAL', targetname='d1', angles='-90 0 0',
            speed='400', wait='-1', lip='8')
    t.brush('func_door', size=(64, 16, 96), at=(100, 64, 0), tex='666MURMETAL', targetname='d2', angles='-90 0 0',
            speed='400', wait='-1', lip='8', master='w')
    t.point('multi_watcher', targetname='w', d1='0', missing='0')
    t.wait(0.2)
    t.snap('a', 'd2', 'origin')
    t.fire('d2')
    t.wait(0.5)
    t.snap('b', 'd2', 'origin')
    t.expect.same('a', 'b', 'd2', 'origin')         # locked: d1 is closed
    t.fire('d1')
    t.wait(0.5)
    t.fire('d2')
    t.wait(0.5)
    t.snap('c', 'd2', 'origin')
    t.expect.value('c', 'd2', 'origin.z', 20, op='>')


@S.test('multi_watcher watching itself: the check stops in a loop with a console note, no crash', players=1, doc=WDOC,
        covers=('multi_watcher',))
def watcher_loop(t):
    t.point('multi_watcher', targetname='w', w='0', target='out')
    info(t, 'out')
    t.wait(1.0)
    t.expect.console('refer to each other in a loop')


# ------------------------------------------------------------------------------------------------ watcher_count

MODES = {0: ('=', lambda c, n: c == n), 1: ('>', lambda c, n: c > n), 2: ('<', lambda c, n: c < n),
         3: ('!=', lambda c, n: c != n), 4: ('<=', lambda c, n: c <= n), 5: ('>=', lambda c, n: c >= n)}


def mode_case(mode):
    sym, fn = MODES[mode]

    @S.test('watcher_count Type of comparison %d (count %s 1) with 0, 1 and 2 of its entities on' % (mode, sym),
            players=1, doc=CDOC, covers=('watcher_count', 'watcher_count.noise', 'watcher_count.impulse',
                              'watcher_count.m_iMode', 'watcher_count.m_iMode=%d' % mode))
    def case(t):
        for i in range(3):
            # three groups: group i has i of its two env_states on
            t.point('env_state', targetname='g%d' % i, spawnflags='1' if i >= 1 else '0')
            t.point('env_state', targetname='g%d' % i, spawnflags='1' if i >= 2 else '0')
            t.point('watcher_count', targetname='c%d' % i, noise='g%d' % i, impulse='1', m_iMode=str(mode))
        t.wait(0.7)
        for i in range(3):
            state(t, 's', 'c%d' % i, ON if fn(i, 1) else OFF)
    case.__name__ = 'count_mode%d' % mode
    return case


for _m in sorted(MODES):
    mode_case(_m)


@S.test('watcher_count targets: count increases / decreases fire noise1 / noise2; turning on / off fires '
        '"Fire on turning on" / "off" and "Fire on changing state" with On / Off; dead monsters do not count',
        players=1, doc=CDOC, covers=('watcher_count.target', 'watcher_count.netname', 'watcher_count.message',
                          'watcher_count.noise1', 'watcher_count.noise2'))
def count_targets(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sci', at=(-60, 150, 0), spawnflags='16')
    t.point('monster_scientist', targetname='sci', at=(60, 150, 0), spawnflags='16')
    t.point('watcher_count', targetname='c', noise='sci', impulse='2', m_iMode='0', target='chg', netname='ton',
            message='toff', noise1='up', noise2='down', names=('noise',))
    for n in ('chg', 'ton', 'toff', 'up', 'down'):
        info(t, n)
    t.wait(1.0)
    state(t, 'two', 'c', ON)
    t.mark('kill')
    t.damage('sci', 500, 2, 'player1')
    t.wait(5.0)
    state(t, 'none', 'c', OFF)
    t.expect.fired('down', min=1, after='kill')
    t.expect.fired('toff', count=1, after='kill')
    t.expect.fired('chg', use=USE_OFF, count=1, after='kill')
    t.expect.not_fired('up', after='kill')
    t.expect.not_fired('ton', after='kill')


@S.test('watcher_count count rising: noise1 fires, and turning on fires netname and target with On', players=1, doc=CDOC,
        covers=('watcher_count.noise1', 'watcher_count.netname', 'watcher_count.target'))
def count_rise(t):
    t.point('env_state', targetname='es')
    t.point('watcher_count', targetname='c', noise='es', impulse='1', m_iMode='5', target='chg', netname='ton',
            noise1='up', names=('noise',))
    for n in ('chg', 'ton', 'up'):
        info(t, n)
    t.wait(1.0)
    t.mark('go')
    t.fire('es', use='on')
    t.wait(0.5)
    t.expect.fired('up', count=1, after='go')
    t.expect.fired('ton', count=1, after='go')
    t.expect.fired('chg', use=USE_ON, count=1, after='go')


def start_case(on):
    @S.test('watcher_count "Fire at startup" (1): fires "Fire on turning %s" once when the comparison is %s at the '
            'start' % ('on' if on else 'off', 'true' if on else 'false'), players=1, doc=CDOC,
            covers=('watcher_count:1', 'watcher_count.netname', 'watcher_count.message'))
    def case(t):
        t.point('env_state', targetname='es', spawnflags='1' if on else '0')
        info(t, 'ton')
        info(t, 'toff')
        t.wait(0.2)
        # created now, so its first check (0.5 s after it spawns) falls inside the test
        t.console('test_create watcher_count 0 0 0 0 noise %s impulse 1 m_iMode 0 netname %s message %s spawnflags 1'
                  % (t.name('es'), t.name('ton'), t.name('toff')))
        t.wait(1.0)
        t.expect.fired('ton' if on else 'toff', count=1)
        t.expect.not_fired('toff' if on else 'ton')
    case.__name__ = 'count_start_%s' % ('on' if on else 'off')
    return case


start_case(True)
start_case(False)


@S.test('watcher_count as a master: a door with it as Master opens while the comparison is true', players=1, doc=CDOC,
        covers=('watcher_count',))
def count_master(t):
    t.point('env_state', targetname='es')
    t.point('watcher_count', targetname='c', noise='es', impulse='1', m_iMode='0', names=('noise',))
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='d', angles='-90 0 0',
            speed='400', wait='-1', lip='8', master='c')
    t.wait(0.7)
    t.snap('a', 'd', 'origin')
    t.fire('d')
    t.wait(0.5)
    t.snap('b', 'd', 'origin')
    t.expect.same('a', 'b', 'd', 'origin')
    t.fire('es', use='on')
    t.fire('d')
    t.wait(0.5)
    t.snap('c', 'd', 'origin')
    t.expect.value('c', 'd', 'origin.z', 20, op='>')


# ------------------------------------------------------------------------------------------------ train_setspeed
SDOC = 'TRAIN_SETSPEED.md'


def track(t, speed='100'):
    t.point('path_track', targetname='k1', target='k2', at=(-220, 100, 0))
    t.point('path_track', targetname='k2', at=(220, 100, 0))
    t.brush('func_tracktrain', targetname='tt', size=(32, 32, 16), at=(-220, 100, 0), target='k1', wheels='8',
            height='0', speed=speed, startspeed='0')


@S.test('train_setspeed "At once" (mode 0): a moving train runs at the New speed at once, and the target fires',
        players=1, doc=SDOC, covers=('train_setspeed', 'train_setspeed.train', 'train_setspeed.speed',
                                     'train_setspeed.mode', 'train_setspeed.mode=0', 'train_setspeed.target'))
def setspeed_instant(t):
    track(t, speed='40')
    t.point('train_setspeed', targetname='ss', train='tt', speed='200', mode='0', target='done')
    info(t, 'done')
    t.wait(0.5)
    t.fire('tt', use='on')
    t.wait(0.3)
    t.mark('go')
    t.fire('ss')
    t.wait(0.3)
    t.snap('s', 'tt', 'speed')
    t.expect.value('s', 'tt', 'speed', 200)
    t.expect.fired('done', count=1, after='go', within=(0, 0.1))


@S.test('train_setspeed "Step by step" (mode 1) up: 50 to 150 over 1 s, about halfway after 0.5 s, then 150 and the '
        'target fires', players=1, doc=SDOC,
        covers=('train_setspeed.mode=1', 'train_setspeed.time', 'train_setspeed.target'))
def setspeed_ramp_up(t):
    track(t, speed='50')
    t.point('train_setspeed', targetname='ss', train='tt', speed='150', mode='1', time='1', target='done')
    info(t, 'done')
    t.wait(0.5)
    t.fire('tt', use='on')
    t.wait(0.2)
    t.mark('go')
    t.fire('ss')
    t.wait(0.55)
    t.snap('mid', 'tt', 'speed')
    t.wait(0.8)
    t.snap('end', 'tt', 'speed')
    t.expect.value('mid', 'tt', 'speed', 100, tol=25)
    t.expect.value('end', 'tt', 'speed', 150)
    t.expect.fired('done', count=1, after='go', within=(0.8, 1.3))


@S.test('train_setspeed "Step by step" down to 0 stops the train (its sound too)', players=1, doc=SDOC,
        covers=('train_setspeed.mode=1', 'train_setspeed.speed'))
def setspeed_ramp_down(t):
    track(t, speed='100')
    t.point('train_setspeed', targetname='ss', train='tt', speed='0', mode='1', time='0.5')
    t.wait(0.5)
    t.fire('tt', use='on')
    t.wait(0.2)
    t.fire('ss')
    t.wait(0.25)
    t.snap('mid', 'tt', 'speed')
    t.wait(0.6)
    t.snap('end', 'tt', 'speed', 'velocity')
    t.expect.value('mid', 'tt', 'speed', 0, op='>')
    t.expect.value('mid', 'tt', 'speed', 100, op='<')
    t.expect.value('end', 'tt', 'speed', 0)
    t.expect.value('end', 'tt', 'velocity', [0, 0, 0])


@S.test('train_setspeed on a stopped train: nothing changes, the target does not fire', players=1, doc=SDOC,
        covers=('train_setspeed',))
def setspeed_stopped(t):
    track(t)
    t.point('train_setspeed', targetname='ss', train='tt', speed='300', mode='0', target='done')
    info(t, 'done')
    t.wait(0.5)
    t.fire('ss')
    t.wait(0.3)
    t.snap('s', 'tt', 'speed')
    t.expect.value('s', 'tt', 'speed', 0)
    t.expect.not_fired('done')


@S.test('train_setspeed with no func_tracktrain of that name: a console note, nothing fires', players=1, doc=SDOC,
        covers=('train_setspeed',))
def setspeed_missing(t):
    t.point('train_setspeed', targetname='ss', train='nothere', speed='300', target='done')
    info(t, 'done')
    t.wait(0.2)
    t.fire('ss')
    t.wait(0.2)
    t.expect.not_fired('done')
    t.expect.console('no func_tracktrain named "%s"' % t.name('nothere'))
