"""Tier 5, batch 7: stock entity FGD items that no 1.38 map uses and no other test asserted: env_blood colors,
direction and On Player, env_beverage beverage types, game_player_equip weapon keys and master, trigger_hurt damage
types and keys."""

from etest import Suite

SUITE = Suite('fgd_stock', 'Tier 5: env_blood, env_beverage, game_player_equip, trigger_hurt FGD-only items',
              players=1)
S = SUITE
TE_BLOODSTREAM, TE_BLOODSPRITE = 101, 115
BLOOD_COLOR_RED, BLOOD_COLOR_YELLOW = 247, 195
COLORS = (0, 1, 70, 192, 244, 12, 5, 25, 59, 178, 92, 106, 124, 145, 161, 210, 214, 43)
WEAPONS = ('weapon_ben', 'weapon_canon', 'weapon_chaingun', 'weapon_chainsaw', 'weapon_fa18', 'weapon_federal',
           'weapon_glauncher', 'weapon_handgrenade', 'weapon_k444', 'weapon_medikit', 'weapon_nm33', 'weapon_pm66',
           'weapon_rc28', 'weapon_rpg', 'weapon_satchel', 'weapon_snark', 'weapon_spa', 'weapon_sw', 'weapon_sword',
           'weapon_tripmine', 'weapon_warhammer', 'weapon_wong')


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def pos(t, at):
    return [float(v) for v in t._abs(at)]


# ---------------------------------------------------------------------------------------------- env_blood
@S.test('env_blood Blood Color: every choice is the color of its blood drips (0 red: 247, 1 yellow: 195, the others '
        'the palette index)', doc='FEATUREFUL_OPTIONS.md',
        covers=tuple('env_blood.color=%d' % c for c in COLORS))
def blood_colors(t):
    for i, c in enumerate(COLORS):
        x, y = -200 + (i % 6) * 80, -150 + (i // 6) * 120
        t.point('env_blood', targetname='b%d' % c, at=(x, y, 48), color=str(c), amount='40')
        t.fire('b%d' % c)
    t.wait(0.1)
    for i, c in enumerate(COLORS):
        x, y = -200 + (i % 6) * 80, -150 + (i // 6) * 120
        want = {0: BLOOD_COLOR_RED, 1: BLOOD_COLOR_YELLOW}.get(c, c)
        t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE, 'color': want, 'pos': pos(t, (x, y, 48))}, count=1)


@S.test('env_blood Direction [LV] with Blood Stream (2): the stream goes that way', doc='FEATUREFUL_OPTIONS.md',
        covers=('env_blood.netname', 'env_blood:2'))
def blood_direction(t):
    t.point('env_blood', targetname='b', at=(0, 0, 48), color='1', amount='40', netname='0 0 1', spawnflags='2')
    t.fire('b')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BLOODSTREAM, 'dir': [0, 0, 1], 'color': BLOOD_COLOR_YELLOW}, count=1)


@S.test('env_blood On Player (4): the blood starts at the activator\'s eyes (within 10 units)', players=1,
        doc='FEATUREFUL_OPTIONS.md', covers=('env_blood:4',))
def blood_on_player(t):
    t.point('env_blood', targetname='b', at=(-200, -200, 48), color='1', amount='40', spawnflags='4')
    t.teleport('player1', (100, 100, 0))
    t.wait(0.2)
    t.snap('p', 'player1', 'origin', 'view_ofs')
    t.fire('b', activator='player1')
    t.wait(0.1)
    t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE, 'color': BLOOD_COLOR_YELLOW}, count=1)
    # the position: player1's eyes (+ up to 10 each way): far from the entity, which is 400 units away
    t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE, 'pos': pos(t, (-200, -200, 48))}, count=0)


