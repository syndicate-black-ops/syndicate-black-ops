"""Entities from the Featureful entity guide (feat/featureful-guide-entities, docs/FEATUREFUL_GUIDE_ENTITIES.md):
every key and flag of each new entity, shown working. Positions in temp entities are compared in map coordinates
(t._abs: the room's floor center plus an offset)."""

from etest import Suite

SUITE = Suite('guide', 'Featureful entity guide: effects, trains, player, trigger, monster and script entities',
              players=2)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'

TE_BEAMPOINTS, TE_TAREXPLOSION, TE_LAVASPLASH, TE_TELEPORT, TE_EXPLOSION2 = 0, 4, 10, 11, 12
TE_BSPDECAL, TE_IMPLOSION, TE_BEAMTORUS, TE_BEAMDISK, TE_BEAMCYLINDER = 13, 14, 19, 20, 21
TE_BEAMFOLLOW, TE_DLIGHT, TE_KILLBEAM, TE_BREAKMODEL, TE_PARTICLEBURST = 22, 27, 99, 108, 122
BREAK_GLASS, BREAK_METAL, BREAK_SMOKE, BREAK_TRANS = 0x01, 0x02, 0x10, 0x20
LGTNING = 'model:sprites/lgtning.spr'


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def pos(t, at):
    return [float(v) for v in t._abs(at)]


def tough(t, player='player1', hp='1000'):
    t.kv(player, 'health', hp)
    t.kv(player, 'max_health', hp)


# ---------------------------------------------------------------------------------------------- env_warpball
@S.test('env_warpball defaults: both stock sounds, up to 20 green lightning beams, the shake, a sprite; fires its target',
        players=1, doc=DOC, covers=('env_warpball', 'env_warpball.target'))
def warpball_defaults(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), target='after')
    info(t, 'after')
    t.teleport('player1', (100, 0, 0))
    t.fire('wb', activator='player1')
    t.wait(0.3)
    t.expect.sound('debris/beamstart2.wav')
    t.expect.sound('debris/beamstart7.wav')
    t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS, 'model': LGTNING, 'color': [20, 243, 20], 'width': 30,
                                   'noise': 65, 'alpha': 220, 'pos': pos(t, (0, 0, 64))}, min=5, max=20)
    # 10-20 picked; beams that hit no wall within the radius are redrawn at half the radius, at most half of them
    t.expect.msg('ScreenShake', to='player1', min=1)
    t.expect.spawned('@env_sprite', count=1)
    t.expect.used('after', caller='wb', count=1)
    t.expect.msg('svc:23', fields={'te': TE_DLIGHT}, count=0, min=0)


@S.test('env_warpball keys: beam color and count, sprite 2 with its own color, scale; warp position [LP]',
        players=1, doc=DOC,
        covers=('env_warpball', 'env_warpball.beamcolor', 'env_warpball.beamcount', 'env_warpball.warp_target',
                'env_warpball.rendercolor', 'env_warpball.renderamt', 'env_warpball.scale', 'env_warpball.model2'))
def warpball_keys(t):
    info(t, 'spot', at=(-100, 50, 40))
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), beamcolor='255 0 0', beamcount='4', warp_target='spot',
            rendercolor='10 20 30', renderamt='200', scale='2', framerate='20', model2='sprites/xflare1.spr',
            rendercolor2='200 100 50', renderamt2='120', scale2='0.5', framerate2='5', radius='300',
            names=('warp_target',))
    t.wait(3.0)                     # the sprites of the test before have played out
    t.mark('go')
    t.fire('wb')
    t.wait(0.05)
    t.snap('s', '@env_sprite', 'rendercolor', 'renderamt', 'scale', 'framerate')
    t.wait(0.3)
    t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS, 'color': [255, 0, 0], 'pos': pos(t, (-100, 50, 40))},
                 min=2, max=4)
    t.expect.spawned('@env_sprite', count=2)
    t.expect.value('s', '@env_sprite', 'rendercolor', [10, 20, 30])
    t.expect.value('s', '@env_sprite', 'renderamt', 200)
    t.expect.value('s', '@env_sprite', 'scale', 2)


@S.test('env_warpball flags: No screen shake (4), Dynamic light (8), No sounds (16), Remove on fire (1)', players=1,
        doc=DOC,
        covers=('env_warpball', 'env_warpball:1', 'env_warpball:4', 'env_warpball:8', 'env_warpball:16',
                'env_warpball.rendercolor'))
def warpball_flags(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), spawnflags=str(1 | 4 | 8 | 16), rendercolor='0 0 255')
    t.teleport('player1', (100, 0, 0))
    t.fire('wb', activator='player1')
    t.wait(0.3)
    t.expect.msg('ScreenShake', count=0, min=0)
    t.expect.msg('svc:23', fields={'te': TE_DLIGHT, 'color': [0, 0, 255], 'radius': 20}, count=1)
    t.expect.sound('beamstart', count=0, min=0)
    t.expect.removed('wb')


@S.test('env_warpball custom sounds (noise1, noise2) and volume', players=1, doc=DOC,
        covers=('env_warpball', 'env_warpball.noise1', 'env_warpball.noise2'))
def warpball_sounds(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), noise1='buttons/blip1.wav', noise2='buttons/blip2.wav',
            volume='0.5')
    t.point('env_warpball', targetname='wb1', at=(0, 0, 64), noise1='buttons/button3.wav')
    t.fire('wb')
    t.fire('wb1')
    t.wait(0.2)
    t.expect.sound('buttons/blip1.wav', count=1)
    t.expect.sound('buttons/blip2.wav', count=1)
    t.expect.sound('buttons/button3.wav', count=1)
    t.expect.sound('beamstart', count=0, min=0)       # sound 1 set: no stock second sound


@S.test('env_warpball Kill center (2): 300 shock damage at the center at once; Damage delay waits', players=2,
        doc=DOC, covers=('env_warpball', 'env_warpball:2', 'env_warpball.damage_delay', 'env_warpball.warp_target'))
def warpball_kill(t):
    tough(t, 'player1')
    tough(t, 'player2')
    info(t, 'p2spot', at=(150, 0, 36))
    t.point('env_warpball', targetname='wb', at=(-150, 0, 36), spawnflags='2')
    t.point('env_warpball', targetname='wb2', warp_target='p2spot', spawnflags='2', damage_delay='0.6',
            names=('warp_target',))
    t.teleport('player1', (-150, 0, 0))
    t.teleport('player2', (150, 0, 0))
    t.wait(0.1)
    t.mark('go')
    t.fire('wb')
    t.fire('wb2')
    t.wait(1.0)
    t.expect.damaged('player1', within=(0, 0.2), after='go')
    t.expect.not_damaged('player2', within=(0, 0.4), after='go')
    t.expect.damaged('player2', within=(0.5, 0.9), after='go')
    t.expect.spawned('@warpball_hurt', count=1)


@S.test('monstermaker Warpball: the named env_warpball plays at each child (sound on the maker), without its target',
        players=1, doc=DOC, covers=('env_warpball', 'monstermaker', 'monstermaker.warpball'))
def warpball_maker(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 200), target='never', spawnflags='2')
    info(t, 'never')
    t.point('monstermaker', targetname='mk', at=(0, 100, 8), monstertype='monster_turret', monstercount='1',
            warpball='wb', impulse='-1', spawnflags='4')
    t.fire('mk')
    t.wait(0.5)
    t.expect.spawned('@monster_turret', count=1)
    t.expect.msg('svc:23', fields={'te': TE_BEAMPOINTS, 'pos': pos(t, (0, 100, 8))}, min=5)
    t.expect.sound('debris/beamstart2.wav', ent='mk')
    t.expect.not_used('never')
    t.expect.not_damaged('@monster_turret')


# ---------------------------------------------------------------------------------------------- env_shockwave
@S.test('env_shockwave defaults: a cylinder of shockwave.spr, raised by half its height, removed after one use',
        doc=DOC,
        covers=('env_shockwave', 'env_shockwave.m_iHeight', 'env_shockwave.m_iRadius', 'env_shockwave.m_iTime',
                'env_shockwave.rendercolor', 'env_shockwave.renderamt'))
def shockwave_defaults(t):
    t.point('env_shockwave', targetname='sw', at=(0, 0, 16), m_iHeight='32', m_iRadius='300', m_iTime='5',
            rendercolor='188 220 255', renderamt='255')
    t.fire('sw')
    t.wait(0.2)
    t.fire('sw')
    t.wait(0.2)
    t.expect.msg('svc:23', fields={'te': TE_BEAMCYLINDER, 'model': 'model:sprites/shockwave.spr',
                                   'pos': pos(t, (0, 0, 32)), 'radius': 300, 'life': 5, 'width': 16,
                                   'color': [188, 220, 255], 'alpha': 255}, count=1)
    t.expect.removed('sw')


@S.test('env_shockwave keys: Centered (1), Repeatable (2), torus and disk, sprite, color, frames, noise, scroll, '
        'position [LP]', doc=DOC,
        covers=('env_shockwave', 'env_shockwave:1', 'env_shockwave:2', 'env_shockwave.m_cType=19',
                'env_shockwave.m_cType=20', 'env_shockwave.netname', 'env_shockwave.rendercolor',
                'env_shockwave.renderamt', 'env_shockwave.m_iFrameRate', 'env_shockwave.m_iStartFrame',
                'env_shockwave.m_iNoise', 'env_shockwave.m_iScrollRate', 'env_shockwave.m_iszPosition',
                'env_shockwave.m_iHeight'))
def shockwave_keys(t):
    info(t, 'spot', at=(50, 50, 80))
    t.point('env_shockwave', targetname='sw', at=(0, 0, 16), spawnflags='3', m_cType='19', netname='sprites/laserbeam.spr',
            rendercolor='255 0 0', renderamt='100', m_iFrameRate='7', m_iStartFrame='2', m_iNoise='9',
            m_iScrollRate='4', m_iszPosition='spot', m_iHeight='20', names=('m_iszPosition',))
    t.point('env_shockwave', targetname='disk', at=(0, 0, 16), spawnflags='3', m_cType='20')
    t.fire('sw')
    t.wait(0.1)
    t.fire('sw')
    t.fire('disk')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BEAMTORUS, 'model': 'model:sprites/laserbeam.spr',
                                   'pos': pos(t, (50, 50, 80)), 'color': [255, 0, 0], 'alpha': 100, 'rate': 7,
                                   'frame': 2, 'noise': 9, 'speed': 4, 'width': 10}, count=2)
    t.expect.msg('svc:23', fields={'te': TE_BEAMDISK, 'pos': pos(t, (0, 0, 16))}, count=1)


