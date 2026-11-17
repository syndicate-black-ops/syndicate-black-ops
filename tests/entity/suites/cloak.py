"""Cloak (CBasePlayer::Cloak, the `cloak` command of the cloak cyberware, loadout cyberware 5).
A living player turns it on and off; a dead player can't turn it on; a player who dies while cloaked loses it,
gibbed or not. Its own suite: it changes the loadouts. player2 stays alive so a death does not end the co-op round.
SBO 2.1 stealth (CBasePlayer::CloakUpdate, Cloak_HidesFrom): the render amount follows what the player does (standing
still 30, a loud shot 255, falling back by 150 a second), and monsters see a cloaked player only within
2048 * (renderamt / 255) ^ 1.5 units (at least 96: standing still, only that), lose him after 3 s and forget him after
8 s without sight. Suites cloak_bk (Bounty Killer) and cloak_light (darkness)."""

from etest import Suite

SUITE = Suite('cloak', 'Cloak: on and off while alive, never while dead, dropped at death', players=2)
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
RENDER_NORMAL = 0
RENDER_TRANSALPHA = 4


def cloak_loadout(t):
    for p in ('player1', 'player2'):
        t.cmd(p, 'cyberware', '5')     # cyberware 5: the cloak
    t.console('test_round restart')        # respawn with the new loadout
    t.console('wait_for alive player1 20')
    t.wait(2.5)     # past the spawn protection of the round start
    t.teleport('player1', (-100, 0, 0), yaw=0)
    t.teleport('player2', (100, 0, 0), yaw=180)
    t.wait(0.5)


def kill(t, damage, mark):
    """player1 dies: 5 damage from 1 health leaves -4 (a corpse), 100 leaves -99 (gibbed, below -40)."""
    t.kv('player1', 'armorvalue', '0')
    t.kv('player1', 'health', '1')
    t.mark(mark)
    t.damage('player1', damage, 0)
    t.wait(0.5)


@S.test('a living player cloaks (render mode 4, amount 30 standing still) and uncloaks (render mode 0)', players=2,
        doc=DOC)
def alive_toggle(t):
    cloak_loadout(t)
    t.mark('on')
    t.cmd('player1', 'cloak')
    t.wait(0.3)
    t.snap('on', 'player1', 'rendermode', 'renderamt')
    t.wait(1.2)     # past the 1 s sound gap
    t.cmd('player1', 'cloak')
    t.wait(0.3)
    t.snap('off', 'player1', 'rendermode', 'renderamt')
    t.expect.value('on', 'player1', 'rendermode', RENDER_TRANSALPHA)
    t.expect.value('on', 'player1', 'renderamt', 30)       # 1.38: always 90
    t.expect.value('off', 'player1', 'rendermode', RENDER_NORMAL)
    t.expect.sound('cloak', ent='player1', after='on', min=2)


@S.test('a dead player can\'t cloak: render mode stays 0, no cloak sound', players=2, doc=DOC)
def dead_no_cloak(t):
    cloak_loadout(t)
    kill(t, 5, 'dead')
    t.snap('dead', 'player1', 'deadflag')
    t.mark('cmd')
    t.cmd('player1', 'cloak')
    t.wait(0.3)
    t.snap('after', 'player1', 'rendermode', 'renderamt')
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('after', 'player1', 'rendermode', RENDER_NORMAL)
    t.expect.sound('cloak', ent='player1', after='cmd', count=0)


@S.test('a cloaked player killed (corpse): the cloak is dropped', players=2, doc=DOC)
def cloaked_killed(t):
    cloak_loadout(t)
    t.cmd('player1', 'cloak')
    t.wait(1.2)
    t.snap('on', 'player1', 'rendermode')
    kill(t, 5, 'dead')
    t.snap('dead', 'player1', 'rendermode', 'deadflag')
    t.expect.value('on', 'player1', 'rendermode', RENDER_TRANSALPHA)
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('dead', 'player1', 'rendermode', RENDER_NORMAL)


