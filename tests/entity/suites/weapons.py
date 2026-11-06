"""Weapons (branch fix/hlu-missed): halflife-updated fixes that SBO 2.0 missed."""

from etest import Suite

SUITE = Suite('weapons', 'weapons: halflife-updated fixes (fix/hlu-missed)')
S = SUITE
ROOM = (512, 512, 1024)
WEAPON_RPG = 2
WEAPON_FA18 = 6     # the default test loadout's best weapon (test_join 1 1 1 ...)


def tough(t, player='player1'):
    """The rocket's blast (250-400, radius 2.5 x that) would kill the test player and end the co-op round."""
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def rpg_ready(t, player='player1', at=(0, -200, 0)):
    """`player` picks up an RPG (one rocket in the clip, laser on), selects it, and looks up."""
    t.point('weapon_rpg', at=at)
    t.teleport(player, at, yaw=90)
    tough(t, player)
    t.wait(0.5)
    t.cmd(player, 'weapon_rpg')
    t.wait(1.5)                 # deploy
    t.look(player, -60, 90)
    t.wait(0.2)


@S.test('RPG: a rocket that detonates in flight gives the launcher back (it switches away when empty)', players=1,
        size=ROOM)
def rpg_inflight_detonate(t):
    rpg_ready(t)
    t.mark('fired')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.5)                 # ignited (0.4 s)
    t.kv('@rpg_rocket', 'movetype', '0')    # MOVETYPE_NONE: it stays in the air, its thinks go on
    t.wait(1.3)                 # past the first second after ignition
    t.mark('slowed')
    t.kv('@rpg_rocket', 'velocity', '0 0 0')    # FollowThink: under 1500 u/s -> Detonate(), no touch
    t.wait(2.0)
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_RPG, 'clip': 0}, after='fired')
    t.expect.removed('@rpg_rocket')
    t.expect.sound('debris', ent='@rpg_rocket', after='slowed')     # CGrenade::Explode, not RocketTouch
    t.expect.sound('mortarhit', ent='@rpg_rocket', count=0)          # RocketTouch never ran
    # empty, no rockets to reload and no active rocket: ItemPostFrame switches to the next best weapon
    # (before the fix the active-rocket count stayed 1, CanHolster said no and the empty RPG stayed up)
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_FA18}, after='slowed')


@S.test('RPG: the launcher is removed while its rocket flies (player_weaponstrip); the rocket explodes normally',
        players=2, size=ROOM)
def rpg_launcher_removed(t):
    # player2: player1 got an RPG in the test before, and a second one isn't picked up
    t.point('player_weaponstrip', targetname='strip')
    rpg_ready(t, 'player2', at=(150, 150, 0))
    t.mark('fire')
    t.move('player2', 0, 0, 3, '+attack')
    t.wait(0.05)
    t.fire('strip', activator='player2')    # RemoveAllItems: the RPG is dropped and removed 0.1 s later
    t.mark('stripped')
    t.wait(2.0)
    t.expect.msg('CurWeapon', to='player2', fields={'state': 1, 'id': WEAPON_RPG, 'clip': 0}, after='fire')
    t.expect.removed('@weapon_rpg')
    t.expect.spawned('@rpg_rocket')
    t.expect.removed('@rpg_rocket')
    t.expect.sound('mortarhit', ent='@rpg_rocket', after='stripped')    # RocketTouch ran after the strip


@S.test('NM33 zoomed, then a fall breaks the legs: SetFOV 0 (the player\'s default_fov), pev->fov 0 too',
        players=1, size=ROOM)
def zoom_leg_injury_fov(t):
    # A leg injury (player.cpp: leg hit, bad fall) unzooms to FOV 0, the player's own default_fov (1.38: a hard 90),
    # and sets pev->fov with m_iFOV, so the client data FOV the predicting client (cl_lw 1) uses agrees
    # (client.cpp UpdateClientData; halflife #3044/#3045).
    t.point('weapon_nm33', at=(-150, -150, 0))
    t.teleport('player1', (-150, -150, 0), yaw=90)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', 'weapon_nm33')
    t.wait(1.5)
    t.move('player1', 0, 0, 3, '+attack2')     # zoom level 1 -> fov 20
    t.wait(0.5)
    t.snap('zoomed', 'player1', 'fov')
    t.mark('jump')
    t.teleport('player1', (-150, -150, 800))
    t.wait(2.5)
    t.snap('fell', 'player1', 'fov')
    t.expect.value('zoomed', 'player1', 'fov', 20)
    t.expect.msg('SetFOV', to='player1', fields={'fov': 0}, after='jump')    # 0: the client uses default_fov
    t.expect.value('fell', 'player1', 'fov', 0)


