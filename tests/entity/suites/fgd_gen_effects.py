"""Tier 5, final pass, generated: effect entities' choice values, each value in its own entity and checked in the
message or state it drives: env_sprite "at attachment point" (the sprite's attachment, body), env_elight Attachment
(KeyedDLight attachment byte), env_tutor Alignment (TutorText align), env_message Sound Radius (the sound's
attenuation), item_sodacan Beverage Type (its skin)."""

from etest import Suite

SUITE = Suite('fgd_gen_effects', 'Tier 5: effect choice values (generated)', players=1)
S = SUITE
DOC = 'FGD effect choices'
MSG_ATTN = {0: 2.0, 1: 1.25}             # env_message Sound Radius -> ATTN_IDLE, ATTN_STATIC (effects.cpp CMessage::Spawn)


@S.test('env_sprite "...at attachment point" 0-4: the sprite rides on that attachment of its entity (body = the '
        'attachment)', players=1, doc=DOC, covers=tuple('env_sprite.frags=%d' % a for a in range(5)))
def sprite_attachment(t):
    t.point('monster_generic', targetname='host', at=(0, 150, 0), model='models/scientist.mdl', spawnflags='16')
    for a in range(5):
        t.point('env_sprite', targetname='sp%d' % a, at=(-200 + a * 100, 0, 64), model='sprites/glow01.spr',
                message='host', frags=str(a), spawnflags='1', names=('message',))
    t.wait(0.5)
    for a in range(5):
        t.snap('s', 'sp%d' % a, 'body', 'movetype')
        t.expect.value('s', 'sp%d' % a, 'body', a)
        t.expect.value('s', 'sp%d' % a, 'movetype', 12)          # MOVETYPE_FOLLOW


@S.test('env_elight Attachment on that entity 0-4: its KeyedDLight carries that attachment', players=1, doc=DOC,
        covers=tuple('env_elight.impulse=%d' % a for a in range(5)))
def elight_attachment(t):
    t.point('monster_generic', targetname='host', at=(0, 150, 0), model='models/scientist.mdl', spawnflags='16')
    for a in range(5):
        t.point('env_elight', targetname='el%d' % a, at=(-200 + a * 100, 0, 64), target='host', impulse=str(a),
                renderamt='64', rendercolor='255 255 255')
    t.wait(0.3)
    t.mark('go')
    for a in range(5):
        t.fire('el%d' % a)
    t.wait(0.3)
    for a in range(5):
        t.expect.msg('KeyedDLight', fields={'elight': 1, 'attachment': a}, after='go', min=1)


@S.test('env_tutor Alignment 0-8: the TutorText carries that alignment', players=1, doc=DOC,
        covers=tuple('env_tutor.align=%d' % a for a in range(9)))
def tutor_align(t):
    for a in range(9):
        t.point('env_tutor', targetname='tu%d' % a, message='ALIGN %d' % a, align=str(a), spawnflags='1')
    t.wait(0.2)
    for a in range(9):
        t.mark('a%d' % a)
        t.fire('tu%d' % a, activator='player1', use='on')
        t.wait(0.5)
        t.fire('tu%d' % a, activator='player1', use='off')     # closed, so the next one is not queued behind it
        t.wait(0.5)
    for a in range(9):
        t.expect.msg('TutorText', to='player1', fields={'align': a}, min=1)


@S.test('env_message Sound Radius 0 (Small: ATTN_IDLE 2) and 1 (Medium: ATTN_STATIC 1.25)', players=1, doc=DOC,
        covers=tuple('env_message.messageattenuation=%d' % a for a in MSG_ATTN))
def message_attn(t):
    for a in MSG_ATTN:
        t.point('env_message', targetname='m%d' % a, message='ATTN', messagesound='buttons/bell1.wav',
                messagevolume='10', messageattenuation=str(a))
    t.wait(0.2)
    for a, attn in MSG_ATTN.items():
        t.mark('g%d' % a)
        t.fire('m%d' % a, activator='player1')
        t.wait(1.0)
        t.expect.sound('buttons/bell1.wav', ent='m%d' % a, attn=attn, min=1, after='g%d' % a)


@S.test('item_sodacan Beverage Type 0-5: the can shows that skin', players=1, doc=DOC,
        covers=('item_sodacan.skin',) + tuple('item_sodacan.skin=%d' % s for s in range(6)))
def sodacan_skin(t):
    for s in range(6):
        t.point('item_sodacan', targetname='c%d' % s, at=(-200 + s * 80, 150, 0), skin=str(s))
    t.wait(0.3)
    for s in range(6):
        t.snap('s', 'c%d' % s, 'skin')
        t.expect.value('s', 'c%d' % s, 'skin', s)


SOUND_TYPES = (1, 2, 4, 8, 16, 32, 64)     # env_soundmark Sound Type: Combat ... Garbage (soundent.h bits_SOUND_*)


@S.test('env_soundmark Sound Type: each choice makes an AI sound of that type, at its Position (an entity) with '
        'its radius as the volume', players=1, doc=DOC,
        covers=('env_soundmark.position',) + tuple('env_soundmark.type=%d' % v for v in SOUND_TYPES))
def soundmark_types(t):
    for i, v in enumerate(SOUND_TYPES):
        x = -210 + i * 70
        t.point('info_target', targetname='at%d' % v, at=(x, 150, 16))
        t.point('env_soundmark', targetname='sm%d' % v, at=(x, -100, 16), position='at%d' % v, type=str(v),
                radius='300', duration='2', names=('position',))
    t.wait(0.2)
    for v in SOUND_TYPES:
        t.fire('sm%d' % v)
    t.wait(0.1)
    t.snap('s', 'player1', 'aisounds')
    for i, v in enumerate(SOUND_TYPES):
        t.expect.aisound('s', 'player1', v, pos=(-210 + i * 70, 150, 16), min=1)


@S.test('env_warpball AI Sound type 1 (Combat) and 32 (Danger): its warp makes that AI sound where it plays; 0: none',
        players=1, doc=DOC, covers=tuple('env_warpball.aisound=%d' % v for v in (0, 1, 32)))
def warpball_aisound(t):
    for i, v in enumerate((0, 1, 32)):
        t.point('env_warpball', targetname='wb%d' % v, at=(-150 + i * 150, 100, 40), aisound=str(v),
                aisound_duration='2', spawnflags='4')
    t.wait(0.2)
    for v in (0, 1, 32):
        t.fire('wb%d' % v)
    t.wait(0.1)
    t.snap('s', 'player1', 'aisounds')
    t.expect.aisound('s', 'player1', 1, pos=(0, 100, 40), tol=40, min=1)
    t.expect.aisound('s', 'player1', 32, pos=(150, 100, 40), tol=40, min=1)
    t.expect.aisound('s', 'player1', 1, pos=(-150, 100, 40), tol=40, count=0)
    t.expect.aisound('s', 'player1', 32, pos=(-150, 100, 40), tol=40, count=0)