# ---------------------------------------------------------------------------------------------- env_beamtrail
@S.test('env_beamtrail: On sends a trail on its target (sprite, life, width, color), Off kills its beams, Start off '
        '(1) waits for a trigger', doc=DOC,
        covers=('env_beamtrail', 'env_beamtrail:1', 'env_beamtrail.target', 'env_beamtrail.netname',
                'env_beamtrail.health', 'env_beamtrail.armorvalue', 'env_beamtrail.rendercolor',
                'env_beamtrail.renderamt'))
def beamtrail(t):
    t.point('info_target', targetname='mover', at=(0, 0, 64))
    t.point('env_beamtrail', targetname='bt', at=(0, 0, 32), target='mover', spawnflags='1', netname='sprites/laserbeam.spr',
            health='2.5', armorvalue='7', rendercolor='0 255 0', renderamt='180')
    t.wait(0.3)
    t.expect.msg('svc:23', fields={'te': TE_BEAMFOLLOW}, count=0, min=0, within=(0, 0.25))
    t.mark('on')
    t.fire('bt', use='on')
    t.wait(0.2)
    t.mark('off')
    t.fire('bt', use='off')
    t.wait(0.2)
    t.expect.msg('svc:23', fields={'te': TE_BEAMFOLLOW, 'model': 'model:sprites/laserbeam.spr', 'life': 25,
                                   'width': 7, 'color': [0, 255, 0], 'alpha': 180}, count=1, after='on')
    t.expect.msg('svc:23', fields={'te': TE_KILLBEAM}, count=1, after='off')


@S.test('env_beamtrail without a target trails itself; Toggle follows the state', doc=DOC, covers=('env_beamtrail',))
def beamtrail_self(t):
    t.point('env_beamtrail', targetname='bt', at=(0, 0, 32), spawnflags='1')
    t.fire('bt')
    t.wait(0.1)
    t.mark('second')
    t.fire('bt')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BEAMFOLLOW, 'model': 'model:sprites/smoke.spr'}, count=1)
    t.expect.msg('svc:23', fields={'te': TE_KILLBEAM}, count=1, after='second')


# ---------------------------------------------------------------------------------------------- env_quakefx
@S.test('env_quakefx: each effect type with its keys; removed after one use unless Repeatable (1)', doc=DOC,
        covers=('env_quakefx', 'env_quakefx:1', 'env_quakefx.impulse=4', 'env_quakefx.impulse=10',
                'env_quakefx.impulse=11', 'env_quakefx.impulse=12', 'env_quakefx.impulse=14', 'env_quakefx.impulse=122',
                'env_quakefx.armortype', 'env_quakefx.armorvalue', 'env_quakefx.health', 'env_quakefx.message',
                'env_quakefx.frags'))
def quakefx(t):
    info(t, 'spot', at=(40, 40, 40))
    for n, imp in (('tar', 4), ('lava', 10), ('tele', 11), ('expl', 12)):
        t.point('env_quakefx', targetname=n, at=(0, 0, 32), impulse=str(imp))
    t.point('env_quakefx', targetname='imp', at=(0, 0, 32), impulse='14', armortype='120', armorvalue='30',
            health='2', spawnflags='1')
    t.point('env_quakefx', targetname='burst', message='spot', impulse='122', armortype='300', frags='208',
            health='1.5', names=('message',))
    for n in ('tar', 'lava', 'tele', 'expl', 'imp', 'burst'):
        t.fire(n)
    t.wait(0.1)
    t.fire('imp')
    t.fire('tar')
    t.wait(0.1)
    for te in (TE_TAREXPLOSION, TE_LAVASPLASH, TE_TELEPORT, TE_EXPLOSION2):
        t.expect.msg('svc:23', fields={'te': te, 'pos': pos(t, (0, 0, 32))}, count=1)
    t.expect.msg('svc:23', fields={'te': TE_IMPLOSION, 'radius': 120, 'count': 30, 'duration': 20}, count=2)
    t.expect.msg('svc:23', fields={'te': TE_PARTICLEBURST, 'pos': pos(t, (40, 40, 40)), 'radius': 300, 'color': 208,
                                   'duration': 15}, count=1)
    t.expect.removed('tar')


# ---------------------------------------------------------------------------------------------- env_decal
@S.test('env_decal: decal groups and a custom texture sprayed where the line hits; direction from angles or [LV]; '
        'an unknown custom decal sprays nothing', doc=DOC,
        covers=('env_decal', 'env_decal.impulse=0', 'env_decal.impulse=1', 'env_decal.impulse=2', 'env_decal.impulse=6',
                'env_decal.noise', 'env_decal.target', 'env_decal.netname'))
def decal(t):
    info(t, 'spot', at=(0, 0, 100))
    t.point('env_decal', targetname='gun', at=(0, 0, 64), angles='0 0 0', impulse='1')
    t.point('env_decal', targetname='scorch', at=(0, 0, 64), angles='0 90 0', impulse='6')
    t.point('env_decal', targetname='custom', at=(0, 0, 64), angles='0 180 0', impulse='0', noise='{bigshot1')
    t.point('env_decal', targetname='bad', at=(0, 0, 64), impulse='0', noise='{no_such_decal_xx')
    t.point('env_decal', targetname='floor', target='spot', netname='0 0 -1', impulse='2', names=('target',))
    for n in ('gun', 'scorch', 'custom', 'bad', 'floor'):
        t.fire(n)
    t.wait(0.1)
    t.fire('gun')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BSPDECAL, 'ent': 0}, count=5)
    t.expect.msg('svc:23', fields={'te': TE_BSPDECAL, 'pos': pos(t, (0, 0, 0))}, count=1)    # the floor one


# ---------------------------------------------------------------------------------------------- func_breakable_effect
@S.test('func_breakable_effect: the material\'s sound and gibs with its flags; removed after one use', doc=DOC,
        covers=('func_breakable_effect', 'func_breakable_effect.material=0', 'func_breakable_effect:8192',
                'func_breakable_effect:16384', 'func_breakable_effect.m_iGibs'))
def breakeffect(t):
    t.brush('func_breakable_effect', targetname='be', size=(64, 64, 64), at=(0, 0, 32), material='0',
            spawnflags=str(8192 | 16384), m_iGibs='12')
    t.fire('be')
    t.wait(0.2)
    t.fire('be')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'model': 'model:models/glassgibs.mdl', 'count': 12,
                                   'flags': BREAK_GLASS | BREAK_SMOKE | BREAK_TRANS, 'pos': pos(t, (0, 0, 64)),
                                   'size': [66, 66, 66]}, count=1)     # a brush model's size: 1 unit more each side
    t.expect.sound('debris/bustglass')
    t.expect.removed('be')


@S.test('func_breakable_effect: Repeatable (1), At position [LP], gib model, no gibs (-1)', doc=DOC,
        covers=('func_breakable_effect', 'func_breakable_effect:1', 'func_breakable_effect.position',
                'func_breakable_effect.gibmodel', 'func_breakable_effect.m_iGibs', 'func_breakable_effect.material=1',
                'func_breakable_effect.material=2'))
def breakeffect_keys(t):
    info(t, 'spot', at=(-100, 0, 50))
    t.brush('func_breakable_effect', targetname='be', size=(32, 32, 32), at=(0, 0, 0), material='2', spawnflags='1',
            position='spot', gibmodel='models/woodgibs.mdl', names=('position',))
    t.brush('func_breakable_effect', targetname='none', size=(32, 32, 32), at=(100, 0, 0), material='1',
            m_iGibs='-1')
    t.fire('be')
    t.wait(0.1)
    t.fire('be')
    t.fire('none')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'model': 'model:models/woodgibs.mdl', 'count': 0,
                                   'flags': BREAK_METAL, 'pos': pos(t, (-100, 0, 50))}, count=2)
    t.expect.sound('debris/bustmetal', min=2)
    t.expect.sound('debris/bustcrate', count=1)
    t.expect.removed('none')


@S.test('func_breakable_effect Material 7 (Unbreakable Glass): breaks like Glass (glass sound, glass gibs); a '
        'func_breakable with Material 7 still never breaks', doc=DOC,
        covers=('func_breakable_effect', 'func_breakable_effect.material=7'))
def breakeffect_unbreakable_glass(t):
    t.brush('func_breakable_effect', targetname='be', size=(32, 32, 32), at=(0, 0, 0), material='7')
    t.brush('func_breakable', targetname='wall', size=(32, 32, 32), at=(100, 0, 0), material='7')
    t.fire('be')
    t.fire('wall')
    t.wait(0.2)
    t.expect.sound('debris/bustglass', ent='be', count=1)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'model': 'model:models/glassgibs.mdl',
                                   'flags': BREAK_GLASS}, count=1)
    t.expect.removed('be')
    t.expect.sound('debris/bust', ent='wall', count=0, min=0)
    t.expect.not_removed('wall')


# ---------------------------------------------------------------------------------------------- func_illusionary_toggle
@S.test('func_illusionary_toggle: not solid; Starts Invisible (1); On shows, Off hides, Toggle toggles', doc=DOC,
        covers=('func_illusionary_toggle', 'func_illusionary_toggle:1'))
def illusionary_toggle(t):
    t.brush('func_illusionary_toggle', targetname='holo', size=(64, 64, 64), at=(0, 0, 0), spawnflags='1')
    t.snap('start', 'holo', 'effects', 'solid')
    t.fire('holo', use='on')
    t.wait(0.05)
    t.snap('on', 'holo', 'effects', 'solid')
    t.fire('holo', use='on')
    t.wait(0.05)
    t.snap('on2', 'holo', 'effects')
    t.fire('holo', use='toggle')
    t.wait(0.05)
    t.snap('off', 'holo', 'effects')
    t.expect.value('start', 'holo', 'effects', 128, op='&')     # EF_NODRAW
    t.expect.value('start', 'holo', 'solid', 0)
    t.expect.value('on', 'holo', 'effects', 128, op='!&')
    t.expect.value('on', 'holo', 'solid', 0)
    t.expect.value('on2', 'holo', 'effects', 128, op='!&')
    t.expect.value('off', 'holo', 'effects', 128, op='&')


# ---------------------------------------------------------------------------------------------- env_spritetrain
def corners(t, a=(-150, -150, 64), b=(150, -150, 64), speed=None, **kv):
    t.point('path_corner', targetname='c1', at=a, target='c2', **kv)
    t.point('path_corner', targetname='c2', at=b, target='c1', **({'speed': speed} if speed else {}))