def pump_after_shot(t, weapon):
    t.point(weapon, at=(150, -150, 0))
    t.teleport('player1', (150, -150, 0), yaw=90)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', weapon)
    t.wait(1.5)
    t.mark('fired')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(2.0)
    # 0.5 s after a shot that leaves shells in the clip, the pump sound (scock1.wav) plays from the player
    t.expect.sound('ben_fire' if weapon == 'weapon_ben' else 'sbarrel1', ent='player1', after='fired')
    t.expect.sound('scock1', ent='player1', after='fired', min=1)


@S.test('Ben: the pump sound plays after a shot (server)', players=1, size=ROOM)
def ben_pump(t):
    pump_after_shot(t, 'weapon_ben')


@S.test('SPA: the pump sound plays after a shot (server)', players=1, size=ROOM)
def spa_pump(t):
    pump_after_shot(t, 'weapon_spa')


@S.test('Satchel: with a charge out, another satchel is picked up (refused only past the carry limit, HL25)',
        players=1, size=ROOM)
def satchel_pickup_charge_out(t):
    t.point('weapon_satchel', at=(-150, 150, 0))
    t.point('weapon_satchel', at=(150, 150, 0), targetname='second')
    t.teleport('player1', (-150, 150, 0), yaw=270)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', 'weapon_satchel')
    t.wait(1.5)
    t.move('player1', 0, 0, 3, '+attack')      # throw it: one charge out, none left in the pocket
    t.wait(1.0)
    t.mark('out')
    t.teleport('player1', (150, 150, 0), yaw=270)
    t.wait(1.0)
    t.expect.spawned('@monster_satchel', count=1)
    t.expect.removed('second')                  # picked up (1.38 / develop: refused while a charge is out)
    t.expect.msg('AmmoPickup', to='player1', after='out')


@S.test('Wong: the decal of a punch comes 0.3 s after the hit (Smack), not on the next frame', players=1, size=ROOM)
def wong_smack_delay(t):
    t.teleport('player1', (0, 230, 0), yaw=90)  # 26 units from the north wall, facing it
    t.wait(0.5)
    t.cmd('player1', 'weapon_wong')
    t.wait(1.5)
    t.mark('punch')
    t.move('player1', 0, 0, 2, '+attack')
    t.wait(1.0)
    t.expect.sound('wong_hit', ent='player1', after='punch', within=(0, 0.15))
    t.expect.msg('svc:23', after='punch', within=(0.25, 0.6))     # SVC_TEMPENTITY: DecalGunshot from Smack
    t.expect.no_msg('svc:23', after='punch', within=(0, 0.2))


@S.test('RPG: not switched to when the last hand grenade is thrown (ITEM_FLAG_NOAUTOSWITCHTO, HL25)', players=2,
        size=ROOM)
def rpg_no_autoswitch(t):
    # player2 has nothing since the strip in t02: a loaded RPG and two hand grenades only
    t.point('weapon_rpg', at=(150, -150, 0))
    t.point('weapon_handgrenade', at=(150, -150, 0))
    t.teleport('player2', (150, -150, 0), yaw=270)
    tough(t, 'player2')
    t.wait(0.5)
    t.cmd('player2', 'weapon_handgrenade')     # (picking up the RPG switched to it: pickups are unchanged)
    t.wait(1.5)
    t.mark('grenades')
    for _ in range(2):
        t.move('player2', 0, 0, 5, '+attack')
        t.wait(2.0)
    t.wait(2.0)
    # the last grenade thrown: GetNextBestWeapon skips the RPG (develop: switched to it, CurWeapon state 1 id 2)
    t.expect.spawned('@grenade', count=2)
    t.expect.msg('CurWeapon', to='player2', fields={'state': 1, 'id': WEAPON_RPG}, count=0, after='grenades')


