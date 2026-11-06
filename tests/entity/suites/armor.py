"""Co-op armor and loadout speed against the item descriptions (#SBO_Item_<item> in
gamedir/syndicate/ui/resource/language/sbo_ui_english.txt): each armor absorbs the share its description gives on
the parts it names (CBasePlayer::TakeDamage, m_iArmorType), and the tripmine costs no speed (CCoop::PlayerSpawn).

Each hit is test_damage with a hit group: TraceAttack (SBO's multipliers: head x10, chest x2, stomach x3, arms x1,
legs x0.5; the armor location 'loca') and then TakeDamage, from the world, 100 damage. The victim's health and armor
are set before every hit (an armor hit drains more armor than these hits leave), and the random rolls before the
armor (critical / weak hit, "Your armor saved your life", the skull's save) are seeded per hit (test_seed) to a
normal roll.

Health lost per hit (100 damage; multiplier first, then the armor, then the int cast):
                     head  chest  stomach  arm  leg   generic
  no armor           1000   200    300     100   50    100
  skin (60%)         1000    80    120      40   20    100   (1.38: 57%, 1000 86 130 43 21; generic not covered)
  cyber torso (50%)  1000   100    150     100   50    100   (1.38: chest only, stomach 300)
  torso+skull (70%)   300    60     90     100   50    100   (1.38: 67%, head 333 chest 66, stomach 300)
"""

from etest import Suite

SUITE = Suite('armor', 'Co-op armor absorption per hit group and the tripmine speed, as the item descriptions say',
              players=1)
S = SUITE
DOC = None

HEAD, CHEST, STOMACH, LEFTARM, RIGHTARM, LEFTLEG, RIGHTLEG, GENERIC = 1, 2, 3, 4, 5, 6, 7, 0
PARTS = (('head', HEAD), ('chest', CHEST), ('stomach', STOMACH), ('larm', LEFTARM), ('rarm', RIGHTARM),
         ('lleg', LEFTLEG), ('rleg', RIGHTLEG), ('generic', GENERIC))
HEALTH = 100000
SEED = 7

# test_join <player> blade light heavy item cyberware armor malefemale (coop.cpp CCoop::ClientCommand):
# chainsaw, rc28, federal, grenade, cyber legs; armor 1 skin (50), 2 cyber torso (100), 3 torso + skull (100), 4 none
def loadout(item=1, armor=4):
    return '2 1 2 %d 1 %d 1' % (item, armor)


def spawn(t, item=1, armor=4):
    t.console('mp_XP 1')
    t.console('test_join player1 %s' % loadout(item, armor))
    t.console('test_player level player1 0')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(2.5)     # past the 2 s spawn protection (CCoop::PlayerSpawn: m_iSpawnProtectTime)


def hits(t, armorvalue, expected):
    """One hit per part, health and armor set before each; expected: health lost per part name."""
    for name, group in PARTS:
        t.kv('player1', 'health', str(HEALTH))
        t.kv('player1', 'max_health', str(HEALTH))
        t.kv('player1', 'armorvalue', str(armorvalue))
        t.console('test_seed %d' % SEED)
        t.damage('player1', 100, 2, hitgroup=group)     # DMG_BULLET
        t.wait_frames(1)
        t.snap(name, 'player1', 'health')
    for name, _ in PARTS:
        t.expect.value(name, 'player1', 'health', HEALTH - expected[name], tol=0.01)
    t.console('test_join player1 1 1 1 1 1 1 1')   # back to the default loadout for the next suite test


@S.test('no armor: full damage on every part', players=1, doc=DOC)
def no_armor(t):
    spawn(t, armor=4)
    hits(t, 0, dict(head=1000, chest=200, stomach=300, larm=100, rarm=100, lleg=50, rleg=50, generic=100))


@S.test('armor skin: 60% absorbed on chest, stomach, arms and legs, not the head', players=1, doc=DOC)
def skin(t):
    spawn(t, armor=1)
    hits(t, 50, dict(head=1000, chest=80, stomach=120, larm=40, rarm=40, lleg=20, rleg=20, generic=100))


