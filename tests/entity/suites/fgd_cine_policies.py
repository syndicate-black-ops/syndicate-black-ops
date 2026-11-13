"""Tier 5, final pass: the Scripted base keys (CCineMonster, scripted.cpp) on scripted_sequence and aiscripted_sequence,
as suite fgd_scripted checks them on scripted_action: TakeDamage, Interruption, Required MonsterState, Search policy,
search radius, Priority. A prisoner barney is held by the script in his idle; the script plays 'reload'."""

from etest import Suite

SUITE = Suite('fgd_cine_policies', 'Tier 5: Scripted base keys on scripted_sequence / aiscripted_sequence',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
CLASSES = ('scripted_sequence', 'aiscripted_sequence')
NAMES = ('m_iszEntity', 'm_iszFireOnPossessed')
STATES = {0: 1, 1: 1, 2: 0, 3: 0, 4: 1}     # required_state -> takes an idle barney


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def held(t, cls, name, at, script_kv=None, script_at=None):
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16')
    kv = dict(m_iszEntity=name, m_iszPlay='reload', m_fMoveTo='0', m_flRadius='512', m_iszIdle='idle1')
    kv.update(script_kv or {})
    t.point(cls, targetname=name + '_act', at=script_at or at, angles='0 270 0', names=NAMES, **kv)


def cases(cls):
    short = cls.replace('scripted_', '').replace('ai', 'ai_')

    @S.test('%s TakeDamage policy 1 (Invulnerable) survives 110 damage; 2 (Non-lethal) keeps 1 health; 0 dies' % cls,
            players=1, doc=DOC, covers=tuple('%s.m_takeDamagePolicy=%d' % (cls, v) for v in (0, 1, 2)))
    def damage_policy(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'binv', (-150, 0, 0), {'m_takeDamagePolicy': '1'})
        held(t, cls, 'bnl', (0, 0, 0), {'m_takeDamagePolicy': '2', 'm_interruptionPolicy': '1'})
        held(t, cls, 'bstk', (150, 0, 0), {'m_takeDamagePolicy': '0'})
        t.wait(2.0)
        t.damage('binv', 110)
        t.damage('bnl', 1000)
        t.damage('bstk', 110)
        t.wait(0.3)
        for n in ('binv', 'bnl', 'bstk'):
            t.snap('s', n, 'deadflag', 'health')
        t.expect.value('s', 'binv', 'deadflag', 0)
        t.expect.value('s', 'bnl', 'deadflag', 0)
        t.expect.value('s', 'bnl', 'health', 1, tol=0.01)
        t.expect.value('s', 'bstk', 'deadflag', 0, op='>')
    damage_policy.__name__ = short + '_damage'

    @S.test('%s Interruption policy 2 and 1 leave the barney in the script after a hit; 0 lets the hit break him out'
            % cls, players=1, doc=DOC, covers=tuple('%s.m_interruptionPolicy=%d' % (cls, v) for v in (0, 1, 2)))
    def interruption(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'bdo', (-150, 0, 0), {'m_interruptionPolicy': '2'})
        held(t, cls, 'bni', (0, 0, 0), {'m_interruptionPolicy': '1'})
        held(t, cls, 'bdf', (150, 0, 0), {'m_interruptionPolicy': '0'})
        t.wait(2.0)
        for n in ('bdo', 'bni', 'bdf'):
            t.damage(n, 10, attacker='player1')
        t.wait(1.0)
        for n in ('bdo', 'bni', 'bdf'):
            t.snap('s', n, 'cine')
        t.expect.value('s', 'bdo', 'cine', 1)
        t.expect.value('s', 'bni', 'cine', 1)
        t.expect.value('s', 'bdf', 'cine', 0)
    interruption.__name__ = short + '_interruption'

    @S.test('%s Required MonsterState: an idle barney is taken by Any, Idle and Non-combat, not by Combat or Alert'
            % cls, players=1, doc=DOC, covers=tuple('%s.required_state=%d' % (cls, s) for s in STATES))
    def required_state(t):
        t.flags('player1', '+FL_NOTARGET')
        for i, s in enumerate(STATES):
            held(t, cls, 'b%d' % s, (-200 + i * 100, 0, 0), {'required_state': str(s)})
        t.wait(2.5)
        for s, took in STATES.items():
            t.snap('s', 'b%d' % s, 'cine')
            t.expect.value('s', 'b%d' % s, 'cine', took)
    required_state.__name__ = short + '_required_state'

    @S.test('%s Search policy 1 (Targetname only) / 2 (Classname only): a class as Target Monster is taken by 0 and '
            '2, a name by 0 and 1' % cls, players=1, doc=DOC,
            covers=tuple('%s.m_searchPolicy=%d' % (cls, v) for v in (0, 1, 2)))
    def search_policy(t):
        t.flags('player1', '+FL_NOTARGET')
        rows = [('c0', 'monster_barney', '0', 1), ('c1', 'monster_barney', '1', 0), ('c2', 'monster_barney', '2', 1),
                ('n0', None, '0', 1), ('n1', None, '1', 1), ('n2', None, '2', 0)]
        for i, (n, target, pol, took) in enumerate(rows):
            x, y = -200 + (i % 3) * 200, -150 + (i // 3) * 300
            t.point('monster_barney', targetname=n, at=(x, y, 0), angles='0 270 0', spawnflags='16')
            t.point(cls, targetname=n + '_act', at=(x, y + 40, 0), m_iszEntity=target or n, m_iszIdle='idle1',
                    m_iszPlay='reload', m_flRadius='80', m_fMoveTo='0', m_searchPolicy=pol,
                    names=() if target else ('m_iszEntity',))
        t.wait(2.5)
        for n, _, _, took in rows:
            t.snap('s', n, 'cine')
            t.expect.value('s', n, 'cine', took)
    search_policy.__name__ = short + '_search_policy'

    @S.test('%s Test against search radius 1 (Always): a barney named in Target Monster 300 away (radius 100) is not '
            'taken; 0: he is' % cls, players=1, doc=DOC,
            covers=tuple('%s.m_applySearchRadius=%d' % (cls, v) for v in (0, 1)))
    def apply_radius(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'bar', (-150, -150, 0), {'m_applySearchRadius': '1', 'm_flRadius': '100'},
             script_at=(-150, 150, 0))
        held(t, cls, 'bad', (150, -150, 0), {'m_applySearchRadius': '0', 'm_flRadius': '100'},
             script_at=(150, 150, 0))
        t.wait(2.5)
        t.snap('s', 'bar', 'cine')
        t.snap('s', 'bad', 'cine')
        t.expect.value('s', 'bar', 'cine', 0)
        t.expect.value('s', 'bad', 'cine', 1)
    apply_radius.__name__ = short + '_apply_radius'

    @S.test('%s Priority 4 (Override) takes a barney another script holds (Fire on Possession fires); 0 does not'
            % cls, players=1, doc=DOC, covers=tuple('%s.m_iPriority=%d' % (cls, v) for v in (0, 4)))
    def priority(t):
        t.flags('player1', '+FL_NOTARGET')
        held(t, cls, 'bpo', (-100, 0, 0))
        held(t, cls, 'bpn', (100, 0, 0))
        for n, who, pri in (('ov', 'bpo', '4'), ('nm', 'bpn', '0')):
            t.point(cls, targetname=n, at=(-100 if who == 'bpo' else 100, 60, 0), m_iszEntity=who,
                    m_iszPlay='reload', m_fMoveTo='0', m_iPriority=pri, m_iszFireOnPossessed=n + '_took', names=NAMES)
            info(t, n + '_took')
        t.wait(2.0)
        t.mark('go')
        t.fire('ov')
        t.fire('nm')
        t.wait(2.5)
        t.expect.fired('ov_took', count=1, after='go')
        t.expect.fired('nm_took', count=0, after='go')
    priority.__name__ = short + '_priority'


for _c in CLASSES:
    cases(_c)
