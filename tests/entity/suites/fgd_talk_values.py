"""Tier 5, final pass: TalkMonster choices no class tested (talkmonster.cpp, Featureful's keys): Pre-provoked No,
every Tolerance level a hit can show (ReactToPlayerHit: direct hit = the player looking at it), Alert friends
Default / Always / If not in combat (ShouldAlertFriendsOnDeath), Followage policy "Scripted only, decline use", and
the Use / Un-Use sentences. On a player-ally Cop (barney) or a scientist, as suite monsterkeys does."""

from etest import Suite

SUITE = Suite('fgd_talk_values', 'Tier 5: talk monster choices', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#monster-keys-batch-2'
R_AL, R_HT = -2, 2
B = 'monster_barney'


def barney(t, name, x, **kv):
    # a player ally, Gag (2): no chatter
    t.point(B, targetname=name, at=(x, 150, 0), angles='0 270 0', is_player_ally='1', spawnflags='2', **kv)


def hit(t, name, x, facing, label=None):
    """player1 150 south of it, looking at it (facing) or away; one hit; snapshot `label`"""
    t.teleport('player1', (x, 0, 0), yaw=90 if facing else 270)
    t.look('player1', 0, 90 if facing else 270)
    t.wait(0.1)
    t.damage(name, 5, attacker='player1')
    t.wait(1.0)
    if label:
        t.snap(label, name, 'rel')


@S.test('%s "suspicious" No (0): a player-ally Cop is his ally' % B, players=1, doc=DOC,
        covers=(B + '.suspicious=0',))
def suspicious_no(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point(B, targetname='b', at=(0, 150, 0), angles='0 270 0', is_player_ally='1', suspicious='0', spawnflags='16')
    t.wait(0.5)
    t.snap('s', 'b', 'rel')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('s', 'b', 'rel', R_AL)


@S.test('%s "tolerance" Default (0) set: one hit from a player looking at it provokes it (Half-Life)' % B, players=1,
        doc=DOC, covers=(B + '.tolerance=0',))
def tolerance_default(t):
    t.flags('player1', '+FL_NOTARGET')
    barney(t, 'b', 0, tolerance='0')
    t.wait(0.5)
    hit(t, 'b', 0, True, 'a')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('a', 'b', 'rel', R_HT)


@S.test('%s "tolerance" Zero (1): a hit from a player looking away provokes it' % B, players=1, doc=DOC,
        covers=(B + '.tolerance=1',))
def tolerance_zero(t):
    t.flags('player1', '+FL_NOTARGET')
    barney(t, 'b', 0, tolerance='1')
    t.wait(0.5)
    hit(t, 'b', 0, False, 'a')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('a', 'b', 'rel', R_HT)


@S.test('%s "tolerance" Low (2): a hit from a player looking away does not provoke it, the second hit does' % B,
        players=1, doc=DOC, covers=(B + '.tolerance=2',))
def tolerance_low(t):
    t.flags('player1', '+FL_NOTARGET')
    barney(t, 'b', 0, tolerance='2')
    t.wait(0.5)
    hit(t, 'b', 0, False, 'a1')
    hit(t, 'b', 0, False, 'a2')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('a1', 'b', 'rel', R_AL)
    t.expect.value('a2', 'b', 'rel', R_HT)


@S.test('%s "tolerance" High (4): three direct hits within 4 s do not provoke it, a fourth does (the hit is counted '
        'after the check, as in Featureful)' % B, players=1, doc=DOC, covers=(B + '.tolerance=4',))
def tolerance_high(t):
    t.flags('player1', '+FL_NOTARGET')
    barney(t, 'b', 0, tolerance='4')
    t.wait(0.5)
    hit(t, 'b', 0, True)
    hit(t, 'b', 0, True)
    hit(t, 'b', 0, True, 'a3')
    hit(t, 'b', 0, True, 'a4')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('a3', 'b', 'rel', R_AL)
    t.expect.value('a4', 'b', 'rel', R_HT)


def friends_case(v, name):
    @S.test('%s "alert_friends" %s (%d): a player killing it (it is in no fight) provokes the Cop next to it' % (
                B, name, v), players=1, doc=DOC, covers=(B + '.alert_friends=%d' % v,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        barney(t, 'victim', -60, alert_friends=str(v))
        barney(t, 'watcher', 60)
        t.wait(0.5)
        t.damage('victim', 1000, attacker='player1')
        t.wait(0.5)
        t.snap('s', 'watcher', 'rel')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.value('s', 'watcher', 'rel', R_HT)
    case.__name__ = 'alert_friends_%d' % v
    return case


for _v, _n in ((0, 'Default'), (1, 'Always'), (2, 'If not in combat')):
    friends_case(_v, _n)


def sci(t, **kv):
    t.point('monster_scientist', targetname='sci', at=(0, 60, 0), angles='0 270 0', **kv)
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -20, 0), yaw=90)
    t.wait(0.5)


@S.test('monster_scientist "followage_policy" Scripted only, decline use (2): +use does not make him follow, he '
        'says no (a sentence)', players=1, doc=DOC, covers=('monster_scientist.followage_policy=2',))
def followage_decline(t):
    sci(t, followage_policy='2')
    t.mark('use')
    t.fire('sci', activator='player1', caller='player1')
    t.wait(1.0)
    t.snap('u', 'sci', 'follow')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('u', 'sci', 'follow', 0)
    t.expect.sound('!', ent='sci', after='use', min=1)


@S.test('monster_scientist "UseSentence" / "UnUseSentence": +use makes him follow saying the BA_OK group; the next '
        '+use stops him saying BA_WAIT', players=1, doc=DOC,
        covers=('monster_scientist.usesentence', 'monster_scientist.unusesentence'))
def use_sentences(t):
    sci(t, UseSentence='BA_OK', UnUseSentence='BA_WAIT')
    t.mark('use')
    t.fire('sci', activator='player1', caller='player1')
    t.wait(1.0)
    t.snap('on', 'sci', 'follow')
    t.wait(3.5)                         # he waits a little between sentences
    t.mark('unuse')
    t.fire('sci', activator='player1', caller='player1')
    t.wait(1.0)
    t.snap('off', 'sci', 'follow')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('on', 'sci', 'follow', 1)
    t.expect.value('off', 'sci', 'follow', 0)
    t.expect.sound('BA_OK', ent='sci', after='use', within=(0, 1.0), min=1)
    t.expect.sound('BA_WAIT', ent='sci', after='unuse', within=(0, 1.0), min=1)
