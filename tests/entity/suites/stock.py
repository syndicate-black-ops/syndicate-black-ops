"""Stock HL SDK entities SBO maps rely on (by use in the SBO maps: most used first)."""

from etest import Suite

SUITE = Suite('stock', 'Stock entities SBO maps rely on')
S = SUITE


@S.test('trigger_relay passes its trigger on, with its Trigger State', doc=None,
        covers=('trigger_relay', 'trigger_relay.target', 'trigger_relay.triggerstate=0',
                'trigger_relay.triggerstate=1'))
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


@S.test('multi_manager fires its targets at their delays, in order', covers=('multi_manager',))
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


@S.test('func_door opens when fired, fires its target, closes after wait',
        covers=('func_door', 'func_door.target', 'func_door.speed', 'func_door.wait', 'func_door.lip'))
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


@S.test('func_button pressed by a player (+use) fires its target', players=1,
        covers=('func_button', 'func_button.target'))
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


@S.test('button_target without a master: a shot, +use ("Use Activates") and a trigger each toggle its frame and '
        'fire its target ON / OFF', players=1, covers=('button_target', 'button_target.target', 'button_target:1'))
def button_target(t):
    t.brush('button_target', size=(32, 8, 32), at=(0, 16, 48), tex='+0~ANZYG1', targetname='bt', target='out',
            spawnflags='1')
    t.point('info_target', targetname='out')
    t.teleport('player1', (0, -24, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.damage('bt', 10, 0, 'player1')
    t.wait(0.1)
    t.snap('shot', 'bt', 'frame')
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.2)
    t.snap('used', 'bt', 'frame')
    t.fire('bt', activator='player1')
    t.wait(0.1)
    t.snap('fired', 'bt', 'frame')
    t.expect.value('shot', 'bt', 'frame', 1)
    t.expect.value('used', 'bt', 'frame', 0)
    t.expect.value('fired', 'bt', 'frame', 1)
    t.expect.fired('out', activator='player1', use=1, count=2)      # USE_ON: the shot and the trigger
    t.expect.fired('out', activator='player1', use=0, count=1, after='use')     # USE_OFF: the +use


@S.test('trigger_multiple fires when a player walks in, again after its wait', players=1,
        covers=('trigger_multiple', 'trigger_multiple.target', 'trigger_multiple.wait'))
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


@S.test('func_breakable breaks under damage, fires its target, is removed',
        covers=('func_breakable', 'func_breakable.health', 'func_breakable.target'))
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


@S.test('trigger_relay with a delay unlocks a multisource, the master of a trigger_teleport (co_spells tower teleport)',
        players=1,
        covers=('trigger_relay', 'trigger_relay.delay', 'trigger_relay.triggerstate=1', 'multisource',
                'trigger_teleport', 'trigger_teleport.target', 'trigger_teleport.master'))
def relay_delay_unlocks_master(t):
    # the delayed Use comes from a temporary DelayedUse entity, which is not a member of the multisource: it toggles
    # the slot of the relay whose delay made it (the HL SDK toggled the last member's, here the same one)
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='relay', target='lock', delay='4', triggerstate='1', spawnflags='1')
    t.trigger('trigger_teleport', size=(64, 64, 96), at=(0, -150, 0), target='dest', master='lock')
    t.point('info_teleport_destination', targetname='dest', at=(150, 150, 0))
    t.teleport('player1', (0, -150, 0))
    t.mark('go')
    t.fire('relay')
    t.wait(3.5)
    t.snap('locked', 'player1', 'origin')
    t.snap('locked', 'dest', 'origin')
    t.wait(1.5)
    t.snap('open', 'player1', 'origin')
    t.snap('open', 'dest', 'origin')
    t.expect.used('lock', after='go', within=(3.8, 4.4), count=1)
    t.expect.distance('locked', 'player1', 'dest', 250, 500)     # still locked before the delay ends
    t.expect.distance('open', 'player1', 'dest', 0, 20)          # teleported once the relay's delayed Use arrived


@S.test('multisource Used by an entity that is not a member: the last member\'s slot toggles, the multisource fires '
        '(HL SDK)', covers=('multisource', 'multisource.target'))
def multisource_non_member(t):
    t.point('multisource', targetname='lock', target='out')
    t.point('trigger_relay', targetname='key', target='lock')      # its only member, never fired
    t.point('info_target', targetname='out')
    t.wait(0.3)                                                    # the multisource registers its members
    t.mark('go')
    t.fire('lock')                                                 # caller: the world, not a member
    t.wait(0.2)
    t.expect.used('out', after='go', count=1)


@S.test('multisource without members, Used: it fires its target (HL SDK)', covers=('multisource', 'multisource.target'))
def multisource_no_members(t):
    t.point('multisource', targetname='lock', target='out')
    t.point('info_target', targetname='out')
    t.wait(0.3)
    t.mark('go')
    t.fire('lock')
    t.wait(0.2)
    t.expect.used('out', after='go', count=1)


@S.test('func_rotating plays its sound at the volume the mapper set: "volume" (the FGD key) or "Volume", 10 = loudest, '
        '0 = no sound (also with spin-up), left out = loudest', doc='SBO-2.0-CHANGES.md',
        covers=('func_rotating', 'func_rotating.volume', 'func_rotating.sounds=1', 'func_rotating:16'))
def rotating_volume(t):
    # No spin-up (spawnflag 16 off): the start sound is EMIT_SOUND at the entity's volume, fan1.wav (sounds 1).
    # With spin-up (16): a start at 0.01, then the volume ramps up.
    for name, x, y, kv in (('rot5', -180, 0, {'volume': '5'}), ('rot10', -60, 0, {'volume': '10'}),
                           ('rot0', 60, 0, {'volume': '0'}), ('rotcap', 180, 0, {'Volume': '5'}),
                           ('rotnokey', -60, 150, {}), ('rot0acc', 60, 150, {'volume': '0', 'spawnflags': '16'})):
        t.brush('func_rotating', size=(32, 32, 32), at=(x, y, 32), origin=True, targetname=name, speed='90',
                sounds='1', **kv)
    t.wait(0.2)
    t.mark('go')
    for name in ('rot5', 'rot10', 'rot0', 'rotcap', 'rotnokey', 'rot0acc'):
        t.fire(name)
    t.wait(1.5)
    t.expect.sound('fans/fan1.wav', ent='rot5', vol=(0.45, 0.55), after='go', count=1)
    t.expect.sound('fans/fan1.wav', ent='rot10', vol=(0.95, 1.0), after='go', count=1)
    t.expect.sound('fans/fan1.wav', ent='rot0', after='go', count=0)
    t.expect.sound('fans/fan1.wav', ent='rotcap', vol=(0.45, 0.55), after='go', count=1)
    t.expect.sound('fans/fan1.wav', ent='rotnokey', vol=(0.95, 1.0), after='go', count=1)
    t.expect.sound('fans/fan1.wav', ent='rot0acc', after='go', count=0)
