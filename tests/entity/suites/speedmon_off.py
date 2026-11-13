"""mp_speedmon -1: the map's monstermakers make nothing (no maker without a name, no Start ON maker, no
maker turned on, no cyclic maker fired); monsters placed in the map spawn; a Game Master's monsters spawn (GM mode
does not use monstermakers). Changes mid-map: suite speedmon_runtime. The suite's game.cfg sets mp_speedmon -1."""

from etest import Suite

SUITE = Suite('speedmon_off', 'mp_speedmon -1: the map\'s monstermakers make nothing', players=2,
              requires=('GmMode_RoundStart',))
# game.cfg runs before the map's entities spawn; server.cfg runs after the first frames, and on the command
# line '+mp_speedmon -1' loses its '-1'
SUITE.add_file('game.cfg', b'mp_speedmon -1\n')
S = SUITE

GM_ENTRY_DEMON_SLAVE = '3'      # game_shared/gm_catalog.h: monster_hunter


def maker(t, name, kid, at=(0, 0, 0), **kv):
    keys = dict(monstertype='monster_scientist', monstercount='3', delay='0.5', netname=kid)
    keys.update(kv)
    if name:
        keys['targetname'] = name
    t.point('monstermaker', at=at, angles='0 0 0', names=('netname', 'killtarget'), **keys)


@S.test('mp_speedmon -1 from the map start: a maker without a name and a Start ON maker make nothing; a monster '
        'placed in the map spawns', players=2)
def map_start(t):
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player2', '+FL_NOTARGET')
    maker(t, None, 'kid', at=(-150, 100, 0))
    t.console('mp_speedmon')          # the engine prints the value game.cfg set
    t.expect.console('"mp_speedmon" is "-1"')
    maker(t, 'mk_on', 'kid_on', at=(0, 100, 0), spawnflags='1')
    t.point('monster_scientist', targetname='mapmon', at=(150, 100, 0), angles='0 270 0')
    t.wait(3.0)
    t.expect.absent('kid')          # the whole run, the map load included
    t.expect.absent('kid_on')
    t.expect.present('mapmon')


@S.test('mp_speedmon -1: a maker turned on and a cyclic maker fired three times make nothing', players=2)
def triggered(t):
    maker(t, 'mk', 'kid', at=(-100, 100, 0))
    maker(t, 'cyc', 'kid_cyc', at=(100, 100, 0), spawnflags='4')
    t.wait(0.2)
    t.fire('mk', use='on')
    for _ in range(3):
        t.fire('cyc')
        t.wait(0.4)
    t.wait(2.0)
    t.expect.absent('kid')
    t.expect.absent('kid_cyc')


def look(t, pitch, yaw):
    t.look('player1', pitch, yaw)
    t.move('player1', 0, 0, 3)
    t.wait(0.2)


@S.test('mp_speedmon -1: a Game Master places a monster (GM mode does not use monstermakers)', players=2,
        doc='GM_MODE.md')
def gm_spawn(t):
    t.console('mp_gmmode 1')
    t.wait(1.0)
    t.flags('player2', '+FL_NOTARGET')
    t.teleport('player2', (-220, -220, 0))
    t.cmd('player1', 'gmon')
    t.wait(0.3)
    t.cmd('player1', 'gm')
    t.wait(0.3)
    t.teleport('player1', (-200, 0, 28), yaw=0)
    look(t, 30, 0)
    t.cmd('player1', 'gm_pick', GM_ENTRY_DEMON_SLAVE)
    t.wait(0.3)
    t.mark('place')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.3)
    t.move('player1', 0, 0, 3)
    t.wait(0.2)
    t.cmd('player1', 'gm_cancel')
    t.wait(1.0)
    t.expect.spawned('@monster_hunter', min=1, after='place')
    t.cmd('player1', 'gm_leave')
    t.wait(0.3)
    t.console('mp_gmmode 0')

