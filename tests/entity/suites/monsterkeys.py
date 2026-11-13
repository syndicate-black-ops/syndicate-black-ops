"""Featureful monster keys, batch 2 onwards: per-monster keys (grunt, barney / scientist, apache, osprey).
Each test shows the key working next to the monster without it (what Half-Life / 1.38 does)."""

from etest import Suite

SUITE = Suite('monsterkeys', 'Featureful monster keys: grunt, barney / scientist, apache, osprey', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#monster-keys-batch-2'
WAKE = 6.0      # a named monster stands its first 5 s (SCHED_WAIT_TRIGGER) before it looks around
R_AL, R_DL, R_HT = -2, 1, 2


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


# ---------------------------------------------------------------------------------------------- monster_human_grunt
# ---------------------------------------------------------------------------------------------- barney / scientist
@S.test('talk monster "suspicious": a player-ally barney and a scientist with the key dislike the player, without '
        'it the barney is his ally; a hostile barney with it still hates him', players=1, doc=DOC,
        covers=('monster_barney', 'monster_barney.suspicious=1', 'monster_barney.is_player_ally=1', 'monster_scientist',
                'monster_scientist.suspicious=1'))
def talk_suspicious(t):
    t.flags('player1', '+FL_NOTARGET')
    # Prisoner (16): they see nobody, so the hostile one does not fight the others (IRelationship is the same)
    t.point('monster_barney', targetname='bsus', at=(-150, 150, 0), angles='0 270 0', is_player_ally='1',
            suspicious='1', spawnflags='16')
    t.point('monster_barney', targetname='bally', at=(-50, 150, 0), angles='0 270 0', is_player_ally='1',
            spawnflags='16')
    t.point('monster_barney', targetname='bhost', at=(50, 150, 0), angles='0 270 0', suspicious='1', spawnflags='16')
    t.point('monster_scientist', targetname='ssus', at=(150, 150, 0), angles='0 270 0', suspicious='1',
            spawnflags='16')
    t.wait(0.5)
    for n in ('bsus', 'bally', 'bhost', 'ssus'):
        t.snap('s', n, 'rel')
    t.expect.value('s', 'bsus', 'rel', R_DL)
    t.expect.value('s', 'bally', 'rel', R_AL)
    t.expect.value('s', 'bhost', 'rel', R_HT)
    t.expect.value('s', 'ssus', 'rel', R_DL)


def hit_facing(t, name, at, label, hits=1):
    """player1 looks straight at the monster and hits it `hits` times (1 s apart); snapshot `label` after."""
    t.teleport('player1', (at[0], at[1] - 150, 0), yaw=90)
    t.look('player1', 0, 90)
    for _ in range(hits):
        t.damage(name, 5, attacker='player1')
        t.wait(1.0)
    t.snap(label, name, 'rel')


@S.test('talk monster "tolerance": Absolute keeps a player-ally barney friendly after three hits by a player facing '
        'it; Default (HL) is provoked by one', players=1, doc=DOC,
        covers=('monster_barney', 'monster_barney.tolerance=5'))
def talk_tolerance_barney(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='babs', at=(-150, 150, 0), angles='0 270 0', is_player_ally='1',
            tolerance='5', spawnflags='2')
    t.point('monster_barney', targetname='bdef', at=(150, 150, 0), angles='0 270 0', is_player_ally='1',
            spawnflags='2')
    t.wait(0.5)
    hit_facing(t, 'babs', (-150, 150), 'a', hits=3)
    hit_facing(t, 'bdef', (150, 150), 'd')
    t.expect.value('a', 'babs', 'rel', R_AL)
    t.expect.value('d', 'bdef', 'rel', R_HT)


@S.test('talk monster "tolerance" Average on a scientist: two hits by a player facing him do not provoke him, the '
        'third does; Default (HL): the first does', players=1, doc=DOC,
        covers=('monster_scientist', 'monster_scientist.tolerance=3'))
def talk_tolerance_scientist(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='savg', at=(-150, 150, 0), angles='0 270 0', tolerance='3',
            spawnflags='2')
    t.point('monster_scientist', targetname='sdef', at=(150, 150, 0), angles='0 270 0', spawnflags='2')
    t.wait(0.5)
    hit_facing(t, 'savg', (-150, 150), 'a2', hits=2)
    hit_facing(t, 'savg', (-150, 150), 'a3')
    hit_facing(t, 'sdef', (150, 150), 'd1')
    t.expect.value('a2', 'savg', 'rel', R_HT, op='!=')
    t.expect.value('a3', 'savg', 'rel', R_HT)
    t.expect.value('d1', 'sdef', 'rel', R_HT)


def friends_watch(t, victim_kv, watcher_kv=None):
    """Two player-ally barneys side by side; the player kills 'victim'; snapshot 's' of 'watcher' after."""
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='victim', at=(-60, 150, 0), angles='0 270 0', is_player_ally='1',
            spawnflags='2', **victim_kv)
    t.point('monster_barney', targetname='watcher', at=(60, 150, 0), angles='0 270 0', is_player_ally='1',
            spawnflags='2', **(watcher_kv or {}))
    t.wait(0.5)
    t.damage('victim', 1000, attacker='player1')
    t.wait(0.5)
    t.snap('s', 'watcher', 'rel')


@S.test('talk monster "alert_friends" Never: killing it does not provoke the barney next to it', players=1, doc=DOC,
        covers=('monster_barney', 'monster_barney.alert_friends=3'))
def talk_alert_never(t):
    friends_watch(t, {'alert_friends': '3'})
    t.expect.value('s', 'watcher', 'rel', R_AL)


@S.test('talk monster "alert_friends" Default (HL): killing it provokes the barney next to it', players=1, doc=DOC,
        covers=('monster_barney',))
def talk_alert_default(t):
    friends_watch(t, {})
    t.expect.value('s', 'watcher', 'rel', R_HT)


@S.test('talk monster "tolerance" Absolute, don\'t alert friends: killing it does not provoke the barney next to it',
        players=1, doc=DOC, covers=('monster_barney', 'monster_barney.tolerance=6'))
def talk_tolerance_no_alerts(t):
    friends_watch(t, {'tolerance': '6'})
    t.expect.value('s', 'watcher', 'rel', R_AL)


@S.test('talk monster "tolerance" Absolute on the watcher: a friend killed next to it does not provoke it', players=1, doc=DOC,
        covers=('monster_barney', 'monster_barney.tolerance=5'))
def talk_tolerance_not_alertable(t):
    friends_watch(t, {}, {'tolerance': '5'})
    t.expect.value('s', 'watcher', 'rel', R_AL)


def use_on(t, name, label):
    t.mark(label)
    t.fire(name, activator='player1', caller='player1')
    t.wait(1.0)


@S.test('talk monster "RefusalSentence": a pre-disaster scientist refusing +use says that group (SC_SCARED); "null" '
        'says nothing; unset: SC_POK (HL)', players=1, doc=DOC,
        covers=('monster_scientist', 'monster_scientist.RefusalSentence', 'monster_scientist:256'))
def talk_refusal_sentence(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sref', at=(-150, 150, 0), angles='0 270 0', spawnflags='256',
            RefusalSentence='SC_SCARED')
    t.point('monster_scientist', targetname='snull', at=(0, 150, 0), angles='0 270 0', spawnflags='256',
            RefusalSentence='null')
    t.point('monster_scientist', targetname='sdef', at=(150, 150, 0), angles='0 270 0', spawnflags='256')
    t.wait(0.5)
    use_on(t, 'sref', 'u1')
    t.wait(3.0)     # the next one may speak (g_talkWaitTime)
    use_on(t, 'snull', 'u2')
    t.wait(3.0)
    use_on(t, 'sdef', 'u3')
    t.expect.sound('SC_SCARED', ent='sref', after='u1')
    t.expect.sound('!', ent='snull', count=0, after='u2')
    t.expect.sound('SC_POK', ent='sdef', after='u3')


@S.test('talk monster "SpeakAs" SC: a player-ally barney says the scientist line SC_OK when he starts following '
        '(a barney without it says BA_OK)', players=1, doc=DOC, covers=('monster_barney', 'monster_barney.SpeakAs'))
def talk_speak_as(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='bsc', at=(-100, 150, 0), angles='0 270 0', is_player_ally='1',
            followage_policy='0', SpeakAs='SC')
    t.point('monster_barney', targetname='bba', at=(100, 150, 0), angles='0 270 0', is_player_ally='1',
            followage_policy='0')
    t.wait(0.5)
    use_on(t, 'bsc', 'u1')
    t.wait(3.0)
    use_on(t, 'bba', 'u2')
    t.expect.sound('SC_OK', ent='bsc', after='u1')
    t.expect.sound('BA_OK', ent='bba', after='u2')


