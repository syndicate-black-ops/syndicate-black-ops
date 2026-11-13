"""Tier 5, final pass, generated: scripted_sequence / aiscripted_sequence flags and keys (CCineMonster, scripted.cpp),
each class with the same checks: Auto-search (1024), Try search once (4096), Allow unnamed idle (262144), Allow Take
Damage TriggerCondition (524288), Repeatable (4), Leave Corpse (8); Target Monster, Action Animation, Fire on
Possession and Fire on Animation start."""

from etest import Suite

SUITE = Suite('fgd_gen_cine', 'Tier 5: scripted_sequence / aiscripted_sequence flags and keys (generated)', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#scripted-scenes-batch-4'
CLASSES = ('scripted_sequence', 'aiscripted_sequence')
NAMES = ('m_iszEntity', 'm_iszFireOnPossessed', 'm_iszFireOnBegin', 'm_iszFireOnAnimStart')


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def held(t, cls, name, at, script_kv=None, monster_kv=None, idle='idle1', play='barn_wave', script_at=None):
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16', names=('TriggerTarget',),
            **(monster_kv or {}))
    kv = dict(m_iszEntity=name, m_iszPlay=play, m_fMoveTo='0', m_flRadius='512')
    if idle:
        kv['m_iszIdle'] = idle
    kv.update(script_kv or {})
    t.point(cls, targetname=name + '_ss', at=script_at or at, angles='0 270 0', names=NAMES, **kv)


