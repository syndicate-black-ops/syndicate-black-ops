"""trigger_gravity (SBO 2.1): master, On / Off / Toggle by name, flag 2 "Start Off", flag 4 "Reset on leave" (the
gravity goes back to 1 when the player leaves the brush, dies, turns spectator, or the trigger is turned off or
removed while he is inside); without the flag a player keeps the gravity, as in Half-Life. The plain touch is in
suite pmove. The spectator and death tests come last (player3 stays a spectator, player2 dead)."""

from etest import Suite

SUITE = Suite('gravity', 'trigger_gravity: master, on / off, Start Off, Reset on leave', players=3)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#other-entities'

IN = (0, 100, 0)
OUT = (0, -150, 0)


def zone(t, **keys):
    t.trigger('trigger_gravity', size=(128, 128, 72), at=IN, gravity='0.25', **keys)


def visit(t, label, player='player1', where=IN):
    """Into the brush, a snapshot of the gravity there, back out (the gravity stays as it is)."""
    t.teleport(player, where, yaw=90)
    t.wait(0.3)
    t.snap(label, player, 'gravity')
    t.teleport(player, OUT, yaw=90)
    t.wait(0.1)


def normal(t, player='player1'):
    t.kv(player, 'gravity', '1')
    t.wait(0.1)


@S.test('master: while the master (env_state, off) is locked a player touching it keeps his gravity; once it is '
        'unlocked he gets the trigger\'s (0.25)', players=1, doc=DOC,
        covers=('trigger_gravity', 'trigger_gravity.master', 'trigger_gravity.gravity', 'env_state'))
def master(t):
    t.point('env_state', targetname='st')
    zone(t, master='st')
    t.wait(0.3)
    visit(t, 'locked')
    t.fire('st', use='on')
    t.wait(0.1)
    visit(t, 'open')
    normal(t)
    t.expect.value('locked', 'player1', 'gravity', 1.0, tol=0.01)
    t.expect.value('open', 'player1', 'gravity', 0.25, tol=0.01)


@S.test('fired by name: Off turns it off, On turns it on, On again keeps it on, Toggle turns it off and on again',
        players=1, doc=DOC, covers=('trigger_gravity.targetname',))
