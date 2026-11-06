"""func_tank kill credit (fix/tank-kill-credit, Valve halflife#561): the player at a tank's controls gets the kill of
its rockets, laser and mortar shells, as of its bullets and of his own weapons (frags / co-op XP, DeathMsg and
SboDeathMsg killer). With nobody at the controls the tank keeps it (killer 0 in the feed).
Kill feed weapon codes: func_tank 'tank' (d_tank), func_tanklaser 'tanklaser' (d_tanklaser), func_tankrocket
'rpg_rocket' (d_rpg_rocket), func_tankmortar 'env_explosion' (the skull: no icon)."""

from etest import Suite

SUITE = Suite('tank', 'func_tank: kill credit of player-controlled rockets, lasers, mortars and bullets', players=2)
S = SUITE
DOC = 'dev/KILLFEED_PORT.md'
ROOM = (512, 512, 256)

# the tank 80 units north of where its controller stands, facing north (the controller looks north), the victim 320
# units north of the tank
TANK_AT, PLAYER_AT, VICTIM_AT = (0, -120, 40), (0, -200, 0), (0, 200, 0)
TANK_KEYS = dict(angles='0 90 0', yawrate='60', yawrange='30', pitchrate='30', pitchrange='30', barrel='24',
                 firerate='1', yawtolerance='15', pitchtolerance='5', persistence='1')     # FGD defaults for the last three
KINDS = {
    'func_tank': dict(code='tank', keys=dict(bullet='1', bullet_damage='1000', firerate='4')),
    'func_tanklaser': dict(code='tanklaser', keys=dict(laserentity='beam')),
    'func_tankrocket': dict(code='rpg_rocket', keys={}),
    'func_tankmortar': dict(code='env_explosion', keys=dict(iMagnitude='300')),
}


def tough(t, player):
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def tank(t, classname, spawnflags):
    keys = dict(TANK_KEYS)
    keys.update(KINDS[classname]['keys'])
    t.brush(classname, size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags=str(spawnflags), **keys)
    if classname == 'func_tanklaser':
        t.point('env_laser', at=(TANK_AT[0], TANK_AT[1], TANK_AT[2] + 16), targetname='beam',
                texture='sprites/laserbeam.spr', damage='1000', width='10')


def fresh_round(t):
    t.console('mp_friendlyfire 1')
    t.console('mp_XP 1')
    t.console('mp_killfeed_npc 1')
    t.console('test_round restart')     # both alive again (an earlier test's victim)
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn)


def man_and_fire(t, seconds, victim=None):
    """player1 takes the tank's controls (looking north, the way it faces) and holds attack. victim: an entity put
    back in the line of fire first (a monster walks about)."""
    tough(t, 'player1')
    t.teleport('player1', PLAYER_AT, yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)                         # landed: the tank lets go of a player who moves 30 units
    t.fire('tk', activator='player1')   # CFuncTank::Use with a player: StartControl
    t.wait(1.0)
    if victim:
        t.teleport(victim, VICTIM_AT, yaw=270)
    t.mark('fire')
    t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
    t.wait(seconds)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.fire('tk', activator='player1', use='off')


def player_victim(t):
    t.teleport('player2', VICTIM_AT, yaw=270)
    t.kv('player2', 'health', '1')


def monster_victim(t):
    t.point('monster_cyborg', at=VICTIM_AT, targetname='npc', angles='0 270 0')


