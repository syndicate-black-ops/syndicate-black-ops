"""Tier 5, final pass: scripted_sentence items no test asserted (scripted.cpp CScriptedSentence): Search Radius for a
speaker named by class, Fire Once (1), Followers Only (2), Interrupt Speech (4), Allow use No, Follow action Nothing /
Stop following. The speaker's sentence is a sound sample "!SC_OK0" in the trace."""

from etest import Suite

SUITE = Suite('fgd_sentence_more', 'Tier 5: scripted_sentence radius, flags, allow_use / follow_action choices',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


def sci(t, name, x, **kv):
    # not Gag (2): a gagged talk monster says no scripted sentence either (FOkToSpeak)
    t.point('monster_scientist', targetname=name, at=(x, 100, 0), angles='0 270 0', **kv)


def sentence(t, name, speaker, at=(0, 0, 0), flags='0', **kv):
    kv.setdefault('duration', '1.5')
    kv.setdefault('refire', '1')
    t.point('scripted_sentence', targetname=name, at=at, sentence='!SC_OK0', entity=speaker, radius='256',
            spawnflags=flags, names=('entity',), **kv)


@S.test('scripted_sentence "Search Radius" with a class as Target Monster: a scientist 200 away is found by a radius '
        'of 300 (he speaks), not by one of 100', players=1, doc=DOC, covers=('scripted_sentence.radius',))
def radius(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'near', -150)
    t.point('scripted_sentence', targetname='far', at=(50, 100, 0), sentence='!SC_OK0', entity='monster_scientist',
            radius='100', duration='1.5', refire='1')
    t.point('scripted_sentence', targetname='wide', at=(50, 100, 0), sentence='!SC_OK0', entity='monster_scientist',
            radius='300', duration='1.5', refire='1')
    t.wait(1.0)
    t.mark('small')
    t.fire('far')
    t.wait(1.0)
    t.mark('big')
    t.fire('wide')
    t.wait(1.0)
    t.expect.sound('SC_OK0', ent='near', after='small', within=(0, 0.9), count=0)
    t.expect.sound('SC_OK0', ent='near', after='big', min=1)


@S.test('scripted_sentence "Fire Once" (1): it is removed once its sentence starts; without the flag it stays',
        players=1, doc=DOC, covers=('scripted_sentence:1',))
def fire_once(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'a', -100)
    sci(t, 'b', 100)
    sentence(t, 'once', 'a', flags='1')
    sentence(t, 'again', 'b', flags='0')
    t.wait(1.0)
    t.fire('once')
    t.fire('again')
    t.wait(4.0)        # it looks for its speaker every 0.5 s and waits for the talk pause (CTalkMonster::FOkToSpeak)
    t.expect.sound('SC_OK0', ent='a', min=1)
    t.expect.removed('once')
    t.expect.not_removed('again')


@S.test('scripted_sentence "Followers Only" (2): a scientist that does not follow the player does not say it; once he '
        'follows (+use), he does', players=1, doc=DOC, covers=('scripted_sentence:2',))
def followers_only(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'f', 0)
    sentence(t, 'say', 'f', flags='2')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(1.0)
    t.mark('idle')
    t.fire('say')
    t.wait(1.5)
    t.fire('f', activator='player1', caller='player1')     # +use: he follows
    t.wait(4.0)                                             # his "ok" answer first
    t.snap('on', 'f', 'follow')
    t.mark('follow')
    t.fire('say')
    t.wait(1.5)
    t.expect.value('on', 'f', 'follow', 1)
    t.expect.sound('SC_OK0', ent='f', after='idle', within=(0, 1.4), count=0)
    t.expect.sound('SC_OK0', ent='f', after='follow', min=1)


@S.test('scripted_sentence "Interrupt Speech" (4): it makes a scientist who is saying another sentence say this one; '
        'without the flag he finishes the first and this one is not said', players=1, doc=DOC,
        covers=('scripted_sentence:4',))
def interrupt(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'i', -100)
    sci(t, 'n', 100)
    t.point('scripted_sentence', targetname='busy_i', sentence='!SC_HELLO0', entity='i', duration='4', radius='256',
            refire='1', names=('entity',))
    t.point('scripted_sentence', targetname='busy_n', sentence='!SC_HELLO0', entity='n', duration='4', radius='256',
            refire='1', names=('entity',))
    sentence(t, 'cut', 'i', flags='4')
    sentence(t, 'wait', 'n', flags='0')
    t.wait(1.0)
    t.fire('busy_i')
    t.fire('busy_n')
    t.wait(0.5)
    t.mark('second')
    t.fire('cut')
    t.fire('wait')
    t.wait(1.0)
    t.expect.sound('SC_OK0', ent='i', after='second', min=1)
    t.expect.sound('SC_OK0', ent='n', after='second', within=(0, 0.9), count=0)


@S.test('scripted_sentence "allow_use" No (0): +use while the scientist says it is ignored (Half-Life)', players=1,
        doc=DOC, covers=('scripted_sentence.allow_use=0',))
def allow_use_no(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'u', 0)
    sentence(t, 'say', 'u', allow_use='0', duration='10')
    t.wait(1.0)
    t.mark('say')
    t.fire('say')
    t.wait(4.0)        # the talk pause; then he speaks for 10 s
    t.fire('u', activator='player1', caller='player1')
    t.wait(0.5)
    t.snap('s', 'u', 'follow')
    t.expect.sound('SC_OK0', ent='u', after='say', within=(0, 4.0), min=1)
    t.expect.value('s', 'u', 'follow', 0)


@S.test('scripted_sentence "follow_action" Nothing (0): the scientist does not start following; Stop following (2): a '
        'following scientist stops', players=1, doc=DOC,
        covers=('scripted_sentence.follow_action=0', 'scripted_sentence.follow_action=2'))
def follow_actions(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'n0', -100)
    sci(t, 's2', 100)
    sentence(t, 'say0', 'n0', follow_action='0')
    sentence(t, 'say2', 's2', follow_action='2', flags='4')     # Interrupt Speech: a follower may be chatting
    t.teleport('player1', (100, 0, 0), yaw=90)
    t.wait(1.0)
    t.fire('s2', activator='player1', caller='player1')     # +use: s2 follows
    t.wait(4.0)
    t.snap('before', 's2', 'follow')
    t.mark('say')
    t.fire('say0')
    t.wait(4.0)        # each looks for its speaker every 0.5 s and waits for the talk pause (one talker at a time)
    t.mark('say2')
    t.fire('say2')
    t.wait(4.0)
    t.snap('after', 'n0', 'follow')
    t.snap('after', 's2', 'follow')
    t.expect.value('before', 's2', 'follow', 1)
    t.expect.value('after', 'n0', 'follow', 0)
    t.expect.value('after', 's2', 'follow', 0)
    t.expect.sound('SC_OK0', ent='n0', after='say', min=1)
    t.expect.sound('SC_OK0', ent='s2', after='say2', min=1)


@S.test('scripted_sentence "Delay Before Refire" 5: fired again 3 s after its 1 s sentence it says nothing (it waits '
        'the sentence time + 5 s); fired 8 s after, it speaks again', players=1, doc=DOC,
        covers=('scripted_sentence.refire',))
def refire(t):
    t.flags('player1', '+FL_NOTARGET')
    sci(t, 'rf', 0)
    sentence(t, 'srf', 'rf', flags='4', duration='1', refire='5')
    t.wait(1.0)
    t.mark('first')
    t.fire('srf')
    t.wait(3.0)
    t.mark('early')
    t.fire('srf')
    t.wait(5.0)
    t.mark('again')
    t.fire('srf')
    t.wait(1.0)
    t.expect.sound('SC_OK0', ent='rf', after='first', within=(0, 0.5), count=1)
    t.expect.sound('SC_OK0', ent='rf', after='early', within=(0, 4.9), count=0)
    t.expect.sound('SC_OK0', ent='rf', after='again', within=(0, 0.9), count=1)
