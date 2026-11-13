"""max_round through real co-op rounds (GAPS.md tier 1 "max_round nbmaxround", 52 maps): "how many rounds players get
to complete the mission" (with mp_easymode 0; 1, the default, gives 1.5 times as many). Every player killed loses the
round; the round limit's last lost round fails the mission. Its own suite (the round limit is the map's, a mission ends
once per map)."""

from etest import Suite

SUITE = Suite('round_limit', 'max_round: rounds lost up to nbmaxround, then the mission fails')
S = SUITE
# the round log lines carry the limit: (round "N" of "M"); mp_easymode 1 (the default) gives 1.5 times nbmaxround
# (set in the test: a command-line +mp_easymode comes before the game registers the cvar)
S.server_args = ('+log', 'on')

HOOKS = ('game_round_1', 'game_round_2', 'game_round_3', 'game_round_4', 'game_round_failed', 'game_mission_success',
         'game_mission_failed')


def hook_targets(t):
    """An info_target named after each round / mission hook, so every firing reaches an entity."""
    for n in HOOKS:
        t.point('info_target', targetname='=' + n)


def lose_round(t, mark):
    t.wait(2.3)                   # a player is protected for 2 s after he spawns
    t.mark(mark)
    t.damage('player1', 5000)
    t.wait_for_round(0, 2)        # lost: the pause between rounds
    t.wait_for_round(3, 20)       # the next round in play (5 s pause, then the players respawn)


@S.test('max_round nbmaxround 3: rounds 1 and 2 lost: FAILED ROUND; round 3 lost: FAILED MISSION (Failed_Mission)',
        players=1, covers=('max_round', 'max_round.nbmaxround'), size=(512, 512, 256))
def three_rounds(t):
    t.point('max_round', nbmaxround='3')
    hook_targets(t)
    t.console('mp_easymode 0')    # CoopRoundLimit reads it every time: round 1's start line was written with 1 (5)
    lose_round(t, 'lost1')
    lose_round(t, 'lost2')
    t.wait(2.3)
    t.mark('lost3')
    t.damage('player1', 5000)
    t.wait_for_round(5, 3)
    t.wait(0.3)
    t.expect.console('World triggered "Round_End" (round "1" of "3")')
    t.expect.console('World triggered "Round_Start" (round "2" of "3")')
    t.expect.console('World triggered "Round_Start" (round "3" of "3")')
    t.expect.no_console('(round "4" of')
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=1, after='lost1', within=(0, 0.5))
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=1, after='lost2', within=(0, 0.5))
    t.expect.msg('TextMsg', fields={'text': 'FAILED MISSION\n'}, count=1, after='lost3')
    t.expect.msg('TextMsg', fields={'text': 'FAILED ROUND\n'}, count=0, after='lost3')
    t.expect.console('World triggered "Failed_Mission"')
    # map hooks (docs/ROUNDS.md "Map hooks"): game_round_<N> with each round, game_round_failed for a lost round
    # that another follows, game_mission_failed (not game_round_failed) for the last one; the world as activator
    t.expect.fired('=game_round_failed', activator='world', count=1, after='lost1', within=(0, 0.5))
    t.expect.fired('=game_round_failed', activator='world', count=1, after='lost2', within=(0, 0.5))
    t.expect.not_fired('=game_round_failed', after='lost3')
    t.expect.order('=game_round_end', '=game_round_failed')
    t.expect.order('=game_round_failed', '=game_round_2')
    t.expect.fired('=game_round_2', activator='world', count=1, after='lost1')
    t.expect.fired('=game_round_3', activator='world', count=1, after='lost2')
    t.expect.not_fired('=game_round_4')
    t.expect.fired('=game_mission_failed', activator='world', count=1, after='lost3', within=(0, 0.5))
    t.expect.not_fired('=game_mission_success')