def manned(cls, code):
    def manned_player(t):
        tank(t, cls, 32)            # Controllable
        fresh_round(t)
        player_victim(t)
        man_and_fire(t, 2.0)
        t.expect.damaged('player2', attacker='player1', after='fire')
        t.expect.msg('DeathMsg', fields={'killer': 1, 'victim': 2, 'weapon': code}, after='fire', count=1)
        t.expect.msg('ScoreInfo', fields={'client': 1}, after='fire')

    def manned_monster(t):
        tank(t, cls, 32)
        monster_victim(t)
        fresh_round(t)
        t.kv('npc', 'health', '1')
        t.flags('npc', '+FL_NOTARGET')
        t.teleport('player2', (200, -200, 0), yaw=90)
        t.flags('player2', '+FL_NOTARGET')
        tough(t, 'player2')                 # the rocket's and the mortar's blasts reach it
        t.flags('player1', '+FL_NOTARGET')  # the monster stays where it is, in the line of fire
        man_and_fire(t, 2.0, victim='npc')
        t.flags('player1', '-FL_NOTARGET')
        t.flags('player2', '-FL_NOTARGET')
        t.expect.damaged('npc', attacker='player1', after='fire')
        t.expect.msg('SboDeathMsg', fields={'killer': 1, 'victim': 0, 'weapon': code}, after='fire', count=1)
        t.expect.msg('SboXP', fields={'client': 1}, after='fire')

    manned_player.__name__ = 'manned_player_' + cls
    manned_monster.__name__ = 'manned_monster_' + cls
    S.test('%s at the controls kills a player: player1 is the killer, weapon "%s"' % (cls, code), players=2,
           size=ROOM, doc=DOC)(manned_player)
    S.test('%s at the controls kills a monster: player1 gets the kill and the XP (SboDeathMsg killer 1)' % cls,
           players=2, size=ROOM, doc=DOC)(manned_monster)


for _cls, _kind in KINDS.items():
    manned(_cls, _kind['code'])


@S.test('func_tankrocket with nobody at the controls (active, targets players) kills a player: the tank keeps the '
        'kill (killer 0, weapon "rpg_rocket")', players=2, size=ROOM, doc=DOC)
def unmanned_rocket(t):
    tank(t, 'func_tankrocket', 1 | 2)     # Active, target players
    fresh_round(t)
    t.flags('player1', '+FL_NOTARGET')
    tough(t, 'player1')
    t.teleport('player1', (200, -200, 0), yaw=90)
    t.mark('fire')
    player_victim(t)
    t.wait(5.0)
    t.fire('tk', use='off')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.damaged('player2', attacker='tk', after='fire')
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 2, 'weapon': 'rpg_rocket'}, after='fire', count=1)


# --- Featureful keys (docs/FEATUREFUL_OPTIONS.md, batch 8): bulletCount, firemaster, extra_barrels,
# bullet_gib_policy, m_iClass ---------------------------------------------------------------------------------------
OPT = 'FEATUREFUL_OPTIONS.md#func_tank-keys-and-gib-scale-batch-8'


def gun(t, name='tk', at=TANK_AT, spawnflags=32, **kv):
    keys = dict(TANK_KEYS)
    keys.update(bullet='1', bullet_damage='1', firerate='4')
    keys.update(kv)
    t.brush('func_tank', size=(32, 32, 32), at=at, origin=True, targetname=name, spawnflags=str(spawnflags), **keys)


def manned_at(t, seconds, victim='player2'):
    """player1 at the controls of 'tk', holding attack at a tough player2 (or `victim`) 320 units north"""
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)
    tough(t, 'player2')
    if victim == 'player2':
        t.teleport('player2', VICTIM_AT, yaw=270)
    else:
        t.teleport('player2', (200, -200, 0), yaw=90)
        t.flags('player2', '+FL_NOTARGET')
    t.flags('player1', '+FL_NOTARGET')
    man_and_fire(t, seconds, victim=None if victim == 'player2' else victim)
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player2', '-FL_NOTARGET')


@S.test('func_tank "bulletCount" 2: a player at the controls holding attack for 2 s (8 shots at rate 4) hits twice, '
        'then the tank clicks (weapons/357_cock1.wav) instead of firing', players=2, size=ROOM, doc=OPT)
def bullet_count(t):
    gun(t, bulletCount='2')
    manned_at(t, 2.0)
    t.expect.damaged('player2', attacker='player1', after='fire', count=2)
    t.expect.sound('357_cock1', ent='tk', after='fire')


@S.test('func_tank without "bulletCount" never runs out (no click)', players=2, size=ROOM, doc=OPT)
def bullet_count_unset(t):
    gun(t)
    manned_at(t, 2.0)
    t.expect.damaged('player2', attacker='player1', after='fire', min=4)
    t.expect.sound('357_cock1', after='fire', count=0)


TE_SMOKE = 5
STEAM = 'model:sprites/steam1.spr'


@S.test('func_tank "Smoke on empty" (256) with "bulletCount" 2: one smoke puff (TE_SMOKE, sprites/steam1.spr, scale 20, '
        '5 fps) when the last bullet is fired', players=2, size=ROOM, doc=OPT)
