"""mp_speedmon changed mid-map: -1 -> 1, a maker turned on while -1 makes monsters; 1 -> -1 stops it at its next child;
-1 -> 1 again resumes it. No restart. The suite's game.cfg sets mp_speedmon -1."""

from etest import Suite

SUITE = Suite('speedmon_runtime', 'mp_speedmon -1 / 1 changed mid-map', players=1)
# game.cfg runs before the map's entities spawn; server.cfg runs after the first frames, and on the command
# line '+mp_speedmon -1' loses its '-1'
SUITE.add_file('game.cfg', b'mp_speedmon -1\n')
S = SUITE


def maker(t, name, kid, at=(0, 0, 0), **kv):
    keys = dict(monstertype='monster_scientist', monstercount='3', delay='0.5', netname=kid, targetname=name)
    keys.update(kv)
    t.point('monstermaker', at=at, angles='0 0 0', names=('netname',), **keys)


@S.test('mp_speedmon -1 -> 1 mid-map: a maker turned on while -1 makes monsters; 1 -> -1 stops it at its next child; '
        '-1 -> 1 again resumes it', players=1)
def runtime(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mk', 'kid', at=(0, 100, 0), monstercount='10', delay='0.5')
    t.console('mp_speedmon')          # the engine prints the value game.cfg set
    t.expect.console('"mp_speedmon" is "-1"')
    t.point('trigger_relay', targetname='kill', killtarget='kid', names=('killtarget',))
    t.wait(0.2)
    t.fire('mk', use='on')
    t.mark('on_off')
    t.wait(1.5)
    t.expect.spawned('kid', count=0, after='on_off', within=(0, 1.5))
    t.console('mp_speedmon 1')
    t.mark('resume')
    t.wait(1.0)
    t.expect.spawned('kid', min=1, after='resume')
    t.console('mp_speedmon -1')
    t.mark('stop')
    t.fire('kill')         # the spot is free: only mp_speedmon keeps the maker from making the next one
    t.wait(2.0)
    t.expect.spawned('kid', count=0, after='stop', within=(0.2, 2.0))
    t.console('mp_speedmon 1')
    t.mark('again')
    t.wait(1.0)
    t.expect.spawned('kid', min=1, after='again')
    t.console('mp_speedmon -1')