def snark_throw(t, pitch, ceiling=None, duck=False):
    t.point('weapon_snark', at=(-150, -150, 0))
    if ceiling:
        t.brush('func_wall', size=(256, 256, 16), at=(-150, -150, ceiling))
    t.teleport('player1', (-150, -150, 0), yaw=90)
    tough(t)
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.cmd('player1', 'weapon_snark')
    t.wait(1.5)
    t.look('player1', pitch, 90)
    t.wait(0.2)
    t.mark('throw')
    t.move('player1', 0, 0, 3, '+attack', *(['+duck'] if duck else []))
    t.wait(0.5)
    t.expect.spawned('@monster_snark', count=1)


@S.test('Snark: thrown looking straight up (open area)', players=1, size=ROOM)
def snark_up(t):
    snark_throw(t, -89)


@S.test('Snark: thrown looking straight up under a low ceiling (8 units above the head)', players=1, size=ROOM)
def snark_up_low_ceiling(t):
    snark_throw(t, -89, ceiling=80)


@S.test('Snark: thrown looking straight down', players=1, size=ROOM)
def snark_down(t):
    snark_throw(t, 89)


# weapon hand (feat/weapon-hand-switch, docs/WEAPON_HAND.md): the client's cl_righthand reaches the server in the
# userinfo; a left-handed player's rocket starts 8 units to his left (rpg.cpp), a right-handed one's 8 to his right.
# Facing yaw 90 (+y) the right is +x, whatever the pitch. Fresh players (3 and 4): a full RPG clip on pickup. The spot
# is clear of the line the suite's players stand on at a test's start (one of them under it lifts the shooter).
def rpg_rocket_side(t, player, at, hand_steps, right):
    rpg_ready(t, player, at=at)
    for value in hand_steps:
        t._step('test_userinfo', t.ref(player), 'cl_righthand', value)
        t.wait(0.1)
    t.mark('fired')
    t.move(player, 0, 0, 3, '+attack')
    t.wait(0.15)                # spawned, not yet ignited (0.4 s): it flies straight, x unchanged
    t.snap('rocket', '@rpg_rocket', 'origin')
    t.wait(2.0)
    t.expect.spawned('@rpg_rocket', count=1)
    t.expect.value('rocket', '@rpg_rocket', 'origin.x', t._abs(at)[0] + (8 if right else -8), tol=0.5)


@S.test('Weapon hand: cl_righthand 1 in the userinfo: the RPG rocket starts on the left', players=3, size=ROOM)
def rpg_left_hand(t):
    rpg_rocket_side(t, 'player3', (0, 120, 0), ['1'], right=False)


@S.test('Weapon hand: cl_righthand changed 1 -> 0 while playing: the RPG rocket starts on the right again',
        players=4, size=ROOM)
def rpg_right_hand_again(t):
    rpg_rocket_side(t, 'player4', (0, 120, 0), ['1', '0'], right=True)


WEAPON_GLAUNCHER = 15


@S.test('grenade launcher: reload with one grenade in the clip fills it to two (1.38: no reload until empty)',
        players=1, size=ROOM)
def glauncher_reload_one_left(t):
    t.point('weapon_glauncher', at=(-150, 150, 0))
    t.point('weapon_glauncher', at=(-150, -100, 0))     # a second launcher: its 2 grenades go to the spare ammo
    t.teleport('player1', (-150, 150, 0), yaw=90)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', 'weapon_glauncher')
    t.wait(1.5)
    t.teleport('player1', (-150, -100, 0), yaw=90)     # spare grenades for the reload
    t.wait(0.5)
    t.look('player1', -80, 90)      # the contact grenade bursts at the ceiling, far from the player
    t.wait(0.2)
    t.mark('fired')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.0)
    t.mark('reload')
    t.move('player1', 0, 0, 3, '+reload')
    t.wait(5.0)                     # reload time 4 s
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_GLAUNCHER, 'clip': 1}, after='fired')
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_GLAUNCHER, 'clip': 2}, after='reload')
