"""trigger_objective (docs/OBJECTIVES.md): groups (partof), the states and how a trigger moves them on, the counter, the target, the
master, the flags Silent, the start states, order and the activation sequence in the Objective message, the long text,
and what a client that (re)initializes its HUD gets (fullupdate: a clear, then every shown objective). The round
reset (flag 2) is in objectives_round, the 32 per map limit in objectives_limit. Every change goes to all clients
(MSG_ALL): the expectations match the objective by its text."""

from etest import Suite

SUITE = Suite('objectives', 'trigger_objective: states, counter, target, master, flags, the Objective message',
              players=1, requires=('Objective_SendQueued',))
S = SUITE
DOC = 'OBJECTIVES.md#trigger_objective-point-entity'
HIDDEN, ACTIVE, COMPLETED = 0, 1, 2


def obj(t, name, text, **kv):
    t.point('trigger_objective', targetname=name, message=text, **kv)


@S.test('Hidden (default): the first trigger makes it Active (new, the notice), the second completes it (done) and '
        'fires its target with the activator; a third does nothing', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective.message', 'trigger_objective.state=0',
                'trigger_objective.target'))
def hidden_active_done(t):
    obj(t, 'o', 'T01 find the keycard', target='done')
    t.point('info_target', targetname='done')
    t.wait(0.3)
    t.mark('show')
    t.fire('o', activator='player1')
    t.wait(0.2)
    t.mark('complete')
    t.fire('o', activator='player1')
    t.wait(0.2)
    t.mark('again')
    t.fire('o', activator='player1')
    t.wait(0.2)
    t.expect.msg('Objective', count=1, after='show', within=(0, 0.15),
                 fields={'text': 'T01 find the keycard', 'state': ACTIVE, 'new': True, 'done': False,
                         'silent': False, 'target': 0})
    t.expect.msg('Objective', count=1, after='complete', within=(0, 0.15),
                 fields={'text': 'T01 find the keycard', 'state': COMPLETED, 'done': True, 'new': False})
    t.expect.msg('Objective', count=0, after='again', fields={'text': 'T01 find the keycard'})
    t.expect.used('done', activator='player1', caller='o', count=1, after='complete', within=(0, 0.15))


