"""SBO 2.0's own entities on develop: trigger_changevalue / trigger_copyvalue (docs/TRIGGER_CHANGEVALUE.md),
game_timer (GAME_TIMER.md), trigger_sequence (SEQUENCES.md), env_fogzone, env_tutor (TUTOR_PORT.md),
game_messagebox (MESSAGE_BOX.md)."""

from etest import Suite

SUITE = Suite('sbo', "SBO 2.0's own entities (develop)")
S = SUITE
CV = 'TRIGGER_CHANGEVALUE.md'


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


# ---------------------------------------------------------------------------------------------- trigger_changevalue
@S.test('trigger_changevalue: Add (1) to a func_breakable health', doc=CV + '#trigger_changevalue')
def cv_add(t):
    t.brush('func_breakable', targetname='crate', health='50')
    t.point('trigger_changevalue', targetname='cv', target='crate', m_iszValueName='health', m_iszNewValue='25',
            m_iszValueType='1', message='after')
    info(t, 'after')
    t.fire('cv')
    t.wait(0.1)
    t.snap('s', 'crate', 'health')
    t.expect.value('s', 'crate', 'health', 75)
    t.expect.used('crate', caller='cv', count=0)        # a change, not a trigger of the destination
    t.expect.used('after', caller='cv', count=1)         # "message" triggered after the change


@S.test('trigger_changevalue: Replace a vector, Mul a float, flag 2 leaves Y alone', doc=CV + '#trigger_changevalue')
def cv_vector(t):
    info(t, 'tgt', rendercolor='10 20 30', renderamt='40')
    t.point('trigger_changevalue', targetname='cv1', target='tgt', m_iszValueName='rendercolor',
            m_iszNewValue='100 200 250', m_iszValueType='0', spawnflags='2')
    t.point('trigger_changevalue', targetname='cv2', target='tgt', m_iszValueName='renderamt', m_iszNewValue='2.5',
            m_iszValueType='2')
    t.fire('cv1')
    t.fire('cv2')
    t.wait(0.1)
    t.snap('s', 'tgt', 'rendercolor', 'renderamt')
    t.expect.value('s', 'tgt', 'rendercolor', [100, 20, 250])
    t.expect.value('s', 'tgt', 'renderamt', 100)


@S.test('trigger_changevalue: custom keyvalue $i_ set then added, read back', doc=CV + '#how-a-value-is-read-and-written')
def cv_custom(t):
    info(t, 'var', **{'$i_count': '5'})
    t.point('trigger_changevalue', targetname='add3', target='var', m_iszValueName='$i_count', m_iszNewValue='3',
            m_iszValueType='1')
    t.snap('before', 'var', 'kv:$i_count')
    t.fire('add3')
    t.fire('add3')
    t.wait(0.1)
    t.snap('after', 'var', 'kv:$i_count')
    t.expect.value('before', 'var', 'kv:$i_count', 5)
    t.expect.value('after', 'var', 'kv:$i_count', 11)


@S.test('trigger_changevalue: division by zero and model keys leave the value as it was', doc=CV + '#limits-and-what-is-not-safe')
def cv_unsafe(t):
    t.brush('func_breakable', targetname='crate', health='50')
    t.point('trigger_changevalue', targetname='div0', target='crate', m_iszValueName='health', m_iszNewValue='0',
            m_iszValueType='4')
    t.point('trigger_changevalue', targetname='model', target='crate', m_iszValueName='model',
            m_iszNewValue='models/player.mdl')
    t.snap('before', 'crate', 'model')
    t.fire('div0')
    t.fire('model')
    t.wait(0.1)
    t.snap('after', 'crate', 'health', 'model')
    t.expect.value('after', 'crate', 'health', 50)
    t.expect.same('before', 'after', 'crate', 'model')


@S.test('trigger_changevalue: !activator changes the triggering player', doc=CV + '#trigger_changevalue', players=1)
def cv_activator(t):
    t.point('trigger_changevalue', targetname='cv', target='!activator', m_iszValueName='armorvalue',
            m_iszNewValue='42')
    t.fire('cv', activator='player1')
    t.wait(0.1)
    t.snap('s', 'player1', 'armorvalue')
    t.expect.value('s', 'player1', 'armorvalue', 42)