@S.test('a cloaked player gibbed: the cloak is dropped too', players=2, doc=DOC)
def cloaked_gibbed(t):
    cloak_loadout(t)
    t.cmd('player1', 'cloak')
    t.wait(1.2)
    t.snap('on', 'player1', 'rendermode')
    kill(t, 100, 'gibbed')
    t.snap('dead', 'player1', 'rendermode', 'deadflag', 'effects')
    t.expect.value('on', 'player1', 'rendermode', RENDER_TRANSALPHA)
    t.expect.value('dead', 'player1', 'deadflag', 0, op='>')
    t.expect.value('dead', 'player1', 'rendermode', RENDER_NORMAL)


# ---------------------------------------------------------------------------------------------- stealth (SBO 2.1)
ROOM = (512, 512, 512)


def fed_and_cloaked_player(t, monster='monster_human_grunt'):
    """A monster in the south-west corner facing north-east, player1 (tough) 424 units away facing it, cloaked and
    standing still (render amount 30: seen only within 96 units).
    The monster waits as a prisoner (sees nobody) until the player is in place."""
    cloak_loadout(t)
    t.point(monster, at=(-200, -200, 0), targetname='npc', angles='0 45 0', spawnflags='16')
    t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
    t.teleport('player1', (100, 100, 0), yaw=225)
    t.kv('npc', 'health', '100000')     # the player's .444 shot must not kill it
    t.teleport('player2', (200, -200, 0), yaw=180)
    t.flags('player2', '+FL_NOTARGET')      # only player1 is a target
    t.kv('player1', 'health', '100000')
    t.kv('player1', 'max_health', '100000')
    t.console('test_lightlevel %s 200' % t.ref('player1'))     # a lit spot: no darkness bonus (suite cloak_light)
    t.cmd('player1', 'cloak')
    t.wait(1.0)


def wake(t):
    t.kv('npc', 'spawnflags', '0')
    t.fire('npc', use='on')


@S.test('a monster does not take a still, cloaked player 424 units away as its enemy; after he fires a loud gun '
        '(render amount 255) it does; the amount falls back after the shot', players=2, size=ROOM, doc=DOC)
def stealth_fed(t):
    fed_and_cloaked_player(t)
    t.snap('still', 'player1', 'renderamt')
    wake(t)
    t.wait(2.0)
    t.snap('a', 'npc', 'enemy')
    t.cmd('player1', 'weapon_k444')
    t.teleport('player1', (100, 100, 0), yaw=45)   # fires away from the Fed: a hit would start its pain schedules
    t.wait(1.0)
    t.mark('fire')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.1)
    t.snap('fired', 'player1', 'renderamt')
    t.wait(2.0)     # a schedule that ignores a new enemy (e.g. turning to the shot's sound) ends first
    t.snap('b', 'npc', 'enemy')
    t.fire('kill_npc')      # its shots would keep the player at the hurt amount (160)
    t.wait(4.0)     # the shot's sound (1000) fades by 250 a second: back to standing still after about 4 s
    t.snap('later', 'player1', 'renderamt')
    t.expect.value('still', 'player1', 'renderamt', 30)
    t.expect.value('a', 'npc', 'enemy', '')
    t.expect.value('fired', 'player1', 'renderamt', 200, op='>=')
    t.expect.value('b', 'npc', 'enemy', 'player')
    t.expect.value('later', 'player1', 'renderamt', 60, op='<')
    t.flags('player2', '-FL_NOTARGET')


@S.test('control: the same player not cloaked: the monster takes him as its enemy', players=2, size=ROOM, doc=DOC)
def stealth_fed_control(t):
    fed_and_cloaked_player(t)
    t.cmd('player1', 'cloak')       # off again
    t.wait(1.2)
    wake(t)
    t.wait(2.0)
    t.snap('a', 'npc', 'enemy')
    t.expect.value('a', 'npc', 'enemy', 'player')
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')


@S.test('the Cyborg (its own Look, 12000 units through walls) does not see a still, cloaked player 424 units away',
        players=2, size=ROOM, doc=DOC)
def stealth_cyborg(t):
    fed_and_cloaked_player(t, 'monster_cyborg')
    wake(t)
    t.wait(2.0)
    t.snap('a', 'npc', 'enemy')
    t.expect.value('a', 'npc', 'enemy', '')
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')


