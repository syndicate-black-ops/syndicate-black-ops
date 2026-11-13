"""Keys that need a precache, changed at run time (trigger_changevalue): env_shooter Gib Model, the func_tank smoke and
flash sprites, env_warpball's second sprite are refused after the spawn and the entity keeps working with its old
value; monstermaker Monster Type and a new game_player_equip item are taken only for a class this map has precached.
A game_player_equip precaches every item it gives, so an item the map never placed works on the first equip.
Nothing on this suite's map places an item_battery, an item_airtank or a monster_werewolf (models/werewolf.mdl is
precached by another class, so only the console line and the maker's old class show the refusal)."""

from etest import Suite

SUITE = Suite('runtime_precache', 'Run-time keys that need a precache: refused, the old value kept', players=1)
SUITE.server_args = ('+developer', '1')
S = SUITE
DOC = 'FGD trigger_changevalue'
NEVER_MDL = 'models/et_runtime_never.mdl'
NEVER_SPR = 'sprites/et_runtime_never.spr'
REFUSED = "can't be changed at run time (it would need a precache)"


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def change(t, name, target, key, value):
    t.point('trigger_changevalue', targetname=name, target=target, m_iszValueName=key, m_iszNewValue=value,
            m_iszValueType='0')


# ---------------------------------------------------------------------------------------------- env_shooter
@S.test('env_shooter Gib Model changed at run time: refused (console line), the next shot still has the old model',
        players=1, doc=DOC, covers=('env_shooter.shootmodel', 'trigger_changevalue'))
def shooter_model(t):
    t.point('env_shooter', targetname='es', at=(0, 0, 100), m_iGibs='1', m_flGibLife='1', shootmodel='models/hgibs.mdl',
            m_iszSpawnTarget='tag', names=('m_iszSpawnTarget',))
    t.point('trigger_changevalue', targetname='tag', target='!activator', m_iszValueName='targetname',
            m_iszNewValue=t.name('proj'), m_iszValueType='0')
    change(t, 'cv', 'es', 'shootmodel', NEVER_MDL)
    t.wait(0.3)
    t.fire('cv')
    t.wait(0.1)
    t.fire('es')
    t.wait_frames(2)
    t.snap('s', 'proj', 'model')
    t.expect.value('s', 'proj', 'model', 'models/hgibs.mdl')
    t.expect.console('Key "shootmodel" of env_shooter "%s" %s' % (t.name('es'), REFUSED))
    t.expect.not_precached(NEVER_MDL)


# ---------------------------------------------------------------------------------------------- func_tank
def tank_case(key, sprite):
    @S.test('func_tank %s changed at run time: refused (console line), its shots still make the old sprite' % key,
            players=1, doc=DOC, covers=('func_tank.%s' % key, 'trigger_changevalue'))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.brush('func_tank', size=(32, 32, 32), at=(0, -40, 40), origin=True, targetname='tk', spawnflags='0',
                angles='0 90 0', yawrate='200', yawrange='60', pitchrate='100', pitchrange='30', barrel='24',
                firerate='4', yawtolerance='15', pitchtolerance='15', persistence='1', maxRange='200', target='shot',
                bullet='1', bullet_damage='0', **{key: sprite})
        info(t, 'shot')
        t.point('monster_target', targetname='foe', at=(0, 120, 40), frags='11')
        t.point('scripted_tanksequence', targetname='seq', at=(0, 0, 64), m_iszEntity='tk', m_iTurn='0',
                m_iShoot='1', m_iUntil='0', m_fDuration='0.1')
        change(t, 'cv', 'tk', key, NEVER_SPR)
        t.wait(1.5)
        t.fire('cv')
        t.wait(0.1)
        t.fire('seq')
        labels = []
        for k in range(8):
            t.wait(0.04)
            t.snap('f%d' % k, '@env_sprite', 'model')
            labels.append('f%d' % k)
        t.expect.value_any(labels, '@env_sprite', 'model', sprite)
        t.expect.console('Key "%s" of func_tank "%s" %s' % (key, t.name('tk'), REFUSED))
        t.expect.not_precached(NEVER_SPR)
    case.__name__ = 'tank_' + key
    return case


tank_case('spritesmoke', 'sprites/steam1.spr')
tank_case('spriteflash', 'sprites/xflare1.spr')


# ---------------------------------------------------------------------------------------------- env_warpball
@S.test('env_warpball Second Sprite changed at run time: refused (console line), the next play still makes its two '
        'sprites', players=1, doc=DOC, covers=('env_warpball.model2', 'trigger_changevalue'))
