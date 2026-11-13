"""Tier 5, final pass: func_guntarget's path, speed, damage and target to fire (plats.cpp CGunTarget), and
game_player_settings "longjump" Remove / No change / Give (maprules.cpp: the player's long jump module, the physics
key "slj" pm_shared reads)."""

from etest import Suite

SUITE = Suite('fgd_misc_more', 'Tier 5: func_guntarget keys, game_player_settings longjump', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'


@S.test('func_guntarget "First stop target", "Speed" 200, "Damage to Take" 10, "Fire on damage": it waits on its first '
        'path_corner, triggered it moves to the next at 200 units a second; a 4 hit does not stop it, the next one '
        'that takes its last health stops it and fires the target',
        players=1, doc=DOC, covers=('func_guntarget.target', 'func_guntarget.speed', 'func_guntarget.health',
                                     'func_guntarget.message'))
def guntarget(t):
    t.point('path_corner', targetname='p1', at=(150, 0, 40), target='p2')
    t.point('path_corner', targetname='p2', at=(-150, 0, 40), target='p1')
    t.brush('func_guntarget', targetname='gt', size=(32, 8, 32), at=(0, 150, 40), origin=True, target='p1',
            speed='200', health='10', message='hit')
    t.point('info_target', targetname='hit')
    t.flags('player1', '+FL_NOTARGET')
    t.wait(0.5)
    t.snap('start', 'gt', 'origin')
    t.snap('start', 'p1', 'origin')
    t.fire('gt', use='on')
    t.wait(0.5)
    t.snap('mid', 'gt', 'origin')
    t.mark('shot')
    t.damage('gt', 4, 2, 'player1')         # (CGunTarget's own TakeDamage: no random hits)
    t.wait(0.1)
    t.damage('gt', 100, 2, 'player1')
    t.wait(0.5)
    t.snap('a', 'gt', 'origin')
    t.wait(0.5)
    t.snap('b', 'gt', 'origin')
    t.flags('player1', '-FL_NOTARGET')
    # the brush's center on p1 (its origin brush is its center)
    t.expect.distance('start', 'gt', 'p1', 0, 2, flat=False)
    t.expect.moved('start', 'mid', 'gt', delta=(-100, 0, 0), tol=20)
    t.expect.fired('hit', count=1, after='shot')
    t.expect.moved('a', 'b', 'gt', delta=(0, 0, 0), tol=1)


@S.test('game_player_settings "longjump": Give (1) gives the player the long jump module (physics key slj 1), No '
        'change (0) leaves it, Remove (-1) takes it', players=1, doc=DOC,
        covers=('game_player_settings.longjump',) + tuple('game_player_settings.longjump=%d' % v for v in (-1, 0, 1)))
def longjump(t):
    for name, v in (('give', '1'), ('keep', '0'), ('rm', '-1')):
        t.point('game_player_settings', targetname=name, longjump=v)
    t.fire('rm', activator='player1')
    t.wait(0.2)
    t.snap('s0', 'player1', 'physinfo:slj')
    t.fire('give', activator='player1')
    t.wait(0.2)
    t.snap('s1', 'player1', 'physinfo:slj')
    t.fire('keep', activator='player1')
    t.wait(0.2)
    t.snap('s2', 'player1', 'physinfo:slj')
    t.fire('rm', activator='player1')
    t.wait(0.2)
    t.snap('s3', 'player1', 'physinfo:slj')
    t.fire('keep', activator='player1')
    t.wait(0.2)
    t.snap('s4', 'player1', 'physinfo:slj')
    for label, want in (('s1', '1'), ('s2', '1'), ('s3', '0')):
        t.expect.value(label, 'player1', 'physinfo:slj', want)
    t.expect.value('s4', 'player1', 'physinfo:slj', '1', op='!=')


@S.test('env_warpball "Sprite 2 Name" with its framerate, brightness, color and scale: a second sprite plays with the '
        'first, drawn with those', players=1, doc=DOC,
        covers=('env_warpball.model2', 'env_warpball.framerate2', 'env_warpball.renderamt2',
                'env_warpball.rendercolor2', 'env_warpball.scale2'))
def warpball_sprite2(t):
    # Quiet (4), No shake (16)
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), model='sprites/xflare1.spr', framerate='7',
            renderamt='255', rendercolor='255 255 255', scale='1', model2='sprites/fire.spr', framerate2='9',
            renderamt2='120', rendercolor2='10 20 30', scale2='2.5', spawnflags=str(4 | 16))
    t.wait(3.0)                     # the sprites of earlier tests have played out
    t.fire('wb')
    t.wait(0.05)
    t.snap('s', '@env_sprite', 'model', 'framerate', 'renderamt', 'rendercolor', 'scale')
    w = {'model': 'sprites/fire.spr'}     # (an animated sprite: a one-frame one is gone at once)
    t.expect.spawned('@env_sprite', count=2)
    t.expect.value('s', '@env_sprite', 'framerate', 9, where=w)
    t.expect.value('s', '@env_sprite', 'renderamt', 120, where=w)
    t.expect.value('s', '@env_sprite', 'rendercolor', [10, 20, 30], where=w)
    t.expect.value('s', '@env_sprite', 'scale', 2.5, where=w)
    t.expect.value('s', '@env_sprite', 'framerate', 7, where={'model': 'sprites/xflare1.spr'})


@S.test('cycler "Animation sequence" 3: it stands still on sequence 3 (framerate 0); 0: it animates', players=1,
        doc=DOC, covers=('cycler.sequence',))
def cycler_sequence(t):
    t.point('cycler', targetname='c3', at=(-80, 100, 0), model='models/scientist.mdl', sequence='3')
    t.point('cycler', targetname='c0', at=(80, 100, 0), model='models/scientist.mdl', sequence='0')
    t.wait(1.5)
    t.snap('s', 'c3', 'sequence', 'framerate')
    t.snap('s', 'c0', 'sequence', 'framerate')
    t.expect.value('s', 'c3', 'sequence', 3)
    t.expect.value('s', 'c3', 'framerate', 0)
    t.expect.value('s', 'c0', 'framerate', 0, op='!=')
