"""mission_failed through a real co-op round (GAPS.md tier 1 "mission success / fail through real rounds"): a locked
master keeps it from failing the mission; unlocked, the mission fails: "FAILED MISSION" to everybody, the round ends
(game_round_end), the Failed_Mission log line, the mission end state. Its own suite: a mission ends once per map."""

from etest import Suite

SUITE = Suite('mission_failed', 'mission_failed in a real round: master, mission failed')
S = SUITE
S.server_args = ('+log', 'on')     # the round and mission log lines (mp_logecho prints them on the console)


@S.test('mission_failed: nothing while its master is locked; unlocked: the mission fails (text to everybody, round '
        'end, log line, mission end state)', players=2,
        covers=('mission_failed', 'mission_failed.master'))
def mission_failed(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock', triggerstate='1')
    t.point('mission_failed', targetname='outcome', master='lock')
    t.point('trigger_relay', targetname='=game_round_end', target='ended', triggerstate='2')
    t.point('info_target', targetname='ended')
    for n in ('game_mission_failed', 'game_mission_success', 'game_round_failed'):
        t.point('info_target', targetname='=' + n)
    t.wait(0.3)
    t.mark('locked')
    t.fire('outcome', activator='player1')
    t.wait(0.5)
    t.mark('open')
    t.fire('key')
    t.fire('outcome', activator='player1')
    t.wait_for_round(5, 3)                                  # the mission end
    t.wait(0.3)
    t.expect.msg('TextMsg', fields={'text': 'FAILED MISSION\n'}, count=0, after='locked', within=(0, 0.4))
    t.expect.used('ended', count=0, after='locked', within=(0, 0.4))
    t.expect.msg('TextMsg', to='player1', fields={'text': 'FAILED MISSION\n'}, min=1, after='open')
    t.expect.msg('TextMsg', to='player2', fields={'text': 'FAILED MISSION\n'}, min=1, after='open')
    t.expect.used('ended', count=1, after='open')
    t.expect.console('World triggered "Failed_Mission"')
    t.expect.no_console('World triggered "Successful_Mission"')
    t.expect.fired('=game_mission_failed', activator='world', count=1, after='open')
    t.expect.not_fired('=game_mission_failed', after='locked', within=(0, 0.4))
    t.expect.order('=game_round_end', '=game_mission_failed')
    t.expect.not_fired('=game_mission_success')
    t.expect.not_fired('=game_round_failed')
