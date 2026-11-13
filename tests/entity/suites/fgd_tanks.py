"""Tier 5, batch 8: FGD items of func_tank, func_tanklaser, func_tankrocket and func_tankmortar that no 1.38 map uses and
no other test asserted (docs/FEATUREFUL_OPTIONS.md, Featureful's tank keys): Behavior/Classification, Fire Master,
extra barrels, Trigger on firing, bullet count and Smoke on empty, smoke / flash sprites and the smoke render mode,
rotate sound, Target. Each tank is Active and shoots at a monster_target that counts as a player ally (or as the
class its Behavior hates); the player is notarget."""

from etest import Suite

SUITE = Suite('fgd_tanks', 'Tier 5: func_tank family, Featureful keys', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
TE_SMOKE = 5
KINDS = {
    'func_tank': dict(bullet='1', bullet_damage='0'),
    'func_tanklaser': dict(laserentity='beam'),
    'func_tankrocket': {},
    'func_tankmortar': dict(iMagnitude='0'),
}
CLASSES = tuple(KINDS)
SMOKE_MODES = (0, 1, 2, 3, 4, 5)


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def tank(t, cls, name='tk', at=(0, -40, 40), flags=1, target='shot', **kv):
    keys = dict(angles='0 90 0', yawrate='200', yawrange='60', pitchrate='100', pitchrange='30', barrel='24',
                firerate='4', yawtolerance='15', pitchtolerance='15', persistence='1', maxRange='200', target=target)    # 200: targets in its own room only
    keys.update(KINDS[cls])
    keys.update(kv)
    t.brush(cls, size=(32, 32, 32), at=at, origin=True, targetname=name, spawnflags=str(flags),
            names=('firemaster', 'extra_barrels', 'm_iszLocusFire'), **keys)
    if cls == 'func_tanklaser':
        t.point('env_laser', at=(at[0], at[1], at[2] + 16), targetname=keys.get('laserentity', 'beam'),
                texture='sprites/laserbeam.spr', damage='0', width='10')
    if target:
        info(t, target)


def foe(t, cls_value='11', at=(0, 120, 40), name='foe'):
    t.point('monster_target', targetname=name, at=at, frags=cls_value)


def short(cls):
    return cls.replace('func_', '')


# ---------------------------------------------------------------------------------------------- smoke render mode
def smoke_case(cls):
    @S.test('%s Smoke Sprite with each Smoke sprite render mode 0-5: the smoke sprite at a shot is drawn that way '
            '(0: Texture alpha)' % cls, players=1, doc=DOC,
            covers=('%s.spritesmoke' % cls, '%s.smokerendermode' % cls)
            + tuple('%s.smokerendermode=%d' % (cls, m) for m in SMOKE_MODES))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        for m in SMOKE_MODES:
            tank(t, cls, name='tk%d' % m, at=(-200 + m * 80, -120, 40), flags=0, target='shot%d' % m,
                 spritesmoke='sprites/steam1.spr', smokerendermode=str(m))
            t.point('scripted_tanksequence', targetname='seq%d' % m, at=(0, 0, 64), m_iszEntity='tk%d' % m,
                    m_iTurn='0', m_iShoot='1', m_iUntil='0', m_fDuration='0.1')
        foe(t)
        t.wait(1.5)                      # earlier tests' sprites gone
        for m in SMOKE_MODES:
            t.fire('seq%d' % m)
            for k in range(8):
                t.wait(0.05)
                t.snap('s%d_%d' % (m, k), '@env_sprite', 'rendermode', 'model')
            t.wait(1.5)                  # the sprite plays out before the next
        for m in SMOKE_MODES:
            labels = ['s%d_%d' % (m, k) for k in range(8)]
            t.expect.value_any(labels, '@env_sprite', 'model', 'sprites/steam1.spr')
            t.expect.value_any(labels, '@env_sprite', 'rendermode', m if m else 4)
    case.__name__ = 'smoke_' + short(cls)
    return case


def target_case(cls):
    @S.test('%s Behavior 0 (attack only players): it shoots at an active monster_target counting as a player ally '
            '(Target fired per shot)' % cls, players=1, doc=DOC,
            covers=('%s.m_iClass=0' % cls, '%s.target' % cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        tank(t, cls, m_iClass='0')
        foe(t, '11')
        t.wait(3.0)
        t.expect.used('shot', min=1)
    case.__name__ = 'target_' + short(cls)
    return case


def flash_case(cls):
    @S.test('%s Flash Sprite: a sprite of it at the barrel at each shot' % cls, players=1, doc=DOC,
            covers=('%s.spriteflash' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        tank(t, cls, flags=0, spriteflash='sprites/xflare1.spr')
        t.point('scripted_tanksequence', targetname='seq', at=(0, 0, 64), m_iszEntity='tk', m_iTurn='0',
                m_iShoot='1', m_iUntil='0', m_fDuration='0.1')
        foe(t)
        t.wait(1.5)
        t.fire('seq')
        labels = []
        for k in range(8):
            t.wait(0.04)
            t.snap('f%d' % k, '@env_sprite', 'model')
            labels.append('f%d' % k)
        t.expect.value_any(labels, '@env_sprite', 'model', 'sprites/xflare1.spr')
        t.expect.precached('sprites/xflare1.spr')
    case.__name__ = 'flash_' + short(cls)
    return case


# ---------------------------------------------------------------------------------------------- behavior
# Behavior -> (monster_target class it shoots, one it leaves alone)
BEHAVIOR = {0: ('2', '7'), 4: ('5', '3'), 5: ('4', '6'), 11: ('7', '3')}


def class_case(cls, c):
    hit, miss = BEHAVIOR[c]

    @S.test('%s Behavior/Classification %d: an Active tank leaves a monster_target counting as %s alone and shoots '
            'once another one counts as %s' % (cls, c, miss, hit), players=1, doc=DOC,
            covers=('%s.m_iClass' % cls, '%s.m_iClass=%d' % (cls, c)))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        tank(t, cls, m_iClass=str(c))
        foe(t, miss, at=(-60, 120, 40), name='miss')
        foe(t, '0', at=(60, 120, 40), name='hit')          # Ignored until the test turns it into the hated class
        t.wait(2.5)
        t.mark('hated')
        t.kv('hit', 'frags', hit)
        t.wait(2.5)
        t.expect.not_used('shot', within=(0, 2.4))
        t.expect.used('shot', after='hated', min=1)
    case.__name__ = 'class_%s_%d' % (short(cls), c)
    return case


# ---------------------------------------------------------------------------------------------- fire master, barrels
def firemaster_case(cls):
    @S.test('%s Fire Master: no shots while the master is off, shots once it is on' % cls, players=1, doc=DOC,
            covers=('%s.firemaster' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.point('env_state', targetname='fm')
        tank(t, cls, firemaster='fm', m_iClass='4')
        foe(t, '5')
        t.wait(2.5)
        t.mark('on')
        t.fire('fm', use='on')
        t.wait(2.5)
        t.expect.not_used('shot', within=(0, 2.4))
        t.expect.used('shot', after='on', min=1)
    case.__name__ = 'firemaster_' + short(cls)
    return case


def barrels_case(cls):
    @S.test('%s Extra barrels: the inactive tank named there fires when this one fires; Trigger on firing at each '
            'shot' % cls, players=1, doc=DOC,
            covers=('%s.extra_barrels' % cls, '%s.m_iszLocusFire' % cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        tank(t, cls, extra_barrels='xb', m_iszLocusFire='lfire', m_iClass='4')
        tank(t, cls, name='xb', at=(80, -40, 40), flags=0, target='xshot', laserentity='beam2')
        info(t, 'lfire')
        foe(t, '5')
        t.wait(3.0)
        t.expect.used('shot', min=1)
        t.expect.used('xshot', min=1)
        t.expect.used('lfire', min=1)
    case.__name__ = 'barrels_' + short(cls)
    return case


def bullets_case(cls):
    @S.test('%s Bullet count 2: two shots (Trigger on firing twice), then no more; Smoke on empty (256): a smoke puff '
            'at the last' % cls, players=1, doc=DOC, covers=('%s.bulletcount' % cls, '%s:256' % cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        # both start inactive (an Active tank shoots from the map start, before its test): turned on here
        tank(t, cls, flags=256, bulletcount='2', m_iszLocusFire='lfire', firerate='2', m_iClass='4')
        tank(t, cls, name='ctl', at=(100, -40, 40), flags=0, target='ctlshot', m_iClass='4', firerate='2',
             laserentity='beam2')                     # no bullet count: keeps firing
        info(t, 'lfire')
        foe(t, '5')
        t.fire('tk', use='on')
        t.fire('ctl', use='on')
        t.wait(2.0)
        t.mark('later')
        t.wait(2.0)
        t.expect.used('lfire', min=1)
        t.expect.not_used('shot', after='later')
        t.expect.used('ctlshot', after='later', min=1)
        t.expect.msg('svc:23', fields={'te': TE_SMOKE, 'scale': 20, 'framerate': 5}, count=1)
    case.__name__ = 'bullets_' + short(cls)
    return case


for _cls in CLASSES:
    target_case(_cls)
for _cls in CLASSES:
    smoke_case(_cls)
    flash_case(_cls)
    for _c in BEHAVIOR:
        if _c:
            class_case(_cls, _c)
    firemaster_case(_cls)
    barrels_case(_cls)
    bullets_case(_cls)


@S.test('func_tankmortar / func_tanklaser / func_tank Rotate Sound: plays while the tank turns', players=1, doc=DOC,
        covers=('func_tankmortar.rotatesound', 'func_tanklaser.rotatesound', 'func_tank.rotatesound'))
def rotate_sound(t):
    t.flags('player1', '+FL_NOTARGET')
    for i, cls in enumerate(('func_tankmortar', 'func_tanklaser', 'func_tank')):
        extra = {'laserentity': 'beam%d' % i} if cls == 'func_tanklaser' else {}
        tank(t, cls, name='r%d' % i, at=(-150 + i * 150, -120, 40), flags=0, target=None,
             rotatesound='plats/rackmove1.wav', **extra)
        t.point('scripted_tanksequence', targetname='turn%d' % i, at=(0, 0, 64), angles='0 270 0',
                m_iszEntity='r%d' % i, m_iTurn='1', m_iShoot='0', m_iUntil='0', m_fDuration='1')
    t.wait(0.3)
    t.mark('go')
    for i in range(3):
        t.fire('turn%d' % i)
    t.wait(1.2)
    for i in range(3):
        t.expect.sound('plats/rackmove1.wav', ent='r%d' % i, after='go', min=1)


@S.test('monster_target: the map\'s 33rd monster_target (this suite places more than 40 before it) is shot at by an '
        'automatic func_tank like the first ones', players=1, doc='FEATUREFUL_GUIDE_ENTITIES.md',
        covers=('monster_target.frags=11',))
def target_limit(t):
    t.flags('player1', '+FL_NOTARGET')
    tank(t, 'func_tank', m_iClass='0')
    foe(t, '11')
    t.wait(3.0)
    t.expect.used('shot', min=1)


@S.test('monster_target made later by a monstermaker (and made a player ally by a trigger_changevalue) is shot at by '
        'an automatic func_tank; once it is removed the tank stops, and a new one is shot at again', players=1,
        doc='FEATUREFUL_GUIDE_ENTITIES.md')
def target_spawned_later(t):
    t.flags('player1', '+FL_NOTARGET')
    tank(t, 'func_tank', m_iClass='0')
    # Cyclic (4): one child per trigger
    t.point('monstermaker', targetname='mk', at=(0, 120, 40), monstertype='monster_target', netname='late',
            monstercount='2', spawnflags='4', names=('netname',))
    t.point('trigger_changevalue', targetname='ally', target='late', m_iszValueName='frags', m_iszNewValue='11',
            m_iszValueType='0', names=('target',))
    t.point('trigger_relay', targetname='kill', killtarget='late', names=('killtarget',))
    t.wait(1.5)
    t.mark('none')
    t.wait(1.0)
    t.expect.not_used('shot', after='none', within=(0, 1.0))    # nothing to shoot at yet
    t.fire('mk', use='on')
    t.wait(0.2)
    t.fire('ally')
    t.mark('made')
    t.wait(2.0)
    t.expect.used('shot', min=1, after='made')
    t.fire('kill')
    t.wait(1.0)
    t.mark('gone')
    t.wait(1.5)
    t.expect.not_used('shot', after='gone', within=(0, 1.4))
    t.fire('mk', use='on')
    t.wait(0.2)
    t.fire('ally')
    t.mark('again')
    t.wait(2.0)
    t.expect.used('shot', min=1, after='again')
    t.expect.spawned('late', count=2)
    t.expect.removed('late', count=1)
