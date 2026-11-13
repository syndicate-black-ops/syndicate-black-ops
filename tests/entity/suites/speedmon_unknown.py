"""mp_speedmon outside -1..3 counts as 1 (normal speed). 1.38 set no next think for such a value: a maker without a
name never made a monster, a named one made one and stopped. The suite's game.cfg sets mp_speedmon 7."""

from etest import Suite

SUITE = Suite('speedmon_unknown', 'mp_speedmon 7 counts as 1', players=1)
# game.cfg runs before the map's entities spawn; server.cfg runs after the first frames, and on the command
# line '+mp_speedmon -1' loses its '-1'
SUITE.add_file('game.cfg', b'mp_speedmon 7\n')
S = SUITE


def maker(t, name, kid, at=(0, 0, 0), **kv):
    keys = dict(monstertype='monster_scientist', monstercount='3', netname=kid)
    keys.update(kv)
    if name:
        keys['targetname'] = name
    t.point('monstermaker', at=at, angles='0 0 0', names=('netname',), **keys)


@S.test('mp_speedmon 7: a maker without a name makes monsters from the map start', players=1)
def unnamed(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, None, 'kid', at=(0, 100, 0), delay='0.5')
    t.console('mp_speedmon')          # the engine prints the value game.cfg set
    t.expect.console('"mp_speedmon" is "7"')
    t.wait(1.0)
    t.expect.present('kid')


@S.test('mp_speedmon 7: a maker turned on makes its next monster Frequency (1 s) after the first, as 1 (0: 2 s, '
        '2: 0.5 s, 3: 0.33 s)', players=1)
def normal_speed(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mk', 'kid', at=(0, 100, 0), delay='1')
    t.point('trigger_relay', targetname='kill', killtarget='kid', names=('killtarget',))
    t.wait(0.2)
    t.fire('mk', use='on')
    t.mark('on')
    t.wait(0.25)
    t.fire('kill')          # the spot is free for the next one
    t.wait(1.5)
    t.expect.spawned('kid', count=1, after='on', within=(0, 0.2))
    t.expect.spawned('kid', count=0, after='on', within=(0.3, 0.85))
    t.expect.spawned('kid', count=1, after='on', within=(0.85, 1.2))