def cases(cls):
    short = 'ai' if cls == 'aiscripted_sequence' else 'ss'

    def test(title, covers, players=1):
        return S.test('%s %s' % (cls, title), players=players, doc=DOC, covers=tuple(c % cls for c in covers))

    @test('Target Monster and Action Animation: fired, it takes the named barney and plays barn_wave; Fire on '
          'Possession and Fire on Animation start fire once each',
          ('%s.m_iszEntity', '%s.m_iszPlay', '%s.m_iszFireOnPossessed', '%s.m_iszFireOnAnimStart'))
    def keys(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'bk', (0, 0, 0), {'m_iszFireOnPossessed': 'poss', 'm_iszFireOnAnimStart': 'anim'}, idle=None)
        for n in ('poss', 'anim'):
            info(t, n)
        t.wait(0.5)
        t.mark('go')
        t.fire('bk_ss')
        t.wait(0.6)
        t.snap('s', 'bk', 'seqname', 'cine')
        t.wait(2.0)
        t.expect.value('s', 'bk', 'seqname', 'barn_wave')
        for n in ('poss', 'anim'):
            t.expect.used(n, after='go', count=1)
    keys.__name__ = short + '_keys'

    @test('flag 1024 Auto-search: a named script takes the barney a maker makes later, without a trigger; without '
          'the flag it waits for its trigger', ('%s:1024',))
    def autosearch(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, fl in (('bau', -100, '1024'), ('bnt', 100, '0')):
            t.point(cls, targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n + '_kid', m_iszPlay='barn_wave',
                    m_fMoveTo='0', spawnflags=fl, names=('m_iszEntity',))
            t.point('monstermaker', targetname=n + '_mk', at=(x, 0, 0), monstertype='monster_barney',
                    monstercount='1', netname=n + '_kid', spawnflags='4', angles='0 270 0', names=('netname',))
        t.wait(0.5)
        t.fire('bau_mk')
        t.fire('bnt_mk')
        t.wait(1.6)
        t.snap('s', 'bau_kid', 'cine')
        t.snap('s', 'bnt_kid', 'cine')
        t.expect.value('s', 'bau_kid', 'cine', 1)
        t.expect.value('s', 'bnt_kid', 'cine', 0)
    autosearch.__name__ = short + '_autosearch'

    @test('flag 4096 Try search once: fired before its barney exists it does not keep looking; without it it takes '
          'the barney once he appears', ('%s:4096',))
    def try_once(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, fl in (('bto', -100, '4096'), ('btl', 100, '0')):
            t.point(cls, targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n + '_kid', m_iszPlay='barn_wave',
                    m_fMoveTo='0', spawnflags=fl, names=('m_iszEntity',))
            t.point('monstermaker', targetname=n + '_mk', at=(x, 0, 0), monstertype='monster_barney',
                    monstercount='1', netname=n + '_kid', spawnflags='4', angles='0 270 0', names=('netname',))
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
    try_once.__name__ = short + '_try_once'

    @test('flag 262144 Allow unnamed idle: an auto-searching script with an idle animation waits (the barney stays '
          'in it); without the flag it plays its action and lets go',
          ('%s:262144',) + (('%s.m_iszIdle',) if cls == 'scripted_sequence' else ()))
    def unnamed_idle(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, fl in (('bui', -100, 1024 | 262144), ('bun', 100, 1024)):
            t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
            t.point(cls, targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n, m_iszIdle='idle1', m_iszPlay='draw',
                    m_fMoveTo='0', spawnflags=str(fl), names=('m_iszEntity',))
        t.wait(5.0)
        t.snap('s', 'bui', 'cine')
        t.snap('s', 'bun', 'cine')
        t.expect.value('s', 'bui', 'cine', 1)
        t.expect.value('s', 'bun', 'cine', 0)
    unnamed_idle.__name__ = short + '_unnamed_idle'

    @test('flag 524288 Allow Take Damage TriggerCondition: the barney held by a no-interrupt script fires his Take '
          'Damage TriggerTarget when hit; without the flag he does not', ('%s:524288',))
    def takedamage(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'btd', (-100, 0, 0), {'m_interruptionPolicy': '1', 'spawnflags': '524288'},
             {'TriggerCondition': '2', 'TriggerTarget': 'btd_hit'})
        held(t, cls, 'btx', (100, 0, 0), {'m_interruptionPolicy': '1'},
             {'TriggerCondition': '2', 'TriggerTarget': 'btx_hit'})
        info(t, 'btd_hit')
        info(t, 'btx_hit')
        t.wait(2.0)
        t.mark('hit')
        t.damage('btd', 5)
        t.damage('btx', 5)
        t.wait(1.0)
        t.expect.fired('btd_hit', after='hit')
        t.expect.not_fired('btx_hit', after='hit')
    takedamage.__name__ = short + '_takedamage_trigger'

    @test('flag 4 Repeatable: fired again after its action it plays it again (Fire on Animation start twice); '
          'without the flag once', ('%s:4',))
    def repeatable(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'brp', (-100, 0, 0), {'spawnflags': '4', 'm_iszFireOnAnimStart': 'brp_b'}, idle=None)
        held(t, cls, 'bon', (100, 0, 0), {'m_iszFireOnAnimStart': 'bon_b'}, idle=None)
        info(t, 'brp_b')
        info(t, 'bon_b')
        t.wait(0.5)
        for _ in range(2):
            t.fire('brp_ss')
            t.fire('bon_ss')
            t.wait(4.0)
        t.expect.used('brp_b', count=2)
        t.expect.used('bon_b', count=1)
    repeatable.__name__ = short + '_repeatable'

    @test('flag 8 Leave Corpse: a barney whose script plays a death animation (dying_barney, script death event) '
          'stays a corpse; without the flag the corpse fades out and is removed', ('%s:8',))
    def leave_corpse(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'blc', (-100, 0, 0), {'spawnflags': '8'}, idle=None, play='dying_barney')
        held(t, cls, 'bfd', (100, 0, 0), idle=None, play='dying_barney')
        t.wait(0.5)
        t.mark('go')
        t.fire('blc_ss')
        t.fire('bfd_ss')
        t.wait(14.0)
        t.snap('d', 'blc', 'deadflag')
        t.expect.value('d', 'blc', 'deadflag', 2)          # DEAD_DEAD: the script's death event killed him
        t.expect.removed('blc', after='go', count=0)
        t.expect.removed('bfd', after='go', count=1)
    leave_corpse.__name__ = short + '_leave_corpse'

for _c in CLASSES:
    cases(_c)


def idle_case(cls, play):
    @S.test('%s Idle Animation: a named script with it takes its monster when the map starts and holds him in '
            'idle1 until it is triggered, then plays its action; one without it leaves its monster alone' % cls,
            players=1, doc=DOC, covers=('%s.m_iszIdle' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, idle in (('bwi', -100, 'idle1'), ('bwo', 100, None)):
            t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
            kv = dict(m_iszEntity=n, m_fMoveTo='0', m_flRadius='512')
            if cls == 'scripted_action':
                kv['m_fAction'] = '6'         # Reload
            else:
                kv['m_iszPlay'] = play
            if idle:
                kv['m_iszIdle'] = idle
            t.point(cls, targetname=n + '_ss', at=(x, 0, 0), angles='0 270 0', names=('m_iszEntity',), **kv)
        t.wait(3.0)
        t.snap('held', 'bwi', 'cine', 'seqname')
        t.snap('held', 'bwo', 'cine')
        t.fire('bwi_ss')
        t.wait(0.4)
        t.snap('act', 'bwi', 'seqname')
        t.expect.value('held', 'bwi', 'cine', 1)
        t.expect.value('held', 'bwi', 'seqname', 'idle1')
        t.expect.value('held', 'bwo', 'cine', 0)
        t.expect.value('act', 'bwi', 'seqname', play)
    case.__name__ = 'idle_' + cls
    return case


idle_case('aiscripted_sequence', 'barn_wave')
idle_case('scripted_action', 'reload')