@S.test('count 3: every trigger counts: the first shows it at 1/3 (new), then 2/3, at 3/3 it is completed and fires '
        'its target once; a fourth does nothing', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective.count', 'trigger_objective.target'))
def counter(t):
    obj(t, 'o', 'T02 restart the pumps', count='3', target='done')
    t.point('info_target', targetname='done')
    t.wait(0.3)
    for i in range(4):
        t.mark('f%d' % i)
        t.fire('o', activator='player1')
        t.wait(0.2)
    text = 'T02 restart the pumps'
    t.expect.msg('Objective', count=1, after='f0', within=(0, 0.15),
                 fields={'text': text, 'state': ACTIVE, 'new': True, 'counted': True, 'count': 1, 'target': 3})
    t.expect.msg('Objective', count=1, after='f1', within=(0, 0.15),
                 fields={'text': text, 'state': ACTIVE, 'counted': True, 'new': False, 'count': 2, 'target': 3})
    t.expect.msg('Objective', count=1, after='f2', within=(0, 0.15),
                 fields={'text': text, 'state': COMPLETED, 'counted': True, 'done': True, 'count': 3, 'target': 3})
    t.expect.msg('Objective', count=0, after='f3', fields={'text': text})
    t.expect.not_used('done', after='f0', within=(0, 0.3))
    t.expect.used('done', caller='o', count=1, after='f2', within=(0, 0.15))


@S.test('master: while the multisource is locked a trigger does nothing; unlocked, the trigger shows it',
        players=1, doc=DOC, covers=('trigger_objective', 'trigger_objective.master'))
def master(t):
    t.point('multisource', targetname='lock', at=(200, -200, 0))
    t.point('trigger_relay', targetname='key', target='lock', at=(180, -200, 0), triggerstate='1')
    obj(t, 'o', 'T03 open the vault', master='lock')
    t.wait(0.3)
    t.mark('locked')
    t.fire('o', activator='player1')
    t.wait(0.2)
    t.fire('key')
    t.wait(0.1)
    t.mark('open')
    t.fire('o', activator='player1')
    t.wait(0.2)
    t.expect.msg('Objective', count=0, after='locked', within=(0, 0.2), fields={'text': 'T03 open the vault'})
    t.expect.msg('Objective', count=1, after='open', within=(0, 0.15),
                 fields={'text': 'T03 open the vault', 'state': ACTIVE, 'new': True})


@S.test('flag 1 Silent: every message carries the silent bit (no notice, no sound on the client)', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective:1'))
def silent(t):
    obj(t, 'o', 'T04 silent one', spawnflags='1')
    t.wait(0.3)
    t.mark('go')
    t.fire('o')
    t.wait(0.2)
    t.fire('o')
    t.wait(0.2)
    t.expect.msg('Objective', count=1, after='go', fields={'text': 'T04 silent one', 'state': ACTIVE, 'silent': True})
    t.expect.msg('Objective', count=1, after='go', fields={'text': 'T04 silent one', 'state': COMPLETED,
                                                           'silent': True})


@S.test('a client that gets its HUD again (fullupdate) gets a clear, then every shown objective as it is now (no new / '
        'done bits): state 1 Active and 2 Completed at map start, order, a counter part-way, a #NAME text; a Hidden one '
        'is not sent', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective.state=1', 'trigger_objective.state=2',
                'trigger_objective.state=0', 'trigger_objective.order', 'trigger_objective.count',
                'trigger_objective.message'))
def late_joiner(t):
    obj(t, 'a', 'T05 active at start', state='1', order='2')
    obj(t, 'c', 'T05 completed at start', state='2', count='4')
    obj(t, 'h', 'T05 hidden')
    obj(t, 'n', 'T05 counter', count='5')
    obj(t, 'ti', '#SBO_SpecMenu_MapBriefing', state='1')
    t.wait(0.3)
    t.fire('n')
    t.fire('n')
    t.fire('n')
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.5)
    t.expect.msg('Objective', to='player1', count=1, after='fu', within=(0, 1.2), fields={'clear': True})
    t.expect.msg('Objective', to='player1', count=1, after='fu', within=(0, 1.4),
                 fields={'text': 'T05 active at start', 'state': ACTIVE, 'order': 2, 'new': False, 'done': False})
    t.expect.msg('Objective', to='player1', count=1, after='fu', within=(0, 1.4),
                 fields={'text': 'T05 completed at start', 'state': COMPLETED, 'count': 4, 'target': 4, 'seq': 0,
                         'done': False})
    t.expect.msg('Objective', to='player1', count=1, after='fu', within=(0, 1.4),
                 fields={'text': 'T05 counter', 'state': ACTIVE, 'count': 3, 'target': 5, 'counted': False})
    t.expect.msg('Objective', to='player1', count=1, after='fu', within=(0, 1.4),
                 fields={'text': '#SBO_SpecMenu_MapBriefing', 'state': ACTIVE})
    t.expect.msg('Objective', count=0, after='fu', fields={'text': 'T05 hidden'})


@S.test('a text longer than 127 characters is cut to 127 in the message, with a console line', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective.message'))
def long_text(t):
    text = 'T06' + 'x' * 200
    obj(t, 'o', text, state='1')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.5)
    t.expect.msg('Objective', to='player1', count=1, after='fu', fields={'text': text[:127], 'state': ACTIVE})
    t.expect.console('the text is longer than 127 characters and is cut')


@S.test('a group (partof): a part shown first shows its hidden group and the other parts (one notice, the group\'s); '
        'the group counts its parts (0/3, 1/3, ...); a part with its own counter counts; the group is completed when '
        'all three are and fires its target once; triggering the group itself does nothing more', players=1, doc=DOC,
        covers=('trigger_objective', 'trigger_objective.partof', 'trigger_objective.count', 'trigger_objective.order',
                'trigger_objective.target'))