@S.test('env_spritetrain: waits at its first corner, a trigger starts it along the path at its speed; render '
        'defaults Additive and 255', doc=DOC,
        covers=('env_spritetrain', 'env_spritetrain.target', 'env_spritetrain.speed', 'env_spritetrain.framerate',
                'env_spritetrain.noise', 'env_spritetrain.noise1'))
def spritetrain(t):
    corners(t)
    t.point('env_spritetrain', targetname='st', at=(0, 0, 64), target='c1', speed='200', model='sprites/fexplo1.spr',
            framerate='10', noise='plats/train1.wav', noise1='plats/train2.wav')
    t.wait(0.3)
    t.snap('waiting', 'st', 'origin', 'rendermode', 'renderamt')
    t.snap('waiting', 'c1', 'origin')
    t.expect.distance('waiting', 'st', 'c1', 0, 1)
    t.fire('st')
    t.wait(0.8)
    t.snap('moving', 'st', 'origin', 'velocity', 'frame')
    t.fire('st')
    t.wait(0.2)
    t.expect.value('waiting', 'st', 'rendermode', 5)
    t.expect.value('waiting', 'st', 'renderamt', 255)
    t.expect.value('moving', 'st', 'velocity.x', 190, op='>')
    t.expect.value('moving', 'st', 'frame', 0, op='>')
    t.expect.sound('plats/train1.wav', ent='st')
    t.expect.sound('plats/train2.wav', ent='st')


@S.test('env_spritetrain Initially On (4), Obey Trigger Mode, negative render amount starts invisible', doc=DOC,
        covers=('env_spritetrain', 'env_spritetrain:4', 'env_spritetrain.m_iObeyTriggerMode=1',
                'env_spritetrain.renderamt', 'env_spritetrain.rendermode'))
def spritetrain_flags(t):
    # c2 -> c1 is a teleport (path_corner flag 2), so the train only ever moves +x; it runs since the map started
    t.point('path_corner', targetname='c1', at=(-150, -150, 64), target='c2', spawnflags='2')
    t.point('path_corner', targetname='c2', at=(150, -150, 64), target='c1')
    t.point('env_spritetrain', targetname='st', at=(0, 0, 64), target='c1', speed='100', model='sprites/glow01.spr',
            spawnflags='4', m_iObeyTriggerMode='1', renderamt='-5', rendermode='2')
    t.wait(0.6)
    t.snap('auto', 'st', 'velocity', 'renderamt', 'rendermode')
    t.fire('st', use='on')          # obey: already moving, On does nothing
    t.wait(0.2)
    t.snap('still', 'st', 'velocity')
    t.fire('st', use='off')
    t.wait(0.2)
    t.snap('stopped', 'st', 'velocity')
    t.expect.value('auto', 'st', 'velocity.x', 50, op='>')
    t.expect.value('auto', 'st', 'renderamt', 0)
    t.expect.value('auto', 'st', 'rendermode', 2)
    t.expect.value('still', 'st', 'velocity.x', 50, op='>')
    t.expect.value('stopped', 'st', 'velocity', [0, 0, 0])


# ---------------------------------------------------------------------------------------------- env_modeltrain
@S.test('env_modeltrain: moves along the path turning to face the next corner (yaw only), move and idle sequences',
        doc=DOC,
        covers=('env_modeltrain', 'env_modeltrain.target', 'env_modeltrain.speed', 'env_modeltrain.turn_policy=1',
                'env_modeltrain.idle_sequence', 'env_modeltrain.move_sequence', 'env_modeltrain.idle_framerate',
                'env_modeltrain.move_framerate'))
def modeltrain(t):
    corners(t, a=(-150, -150, 64), b=(150, 150, 64))
    t.point('env_modeltrain', targetname='mt', at=(0, 0, 64), target='c1', speed='150',
            model='models/scientist.mdl', turn_policy='1', idle_sequence='idle1', move_sequence='walk',
            move_framerate='2', idle_framerate='0.5')
    t.wait(0.3)
    t.snap('idle', 'mt', 'sequence', 'framerate', 'angles')
    t.fire('mt')
    t.wait(0.5)
    t.snap('moving', 'mt', 'sequence', 'framerate', 'angles', 'velocity')
    t.fire('mt')
    t.wait(0.3)
    t.snap('stopped', 'mt', 'sequence', 'framerate')
    t.expect.value('idle', 'mt', 'framerate', 0.5, tol=0.01)
    t.expect.value('moving', 'mt', 'framerate', 2, tol=0.01)
    t.expect.value('moving', 'mt', 'angles', [0, 45, 0], tol=1)
    t.expect.value('stopped', 'mt', 'framerate', 0.5, tol=0.01)
    t.expect.same('idle', 'stopped', 'mt', 'sequence')


@S.test('env_modeltrain Turn Policy: Instant faces the corner in pitch and yaw, No turn keeps its angles', doc=DOC,
        covers=('env_modeltrain', 'env_modeltrain:4', 'env_modeltrain.turn_policy=0', 'env_modeltrain.turn_policy=2'))
def modeltrain_turn(t):
    t.point('path_corner', targetname='a1', at=(-150, 0, 40), target='a2')
    t.point('path_corner', targetname='a2', at=(150, 0, 140))
    t.point('path_corner', targetname='b1', at=(-150, 100, 40), target='b2')
    t.point('path_corner', targetname='b2', at=(150, 100, 140))
    t.point('env_modeltrain', targetname='inst', target='a1', speed='100', model='models/scientist.mdl',
            turn_policy='0', spawnflags='4')
    t.point('env_modeltrain', targetname='none', target='b1', speed='100', model='models/scientist.mdl',
            turn_policy='2', spawnflags='4', angles='0 90 0')
    t.wait(0.5)
    t.snap('run', 'inst', 'angles')
    t.snap('run', 'none', 'angles')
    t.expect.value('run', 'inst', 'angles.y', 0, tol=1)
    t.expect.value('run', 'inst', 'angles.x', 10, op='>')
    t.expect.value('run', 'none', 'angles', [0, 90, 0], tol=0.5)


# ================================================================================ batch (b): player and trigger
FL_DUCKING = 16384
EF_DIMLIGHT = 8
SF_TRACKTRAIN_NOCONTROL = 2


def hold(t, player, forward, side, seconds, *buttons):
    """Input held for `seconds` (test_move -1), not released: snapshot during the move, then release()."""
    t._step('test_move', t.ref(player), forward, side, -1, *buttons)
    t.wait(seconds)


def release(t, player):
    t._step('test_move', t.ref(player), 0, 0, 0)


@S.test('player_capabilities Jump off: +jump does not leave the ground; Copy use input On gives it back', players=1,
        doc=DOC,
        covers=('player_capabilities', 'player_capabilities.jump_capability=-1',
                'player_capabilities.jump_capability=3'))
def caps_jump(t):
    t.point('player_capabilities', targetname='nojump', jump_capability='-1')
    t.point('player_capabilities', targetname='copy', jump_capability='3')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.wait(0.3)
    t.fire('nojump', activator='player1')
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+jump')
    t.wait(0.1)
    t.snap('off', 'player1', 'velocity', 'flags')
    t.wait(0.6)
    t.fire('copy', activator='player1', use='on')
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+jump')
    t.wait(0.1)
    t.snap('on', 'player1', 'velocity')
    t.wait(0.8)
    t.expect.value('off', 'player1', 'velocity.z', 1, op='<')
    t.expect.value('on', 'player1', 'velocity.z', 100, op='>')


@S.test('player_capabilities Movement off: no walking; Duck off: no ducking; Toggle and respawn give them back',
        players=1, doc=DOC,
        covers=('player_capabilities', 'player_capabilities.movement_capability=-1',
                'player_capabilities.movement_capability=2', 'player_capabilities.duck_capability=-1',
                'player_capabilities.duck_capability=2'))
def caps_move_duck(t):
    t.point('player_capabilities', targetname='freeze', movement_capability='-1', duck_capability='-1')
    t.point('player_capabilities', targetname='flip', movement_capability='2', duck_capability='2')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.wait(0.3)
    t.fire('freeze', activator='player1')
    t.wait(0.1)
    hold(t, 'player1', 300, 0, 0.5, '+duck')
    t.snap('off', 'player1', 'velocity', 'flags')
    release(t, 'player1')
    t.wait(0.3)
    t.fire('flip', activator='player1')         # Toggle: both back on
    t.wait(0.1)
    hold(t, 'player1', 300, 0, 0.5, '+duck')
    t.snap('on', 'player1', 'velocity', 'flags')
    release(t, 'player1')
    t.wait(0.5)
    t.expect.value('off', 'player1', 'velocity.x', 5, op='<')
    t.expect.value('off', 'player1', 'flags', FL_DUCKING, op='!&')
    t.expect.value('on', 'player1', 'velocity.x', 30, op='>')
    t.expect.value('on', 'player1', 'flags', FL_DUCKING, op='&')


@S.test('player_capabilities Use off: +use on a button does nothing; On: it works', players=1, doc=DOC,
        covers=('player_capabilities', 'player_capabilities.use_capability=-1', 'player_capabilities.use_capability=1'))
def caps_use(t):
    t.point('player_capabilities', targetname='nouse', use_capability='-1')
    t.point('player_capabilities', targetname='use', use_capability='1')
    t.brush('func_button', size=(16, 64, 64), at=(60, 0, 16), target='pressed', spawnflags='1', wait='0.2')
    info(t, 'pressed')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait(0.3)
    t.fire('nouse', activator='player1')
    t.wait(0.1)
    t.mark('blocked')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.mark('allowed')
    t.fire('use', activator='player1')
    t.wait(0.1)
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.5)
    t.expect.not_used('pressed', within=(0, 0.45), after='blocked')
    t.expect.used('pressed', after='allowed', min=1)


@S.test('player_capabilities Attack off: +attack does not fire; a spectator activator is ignored', players=1, doc=DOC,
        covers=('player_capabilities', 'player_capabilities.attack_capability=-1',
                'player_capabilities.attack_capability=1'))
def caps_attack(t):
    t.point('player_capabilities', targetname='noattack', attack_capability='-1')
    t.point('player_capabilities', targetname='attack', attack_capability='1')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.wait(0.3)
    t.fire('noattack', activator='player1')
    t.wait(0.1)
    t.mark('blocked')
    t.move('player1', 0, 0, 10, '+attack')
    t.wait(0.5)
    t.mark('allowed')
    t.fire('attack', activator='player1')
    t.wait(0.1)
    t.move('player1', 0, 0, 10, '+attack')
    t.wait(0.5)
    t.expect.event('', ent='player1', count=0, min=0, within=(0, 0.6), after='blocked')
    t.expect.event('', ent='player1', min=1, after='allowed')