def lobby(t):
    """The suite map's lobby (absolute 0 0), relative to this test's room: out of the monster's sight."""
    x0, y0, _, x1, y1, _ = t.room
    return (-(x0 + x1) / 2.0, 100 - (y0 + y1) / 2.0, 0)


@S.test('a monster loses a cloaked enemy it no longer sees: still its enemy 2 s after he left, forgotten after 9 s; '
        'back in the room standing still 300+ units away he is not attacked; he fires and it takes him again',
        players=2, size=ROOM, doc=DOC)
def stealth_forget(t):
    fed_and_cloaked_player(t)
    wake(t)
    t.cmd('player1', 'weapon_k444')
    t.teleport('player1', (100, 100, 0), yaw=45)   # fires away from the Fed: a hit would start its pain schedules
    t.wait(1.0)
    t.move('player1', 0, 0, 3, '+attack')   # a loud shot: seen
    t.wait(2.0)
    t.snap('seen', 'npc', 'enemy')
    t.teleport('player1', lobby(t), yaw=0)  # out of sight
    t.wait(2.0)
    t.snap('lost2', 'npc', 'enemy')
    t.wait(7.5)                             # 9.5 s without sight
    t.snap('lost9', 'npc', 'enemy')
    t.teleport('player1', (-200, 200, 0), yaw=270)
    t.mark('back')
    t.wait(3.0)
    t.snap('back', 'npc', 'enemy')
    t.mark('fire2')
    t.teleport('player1', (-200, 200, 0), yaw=90)  # fires away from the Fed
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(3.0)
    t.snap('again', 'npc', 'enemy')
    t.expect.value('seen', 'npc', 'enemy', 'player')
    t.expect.value('lost2', 'npc', 'enemy', 'player')
    t.expect.value('lost9', 'npc', 'enemy', '')
    t.expect.value('back', 'npc', 'enemy', '')
    t.expect.not_damaged('player1', attacker='npc', after='back', within=(0, 3.0))
    t.expect.value('again', 'npc', 'enemy', 'player')
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')


