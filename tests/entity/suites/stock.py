"""Stock HL SDK entities SBO maps rely on (by use in the SBO maps: most used first)."""

from etest import Suite

SUITE = Suite('stock', 'Stock entities SBO maps rely on')
S = SUITE


@S.test('trigger_relay passes its trigger on, with its Trigger State', doc=None)
def relay(t):
    t.point('trigger_relay', targetname='relay_on', target='out_on', triggerstate='1')
    t.point('trigger_relay', targetname='relay_off', target='out_off', triggerstate='0')
    t.point('info_target', targetname='out_on')
    t.point('info_target', targetname='out_off')
    t.fire('relay_on')
    t.fire('relay_off')
    t.wait(0.1)
    t.expect.fired('out_on', caller='relay_on', use=1, count=1)     # USE_ON
    t.expect.fired('out_off', caller='relay_off', use=0, count=1)   # USE_OFF


@S.test('multi_manager fires its targets at their delays, in order')
def multi_manager(t):
    t.point('multi_manager', targetname='mm', a='0', b='0.5', c='1.2')
    for n in 'abc':
        t.point('info_target', targetname=n)
    t.mark('go')
    t.fire('mm')
    t.wait(1.6)
    t.expect.fired('a', count=1, after='go', within=(0, 0.15))
    t.expect.fired('b', count=1, after='go', within=(0.4, 0.7))
    t.expect.fired('c', count=1, after='go', within=(1.1, 1.4))
    t.expect.order('a', 'b')
    t.expect.order('b', 'c')


@S.test('func_door opens when fired, fires its target, closes after wait')
def door(t):
    # a door moving up 64 - lip; "wait" 1: closes 1 s after it is open
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='200', wait='1', lip='8', target='door_open')
    t.point('info_target', targetname='door_open')
    t.snap('closed', 'door', 'origin')
    t.mark('go')
    t.fire('door')
    t.wait(0.9)
    t.snap('open', 'door', 'origin')
    t.wait(2.0)
    t.snap('back', 'door', 'origin')
    t.expect.used('door', count=1)
    t.expect.fired('door_open', after='go', within=(0.3, 0.8))     # 88 units at 200 u/s
    t.expect.value('closed', 'door', 'origin', [0, 0, 0])
    t.expect.value('open', 'door', 'origin', [0, 0, 88], tol=0.5)
    t.expect.value('back', 'door', 'origin', [0, 0, 0], tol=0.5)


@S.test('func_button pressed by a player (+use) fires its target', players=1)
def button(t):
    t.brush('func_button', size=(16, 32, 32), at=(0, 120, 40), tex='+0~ANZYG1', targetname='btn', target='pressed',
            angles='0 90 0', wait='1', speed='50', lip='12')
    t.point('info_target', targetname='pressed')
    t.teleport('player1', (0, 70, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.move('player1', 0, 0, 10, '+use')
    t.wait(0.5)
    t.expect.used('btn', activator='player1', min=1)
    t.expect.fired('pressed', activator='player1', count=1)


@S.test('trigger_multiple fires when a player walks in, again after its wait', players=1)
def trigger_multiple(t):
    t.trigger('trigger_multiple', size=(96, 96, 72), at=(0, 64, 0), target='touched', wait='0.5')
    t.point('info_target', targetname='touched')
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 64, 0))
    t.wait(1.2)
    t.expect.not_fired('touched', within=(0, 0.25))     # the player stands outside it at the start
    t.expect.fired('touched', activator='player1', min=2, after='in')
    t.expect.fired('touched', activator='player1', count=1, after='in', within=(0, 0.45))


@S.test('func_breakable breaks under damage, fires its target, is removed')
def breakable(t):
    t.brush('func_breakable', size=(32, 32, 32), at=(0, 0, 0), targetname='crate', health='30', target='broken',
            material='1')
    t.point('info_target', targetname='broken')
    t.damage('crate', 10)
    t.wait(0.1)
    t.snap('hurt', 'crate', 'health')
    t.damage('crate', 25)
    t.wait(0.3)
    t.expect.value('hurt', 'crate', 'health', 20)
    t.expect.fired('broken', count=1)
    t.expect.removed('crate')