@S.test('player_capabilities "Capability" message (Featureful): sent to the player when his capabilities change, '
        'again after a fullupdate, and 0 when they are back', players=1, doc=DOC,
        covers=('player_capabilities', 'player_capabilities.attack_capability=-1',
                'player_capabilities.attack_capability=1', 'player_capabilities.jump_capability=-1',
                'player_capabilities.jump_capability=1'))
def capability_message(t):
    t.point('player_capabilities', targetname='noatk', attack_capability='-1', jump_capability='-1')
    t.point('player_capabilities', targetname='back', attack_capability='1', jump_capability='1')
    t.wait(0.3)
    t.mark('off')
    t.fire('noatk', activator='player1')
    t.wait(1.0)     # steps apart by more than a loaded frame (windows widen by the frame length)
    t.mark('full')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('on')
    t.fire('back', activator='player1')
    t.wait(0.3)
    t.expect.msg('Capability', to='player1', fields={'bits': 3}, after='off', within=(0, 0.5), count=1)
    t.expect.msg('Capability', to='player1', fields={'bits': 3}, after='full', within=(0, 0.5), count=1)
    t.expect.msg('Capability', to='player1', fields={'bits': 0}, after='on', count=1)


@S.test('player_speed: a fraction of his own speed, an absolute speed, 0 resets', players=2, doc=DOC,
        covers=('player_speed', 'player_speed.maxspeed', 'player_speed.value_notion=0', 'player_speed.value_notion=1'))
def player_speed(t):
    t.point('player_speed', targetname='half', maxspeed='0.5', value_notion='0')
    t.point('player_speed', targetname='abs', maxspeed='120', value_notion='1')
    t.point('player_speed', targetname='reset', maxspeed='0')
    t.teleport('player1', (-220, -100, 0), yaw=0)
    t.teleport('player2', (-220, 100, 0), yaw=0)
    t.wait(0.3)
    t.fire('half', activator='player1')
    t.wait(0.1)
    hold(t, 'player1', 400, 0, 0.9)
    hold(t, 'player2', 400, 0, 0.9)
    t.snap('half', 'player1', 'velocity')
    t.snap('half', 'player2', 'velocity')
    t.snap('half_k', 'player1', 'maxspeed')
    t.snap('half_k', 'player2', 'maxspeed')
    release(t, 'player1')
    release(t, 'player2')
    t.wait(0.4)
    t.teleport('player1', (-220, -100, 0), yaw=0)
    t.fire('abs', activator='player1')
    t.wait(0.2)
    hold(t, 'player1', 400, 0, 0.9)
    t.snap('abs', 'player1', 'velocity')
    release(t, 'player1')
    t.wait(0.4)
    t.teleport('player1', (-220, -100, 0), yaw=0)
    t.teleport('player2', (-220, 100, 0), yaw=0)
    t.fire('reset', activator='player1')
    t.wait(0.2)
    hold(t, 'player1', 400, 0, 0.9)
    hold(t, 'player2', 400, 0, 0.9)
    t.snap('reset', 'player1', 'velocity')
    t.snap('reset', 'player2', 'velocity')
    t.snap('reset_k', 'player1', 'maxspeed')
    t.snap('reset_k', 'player2', 'maxspeed')
    release(t, 'player1')
    release(t, 'player2')
    t.wait(0.2)
    t.expect.ratio('half', 'player2', 'half', 'player1', 'velocity.x', 0.5, tol=0.08)
    # as Featureful: the override is the player's max speed (the client predicts with it)
    t.expect.ratio('half_k', 'player2', 'half_k', 'player1', 'maxspeed', 0.5, tol=0.01)
    t.expect.ratio('reset_k', 'player2', 'reset_k', 'player1', 'maxspeed', 1.0, tol=0.01)
    t.expect.value('abs', 'player1', 'velocity.x', 120, tol=6)
    t.expect.ratio('reset', 'player2', 'reset', 'player1', 'velocity.x', 1.0, tol=0.05)


@S.test('game_player_settings: health = + - < >, max health with overheal, armor = and overcharge; fires its target '
        'with the player as activator; a weapon given, then player_hasweapon pass / fail and as a master', players=1,
        doc=DOC,
        covers=('game_player_settings', 'game_player_settings.health', 'game_player_settings.armorvalue',
                'game_player_settings.max_health', 'game_player_settings.allow_overheal=0',
                'game_player_settings.allow_overcharge=1', 'game_player_settings.target', 'player_hasweapon',
                'player_hasweapon.weapon_name', 'player_hasweapon.pass_target', 'player_hasweapon.fail_target',
                'trigger_multiple.master'))
def player_settings(t):
    t.point('game_player_settings', targetname='set50', health='=50', armorvalue='=30', target='equipped')
    t.point('game_player_settings', targetname='add20', health='+20')
    t.point('game_player_settings', targetname='sub10', health='-10')
    t.point('game_player_settings', targetname='atmost40', health='<40')
    t.point('game_player_settings', targetname='atleast80', health='>80', armorvalue='+90')
    t.point('game_player_settings', targetname='max150', max_health='=150', health='=140', allow_overheal='0',
            armorvalue='=150', allow_overcharge='1')
    t.point('game_player_settings', targetname='givesword', weapon_sword='1')
    t.point('game_player_settings', targetname='takesword', weapon_sword='-1')
    t.point('player_hasweapon', targetname='has', weapon_name='weapon_sword', pass_target='yes', fail_target='no')
    t.trigger('trigger_multiple', size=(64, 64, 72), at=(150, 150, 0), target='through', master='has', wait='2')
    for n in ('equipped', 'yes', 'no', 'through'):
        info(t, n)
    tough(t, 'player1', '100')
    t.fire('set50', activator='player1')
    t.wait(0.05)
    t.snap('s1', 'player1', 'health', 'armorvalue')
    t.fire('add20', activator='player1')
    t.wait(0.05)
    t.snap('s2', 'player1', 'health')
    t.fire('sub10', activator='player1')
    t.wait(0.05)
    t.snap('s3', 'player1', 'health')
    t.fire('atmost40', activator='player1')
    t.wait(0.05)
    t.snap('s4', 'player1', 'health')
    t.fire('atleast80', activator='player1')
    t.wait(0.05)
    t.snap('s5', 'player1', 'health', 'armorvalue')
    t.fire('max150', activator='player1')
    t.wait(0.05)
    t.snap('s6', 'player1', 'health', 'max_health', 'armorvalue')
    t.fire('takesword', activator='player1')
    t.wait(0.1)
    t.mark('without')
    t.fire('has', activator='player1')
    t.teleport('player1', (150, 150, 0))
    t.wait(0.3)
    t.teleport('player1', (-150, -150, 0))
    t.wait(0.1)
    t.fire('givesword', activator='player1')
    t.wait(0.3)
    t.mark('with')
    t.fire('has', activator='player1')
    t.teleport('player1', (150, 150, 0))
    t.wait(0.3)
    t.teleport('player1', (-150, -150, 0))
    t.wait(0.1)
    t.expect.value('s1', 'player1', 'health', 50)
    t.expect.value('s1', 'player1', 'armorvalue', 30)
    t.expect.used('equipped', activator='player1', count=1)
    t.expect.value('s2', 'player1', 'health', 70)
    t.expect.value('s3', 'player1', 'health', 60)
    t.expect.value('s4', 'player1', 'health', 40, tol=0.5)
    t.expect.value('s5', 'player1', 'health', 80)
    t.expect.value('s5', 'player1', 'armorvalue', 100)          # +90 capped at 100 without overcharge
    t.expect.value('s6', 'player1', 'max_health', 150)
    t.expect.value('s6', 'player1', 'health', 140)
    t.expect.value('s6', 'player1', 'armorvalue', 150)
    t.expect.used('no', activator='player1', count=1, after='without')
    t.expect.used('yes', activator='player1', count=1, after='with')
    t.expect.used('through', count=1)                      # only with the sword (trigger_multiple master)
    t.expect.used('through', after='with', count=1)


@S.test('player_flicker: On flickers the flashlight, Off keeps it lit; entities ignore a non-player activator',
        players=1, doc=DOC, covers=('player_flicker',))
def flicker(t):
    t.point('player_flicker', targetname='fl')
    t.console('mp_flashlight 1')
    t.wait(0.1)
    t.kv('player1', 'impulse', '100')          # flashlight on
    t.wait(0.3)
    t.snap('lit', 'player1', 'effects')
    t.fire('fl', activator='player1', use='on')
    for i in range(8):
        t.wait(0.12)
        t.snap('f%d' % i, 'player1', 'effects')
    t.fire('fl', activator='player1', use='off')
    t.wait(0.1)
    t.snap('steady', 'player1', 'effects')
    t.fire('fl', activator='fl', use='on')     # not a player: nothing
    t.wait(0.8)
    t.snap('still', 'player1', 'effects')
    t.kv('player1', 'impulse', '100')
    t.expect.value('lit', 'player1', 'effects', EF_DIMLIGHT, op='&')
    t.expect.value('steady', 'player1', 'effects', EF_DIMLIGHT, op='&')
    t.expect.value('still', 'player1', 'effects', EF_DIMLIGHT, op='&')


@S.test('trigger_vehiclecontrols: Off locks the train\'s controls, On unlocks, Toggle switches', doc=DOC,
        covers=('trigger_vehiclecontrols', 'trigger_vehiclecontrols.target'))
def vehiclecontrols(t):
    t.point('path_track', targetname='p1', target='p2', at=(0, -150, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 150, 0))
    t.brush('func_tracktrain', size=(64, 64, 16), at=(0, -150, 0), origin=True, targetname='tt', target='p1',
            speed='100', height='8', wheels='32')
    t.point('trigger_vehiclecontrols', targetname='vc', target='tt')
    t.wait(0.3)
    t.snap('start', 'tt', 'spawnflags')
    t.fire('vc', use='off')
    t.wait(0.05)
    t.snap('locked', 'tt', 'spawnflags')
    t.fire('vc', use='on')
    t.wait(0.05)
    t.snap('unlocked', 'tt', 'spawnflags')
    t.fire('vc', use='toggle')
    t.wait(0.05)
    t.snap('toggled', 'tt', 'spawnflags')
    t.expect.value('start', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='!&')
    t.expect.value('locked', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='&')
    t.expect.value('unlocked', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='!&')
    t.expect.value('toggled', 'tt', 'spawnflags', SF_TRACKTRAIN_NOCONTROL, op='&')


