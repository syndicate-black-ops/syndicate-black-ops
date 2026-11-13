"""trigger_objective flag 2 "Reset on new round" (docs/OBJECTIVES.md): a co-op round restart puts an objective with the
flag back to its map-start state (Hidden: the clients forget it; Active: shown again at 0); one without the flag keeps
its state. Its own suite: a round restart respawns every player."""

from etest import Suite

SUITE = Suite('objectives_round', 'trigger_objective: Reset on new round (flag 2)', players=1,
              requires=('Objective_SendQueued', 'RoundHooks_Reset'))
S = SUITE
DOC = 'OBJECTIVES.md#trigger_objective-point-entity'
HIDDEN, ACTIVE, COMPLETED = 0, 1, 2


@S.test('a round restart: flag 2 objectives go back to their map-start state (a hidden one hidden again, an active '
        'counter back to 0/3), one without the flag stays completed', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective:2', 'trigger_objective.state=0', 'trigger_objective.state=1',
                'trigger_objective.count'))
def round_reset(t):
    t.point('trigger_objective', targetname='r', message='R01 reset hidden', spawnflags='2')
    t.point('trigger_objective', targetname='c', message='R01 reset counter', spawnflags='2', state='1', count='3')
    t.point('trigger_objective', targetname='k', message='R01 kept', state='1')
    t.wait(0.3)
    t.fire('r')
    t.fire('c')
    t.fire('c')
    t.fire('k')
    t.wait(0.2)
    t.mark('restart')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.5)
    t.expect.msg('Objective', count=1, after='restart', fields={'state': HIDDEN, 'new': False})
    # the reset's message to all, and the fullupdate's resend to player1
    t.expect.msg('Objective', count=2, after='restart',
                 fields={'text': 'R01 reset counter', 'state': ACTIVE, 'count': 0, 'target': 3, 'new': False})
    t.expect.msg('Objective', count=0, after='restart', fields={'text': 'R01 kept', 'state': ACTIVE})
    # what a client gets afterwards
    t.expect.msg('Objective', to='player1', count=0, after='fu', fields={'text': 'R01 reset hidden'})
    t.expect.msg('Objective', to='player1', count=1, after='fu', fields={'text': 'R01 kept', 'state': COMPLETED})
    t.expect.msg('Objective', to='player1', count=1, after='fu', fields={'text': 'R01 reset counter', 'count': 0})
