"""Tier 5, batch 4: more FGD items of the Featureful guide entities that no 1.38 map uses and no other test asserted
(docs/FEATUREFUL_GUIDE_ENTITIES.md): env_warpball's shake, sprite and volume keys, func_breakable_effect's other
materials, func_illusionary_toggle's Contents."""

from etest import Suite

SUITE = Suite('fgd_guide2', 'Tier 5: env_warpball keys, func_breakable_effect materials, func_illusionary_toggle',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
TE_BREAKMODEL = 108
BREAK_GLASS, BREAK_METAL, BREAK_FLESH, BREAK_WOOD, BREAK_CONCRETE = 0x01, 0x02, 0x04, 0x08, 0x40


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


# ---------------------------------------------------------------------------------------------- env_warpball
@S.test('env_warpball Shake Amplitude / Duration / Frequency / Radius: the player in the radius gets that shake, '
        'one outside a smaller radius none', players=1, doc=DOC,
        covers=('env_warpball.amplitude', 'env_warpball.duration', 'env_warpball.frequency', 'env_warpball.radius'))
def warp_shake(t):
    t.point('env_warpball', targetname='near', at=(0, 0, 36), amplitude='4', duration='2', frequency='50',
            radius='300', spawnflags='16')
    t.point('env_warpball', targetname='far', at=(0, 0, 36), amplitude='9', duration='3', frequency='90',
            radius='60', spawnflags='16')
    t.teleport('player1', (150, 0, 0))
    t.wait(0.2)
    t.mark('go')
    t.fire('near')
    t.fire('far')
    t.wait(0.3)
    t.expect.msg('ScreenShake', to='player1', count=1, after='go',
                 fields={'amplitude': 4, 'duration': 2, 'frequency': 50})
    t.expect.msg('ScreenShake', fields={'amplitude': 9}, count=0, min=0)      # the far one: out of its radius


@S.test('env_warpball Sprite Name and Framerate: the sprite it shows', players=1, doc=DOC,
        covers=('env_warpball.model', 'env_warpball.framerate'))
def warp_sprite(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), model='sprites/xflare1.spr', framerate='7',
            spawnflags=str(4 | 16))
    t.wait(3.0)                     # the sprites of earlier tests have played out
    t.fire('wb')
    t.wait(0.05)
    t.snap('s', '@env_sprite', 'model', 'framerate')
    t.expect.spawned('@env_sprite', count=1)
    t.expect.value('s', '@env_sprite', 'model', 'sprites/xflare1.spr')
    t.expect.value('s', '@env_sprite', 'framerate', 7)
    t.expect.precached('sprites/xflare1.spr')


@S.test('env_warpball Volume 0.4: both sounds at that volume', players=1, doc=DOC, covers=('env_warpball.volume',))
def warp_volume(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), volume='0.4', spawnflags='4')
    t.fire('wb')
    t.wait(0.2)
    t.expect.sound('debris/beamstart2.wav', vol=(0.35, 0.45), count=1)
    t.expect.sound('debris/beamstart7.wav', vol=(0.35, 0.45), count=1)


# ---------------------------------------------------------------------------------------------- func_breakable_effect
# material -> (sound, gib flag)
MATERIALS = {3: ('debris/bustflesh', BREAK_FLESH), 4: ('debris/bustconcrete', BREAK_CONCRETE),
             5: ('debris/bustceiling', 0), 6: ('debris/bustmetal', BREAK_METAL),
             8: ('debris/bustconcrete', BREAK_CONCRETE)}


@S.test('func_breakable_effect Material 3 Flesh, 4 Cinder Block, 5 Ceiling Tile, 6 Computer, 8 Rocks: the '
        'material\'s break sound and gib type', doc=DOC,
        covers=tuple('func_breakable_effect.material=%d' % m for m in MATERIALS))
def effect_materials(t):
    for i, m in enumerate(MATERIALS):
        t.brush('func_breakable_effect', targetname='be%d' % m, size=(32, 32, 32), at=(-200 + i * 100, 0, 0),
                material=str(m))
        t.fire('be%d' % m)
    t.wait(0.2)
    for m, (snd, flag) in MATERIALS.items():
        t.expect.sound(snd, ent='be%d' % m, count=1)
        t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'flags': flag,
                                       'pos': [float(v) for v in t._abs((-200 + list(MATERIALS).index(m) * 100,
                                                                         0, 16))]}, count=1)


@S.test('func_breakable_effect Material 7 Unbreakable Glass: breaks with the glass sound and glass gibs', doc=DOC,
        covers=('func_breakable_effect.material=7',))
def effect_unbreakable_glass(t):
    t.brush('func_breakable_effect', targetname='be', size=(32, 32, 32), at=(0, 0, 0), material='7')
    t.fire('be')
    t.wait(0.2)
    t.expect.sound('debris/bustglass', ent='be', count=1)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'flags': BREAK_GLASS}, count=1)


# ---------------------------------------------------------------------------------------------- func_illusionary_toggle
@S.test('func_illusionary_toggle Contents -1 (Empty) / -7 (Volumetric Light)', doc=DOC,
        covers=('func_illusionary_toggle.skin', 'func_illusionary_toggle.skin=-1', 'func_illusionary_toggle.skin=-7'))
def illusionary_contents(t):
    t.brush('func_illusionary_toggle', targetname='e', size=(32, 32, 32), at=(-60, 0, 0), skin='-1')
    t.brush('func_illusionary_toggle', targetname='v', size=(32, 32, 32), at=(60, 0, 0), skin='-7')
    t.wait(0.1)
    t.snap('s', 'e', 'skin', 'solid')
    t.snap('s', 'v', 'skin', 'solid')
    t.expect.value('s', 'e', 'skin', -1)
    t.expect.value('s', 'v', 'skin', -7)
    t.expect.value('s', 'e', 'solid', 0)
    t.expect.value('s', 'v', 'solid', 0)