@S.test('trigger_vehiclecontrols as the player sees it: locked, +use and +forward on the platform do not drive it; '
        'unlocked, they do', players=1, doc=DOC, covers=('trigger_vehiclecontrols', 'trigger_vehiclecontrols.target'))
def vehiclecontrols_drive(t):
    t.point('path_track', targetname='p1', target='p2', at=(0, -200, 0))
    t.point('path_track', targetname='p2', target='p1', at=(0, 200, 0))
    t.brush('func_tracktrain', size=(96, 96, 16), at=(0, -200, 0), origin=True, targetname='tt', target='p1',
            speed='150', height='8', wheels='32')
    t.trigger('func_traincontrols', size=(96, 96, 96), at=(0, -200, 16), target='tt')
    t.point('trigger_vehiclecontrols', targetname='vc', target='tt')
    t.wait(0.3)
    t.fire('vc', use='off')
    t.teleport('player1', (0, -200, 60), yaw=90)
    t.wait(1.0)
    t.snap('a', 'tt', 'origin')
    t._step('test_move', t.ref('player1'), 0, 0, 3, '+use')
    t.wait(0.3)
    t._step('test_move', t.ref('player1'), 0, 0, 3, '+forward')
    t.wait(1.5)
    t.snap('b', 'tt', 'origin')
    t.fire('vc', use='on')
    t.wait(0.2)
    t._step('test_move', t.ref('player1'), 0, 0, 3, '+use')
    t.wait(0.3)
    t._step('test_move', t.ref('player1'), 0, 0, 3, '+forward')
    t.wait(1.5)
    t.snap('c', 'tt', 'origin')
    t._step('test_move', t.ref('player1'), 0, 0, 3, '+use')     # let go of the controls (the next tests move him)
    t.wait(0.3)
    t.teleport('player1', (0, 0, 0))
    t.wait(0.3)
    t.expect.moved('a', 'b', 'tt', (0, 0, 0), tol=2)
    t.expect.value('c', 'tt', 'origin.y', t._abs((0, -200, 0))[1] + 40, op='>')


@S.test('ambient_random: each trigger plays one of its sounds (all of them over 12 triggers); not toggled', doc=DOC,
        covers=('ambient_random', 'ambient_random.message', 'ambient_random.noise', 'ambient_random.noise1'))
def ambient_random(t):
    t.point('ambient_random', targetname='ar', at=(0, 0, 64), message='buttons/blip1.wav', noise='buttons/blip2.wav',
            noise1='buttons/button3.wav', spawnflags=str(16 | 32), health='10')
    for i in range(12):
        t.fire('ar')
        t.wait(0.1)
    t.expect.sound('buttons/blip1.wav', min=1)
    t.expect.sound('buttons/blip2.wav', min=1)
    t.expect.sound('buttons/button3.wav', min=1)
    t.expect.precached('buttons/button3.wav', kind='sound')


@S.test('ambient_extraspeaker: plays on a speaker that follows the activator (blank target or !activator); toggled mode stops it',
        players=1, doc=DOC,
        covers=('ambient_extraspeaker', 'ambient_extraspeaker.message', 'ambient_extraspeaker.target'))
def extraspeaker(t):
    t.point('ambient_extraspeaker', targetname='es', at=(0, 0, 64), message='buttons/blip1.wav',
            spawnflags=str(16 | 32), channel='2')               # no target: the activator (*player)
    t.point('ambient_extraspeaker', targetname='loop', at=(0, 0, 64), message='ambience/alien_zonerator.wav',
            target='!activator', spawnflags='16')
    t.fire('es', activator='player1')
    t.wait(0.2)
    t.snap('s', '@extra_speaker', 'movetype', 'aiment')
    t.fire('loop', activator='player1')
    t.wait(0.2)
    t.mark('stop')
    t.fire('loop', activator='player1')
    t.wait(0.2)
    t.expect.spawned('@extra_speaker', count=1)
    t.expect.sound('buttons/blip1.wav', ent='@extra_speaker', count=1)
    t.expect.sound('alien_zonerator', ent='@extra_speaker', min=1)
    t.expect.sound('alien_zonerator', ent='@extra_speaker', flags=32, after='stop', min=1)
    t.expect.value('s', '@extra_speaker', 'movetype', 12)     # MOVETYPE_FOLLOW


@S.test('ambient_extraspeaker without Start Silent: the default target (the activator) is nobody at the map start, '
        'so the first trigger plays it on the player and the next stops it; a named target plays from the start and '
        'the first trigger stops it', players=1, doc=DOC,
        covers=('ambient_extraspeaker', 'ambient_extraspeaker.target', 'ambient_extraspeaker:16'))
def extraspeaker_start_on(t):
    info(t, 'box', at=(100, 0, 16))
    t.point('ambient_extraspeaker', targetname='es', at=(0, 0, 64), message='ambience/alien_zonerator.wav',
            spawnflags='4')                                     # no target: the activator
    t.point('ambient_extraspeaker', targetname='named', at=(0, 0, 64), message='ambience/alien_zonerator.wav',
            target='box', spawnflags='4')
    t.wait(0.3)
    t.mark('first')
    t.fire('es', activator='player1')
    t.wait(1.0)     # apart by more than a loaded frame
    t.mark('second')
    t.fire('es', activator='player1')
    t.wait(1.0)
    t.mark('third')
    t.fire('named', activator='player1')
    t.wait(1.0)
    t.expect.sound('alien_zonerator', ent='@extra_speaker', after='first', within=(0, 0.5), count=1)    # plays
    t.expect.sound('alien_zonerator', after='first', within=(0, 0.5), flags=32, count=0, min=0)
    t.expect.sound('alien_zonerator', ent='@extra_speaker', after='second', within=(0, 0.5), flags=32, count=1)
    t.expect.sound('alien_zonerator', ent='@extra_speaker', after='third', within=(0, 0.5), flags=32, count=1)
    t.expect.sound('alien_zonerator', after='third', within=(0, 0.5), count=1)      # only the stop


@S.test('env_soundmark: a Danger mark at a position; Remove On fire (1)', doc=DOC,
        covers=('env_soundmark', 'env_soundmark:1'))
def soundmark(t):
    info(t, 'spot', at=(100, 0, 16))
    t.point('env_soundmark', targetname='sm', at=(0, 0, 16), position='spot', type='32', radius='500',
            duration='1', spawnflags='1', names=('position',))
    t.fire('sm')
    t.wait(0.2)
    t.expect.removed('sm')


# ================================================================================ batch (c): monsters and scripts
SF_TANK_ACTIVE, SF_TANK_CANCONTROL = 1, 32


def tank(t, name='tank', at=(0, 150, 64), flags=0, target='shot', **kv):
    """A func_tank (an ORIGIN brush at its center), yaw 270 (facing -y), firing fast; fires `target` per shot."""
    t.brush('func_tank', size=(32, 32, 32), at=at, origin=True, targetname=name, target=target,
            spawnflags=str(flags), yawrate='300', yawrange='180', yawtolerance='15', pitchrate='300',
            pitchrange='60', pitchtolerance='15', firerate='5', bullet='1', barrel='16', persistence='1',
            angles='0 270 0', **kv)
    info(t, target)


@S.test('monster_target: an automatic func_tank shoots at an active one counting as a player ally; inactive (1): '
        'no shots; On switches it on', doc=DOC,
        covers=('monster_target', 'monster_target:1', 'monster_target.frags=11'))
def monster_target(t):
    tank(t, flags=SF_TANK_ACTIVE)
    t.point('monster_target', targetname='mt', at=(0, -150, 40), frags='11', spawnflags='1')
    t.wait(1.0)
    t.mark('on')
    t.fire('mt', use='on')
    t.wait(3.0)                     # a tank with nothing to shoot looks again every 2 s
    t.expect.not_used('shot', within=(0, 0.9))
    t.expect.used('shot', after='on', min=2)


@S.test('monster_target: a target counting as an alien is not shot by tanks', doc=DOC,
        covers=('monster_target.frags=7',))
def monster_target_alien(t):
    tank(t, flags=SF_TANK_ACTIVE)
    t.point('monster_target', targetname='mb', at=(0, -150, 40), frags='7')
    t.wait(1.5)
    t.snap('s', 'mb', 'health', 'flags')
    t.expect.not_used('shot')
    t.expect.value('s', 'mb', 'health', 1)
    t.expect.value('s', 'mb', 'flags', 32, op='&')      # FL_MONSTER


@S.test('scripted_tanksequence: face the sequence, fire once at the end, halt when facing; Control and Active '
        'afterwards; not repeatable: removed', doc=DOC,
        covers=('scripted_tanksequence', 'scripted_tanksequence.m_iszEntity', 'scripted_tanksequence.m_iTurn=2',
                'scripted_tanksequence.m_iShoot=1', 'scripted_tanksequence.m_iUntil=1', 'scripted_tanksequence.target',
                'scripted_tanksequence.m_iActive=1', 'scripted_tanksequence.m_iControllable=1'))
def tanksequence_face(t):
    tank(t)
    t.point('scripted_tanksequence', targetname='seq', at=(-150, 150, 64), m_iszEntity='tank', m_iTurn='2',
            m_iShoot='1', m_iUntil='1', target='halted', m_iActive='1', m_iControllable='1')
    info(t, 'halted')
    t.wait(0.3)
    t.fire('seq')
    t.wait(2.0)
    t.snap('after', 'tank', 'spawnflags', 'angles')
    t.expect.used('halted', count=1)
    t.expect.used('shot', min=1)
    t.expect.value('after', 'tank', 'spawnflags', SF_TANK_ACTIVE | SF_TANK_CANCONTROL, op='&')
    t.expect.value('after', 'tank', 'angles.y', 180, tol=20)
    t.expect.removed('seq')


@S.test('scripted_tanksequence: match the sequence angle, fire constantly, time limit fires the timeout target; '
        'Repeatable (2) stays', doc=DOC,
        covers=('scripted_tanksequence', 'scripted_tanksequence:2', 'scripted_tanksequence.m_iTurn=1',
                'scripted_tanksequence.m_iShoot=2', 'scripted_tanksequence.m_iUntil=0',
                'scripted_tanksequence.m_fDuration', 'scripted_tanksequence.netname',
                'scripted_tanksequence.m_iActive=0'))
