"""A player's death sound (CBasePlayer::DeathSound). Under water it emits nothing: player/h2odeath.wav is not
precached, and emitting it made the engine print "SV_BuildSoundMsg: player/h2odeath.wav not precached".
On land a pain sound (pl_pain5/6/7; RANDOM_LONG(1,5), two times in five none) and the suit's death alarm (a random
HEV_DEAD sentence, which is sometimes not sent) play on CHAN_VOICE. Both are random, so six players die on land and
at least one of them must play each. Every player dies once (a player who respawns has spawn protection); player1
under water first, so the others keep the round going."""

from etest import Suite

LAND = ['player%d' % i for i in range(2, 8)]
SUITE = Suite('death_sound', 'Player death sounds: under water and on land', players=1 + len(LAND))
S = SUITE
S.server_args = ('+developer', '1')    # any engine warning about the sound, developer message or not
DOC = 'SBO-2.0-CHANGES.md'
CHAN_VOICE = 2


def kill(t, player, mark='death'):
    """Kills `player` without gibbing him (Killed gibs below -40 health, and a gibbed player plays no death sound)."""
    t.kv(player, 'armorvalue', '0')
    t.kv(player, 'health', '1')
    t.mark(mark)
    t.damage(player, 5, 0)      # DMG_GENERIC: health -4
    t.wait(1.0)
    t.snap('dead_' + player, player, 'deadflag')
    t.expect.value('dead_' + player, player, 'deadflag', 0, op='>')


@S.test('player killed under water: no death sound and no "not precached" warning', players=1, doc=DOC)
def under_water(t):
    t.brush('func_water', size=(400, 400, 160), at=(0, 0, 0), skin='-3')
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.wait(2.5)     # past the 2 s spawn protection of the round start (CCoop::PlayerSpawn: m_iSpawnProtectTime)
    t.snap('in', 'player1', 'waterlevel')
    kill(t, 'player1')
    t.expect.value('in', 'player1', 'waterlevel', 3)
    t.expect.sound('h2odeath', count=0)
    t.expect.sound('pl_pain', ent='player1', after='death', count=0)     # the land death sounds do not play either
    t.expect.sound('HEV_DEAD', ent='player1', after='death', count=0)
    t.expect.no_console('player/h2odeath.wav')
    t.expect.no_console('not precached')


@S.test('players killed on land: their death sounds (pain sound, suit alarm HEV_DEAD) play', players=1 + len(LAND),
        doc=DOC)
def on_land(t):
    for i, p in enumerate(LAND):
        t.teleport(p, (-200 + 80 * i, 0, 0), yaw=0)
    t.wait(0.5)
    t.snap('in', LAND[0], 'waterlevel')
    t.expect.value('in', LAND[0], 'waterlevel', 0)
    t.mark('land')
    for p in LAND:
        kill(t, p)
    t.expect.sound('pl_pain', after='land', chan=CHAN_VOICE, min=1)
    t.expect.sound('HEV_DEAD', after='land', chan=CHAN_VOICE, min=1)
    t.expect.sound('h2odeath', count=0)