def group(t):
    obj(t, 'g', 'T07 open door A', target='door')
    obj(t, 'p1', 'T07 find the fuse', partof='g', order='1')
    obj(t, 'p2', 'T07 kill 3 Feds', partof='g', order='2', count='3')
    obj(t, 'p3', 'T07 pull the lever', partof='g', order='3')
    t.point('info_target', targetname='door')
    t.wait(0.3)
    t.mark('show')
    t.fire('p1', activator='player1')
    t.wait(0.2)
    t.mark('p1done')
    t.fire('p1', activator='player1')
    t.wait(0.2)
    t.mark('grp')
    t.fire('g')
    t.wait(0.2)
    t.mark('kills')
    for _ in range(3):
        t.fire('p2', activator='player1')
    t.wait(0.2)
    t.mark('last')
    t.fire('p3', activator='player1')
    t.wait(0.3)
    t.expect.msg('Objective', count=1, after='show', within=(0, 0.15),
                 fields={'text': 'T07 open door A', 'state': 1, 'new': True, 'count': 0, 'target': 3, 'parent': -1})
    t.expect.msg('Objective', count=1, after='show', within=(0, 0.15),
                 fields={'text': 'T07 find the fuse', 'state': 1, 'new': False})
    t.expect.msg('Objective', min=1, after='show', within=(0, 0.15), fields={'text': 'T07 find the fuse', 'order': 1})
    t.expect.msg('Objective', count=1, after='show', within=(0, 0.15),
                 fields={'text': 'T07 kill 3 Feds', 'state': 1, 'new': False, 'count': 0, 'target': 3})
    t.expect.msg('Objective', count=1, after='show', within=(0, 0.15),
                 fields={'text': 'T07 pull the lever', 'state': 1, 'new': False})
    t.expect.msg('Objective', count=1, after='p1done', within=(0, 0.15),
                 fields={'text': 'T07 open door A', 'state': 1, 'count': 1, 'target': 3, 'counted': True})
    t.expect.msg('Objective', count=0, after='grp', within=(0, 0.15), fields={'text': 'T07 open door A'})
    t.expect.msg('Objective', count=1, after='kills', within=(0, 0.15),
                 fields={'text': 'T07 kill 3 Feds', 'state': 2, 'done': True, 'count': 3, 'target': 3})
    t.expect.msg('Objective', count=1, after='kills', within=(0, 0.15),
                 fields={'text': 'T07 open door A', 'state': 1, 'count': 2, 'target': 3})
    t.expect.not_used('door', after='show', within=(0, 0.6))
    t.expect.msg('Objective', count=1, after='last', within=(0, 0.15),
                 fields={'text': 'T07 open door A', 'state': 2, 'done': True, 'count': 3, 'target': 3})
    t.expect.used('door', caller='g', activator='player1', count=1, after='last', within=(0, 0.15))


@S.test('a part names a group that is a part itself, or no objective: listed on its own (parent none), a console '
        'line', players=1, doc=DOC, covers=('trigger_objective', 'trigger_objective.partof'))
def group_bad(t):
    obj(t, 'a', 'T08 top')
    obj(t, 'b', 'T08 mid', partof='a')
    obj(t, 'c', 'T08 deep', partof='b')
    obj(t, 'd', 'T08 lost', partof='nothing')
    t.wait(0.3)
    t.mark('go')
    t.fire('c')
    t.fire('d')
    t.wait(0.2)
    t.expect.msg('Objective', count=1, after='go', fields={'text': 'T08 deep', 'parent': -1, 'new': True})
    t.expect.msg('Objective', count=1, after='go', fields={'text': 'T08 lost', 'parent': -1, 'new': True})
    t.expect.console('is a part itself (one level only)')
    t.expect.console('(partof): listed on its own')


@S.test('master fed by a func_button (Half-Life): locked, the objective ignores triggers; a button with a wait unlocks '
        'it only until the button is back (it toggles the multisource again), a button with wait -1 keeps it '
        'unlocked', players=1, doc=DOC, covers=('trigger_objective', 'trigger_objective.master'))
def master_button(t):
    t.point('multisource', targetname='lw', at=(150, -200, 0))
    t.point('multisource', targetname='lk', at=(200, -200, 0))
    t.brush('func_button', size=(16, 32, 32), at=(-100, 200, 40), targetname='bw', target='lw', wait='0.5',
            speed='2000', spawnflags='1')
    t.brush('func_button', size=(16, 32, 32), at=(100, 200, 40), targetname='bk', target='lk', wait='-1',
            speed='2000', spawnflags='1')
    obj(t, 'ow', 'T09 behind a returning button', master='lw')
    obj(t, 'ok', 'T09 behind a button that stays in', master='lk')
    t.wait(0.3)
    t.mark('locked')
    t.fire('ow', activator='player1')
    t.fire('ok', activator='player1')
    t.wait(0.2)
    t.fire('bw', activator='player1')
    t.fire('bk', activator='player1')
    t.wait(0.2)
    t.mark('pressed')
    t.fire('ow', activator='player1')
    t.wait(1.2)
    t.mark('back')
    t.fire('ow', activator='player1')
    t.fire('ok', activator='player1')
    t.wait(0.2)
    t.expect.msg('Objective', count=0, after='locked', within=(0, 0.15), fields={'text': 'T09 behind a returning button'})
    t.expect.msg('Objective', count=0, after='locked', within=(0, 0.15),
                 fields={'text': 'T09 behind a button that stays in'})
    t.expect.msg('Objective', count=1, after='pressed', within=(0, 0.15),
                 fields={'text': 'T09 behind a returning button', 'state': ACTIVE, 'new': True})
    t.expect.msg('Objective', count=0, after='back', fields={'text': 'T09 behind a returning button'})
    t.expect.msg('Objective', count=1, after='back', within=(0, 0.15),
                 fields={'text': 'T09 behind a button that stays in', 'state': ACTIVE, 'new': True})