# ---------------------------------------------------------------------------------------------- env_beverage
def beverage_case(skin):
    @S.test('env_beverage Beverage Type %d: the can it drops %s' % (
            skin, 'has a random skin 0-5' if skin == 6 else 'has skin %d' % skin), doc='FEATUREFUL_OPTIONS.md',
            players=1, covers=('env_beverage.skin=%d' % skin,))
    def case(t):
        t.point('env_beverage', targetname='bev', at=(0, 0, 48), skin=str(skin), health='5')
        t.teleport('player1', (0, -200, 0))
        t.fire('bev')
        t.wait(0.5)
        t.snap('s', '@item_sodacan', 'skin')
        t.kv('player1', 'health', '50')
        t.teleport('player1', (0, 0, 0))          # drinks it: no can left for the next test
        t.wait(0.5)
        t.expect.removed('@item_sodacan', count=1)
        t.expect.spawned('@item_sodacan', count=1)
        if skin == 6:
            t.expect.value('s', '@item_sodacan', 'skin', 0, op='>=')
            t.expect.value('s', '@item_sodacan', 'skin', 5, op='<=')
        else:
            t.expect.value('s', '@item_sodacan', 'skin', skin)
    case.__name__ = 'beverage_%d' % skin
    return case


for _sk in (1, 2, 3, 4, 6):
    beverage_case(_sk)


# ---------------------------------------------------------------------------------------------- game_player_equip
@S.test('game_player_equip weapon keys: each gives its weapon (player_hasweapon passes for each)', players=1,
        doc='FEATUREFUL_OPTIONS.md', covers=tuple('game_player_equip.%s' % w for w in WEAPONS))
def equip_weapons(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='eq', **{w: '1' for w in WEAPONS})
    for i, w in enumerate(WEAPONS):
        t.point('player_hasweapon', targetname='hw%d' % i, weapon_name=w, pass_target='yes%d' % i,
                fail_target='no%d' % i)
        info(t, 'yes%d' % i)
        info(t, 'no%d' % i)
    t.fire('strip', activator='player1')
    t.wait(0.2)
    t.mark('stripped')
    for i in range(len(WEAPONS)):
        t.fire('hw%d' % i, activator='player1')
    t.wait(0.2)
    t.fire('eq', activator='player1')
    t.wait(0.3)
    t.mark('given')
    for i in range(len(WEAPONS)):
        t.fire('hw%d' % i, activator='player1')
    t.wait(0.2)
    for i in range(len(WEAPONS)):
        t.expect.fired('no%d' % i, count=1, after='stripped', within=(0, 0.1))
        t.expect.fired('yes%d' % i, count=1, after='given')


@S.test('game_player_hurt Target: fired after it hurts the player, with the player as activator', players=1,
        doc='FEATUREFUL_OPTIONS.md', covers=('game_player_hurt.target',))
def hurt_target(t):
    t.point('game_player_hurt', targetname='hurt', dmg='5', target='hurt_done')
    info(t, 'hurt_done')
    t.wait(0.2)
    t.mark('go')
    t.fire('hurt', activator='player1')
    t.wait(0.3)
    t.expect.damaged('player1', after='go')
    t.expect.used('hurt_done', activator='player1', count=1, after='go')


@S.test('game_player_equip master: while the master is locked, triggering it gives nothing', players=1,
        doc='FEATUREFUL_OPTIONS.md', covers=('game_player_equip.master',))