def smoke_on_empty(t):
    gun(t, spawnflags=32 | 256, bulletCount='2')
    manned_at(t, 2.0)
    t.expect.damaged('player2', attacker='player1', after='fire', count=2)
    t.expect.msg('svc:23', count=1, fields={'te': TE_SMOKE, 'model': STEAM, 'scale': 20, 'framerate': 5},
                 after='fire')


@S.test('func_tank "bulletCount" 2 without "Smoke on empty": it runs out with no smoke puff', players=2, size=ROOM,
        doc=OPT)
def smoke_on_empty_unset(t):
    gun(t, bulletCount='2')
    manned_at(t, 2.0)
    t.expect.damaged('player2', attacker='player1', after='fire', count=2)
    t.expect.msg('svc:23', count=0, min=0, fields={'te': TE_SMOKE}, after='fire')


@S.test('func_tank "firemaster" off (a multisource nobody fired): no shot and no click (with one bullet)',
        players=2, size=ROOM, doc=OPT)
def fire_master(t):
    t.point('multisource', targetname='fm_off')
    t.point('trigger_relay', targetname='fm_src', target='fm_off')     # its one source, never fired
    gun(t, firemaster='fm_off', bulletCount='1', names=('firemaster',))
    manned_at(t, 1.5)
    t.expect.not_damaged('player2', after='fire')
    t.expect.sound('357_cock1', after='fire', count=0)


@S.test('func_tank "extra_barrels": the named (inactive) tank turns with the controlled one and fires with it (its '
        'target fires, its bullets hit)', players=2, size=ROOM, doc=OPT)
def extra_barrels(t):
    gun(t, extra_barrels='tkx', names=('extra_barrels',))
    gun(t, name='tkx', at=(TANK_AT[0], TANK_AT[1], TANK_AT[2] + 40), spawnflags=0, target='tkx_shot')
    t.point('info_target', targetname='tkx_shot')
    manned_at(t, 1.5)
    t.expect.fired('tkx_shot', caller='tkx', after='fire', min=2)


def gib_test(policy, damage, gibbed):
    def body(t):
        gun(t, bullet_damage=str(damage), firerate='1', **({'bullet_gib_policy': str(policy)} if policy else {}))
        t.point('monster_zombie', at=VICTIM_AT, targetname='npc', angles='0 270 0', spawnflags='16')
        fresh_round(t)
        t.kv('npc', 'health', '1')
        t.teleport('player2', (200, -200, 0), yaw=90)
        t.flags('player2', '+FL_NOTARGET')
        tough(t, 'player2')
        t.flags('player1', '+FL_NOTARGET')
        man_and_fire(t, 1.5, victim='npc')
        t.flags('player1', '-FL_NOTARGET')
        t.flags('player2', '-FL_NOTARGET')
        t.wait(0.5)
        t.expect.damaged('npc', attacker='player1', after='fire')
        if gibbed:
            t.expect.removed('npc')
        else:
            t.snap('s', 'npc', 'deadflag')
            t.expect.value('s', 'npc', 'deadflag', 0, op='>')
    body.__name__ = 'gib_policy_%d_damage_%d' % (policy, damage)
    S.test('func_tank "bullet_gib_policy" %d, Damage Per Bullet %d: the killed zombie %s' % (
        policy, damage, 'is gibbed' if gibbed else 'keeps its body'), players=2, size=ROOM, doc=OPT)(body)


gib_test(0, 4000, True)     # Default: over 16 always gibs (FireBullets)
gib_test(2, 4000, False)    # Never gib
gib_test(0, 10, False)      # Default: 16 or less never gibs
gib_test(3, 10, True)       # Always gib


# m_iClass: a tank turned on (Active) once the room is set, facing north; the candidates stand 280 units north, 100 apart; every
# monster is a Prisoner (16: the monsters leave it alone), Gagged, very tough; the player is tough
LEFT_AT, RIGHT_AT = (-100, 160, 0), (100, 160, 0)


def class_room(t, tank_class, player_in_range=True):
    keys = dict(TANK_KEYS)
    keys.update(bullet='1', bullet_damage='1', firerate='4', yawrange='60', maxRange='1000')
    if tank_class:
        keys['m_iClass'] = str(tank_class)
    t.brush('func_tank', size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags='0', **keys)
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(2.5)
    tough(t, 'player1')
    t.teleport('player1', (0, 200, 0) if player_in_range else (200, -200, 0), yaw=270)


