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
        players=2, size=ROOM, covers=('player_weaponstrip',))
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


HANDGRENADE_DRAW = 7


@S.test('hand grenade: a new grenade is drawn after a throw (halflife #2495; 1.38: the idle animation, no draw)',
        players=1, size=ROOM)
def handgrenade_draw_after_throw(t):
    # A picked-up hand grenade gives two: after the first throw one is left, so WeaponIdle draws it (the throw
    # sequence is sent 0.5 s after the pin pull, the draw 0.5 s after that).
    t.point('weapon_handgrenade', at=(-150, 150, 0))
    t.teleport('player1', (-150, 150, 0), yaw=90)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', 'weapon_handgrenade')
    t.wait(1.5)                     # deploy (its draw is before the mark)
    t.look('player1', -80, 90)      # the grenade lands away from the player
    t.mark('pull')
    t.move('player1', 0, 0, 2, '+attack')
    t.wait(2.0)
    t.expect.spawned('@grenade', count=1, after='pull')
    t.expect.msg('svc:35', to='player1', fields={'anim': HANDGRENADE_DRAW}, after='pull', count=1)


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


CHAN_WEAPON = 1
CHAN_ITEM = 3


@S.test('Chainsaw: the flashlight click (CHAN_ITEM) leaves the running loop on CHAN_WEAPON playing (1.38: cut it)',
        players=1, size=ROOM)
def chainsaw_flashlight_keeps_loop(t):
    t.console('mp_flashlight 1')
    t.point('weapon_chainsaw', at=(-150, 150, 0))
    t.teleport('player1', (-150, 150, 0), yaw=90)
    t.wait(0.5)
    t.cmd('player1', 'weapon_chainsaw')
    t.wait(1.5)                     # deploy (0.5 s), then WeaponIdle starts the loop once
    t.mark('running')
    t.kv('player1', 'impulse', '100')          # flashlight on
    t.wait(0.5)
    t.kv('player1', 'impulse', '100')          # flashlight off
    t.wait(0.5)
    t.expect.sound('chain_idle', ent='player1', chan=CHAN_WEAPON, pitch=110)   # the loop started (a stop has pitch 100)
    t.expect.sound('flashlight1', ent='player1', chan=CHAN_ITEM, after='running', count=2)
    # nothing replaced or stopped the loop: no other sound on the player's CHAN_WEAPON after it started
    t.expect.sound('', ent='player1', chan=CHAN_WEAPON, after='running', count=0)


WEAPON_CHAINGUN = 8
CHAINGUN_SPINDOWN = 3
CHAINGUN_FIRE = 4


@S.test('Minigun: held fire runs the clip dry, then spins down and clicks; no shot, no fire sequence (1.38: fired on)',
        players=1, size=ROOM)
def chaingun_empty_held(t):
    # The picked-up minigun has 300 rounds in the clip and none spare: held fire empties it in about 15 s (spin-up
    # 0.4 s, then a shot every 0.04 s, in practice about every 0.05 s: server frames). The button stays down for 18.5 s:
    # the spin-down right after the last shot, the empty click 1 s later (its end), then the release.
    t.point('weapon_chaingun', at=(-150, 150, 0))
    t.teleport('player1', (-150, 150, 0), yaw=90)
    tough(t)
    t.wait(0.5)
    t.cmd('player1', 'weapon_chaingun')
    t.wait(1.5)                     # deploy
    t.mark('fire')
    t._step('test_move', t.ref('player1'), 0, 0, -1, '+attack')    # held until changed (as walk())
    t.wait(17.0)
    t.mark('empty')                 # the clip ran dry about 2 s ago; the button is still down
    t.wait(1.5)
    t._step('test_move', t.ref('player1'), 0, 0, 0)
    t.mark('released')
    t.wait(1.5)
    t.expect.msg('CurWeapon', to='player1', fields={'state': 1, 'id': WEAPON_CHAINGUN, 'clip': 0}, after='fire')
    t.expect.event('chaingun', ent='player1', count=300, after='fire')     # one event per shot, 300 rounds
    t.expect.event('chaingun', ent='player1', count=0, after='empty')
    t.expect.sound('gat_fire1', ent='player1', count=0, after='empty')
    # the spin-down while the button is still held (it replaces the fire sequence), not at the release
    t.expect.msg('svc:35', to='player1', fields={'anim': CHAINGUN_SPINDOWN}, after='fire', count=1)
    t.expect.msg('svc:35', to='player1', fields={'anim': CHAINGUN_SPINDOWN}, after='released', count=0)
    t.expect.sound('357_cock1', ent='player1', after='fire', count=1)     # the empty click, once per press