def tanksequence_timeout(t):
    tank(t)
    t.point('scripted_tanksequence', targetname='seq', at=(150, 150, 64), m_iszEntity='tank', m_iTurn='1',
            angles='0 180 0', m_iShoot='2', m_iUntil='0', m_fDuration='1', netname='timedout', spawnflags='2',
            m_iActive='0')
    info(t, 'timedout')
    t.wait(0.3)
    t.mark('go')
    t.fire('seq')
    t.wait(1.6)
    t.snap('after', 'tank', 'angles', 'spawnflags')
    t.expect.used('timedout', count=1)
    t.expect.used('shot', min=3, after='go')
    t.expect.value('after', 'tank', 'angles.y', 180, tol=15)
    t.expect.value('after', 'tank', 'spawnflags', SF_TANK_ACTIVE, op='!&')
    t.expect.not_used('seq_removed_marker')


@S.test('scripted_tanksequence: face an enemy (a monster_target by name), halt when it dies (Off): no halt '
        'target before; a trigger ends it early', doc=DOC,
        covers=('scripted_tanksequence', 'scripted_tanksequence.m_iTurn=3', 'scripted_tanksequence.m_iszEnemy',
                'scripted_tanksequence.m_iShoot=3', 'scripted_tanksequence.m_iUntil=2', 'scripted_tanksequence.target',
                'monster_target'))
def tanksequence_enemy(t):
    tank(t)
    t.point('monster_target', targetname='foe', at=(-150, 0, 40))
    t.point('scripted_tanksequence', targetname='seq', at=(150, 150, 64), m_iszEntity='tank', m_iTurn='3',
            m_iszEnemy='foe', m_iShoot='3', m_iUntil='2', target='halted', spawnflags='2', names=('m_iszEnemy',))
    info(t, 'halted')
    t.wait(0.3)
    t.fire('seq')
    t.wait(1.5)
    t.mark('kill')
    t.kv('foe', 'health', '0')
    t.wait(0.5)
    t.expect.used('shot', min=1)
    t.expect.not_used('halted', within=(0, 1.4))
    t.expect.used('halted', after='kill', count=1)


@S.test('trigger_startpatrol: a scientist walks the route from the named path_corner', doc=DOC,
        covers=('trigger_startpatrol', 'trigger_startpatrol.target', 'trigger_startpatrol.m_iszPath'))
def startpatrol(t):
    t.point('monster_scientist', targetname='sci', at=(-150, -150, 0), angles='0 0 0')
    t.point('path_corner', targetname='r1', at=(150, -150, 0), target='r2')
    t.point('path_corner', targetname='r2', at=(150, 150, 0), target='r1')
    t.point('trigger_startpatrol', targetname='go', target='sci', m_iszPath='r1')
    t.wait(1.5)
    t.snap('before', 'sci', 'origin')
    t.snap('before', 'r1', 'origin')
    t.fire('go')
    t.wait(4.0)
    t.snap('after', 'sci', 'origin')
    t.snap('after', 'r1', 'origin')
    t.expect.distance('before', 'sci', 'r1', 250, 350)
    t.expect.distance('after', 'sci', 'r1', 0, 200)


@S.test('scripted_schedule: move to a spot, turn to a spot; a scientist obeys', doc=DOC,
        covers=('scripted_schedule', 'scripted_schedule.entity', 'scripted_schedule.schedule=4',
                'scripted_schedule.schedule=5', 'scripted_schedule.spot_entity'))
def scripted_schedule(t):
    t.point('monster_scientist', targetname='sci', at=(-150, 0, 0), angles='0 0 0')
    info(t, 'spot', at=(150, 0, 0))
    info(t, 'north', at=(-150, 200, 0))
    t.point('scripted_schedule', targetname='move', entity='sci', schedule='5', spot_entity='spot',
            move_preference='2', names=('entity', 'spot_entity'))
    t.point('scripted_schedule', targetname='turn', entity='sci', schedule='4', spot_entity='north',
            names=('entity', 'spot_entity'))
    t.wait(1.5)
    t.fire('turn')
    t.wait(2.0)
    t.snap('turned', 'sci', 'angles')
    t.fire('move')
    t.wait(4.0)
    t.snap('moved', 'sci', 'origin')
    t.snap('moved', 'spot', 'origin')
    t.expect.value('turned', 'sci', 'angles.y', 90, tol=20)
    t.expect.distance('moved', 'sci', 'spot', 0, 140)


def no_run_test(key, classname, away=True):
    def runs_by_walking(t):
        # Prisoners (16): they see nobody
        for x in (-192, -64, 64, 192):
            for y in (-192, -64, 64, 192):
                t.point('info_node', at=(x, y, 16))
        t.point(classname, targetname='mon', at=(-150, -100, 0), angles='0 0 0', spawnflags='16')
        info(t, 'spot', at=(150, -100, 0))
        info(t, 'start', at=(-150, -100, 0))
        t.point('scripted_schedule', targetname='run', entity='mon', schedule='5', spot_entity='spot',
                move_preference='2', names=('entity', 'spot_entity'))
        t.point('scripted_schedule', targetname='away', entity='mon', schedule='1', mindist='96', maxdist='300',
                names=('entity',))
        t.wait(1.0)
        t.fire('mon', use='on')
        t.wait(0.5)
        t.fire('run')
        t.wait(8.0)
        t.snap('ran', 'mon', 'origin', 'schedule')
        t.snap('ran', 'spot', 'origin')
        t.fire('away')     # move away: "auto" runs
        t.wait(6.0)
        t.snap('away', 'mon', 'origin')
        t.snap('away', 'spot', 'origin')
        t.expect.distance('ran', 'mon', 'spot', 0, 100)
        if away:
            t.expect.distance('away', 'mon', 'spot', 90, 600)

    runs_by_walking.__name__ = 'scripted_schedule_run_walks_' + key
    S.test('scripted_schedule run orders on %s (no run animation): it walks instead, to the spot%s, as Half-Life\'s '
           'run tasks do' % (classname, ' and away from it' if away else ''), players=1, doc=DOC,
           covers=('scripted_schedule', 'scripted_schedule.entity', 'scripted_schedule.schedule=5',
                   'scripted_schedule.schedule=1', 'scripted_schedule.spot_entity',
                   'scripted_schedule.move_preference=2', 'scripted_schedule.mindist', 'scripted_schedule.maxdist',
                   classname))(runs_by_walking)


for _key, _classname in (('zombie', 'monster_zombie'), ('cyborg', 'monster_cyborg'),
                         ('cyberminotor', 'monster_cyberminotor')):
    no_run_test(_key, _classname)


@S.test('scripted_following: a scientist starts following the player (fires on start), Forward input Toggle stops '
        'him (fires on stop); a second start fires nothing', players=1, doc=DOC,
        covers=('scripted_following', 'scripted_following.entity', 'scripted_following.use_type=0',
                'scripted_following.use_type=1', 'scripted_following.on_start_following',
                'scripted_following.on_stop_following'))
def scripted_following(t):
    t.point('monster_scientist', targetname='sci', at=(0, 150, 0), angles='0 270 0')
    t.point('scripted_following', targetname='follow', entity='sci', use_type='1', on_start_following='started',
            on_stop_following='stopped', names=('entity',))
    t.point('scripted_following', targetname='fwd', entity='sci', use_type='0', on_start_following='started2',
            on_stop_following='stopped', names=('entity',))
    for n in ('started', 'stopped', 'started2'):
        info(t, n)
    t.teleport('player1', (0, -100, 0))
    t.wait(1.5)
    t.fire('follow', activator='player1')
    t.wait(0.3)
    t.fire('follow', activator='player1')     # already following: nothing
    t.wait(0.3)
    t.mark('stop')
    t.fire('fwd', activator='player1', use='toggle')
    t.wait(0.3)
    t.expect.used('started', activator='player1', count=1)
    t.expect.used('stopped', activator='player1', count=1, after='stop')
    t.expect.not_used('started2')


@S.test('scripted_action: a grunt fires at a crate (a func_breakable) on cue: the crate takes his bullets; the action '
        'fires its target; No action (8) finishes at once', doc=DOC,
        covers=('scripted_action', 'scripted_action.m_iszEntity', 'scripted_action.m_fAction=0',
                'scripted_action.m_fAction=8', 'scripted_action.m_iszAttack', 'scripted_action.target'))
def scripted_action(t):
    t.point('monster_human_grunt', targetname='grunt', at=(-150, 0, 0), angles='0 0 0')
    t.point('monster_scientist', targetname='sci', at=(-150, 150, 0), angles='0 0 0')
    t.brush('func_breakable', targetname='crate', size=(48, 48, 48), at=(150, 0, 0), material='1', health='100000')
    t.point('scripted_action', targetname='act', at=(-150, 0, 0), m_iszEntity='grunt', m_fAction='0',
            m_iszAttack='crate', target='acted', m_fMoveTo='0', spawnflags='64', names=('m_iszEntity', 'm_iszAttack'))
    t.point('scripted_action', targetname='noact', at=(-150, 150, 0), m_iszEntity='sci', m_fAction='8',
            target='noacted', spawnflags='64', names=('m_iszEntity',))
    info(t, 'acted')
    info(t, 'noacted')
    t.wait(1.0)
    t.mark('go')
    t.fire('act')
    t.fire('noact')
    t.wait(3.0)
    t.snap('s', 'grunt', 'angles')
    t.expect.used('acted', count=1)
    t.expect.used('noacted', count=1)
    t.expect.damaged('crate', attacker='grunt', after='go')
    t.expect.value('s', 'grunt', 'angles.y', 0, tol=10)


@S.test('scripted_action Entity to attack !activator: the grunt fires at the entity that fired the action (a crate '
        'to his left), not at the script', doc=DOC, covers=('scripted_action.m_iszAttack',))
def scripted_action_activator(t):
    t.point('monster_human_grunt', targetname='grunt', at=(-150, 0, 0), angles='0 0 0')
    t.brush('func_breakable', targetname='crate', size=(48, 48, 48), at=(-150, 180, 0), material='1',
            health='100000')
    t.point('scripted_action', targetname='act', at=(-150, 0, 0), m_iszEntity='grunt', m_fAction='0',
            m_iszAttack='!activator', target='acted', m_fMoveTo='0', spawnflags='64', names=('m_iszEntity',))
    info(t, 'acted')
    t.wait(1.0)
    t.mark('go')
    t.fire('act', activator='crate')
    t.wait(3.0)
    t.snap('s', 'grunt', 'angles')
    t.expect.used('acted', count=1)
    t.expect.damaged('crate', attacker='grunt', after='go')
    t.expect.value('s', 'grunt', 'angles.y', 90, tol=10)
    t.expect.no_console('no attack target')