@S.test('trigger_changevalue / copyvalue on the int fields the save table calls float (flags, deadflag)',
        doc=CV + '#how-a-value-is-read-and-written')
def cv_int_flags(t):
    # flags, deadflag, fixangle, light_level are ints in entvars_t but FIELD_FLOAT in the save table: they were
    # computed and written as floats (flags 0 OR FL_NOTARGET read back 1124073472, the bits of 128.0f)
    info(t, 'tgt')
    info(t, 'copy_dst')
    info(t, 'loaded', deadflag='2')     # the map loader's path (EntvarsKeyvalue)
    t.point('trigger_changevalue', targetname='or', target='tgt', m_iszValueName='flags', m_iszNewValue='FL_NOTARGET',
            m_iszValueType='6')
    t.point('trigger_changevalue', targetname='dead', target='tgt', m_iszValueName='deadflag', m_iszNewValue='DEAD_DEAD')
    t.point('trigger_changevalue', targetname='add', target='tgt', m_iszValueName='deadflag', m_iszNewValue='1',
            m_iszValueType='1')
    t.point('trigger_copyvalue', targetname='copy', netname='tgt', m_iszSrcValueName='flags', target='copy_dst',
            m_iszDstValueName='flags', names=('netname',))
    t.fire('or')
    t.fire('dead')
    t.fire('add')
    t.fire('copy')
    t.wait(0.1)
    t.snap('s', 'tgt', 'flags', 'deadflag', 'kv:flags', 'kv:deadflag')
    t.snap('s', 'copy_dst', 'flags')
    t.snap('s', 'loaded', 'deadflag')
    t.expect.value('s', 'tgt', 'flags', 128)
    t.expect.value('s', 'tgt', 'deadflag', 3)
    t.expect.value('s', 'tgt', 'kv:flags', 128)
    t.expect.value('s', 'copy_dst', 'flags', 128)
    t.expect.value('s', 'loaded', 'deadflag', 2)


# ---------------------------------------------------------------------------------------------- trigger_copyvalue
@S.test('trigger_copyvalue: copies a key from one entity to another', doc=CV + '#trigger_copyvalue')
def copy_once(t):
    t.brush('func_breakable', at=(-64, 0, 0), targetname='src', health='33')
    t.brush('func_breakable', at=(64, 0, 0), targetname='dst', health='1')
    t.point('trigger_copyvalue', targetname='copy', netname='src', m_iszSrcValueName='health', target='dst',
            m_iszDstValueName='health', names=('netname',))
    t.fire('copy')
    t.wait(0.1)
    t.snap('s', 'dst', 'health')
    t.expect.value('s', 'dst', 'health', 33)


@S.test('trigger_copyvalue: constant mode copies every dmg seconds until OFF', doc=CV + '#trigger_copyvalue')
def copy_constant(t):
    info(t, 'src', **{'$f_v': '1'})
    info(t, 'dst', **{'$f_v': '0'})
    t.point('trigger_copyvalue', targetname='copy', netname='src', m_iszSrcValueName='$f_v', target='dst',
            m_iszDstValueName='$f_v', spawnflags='8', dmg='0.1', names=('netname',))
    t.point('trigger_changevalue', targetname='set7', target='src', m_iszValueName='$f_v', m_iszNewValue='7')
    t.point('trigger_changevalue', targetname='set9', target='src', m_iszValueName='$f_v', m_iszNewValue='9')
    t.fire('copy', use='on')
    t.wait(0.2)
    t.fire('set7')
    t.wait(0.4)
    t.snap('running', 'dst', 'kv:$f_v')
    t.fire('copy', use='off')
    t.wait(0.1)
    t.fire('set9')
    t.wait(0.4)
    t.snap('stopped', 'dst', 'kv:$f_v')
    t.expect.value('running', 'dst', 'kv:$f_v', 7)
    t.expect.value('stopped', 'dst', 'kv:$f_v', 7)


# ---------------------------------------------------------------------------------------------- game_timer
GT = 'GAME_TIMER.md'


@S.test('game_timer: enabled with ON runs out, fires target / resumetarget / pausetarget, tells clients',
        doc=GT + '#outputs-target-keys')