# Both fire buttons held: ItemPostFrame runs the secondary attack, then on the next frame the primary one whenever the
# secondary's own time is not up; a weapon whose two modes shoot sets both times on a shot (HL mp5.cpp).
@S.test('grenade launcher: primary and secondary fire held together shoot one grenade (1.38: two, one per mode)',
        players=5, size=ROOM)
def glauncher_both_buttons(t):
    t.point('weapon_glauncher', at=(150, 150, 0))
    t.teleport('player5', (150, 150, 0), yaw=90)
    tough(t, 'player5')
    t.wait(0.5)
    t.cmd('player5', 'weapon_glauncher')
    t.wait(1.5)
    t.look('player5', -80, 90)      # the contact grenade bursts at the ceiling, far from the player
    t.wait(0.2)
    t.mark('fired')
    t.move('player5', 0, 0, 10, '+attack', '+attack2')     # well under the 0.5 s between shots
    t.wait(0.3)
    t.expect.spawned('@grenade', count=1, after='fired')
    t.expect.msg('CurWeapon', to='player5', fields={'state': 1, 'id': WEAPON_GLAUNCHER, 'clip': 1}, after='fired')
    t.expect.msg('CurWeapon', to='player5', fields={'state': 1, 'id': WEAPON_GLAUNCHER, 'clip': 0}, count=0,
                 after='fired')


@S.test('RC28: primary and secondary fire held together shoot once, not a secondary and a primary shot',
        players=6, size=ROOM)
def rc28_both_buttons(t):
    t.point('weapon_rc28', at=(-150, 150, 0))
    t.teleport('player6', (-150, 150, 0), yaw=90)
    tough(t, 'player6')
    t.wait(0.5)
    t.cmd('player6', 'weapon_rc28')
    t.wait(1.5)
    t.mark('fired')
    t.move('player6', 0, 0, 5, '+attack', '+attack2')      # well under the 0.08 s between secondary shots
    t.wait(0.3)
    t.expect.sound('rc28_fire1', ent='player6', count=1, after='fired')


# The PM66 alt-fire burst (2 bullets) has its own time, 0.01 s: held, it fires at most one burst every 0.01 s of game
# time at any server frame rate (1.38 set none: one burst on every user command, every frame here).
def pm66_hold_alt_fire(t, player, at, ticrate):
    t.console('sys_ticrate %d' % ticrate)
    t.point('weapon_pm66', at=at)
    t.teleport(player, at, yaw=90)
    tough(t, player)
    t.wait(0.5)
    t.cmd(player, 'weapon_pm66')
    t.wait(1.5)                 # deploy
    t.mark('held')
    t.move(player, 0, 0, 40, '+attack2')    # 40 user commands: 0.2 s at 5 ms frames; under load frames grow
    t.wait(0.5)
    t.console('sys_ticrate 1000')
    # never more than one burst per 0.01 s of game time: at most 20 in the first 0.2 s (one more for a frame on the
    # edge). 5 ms frames (sys_ticrate 1000, idle machine) gave 1.38 a burst on each: the clip's 25 in 0.125 s.
    # Frame lengths vary with the machine's load, so the exact rate at fixed frame lengths is measured with the
    # map-test case runner (tests/fuzz/server: frametime, buttons)
    t.expect.sound('pm66_fire1', ent=player, min=1, after='held')
    t.expect.sound('pm66_fire1', ent=player, min=0, max=21, after='held', within=(0, 0.2))


@S.test('PM66 alt-fire held 40 frames at sys_ticrate 1000: at most one burst per 0.01 s (1.38: one every frame)',
        players=7, size=ROOM)
def pm66_burst_rate_1000(t):
    pm66_hold_alt_fire(t, 'player7', (0, -150, 0), 1000)


@S.test('PM66 alt-fire held 40 frames at sys_ticrate 500: at most one burst per 0.01 s too', players=8, size=ROOM)
def pm66_burst_rate_500(t):
    pm66_hold_alt_fire(t, 'player8', (0, 150, 0), 500)


# Held fire runs at the gun's set rate at any frame rate (halflife #3252, GetNextAttackDelay): the next shot counts
# from the time the last one was due, not from the frame it fired in. The PM66 fires every 0.04 s (50 rounds).
# Test players send one user command per server frame, its msec cut to whole milliseconds, so the weapon's clock runs
# up to 1 ms per frame behind the game time the sounds are stamped with: at 10 ms frames (sys_ticrate 100) that is too
# much to measure the rate, so the second frame rate is 30 a second.
WEAPON_PM66 = 18


