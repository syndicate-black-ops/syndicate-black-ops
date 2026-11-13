"""trigger_push's Featureful / Spirit keys (sdk/syndicate.fgd): Push direction [LV] (m_iszPushVel) instead of the
angles, Speed factor [LR] (m_iszPushSpeed) per toucher, and Obey Trigger Mode; a player pushed by a "Push Once Only"
(1) trigger gets the push added to his velocity at once. The player is put into the trigger in mid-air (no ground
friction), so the horizontal velocity read a few frames later is the push itself."""

from etest import Suite

SUITE = Suite('push_keys', 'trigger_push: push direction, speed factor, Obey Trigger Mode', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


def push_once(t, **kv):
    kv.setdefault('speed', '400')
    t.trigger('trigger_push', targetname='tp', size=(96, 96, 160), at=(0, 0, 0), spawnflags='1', **kv)


def pushed(t, mark='go'):
    t.wait(0.3)
    t.mark(mark)
    t.teleport('player1', (0, 0, 60), yaw=90)
    t.flags('player1', '-FL_ONGROUND')
    t.wait_frames(3)
    t.snap('v', 'player1', 'velocity')


@S.test('control: angles 0 0 0 push east (+X) at Speed of push', players=1, doc=DOC,
        covers=('trigger_push', 'trigger_push.speed', 'trigger_push:1'))
def control(t):
    t.wait(1.0)     # right after the suite start a player's touches are not taken at once (GAPS tier 1)
    push_once(t, angles='0 0 0')
    pushed(t)
    t.expect.value('v', 'player1', 'velocity.x', 400, tol=20)
    t.expect.value('v', 'player1', 'velocity.y', 0, tol=20)
    t.expect.removed('tp', after='go')


@S.test('Push direction [LV] "0 1 0" replaces the angles: the push goes north (+Y)', players=1, doc=DOC,
        covers=('trigger_push.m_iszPushVel',))
def push_vel(t):
    push_once(t, angles='0 0 0', m_iszPushVel='0 1 0')
    pushed(t)
    t.expect.value('v', 'player1', 'velocity.y', 400, tol=20)
    t.expect.value('v', 'player1', 'velocity.x', 0, tol=20)


@S.test('Speed factor [LR] 0.25 multiplies the push (400 x 0.25 = 100)', players=1,
        doc=DOC, covers=('trigger_push.m_iszPushSpeed',))
def push_speed(t):
    push_once(t, angles='0 0 0', m_iszPushSpeed='0.25')
    pushed(t)
    t.expect.value('v', 'player1', 'velocity.x', 100, tol=20)


@S.test('Obey Trigger Mode 1 on a Start Off (2) push: On twice leaves it on (it pushes); mode 0 (control): On twice '
        'toggles it off again', players=1, doc='LOGIC_ENTITIES.md#obey-trigger-mode',
        covers=('trigger_push.m_iObeyTriggerMode=1', 'trigger_push.m_iObeyTriggerMode=0', 'trigger_push:2'))
def obey(t):
    t.trigger('trigger_push', targetname='ob', size=(96, 96, 160), at=(-120, 0, 0), angles='0 0 0', speed='400',
              spawnflags=str(1 | 2), m_iObeyTriggerMode='1')
    t.trigger('trigger_push', targetname='tg', size=(96, 96, 160), at=(120, 0, 0), angles='0 0 0', speed='400',
              spawnflags=str(1 | 2), m_iObeyTriggerMode='0')
    for n in ('ob', 'tg'):
        t.fire(n, use='on')
        t.fire(n, use='on')
    t.wait(0.3)
    t.snap('s', 'ob', 'solid')
    t.snap('s', 'tg', 'solid')
    t.mark('go')
    t.teleport('player1', (-120, 0, 60), yaw=90)
    t.flags('player1', '-FL_ONGROUND')
    t.wait_frames(3)
    t.snap('v', 'player1', 'velocity')
    t.expect.value('s', 'ob', 'solid', 1)       # SOLID_TRIGGER: on
    t.expect.value('s', 'tg', 'solid', 0)       # SOLID_NOT: toggled off again
    t.expect.value('v', 'player1', 'velocity.x', 400, tol=20)
    t.expect.removed('ob', after='go')