def timer_basic(t):
    t.point('game_timer', targetname='timer', timer_length='1', target='done', resumetarget='resumed',
            pausetarget='paused', warn_time='0')
    for n in ('done', 'resumed', 'paused'):
        info(t, n)
    t.mark('go')
    t.fire('timer', use='on')
    t.wait(1.5)
    t.expect.fired('resumed', count=1, after='go', within=(0, 0.1))
    t.expect.fired('done', caller='timer', count=1, after='go', within=(0.95, 1.15))
    t.expect.fired('paused', count=1, after='go', within=(0.95, 1.15))
    t.expect.order('done', 'paused')
    t.expect.msg('GameTimer', fields={'enabled': True, 'full': True}, after='go', within=(0, 0.1))
    t.expect.msg('GameTimer', fields={'enabled': False}, after='go', within=(0.95, 1.15))


@S.test('game_timer: warntarget at the warning time', doc=GT + '#outputs-target-keys')
def timer_warn(t):
    t.point('game_timer', targetname='timer', timer_length='2', warn_time='0.5', warntarget='warned', target='done')
    info(t, 'warned')
    info(t, 'done')
    t.mark('go')
    t.fire('timer', use='on')
    t.wait(2.3)
    t.expect.fired('warned', count=1, after='go', within=(1.4, 1.65))
    t.expect.fired('done', count=1, after='go', within=(1.95, 2.15))


@S.test('game_timer: SET with a value is SetTime', doc=GT + '#inputs')
def timer_set(t):
    t.point('game_timer', targetname='timer', timer_length='10', target='done', warn_time='0')
    info(t, 'done')
    t.fire('timer', use='on')
    t.wait(0.2)
    t.mark('set')
    t.fire('timer', use='set', value=0.5)
    t.wait(0.8)
    t.expect.fired('done', count=1, after='set', within=(0.45, 0.65))


@S.test('game_timer: paused 1 / 0 through trigger_changevalue holds and resumes it', doc=GT + '#inputs')
def timer_pause(t):
    t.point('game_timer', targetname='timer', timer_length='1', target='done', warn_time='0')
    info(t, 'done')
    t.point('trigger_changevalue', targetname='pause', target='timer', m_iszValueName='paused', m_iszNewValue='1')
    t.point('trigger_changevalue', targetname='resume', target='timer', m_iszValueName='paused', m_iszNewValue='0')
    t.fire('timer', use='on')
    t.wait(0.5)
    t.fire('pause')
    t.wait(1.0)
    t.snap('held', 'timer', 'kv:paused')
    t.mark('resume')
    t.fire('resume')
    t.wait(0.8)
    t.expect.value('held', 'timer', 'kv:paused', 1)
    t.expect.not_fired('done', within=(0, 1.45))
    t.expect.fired('done', count=1, after='resume', within=(0.4, 0.65))


@S.test('game_timer: add_time through trigger_changevalue, capped at max_length', doc=GT + '#keys-of-a-live-timer')
def timer_add(t):
    t.point('game_timer', targetname='timer', timer_length='1', max_length='1.5', target='done', warn_time='0')
    info(t, 'done')
    t.point('trigger_changevalue', targetname='add', target='timer', m_iszValueName='add_time', m_iszNewValue='5')
    t.mark('go')
    t.fire('timer', use='on')
    t.fire('add')
    t.wait(2.0)
    t.expect.fired('done', count=1, after='go', within=(1.45, 1.65))


@S.test('game_timer with trigger_copyvalue: time_remaining copied into a custom key', doc=GT + '#outputs-target-keys')
def timer_copy(t):
    t.point('game_timer', targetname='timer', timer_length='5', warn_time='0')
    info(t, 'store', **{'$f_left': '0'})
    t.point('trigger_copyvalue', targetname='read', netname='timer', m_iszSrcValueName='time_remaining',
            target='store', m_iszDstValueName='$f_left', names=('netname',))
    t.fire('timer', use='on')
    t.wait(1.0)
    t.fire('read')
    t.wait(0.05)
    t.snap('s', 'store', 'kv:$f_left')
    t.expect.value('s', 'store', 'kv:$f_left', 4.0, tol=0.15)


# ---------------------------------------------------------------------------------------------- trigger_sequence
SQ = 'SEQUENCES.md'