def pm66_hold_fire(t, player, at, ticrate):
    t.console('sys_ticrate %d' % ticrate)
    t.point('weapon_pm66', at=at)
    t.teleport(player, at, yaw=90)
    tough(t, player)
    t.wait(0.5)
    t.cmd(player, 'weapon_pm66')
    t.wait(1.5)                 # deploy
    t.mark('held')
    t._step('test_move', t.ref(player), 0, 0, -1, '+attack')     # held until changed (as walk())
    t.wait(2.5)                 # the 50-round clip lasts 2 s
    t._step('test_move', t.ref(player), 0, 0, 0)
    t.wait(0.3)
    t.console('sys_ticrate 1000')
    return 'held'


@S.test('PM66 held fire at sys_ticrate 60: a shot every 0.04 s (1.38: every third 16.7 ms frame, 0.05 s)', players=9,
        size=ROOM)
def pm66_fire_rate_60(t):
    pm66_hold_fire(t, 'player9', (0, -150, 0), 60)
    # 0.04 s from shot to shot: 31 in the first 1.2 s (the first at once), 29 with the weapon clock up to 1 ms per
    # frame behind; never more than 31. 1.38's 0.05 s: 25
    t.expect.sound('pm66_fire1', ent='player9', min=28, max=31, after='held', within=(0, 1.2))
    # each shot still takes one round: the 50-round clip is empty after 50 shots, 50 events
    t.expect.event('pm66', ent='player9', count=50, after='held')
    t.expect.msg('CurWeapon', to='player9', fields={'state': 1, 'id': WEAPON_PM66, 'clip': 0}, after='held')


# 30 frames a second: a shot is due 0.04 s after the last, about 6 ms into the second 33 ms frame after it
@S.test('PM66 held fire at sys_ticrate 30: a shot every 0.04 s too, never faster (1.38: every second frame, 0.067 s)',
        players=10, size=ROOM)
def pm66_fire_rate_30(t):
    pm66_hold_fire(t, 'player10', (0, 150, 0), 30)
    t.expect.sound('pm66_fire1', ent='player10', min=28, max=31, after='held', within=(0, 1.2))
    t.expect.event('pm66', ent='player10', count=50, after='held')


# No catch-up shots: the count starts again after a pause (button up) or a block (FL_FROZEN, held fire), so the
# first shot fires at once and the next ones keep the 0.04 s rate (a shot owed from the pause would come at once).
@S.test('PM66 fire after a pause and after a freeze: the first shot at once, no burst of catch-up shots', players=11,
        size=ROOM)
def pm66_fire_no_banking(t):
    p = 'player11'
    t.console('sys_ticrate 60')
    t.point('weapon_pm66', at=(-150, -150, 0))
    t.teleport(p, (-150, -150, 0), yaw=90)
    tough(t, p)
    t.wait(0.5)
    t.cmd(p, 'weapon_pm66')
    t.wait(1.5)                 # deploy
    t._step('test_move', t.ref(p), 0, 0, -1, '+attack')
    t.wait(0.2)
    t._step('test_move', t.ref(p), 0, 0, 0)         # button up for 0.5 s
    t.wait(0.5)
    t.mark('pressed')
    t._step('test_move', t.ref(p), 0, 0, -1, '+attack')
    t.wait(0.3)
    t.flags(p, '+FL_FROZEN')                        # held fire, no shot for 0.5 s
    t.wait(0.5)
    t.mark('thawed')
    t.flags(p, '-FL_FROZEN')
    t.wait(0.3)
    t._step('test_move', t.ref(p), 0, 0, 0)
    t.wait(0.2)
    t.console('sys_ticrate 1000')
    for mark in ('pressed', 'thawed'):
        t.expect.sound('pm66_fire1', ent=p, min=1, after=mark, within=(0, 0.06))     # at once
        # 0.04 s apart: at most 7 in 0.25 s (one more for a frame on the edge); catch-up shots would make it more
        t.expect.sound('pm66_fire1', ent=p, min=4, max=8, after=mark, within=(0, 0.25))

# 64 weapons (feat/weapons-64, Half-Life Unified SDK HLU #98): weapon ids 32..63 are held in
# CBasePlayer::m_iWeaponBitsHigh (bit id - 32) and reach the client in the WeaponsHigh message; ids 0..31 and the suit
# stay in pev->weapons. weapon_test_hi (test layer) is the fists under id 40, registered at run time as WeaponMod does.
WEAPON_TEST_HI = 40
BIT_TEST_HI = 1 << (WEAPON_TEST_HI - 32)
SUIT_ONLY = -2147483648             # pev->weapons with only bit 31 (the suit): player_weaponstrip keeps the suit
DMG_NEVERGIB = 4096


@S.test('64 weapons: a weapon with id 40 is held, dropped (DropPlayerItem) and picked up by another player; '
        'pev->weapons keeps only the suit', players=10, size=ROOM, covers=('player_weaponstrip',))
