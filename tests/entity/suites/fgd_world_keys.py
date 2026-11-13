"""Tier 5, final pass: worldspawn keys of this suite's map (its own suite: the keys are the map's). The grass keys
reach each player in GrassInfo when he joins (sound.cpp TEXTURETYPE_SendGrassInfo, CS:CZ Deleted Scenes' message;
the client draws it), "materials_file" is loaded at the map start and named to the players in MatFile (sound.cpp
TEXTURETYPE_Init; a test-only file in the run's game folder), and "freeroam" No (0) leaves idle monsters with
Freeroam Map default standing (CBaseMonster::GetFreeroamSchedule)."""

from etest import Suite

SUITE = Suite('fgd_world_keys', 'Tier 5: worldspawn grass, materials file, freeroam', players=0)
S = SUITE
DOC = 'MATERIALS.md'
MATFILE = 'materials/maps/et_world_keys.txt'
SUITE.add_file(MATFILE, b'// test-only materials file\nM 666MURMETAL\n')
SUITE.world_keys = {'grassDensity': '200', 'grassFadeDist': '600', 'grassMinHeight': '10', 'grassMaxHeight': '40',
                    'grassSprite': 'sprites/grass_03.spr', 'materials_file': MATFILE, 'freeroam': '0'}


@S.test('worldspawn grass keys and "materials_file": a player who joins gets GrassInfo with the sprite, fade '
        'distance, heights and density the map sets, and MatFile naming the materials file the map loaded',
        doc=DOC, covers=('worldspawn.grassdensity', 'worldspawn.grassfadedist', 'worldspawn.grassminheight',
                         'worldspawn.grassmaxheight', 'worldspawn.grasssprite', 'worldspawn.materials_file'))
def grass_and_materials(t):
    # the suite's player joins here, so his join messages fall in this test's window (as suite chaptertitle)
    t.mark('join')
    t.console('test_player add wk1')
    t.wait_frames(2)
    t.console('test_join player1')
    t.wait(1.0)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.duration += 20
    t.wait(0.5)
    t.expect.msg('GrassInfo', fields={'sprite': 'sprites/grass_03.spr', 'fade': 600, 'min': 10,
                                                    'max': 40, 'density': 200}, min=1, after='join')
    t.expect.msg('MatFile', fields={'file': MATFILE}, min=1, after='join')
    t.expect.no_console('materials: worldspawn materials_file')


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


@S.test('worldspawn "freeroam" No (0): an idle scientist with Freeroam Map default stands; one with Always walks '
        'from node to node (the control)', players=1, doc='FEATUREFUL_OPTIONS.md',
        covers=('worldspawn', 'worldspawn.freeroam=0'))
def freeroam_off(t):
    node_grid(t)
    t.point('monster_scientist', targetname='stay', at=(150, 150, 0), angles='0 270 0', spawnflags='2', freeroam='0')
    t.point('monster_scientist', targetname='roam', at=(-150, -150, 0), angles='0 90 0', spawnflags='2', freeroam='2')
    t.point('info_target', targetname='stay0', at=(150, 150, 0))
    t.point('info_target', targetname='roam0', at=(-150, -150, 0))
    t.teleport('player1', (200, -200, 0))
    t.wait(11.0)
    for n in ('roam', 'stay', 'roam0', 'stay0'):
        t.snap('b', n, 'origin')
    t.expect.distance('b', 'roam', 'roam0', 48, 1000)
    t.expect.distance('b', 'stay', 'stay0', 0, 16)