@S.test('trigger_sequence: #firetargets and #pause, caller is the trigger_sequence', doc=SQ + '#commands-')
def seq_basic(t):
    seq = t.sequence('alarm', '    #firetargets = "%s"\n    #pause = 0.5\n    #firetargets = "%s"'
                     % (t.name('a'), t.name('b')))
    t.point('trigger_sequence', targetname='seq', sequence_id=seq)
    info(t, 'a')
    info(t, 'b')
    t.mark('go')
    t.fire('seq', use='on')
    t.wait(0.9)
    t.expect.fired('a', caller='seq', count=1, after='go', within=(0, 0.1))
    t.expect.fired('b', caller='seq', count=1, after='go', within=(0.45, 0.65))


@S.test('trigger_sequence: OFF stops a running entry', doc=SQ + '#on-and-off')
def seq_off(t):
    seq = t.sequence('stopme', '    #firetargets = "%s"\n    #pause = 0.6\n    #firetargets = "%s"'
                     % (t.name('a'), t.name('b')))
    t.point('trigger_sequence', targetname='seq', sequence_id=seq)
    info(t, 'a')
    info(t, 'b')
    t.fire('seq', use='on')
    t.wait(0.2)
    t.fire('seq', use='off')
    t.wait(0.8)
    t.expect.fired('a', count=1)
    t.expect.not_fired('b')


@S.test('trigger_sequence: a sound line without a speaker plays everywhere (ambient sound, ATTN_NONE, full volume), '
        'not as the entity\'s own sound (the engine sends that only to the players near it)', doc=SQ + '#sounds')
def seq_sound_everywhere(t):
    t.seq.append('!sentences\n{\n    ETSQA0 debris/beamstart1\n}\n')
    seq = t.sequence('radio', '    #sound = ETSQA')
    t.point('trigger_sequence', at=(0, 0, 64), targetname='seq', sequence_id=seq)
    t.mark('go')
    t.fire('seq', use='on')
    t.wait(0.3)
    t.expect.sound('beamstart1', ent='seq', ambient=True, attn=0, vol=(1, 1), count=1, after='go')
    t.expect.sound('beamstart1', ambient=False, count=0, min=0)


@S.test('trigger_sequence: a sentences.txt group without a speaker plays everywhere too (ambient sound "!<number>", '
        'ATTN_NONE)', doc=SQ + '#sounds')
def seq_sentence_group_everywhere(t):
    seq = t.sequence('okay', '    #sound = BA_OK')
    t.point('trigger_sequence', at=(0, 0, 64), targetname='seq', sequence_id=seq)
    t.mark('go')
    t.fire('seq', use='on')
    t.wait(0.3)
    t.expect.sound('!', ent='seq', ambient=True, attn=0, count=1, after='go')
    t.expect.sound('', ent='seq', ambient=False, count=0, min=0)


@S.test('trigger_sequence: OFF stops its sound without a speaker (the ambient sound with SND_STOP); the next line '
        'stops the one before it', doc=SQ + '#on-and-off')
def seq_sound_off(t):
    t.seq.append('!sentences\n{\n    ETSQB0 debris/beamstart1\n}\n!sentences\n{\n    ETSQC0 debris/beamstart2\n}\n')
    seq = t.sequence('twolines', '    #sound = ETSQB\n    #pause = 0.3\n    #sound = ETSQC\n    #pause = 2\n'
                     '    #firetargets = "%s"' % t.name('end'))
    t.point('trigger_sequence', at=(0, 0, 64), targetname='seq', sequence_id=seq)
    info(t, 'end')
    t.mark('go')
    t.fire('seq', use='on')
    t.wait(0.5)
    t.mark('off')
    t.fire('seq', use='off')
    t.wait(0.2)
    t.expect.sound('beamstart1', ent='seq', ambient=True, attn=0, count=1, after='go', within=(0, 0.1))
    t.expect.sound('beamstart1', ent='seq', ambient=True, flags=32, count=1, after='go', within=(0.25, 0.4))
    t.expect.sound('beamstart2', ent='seq', ambient=True, attn=0, count=1, after='go', within=(0.25, 0.4))
    t.expect.sound('beamstart2', ent='seq', ambient=True, flags=32, count=1, after='off', within=(0, 0.1))
    t.expect.not_fired('end')


@S.test('trigger_sequence: a sound line with a speaker stays the monster\'s own sound at speech distance (ATTN_NORM)',
        doc=SQ + '#sounds')