def equip_master(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('env_state', targetname='gate')
    t.point('game_player_equip', targetname='eq', master='gate', weapon_sword='1')
    t.point('player_hasweapon', targetname='hw', weapon_name='weapon_sword', pass_target='yes', fail_target='no')
    info(t, 'yes')
    info(t, 'no')
    t.fire('strip', activator='player1')
    t.wait(0.2)
    t.fire('eq', activator='player1')
    t.wait(0.2)
    t.fire('hw', activator='player1')
    t.wait(0.1)
    t.expect.fired('no', count=1)
    t.expect.not_fired('yes')


# ---------------------------------------------------------------------------------------------- trigger_hurt
HURT_TYPES = (2, 512, 16384, 32768, 65536, 2097152, 4194304)


@S.test('trigger_hurt Damage Type 2 / 512 / 16384 / 32768 / 65536 / 2097152 / 4194304: the player standing in it '
        'takes damage of that type', players=1, doc='FEATUREFUL_OPTIONS.md', size=(512, 512, 200),
        covers=tuple('trigger_hurt.damagetype=%d' % d for d in HURT_TYPES))
def hurt_types(t):
    t.kv('player1', 'max_health', '100000')
    t.kv('player1', 'health', '100000')
    for i, d in enumerate(HURT_TYPES):
        x = -210 + i * 70
        t.trigger('trigger_hurt', size=(48, 48, 96), at=(x, 0, 0), dmg='5', damagetype=str(d))
    for i, d in enumerate(HURT_TYPES):
        x = -210 + i * 70
        t.mark('h%d' % i)
        t.teleport('player1', (x, 0, 0))
        t.wait(0.6)
    t.teleport('player1', (0, -200, 0))
    t.wait(0.2)
    for i, d in enumerate(HURT_TYPES):
        t.expect.damaged('player1', type=d, after='h%d' % i, within=(0, 0.6), min=1)


@S.test('trigger_hurt Kill target with a Target: removed the first time it hurts; Obey Trigger Mode 0: any trigger '
        'toggles it', players=1, doc='FEATUREFUL_OPTIONS.md', size=(512, 512, 200),
        covers=('trigger_hurt.killtarget', 'trigger_hurt.m_iObeyTriggerMode=0'))
def hurt_keys(t):
    t.kv('player1', 'max_health', '100000')
    t.kv('player1', 'health', '100000')
    info(t, 'victim')
    info(t, 'hurt')
    t.trigger('trigger_hurt', targetname='th', size=(64, 64, 96), at=(-100, 0, 0), dmg='5', target='hurt',
              killtarget='victim', m_iObeyTriggerMode='0', spawnflags='2')            # 2: Start Off
    t.teleport('player1', (-100, 0, 0))
    t.wait(0.3)
    t.mark('on')
    t.fire('th', use='on')
    t.wait(0.6)
    t.mark('off')
    t.fire('th', use='on')                 # not obeying: On toggles it off
    t.wait(0.2)
    t.mark('still')
    t.wait(0.8)
    t.teleport('player1', (100, 0, 0))
    t.expect.damaged('player1', after='on', within=(0, 0.6), min=1)
    t.expect.removed('victim', after='on', within=(0, 0.6))
    t.expect.not_damaged('player1', after='still')


@S.test('trigger_hurt Kill target without a Target: removed the first time it hurts', players=1,
        doc='FEATUREFUL_OPTIONS.md', size=(512, 512, 200), covers=('trigger_hurt.killtarget',))
def hurt_killtarget_only(t):
    t.kv('player1', 'max_health', '100000')
    t.kv('player1', 'health', '100000')
    info(t, 'victim')
    t.trigger('trigger_hurt', size=(64, 64, 96), at=(-100, 0, 0), dmg='5', killtarget='victim')
    t.mark('in')
    t.teleport('player1', (-100, 0, 0))
    t.wait(0.6)
    t.teleport('player1', (100, 0, 0))
    t.expect.damaged('player1', after='in', min=1)
    t.expect.removed('victim', after='in')


@S.test('FGD defaults of 2.x entities: an env_quakefx without frags / impulse gets the FGD defaults (70, 4); one '
        'with them keeps the map\'s values (20, 2)', players=0, doc='FEATUREFUL_OPTIONS.md',
        covers=('env_quakefx.frags', 'env_quakefx.impulse'))
def fgd_defaults_override(t):
    t.point('env_quakefx', targetname='q_def', at=(-100, 0, 64))
    t.point('env_quakefx', targetname='q_map', at=(100, 0, 64), frags='20', impulse='2')
    t.wait(0.2)
    for n in ('q_def', 'q_map'):
        t.snap('s', n, 'frags', 'impulse')
    x = t.expect
    x.value('s', 'q_def', 'frags', 70)
    x.value('s', 'q_def', 'impulse', 4)
    x.value('s', 'q_map', 'frags', 20)
    x.value('s', 'q_map', 'impulse', 2)