# ------------------------------------------------------------------------- squad mates, and a shot from behind
def cloaked_still(t, at, yaw):
    """player1 cloaked, standing still at `at` (god mode: a Fed's hits must not raise his visibility), player2 out of
    the way with notarget."""
    cloak_loadout(t)
    t.teleport('player1', at, yaw=yaw)
    t.teleport('player2', (200, -200, 0), yaw=180)
    t.flags('player2', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.kv('player1', 'health', '100000')
    t.console('test_lightlevel %s 200' % t.ref('player1'))     # a lit spot
    t.cmd('player1', 'cloak')
    t.wait(1.0)


def fed(t, name, at, yaw, squad=None):
    kv = {'targetname': name, 'angles': '0 %d 0' % yaw, 'spawnflags': '16'}
    if squad:
        kv['netname'] = squad
    t.point('monster_human_grunt', at=at, **kv)
    t.point('trigger_relay', targetname='kill_' + name, killtarget=name)


def wake_fed(t, name):
    t.kv(name, 'spawnflags', '0')
    t.kv(name, 'health', '100000')
    t.fire(name, use='on')


@S.test('a Fed squad: the mate that bumps into a still, cloaked player shares him with the squad, but a mate 380 '
        'units away that does not see him himself does not fire at him', players=2, size=ROOM, doc=DOC)
def stealth_squad(t):
    fed(t, 'near', (70, 0, 0), 0, squad='sq')
    fed(t, 'far', (-200, 150, 0), 0, squad='sq')
    cloaked_still(t, (150, 0, 0), 180)
    t.mark('wake')
    wake_fed(t, 'far')
    wake_fed(t, 'near')
    t.wait(8.0)
    t.snap('a', 'near', 'enemy')
    t.snap('b', 'far', 'enemy')
    t.expect.value('a', 'near', 'enemy', 'player')          # within 96: seen
    t.expect.sound('weapons/', ent='far', after='wake', count=0, chan=1)     # it never fires (the player is in god mode)
    t.fire('kill_near')
    t.fire('kill_far')
    t.flags('player2', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')


@S.test('a Fed facing away, shot in the back by a cloaked player 300 units behind it: it turns toward the shot '
        'and, the shot showing him (render amount 255, about 1000 units in the dark), attacks him',
        players=2, size=ROOM, doc=DOC)
def stealth_shot_from_behind(t):
    fed(t, 'npc', (100, 0, 0), 0)
    cloaked_still(t, (-200, 0, 0), 0)
    t.cmd('player1', 'weapon_k444')
    wake_fed(t, 'npc')
    t.wait(1.5)
    t.snap('before', 'npc', 'enemy', 'angles')
    t.mark('shot')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(3.0)
    t.snap('after', 'npc', 'enemy', 'angles')
    t.expect.value('before', 'npc', 'enemy', '')
    t.expect.damaged('npc', attacker='player1', after='shot', within=(0, 0.5))
    t.expect.value('after', 'npc', 'enemy', 'player')
    t.expect.value('after', 'npc', 'angles.y', 90, op='>=')     # turned from 0 (east) toward the west
    t.expect.sound('weapons/', ent='npc', after='shot', min=1, chan=1)       # it fires at him
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')


@S.test('control: the same shot in the back from an uncloaked player: as in 1.38, the alert Fed takes cover from the '
        'hit and does not take the unseen shooter as its enemy',
        players=2, size=ROOM, doc=DOC)
def shot_from_behind_control(t):
    fed(t, 'npc', (100, 0, 0), 0)
    cloaked_still(t, (-200, 0, 0), 0)
    t.cmd('player1', 'cloak')       # off again
    t.cmd('player1', 'weapon_k444')
    wake_fed(t, 'npc')
    t.wait(1.5)
    t.mark('shot')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(3.0)
    t.snap('after', 'npc', 'enemy', 'angles')
    t.expect.value('after', 'npc', 'enemy', '')
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')


# ------------------------------------------------------------------------- tracking: a monster that spotted you
@S.test('tracking: a Fed that spotted a cloaked player (a loud shot) keeps him while he stays in view: crouched and '
        'still 424 units away, long after the shot faded (seen only within 96 by the table), it still has him and '
        'fires', players=2, size=ROOM, doc=DOC)
def track_still_in_view(t):
    fed(t, 'npc', (-200, -200, 0), 45)
    cloaked_still(t, (100, 100, 0), 45)        # facing away from the Fed: the shot must not hit it
    t.cmd('player1', 'weapon_k444')
    wake_fed(t, 'npc')
    t.wait(1.0)
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.0)
    t.move('player1', 0, 0, -1, '+duck')       # crouched from now on
    t.wait(5.0)                                 # the shot has faded: render amount 15
    t.mark('late')
    t.snap('p', 'player1', 'renderamt')
    t.wait(3.0)
    t.snap('a', 'npc', 'enemy')
    t.expect.value('p', 'player1', 'renderamt', 15)
    t.expect.value('a', 'npc', 'enemy', 'player')
    t.expect.sound('weapons/', ent='npc', after='late', min=1, chan=1)
    t.move('player1', 0, 0, 1)
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')


@S.test('tracking ends when the cloaked player, out of range, moves 300 units from where the Fed last saw him in '
        'range: lost, forgotten 9.5 s later, and not fired at', players=2, size=ROOM, doc=DOC)
def track_moved_away(t):
    fed(t, 'npc', (-200, -200, 0), 45)
    cloaked_still(t, (100, 100, 0), 45)
    t.cmd('player1', 'weapon_k444')
    wake_fed(t, 'npc')
    t.wait(1.0)
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(6.0)                                 # spotted, then the shot fades while he stays in view
    t.snap('tracked', 'npc', 'enemy')
    t.teleport('player1', (100, -200, 0), yaw=45)
    t.mark('moved')
    t.wait(9.5)
    t.snap('lost', 'npc', 'enemy')
    t.expect.value('tracked', 'npc', 'enemy', 'player')
    t.expect.value('lost', 'npc', 'enemy', '')
    t.expect.sound('weapons/', ent='npc', after='moved', count=0, chan=1, within=(1.0, 9.5))
    t.fire('kill_npc')
    t.flags('player2', '-FL_NOTARGET')
    t.flags('player1', '-FL_GODMODE')
