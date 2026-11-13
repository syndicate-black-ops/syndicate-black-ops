"""Tier 5, final pass: talk monster flags and keys on the classes with their own code: monster_barney Gag (2) and
Pre-Disaster (256), monster_sitting_scientist Gag (2) and "voicepitch" (scientist.cpp CSittingScientist: its own
Spawn / Precache / SittingThink; its "SpeakAs": suite fgd_talk_speakas). A gagged talk monster says no scripted
sentence without Interrupt Speech (FOkToSpeak). The player is notarget."""

from etest import Suite

SUITE = Suite('fgd_talk_flags', 'Tier 5: barney / sitting scientist Gag, Pre-Disaster, voicepitch, SpeakAs', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


def sentence(t, name, speaker, at=(0, 0, 0), flags='4'):
    t.point('scripted_sentence', targetname=name, at=at, sentence='!SC_OK0', entity=speaker, radius='256',
            refire='1', duration='1.5', spawnflags=flags, names=('entity',))


def gag_case(cls):
    @S.test('%s Gag (2): it says no scripted sentence; one without the flag does' % cls, players=1, doc=DOC,
            covers=('%s:2' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (0, -150, 0), yaw=90)     # a talk monster says nothing with no player in its PVS
        z = 16 if cls == 'monster_sitting_scientist' else 0     # as on a chair (fgd_talk_speakas)
        t.point(cls, targetname='mg', at=(-100, 100, z), angles='0 270 0', spawnflags='2')
        t.point(cls, targetname='mt', at=(100, 100, z), angles='0 270 0')
        # without Interrupt Speech (4): with it the speaker's state is not asked (FOkToSpeak, where Gag is)
        sentence(t, 'sg', 'mg', at=(-100, 100, 0), flags='0')
        sentence(t, 'st', 'mt', at=(100, 100, 0), flags='0')
        t.wait(1.0)
        t.mark('gag')
        t.fire('sg')
        t.wait(2.0)
        t.mark('talk')
        t.fire('st')
        # a talk monster's own chatter holds every talker's next line up to 5.2 s (the shared talk timer,
        # FIdleSpeak); a scripted sentence asked for meanwhile waits and tries again
        t.wait(7.0)
        t.expect.sound('SC_OK0', ent='mg', count=0, after='gag')
        t.expect.sound('SC_OK0', ent='mt', min=1, after='talk')
    case.__name__ = 'gag_' + cls.replace('monster_', '')
    return case


for _c in ('monster_barney', 'monster_sitting_scientist'):
    gag_case(_c)


def use_on(t, name, label):
    t.mark(label)
    t.fire(name, activator='player1', caller='player1')
    t.wait(1.0)


@S.test('monster_barney Pre-Disaster (256): a player-ally barney refuses +use (BA_POK); one without the flag '
        'starts following (BA_OK)', players=1, doc=DOC, covers=('monster_barney:256',))
def barney_predisaster(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='bpd', at=(-100, 150, 0), angles='0 270 0', is_player_ally='1',
            followage_policy='0', spawnflags='256')
    t.point('monster_barney', targetname='bok', at=(100, 150, 0), angles='0 270 0', is_player_ally='1',
            followage_policy='0')
    t.wait(0.5)
    use_on(t, 'bpd', 'u1')
    t.wait(3.0)         # the next one may speak (g_talkWaitTime)
    use_on(t, 'bok', 'u2')
    t.expect.sound('BA_POK', ent='bpd', after='u1')
    t.expect.sound('BA_OK', ent='bpd', count=0)
    t.expect.sound('BA_OK', ent='bok', after='u2')


@S.test('monster_sitting_scientist "voicepitch" 150: he says his sentences at pitch 150 .. 153; one without it at '
        'his head\'s pitch (Glasses: 105 .. 108)', players=1, doc=DOC,
        covers=('monster_sitting_scientist.voicepitch',))
def sitting_voicepitch(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_sitting_scientist', targetname='hp', at=(-100, 100, 0), angles='0 270 0', voicepitch='150',
            body='0')
    t.point('monster_sitting_scientist', targetname='np', at=(100, 100, 0), angles='0 270 0', body='0')
    sentence(t, 'shp', 'hp', at=(-100, 100, 0))
    sentence(t, 'snp', 'np', at=(100, 100, 0))
    t.wait(1.0)
    t.mark('hp')
    t.fire('shp')
    t.wait(2.0)
    t.mark('np')
    t.fire('snp')
    t.wait(2.0)
    t.expect.sound('SC_OK0', ent='hp', pitch=(150, 153), min=1, after='hp')
    t.expect.sound('SC_OK0', ent='np', pitch=(105, 108), min=1, after='np')
