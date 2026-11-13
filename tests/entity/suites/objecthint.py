"""Object hints (docs/OBJECT_HINTS.md; Featureful): an entity's "objecthint" key names a template; every 0.1 s the
server sends each living player an "ObjectHint" message per hint he sees (unreliable). The interaction hint
(flags 1) marks what +use would use now; scan hints (flags 0) mark hinted objects (every template but hidden) within
the hint distance: the "objecthint_distance" key, 512 units when it is empty or 0. Spectators get none.
Its own suite: the last test leaves player2 in spectator mode."""

import math

from etest import Suite

SUITE = Suite('objecthint', 'Object hints: templates, states, spectators', players=2,
              requires=('ObjectHint_PlayerPostThink',))
S = SUITE
DOC = 'OBJECT_HINTS.md'
SPRITE = 'sprites/hud_brackets.spr'
INTERACTABLE = {'flags': 1, 'r': 255, 'g': 200, 'b': 50, 'sprite': SPRITE}
LOCK = {'flags': 1, 'r': 255, 'g': 50, 'b': 0, 'sprite': SPRITE}
SCAN = {'flags': 0, 'r': 0, 'g': 0, 'b': 0, 'sprite': SPRITE}
EYE = 64        # a standing player's eyes above his feet (origin 36 + view_ofs 28)


def look_at(t, player, stand, point):
    """player stands at `stand` (room floor coordinates) and looks at `point`."""
    dx, dy, dz = point[0] - stand[0], point[1] - stand[1], point[2] - EYE
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = -math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    t.teleport(player, stand, yaw=round(yaw, 1))
    t.look(player, round(pitch, 1), round(yaw, 1))


def scan_near_far(t, near, far, target):
    """Scan hints (flags 0) while player1 stands at `near`, none while he stands at `far`; both look at `target`."""
    look_at(t, 'player1', near, target)
    t.wait(0.5)
    t.mark('near')
    t.wait(0.5)
    look_at(t, 'player1', far, target)
    t.wait(0.3)
    t.mark('far')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', min=3, fields=SCAN, after='near', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'flags': 0}, after='far', within=(0, 0.5))


def face_button(t, **kv):
    """A wall button 40 units in front of player1, who looks at it."""
    t.brush('func_button', size=(32, 8, 32), at=(0, 16, 48), spawnflags='1', wait='1', **kv)
    t.teleport('player1', (0, -24, 0), yaw=90)
    t.look('player1', 0, 90)


@S.test('func_button objecthint "button" in front of the player: the interaction hint, yellow, about 10 per second',
        players=1, doc=DOC, covers=('func_button', 'func_button.objecthint=button'))
def button_hint(t):
    face_button(t, objecthint='button', targetname='b')
    t.wait(0.5)
    t.mark('m')
    t.wait(1.0)
    t.expect.msg('ObjectHint', to='player1', min=6, max=13, fields=INTERACTABLE, after='m', within=(0, 1.0))


@S.test('func_button "button" behind an off multisource: orange (locked); a relay turns the master on: yellow',
        players=1, doc=DOC, covers=('func_button', 'func_button.objecthint=button', 'func_button.master'))
def locked_button(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='unlock', target='lock', triggerstate='1')
    face_button(t, objecthint='button', master='lock')
    t.wait(0.5)
    t.mark('locked')
    t.wait(0.5)
    t.fire('unlock')
    t.wait(0.2)
    t.mark('open')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', min=3, fields=LOCK, after='locked', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', count=0, fields=LOCK, after='open', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', min=3, fields=INTERACTABLE, after='open', within=(0, 0.5))


@S.test('func_button without the key: no hint, only "no interaction hint" messages (flags 1, entity 0)',
        players=1, doc=DOC, covers=('func_button',))
def no_key(t):
    face_button(t)
    t.wait(0.5)
    t.mark('m')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'r': 255}, after='m', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', min=3, fields={'flags': 1, 'ent': 0}, after='m', within=(0, 0.5))


@S.test('item_battery "pickup" without objecthint_distance: a scan hint (flags 0) in the HUD color from about 450 '
        'units; none from about 615 (the default distance is 512)', players=1, doc=DOC,
        covers=('item_battery', 'item_battery.objecthint=pickup'))
def pickup_scan_default(t):
    # battery box (204..236, 204..236, 0..16); eyes 64 above the floor: (-110, -110) is 447 away, (-230, -230) 616
    t.point('item_battery', at=(220, 220, 0), objecthint='pickup')
    scan_near_far(t, (-110, -110, 0), (-230, -230, 0), (220, 220, 8))