def seq_sound_speaker(t):
    t.seq.append('!sentences\n{\n    ETSQD0 debris/beamstart1\n}\n')
    seq = t.sequence('said', '    #sound = ETSQD, speaker = %s' % t.name('sci'))
    t.point('monster_scientist', at=(0, 100, 0), targetname='sci', angles='0 270 0')
    t.point('trigger_sequence', at=(0, 0, 64), targetname='seq', sequence_id=seq)
    t.flags('sci', '+FL_NOTARGET')
    t.mark('go')
    t.fire('seq', use='on')
    t.wait(0.3)
    t.expect.sound('beamstart1', ent='sci', ambient=False, attn=0.8, count=1, after='go')
    t.expect.sound('beamstart1', ambient=True, count=0, min=0)


# ---------------------------------------------------------------------------------------------- env_fogzone
@S.test('env_fogzone: the client gets the zone (FogZone; it draws the zone fog itself), Fog carries the map fog (none)',
        players=1, doc='WEATHER.md#env_fogzone-prototype-haze-from-outside-fog-by-depth-prototype-not-tested-in-the-game')
def fogzone(t):
    t.brush('env_fogzone', size=(160, 160, 128), at=(0, 96, 0), tex='AAATRIGGER', targetname='fog', spawnflags='1',
            rendercolor='200 100 50', density='0.002')
    t.wait(0.3)
    # the full zone list went out with the HUD, before the test; switching the zone off and on sends it again
    t.mark('toggle')
    t.fire('fog')
    t.wait(0.1)
    t.fire('fog')
    t.wait(0.1)
    t.mark('in')
    t.teleport('player1', (0, 96, 0))
    t.wait(0.5)
    t.mark('out')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.5)
    # since the env_fogzone prototype (6c683df, player.cpp SendFog FOGZONE_CLIENT_SIDE) the zone's fog is not in Fog:
    # the client has the zone list (FogZone, with the HUD) and makes the fog by how deep the view is inside
    t.expect.msg('FogZone', to='player1', fields={'flags': 0, 'color': [200, 100, 50], 'density': 0.002}, after='toggle')
    t.expect.msg('FogZone', to='player1', fields={'flags': 1, 'color': [200, 100, 50], 'density': 0.002}, after='toggle')
    t.expect.msg('Fog', to='player1', fields={'density': 0}, after='in', within=(0, 0.3))
    t.expect.msg('Fog', to='player1', fields={'density': 0}, after='out', within=(0, 0.3))


# ---------------------------------------------------------------------------------------------- env_tutor / message box
@S.test('env_tutor: "Activator only" shows the window to the triggering player', players=1, doc='TUTOR_PORT.md')
def tutor(t):
    t.point('env_tutor', targetname='tut', spawnflags='1', message='Entity test tutor', type='64')
    t.mark('go')
    t.fire('tut', activator='player1')
    t.wait(0.3)
    t.expect.msg('TutorPart', to='player1', min=0)
    t.expect.msg('TutorText', to='player1', fields={'type': 64}, after='go', within=(0, 0.2))


@S.test('env_tutor: triggered again while shown, it is sent again (its sound plays again), the same show',
        players=1, doc='TUTOR_PORT.md')
def tutor_again(t):
    # interrupt: the last test's message may still be shown
    t.point('env_tutor', targetname='tut', spawnflags='9', message='Entity test tutor again', type='64',
            holdtime='10', target='shown', priority='100', interrupt='1')
    info(t, 'shown')
    t.mark('go')
    t.fire('tut', activator='player1')
    t.wait(1.0)
    t.fire('tut', activator='player1')
    t.wait(0.3)
    t.expect.msg('TutorText', to='player1', count=2, after='go')
    t.expect.no_msg('TutorClose', to='player1', after='go', within=(0, 1.2))     # (the OFF below closes it)
    t.expect.fired('shown', count=1)        # "target": once, for the one show
    t.fire('tut', activator='player1', use='off')


@S.test('game_messagebox: opens a box with its title and text for the activator', players=1, doc='MESSAGE_BOX.md')
def messagebox(t):
    t.point('game_messagebox', targetname='box', title='Test title', message='Test text')
    t.mark('go')
    t.fire('box', activator='player1')
    t.wait(0.3)
    t.expect.msg('MessageBox', to='player1', fields={'open': 1, 'title': 'Test title', 'text': 'Test text'},
                 after='go', within=(0, 0.2))