@S.test('monster_barney "bodystate" Gun Drawn: body 1 (the pistol in his hand) from the start; default holstered (0)',
        players=1, doc=DOC, covers=('monster_barney', 'monster_barney.bodystate=1'))
def barney_bodystate(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='bdrawn', at=(-100, 150, 0), angles='0 270 0', bodystate='1',
            spawnflags='2')
    t.point('monster_barney', targetname='bhol', at=(100, 150, 0), angles='0 270 0', spawnflags='2')
    t.wait(0.5)
    t.snap('s', 'bdrawn', 'body')
    t.snap('s', 'bhol', 'body')
    t.expect.value('s', 'bdrawn', 'body', 1)
    t.expect.value('s', 'bhol', 'body', 0)


# ---------------------------------------------------------------------------------------------- monster_apache
def apache(t, name, at, **kv):
    t.point('monster_apache', targetname=name, at=at, angles='0 90 0', **kv)


@S.test('monster_apache "rotorvolume" 0.3: its rotor sound plays at volume 0.3; without the key at full volume',
        size=(512, 512, 1024), players=1, doc=DOC, covers=('monster_apache', 'monster_apache.rotorvolume'))
def apache_rotor_volume(t):
    t.flags('player1', '+FL_NOTARGET')
    apache(t, 'aquiet', (-150, 0, 500), rotorvolume='0.3', spawnflags='4')
    apache(t, 'aloud', (150, 0, 500), spawnflags='4')
    t.wait(0.5)
    t.mark('go')
    t.fire('aquiet', use='on')
    t.fire('aloud', use='on')
    t.wait(1.5)
    t.expect.sound('ap_rotor2', ent='aquiet', vol=(0.29, 0.31), after='go')
    t.expect.sound('ap_rotor2', ent='aloud', vol=(0.99, 1.01), after='go')


@S.test('monster_apache "m_iObeyTriggerMode" 1: Off does not start a waiting apache, On does, Off stops it (rotor '
        'sound stopped); without the key a second trigger does nothing', size=(512, 512, 1024), players=1, doc=DOC,
        covers=('monster_apache', 'monster_apache:4', 'monster_apache.m_iObeyTriggerMode=1'))
def apache_obey_trigger(t):
    t.flags('player1', '+FL_NOTARGET')
    apache(t, 'aobey', (-150, 0, 500), m_iObeyTriggerMode='1', spawnflags='4')
    apache(t, 'astock', (150, 0, 500), spawnflags='4')
    t.wait(0.5)
    t.mark('off1')
    t.fire('aobey', use='off')
    t.wait(1.0)
    t.mark('on')
    t.fire('aobey', use='on')
    t.fire('astock', use='on')
    t.wait(1.5)
    t.mark('off2')
    t.fire('aobey', use='off')
    t.fire('astock', use='off')
    t.wait(1.0)
    t.expect.sound('ap_rotor2', ent='aobey', count=0, after='off1', within=(0, 1.0))
    t.expect.sound('ap_rotor2', ent='aobey', after='on', within=(0, 1.5))
    t.expect.sound('ap_rotor2', ent='aobey', flags=32, after='off2')
    t.expect.sound('ap_rotor2', ent='astock', flags=32, count=0, after='off2')


# ---------------------------------------------------------------------------------------------- monster_osprey
@S.test('monster_osprey "num" 2, "trigger_on_deploy", "trigger_on_deploy_grunt": four grunts died, the osprey drops '
        'two; its deploy target fires once (activator: the osprey), the grunt target once per grunt (activator: '
        'the grunt)', size=(512, 512, 1024), players=1, doc=DOC,
        covers=('monster_osprey', 'monster_osprey.num', 'monster_osprey.trigger_on_deploy',
                'monster_osprey.trigger_on_deploy_grunt'))
def osprey_num_triggers(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (220, -220, 0))
    for k in range(4):
        t.point('monster_human_grunt', targetname='og%d' % k, at=(-180 + 120 * k, -150, 0), angles='0 90 0',
                spawnflags='16')
    t.point('path_corner', targetname='op1', at=(0, 150, 600), target='op2', speed='300')
    t.point('path_corner', targetname='op2', at=(0, 0, 600), target='op3', speed='0')
    t.point('path_corner', targetname='op3', at=(0, 150, 600), speed='300')
    t.point('monster_osprey', targetname='osp', at=(0, -150, 600), angles='0 90 0', target='op1', spawnflags='64',
            num='2', trigger_on_deploy='o_deploy', trigger_on_deploy_grunt='o_grunt')
    info(t, 'o_deploy')
    info(t, 'o_grunt')
    t.wait(0.5)
    t.fire('osp', use='on')
    t.wait(0.5)                    # it counted the four grunts (FindAllThink); it deploys at op2 if one is dead
    for k in range(4):
        t.damage('og%d' % k, 1000)
    t.mark('dead')
    t.wait(12.0)
    t.expect.fired('o_deploy', count=1, activator='@monster_osprey', after='dead')
    t.expect.fired('o_grunt', count=2, activator='@monster_human_grunt', caller='@monster_osprey', after='dead')


@S.test('monster_osprey "num" 2 on a closed path with no corner of speed 400 or more: after the second drop (all '
        'four grunt slots alive) it flies on around the loop (the server does not hang in its path walk)',
        size=(512, 512, 1024), players=1, doc=DOC,
        covers=('monster_osprey', 'monster_osprey.num', 'monster_osprey.trigger_on_deploy'))
def osprey_closed_slow_path(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (220, -220, 0))
    for k in range(4):
        t.point('monster_human_grunt', targetname='og%d' % k, at=(-180 + 120 * k, -150, 0), angles='0 90 0',
                spawnflags='16')
    t.point('path_corner', targetname='op1', at=(0, 150, 600), target='op2', speed='300')
    t.point('path_corner', targetname='op2', at=(0, 0, 600), target='op3', speed='0')
    t.point('path_corner', targetname='op3', at=(150, 150, 600), target='op4', speed='300')
    t.point('path_corner', targetname='op4', at=(-150, 150, 600), target='op1', speed='300')
    t.point('monster_osprey', targetname='osp', at=(0, -150, 600), angles='0 90 0', target='op1', spawnflags='64',
            num='2', trigger_on_deploy='o_deploy')
    info(t, 'o_deploy')
    t.wait(0.5)
    t.fire('osp', use='on')
    t.wait(0.5)
    for k in range(4):
        t.damage('og%d' % k, 1000)
    t.mark('dead')
    t.wait(25.0)                   # two laps: a drop at each pass over op2 while a slot is free, then flying on
    t.snap('a', 'osp', 'origin')
    t.wait(2.0)
    t.snap('b', 'osp', 'origin')
    t.expect.fired('o_deploy', min=2, activator='@monster_osprey', after='dead')
    t.expect.moved('a', 'b', 'osp', min_dist=50)


# ---------------------------------------------------------------------------------------------- monstermaker (batch 3)
# ---------------------------------------------------------------------------------------------- flag 64 "wait until provoked"
# monster_dark and the alien slave family: while waiting, IRelationship toward players is R_NO (Half-Life's
# islave.cpp); 1.38 returned R_DL (dark) / R_NM (the others), so the flag never waited. A hit by the player ends it.
WAITERS = (('dark', 'monster_dark'), ('alien_slave', 'monster_alien_slave'), ('hunter', 'monster_hunter'),
           ('werewolf', 'monster_werewolf'), ('werewolf_hunter', 'monster_werewolf_hunter'))


def make_wait_test(key, classname):
    hunter = classname.endswith('hunter')     # the hunters see every monster within 12000 units (their own Look)

    def waits_until_provoked(t):
        t.flags('player1', '-FL_NOTARGET')     # earlier tests leave it set
        t.point(classname, targetname='npc', at=(-150, 0, 0), angles='0 0 0', spawnflags='64')
        tough(t)
        t.teleport('player1', (100, 0, 0), yaw=180)
        t.wait(0.5)
        t.fire('npc', use='on')     # ends a named monster's SCHED_WAIT_TRIGGER
        t.wait(3.0)
        t.snap('wait', 'npc', 'rel', 'enemy', 'schedule')
        t.mark('hit')
        t.damage('npc', 5, attacker='player1')
        t.wait(5.0)
        t.snap('mad', 'npc', 'rel', 'enemy', 'schedule')
        t.expect.value('wait', 'npc', 'rel', 0)          # R_NO: the player is ignored
        t.expect.not_damaged('player1', within=(0, 3.5))
        t.expect.value('mad', 'npc', 'rel', 0, op='>')    # provoked: an enemy again
        t.expect.value('mad', 'npc', 'enemy', 'player')
        t.expect.damaged('player1', attacker='npc', after='hit')
        if not hunter:     # a hunter meanwhile chases a monster of another room (players are what it ignores)
            t.expect.value('wait', 'npc', 'enemy', '')

    waits_until_provoked.__name__ = 'waits_until_provoked_' + key
    S.test('%s flag 64 "wait until provoked": it ignores the player in view until he hurts it, then attacks him'
           % classname, players=1, doc='FEATUREFUL_OPTIONS.md#monstermaker-batch-3',
           covers=(classname, classname + ':64'))(waits_until_provoked)


