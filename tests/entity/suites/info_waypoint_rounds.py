"""info_waypoint round_reset (docs/INFO_WAYPOINT.md): its own suite, since a round restart respawns every player."""

from etest import Suite

SUITE = Suite('info_waypoint_rounds', 'info_waypoint: Reset at round start', players=1,
              requires=('info_waypoint',))
S = SUITE
DOC = 'INFO_WAYPOINT.md#keys'


def restart(t):
    t.mark('restart')
    t.console('test_round restart')
    t.steps.append('wait_for round 3 10')
    t.duration += 10
    t.wait(0.3)
    t.mark('after')


@S.test('Reset at round start Yes: a marker turned on with a new label goes off and gets its map-load label back; '
        'a Start on marker with Yes, turned off, comes back on with its map-load label and color; No keeps its state',
        players=1, doc=DOC,
        covers=('info_waypoint.round_reset', 'info_waypoint.round_reset=1', 'info_waypoint.round_reset=0'))
def round_reset(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='r1', message='R1', round_reset='1')
    t.point('info_waypoint', at=(0, 64, 64), targetname='r1on', message='R1ON', rendercolor='1 2 3', spawnflags='2',
            round_reset='1')
    t.point('info_waypoint', at=(0, -64, 64), targetname='r0', message='R0', round_reset='0')
    for n, key, val in (('cv1', 'message', 'CHANGED'), ('cv2', 'rendercolor', '9 9 9')):
        t.point('trigger_changevalue', targetname=n, target='r1on' if n == 'cv2' else 'r1', m_iszValueName=key,
                m_iszNewValue=val)
    t.wait(0.5)
    t.fire('r1', use='on')
    t.fire('cv1')
    t.fire('cv2')
    t.fire('r1on', use='off')
    t.fire('r0', use='on')
    t.wait(0.5)
    restart(t)
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('Waypoint', to='player1', min=1, after='restart', fields={'on': 1, 'label': 'R1ON',
                                                                            'color': [1, 2, 3]})
    t.expect.msg('Waypoint', to='player1', count=1, after='fu', within=(0, 0.9), fields={'on': 1, 'label': 'R1ON'})
    t.expect.msg('Waypoint', to='player1', count=0, after='fu', within=(0, 0.9), fields={'on': 1, 'label': 'R1'})
    t.expect.msg('Waypoint', to='player1', count=0, after='fu', within=(0, 0.9), fields={'on': 1, 'label': 'CHANGED'})
    t.expect.msg('Waypoint', to='player1', count=1, after='fu', within=(0, 0.9), fields={'on': 1, 'label': 'R0'})
