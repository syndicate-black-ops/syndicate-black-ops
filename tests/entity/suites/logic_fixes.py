"""env_state while it turns on or off (Spirit of Half-Life: turning on counts as on, turning off as off) and a
multisource input with a delay (it toggles its own slot, not the last member's). calc_velocity_polar's up / down
part: suite calc."""

from etest import Suite

SUITE = Suite('logic_fixes', 'env_state while turning, multisource delayed inputs')
S = SUITE
STATE = 'STATE_LOGIC.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


# ---------------------------------------------------------------------------------------------- env_state
@S.test('env_state: On while it turns on is ignored, it turns on when the time is over', doc=STATE,
        covers=('env_state', 'env_state.turnontime', 'env_state.target'))
def state_on_while_turning_on(t):
    t.point('env_state', targetname='st', target='out', turnontime='0.5', turnofftime='0')
    info(t, 'out')
    t.mark('go')
    t.fire('st', use='on')
    t.wait(0.2)
    t.fire('st', use='on')          # still turning on: ignored (it turned around to off before)
    t.wait(0.6)
    t.expect.fired('out', use=USE_ON, count=1, after='go', within=(0.45, 0.6))
    t.expect.fired('out', use=USE_OFF, count=0, min=0)


@S.test('env_state: Off while it turns on turns it around (it turns off at once with no turn-off time)', doc=STATE,
        covers=('env_state', 'env_state.turnontime', 'env_state.turnofftime'))
def state_off_while_turning_on(t):
    t.point('env_state', targetname='st', target='out', turnontime='0.5', turnofftime='0')
    info(t, 'out')
    t.fire('st', use='on')
    t.wait(0.2)
    t.mark('off')
    t.fire('st', use='off')         # turning on counts as on: Off turns it off (it was ignored before)
    t.wait(0.6)
    t.expect.fired('out', use=USE_OFF, count=1, after='off', within=(0, 0.1))
    t.expect.fired('out', use=USE_ON, count=0, min=0)


@S.test('env_state: Off while it turns off is ignored; On turns it around and it turns on again', doc=STATE,
        covers=('env_state', 'env_state:1', 'env_state.turnofftime', 'env_state.turnontime'))
def state_while_turning_off(t):
    t.point('env_state', targetname='st', target='out', turnontime='0', turnofftime='0.5', spawnflags='1')
    info(t, 'out')
    t.fire('st', use='off')
    t.wait(0.1)
    t.fire('st', use='off')         # still turning off: ignored
    t.wait(0.1)
    t.mark('on')
    t.fire('st', use='on')          # turning off counts as off: On turns it on at once
    t.wait(0.6)
    t.expect.fired('out', use=USE_ON, count=1, after='on', within=(0, 0.1))
    t.expect.fired('out', use=USE_OFF, count=0, min=0)


# ---------------------------------------------------------------------------------------------- multisource
@S.test('multisource: an input with a Delay before trigger toggles its own slot; with the other input on too the '
        'multisource fires (it toggled the last input\'s slot, which the other input then toggled back)',
        doc='SBO-2.0-CHANGES.md', covers=('multisource', 'multisource.target', 'trigger_relay.delay'))
def multisource_delayed_member(t):
    t.point('multisource', targetname='ms', target='ms_out')
    t.point('trigger_relay', targetname='r1', target='ms', delay='0.3', triggerstate='2')   # first member, delayed
    t.point('trigger_relay', targetname='r2', target='ms', triggerstate='2')                # last member
    info(t, 'ms_out')
    t.wait(0.3)                     # the multisource registers its members 0.1 s after the map starts
    t.fire('r1')
    t.wait(0.5)                     # r1's delayed Use arrives: its own slot is on
    t.mark('r2')
    t.fire('r2')
    t.wait(0.2)
    t.expect.fired('ms_out', count=1, after='r2', within=(0, 0.1))

