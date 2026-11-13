"""Tier 5, final pass: "SpeakAs" on monster_sitting_scientist (its own Precache now gives the key to its sentence
groups, CTalkMonster::Precache). Its own suite: a sitting scientist greets a player it sees once nobody else talks
(FIdleHello, the shared g_talkWaitTime), so no other talking monster is in the map."""

from etest import Suite

SUITE = Suite('fgd_talk_speakas', 'Tier 5: sitting scientist SpeakAs', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


@S.test('monster_sitting_scientist "SpeakAs" BA: his greeting is the barney group BA_PHELLO (Half-Life has none, so '
        'he says nothing), no scientist line; one without the key greets the player with SC_PHELLO', players=1, doc=DOC,
        covers=('monster_sitting_scientist.SpeakAs',))
def sitting_speak_as(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, -150, 0), yaw=90)
    t.flags('player1', '-FL_NOTARGET')
    # placed 16 up, as on a chair (he settles on the floor): placed on the floor plane, he found no player in his PVS
    # and never spoke (FOkToSpeak)
    t.point('monster_sitting_scientist', targetname='sba', at=(-60, 60, 16), angles='0 270 0', SpeakAs='BA')
    t.point('monster_sitting_scientist', targetname='ssc', at=(60, 60, 16), angles='0 270 0')
    t.mark('seen')
    t.wait(10.0)        # each greets once he sees the player, one at a time (g_talkWaitTime)
    t.flags('player1', '+FL_NOTARGET')
    t.expect.sound('SC_PHELLO', ent='ssc', count=1, after='seen')
    t.expect.sound('SC_', ent='sba', count=0, after='seen')
