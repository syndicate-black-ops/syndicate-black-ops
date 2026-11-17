"""Only a co-op mission's first result counts: a game_end, then a mission_failed in the same frame (the result not yet
acted on) and again in the mission end. Only "SUCCESSFULL MISSION" shows, the mission stays accomplished (the
Successful_Mission log line, game_mission_success). Its own suite: a mission ends once per map."""

from etest import Suite

SUITE = Suite('game_end_first', 'game_end, then mission_failed: only the first result shows and counts')
S = SUITE
S.server_args = ('+log', 'on')     # the round and mission log lines (mp_logecho prints them on the console)


@S.test('game_end, then mission_failed in the same frame and in the mission end: only SUCCESSFULL MISSION shows, '
        'the mission stays accomplished', players=2, covers=('mission_failed', 'game_end'))
def end_then_failed(t):
    t.point('mission_failed', targetname='fail')
    t.point('game_end', targetname='success')
    for n in ('game_mission_failed', 'game_mission_success'):
        t.point('info_target', targetname='=' + n)
    t.wait(0.3)
    t.mark('open')
    t.fire('success', activator='player1')
    t.fire('fail', activator='player1')                     # same frame: before the game acts on the first result
    t.wait_for_round(5, 3)                                  # the mission end
    t.wait(0.3)
    t.mark('late')
    t.fire('fail', activator='player1')
    t.wait(0.5)
    t.expect.msg('TextMsg', to='player1', fields={'text': 'SUCCESSFULL MISSION\n'}, count=1, after='open')
    t.expect.msg('TextMsg', to='player2', fields={'text': 'SUCCESSFULL MISSION\n'}, count=1, after='open')
    t.expect.msg('TextMsg', fields={'text': 'FAILED MISSION\n'}, count=0, after='open')
    t.expect.console('World triggered "Successful_Mission"')
    t.expect.no_console('World triggered "Failed_Mission"')
    t.expect.fired('=game_mission_success', activator='world', count=1, after='open')
    t.expect.not_fired('=game_mission_failed')
