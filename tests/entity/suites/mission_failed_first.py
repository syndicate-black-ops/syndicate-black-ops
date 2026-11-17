"""Only a co-op mission's first result counts: a mission_failed, then a game_end in the same frame (the result not yet
acted on) and again in the mission end. Only "FAILED MISSION" shows, the mission stays failed (the Failed_Mission log
line, game_mission_failed). Its own suite: a mission ends once per map."""

from etest import Suite

SUITE = Suite('mission_failed_first', 'mission_failed, then game_end: only the first result shows and counts')
S = SUITE
S.server_args = ('+log', 'on')     # the round and mission log lines (mp_logecho prints them on the console)


@S.test('mission_failed, then game_end in the same frame and in the mission end: only FAILED MISSION shows, the '
        'mission stays failed', players=2, covers=('mission_failed', 'game_end'))
def failed_then_end(t):
    t.point('mission_failed', targetname='fail')
    t.point('game_end', targetname='success')
    for n in ('game_mission_failed', 'game_mission_success'):
        t.point('info_target', targetname='=' + n)
    t.wait(0.3)
    t.mark('open')
    t.fire('fail', activator='player1')
    t.fire('success', activator='player1')                  # same frame: before the game acts on the first result
    t.wait_for_round(5, 3)                                  # the mission end
    t.wait(0.3)
    t.mark('late')
    t.fire('success', activator='player1')
    t.wait(0.5)
    t.expect.msg('TextMsg', to='player1', fields={'text': 'FAILED MISSION\n'}, count=1, after='open')
    t.expect.msg('TextMsg', to='player2', fields={'text': 'FAILED MISSION\n'}, count=1, after='open')
    t.expect.msg('TextMsg', fields={'text': 'SUCCESSFULL MISSION\n'}, count=0, after='open')
    t.expect.console('World triggered "Failed_Mission"')
    t.expect.no_console('World triggered "Successful_Mission"')
    t.expect.fired('=game_mission_failed', activator='world', count=1, after='open')
    t.expect.not_fired('=game_mission_success')
