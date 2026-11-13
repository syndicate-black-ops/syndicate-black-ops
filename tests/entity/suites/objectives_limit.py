"""trigger_objective: at most 32 per map (docs/OBJECTIVES.md "Limits"). The 33rd is removed when the map starts, with a
console line; the 32 others work. Its own suite: it fills the map's slots."""

from etest import Suite

SUITE = Suite('objectives_limit', 'trigger_objective: 32 per map', players=1, requires=('Objective_SendQueued',))
S = SUITE
DOC = 'OBJECTIVES.md#trigger_objective-point-entity'


@S.test('33 objectives: one is removed at map start with a console line; the others are all sent to a client that '
        'gets its HUD again', players=1, doc=DOC, covers=('trigger_objective',))
def limit(t):
    for i in range(33):
        t.point('trigger_objective', targetname='o%02d' % i, message='L%02d' % i, state='1', at=(-200 + (i % 11) * 40,
                -150 + (i // 11) * 40, 0))
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(2.0)
    t.expect.console('more than 32 objectives in the map, this one is removed')
    t.expect.msg('Objective', to='player1', count=32, after='fu', fields={'state': 1})