# ---------------------------------------------------------------------------------------------- env_decal
TE_BSPDECAL = 13
DECAL_GROUPS = (3, 4, 5, 7, 8, 9, 10, 11)


@S.test('env_decal Decal group 3-5, 7-11 with angles 90 0 0 (straight down): each sprays one decal on the floor '
        'below it', doc=DOC,
        covers=('env_decal.angles',) + tuple('env_decal.impulse=%d' % g for g in DECAL_GROUPS))
def decal_groups(t):
    for i, g in enumerate(DECAL_GROUPS):
        x = -210 + i * 60
        t.point('env_decal', targetname='d%d' % g, at=(x, 0, 64), angles='90 0 0', impulse=str(g))
        t.fire('d%d' % g)
    t.wait(0.2)
    for i, g in enumerate(DECAL_GROUPS):
        x = -210 + i * 60
        t.expect.msg('svc:23', fields={'te': TE_BSPDECAL, 'ent': 0, 'pos': [float(v) for v in t._abs((x, 0, 0))]},
                     count=1)


# ---------------------------------------------------------------------------------------------- monster_target
# CBaseMonster::IDefaultRelationship (monsters.cpp): R_AL -2, R_FR -1, R_NO 0, R_DL 1, R_HT 2, R_NM 3
REL = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, -2, 1, 1, -2, 1, 1, 1, 1, 1, 2, 1, 1, 1],
    [0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1],
    [0, -1, -1, -2, 0, -1, 0, 2, 1, -1, 2, -2, 0, 0],
    [0, -2, 2, 0, -2, 2, 1, 1, 1, 1, 2, 2, 0, 0],
    [0, 1, 2, 1, 2, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0],
    [0, 1, 3, 1, 1, 0, 0, 0, 0, 0, 2, 1, 0, 0],
    [0, 1, 1, 1, 1, 0, 0, 0, 0, -1, 2, 1, 0, 0],
    [0, 1, 3, 2, 2, 0, 0, 0, 2, 1, 2, 1, 0, 0],
    [2, 2, 2, 2, -1, 2, 2, 2, 2, 2, 2, 2, 2, 2],
    [0, 1, -2, -2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 0, 1],
    [0, 0, 1, 1, 1, -2, 0, 1, 1, 0, 2, 1, 1, 0],
]
TARGET_CLASSES = (0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13)
SENSOR_CLASS = 4            # human military


@S.test('monster_target Classification: each choice is its class for the AI (a trigger_sensor acting as '
        'human military senses it as an enemy exactly when the class table says so)', doc=DOC,
        covers=tuple('monster_target.frags=%d' % c for c in TARGET_CLASSES))
def target_classes(t):
    for i, c in enumerate(TARGET_CLASSES):
        x, y = -220 + (i % 6) * 88, -100 + (i // 6) * 200
        t.point('monster_target', targetname='mt%d' % c, at=(x, y, 40), frags=str(c))
        t.point('info_target', targetname='h%d' % c, at=(x, y + 60, 40))
        t.point('trigger_sensor', targetname='s%d' % c, at=(x, y, 40), target='h%d' % c, radius='30',
                sensed_classname='monster_target', line_blocked='0', trace_to='0', fire_trigger_policy='0',
                alive_filter='0', takedamage_filter='0', classify=str(SENSOR_CLASS), relationship_filter='2')
        t.fire('s%d' % c)
    t.wait(0.2)
    for c in TARGET_CLASSES:
        enemy = REL[SENSOR_CLASS][c] >= 1 or REL[c][SENSOR_CLASS] >= 1
        t.expect.fired('h%d' % c, activator='mt%d' % c, count=1 if enemy else 0)


@S.test('monster_target Classification left out: Player ally (11), the FGD default; an explicit 0 stays '
        'Ignored', doc=DOC, covers=('monster_target.frags',))
def target_class_default(t):
    t.point('monster_target', targetname='mt_def', at=(-100, 0, 40))
    t.point('monster_target', targetname='mt_zero', at=(100, 0, 40), frags='0')
    t.wait(0.2)
    t.snap('s', 'mt_def', 'frags')
    t.snap('s', 'mt_zero', 'frags')
    t.expect.value('s', 'mt_def', 'frags', 11)
    t.expect.value('s', 'mt_zero', 'frags', 0)


@S.test('monster_target count as Player (2): an automatic func_tank shoots at it; Ignored (0): it does not', doc=DOC,
        covers=('monster_target.frags=2', 'monster_target.frags=0'))
def target_player(t):
    t.brush('func_tank', size=(32, 32, 32), at=(0, 150, 64), origin=True, targetname='tank', target='shot',
            spawnflags='1', yawrate='300', yawrange='180', yawtolerance='15', pitchrate='300', pitchrange='60',
            pitchtolerance='15', firerate='5', bullet='1', barrel='16', persistence='1', angles='0 270 0')
    info(t, 'shot')
    t.point('monster_target', targetname='ign', at=(0, -150, 40), frags='0')
    t.point('monster_target', targetname='ply', at=(0, -150, 40), frags='2', spawnflags='1')
    t.wait(2.5)
    t.mark('on')
    t.fire('ply', use='on')
    t.wait(3.0)
    t.expect.not_used('shot', within=(0, 2.4))
    t.expect.used('shot', after='on', min=2)
