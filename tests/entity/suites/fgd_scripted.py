"""Tier 5, batch 5: scripted_action FGD items that no 1.38 map uses and no other test asserted
(docs/FEATUREFUL_GUIDE_ENTITIES.md; the Scripted base keys work as on scripted_sequence).
Each action is seen as the sequence the monster plays (its model's sequence for that activity: hgrunt.mdl, imp.mdl).
The player is notarget so the monsters stay idle."""

from etest import Suite

SUITE = Suite('fgd_scripted', 'Tier 5: scripted_action actions, flags and Scripted keys', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
NAMES = ('m_iszEntity', 'm_iszAttack')

# action -> (monster class, the model's sequences for that activity)
ACTIONS = {1: ('monster_human_grunt', [9, 10]), 2: ('monster_human_grunt', [14]),
           3: ('monster_imp', [9]), 4: ('monster_human_grunt', [22]), 6: ('monster_human_grunt', [17]),
           7: ('monster_imp', [2])}
ACTION_NAMES = {1: 'Ranged Attack 2', 2: 'Melee Attack', 3: 'Melee Attack 2', 4: 'Special Attack', 6: 'Reload',
                7: 'Jump'}


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def action_case(action):
    cls, seqs = ACTIONS[action]

    @S.test('scripted_action Action %d (%s): the %s plays that animation, then the script fires its target' % (
            action, ACTION_NAMES[action], cls), players=1, doc=DOC,
            covers=('scripted_action.m_fAction=%d' % action, 'scripted_action.m_fMoveTo=0', 'scripted_action:64'))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.point(cls, targetname='mon', at=(-100, 0, 0), angles='0 0 0', spawnflags='16')
        t.point('scripted_action', targetname='act', at=(-100, 0, 0), m_iszEntity='mon', m_fAction=str(action),
                m_iszAttack='aim', m_fMoveTo='0', target='done', spawnflags=str(64 | 32), m_takeDamagePolicy='1',
                names=NAMES)
        info(t, 'aim', at=(220, 0, 32))
        t.kv('player1', 'max_health', '5000')
        t.kv('player1', 'health', '5000')
        t.teleport('player1', (-220, -200, 0))
        info(t, 'done')
        t.wait(1.0)
        t.mark('go')
        t.fire('act')
        t.wait(0.25)
        t.snap('s', 'mon', 'sequence')
        t.wait(3.0)
        t.expect.value_any(['s'], 'mon', 'sequence', seqs)
        t.expect.used('done', count=1, after='go', within=(0.3, 3.2))
    case.__name__ = 'action_%d' % action
    return case


for _a in sorted(ACTIONS):
    action_case(_a)


@S.test('scripted_action Action 5 (Special Attack 2) on a grunt, who has no such animation: the script ends at once '
        '(target fired), the grunt plays nothing new', players=1, doc=DOC, covers=('scripted_action.m_fAction=5',))
def action_missing(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_human_grunt', targetname='mon', at=(-100, 0, 0), angles='0 0 0', spawnflags='16')
    t.point('scripted_action', targetname='act', at=(-100, 0, 0), m_iszEntity='mon', m_fAction='5', m_fMoveTo='0',
            target='done', spawnflags='64', names=NAMES)
    info(t, 'done')
    t.wait(1.0)
    t.mark('go')
    t.fire('act')
    t.wait(0.5)
    t.expect.used('done', count=1, after='go', within=(0, 0.25))


# ---------------------------------------------------------------------------------------------- Scripted base keys
def held(t, name, at, script_kv=None, action='6', script_at=None):
    """A prisoner barney `name` and a scripted_action 'name_act' (Reload, 1.7 s) that takes him at map start and
    holds him in his idle until fired."""
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16')
    kv = dict(m_iszEntity=name, m_fAction=action, m_fMoveTo='0', m_flRadius='512', m_iszIdle='idle1')
    kv.update(script_kv or {})
    t.point('scripted_action', targetname=name + '_act', at=script_at or at, angles='0 270 0', names=NAMES, **kv)


@S.test('scripted_action TakeDamage policy 1 (Invulnerable): the barney survives 110 damage; 2 (Non-lethal) keeps '
        '1 health; 0 (As usual) dies', players=1, doc=DOC,
        covers=('scripted_action.m_takeDamagePolicy', 'scripted_action.m_takeDamagePolicy=0',
                'scripted_action.m_takeDamagePolicy=1', 'scripted_action.m_takeDamagePolicy=2'))
def damage_policy(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'binv', (-150, 0, 0), {'m_takeDamagePolicy': '1'})
    held(t, 'bnl', (0, 0, 0), {'m_takeDamagePolicy': '2', 'm_interruptionPolicy': '1'})
    held(t, 'bstk', (150, 0, 0), {'m_takeDamagePolicy': '0'})
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


@S.test('scripted_action Interruption policy 2 (By Death Only), 1 and flag 32 (No Interruptions): a hit leaves the '
        'barney in the script; 0 (Default): the hit breaks him out', players=1, doc=DOC,
        covers=('scripted_action.m_interruptionPolicy', 'scripted_action.m_interruptionPolicy=0',
                'scripted_action.m_interruptionPolicy=1', 'scripted_action.m_interruptionPolicy=2',
                'scripted_action:32'))
def interruption(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bdo', (-180, 0, 0), {'m_interruptionPolicy': '2'})
    held(t, 'bni', (-60, 0, 0), {'m_interruptionPolicy': '1'})
    held(t, 'bfl', (60, 0, 0), {'m_interruptionPolicy': '0', 'spawnflags': '32'})
    held(t, 'bdf', (180, 0, 0), {'m_interruptionPolicy': '0'})
    t.wait(2.0)
    for n in ('bdo', 'bni', 'bfl', 'bdf'):
        t.damage(n, 10, attacker='player1')
    t.wait(1.0)
    for n in ('bdo', 'bni', 'bfl', 'bdf'):
        t.snap('s', n, 'cine')
    t.expect.value('s', 'bdo', 'cine', 1)
    t.expect.value('s', 'bni', 'cine', 1)
    t.expect.value('s', 'bfl', 'cine', 1)
    t.expect.value('s', 'bdf', 'cine', 0)


STATES = {0: 1, 1: 1, 2: 0, 3: 0, 4: 1}     # required_state -> takes an idle barney


@S.test('scripted_action Required MonsterState: an idle barney is taken by Any, Idle and Non-combat, not by Combat '
        'or Alert / Hunt', players=1, doc=DOC,
        covers=('scripted_action.required_state',) + tuple('scripted_action.required_state=%d' % s for s in STATES))
def required_state(t):
    t.flags('player1', '+FL_NOTARGET')
    for i, s in enumerate(STATES):
        held(t, 'b%d' % s, (-200 + i * 100, 0, 0), {'required_state': str(s)})
    t.wait(2.5)
    for s in STATES:
        t.snap('s', 'b%d' % s, 'cine')
    for s, took in STATES.items():
        t.expect.value('s', 'b%d' % s, 'cine', took)


@S.test('scripted_action Search policy 1 (Targetname only) and 2 (Classname only): "monster_barney" as Target '
        'Monster is a class (taken by 0 and 2, not 1); a barney\'s name is taken by 0 and 1, not 2', players=1,
        doc=DOC,
        covers=('scripted_action.m_searchPolicy', 'scripted_action.m_searchPolicy=0',
                'scripted_action.m_searchPolicy=1', 'scripted_action.m_searchPolicy=2', 'scripted_action.m_flRadius'))
def search_policy(t):
    t.flags('player1', '+FL_NOTARGET')
    cases = [('c0', 'monster_barney', '0', 1), ('c1', 'monster_barney', '1', 0), ('c2', 'monster_barney', '2', 1),
             ('n0', None, '0', 1), ('n1', None, '1', 1), ('n2', None, '2', 0)]
    for i, (n, target, pol, took) in enumerate(cases):
        x, y = -200 + (i % 3) * 200, -150 + (i // 3) * 300
        t.point('monster_barney', targetname=n, at=(x, y, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_action', targetname=n + '_act', at=(x, y + 40, 0), m_iszEntity=target or n,
                m_iszIdle='idle1', m_fAction='6', m_flRadius='80', m_fMoveTo='0', m_searchPolicy=pol,
                names=() if target else ('m_iszEntity',))
    t.wait(2.5)
    for n, _, _, took in cases:
        t.snap('s', n, 'cine')
        t.expect.value('s', n, 'cine', took)


@S.test('scripted_action Test against search radius 1 (Always): a barney named in Target Monster 300 away (radius '
        '100) is not taken; 0: he is', players=1, doc=DOC,
        covers=('scripted_action.m_applySearchRadius', 'scripted_action.m_applySearchRadius=0',
                'scripted_action.m_applySearchRadius=1', 'scripted_action.m_flRadius'))
def apply_radius(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bar', (-150, -150, 0), {'m_applySearchRadius': '1', 'm_flRadius': '100'}, script_at=(-150, 150, 0))
    held(t, 'bad', (150, -150, 0), {'m_applySearchRadius': '0', 'm_flRadius': '100'}, script_at=(150, 150, 0))
    t.wait(2.5)
    t.snap('s', 'bar', 'cine')
    t.snap('s', 'bad', 'cine')
    t.expect.value('s', 'bar', 'cine', 0)
    t.expect.value('s', 'bad', 'cine', 1)


@S.test('scripted_action Priority 4 (Override) takes a barney another script holds (Fire on Possession fires); 0 '
        '(Normal) does not', players=1, doc=DOC,
        covers=('scripted_action.m_iPriority', 'scripted_action.m_iPriority=0', 'scripted_action.m_iPriority=4',
                'scripted_action.m_iszFireOnPossessed'))
def priority(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bpo', (-100, 0, 0))
    held(t, 'bpn', (100, 0, 0))
    t.point('scripted_action', targetname='ov', at=(-100, 60, 0), m_iszEntity='bpo', m_fAction='6', m_fMoveTo='0',
            m_iPriority='4', m_iszFireOnPossessed='ov_took', names=NAMES)
    t.point('scripted_action', targetname='nm', at=(100, 60, 0), m_iszEntity='bpn', m_fAction='6', m_fMoveTo='0',
            m_iPriority='0', m_iszFireOnPossessed='nm_took', names=NAMES)
    info(t, 'ov_took')
    info(t, 'nm_took')
    t.wait(2.0)
    t.mark('go')
    t.fire('ov')
    t.fire('nm')
    t.wait(2.5)
    t.expect.fired('ov_took', count=1, after='go')
    t.expect.not_fired('nm_took', after='go')


@S.test('scripted_action Repeat action 2 more times: the 1.7 s reload plays three times (still in the script at '
        '4 s); without it out at 4 s', players=1, doc=DOC, covers=('scripted_action.m_iRepeats',))
def repeats(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'brp', (-100, 0, 0), {'m_iRepeats': '2'})
    held(t, 'bon', (100, 0, 0))
    t.wait(2.0)
    t.fire('brp_act')
    t.fire('bon_act')
    t.wait(3.0)
    t.snap('a', 'brp', 'cine')
    t.snap('a', 'bon', 'cine')
    t.expect.value('a', 'brp', 'cine', 1)
    t.expect.value('a', 'bon', 'cine', 0)


@S.test('scripted_action master: while the env_state master is off the script waits; once on it takes the barney',
        players=1, doc=DOC, covers=('scripted_action.master',))
def master(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('env_state', targetname='gate')
    t.point('monster_barney', targetname='bm', at=(0, 0, 0), angles='0 270 0', spawnflags='16')
    t.point('scripted_action', targetname='act', at=(0, 0, 0), m_iszEntity='bm', m_fAction='6', m_fMoveTo='0',
            master='gate', m_iszFireOnPossessed='took', names=NAMES)
    info(t, 'took')
    t.wait(0.5)
    t.mark('locked')
    t.fire('act')
    t.wait(1.5)
    t.mark('open')
    t.fire('gate', use='on')
    t.wait(2.5)
    t.expect.not_fired('took', after='locked', within=(0, 1.4))
    t.expect.fired('took', count=1, after='open')


@S.test('scripted_action Move to 4 (Instantaneous) and 7 (Teleport): the barney is at the script at once; Turn mode '
        '1 / 2 / 0 with Move to 5 (No - Turn to Face): he turns to it, does not turn, takes its angle (90)',
        players=1, doc=DOC,
        covers=('scripted_action.m_fMoveTo', 'scripted_action.m_fMoveTo=4', 'scripted_action.m_fMoveTo=7',
                'scripted_action.m_fMoveTo=5', 'scripted_action.m_fTurnType', 'scripted_action.m_fTurnType=0',
                'scripted_action.m_fTurnType=1', 'scripted_action.m_fTurnType=2'))
def move_turn(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, mt in (('bi', -150, '4'), ('bt', 150, '7')):
        t.point('monster_barney', targetname=n, at=(x, -150, 0), angles='0 90 0', spawnflags='16')
        t.point('scripted_action', targetname=n + '_act', at=(x, 150, 0), angles='0 90 0', m_iszEntity=n,
                m_fAction='6', m_fMoveTo=mt, names=NAMES)
        info(t, n + '_mark', at=(x, 150, 0))
    for n, x, tt in (('btf', -60, '1'), ('btn', 0, '2'), ('btm', 60, '0')):
        t.point('monster_barney', targetname=n, at=(x, -100, 0), angles='0 0 0', spawnflags='16')
        t.point('scripted_action', targetname=n + '_act', at=(x, 100, 0), angles='0 90 0', m_iszEntity=n,
                m_fAction='8', m_fMoveTo='5', m_fTurnType=tt, names=NAMES)
    t.wait(1.5)
    for n in ('bi', 'bt', 'btf', 'btn', 'btm'):
        t.fire(n + '_act')
    t.wait(3.0)                  # the turn takes longer under load (seen 77 of 90 degrees at 1.5 s)
    for n in ('bi', 'bt', 'bi_mark', 'bt_mark', 'btf', 'btn', 'btm'):
        t.snap('s', n, 'origin', 'angles')
    t.expect.distance('s', 'bi', 'bi_mark', 0, 20)
    t.expect.distance('s', 'bt', 'bt_mark', 0, 20)
    t.expect.value('s', 'btf', 'angles.y', 90, tol=12)
    t.expect.value('s', 'btn', 'angles.y', 0, tol=12)
    t.expect.value('s', 'btm', 'angles.y', 90, tol=12)


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


@S.test('scripted_action Move to 1 (Walk) and 2 (Run): the barney goes to the script (Run faster); Fire on '
        'Possession, Fire after moving, Fire on Animation start in that order; Move to target\'s radius 150 stops '
        'him short', players=1, doc=DOC,
        covers=('scripted_action.m_fMoveTo=1', 'scripted_action.m_fMoveTo=2', 'scripted_action.m_iszFireOnPossessed',
                'scripted_action.m_iszFireOnBegin', 'scripted_action.m_iszFireOnAnimStart',
                'scripted_action.moveto_radius'))
def move_walk_run(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, x, mt, kv in (('bw', -150, '1', {}), ('br', 0, '2', {}), ('bm', 150, '1', {'moveto_radius': '150'})):
        t.point('monster_barney', targetname=n, at=(x, -200, 0), angles='0 90 0', spawnflags='16')
        t.point('scripted_action', targetname=n + '_act', at=(x, 200, 0), angles='0 90 0', m_iszEntity=n,
                m_fAction='6', m_fMoveTo=mt, m_iszFireOnPossessed=n + '_poss', m_iszFireOnBegin=n + '_begin',
                m_iszFireOnAnimStart=n + '_anim', names=NAMES, **kv)
        info(t, n + '_mark', at=(x, 200, 0))
        for e in ('poss', 'begin', 'anim'):
            info(t, '%s_%s' % (n, e))
    t.wait(1.5)
    t.mark('go')
    for n in ('bw', 'br', 'bm'):
        t.fire(n + '_act')
    t.wait(8.0)
    for n in ('bw', 'br', 'bm', 'bw_mark', 'br_mark', 'bm_mark'):
        t.snap('s', n, 'origin')
    t.expect.distance('s', 'bw', 'bw_mark', 0, 20)
    t.expect.distance('s', 'br', 'br_mark', 0, 20)
    t.expect.distance('s', 'bm', 'bm_mark', 110, 165)
    for n in ('bw', 'br'):
        t.expect.fired(n + '_poss', count=1, after='go')
        t.expect.fired(n + '_begin', count=1, after='go')
        t.expect.fired(n + '_anim', count=1, after='go')
        t.expect.order(n + '_poss', n + '_begin')
        t.expect.order(n + '_begin', n + '_anim')


@S.test('scripted_action Repeatable (4): a second trigger after the first action plays it again (target fired '
        'twice); without it the script is used once', players=1, doc=DOC, covers=('scripted_action:4',))
def repeatable(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, fl in (('rep', -100, '4'), ('one', 100, '0')):
        t.point('monster_barney', targetname=n, at=(x, 0, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_action', targetname=n + '_act', at=(x, 0, 0), m_iszEntity=n, m_fAction='6',
                m_fMoveTo='0', target=n + '_done', spawnflags=fl, names=NAMES)
        info(t, n + '_done')
    t.wait(1.0)
    t.fire('rep_act')
    t.fire('one_act')
    t.wait(3.0)
    t.fire('rep_act')
    t.fire('one_act')
    t.wait(3.0)
    t.expect.used('rep_done', count=2)
    t.expect.used('one_done', count=1)