@S.test('cyber torso armor: 50% absorbed on the torso, stomach and back (chest and stomach hit groups)', players=1, doc=DOC)
def cyber_torso(t):
    spawn(t, armor=2)
    hits(t, 100, dict(head=1000, chest=100, stomach=150, larm=100, rarm=100, lleg=50, rleg=50, generic=100))


@S.test('torso and skull armor: 70% absorbed on the torso, stomach, back and head', players=1, doc=DOC)
def torso_skull(t):
    spawn(t, armor=3)
    hits(t, 100, dict(head=300, chest=60, stomach=90, larm=100, rarm=100, lleg=50, rleg=50, generic=100))


@S.test('loadout speed: tripmines cost nothing (330 as with grenades), satchels 5% (315)', players=1, doc=DOC)
def mine_speed(t):
    spawn(t, item=1)
    t.snap('grenade', 'player1', 'maxspeed', 'sbo_maxspeed')
    spawn(t, item=2)
    t.snap('tripmine', 'player1', 'maxspeed', 'sbo_maxspeed')
    spawn(t, item=3)
    t.snap('satchel', 'player1', 'maxspeed', 'sbo_maxspeed')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('grenade', 'player1', 'maxspeed', 330)
    t.expect.value('tripmine', 'player1', 'maxspeed', 330)
    t.expect.value('tripmine', 'player1', 'sbo_maxspeed', 330)
    t.expect.value('satchel', 'player1', 'maxspeed', 315)


@S.test('satchel charge: 200-300 damage, rolled per charge (#SBO_Item_satchel; 1.38: always 200)', players=1)
def satchel_damage(t):
    t.brush('func_breakable', size=(32, 32, 32), at=(48, 0, 0), targetname='box', health='100000', material='2')
    spawn(t, item=3)                    # satchel in the loadout
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.kv('player1', 'health', str(HEALTH))
    t.cmd('player1', 'weapon_satchel')
    t.wait(1.0)
    t.look('player1', 89, 0)            # straight down: the charge lands at his feet, beside the box
    t.console('test_seed 3141592653')     # a roll of about 240
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.5)
    t.snap('charge', '@monster_satchel', 'dmg')
    t.snap('before', 'box', 'health')
    t.mark('boom')
    t.move('player1', 0, 0, 3, '+attack')   # the second press detonates it
    t.wait(0.5)
    t.snap('after', 'box', 'health')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('charge', '@monster_satchel', 'dmg', 200, op='>')
    t.expect.value('charge', '@monster_satchel', 'dmg', 300, op='<=')
    # the box beside the charge takes most of it (RadiusDamage falloff): more than 1.38's whole 200 with this roll
    t.expect.damaged('box', after='boom', total=(205, 300))


@S.test('adrenaline surge: +75 health (175 at the spawn health of 100, #SBO_Item_adrenaline)', players=1)
def adrenaline_coop(t):
    t.console('test_join player1 2 1 2 1 4 4 1')     # cyberware 4: adrenaline, no armor
    t.console('test_player level player1 0')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.snap('spawn', 'player1', 'health', 'sbo_health')
    t.console('test_join player1 1 1 1 1 1 1 1')
    t.expect.value('spawn', 'player1', 'sbo_health', 100)
    t.expect.value('spawn', 'player1', 'health', 175)


@S.test('Assassin: the armor skin\'s weight whatever the armor pick (295 with each of the 4; 1.38: 265 to 325)',
        players=1)
def assassin_armor_speed(t):
    # cyberware 5 (Assassin) clears sidearm, primary and equipment and gives the armor skin: 300 - skin 30 + 25.
    # The chainsaw blade weighs nothing. 1.38 took the hidden armor pick's weight (skin / skull -30, torso -15, none 0).
    for armor in (1, 2, 3, 4):
        t.console('mp_XP 1')
        t.console('test_join player1 2 1 2 1 5 %d 1' % armor)
        t.console('test_player level player1 0')
        t.console('test_round restart')
        t.console('wait_for alive player1 20')
        t.wait(0.5)
        t.snap('armor%d' % armor, 'player1', 'maxspeed', 'armorvalue')
    t.console('test_join player1 1 1 1 1 1 1 1')
    for armor in (1, 2, 3, 4):
        t.expect.value('armor%d' % armor, 'player1', 'maxspeed', 295)
        t.expect.value('armor%d' % armor, 'player1', 'armorvalue', 50)