def weapon_id_40(t):
    t.point('player_weaponstrip', targetname='strip64')
    t.teleport('player9', (-150, 150, 0), yaw=90)
    t.teleport('player10', (150, -150, 0), yaw=90)
    t.wait(2.5)                     # past the spawn protection of a player who joined for this test
    t.fire('strip64', activator='player9')
    t.wait(0.3)
    t.snap('before', 'player9', 'weapons', 'weapons_high')
    t.mark('give')
    t._step('test_weapon_hi', t.ref('player9'), WEAPON_TEST_HI)
    t.wait(1.5)                     # the only weapon: deployed at once
    t.snap('held', 'player9', 'weapons', 'weapons_high')
    t.mark('drop')
    t._step('test_drop', t.ref('player9'), 'weapon_test_hi')
    t.wait(1.5)                     # the weaponbox lands
    t.snap('dropped', 'player9', 'weapons', 'weapons_high')
    t.snap('other', 'player10', 'weapons_high')
    t.mark('pickup')
    t.teleport('@weaponbox', (150, -150, 2))
    t.wait(1.0)
    t.snap('picked', 'player10', 'weapons_high')
    # held: only m_iWeaponBitsHigh changed (pev->weapons keeps the suit alone), the client heard it, the HUD pickup
    # names id 40, and he holds it
    t.expect.value('before', 'player9', 'weapons', SUIT_ONLY)
    t.expect.value('before', 'player9', 'weapons_high', 0)
    t.expect.value('held', 'player9', 'weapons', SUIT_ONLY)
    t.expect.value('held', 'player9', 'weapons_high', BIT_TEST_HI)
    t.expect.msg('WeaponsHigh', to='player9', fields={'bits': BIT_TEST_HI}, after='give', within=(0, 0.5))
    t.expect.msg('WeapPickup', to='player9', fields={'id': WEAPON_TEST_HI}, after='give', within=(0, 0.5))
    t.expect.msg('CurWeapon', to='player9', fields={'state': 1, 'id': WEAPON_TEST_HI}, after='give', within=(0, 1.5))
    # dropped: in a weaponbox, the bit cleared and the client told
    t.expect.spawned('@weaponbox', count=1, after='drop')
    t.expect.value('dropped', 'player9', 'weapons', SUIT_ONLY)
    t.expect.value('dropped', 'player9', 'weapons_high', 0)
    t.expect.msg('WeaponsHigh', to='player9', fields={'bits': 0}, after='drop', within=(0, 0.5))
    # picked up by player10 from the box
    t.expect.value('other', 'player10', 'weapons_high', 0)
    t.expect.value('picked', 'player10', 'weapons_high', BIT_TEST_HI)
    t.expect.msg('WeaponsHigh', to='player10', fields={'bits': BIT_TEST_HI}, after='pickup', within=(0, 1.0))
    t.expect.msg('WeapPickup', to='player10', fields={'id': WEAPON_TEST_HI}, after='pickup', within=(0, 1.0))
    # players with SBO's own weapons only (ids 1..22) never get the message
    t.expect.msg('WeaponsHigh', to='player1', count=0)


@S.test('64 weapons: selecting the id 40 weapon switches to it (CurWeapon id 40), the fists stay held',
        players=11, size=ROOM)
def weapon_id_40_select(t):
    t.teleport('player11', (0, 0, 0), yaw=90)
    t.wait(2.5)
    t._step('test_weapon_hi', t.ref('player11'), WEAPON_TEST_HI)
    t.wait(0.5)
    t.mark('select')
    t.cmd('player11', 'weapon_test_hi')
    t.wait(1.5)
    t.snap('held', 'player11', 'weapons_high')
    t.expect.value('held', 'player11', 'weapons_high', BIT_TEST_HI)
    t.expect.msg('CurWeapon', to='player11', fields={'state': 1, 'id': WEAPON_TEST_HI}, after='select', within=(0, 1.5))


@S.test('64 weapons: a player holding the id 40 weapon dies: his weapons are removed, the id 40 bit too, the client '
        'told (WeaponsHigh 0)', players=12, size=ROOM)
def weapon_id_40_death(t):
    t.teleport('player12', (-150, -150, 0), yaw=90)
    t.wait(2.5)
    t._step('test_weapon_hi', t.ref('player12'), WEAPON_TEST_HI)
    t.wait(0.5)
    t.snap('held', 'player12', 'weapons_high')
    t.mark('kill')
    t.damage('player12', 5000, DMG_NEVERGIB)
    t.wait(0.5)
    t.snap('dead', 'player12', 'weapons_high')
    t.expect.value('held', 'player12', 'weapons_high', BIT_TEST_HI)
    t.expect.value('dead', 'player12', 'weapons_high', 0)
    t.expect.msg('WeaponsHigh', to='player12', fields={'bits': 0}, after='kill', within=(0, 0.5))