@S.test('env_soundmark Danger: a scientist near the mark runs away from it (a node grid gives him cover; monsters '
        'listen only with a player in their PVS)', players=1, doc=DOC,
        covers=('env_soundmark', 'env_soundmark.type=32', 'env_soundmark.radius'))
def soundmark_scatter(t):
    for x in (-200, -100, 0, 100, 200):
        for y in (-200, -100, 0, 100, 200):
            if abs(x) != 150:
                t.point('info_node', at=(x, y, 16))
    # cover: a wall the scientist can hide behind (HL monsters take cover from a danger sound; an open room has none)
    t.brush('func_wall', size=(32, 160, 120), at=(150, 0, 0))
    t.brush('func_wall', size=(32, 160, 120), at=(-150, 0, 0))
    t.point('monster_scientist', targetname='sci', at=(30, 0, 0), angles='0 0 0')
    info(t, 'mark', at=(0, 0, 16))
    t.point('env_soundmark', targetname='sm', at=(0, 0, 16), type='32', radius='160', duration='3')
    # a danger sound: monsters run to cover at least its radius away (Half-Life's TASK_FIND_COVER_FROM_BEST_SOUND)
    t.teleport('player1', (-200, -200, 0))
    t.wait(2.0)
    t.snap('before', 'sci', 'origin')
    t.snap('before', 'mark', 'origin')
    t.fire('sm')
    t.wait(3.0)
    t.snap('after', 'sci', 'origin')
    t.snap('after', 'mark', 'origin')
    t.expect.distance('before', 'sci', 'mark', 0, 60)
    t.expect.distance('after', 'sci', 'mark', 100, 600)


@S.test('scripted_schedule turn to !activator: the scientist turns to the player and does not start following him '
        '(the spot is not his follow target)', players=1, doc=DOC,
        covers=('scripted_schedule', 'scripted_schedule.schedule=4', 'scripted_schedule.spot_entity'))
def scripted_schedule_turn_player(t):
    t.point('monster_scientist', targetname='sci', at=(-150, 0, 0), angles='0 0 0')
    t.point('scripted_schedule', targetname='turn', entity='sci', schedule='4', spot_entity='!activator',
            names=('entity',))
    t.teleport('player1', (100, 150, 0))
    t.wait(1.5)
    t.fire('turn', activator='player1')
    t.wait(2.0)
    t.snap('turned', 'sci', 'angles', 'origin')
    t.teleport('player1', (150, -200, 0))
    t.wait(3.0)
    t.snap('later', 'sci', 'origin')
    t.expect.value('turned', 'sci', 'angles.y', 31, tol=15)
    t.expect.moved('turned', 'later', 'sci', (0, 0, 0), tol=16, flat=True)


@S.test('scripted_schedule waits while the scientist is afraid (a zombie behind glass): once the zombie is dead '
        'he runs to the spot (Featureful keeps a suggestion until the monster is free)', players=1, doc=DOC,
        covers=('scripted_schedule', 'scripted_schedule.schedule=5', 'scripted_schedule.spot_entity'))
def scripted_schedule_waits(t):
    t.point('monster_scientist', targetname='sci', at=(-150, -150, 0), angles='0 90 0')
    t.console('developer 2')
    t.point('monster_zombie', targetname='zom', at=(-150, 150, 0), angles='0 270 0')
    # the zombie in a glass box: the scientist sees him and is afraid, nobody gets hurt
    for size, at in (((128, 8, 128), (-150, 90, 0)), ((128, 8, 128), (-150, 210, 0)),
                     ((8, 128, 128), (-210, 150, 0)), ((8, 128, 128), (-90, 150, 0))):
        t.brush('func_wall', size=size, at=at, rendermode='2', renderamt='80')
    info(t, 'spot', at=(150, -150, 8))
    t.point('scripted_schedule', targetname='move', entity='sci', schedule='5', spot_entity='spot',
            move_preference='2', names=('entity', 'spot_entity'))
    t.teleport('player1', (200, 200, 0))
    t.wait(3.0)
    t.fire('move')
    t.wait(1.0)
    t.damage('zom', 1000)
    t.wait(6.0)
    t.snap('s', 'sci', 'origin')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'sci', 'spot', 0, 80)


@S.test('scripted_schedule move away with a wall behind the scientist: he goes round it to a node away from the spot',
        doc=DOC,
        covers=('scripted_schedule', 'scripted_schedule.schedule=1', 'scripted_schedule.spot_entity',
                'scripted_schedule.mindist'))
def scripted_schedule_away_wall(t):
    for x in (-192, -64, 64, 192):
        for y in (-192, -64, 64, 192):
            t.point('info_node', at=(x, y, 16))
    t.brush('func_wall', size=(32, 256, 96), at=(-150, 0, 0))
    t.point('monster_scientist', targetname='sci', at=(-110, 0, 0), angles='0 0 0')
    info(t, 'spot', at=(-40, 0, 8))
    info(t, 'start', at=(-110, 0, 0))
    t.point('scripted_schedule', targetname='away', entity='sci', schedule='1', spot_entity='spot', mindist='128',
            maxdist='400', names=('entity', 'spot_entity'))
    t.wait(1.5)
    t.fire('away')
    t.wait(4.0)
    t.snap('s', 'sci', 'origin')
    t.snap('s', 'spot', 'origin')
    t.snap('s', 'start', 'origin')
    t.expect.distance('s', 'sci', 'start', 128, 600)     # mindist: how far he goes
    t.expect.distance('s', 'sci', 'spot', 100, 600)      # and away from the spot, not towards it


@S.test('trigger_startpatrol: a player standing in the route does not stop the patrol (the scientist goes round him '
        'and reaches the last corner)', players=1, doc=DOC,
        covers=('trigger_startpatrol', 'trigger_startpatrol.target', 'trigger_startpatrol.m_iszPath'))
def startpatrol_player_in_path(t):
    t.point('monster_scientist', targetname='sci', at=(-150, -150, 0), angles='0 0 0')
    t.point('path_corner', targetname='r1', at=(100, -150, 0), target='r2')
    t.point('path_corner', targetname='r2', at=(100, 50, 0))
    t.point('trigger_startpatrol', targetname='go', target='sci', m_iszPath='r1')
    t.teleport('player1', (-25, -150, 0))
    t.wait(1.5)
    t.fire('go', activator='player1')
    t.wait(18.0)     # 450 units at a scientist's walk (about 60 units/s), the way round the player, a pause
    t.snap('after', 'sci', 'origin')
    t.snap('after', 'r2', 'origin')
    t.expect.distance('after', 'sci', 'r2', 0, 80)


# ================================================================================ batch (e): motion
def mover(t, name='box', at=(0, 0, 100)):
    """A point entity to move: an info_target (keeps its position, no physics)."""
    return t.point('info_target', targetname=name, at=at)


@S.test('trigger_motion: set position [LP], add offset, set angles from a direction, rotate by values, set / add '
        'velocity, set / add avelocity', doc=DOC,
        covers=('trigger_motion', 'trigger_motion.target', 'trigger_motion.m_iszPosition',
                'trigger_motion.m_iPosMode=0', 'trigger_motion.m_iPosMode=1', 'trigger_motion.m_iszAngles',
                'trigger_motion.m_iAngMode=0', 'trigger_motion.m_iAngMode=2', 'trigger_motion.m_iszVelocity',
                'trigger_motion.m_iVelMode=0', 'trigger_motion.m_iVelMode=1', 'trigger_motion.m_iszAVelocity',
                'trigger_motion.m_iAVelMode=0', 'trigger_motion.m_iAVelMode=1'))
def trigger_motion(t):
    mover(t)
    info(t, 'spot', at=(100, 50, 120))
    t.point('trigger_motion', targetname='setpos', target='box', m_iszPosition='spot', m_iPosMode='0',
            m_iszAngles='0 1 0', m_iAngMode='0', names=('m_iszPosition',))
    t.point('trigger_motion', targetname='offset', target='box', m_iszPosition='10 0 0', m_iPosMode='1',
            m_iszAngles='0 30 0', m_iAngMode='2', m_iszVelocity='5 0 0', m_iVelMode='0',
            m_iszAVelocity='0 40 0', m_iAVelMode='0')
    t.point('trigger_motion', targetname='more', target='box', m_iszVelocity='0 7 0', m_iVelMode='1',
            m_iszAVelocity='0 2 0', m_iAVelMode='1', spawnflags='1')
    t.wait(0.2)
    t.fire('setpos')
    t.wait(0.05)
    t.snap('a', 'box', 'origin', 'angles')
    t.fire('offset')
    t.wait(0.05)
    t.snap('b', 'box', 'origin', 'angles', 'velocity', 'avelocity')
    t.fire('more')
    t.wait(0.05)
    t.snap('c', 'box', 'velocity', 'avelocity')
    t.expect.value('a', 'box', 'origin', pos(t, (100, 50, 120)), tol=0.5)
    t.expect.value('a', 'box', 'angles.y', 90, tol=0.5)
    t.expect.value('b', 'box', 'origin', pos(t, (110, 50, 120)), tol=2)
    t.expect.value('b', 'box', 'angles.y', 120, tol=0.5)
    t.expect.value('b', 'box', 'velocity', [5, 0, 0], tol=0.1)
    t.expect.value('b', 'box', 'avelocity', [0, 40, 0], tol=0.1)
    t.expect.value('c', 'box', 'velocity', [5, 7, 0], tol=0.1)
    t.expect.value('c', 'box', 'avelocity', [0, 42, 0], tol=0.1)


@S.test('trigger_motion: rotate velocity by values keeps its speed; !activator moves the player; a missing target '
        'does nothing', players=1, doc=DOC,
        covers=('trigger_motion', 'trigger_motion.target', 'trigger_motion.m_iszVelocity',
                'trigger_motion.m_iVelMode=0', 'trigger_motion.m_iVelMode=3'))