def tough(t, player='player1', hp='100000'):
    t.kv(player, 'health', hp)
    t.kv(player, 'max_health', hp)


for _key, _classname in WAITERS:
    make_wait_test(_key, _classname)


MAKER = 'FEATUREFUL_OPTIONS.md#monstermaker-batch-3'


def maker(t, name, cls='monster_scientist', at=(0, 0, 0), flags=4, **kv):
    """A cyclic maker (flag 4: one child per trigger) facing south, unlimited, children named name + '_kid'."""
    kv.setdefault('m_imaxlivechildren', '-1')
    kv.setdefault('monstercount', '-1')
    t.point('monstermaker', targetname=name, at=at, monstertype=cls, delay='0.5',
            netname=name + '_kid', spawnflags=str(flags), angles='0 270 0', names=('netname',), **kv)


@S.test('monstermaker "spawndelay" 1.5: its target fires (the child appears) 1.5 s after the trigger, not at once; '
        'without the key at once', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.spawndelay', 'monstermaker.target'))
def maker_spawndelay(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mdel', at=(-120, 0, 0), spawndelay='1.5', target='mdel_out')
    maker(t, 'mnow', at=(120, 0, 0), target='mnow_out')
    info(t, 'mdel_out')
    info(t, 'mnow_out')
    t.wait(0.5)
    t.mark('go')
    t.fire('mdel')
    t.fire('mnow')
    t.wait(2.5)
    t.expect.fired('mdel_out', count=1, after='go', within=(1.4, 1.8))
    t.expect.not_fired('mdel_out', after='go', within=(0, 1.3))
    t.expect.fired('mnow_out', count=1, after='go', within=(0, 0.2))
    t.expect.spawned('mdel_kid', count=1)


@S.test('monstermaker "delay_after_blocked" 0.5: with the spot taken it tries again 0.5 s later (not after its '
        'Frequency of 10 s)', players=1, doc=MAKER, covers=('monstermaker', 'monstermaker.delay_after_blocked'))
def maker_delay_after_blocked(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='blocker', at=(0, 0, 0), spawnflags='16')
    t.point('monstermaker', targetname='mblk', at=(0, 0, 0), monstertype='monster_scientist', monstercount='1',
            delay='10', delay_after_blocked='0.5', target='mblk_out', angles='0 270 0')
    info(t, 'mblk_out')
    t.wait(0.5)
    t.fire('mblk', use='on')       # tries at once: blocked
    t.wait(1.0)
    t.mark('free')
    t.teleport('blocker', (180, 180, 0))
    t.wait(1.5)
    t.expect.fired('mblk_out', count=1, after='free', within=(0, 0.7))


@S.test('monstermaker "classify" 11 (player ally) and "respawn_as_playerally": the made grunt and barney are allies '
        'of the player; a grunt from a maker without them hates him', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.classify=11', 'monstermaker.respawn_as_playerally=1'))
def maker_classify(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mcls', cls='monster_human_grunt', at=(-150, 0, 0), classify='11')
    maker(t, 'mrev', cls='monster_barney', at=(0, 0, 0), respawn_as_playerally='1')
    maker(t, 'mstk', cls='monster_human_grunt', at=(150, 0, 0))
    t.wait(0.5)
    for n in ('mcls', 'mrev', 'mstk'):
        t.fire(n)
    t.wait(0.5)
    for n in ('mcls', 'mrev', 'mstk'):
        t.snap('s', n + '_kid', 'rel')
    t.expect.value('s', 'mcls_kid', 'rel', R_AL)
    t.expect.value('s', 'mrev_kid', 'rel', R_AL)
    t.expect.value('s', 'mstk_kid', 'rel', R_HT)


@S.test('monstermaker "weapons" 10, "squadname" and "Ff" 1: the made grunt has shotgun + hand grenade (its shotgun '
        'body: head 2, gun 1), joins the squad of the leader another maker makes with the same squadname and ignores '
        'damage from an allied grunt; a grunt from a maker without the keys has 9mmAR + HG (body 0), no squad and '
        'takes it',
        players=1, doc='dev/sdk-update/server-ai.md',
        covers=('monstermaker', 'monstermaker.weapons=10', 'monstermaker.squadname', 'monstermaker.Ff=1',
                'monstermaker.childspawnflags'))
def maker_child_keys(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mwpn', cls='monster_human_grunt', at=(-150, 0, 0), weapons='10', squadname='mk_squad', Ff='1')
    # the leader (SquadLeader 32) wears the commander head (Half-Life's StartMonster), so the loadout is on the other
    maker(t, 'mmem', cls='monster_human_grunt', at=(0, 0, 0), squadname='mk_squad', childspawnflags='32')
    maker(t, 'mstk', cls='monster_human_grunt', at=(150, 0, 0))
    t.wait(0.5)
    for n in ('mmem', 'mwpn', 'mstk'):
        t.fire(n)
    t.wait(1.0)
    for n in ('mwpn', 'mmem', 'mstk'):
        t.snap('s', n + '_kid', 'weapons', 'body', 'netname', 'squad')
    for n in ('mwpn', 'mstk'):
        t.kv(n + '_kid', 'health', '100')
    t.damage('mwpn_kid', 10, attacker='mstk_kid')
    t.damage('mstk_kid', 10, attacker='mwpn_kid')
    t.wait(0.3)
    for n in ('mwpn', 'mstk'):
        t.snap('d', n + '_kid', 'health')
    t.expect.value('s', 'mwpn_kid', 'weapons', 10)
    t.expect.value('s', 'mwpn_kid', 'body', 6)          # HEAD_SHOTGUN 2 + GUN_SHOTGUN 1 * 4
    t.expect.value('s', 'mstk_kid', 'weapons', 3)       # the grunt's own: 9mmAR + HG
    t.expect.value('s', 'mstk_kid', 'body', 0)
    t.expect.value('s', 'mwpn_kid', 'netname', 'mk_squad')
    t.expect.value('s', 'mmem_kid', 'netname', 'mk_squad')
    t.expect.value('s', 'mwpn_kid', 'squad', 2)
    t.expect.value('s', 'mmem_kid', 'squad', 2)
    t.expect.value('s', 'mstk_kid', 'squad', 0)
    t.expect.value('d', 'mwpn_kid', 'health', 100)
    t.expect.value('d', 'mstk_kid', 'health', 90)


@S.test('monstermaker "yawdeviation" 90: each child faces the maker\'s angle turned by at most 90 degrees', players=1,
        doc=MAKER, covers=('monstermaker', 'monstermaker.yawdeviation'))
def maker_yawdeviation(t):
    t.flags('player1', '+FL_NOTARGET')
    for k in range(4):
        t.point('monstermaker', targetname='myaw%d' % k, at=(-180 + 120 * k, 0, 0), monstertype='monster_scientist',
                monstercount='1', netname='myaw%d_kid' % k, spawnflags='4', angles='0 180 0', yawdeviation='90',
                names=('netname',))
    t.wait(0.5)
    for k in range(4):
        t.fire('myaw%d' % k)
    t.wait(0.3)
    for k in range(4):
        t.snap('s', 'myaw%d_kid' % k, 'angles')
        t.expect.value('s', 'myaw%d_kid' % k, 'angles.y', 89.5, op='>=')
        t.expect.value('s', 'myaw%d_kid' % k, 'angles.y', 270.5, op='<=')


@S.test('monstermaker "target_activator": 2 the child is the activator of its target, 3 the activator that fired '
        'the maker; default the maker', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.target', 'monstermaker.target_activator=2',
                'monstermaker.target_activator=3'))
def maker_target_activator(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mta2', at=(-150, 0, 0), target='mta2_out', target_activator='2')
    maker(t, 'mta3', at=(0, 0, 0), target='mta3_out', target_activator='3')
    maker(t, 'mta0', at=(150, 0, 0), target='mta0_out')
    for n in ('mta2_out', 'mta3_out', 'mta0_out'):
        info(t, n)
    t.wait(0.5)
    for n in ('mta2', 'mta3', 'mta0'):
        t.fire(n, activator='player1')
    t.wait(0.5)
    t.expect.fired('mta2_out', activator='@monster_scientist')
    t.expect.fired('mta3_out', activator='player1')
    t.expect.fired('mta0_out', activator='@monstermaker')


@S.test('monstermaker "trigger_on_death_notice": fired (activator: the maker) when its child dies', players=1,
        doc=MAKER, covers=('monstermaker', 'monstermaker.trigger_on_death_notice'))
def maker_death_notice(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mdn', trigger_on_death_notice='mdn_dead')
    info(t, 'mdn_dead')
    t.wait(0.5)
    t.fire('mdn')
    t.wait(0.5)
    t.mark('kill')
    t.damage('mdn_kid', 1000)
    t.wait(0.5)
    t.expect.fired('mdn_dead', count=1, activator='@monstermaker', after='kill')


@S.test('monstermaker flags 16 Prisoner, 256 Pre-Disaster, 8192 (monster flag 64), 32768 (not in the FGD), 65536, 524288, 1048576 and '
        '"gag": the child has them', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker:16', 'monstermaker:256', 'monstermaker:8192',
                'monstermaker:65536', 'monstermaker:524288', 'monstermaker:1048576', 'monstermaker.gag=1'))
def maker_child_flags(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mfl', flags=4 | 16 | 256 | 8192 | 32768 | 65536 | 524288 | 1048576, gag='1')
    maker(t, 'mno', at=(150, 0, 0))
    t.wait(0.5)
    t.fire('mfl')
    t.fire('mno')
    t.wait(0.3)
    t.snap('s', 'mfl_kid', 'spawnflags')
    t.snap('s', 'mno_kid', 'spawnflags')
    t.expect.value('s', 'mfl_kid', 'spawnflags', 2 | 16 | 64 | 256 | 32768 | 65536 | 524288 | 1048576, op='&')
    t.expect.value('s', 'mno_kid', 'spawnflags', 2 | 16 | 64 | 256 | 32768 | 65536 | 524288 | 1048576, op='!&')


@S.test('monstermaker flag 256 Pre-Disaster seen: its scientist refuses +use (SC_POK); a stock child follows', players=1,
        doc=MAKER, covers=('monstermaker', 'monstermaker:256'))
def maker_predisaster_use(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mpd', at=(-120, 0, 0), flags=4 | 256)
    maker(t, 'mst', at=(120, 0, 0))
    t.wait(0.5)
    t.fire('mpd')
    t.fire('mst')
    t.wait(0.5)
    t.mark('u1')
    t.fire('mpd_kid', activator='player1', caller='player1')
    t.wait(3.0)
    t.fire('mst_kid', activator='player1', caller='player1')
    t.wait(0.5)
    t.snap('s', 'mpd_kid', 'follow')
    t.snap('s', 'mst_kid', 'follow')
    t.expect.sound('SC_POK', ent='mpd_kid', after='u1')
    t.expect.value('s', 'mpd_kid', 'follow', 0)
    t.expect.value('s', 'mst_kid', 'follow', 1)


@S.test('monstermaker flag 65536 Non-solid corpse: its dead child is not solid; a stock child\'s corpse is (makers with '
        'Number of Monsters 1: other makers\' corpses fade out, not solid anyway)', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker:65536'))
def maker_nonsolid_corpse(t):
    t.flags('player1', '+FL_NOTARGET')
    maker(t, 'mns', cls='monster_zombie', at=(-120, 0, 0), flags=4 | 65536, monstercount='1')
    maker(t, 'mso', cls='monster_zombie', at=(120, 0, 0), monstercount='1')
    t.wait(0.5)
    t.fire('mns')
    t.fire('mso')
    t.wait(0.5)
    for n in ('mns_kid', 'mso_kid'):
        t.kv(n, 'health', '10')
        t.damage(n, 15)        # even a random double hit (SBO's "perfect shot") does not gib it
    t.wait(3.5)
    t.snap('s', 'mns_kid', 'solid', 'deadflag')
    t.snap('s', 'mso_kid', 'solid', 'deadflag')
    t.expect.value('s', 'mns_kid', 'solid', 0)
    t.expect.value('s', 'mso_kid', 'solid', 0, op='!=')


@S.test('monstermaker flag 32 Auto size BBox: a scientist standing 40 units off the spot blocks a stock maker (its '
        '68-wide box) but not one with the flag (the child\'s own 32-wide hull)', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker:32'))
def maker_autosize(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('mauto', -120, 4 | 32), ('mstock', 120, 4)):
        t.point('monster_scientist', targetname=n + '_blk', at=(x + 40, 0, 0), spawnflags='16')
        maker(t, n, at=(x, 0, 0), flags=fl, target=n + '_out')
        info(t, n + '_out')
    t.wait(0.5)
    t.fire('mauto')
    t.fire('mstock')
    t.wait(0.5)
    t.expect.fired('mauto_out', count=1)
    t.expect.not_fired('mstock_out')


@S.test('monstermaker flag 64 Cyclic backlog: a trigger while the spot is taken is kept and made once it is free; '
        'without the flag it is lost', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker:4', 'monstermaker:64'))
def maker_backlog(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('mbl', -120, 4 | 64), ('mlost', 120, 4)):
        t.point('monster_scientist', targetname=n + '_blk', at=(x, 0, 0), spawnflags='16')
        maker(t, n, at=(x, 0, 0), flags=fl, target=n + '_out')
        info(t, n + '_out')
    t.wait(0.5)
    t.fire('mbl')
    t.fire('mlost')
    t.wait(0.3)
    t.mark('free')
    t.teleport('mbl_blk', (-120, 200, 0))
    t.teleport('mlost_blk', (120, 200, 0))
    t.wait(1.5)
    t.expect.fired('mbl_out', count=1, after='free')
    t.expect.not_fired('mlost_out')


@S.test('monstermaker "make_blocker_move_away" 2: the scientist standing on the spot is asked to run away; without '
        'the key it stays', players=1, doc=MAKER, covers=('monstermaker', 'monstermaker.make_blocker_move_away=2'))
def maker_blocker_move_away(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, x, kv in (('mmv', -100, {'make_blocker_move_away': '2'}), ('mst', 100, {})):
        t.point('monster_scientist', targetname=n + '_blk', at=(x, -100, 0), spawnflags='16')
        info(t, n + '_spot', at=(x, -100, 0))
        t.point('monstermaker', targetname=n, at=(x, -100, 0), monstertype='monster_scientist', monstercount='1',
                delay='0.5', angles='0 90 0', **kv)
    t.wait(WAKE)
    t.fire('mmv', use='on')
    t.fire('mst', use='on')
    t.wait(4.0)
    t.snap('s', 'mmv_blk', 'origin')
    t.snap('s', 'mst_blk', 'origin')
    t.snap('s', 'mmv_spot', 'origin')
    t.snap('s', 'mst_spot', 'origin')
    t.expect.distance('s', 'mmv_blk', 'mmv_spot', 40, 2000)
    t.expect.distance('s', 'mst_blk', 'mst_spot', 0, 8)


@S.test('monstermaker "make_blocker_move_away" 2 with the station\'s setup: a gagged scientist (not a prisoner, the '
        'player in view) on the spot runs off and the maker makes its zombie; without the key he stays and no zombie '
        'comes', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.make_blocker_move_away=2', 'monstermaker.target'))
def maker_blocker_station(t):
    t.flags('player1', '-FL_NOTARGET')
    node_grid(t)
    t.teleport('player1', (0, 200, 0), yaw=270)
    for n, x, kv in (('mfree', -120, {'make_blocker_move_away': '2'}), ('mstay', 120, {})):
        t.point('monster_scientist', targetname=n + '_blk', at=(x, -100, 8), angles='0 90 0', spawnflags='2')
        info(t, n + '_spot', at=(x, -100, 8))
        info(t, n + '_out')
        t.point('monstermaker', targetname=n, at=(x, -100, 8), monstertype='monster_zombie', monstercount='1',
                delay='2', angles='0 90 0', target=n + '_out', spawnflags='16', gag='1', **kv)
    t.wait(1.0)
    t.mark('go')
    t.fire('mfree', use='on')
    t.fire('mstay', use='on')
    t.wait(8.0)
    t.snap('s', 'mfree_blk', 'origin', 'schedule')
    t.snap('s', 'mfree_spot', 'origin')
    t.snap('s', 'mstay_blk', 'origin')
    t.snap('s', 'mstay_spot', 'origin')
    t.expect.distance('s', 'mfree_blk', 'mfree_spot', 40, 2000)
    t.expect.fired('mfree_out', count=1, after='go')
    t.expect.distance('s', 'mstay_blk', 'mstay_spot', 0, 8)
    t.expect.not_fired('mstay_out')


@S.test('monstermaker "make_blocker_move_away" 2 when the nearby nodes lie past another monster (co_monsterkeys_test): '
        'the blocker does not take a node route it cannot start; it goes straight away into the free space and the '
        'zombie comes', players=1, doc=MAKER, covers=('monstermaker', 'monstermaker.make_blocker_move_away=2'))
def maker_blocker_node_taken(t):
    t.flags('player1', '-FL_NOTARGET')
    t.teleport('player1', (-200, 150, 0), yaw=0)
    # the only nodes: south-west, past the second scientist (a node route is built without checking monsters)
    t.point('info_node', at=(-24, -94, 16))
    t.point('info_node', at=(-152, -94, 16))
    t.point('monster_scientist', targetname='other', at=(0, -100, 8), angles='0 180 0', spawnflags='2')
    t.point('monster_scientist', targetname='blk', at=(0, 0, 8), angles='0 180 0', spawnflags='2')
    t.brush('func_wall', size=(400, 16, 160), at=(0, 74, 0))      # a wall 50 units north of him
    info(t, 'spot', at=(0, 0, 8))
    info(t, 'made')
    t.point('monstermaker', targetname='mk', at=(0, 0, 8), monstertype='monster_zombie', monstercount='1', delay='2',
            angles='0 180 0', make_blocker_move_away='2', target='made', spawnflags='16', gag='1')
    t.wait(1.0)
    t.mark('go')
    t.fire('mk', use='on')
    t.wait(6.0)
    t.snap('s', 'blk', 'origin', 'schedule')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'blk', 'spot', 40, 600)
    t.expect.fired('made', count=1, after='go')


@S.test('monstermaker "body" goes to scientists and zombies, never to the grunt family: body 4 on an MP5 grunt '
        'would draw the shotgun, so the grunt keeps his own body', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.body'))
def maker_body_grunts(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, cls, x in (('mbz', 'monster_zombie', -150), ('mbs', 'monster_scientist', 0), ('mbg', 'monster_human_grunt', 150)):
        maker(t, n, cls=cls, at=(x, 0, 0), body={'monster_zombie': '1', 'monster_scientist': '2'}.get(cls, '4'))
    t.wait(0.5)
    for n in ('mbz', 'mbs', 'mbg'):
        t.fire(n)
    t.wait(0.5)
    for n in ('mbz', 'mbs', 'mbg'):
        t.snap('s', n + '_kid', 'body')
    t.expect.value('s', 'mbz_kid', 'body', 1)
    t.expect.value('s', 'mbs_kid', 'body', 2)
    t.expect.value('s', 'mbg_kid', 'body', 0)     # MP5 grunt: head 0, gun 0 (body 4 = the shotgun)


@S.test('monstermaker "body" 1 on zombies: every child is the black zombie, whatever the zombie\'s own random pick '
        '(eight children, eight seeds); a maker without body makes both kinds', players=1, doc=MAKER,
        covers=('monstermaker', 'monstermaker.body'))
def maker_body_zombies(t):
    t.flags('player1', '+FL_NOTARGET')
    for k in range(8):
        # 120 / 100 apart: a stock maker's free-spot box is 68 wide
        maker(t, 'mzb%d' % k, cls='monster_zombie', at=(-180 + 120 * (k % 4), 200 - 100 * (k // 4), 0), body='1',
              flags=4 | 16)
        maker(t, 'mzr%d' % k, cls='monster_zombie', at=(-180 + 120 * (k % 4), -100 - 100 * (k // 4), 0), flags=4 | 16)
    t.wait(0.5)
    for k in range(8):
        t.console('test_seed %d' % (11 + k))
        t.fire('mzb%d' % k)
        t.fire('mzr%d' % k)
        t.wait(0.1)
    t.wait(0.3)
    for k in range(8):
        t.snap('s', 'mzb%d_kid' % k, 'body')
        t.expect.value('s', 'mzb%d_kid' % k, 'body', 1)
        t.snap('s', 'mzr%d_kid' % k, 'body')
    # without the key the zombie picks its own body (Spawn: black about half the time, else white)


@S.test('monstermaker "change_rendermode" 1: its child gets the maker\'s Render Mode, FX, Amount and Color; a maker '
        'with the same render keys and no "change_rendermode" leaves its child at its own (normal)', players=1,
        doc=MAKER,
        covers=('monstermaker', 'monstermaker.change_rendermode=1', 'monstermaker.rendermode', 'monstermaker.renderamt',
                'monstermaker.rendercolor', 'monstermaker.renderfx'))
def maker_change_rendermode(t):
    t.flags('player1', '+FL_NOTARGET')
    render = dict(rendermode='1', renderamt='120', rendercolor='255 40 10', renderfx='19')
    maker(t, 'mrm', at=(-120, 0, 0), change_rendermode='1', **render)
    maker(t, 'mrn', at=(120, 0, 0), **render)
    t.wait(0.5)
    t.fire('mrm')
    t.fire('mrn')
    t.wait(0.5)
    for n in ('mrm_kid', 'mrn_kid'):
        t.snap('s', n, 'rendermode', 'renderamt', 'rendercolor', 'renderfx')
    t.expect.value('s', 'mrm_kid', 'rendermode', 1)
    t.expect.value('s', 'mrm_kid', 'renderamt', 120)
    t.expect.value('s', 'mrm_kid', 'rendercolor', [255, 40, 10])
    t.expect.value('s', 'mrm_kid', 'renderfx', 19)
    t.expect.value('s', 'mrn_kid', 'rendermode', 0)
    t.expect.value('s', 'mrn_kid', 'renderamt', 0)
    t.expect.value('s', 'mrn_kid', 'rendercolor', [0, 0, 0])
    t.expect.value('s', 'mrn_kid', 'renderfx', 0)


@S.test('monstermaker flag 65536 Non-solid corpse seen: a door coming down closes over its corpse; the stock corpse '
        'blocks its door (it goes back up)', players=1, doc=MAKER, covers=('monstermaker', 'monstermaker:65536'))
def maker_nonsolid_door(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('mdns', -120, 4 | 65536), ('mdst', 120, 4)):
        maker(t, n, cls='monster_zombie', at=(x, 0, 0), flags=fl, monstercount='1')
        t.brush('func_door', size=(80, 80, 160), at=(x, 0, 120), targetname=n + '_door', angles='0 -2 0',
                speed='60', lip='40', dmg='0', wait='3')
    t.wait(0.5)
    for n in ('mdns', 'mdst'):
        t.fire(n)
    t.wait(0.5)
    for n in ('mdns_kid', 'mdst_kid'):
        t.kv(n, 'health', '10')
        t.damage(n, 15)
    t.wait(3.5)
    t.fire('mdns_door')
    t.fire('mdst_door')
    t.wait(2.5)
    t.snap('s', 'mdns_door', 'origin')
    t.snap('s', 'mdst_door', 'origin')
    t.expect.value('s', 'mdns_door', 'origin.z', -100, tol=30)
    t.expect.value('s', 'mdst_door', 'origin.z', -100, op='>')


@S.test('monster flag 65536 Non-solid corpse on a placed monster: a dead zombie with the flag is not solid; the same '
        'zombie without it leaves a solid corpse', players=1, doc=MAKER, covers=('monster_zombie', 'monster_zombie:65536'))
def monster_nonsolid_corpse(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('zns', -120, 65536), ('zso', 120, 0)):
        t.point('monster_zombie', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags=str(16 | fl))
    t.wait(1.0)
    for n in ('zns', 'zso'):
        t.kv(n, 'health', '10')
        t.damage(n, 15)        # even a random double hit does not gib it
    t.wait(4.0)
    for n in ('zns', 'zso'):
        t.snap('s', n, 'solid', 'deadflag')
    t.expect.value('s', 'zns', 'solid', 0)
    t.expect.value('s', 'zso', 'solid', 0, op='!=')



# ---------------------------------------------------------------------------------------------- scripts (batch 4)
SCRIPT = 'FEATUREFUL_OPTIONS.md#scripted-scenes-batch-4'


def held(t, name, at, script_kv=None, monster_kv=None, idle='idle1', play='barn_wave', script_at=None):
    """A prisoner barney `name` (no fights) and a named script 'name_ss' that takes it at once (idle animation: it
    waits for the trigger in its idle)."""
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16', names=('TriggerTarget',),
            **(monster_kv or {}))
    kv = dict(m_iszEntity=name, m_iszPlay=play, m_fMoveTo='0', m_flRadius='512')
    if idle:
        kv['m_iszIdle'] = idle
    kv.update(script_kv or {})
    t.point('scripted_sequence', targetname=name + '_ss', at=script_at or at, angles='0 270 0', names=('m_iszEntity',),
            **kv)


@S.test('scripted_sequence "m_takeDamagePolicy" Invulnerable: the barney it holds survives 1000 damage; without it '
        'he dies', players=1, doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence.m_takeDamagePolicy=1'))
def script_invulnerable(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'binv', (-100, 0, 0), {'m_takeDamagePolicy': '1'})
    held(t, 'bstk', (100, 0, 0))
    t.wait(2.0)
    t.damage('binv', 110)
    t.damage('bstk', 110)      # not enough to gib (a gibbed one is removed)
    t.wait(0.1)
    t.snap('s', 'binv', 'deadflag', 'health')
    t.snap('s', 'bstk', 'deadflag')
    t.expect.value('s', 'binv', 'deadflag', 0)
    t.expect.value('s', 'bstk', 'deadflag', 0, op='>')


@S.test('scripted_sequence "m_takeDamagePolicy" Non-lethal: the barney it holds keeps 1 health after 1000 damage',
        players=1, doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence.m_takeDamagePolicy=2'))
def script_nonlethal(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bnl', (0, 0, 0), {'m_takeDamagePolicy': '2', 'm_interruptionPolicy': '1'})
    t.wait(2.0)
    t.damage('bnl', 1000)
    t.wait(0.5)
    t.snap('s', 'bnl', 'deadflag', 'health')
    t.expect.value('s', 'bnl', 'deadflag', 0)
    t.expect.value('s', 'bnl', 'health', 1, tol=0.01)


@S.test('scripted_sequence "m_interruptionPolicy" By Death Only and No Interruptions: a hit leaves the barney in its '
        'script; Default: the hit breaks him out', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.m_interruptionPolicy=1',
                'scripted_sequence.m_interruptionPolicy=2'))
def script_interruption(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bdo', (-150, 0, 0), {'m_interruptionPolicy': '2'})
    held(t, 'bni', (0, 0, 0), {'m_interruptionPolicy': '1'})
    held(t, 'bdf', (150, 0, 0))
    t.wait(2.0)
    for n in ('bdo', 'bni', 'bdf'):
        t.damage(n, 10, attacker='player1')     # only a monster's or a player's hit sets the damage conditions
    t.wait(1.0)
    for n in ('bdo', 'bni', 'bdf'):
        t.snap('s', n, 'cine')
    t.expect.value('s', 'bdo', 'cine', 1)
    t.expect.value('s', 'bni', 'cine', 1)
    t.expect.value('s', 'bdf', 'cine', 0)


@S.test('scripted_sequence "m_iRepeats" 2: the 2.55 s wave plays three times (still in the script at 4 s, out by 9 s); '
        'without it out at 4 s', players=1, doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence.m_iRepeats'))
def script_repeats(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'brp', (-100, 0, 0), {'m_iRepeats': '2'})
    held(t, 'bon', (100, 0, 0))
    t.wait(2.0)
    t.fire('brp_ss')
    t.fire('bon_ss')
    t.wait(4.0)
    t.snap('a', 'brp', 'cine')
    t.snap('a', 'bon', 'cine')
    t.wait(5.0)
    t.snap('b', 'brp', 'cine')
    t.expect.value('a', 'brp', 'cine', 1)
    t.expect.value('a', 'bon', 'cine', 0)
    t.expect.value('b', 'brp', 'cine', 0)


@S.test('scripted_sequence "required_state" Combat: it does not take an idle barney; Idle: it does', players=1,
        doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.required_state=1', 'scripted_sequence.required_state=2'))
def script_required_state(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'brc', (-100, 0, 0), {'required_state': '2'})
    held(t, 'bri', (100, 0, 0), {'required_state': '1'})
    t.wait(2.5)
    t.snap('s', 'brc', 'cine')
    t.snap('s', 'bri', 'cine')
    t.expect.value('s', 'brc', 'cine', 0)
    t.expect.value('s', 'bri', 'cine', 1)


@S.test('scripted_sequence "m_searchPolicy" Targetname only: Target Monster "monster_barney" (a class name) takes '
        'no barney; Default: the barney near it (by class)', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.m_searchPolicy=0', 'scripted_sequence.m_searchPolicy=1'))
def script_search_policy(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, pol in (('bsn', -150, '1'), ('bsd', 150, '0')):
        t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 40, 0), m_iszEntity='monster_barney',
                m_iszIdle='idle1', m_iszPlay='barn_wave', m_flRadius='80', m_fMoveTo='0', m_searchPolicy=pol)
    t.wait(2.5)
    t.snap('s', 'bsn', 'cine')
    t.snap('s', 'bsd', 'cine')
    t.expect.value('s', 'bsn', 'cine', 0)
    t.expect.value('s', 'bsd', 'cine', 1)


@S.test('scripted_sequence "m_applySearchRadius" Always: a barney named in Target Monster but 300 away (radius 100) '
        'is not taken; Default: he is', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.m_applySearchRadius=1', 'scripted_sequence.m_flRadius'))
def script_apply_radius(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bar', (-150, -150, 0), {'m_applySearchRadius': '1', 'm_flRadius': '100'}, script_at=(-150, 150, 0))
    held(t, 'bad', (150, -150, 0), {'m_flRadius': '100'}, script_at=(150, 150, 0))
    t.wait(2.5)
    t.snap('s', 'bar', 'cine')
    t.snap('s', 'bad', 'cine')
    t.expect.value('s', 'bar', 'cine', 0)
    t.expect.value('s', 'bad', 'cine', 1)


@S.test('scripted_sequence "m_iPriority" Override: it takes a barney another script holds (its Fire on Possession '
        'fires); a Normal one does not', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.m_iPriority=4', 'scripted_sequence.m_iszFireOnPossessed'))
def script_priority(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bpo', (-100, 0, 0))
    held(t, 'bpn', (100, 0, 0))
    t.point('scripted_sequence', targetname='ov_ss', at=(-100, 60, 0), m_iszEntity='bpo', m_iszPlay='barn_wave',
            m_fMoveTo='0', m_iPriority='4', m_iszFireOnPossessed='ov_took', names=('m_iszEntity',))
    t.point('scripted_sequence', targetname='nm_ss', at=(100, 60, 0), m_iszEntity='bpn', m_iszPlay='barn_wave',
            m_fMoveTo='0', m_iszFireOnPossessed='nm_took', names=('m_iszEntity',))
    info(t, 'ov_took')
    info(t, 'nm_took')
    t.wait(2.0)
    t.mark('go')
    t.fire('ov_ss')
    t.fire('nm_ss')
    t.wait(2.5)
    t.expect.fired('ov_took', count=1, after='go')
    t.expect.not_fired('nm_took', after='go')


@S.test('scripted_sequence "m_fTurnType" with Move to "No - Turn to Face": 1 the barney turns toward the script, 2 '
        'he does not turn, 0 he takes its angle (90)', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.m_fMoveTo=5', 'scripted_sequence.m_fTurnType=0',
                'scripted_sequence.m_fTurnType=1', 'scripted_sequence.m_fTurnType=2'))
def script_turn_type(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, tt in (('btf', -150, '1'), ('btn', 0, '2'), ('btm', 150, '0')):
        t.point('monster_barney', targetname=n, at=(x, -100, 0), angles='0 0 0', spawnflags='16')
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 100, 0), angles='0 90 0', m_iszEntity=n,
                m_iszPlay='idle1', m_fMoveTo='5', m_fTurnType=tt, names=('m_iszEntity',))
    t.wait(1.5)
    for n in ('btf', 'btn', 'btm'):
        t.fire(n + '_ss')
    t.wait(2.0)
    for n in ('btf', 'btn', 'btm'):
        t.snap('s', n, 'angles')
    t.expect.value('s', 'btf', 'angles.y', 90, tol=12)
    t.expect.value('s', 'btn', 'angles.y', 0, tol=12)
    t.expect.value('s', 'btm', 'angles.y', 90, tol=12)


@S.test('scripted_sequence "moveto_radius" 150 with Walk: the barney stops about 150 from the mark; without it he '
        'walks onto it', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence.moveto_radius', 'scripted_sequence.m_fMoveTo=1'))
def script_moveto_radius(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, x, kv in (('bmr', -120, {'moveto_radius': '150'}), ('bmo', 120, {})):
        t.point('monster_barney', targetname=n, at=(x, -200, 0), angles='0 90 0', spawnflags='16')
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 200, 0), angles='0 90 0', m_iszEntity=n,
                m_iszPlay='idle1', m_fMoveTo='1', names=('m_iszEntity',), **kv)
        info(t, n + '_mark', at=(x, 200, 0))     # the script is removed once it is done
    t.wait(1.5)
    t.fire('bmr_ss')
    t.fire('bmo_ss')
    t.wait(9.0)
    for n in ('bmr', 'bmo', 'bmr_mark', 'bmo_mark'):
        t.snap('s', n, 'origin')
    t.expect.distance('s', 'bmr', 'bmr_mark', 110, 165)
    t.expect.distance('s', 'bmo', 'bmo_mark', 0, 20)


@S.test('scripted_sequence flag 32768 "Remove on interruption": a hit breaks the barney out and the script is removed',
        players=1, doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence:32768'))
def script_remove_on_interruption(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bri2', (0, 0, 0), {'spawnflags': '32768'})
    t.wait(2.0)
    t.damage('bri2', 10, attacker='player1')
    t.wait(1.0)
    t.expect.removed('bri2_ss')


@S.test('scripted_sequence flag 1024 "Auto-search": a named script keeps looking in its radius from the map start and '
        'takes the barney a maker makes later, without a trigger; without the flag it waits for its trigger',
        players=1, doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence:1024'))
def script_autosearch(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('bau', -100, '1024'), ('bnt', 100, '0')):
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n + '_kid', m_iszPlay='barn_wave',
                m_fMoveTo='0', spawnflags=fl, names=('m_iszEntity',))
        t.point('monstermaker', targetname=n + '_mk', at=(x, 0, 0), monstertype='monster_barney', monstercount='1',
                netname=n + '_kid', spawnflags='4', angles='0 270 0', names=('netname',))
    t.wait(0.5)
    t.fire('bau_mk')
    t.fire('bnt_mk')
    t.wait(1.6)
    t.snap('s', 'bau_kid', 'cine')
    t.snap('s', 'bnt_kid', 'cine')
    t.expect.value('s', 'bau_kid', 'cine', 1)
    t.expect.value('s', 'bnt_kid', 'cine', 0)


@S.test('scripted_sequence flag 4096 "Try search once": fired before its barney exists it does not keep looking; '
        'without it it takes the barney once he appears', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence:4096'))
def script_try_once(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('bto', -100, '4096'), ('btl', 100, '0')):
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n + '_kid', m_iszPlay='barn_wave',
                m_fMoveTo='0', spawnflags=fl, names=('m_iszEntity',))
        t.point('monstermaker', targetname=n + '_mk', at=(x, 0, 0), monstertype='monster_barney', monstercount='1',
                netname=n + '_kid', spawnflags='4', angles='0 270 0', names=('netname',))
    t.wait(0.5)
    t.fire('bto_ss')
    t.fire('btl_ss')
    t.wait(1.5)
    t.fire('bto_mk')
    t.fire('btl_mk')
    t.wait(2.5)
    t.snap('s', 'bto_kid', 'cine')
    t.snap('s', 'btl_kid', 'cine')
    t.expect.value('s', 'bto_kid', 'cine', 0)
    t.expect.value('s', 'btl_kid', 'cine', 1)


@S.test('scripted_sequence flag 262144 "Allow unnamed idle": an auto-searching script with an idle animation waits '
        '(the barney stays in it, playing the idle); without the flag it plays its action and lets go', players=1,
        doc=SCRIPT, covers=('scripted_sequence', 'scripted_sequence:262144', 'scripted_sequence.m_iszIdle'))
def script_unnamed_idle(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('bui', -100, 1024 | 262144), ('bun', 100, 1024)):
        t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_sequence', targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n, m_iszIdle='idle1',
                m_iszPlay='draw', m_fMoveTo='0', spawnflags=str(fl), names=('m_iszEntity',))
    t.wait(5.0)
    t.snap('s', 'bui', 'cine')
    t.snap('s', 'bun', 'cine')
    t.expect.value('s', 'bui', 'cine', 1)
    t.expect.value('s', 'bun', 'cine', 0)


@S.test('scripted_sequence flag 524288 "Allow Take Damage TriggerCondition": the barney held by a no-interrupt script '
        'fires his Take Damage TriggerTarget when hit; without the flag he does not', players=1, doc=SCRIPT,
        covers=('scripted_sequence', 'scripted_sequence:524288', 'scripted_sequence:32'))
def script_takedamage_trigger(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'btd', (-100, 0, 0), {'spawnflags': str(32 | 524288)},
         {'TriggerCondition': '2', 'TriggerTarget': 'btd_hit'})
    held(t, 'btx', (100, 0, 0), {'spawnflags': '32'}, {'TriggerCondition': '2', 'TriggerTarget': 'btx_hit'})
    info(t, 'btd_hit')
    info(t, 'btx_hit')
    t.wait(2.0)
    t.mark('hit')
    t.damage('btd', 5)
    t.damage('btx', 5)
    t.wait(1.0)
    t.expect.fired('btd_hit', after='hit')
    t.expect.not_fired('btx_hit', after='hit')


def sentence(t, name, at, speaker, duration='1.5', **kv):
    t.point('scripted_sentence', targetname=name, at=at, sentence='!SC_OK0', entity=speaker, duration=duration,
            radius='256', refire='1', spawnflags='4', names=('entity',), **kv)


@S.test('scripted_sentence "follow_action" Start following: the scientist starts following the nearest player; a '
        'barney that is no player ally does not (SBO rule)', players=1, doc=SCRIPT,
        covers=('scripted_sentence', 'scripted_sentence.follow_action=1'))
def sentence_follow_action(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sfa', at=(-100, 100, 0), angles='0 270 0')
    t.point('monster_barney', targetname='bfa', at=(100, 100, 0), angles='0 270 0', spawnflags='16')
    sentence(t, 'ssfa', (-100, 100, 0), 'sfa', follow_action='1')
    sentence(t, 'sbfa', (100, 100, 0), 'bfa', follow_action='1')
    t.teleport('player1', (0, -150, 0))
    t.wait(1.0)
    t.fire('ssfa')
    t.fire('sbfa')
    t.wait(1.0)
    t.snap('s', 'sfa', 'follow')
    t.snap('s', 'bfa', 'follow')
    t.expect.value('s', 'sfa', 'follow', 1)
    t.expect.value('s', 'bfa', 'follow', 0)


@S.test('scripted_sentence "fire_after" with "fire_after_delay" 0.5: fired 0.5 s after the 1.5 s sentence', players=1,
        doc=SCRIPT,
        covers=('scripted_sentence', 'scripted_sentence.fire_after', 'scripted_sentence.fire_after_delay',
                'scripted_sentence.duration'))
def sentence_fire_after(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sfi', at=(0, 100, 0), angles='0 270 0', spawnflags='2')
    sentence(t, 'ssfi', (0, 100, 0), 'sfi', fire_after='sfi_done', fire_after_delay='0.5')
    info(t, 'sfi_done')
    t.wait(1.0)
    t.mark('go')
    t.fire('ssfi')
    t.wait(3.0)
    t.expect.fired('sfi_done', count=1, after='go', within=(1.9, 2.3))


@S.test('scripted_sentence "allow_use": +use during its sentence makes the scientist follow; without it +use is '
        'ignored while he speaks (HL)', players=1, doc=SCRIPT,
        covers=('scripted_sentence', 'scripted_sentence.allow_use=1'))
def sentence_allow_use(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sau', at=(-100, 100, 0), angles='0 270 0', spawnflags='2')
    t.point('monster_scientist', targetname='snu', at=(100, 100, 0), angles='0 270 0', spawnflags='2')
    sentence(t, 'ssau', (-100, 100, 0), 'sau', allow_use='1', duration='4')
    sentence(t, 'ssnu', (100, 100, 0), 'snu', duration='4')
    t.wait(1.0)
    t.fire('ssau')
    t.fire('ssnu')
    t.wait(0.5)
    t.fire('sau', activator='player1', caller='player1')
    t.fire('snu', activator='player1', caller='player1')
    t.wait(0.5)
    t.snap('s', 'sau', 'follow')
    t.snap('s', 'snu', 'follow')
    t.expect.value('s', 'sau', 'follow', 1)
    t.expect.value('s', 'snu', 'follow', 0)


@S.test('scripted_sentence "search_origin": the speaker class is looked for around that point (radius 64), not '
        'around the entity 300 away; attenuation 4 plays at 0.5', players=1, doc=SCRIPT,
        covers=('scripted_sentence', 'scripted_sentence.search_origin', 'scripted_sentence.entity',
                'scripted_sentence.attenuation=4'))
def sentence_search_origin(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_scientist', targetname='sso', at=(150, 150, 0), angles='0 270 0', spawnflags='2')
    info(t, 'sso_spot', at=(150, 150, 0))
    t.point('scripted_sentence', targetname='ssso', at=(-150, -150, 0), sentence='!SC_OK0',
            entity='monster_scientist', duration='1.5', radius='64', refire='1', spawnflags='4',
            search_origin='sso_spot', attenuation='4', names=('search_origin',))
    t.wait(1.0)
    t.mark('go')
    t.fire('ssso')
    t.wait(1.0)
    t.expect.sound('!', ent='sso', after='go', attn=0.5)


# ---------------------------------------------------------------------------------------------- leftovers (batch 5)
LEFT = 'FEATUREFUL_OPTIONS.md#small-leftovers-and-shooter-physics-batch-5'


@S.test('func_pendulum flag 256 "Swing both ways": it swings Distance (60) to both sides of where it starts (beyond +20 '
        'at 1 s, beyond -20 at 3 s); a stock pendulum stays between 0 and 60', players=1, doc=LEFT,
        covers=('func_pendulum', 'func_pendulum:256', 'func_pendulum.distance'))
def pendulum_bothways(t):
    for n, x, fl in (('pb', -120, 256), ('ps', 120, 0)):
        t.brush('func_pendulum', size=(16, 16, 64), at=(x, 0, 32), targetname=n, speed='120', distance='60',
                spawnflags=str(fl), origin=True)
    t.wait(0.3)
    t.fire('pb')
    t.fire('ps')
    t.wait(1.0)
    t.snap('a', 'pb', 'angles')
    t.snap('a', 'ps', 'angles')
    t.wait(2.0)
    t.snap('b', 'pb', 'angles')
    t.snap('b', 'ps', 'angles')
    t.expect.value('a', 'pb', 'angles.y', 20, op='>=')     # out to one side ...
    t.expect.value('b', 'pb', 'angles.y', -20, op='<=')    # ... and to the other (a stock pendulum never goes below 0)
    t.expect.value('b', 'ps', 'angles.y', -15, op='>=')    # (it overshoots 0 a little: 0.1 s steps)


@S.test('func_pushable "toggleable_push": fired, it turns solid (SOLID_BSP) and a player walking into it does not '
        'move it; fired again it is pushable (SOLID_BBOX)', players=1, doc=LEFT,
        covers=('func_pushable', 'func_pushable.toggleable_push=1'))
def pushable_toggle(t):
    t.brush('func_pushable', size=(48, 48, 48), at=(0, 0, 0), targetname='pt', toggleable_push='1')
    info(t, 'pt0', at=(0, 0, 24))
    t.wait(0.3)
    t.fire('pt')
    t.wait(0.2)
    t.snap('off', 'pt', 'solid')
    t.teleport('player1', (0, -100, 0), yaw=90)
    t.walk('player1', 250, 0, 1.0)
    t.snap('pushed', 'pt', 'origin')
    t.fire('pt')
    t.wait(0.2)
    t.snap('on', 'pt', 'solid')
    t.expect.value('off', 'pt', 'solid', 4)
    t.expect.value('on', 'pt', 'solid', 2)
    t.expect.value('pushed', 'pt', 'origin.y', 0, tol=2)


@S.test('func_breakable flag 512 "Explosives only": bullets do nothing, blast damage breaks it; a stock one breaks '
        'from bullets', players=1, doc=LEFT, covers=('func_breakable', 'func_breakable:512'))
def breakable_explosives_only(t):
    t.brush('func_breakable', size=(48, 48, 48), at=(-100, 0, 0), targetname='bx', health='20', spawnflags='512')
    t.brush('func_breakable', size=(48, 48, 48), at=(100, 0, 0), targetname='bs', health='20')
    t.wait(0.3)
    t.damage('bx', 50, dmgtype=2)
    t.damage('bs', 50, dmgtype=2)
    t.wait(0.3)
    t.snap('s', 'bx', 'health')
    t.mark('blast')
    t.damage('bx', 50, dmgtype=64)
    t.wait(0.3)
    t.expect.value('s', 'bx', 'health', 20)
    t.expect.removed('bs')
    t.expect.removed('bx')


@S.test('func_tank flag 64 "Laser Spot": an active tank shows a laser dot (laser_spot); off, the dot is hidden',
        players=1, doc=LEFT, covers=('func_tank', 'func_tank:64'))
def tank_laser_spot(t):
    t.flags('player1', '+FL_NOTARGET')
    t.brush('func_tank', size=(32, 32, 32), at=(0, 0, 64), targetname='tls', spawnflags='64', yawrate='30',
            yawrange='180', pitchrate='30', pitchrange='30', barrel='16', firerate='1', bullet='1', origin=True)
    t.wait(0.3)
    t.fire('tls', use='on')
    t.wait(0.5)
    t.mark('off')
    t.fire('tls', use='off')
    t.wait(0.3)
    t.expect.spawned('@laser_spot', count=1)


@S.test('env_shooter "m_iPhysics" 5 (arc shot) with "m_iszSpawnTarget" and "m_iszTouch": each shot is made as a '
        '"shot" (the spawn target fires with it as activator) and fires its touch target when it lands', players=1,
        doc=LEFT,
        covers=('env_shooter', 'env_shooter.m_iPhysics=5', 'env_shooter.m_iszSpawnTarget', 'env_shooter.m_iszTouch',
                'env_shooter.m_iGibs'))
def shooter_shots(t):
    t.point('env_shooter', targetname='sh', at=(0, 0, 120), angles='-60 0 0', shootmodel='sprites/glow01.spr',
            m_iGibs='2', delay='0.2', m_flVelocity='100', m_flGibLife='3', m_iPhysics='5', m_vecSize='8 8 8',
            m_iszSpawnTarget='sh_made', m_iszTouch='sh_hit', names=('m_iszTouch',))
    info(t, 'sh_made')
    info(t, 'sh_hit')
    t.wait(0.3)
    t.mark('go')
    t.fire('sh')
    t.wait(2.5)
    t.expect.fired('sh_made', count=2, activator='@shot', after='go')
    t.expect.fired('sh_hit', min=1, after='go')


@S.test('env_shooter default (m_iPhysics 0) stays a bouncing gib; "m_iszSpawnTarget" fires with each gib', players=1,
        doc=LEFT, covers=('env_shooter', 'env_shooter.m_iszSpawnTarget', 'env_shooter.m_iGibs'))
def shooter_gibs(t):
    t.point('env_shooter', targetname='shg', at=(0, 0, 120), angles='-60 0 0', shootmodel='models/woodgibs.mdl',
            m_iGibs='2', delay='0.2', m_flVelocity='100', m_flGibLife='3', m_iszSpawnTarget='shg_made')
    info(t, 'shg_made')
    t.wait(0.3)
    t.mark('go')
    t.fire('shg')
    t.wait(1.0)
    t.expect.fired('shg_made', count=2, activator='@gib', after='go')


# ---------------------------------------------------------------------------------------------- fights (last)
# Every test's monsters exist from the map start and Half-Life's barneys are provoked by the combat they hear (also
# through walls): the tests with gunfire come last.
@S.test('monster_human_grunt "standgroundrange": when the player he fights hides behind a wall, a grunt with the key '
        'keeps his spot; without it he moves to get a line of fire', players=1, doc=DOC,
        covers=('monster_human_grunt', 'monster_human_grunt.standgroundrange'))
def grunt_standground(t):
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    node_grid(t, step=96, lo=-220, hi=220)
    t.brush('func_wall', size=(240, 16, 160), at=(0, -170, 0))     # the player's cover
    t.point('monster_human_grunt', targetname='gstand', at=(-150, 120, 0), angles='0 270 0', standgroundrange='64',
            netname='sq_a', spawnflags='2')
    t.point('monster_human_grunt', targetname='gfree', at=(150, 120, 0), angles='0 270 0', netname='sq_b',
            spawnflags='2')
    info(t, 'gstand0', at=(-150, 120, 0))
    info(t, 'gfree0', at=(150, 120, 0))
    t.teleport('player1', (0, -60, 0), yaw=90)
    t.wait(0.3)
    for n in ('gstand', 'gfree'):
        t.kv(n, 'health', '100000')
    t.wait(WAKE + 1.0)
    t.snap('seen', 'gstand', 'enemy')
    t.snap('seen', 'gfree', 'enemy')
    t.teleport('player1', (0, -225, 0), yaw=90)     # behind the wall
    for k in range(4):
        t.wait(2.0)
        t.snap('m%d' % k, 'gfree', 'schedule', 'origin')
        t.snap('m%d' % k, 'gstand', 'schedule', 'origin')
    for n in ('gstand', 'gfree', 'gstand0', 'gfree0'):
        t.snap('e', n, 'origin')
    for n in ('gstand', 'gfree'):
        t.damage(n, 200000)     # their gunfire would provoke the barneys of the next tests
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('seen', 'gstand', 'enemy', 'player')
    t.expect.value('seen', 'gfree', 'enemy', 'player')
    t.expect.distance('e', 'gstand', 'gstand0', 0, 16)
    t.expect.distance('e', 'gfree', 'gfree0', 48, 2000)


@S.test('talk monster "suspicious" seen: a player-ally barney with the key takes the player as his enemy and shoots',
        players=1, doc=DOC, covers=('monster_barney', 'monster_barney.suspicious=1', 'monster_barney.is_player_ally=1'))
def talk_suspicious_attacks(t):
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.point('monster_barney', targetname='bsus', at=(0, 150, 0), angles='0 270 0', is_player_ally='1',
            suspicious='1', spawnflags='2')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.wait(WAKE + 1.5)
    t.snap('s', 'bsus', 'enemy')
    t.damage('bsus', 1000)      # its gunfire would provoke the barneys of the next tests (HL: they hear combat)
    t.flags('player1', '-FL_GODMODE')
    t.expect.value('s', 'bsus', 'enemy', 'player')
    t.expect.sound('ba_attack', ent='bsus')