@S.test('func_button "button" about 200 units ahead: a scan hint (flags 0) in the HUD color; none from about 585',
        players=1, doc=DOC, covers=('func_button', 'func_button.objecthint=button'))
def button_scan(t):
    # button box (184..216, 184..216, 32..64): (50, 50) is 189 away, (-230, -230) 585
    t.brush('func_button', size=(32, 32, 32), at=(200, 200, 32), spawnflags='1', wait='1', objecthint='button')
    scan_near_far(t, (50, 50, 0), (-230, -230, 0), (200, 200, 48))


@S.test('func_button "pickup" with objecthint_distance 1000 near the ceiling: a scan hint from about 920 units; none '
        'from about 1090', players=1, size=(512, 512, 1024), doc=DOC,
        covers=('func_button', 'func_button.objecthint=pickup', 'func_button.objecthint_distance'))
def custom_distance(t):
    # button box (204..236, 204..236, 960..992): (50, 50) is 922 away, (-236, -236) 1091
    t.brush('func_button', size=(32, 32, 32), at=(220, 220, 960), spawnflags='1', wait='1', objecthint='pickup',
            objecthint_distance='1000')
    scan_near_far(t, (50, 50, 0), (-236, -236, 0), (220, 220, 976))


@S.test('trigger_changevalue sets objecthint_distance 1000 while the map runs: an item_battery about 615 units away '
        'gets a scan hint; set back to 0 (the default 512): none again', players=1, doc=DOC,
        covers=('item_battery', 'item_battery.objecthint=pickup', 'item_battery.objecthint_distance'))
def runtime_distance(t):
    t.point('trigger_changevalue', targetname='far', target='bat', m_iszValueName='objecthint_distance',
            m_iszNewValue='1000', m_iszValueType='0')
    t.point('trigger_changevalue', targetname='def', target='bat', m_iszValueName='objecthint_distance',
            m_iszNewValue='0', m_iszValueType='0')
    t.point('item_battery', at=(220, 220, 0), objecthint='pickup', targetname='bat')
    look_at(t, 'player1', (-230, -230, 0), (220, 220, 8))
    t.wait(0.5)
    t.mark('before')
    t.wait(0.5)
    t.fire('far')
    t.wait(0.2)
    t.mark('set')
    t.wait(0.5)
    t.fire('def')
    t.wait(0.3)
    t.mark('reset')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'flags': 0}, after='before', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', min=3, fields=SCAN, after='set', within=(0, 0.5))
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'flags': 0}, after='reset', within=(0, 0.5))


@S.test('item_battery "pickup" behind the player (looking away): no scan hint', players=1, doc=DOC,
        covers=('item_battery', 'item_battery.objecthint=pickup'))
def pickup_behind(t):
    t.point('item_battery', at=(0, 150, 0), objecthint='pickup')
    t.teleport('player1', (0, 0, 0), yaw=270)
    t.look('player1', 0, 270)
    t.wait(0.5)
    t.mark('m')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'flags': 0}, after='m', within=(0, 0.5))


@S.test('trigger_changevalue clears objecthint while the map runs: the hint stops', players=1, doc=DOC,
        covers=('func_button', 'func_button.objecthint'))
def runtime_clear(t):
    t.point('trigger_changevalue', targetname='cv', target='b', m_iszValueName='objecthint', m_iszNewValue='',
            m_iszValueType='0')
    face_button(t, objecthint='button', targetname='b')
    t.wait(0.5)
    t.fire('cv')
    t.wait(0.2)
    t.mark('cleared')
    t.wait(0.5)
    t.expect.msg('ObjectHint', to='player1', count=0, fields={'r': 255}, after='cleared', within=(0, 0.5))


@S.test('a spectator in front of a hinted button gets no ObjectHint messages at all', players=2, doc=DOC,
        covers=('func_button', 'func_button.objecthint=button'))
def spectator_none(t):
    t.brush('func_button', size=(32, 8, 32), at=(0, 16, 48), spawnflags='1', wait='1', objecthint='button')
    t.teleport('player1', (-150, -150, 0), yaw=0)
    t.teleport('player2', (0, -24, 0), yaw=90)
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.mark('spec')
    t.wait(1.0)
    t.expect.msg('ObjectHint', to='player2', count=0, after='spec', within=(0, 1.0))