def trigger_motion_rotate(t):
    mover(t)
    t.point('trigger_motion', targetname='setvel', target='box', m_iszVelocity='100 0 0', m_iVelMode='0')
    t.point('trigger_motion', targetname='rot', target='box', m_iszVelocity='0 90 0', m_iVelMode='3')
    t.point('trigger_motion', targetname='lift', target='!activator', m_iszVelocity='0 0 300', m_iVelMode='0')
    t.point('trigger_motion', targetname='none', target='nothing_here', m_iszPosition='0 0 0')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.3)
    t.fire('setvel')
    t.fire('rot')
    t.wait(0.02)
    t.snap('r', 'box', 'velocity')
    t.fire('lift', activator='player1')
    t.wait(0.05)
    t.snap('p', 'player1', 'velocity')
    t.fire('none')
    t.wait(0.1)
    t.expect.value('r', 'box', 'velocity', [0, 100, 0], tol=1)
    t.expect.value('p', 'player1', 'velocity.z', 200, op='>')


@S.test('motion_manager: follow a moving entity every frame (set position), face a direction; Obey Trigger Mode Off '
        'stops it', doc=DOC,
        covers=('motion_manager', 'motion_manager:16', 'motion_manager.target', 'motion_manager.m_iszPosition',
                'motion_manager.m_iPosMode=0', 'motion_manager.m_iszFacing', 'motion_manager.m_iFaceMode=0'))
def motion_follow(t):
    mover(t, 'lamp', at=(0, 0, 100))
    t.point('path_corner', targetname='c1', at=(-150, -150, 64), target='c2')
    t.point('path_corner', targetname='c2', at=(150, -150, 64), target='c1')
    t.point('env_spritetrain', targetname='st', at=(0, 0, 64), target='c1', speed='150', model='sprites/glow01.spr',
            spawnflags='4')
    t.point('motion_manager', targetname='mm', target='lamp', m_iszPosition='st', m_iPosMode='0',
            m_iszFacing='1 0 0', m_iFaceMode='0', spawnflags='16', names=('m_iszPosition',))
    t.wait(0.3)
    t.fire('mm', use='on')
    t.wait(0.5)
    t.snap('on', 'lamp', 'origin', 'angles')
    t.snap('on', 'st', 'origin')
    t.fire('mm', use='on')       # obey: already on
    t.fire('mm', use='off')
    t.wait(0.2)
    t.snap('off', 'lamp', 'origin')
    t.wait(0.5)
    t.snap('off2', 'lamp', 'origin')
    t.expect.distance('on', 'lamp', 'st', 0, 3, flat=False)
    t.expect.value('on', 'lamp', 'angles.y', 0, tol=0.5)
    t.expect.same('off', 'off2', 'lamp', 'origin')         # off: the lamp stays where it was
    t.expect.spawned('@motion_thread', count=1)


@S.test('motion_manager: offset position (a speed), rotate by values per second; Start On (32); not obeying: '
        'Toggle restarts', doc=DOC, covers=('motion_manager', 'motion_manager:32', 'motion_manager.m_iPosMode=1'))
def motion_offset(t):
    mover(t, 'drift', at=(-150, 0, 100))
    t.point('motion_manager', targetname='mm', target='drift', m_iszPosition='100 0 0', m_iPosMode='1',
            m_iszFacing='0 90 0', m_iFaceMode='2', spawnflags='32')
    t.wait(0.2)
    t.snap('s0', 'drift', 'origin', 'angles')
    t.wait(1.0)
    t.snap('s1', 'drift', 'origin', 'angles')
    t.fire('mm', use='off')
    t.wait(0.3)
    t.snap('s2', 'drift', 'origin')
    t.wait(0.3)
    t.snap('s3', 'drift', 'origin')
    t.expect.value('s1', 'drift', 'origin.x', t._abs((-150, 0, 0))[0] + 80, op='>')
    t.expect.same('s2', 's3', 'drift', 'origin')


@S.test('motion_manager: set velocity, accelerate, follow position (velocity towards it), set avelocity, set angles '
        'to an entity\'s', doc=DOC,
        covers=('motion_manager', 'motion_manager:32', 'motion_manager.m_iszPosition', 'motion_manager.m_iPosMode=2',
                'motion_manager.m_iPosMode=3', 'motion_manager.m_iPosMode=4', 'motion_manager.m_iszFacing',
                'motion_manager.m_iFaceMode=3', 'motion_manager.m_iFaceMode=4'))
def motion_modes(t):
    t.point('info_target', targetname='a', at=(-150, -100, 100))
    t.point('info_target', targetname='b', at=(-150, 0, 100))
    t.point('info_target', targetname='c', at=(-150, 100, 100))
    info(t, 'goal', at=(150, 100, 100))
    info(t, 'aimer', at=(0, 0, 0), angles='0 135 0')
    t.point('motion_manager', targetname='m1', target='a', m_iszPosition='50 0 0', m_iPosMode='2',
            m_iszFacing='0 30 0', m_iFaceMode='3', spawnflags='32')
    t.point('motion_manager', targetname='m2', target='b', m_iszPosition='0 100 0', m_iPosMode='3',
            m_iszFacing='aimer', m_iFaceMode='4', spawnflags='32', names=('m_iszFacing',))
    t.point('motion_manager', targetname='m3', target='c', m_iszPosition='goal', m_iPosMode='4', spawnflags='32',
            names=('m_iszPosition',))
    t.wait(0.5)
    t.snap('s', 'a', 'velocity', 'avelocity')
    t.snap('s', 'b', 'velocity', 'angles')
    t.snap('s', 'c', 'velocity')
    t.expect.value('s', 'a', 'velocity', [50, 0, 0], tol=0.1)
    t.expect.value('s', 'a', 'avelocity', [0, 30, 0], tol=0.1)
    t.expect.value('s', 'b', 'velocity.y', 20, op='>')
    t.expect.value('s', 'b', 'angles', [0, 135, 0], tol=0.5)
    t.expect.value('s', 'c', 'velocity.x', 50, op='>')


GAME_WEAPONS = ('weapon_spa weapon_medikit weapon_fa18 weapon_nm33 weapon_federal weapon_pm66 weapon_ben weapon_sw '
                'weapon_chaingun weapon_wong weapon_rc28 weapon_k444 weapon_canon weapon_glauncher weapon_warhammer '
                'weapon_sword weapon_chainsaw weapon_rpg weapon_tripmine weapon_satchel weapon_handgrenade '
                'weapon_snark').split()


@S.test('game_player_settings: take each game weapon while it is held, and while another is held; the player keeps '
        'a working weapon (the removed active weapon crashed the server: GetNextBestWeapon with no current weapon)',
        players=1, doc=DOC, size=(512, 512, 256),
        covers=('game_player_settings', 'player_hasweapon', 'player_hasweapon.weapon_name',
                'player_hasweapon.pass_target', 'game_player_settings.weapon_spa=-1',
                'game_player_settings.weapon_medikit=-1', 'game_player_settings.weapon_fa18=-1',
                'game_player_settings.weapon_nm33=-1', 'game_player_settings.weapon_federal=-1',
                'game_player_settings.weapon_pm66=-1', 'game_player_settings.weapon_ben=-1',
                'game_player_settings.weapon_sw=-1', 'game_player_settings.weapon_chaingun=-1',
                'game_player_settings.weapon_rc28=-1', 'game_player_settings.weapon_k444=-1',
                'game_player_settings.weapon_canon=-1', 'game_player_settings.weapon_glauncher=-1',
                'game_player_settings.weapon_chainsaw=-1', 'game_player_settings.weapon_rpg=-1',
                'game_player_settings.weapon_satchel=-1', 'game_player_settings.weapon_handgrenade=-1',
                'game_player_settings.weapon_snark=-1'))
def take_every_weapon(t):
    tough(t, 'player1', '100000')
    for i, w in enumerate(GAME_WEAPONS):
        t.point('game_player_settings', targetname='give%d' % i, **{w: '1'})
        t.point('game_player_settings', targetname='take%d' % i, **{w: '-1'})
        t.point('player_hasweapon', targetname='has%d' % i, weapon_name=w, pass_target='still%d' % i)
        info(t, 'still%d' % i)
    t.point('game_player_settings', targetname='givesword', weapon_sword='1')
    for i, w in enumerate(GAME_WEAPONS):
        # held: give it, select it, take it
        t.fire('give%d' % i, activator='player1')
        t.wait(0.1)
        t.cmd('player1', w)
        t.wait(0.3)
        t.fire('take%d' % i, activator='player1')
        t.wait(0.2)
        t.fire('has%d' % i, activator='player1')
        # not held: give it, hold the sword (or another), take it
        t.fire('give%d' % i, activator='player1')
        t.fire('givesword', activator='player1')
        t.wait(0.1)
        t.cmd('player1', 'weapon_sword' if w != 'weapon_sword' else 'weapon_warhammer')
        t.wait(0.3)
        t.fire('take%d' % i, activator='player1')
        t.wait(0.2)
        t.fire('has%d' % i, activator='player1')
    t.snap('end', 'player1', 'health', 'weapons')
    for i in range(len(GAME_WEAPONS)):
        t.expect.not_used('still%d' % i)
    t.expect.value('end', 'player1', 'health', 1000, op='>')


@S.test('player_speed absolute 160 after a live co-op penalty (SboLevelPenalty lowers the max speed at once): the '
        'player still moves at 160', players=2, doc=DOC,
        covers=('player_speed', 'player_speed.maxspeed', 'player_speed.value_notion=1'))
def player_speed_after_penalty(t):
    # last in the suite: it restarts the round and leaves player2 five levels down
    t.point('player_speed', targetname='abs', maxspeed='160', value_notion='1')
    t.console('mp_friendlyfire 1')
    t.console('mp_XP 1')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)     # past the spawn protection
    t.teleport('player2', (-150, 100, 0), yaw=0)
    t.fire('abs', activator='player2')
    t.wait(0.2)
    t.snap('before', 'player2', 'sbo_maxspeed', 'maxspeed')
    t.mark('kill')
    t.damage('player1', 5000, 0, 'player2')     # a teamkill: player2's level -5, max speed -25 at once
    t.wait(0.5)
    t.snap('after', 'player2', 'sbo_maxspeed', 'maxspeed', 'level')
    t.teleport('player2', (-150, -100, 0), yaw=0)
    t.wait(0.2)
    hold(t, 'player2', 400, 0, 0.8)
    t.snap('run', 'player2', 'velocity')
    release(t, 'player2')
    t.wait(0.2)
    t.expect.value('after', 'player2', 'level', -5)
    t.expect.value('after', 'player2', 'sbo_maxspeed', 0, op='>')
    t.expect.value('run', 'player2', 'velocity.x', 155, op='>=')
    t.expect.value('run', 'player2', 'velocity.x', 165, op='<=')