def on_off(t):
    zone(t, targetname='tg')
    t.wait(0.3)
    t.fire('tg', use='off')
    t.wait(0.1)
    visit(t, 'off')
    t.fire('tg', use='on')
    t.wait(0.1)
    visit(t, 'on')
    normal(t)
    t.fire('tg', use='on')
    t.wait(0.1)
    visit(t, 'on_again')
    normal(t)
    t.fire('tg', use='toggle')
    t.wait(0.1)
    visit(t, 'toggled_off')
    t.fire('tg', use='toggle')
    t.wait(0.1)
    visit(t, 'toggled_on')
    normal(t)
    t.expect.value('off', 'player1', 'gravity', 1.0, tol=0.01)
    t.expect.value('on', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('on_again', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('toggled_off', 'player1', 'gravity', 1.0, tol=0.01)
    t.expect.value('toggled_on', 'player1', 'gravity', 0.25, tol=0.01)


@S.test('flag 2 "Start Off": nothing until it is fired; then a player touching it gets its gravity', players=1,
        doc=DOC, covers=('trigger_gravity:2',))
def start_off(t):
    zone(t, targetname='tg', spawnflags='2')
    t.wait(0.3)
    visit(t, 'start')
    t.fire('tg')
    t.wait(0.1)
    visit(t, 'fired')
    normal(t)
    t.expect.value('start', 'player1', 'gravity', 1.0, tol=0.01)
    t.expect.value('fired', 'player1', 'gravity', 0.25, tol=0.01)


@S.test('flag 4 "Reset on leave": inside the player has its gravity (0.25); once he leaves the brush it is 1 again',
        players=1, doc=DOC, covers=('trigger_gravity:4',))
def reset_leave(t):
    zone(t, spawnflags='4')
    t.wait(0.3)
    visit(t, 'in')
    t.wait(0.3)
    t.snap('left', 'player1', 'gravity')
    normal(t)
    t.expect.value('in', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('left', 'player1', 'gravity', 1.0, tol=0.01)


@S.test('without "Reset on leave" (Half-Life): the gravity stays after the trigger is turned off with the player '
        'inside, and after he leaves', players=1, doc=DOC, covers=('trigger_gravity:4',))
def no_reset(t):
    zone(t, targetname='tg')
    t.wait(0.3)
    t.teleport('player1', IN, yaw=90)
    t.wait(0.3)
    t.snap('in', 'player1', 'gravity')
    t.fire('tg', use='off')
    t.wait(0.3)
    t.snap('off', 'player1', 'gravity')
    t.teleport('player1', OUT, yaw=90)
    t.wait(0.5)
    t.snap('left', 'player1', 'gravity')
    normal(t)
    t.expect.value('in', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('off', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('left', 'player1', 'gravity', 0.25, tol=0.01)


@S.test('"Reset on leave": turned off while the player is inside, his gravity is 1 at once', players=1, doc=DOC,
        covers=('trigger_gravity:4', 'trigger_gravity.targetname'))
def reset_off(t):
    zone(t, targetname='tg', spawnflags='4')
    t.wait(0.3)
    t.teleport('player1', IN, yaw=90)
    t.wait(0.3)
    t.snap('in', 'player1', 'gravity')
    t.fire('tg', use='off')
    t.wait_frames(2)
    t.snap('off', 'player1', 'gravity')
    t.teleport('player1', OUT, yaw=90)
    normal(t)
    t.expect.value('in', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('off', 'player1', 'gravity', 1.0, tol=0.01)


@S.test('"Reset on leave": removed (killtarget) while the player is inside, his gravity is 1 at once', players=1,
        doc=DOC, covers=('trigger_gravity:4', 'trigger_relay.killtarget'))
def reset_removed(t):
    zone(t, targetname='tg', spawnflags='4')
    t.point('trigger_relay', targetname='kill', killtarget='tg', triggerstate='2')
    t.wait(0.3)
    t.teleport('player1', IN, yaw=90)
    t.wait(0.3)
    t.snap('in', 'player1', 'gravity')
    t.mark('go')
    t.fire('kill')
    t.wait_frames(2)
    t.snap('removed', 'player1', 'gravity')
    t.teleport('player1', OUT, yaw=90)
    normal(t)
    t.expect.removed('tg', after='go', count=1)
    t.expect.value('in', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('removed', 'player1', 'gravity', 1.0, tol=0.01)


@S.test('"Reset on leave" next to a trigger_gravity without it (0.5): stepping from the first into the second keeps '
        'the second\'s gravity (only the gravity the trigger gave is reset)', players=1, doc=DOC,
        covers=('trigger_gravity:4',))
def reset_overlap(t):
    t.trigger('trigger_gravity', size=(96, 128, 72), at=(-48, 100, 0), gravity='0.25', spawnflags='4')
    t.trigger('trigger_gravity', size=(96, 128, 72), at=(48, 100, 0), gravity='0.5')
    t.wait(0.3)
    t.teleport('player1', (-56, 100, 0), yaw=90)
    t.wait(0.3)
    t.snap('first', 'player1', 'gravity')
    t.teleport('player1', (56, 100, 0), yaw=90)
    t.wait(0.5)
    t.snap('second', 'player1', 'gravity')
    t.teleport('player1', OUT, yaw=90)
    normal(t)
    t.expect.value('first', 'player1', 'gravity', 0.25, tol=0.01)
    t.expect.value('second', 'player1', 'gravity', 0.5, tol=0.01)


# ---------------------------------------------------------------------------------------------- leaving play (last)

@S.test('"Reset on leave": a player inside who turns spectator has gravity 1', players=3, doc=DOC,
        covers=('trigger_gravity:4',))
def reset_spectator(t):
    zone(t, spawnflags='4')
    t.wait(0.3)
    t.teleport('player3', IN, yaw=90)
    t.wait(0.3)
    t.snap('in', 'player3', 'gravity')
    t.cmd('player3', 'spectate')
    t.wait(0.5)
    t.snap('spec', 'player3', 'gravity')
    t.expect.value('in', 'player3', 'gravity', 0.25, tol=0.01)
    t.expect.value('spec', 'player3', 'gravity', 1.0, tol=0.01)


@S.test('"Reset on leave": a player who dies inside has gravity 1', players=3, doc=DOC,
        covers=('trigger_gravity:4',))
def reset_death(t):
    zone(t, spawnflags='4')
    t.wait(0.3)
    t.teleport('player2', IN, yaw=90)
    t.wait(0.3)
    t.snap('in', 'player2', 'gravity')
    t.damage('player2', 5000, 0)
    t.wait(0.5)
    t.snap('dead', 'player2', 'gravity', 'deadflag')
    t.expect.value('in', 'player2', 'gravity', 0.25, tol=0.01)
    t.expect.value('dead', 'player2', 'deadflag', 0, op='>')
    t.expect.value('dead', 'player2', 'gravity', 1.0, tol=0.01)