def warpball_model2(t):
    t.point('env_warpball', targetname='wb', at=(0, 0, 64), model2='sprites/xflare1.spr', spawnflags='20')
    change(t, 'cv', 'wb', 'model2', NEVER_SPR)
    t.wait(0.3)
    t.fire('cv')
    t.wait(0.1)
    t.mark('go')
    t.fire('wb')
    t.wait(0.1)
    t.expect.spawned('@env_sprite', count=2, after='go')
    t.expect.console('Key "model2" of env_warpball "%s" %s' % (t.name('wb'), REFUSED))
    t.expect.not_precached(NEVER_SPR)


# ---------------------------------------------------------------------------------------------- monstermaker
@S.test('monstermaker Monster Type changed at run time to a class every map precaches (monster_human_grunt): its next '
        'monster is one', players=1, doc=DOC, covers=('monstermaker.monstertype', 'trigger_changevalue'))
def maker_type_precached(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monstermaker', at=(0, 0, 0), targetname='mk', monstertype='monster_zombie', monstercount='1',
            spawnflags='0')
    change(t, 'cv', 'mk', 'monstertype', 'monster_human_grunt')
    t.wait(0.3)
    t.fire('cv')
    t.wait(0.1)
    t.mark('go')
    t.fire('mk')
    t.wait(1.0)
    t.expect.spawned('@monster_human_grunt', count=1, after='go')
    t.expect.spawned('@monster_zombie', count=0, min=0, after='go')


@S.test('monstermaker Monster Type changed at run time to a class this map has not precached (monster_werewolf): '
        'refused (console line), its next monster is of the old class', players=1, doc=DOC,
        covers=('monstermaker.monstertype', 'trigger_changevalue'))
def maker_type_refused(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monstermaker', at=(0, 0, 0), targetname='mk', monstertype='monster_zombie', monstercount='1',
            spawnflags='0')
    change(t, 'cv', 'mk', 'monstertype', 'monster_werewolf')
    t.wait(0.3)
    t.fire('cv')
    t.wait(0.1)
    t.mark('go')
    t.fire('mk')
    t.wait(1.0)
    t.expect.spawned('@monster_zombie', count=1, after='go')
    t.expect.absent('@monster_werewolf')
    t.expect.console('Key "monstertype" of monstermaker "%s" %s' % (t.name('mk'), REFUSED))


# ---------------------------------------------------------------------------------------------- game_player_equip
@S.test('game_player_equip giving an item the map never placed (item_battery): precached with the map, the first '
        'equip runs through it (no precache at run time, the server keeps running) and gives the weapon after it',
        players=1, doc=DOC, covers=('game_player_equip',))
def equip_unplaced_item(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='eq', spawnflags='1', item_battery='1', weapon_pm66='1')
    t.point('player_hasweapon', targetname='hw', weapon_name='weapon_pm66', pass_target='yes', fail_target='no')
    info(t, 'yes')
    info(t, 'no')
    t.wait(0.3)
    t.fire('strip', activator='player1')
    t.wait(0.2)
    t.mark('go')
    t.fire('eq', activator='player1')
    t.wait(0.3)
    t.fire('hw', activator='player1')
    t.wait(0.2)
    t.expect.fired('yes', count=1, after='go')
    t.expect.precached('models/w_battery.mdl')


@S.test('game_player_equip item added at run time: a class the map precached (weapon_pm66) is given, one it did not '
        '(item_airtank) is refused (console line)', players=1, doc=DOC,
        covers=('game_player_equip', 'game_player_equip.weapon_pm66', 'trigger_changevalue'))
def equip_runtime_item(t):
    t.point('player_weaponstrip', targetname='strip')
    t.point('game_player_equip', targetname='eq', spawnflags='1', weapon_sword='1')
    change(t, 'cv1', 'eq', 'weapon_pm66', '1')
    change(t, 'cv2', 'eq', 'item_airtank', '1')
    t.point('player_hasweapon', targetname='hw', weapon_name='weapon_pm66', pass_target='yes', fail_target='no')
    info(t, 'yes')
    info(t, 'no')
    t.wait(0.3)
    t.fire('cv1')
    t.fire('cv2')
    t.fire('strip', activator='player1')
    t.wait(0.2)
    t.mark('given')
    t.fire('eq', activator='player1')
    t.wait(0.3)
    t.fire('hw', activator='player1')
    t.wait(0.2)
    t.expect.fired('yes', count=1, after='given')
    t.expect.absent('@item_airtank')
    t.expect.console('Key "item_airtank" of game_player_equip "%s" %s' % (t.name('eq'), REFUSED))
    t.expect.not_precached('models/w_oxygen.mdl')
