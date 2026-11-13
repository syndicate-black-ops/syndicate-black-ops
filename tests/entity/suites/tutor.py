"""env_tutor "Show on player spawn" + "Repeatable" (docs/TUTOR_PORT.md): a respawn within the holdtime sends the
message again, so its sound plays again (playtest 2026-10-17: silent at a respawn). Its own suite: a "Show on
player spawn" message is shown at every spawn of the suite."""

from etest import Suite

SUITE = Suite('tutor', 'env_tutor: shown again at a respawn', players=1)
S = SUITE


@S.test('"Show on player spawn", repeatable, holdtime 10: each respawn within it sends the TutorText again',
        players=1, doc='TUTOR_PORT.md', covers=('env_tutor', 'env_tutor:4', 'env_tutor:8', 'env_tutor.holdtime'))
def respawn_again(t):
    t.point('env_tutor', targetname='tut', spawnflags='12', message='Entity test spawn tutor', type='8', holdtime='10')
    t.mark('go')
    t.console('test_spawn player1')
    t.wait(1.0)
    t.console('test_spawn player1')
    t.wait(1.0)
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('TutorText', to='player1', min=3, after='go')
    t.expect.no_msg('TutorClose', to='player1', after='go')
