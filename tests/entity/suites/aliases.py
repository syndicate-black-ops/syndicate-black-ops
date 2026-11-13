"""Aliases (docs/ALIASES.md; branch feat/spirit-aliases): info_alias, multi_alias, trigger_changealias, info_group and
the "*alias" / "group.member" references in Target, Kill target and Master. What each test checks is the FGD's text
(sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('aliases', 'Aliases: info_alias, multi_alias, trigger_changealias, info_group', players=1,
              requires=('Alias_FindEdict',))
S = SUITE
SUITE.server_args = ('+developer', '1')
DOC = 'ALIASES.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3


def info(t, *names):
    for n in names:
        t.point('info_target', targetname=n)


def relay(t, name, target, **kv):
    return t.point('trigger_relay', targetname=name, target=target, triggerstate='2', **kv)


@S.test('info_alias On/Off mode: "*al" is "Reference while On"; triggered, the alias turns off and "*al" is '
        '"Reference while Off"', players=1, doc=DOC,
        covers=('info_alias', 'info_alias.target', 'info_alias.netname', 'info_alias.mode', 'info_alias.mode=0'))
def alias_onoff(t):
    t.point('info_alias', targetname='al', target='d1', netname='d2', mode='0')
    info(t, 'd1', 'd2')
    relay(t, 'r', '*al')
    t.mark('a')
    t.fire('r')
    t.wait(0.1)
    t.fire('al')
    t.wait(0.1)
    t.mark('b')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('d1', caller='r', count=1, after='a')
    t.expect.used('d2', caller='r', count=1, after='b')
    t.expect.not_used('d1', after='b')


@S.test('info_alias "Start Off" (1): it starts as "Reference while Off"', players=1, doc=DOC,
        covers=('info_alias:1',))
def alias_start_off(t):
    t.point('info_alias', targetname='al', target='d1', netname='d2', spawnflags='1')
    info(t, 'd1', 'd2')
    relay(t, 'r', '*al')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('d2', count=1)
    t.expect.not_used('d1')


@S.test('"*alias" as a Kill target removes the entity the alias points to', players=1, doc=DOC,
        covers=('info_alias',))
def alias_killtarget(t):
    t.point('info_alias', targetname='al', target='d1')
    info(t, 'd1', 'd2')
    t.point('trigger_relay', targetname='r', killtarget='*al', triggerstate='2')
    t.mark('go')
    t.fire('r')
    t.wait(0.2)
    t.expect.removed('d1', after='go')
    t.expect.not_removed('d2')


@S.test('"*alias" as a Master: the door follows the master the alias points to; trigger_changealias repoints it',
        players=1, doc=DOC, covers=('trigger_changealias', 'trigger_changealias.target', 'trigger_changealias.netname'))
def alias_master(t):
    t.point('env_state', targetname='locked')
    t.point('env_state', targetname='open', spawnflags='1')
    t.point('info_alias', targetname='al', target='locked')
    t.point('trigger_changealias', targetname='chg', target='al', netname='open')
    t.brush('func_door', size=(64, 16, 96), at=(0, 64, 0), tex='666MURMETAL', targetname='door', angles='-90 0 0',
            speed='400', wait='-1', lip='8', master='*al')
    t.wait(0.2)
    t.snap('a', 'door', 'origin')
    t.fire('door')
    t.wait(0.5)
    t.snap('b', 'door', 'origin')
    t.expect.same('a', 'b', 'door', 'origin')       # locked: the alias points to the env_state that is off
    t.fire('chg')
    t.fire('door')
    t.wait(0.5)
    t.snap('c', 'door', 'origin')
    t.expect.value('c', 'door', 'origin.z', 20, op='>')


@S.test('info_alias List mode: starts at place 1; On or Toggle steps to the next name, Off to the one before, and it '
        'wraps around', players=1, doc=DOC, covers=('info_alias.mode=1',))
def alias_list(t):
    t.point('info_alias', targetname='al', mode='1', a='1', b='2', c='3')
    info(t, 'a', 'b', 'c')
    relay(t, 'r', '*al')
    steps = (('s1', None, 'a'), ('s2', 'on', 'b'), ('s3', 'on', 'c'), ('s4', 'on', 'a'), ('s5', 'off', 'c'))
    for mark, use, expect in steps:
        if use:
            t.fire('al', use=use)
            t.wait(0.1)
        t.mark(mark)
        t.fire('r')
        t.wait(0.1)
    for i, (mark, use, expect) in enumerate(steps):
        t.expect.used(expect, caller='r', count=1, after=mark, within=(0, 0.08))


@S.test('trigger_changealias inside a firing: the change takes effect when the firing is done, so a target of the '
        'same firing still finds the old entity', players=1, doc=DOC, covers=('trigger_changealias',))
def changealias_deferred(t):
    t.point('info_alias', targetname='al', target='d1')
    info(t, 'd1', 'd2')
    t.point('trigger_changealias', targetname='both', target='al', netname='d2')   # spawned first: fired first
    relay(t, 'both', '*al')
    relay(t, 'r', '*al')
    t.mark('a')
    t.fire('both')
    t.wait(0.1)
    t.mark('b')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('d1', count=1, after='a', within=(0, 0.08))
    t.expect.used('d2', count=1, after='b')


@S.test('trigger_changealias "String to Set" !activator: the alias points to the activator itself (a player, who '
        'has no name)', players=1, doc=DOC, covers=('trigger_changealias.netname',))
def changealias_activator(t):
    t.point('info_alias', targetname='al', target='d1')
    info(t, 'd1')
    t.point('trigger_changealias', targetname='chg', target='al', netname='!activator')
    relay(t, 'r', '*al')
    t.fire('chg', activator='player1')
    t.wait(0.1)
    t.mark('go')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('player1', caller='r', count=1, after='go')
    t.expect.not_used('d1', after='go')


def resolve_case(flag):
    @S.test('trigger_changealias "Resolve references" %s: alias2 set to "*al" %s' % (
        ('on' if flag else 'off'), 'keeps the entity al pointed to then' if flag else 'follows al when al changes later'),
        players=1, doc=DOC, covers=('trigger_changealias:1',) if flag else ('trigger_changealias',))
    def case(t):
        t.point('info_alias', targetname='al', target='d1', netname='d2')
        t.point('info_alias', targetname='al2', target='nothing')
        info(t, 'd1', 'd2')
        t.point('trigger_changealias', targetname='chg', target='al2', netname='*al', spawnflags='1' if flag else '0')
        relay(t, 'r', '*al2')
        t.fire('chg')
        t.wait(0.1)
        t.fire('al')            # al: now d2
        t.wait(0.1)
        t.mark('go')
        t.fire('r')
        t.wait(0.1)
        t.expect.used('d1' if flag else 'd2', count=1, after='go')
        t.expect.not_used('d2' if flag else 'd1', after='go')
    case.__name__ = 'changealias_resolve%d' % flag
    return case


resolve_case(1)
resolve_case(0)


def multi_case(mode):
    label = {0: 'Normal: both names', 1: 'Choose one (weighted), weights 1 and 0: always the first',
             2: '% chance for each, 100 and 0: the first only'}[mode]

    @S.test('multi_alias Mode %d %s' % (mode, label), players=1, doc=DOC,
            covers=('multi_alias', 'multi_alias.m_iMode', 'multi_alias.m_iMode=%d' % mode))
    def case(t):
        kv = {'d1': '1', 'd2': '1'} if mode == 0 else ({'d1': '1', 'd2': '0'} if mode == 1 else {'d1': '100', 'd2': '0'})
        t.point('multi_alias', targetname='ma', m_iMode=str(mode), **kv)
        info(t, 'd1', 'd2')
        relay(t, 'r', '*ma')
        for _ in range(3):
            t.fire('r')
            t.wait(0.1)
        t.expect.used('d1', caller='r', count=3)
        t.expect.used('d2', caller='r', count=3 if mode == 0 else 0)
    case.__name__ = 'multi_alias_mode%d' % mode
    return case


for _m in (0, 1, 2):
    multi_case(_m)


@S.test('info_group: "grp.door" is the member door; "Default member prefix" makes "grp.other" the entity '
        'pre_other; triggered, the group points its alias to itself, so "*al.light" is its member light',
        players=1, doc=DOC, covers=('info_group', 'info_group.target', 'info_group.defaultmember'))
def group(t):
    t.point('info_group', targetname='grp', door='d1', light='d2', defaultmember='pre_', target='al')
    t.point('info_alias', targetname='al', target='nothing')
    info(t, 'd1', 'd2', 'pre_other')
    relay(t, 'r1', 'grp.door')
    relay(t, 'r2', 'grp.other')
    relay(t, 'r3', '*al.light')
    t.fire('r1')
    t.fire('r2')
    t.wait(0.1)
    t.fire('grp')
    t.wait(0.1)
    t.mark('go')
    t.fire('r3')
    t.wait(0.1)
    t.expect.used('d1', caller='r1', count=1)
    t.expect.used('pre_other', caller='r2', count=1)
    t.expect.used('d2', caller='r3', count=1, after='go')


@S.test('An alias that points to a wildcard ("d_*") finds every entity whose name starts with "d_"', players=1,
        doc=DOC, covers=('info_alias',))
def alias_wildcard(t):
    t.point('info_alias', targetname='al', target='d_*')
    info(t, 'd_a', 'd_b', 'x')
    relay(t, 'r', '*al')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('d_a', count=1)
    t.expect.used('d_b', count=1)
    t.expect.not_used('x')


@S.test('The alias keeps its current value in the save data (repointed: the new name)', players=1, doc=DOC,
        covers=('info_alias',))
def alias_save(t):
    t.point('info_alias', targetname='al', target='d1')
    t.point('trigger_changealias', targetname='chg', target='al', netname='d2')
    info(t, 'd1', 'd2')
    t.fire('chg')
    t.wait(0.1)
    t.snap('s', 'al', 'save')
    t.expect.value('s', 'al', 'save.m_iszValue', '"%s"' % t.name('d2'))


@S.test('A missing alias: "*nope" finds nothing, trigger_changealias with no alias of that name says so', players=1,
        doc=DOC, covers=('trigger_changealias',))
def alias_missing(t):
    info(t, 'nope')
    relay(t, 'r', '*nope')
    t.point('trigger_changealias', targetname='chg', target='ghost', netname='x')
    t.fire('r')
    t.fire('chg')
    t.wait(0.1)
    t.expect.not_used('nope')
    t.expect.console('no info_alias named "%s"' % t.name('ghost'))


@S.test('An alias that points to itself: the search stops with a console note, no crash', players=1, doc=DOC,
        covers=('info_alias',))
def alias_loop(t):
    t.point('info_alias', targetname='al', target='*al')
    relay(t, 'r', '*al')
    t.fire('r')
    t.wait(0.1)
    t.expect.console('point to each other in a loop')


@S.test('Debug Mode (2) of info_alias, trigger_changealias and info_group: console notes for each change',
        players=1, doc=DOC, covers=('info_alias:2', 'trigger_changealias:2', 'info_group:2'))
def alias_debug(t):
    t.point('info_alias', targetname='al', target='d1', netname='d2', spawnflags='2')
    t.point('trigger_changealias', targetname='chg', target='al', netname='d1', spawnflags='2')
    t.point('info_group', targetname='grp', door='d1', target='al', spawnflags='2')
    info(t, 'd1', 'd2')
    t.fire('al')
    t.fire('chg')
    t.fire('grp')
    t.wait(0.1)
    a = t.name('al')
    t.expect.console('DEBUG: info_alias "%s" now points to "%s"' % (a, t.name('d2')))
    t.expect.console('DEBUG: trigger_changealias "%s" points info_alias "%s"' % (t.name('chg'), a))
    t.expect.console('DEBUG: info_group "%s" points info_alias "%s" to itself' % (t.name('grp'), a))


# ------------------------------------------------------------------------------------------------ what * means
@S.test('"*name*" (a * at both ends) is refused: nothing found, a console line; an alias pointing to "name*" is the '
        'way to use a wildcard', players=1, doc=DOC, covers=('info_alias',))
def star_both_ends(t):
    t.point('info_alias', targetname='d_*', target='d_a')    # even an alias with that name is not reached
    info(t, 'd_a')
    relay(t, 'r', '*d_*')
    t.fire('r')
    t.wait(0.1)
    t.expect.not_used('d_a')
    t.expect.console('"*%s": a * at both ends is not allowed' % t.name('d_*'))


@S.test('An info_alias named "locus" or "player", or with a name starting with *, is refused with a console line and '
        'removed; *locus keeps its built-in meaning', players=1, doc=DOC, covers=('info_alias',))
def star_refused_names(t):
    t.point('info_alias', targetname='=locus', target='d1')
    t.point('info_alias', targetname='=player', target='d1')
    t.point('info_alias', targetname='=*starred', target='d1')
    info(t, 'd1')
    t.wait(0.1)
    t.expect.console('info_alias "locus": refused (*locus and *player are built-in names)')
    t.expect.console('info_alias "player": refused (*locus and *player are built-in names)')
    t.expect.console('info_alias "*starred": refused (a name starts without the *')
    t.expect.not_used('d1')


@S.test('info_alias List mode after a trigger_changealias pointed it at a list entry: the next step continues from '
        'that entry', players=1, doc=DOC, covers=('info_alias.mode=1', 'trigger_changealias'))
def alias_list_after_change(t):
    t.point('info_alias', targetname='al', mode='1', a='1', b='2', c='3')
    t.point('trigger_changealias', targetname='chg', target='al', netname='b')
    info(t, 'a', 'b', 'c')
    relay(t, 'r', '*al')
    t.fire('chg')
    t.wait(0.1)
    t.fire('al')            # from b: c
    t.wait(0.1)
    t.mark('go')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('c', caller='r', count=1, after='go')
    t.expect.not_used('b', after='go')