def npc(t, classname, name, at, **kv):
    t.point(classname, at=at, targetname=name, angles='0 270 0', spawnflags='16', **kv)


def tough_npc(t, name):
    t.kv(name, 'health', '100000')
    t.kv(name, 'max_health', '100000')


@S.test('func_tank "m_iClass" 0 (default, Active): it shoots the player and never a grunt or a zombie beside him',
        players=1, size=(512, 512, 256), doc=OPT)
def class_default(t):
    npc(t, 'monster_human_grunt', 'g', LEFT_AT)
    npc(t, 'monster_zombie', 'z', RIGHT_AT)
    class_room(t, 0)
    tough_npc(t, 'g')
    tough_npc(t, 'z')
    t.mark('fire')
    t.fire('tk', use='on')     # Active from here (TankActivate)
    t.wait(3.0)
    t.fire('tk', use='off')
    t.expect.damaged('player1', attacker='tk', after='fire')
    t.expect.not_damaged('g', attacker='tk', after='fire')
    t.expect.not_damaged('z', attacker='tk', after='fire')


@S.test('func_tank "m_iClass" 11 (Player Ally): it shoots a grunt and a zombie, never the player', players=1,
        size=(512, 512, 256), doc=OPT)
def class_player_ally(t):
    npc(t, 'monster_human_grunt', 'g', LEFT_AT)
    class_room(t, 11)
    tough_npc(t, 'g')
    t.mark('fire')
    t.fire('tk', use='on')     # Active from here (TankActivate)
    t.wait(3.0)
    t.fire('tk', use='off')
    t.expect.damaged('g', attacker='tk', after='fire')
    t.expect.not_damaged('player1', attacker='tk', after='fire')


@S.test('func_tank "m_iClass" 11 shoots a zombie (Alien Military) and never the player', players=1,
        size=(512, 512, 256), doc=OPT)
def class_player_ally_alien(t):
    npc(t, 'monster_zombie', 'z', RIGHT_AT)
    class_room(t, 11)
    tough_npc(t, 'z')
    t.mark('fire')
    t.fire('tk', use='on')     # Active from here (TankActivate)
    t.wait(3.0)
    t.fire('tk', use='off')
    t.expect.damaged('z', attacker='tk', after='fire')
    t.expect.not_damaged('player1', attacker='tk', after='fire')


def ally_case(tank_class, shot):
    def body(t):
        npc(t, 'monster_human_grunt', 'ga', LEFT_AT, is_player_ally='1')
        class_room(t, tank_class, player_in_range=False)
        t.flags('player1', '+FL_NOTARGET')
        tough_npc(t, 'ga')
        t.mark('fire')
        t.fire('tk', use='on')
        t.wait(3.0)
        t.fire('tk', use='off')
        t.flags('player1', '-FL_NOTARGET')
        if shot:
            t.expect.damaged('ga', attacker='tk', after='fire')
        else:
            t.expect.not_damaged('ga', attacker='tk', after='fire')
    body.__name__ = 'class_%d_ally_grunt' % tank_class
    S.test('func_tank "m_iClass" %d and a grunt with "is_player_ally" 1: %s' % (
        tank_class, 'shot (Human Military hates player allies)' if shot else 'not shot'), players=1,
        size=(512, 512, 256), doc=OPT)(body)


ally_case(0, False)
ally_case(11, False)
ally_case(4, True)


@S.test('func_tank "m_iClass" 4 (Human Military) judges a monster by its "classify": a zombie with classify 4 is '
        'left alone, a plain zombie beside it is shot', players=1, size=(512, 512, 256), doc=OPT)
def class_keyed_classify(t):
    npc(t, 'monster_zombie', 'zh', LEFT_AT, classify='4')
    npc(t, 'monster_zombie', 'z', RIGHT_AT)
    class_room(t, 4, player_in_range=False)
    t.flags('player1', '+FL_NOTARGET')
    tough_npc(t, 'zh')
    tough_npc(t, 'z')
    t.mark('fire')
    t.fire('tk', use='on')     # Active from here (TankActivate)
    t.wait(3.0)
    t.fire('tk', use='off')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.damaged('z', attacker='tk', after='fire')
    t.expect.not_damaged('zh', attacker='tk', after='fire')
