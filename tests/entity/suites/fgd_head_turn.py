"""Tier 5, final pass: what turns a talking barney's head (bone controller 0, snapshot field controller.x; 127 is
straight ahead: the barney's head turns -60 .. 60): scripted_sentence "listener" and "listener_radius" (the speaker
looks at its listener while it talks, CTalkMonster::RunTask IdleHeadTurn), and the scripts' flag 8192 "Don't reset
head turn" (once the talk is over a held barney keeps the turn; without it the head goes back). The player stands to
the barney's right and is notarget."""

from etest import Suite

SUITE = Suite('fgd_head_turn', 'Tier 5: scripted_sentence listener, scripts\' Don\'t reset head turn', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
AHEAD = 127


def sentence(t, name, speaker, **kv):
    kv.setdefault('duration', '2')
    t.point('scripted_sentence', targetname=name, at=(0, 0, 0), sentence='!BA_OK0', entity=speaker, radius='512',
            refire='1', spawnflags='4', names=('entity',), **kv)


def side(t):
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (-120, 60, 0), yaw=0)       # the barney at (0, 60) faces south: the player on his right


@S.test('scripted_sentence "listener" player: the barney looks at the player while he says it (head turned); without '
        'a listener he looks ahead', players=1, doc=DOC, covers=('scripted_sentence.listener',))
def listener(t):
    side(t)
    t.point('monster_barney', targetname='b', at=(0, 60, 0), angles='0 270 0', spawnflags='16')
    sentence(t, 'with', 'b', listener='player')
    sentence(t, 'none', 'b')
    t.wait(1.0)
    t.fire('with')
    t.wait(1.0)
    t.snap('with', 'b', 'controller')
    t.wait(5.0)                 # the talk is over (and the talk wait)
    t.fire('none')
    t.wait(1.0)
    t.snap('none', 'b', 'controller')
    t.wait(1.5)
    t.expect.value('with', 'b', 'controller.x', AHEAD, op='!=')
    t.expect.value('none', 'b', 'controller.x', AHEAD)


@S.test('scripted_sentence "listener_radius" 100: a scientist listener 230 away is not found (the barney looks '
        'ahead); without it the Search Radius (512) finds him (head turned)', players=1, doc=DOC,
        covers=('scripted_sentence.listener_radius', 'scripted_sentence.listener'))
def listener_radius(t):
    side(t)
    t.point('monster_barney', targetname='b', at=(0, 60, 0), angles='0 270 0', spawnflags='16')
    t.point('monster_scientist', targetname='sci', at=(-230, 60, 0), angles='0 0 0', spawnflags='16')
    sentence(t, 'near', 'b', listener='monster_scientist', listener_radius='100')
    sentence(t, 'wide', 'b', listener='monster_scientist')
    t.wait(1.0)
    t.fire('near')
    t.wait(1.0)
    t.snap('near', 'b', 'controller')
    t.wait(5.0)
    t.fire('wide')
    t.wait(1.0)
    t.snap('wide', 'b', 'controller')
    t.wait(1.5)
    t.expect.value('near', 'b', 'controller.x', AHEAD)
    t.expect.value('wide', 'b', 'controller.x', AHEAD, op='!=')


def head_case(cls, keep):
    @S.test('%s %s: the barney it holds talks to the player (head turned); %s' % (
                cls, 'flag 8192 "Don\'t reset head turn"' if keep else 'without flag 8192',
                'once the talk is over he keeps the turn' if keep else 'once the talk is over his head goes back'),
            players=1, doc=DOC + '#scripted-scenes-batch-4', covers=('%s:8192' % cls,))
    def case(t):
        side(t)
        t.point('monster_barney', targetname='b', at=(0, 60, 0), angles='0 270 0', spawnflags='16')
        t.point(cls, targetname='ss', at=(0, 60, 0), angles='0 270 0', m_iszEntity='b', m_iszPlay='barn_wave',
                m_iszIdle='idle1', m_fMoveTo='0', spawnflags='8192' if keep else '0', names=('m_iszEntity',))
        sentence(t, 'talk', 'b', listener='player')
        t.wait(1.5)
        t.snap('held', 'b', 'cine')
        t.fire('talk')
        t.wait(1.0)
        t.snap('talk', 'b', 'controller')
        t.wait(3.0)                 # the 2 s sentence is over
        t.snap('after', 'b', 'controller', 'cine')
        t.expect.value('held', 'b', 'cine', 1)
        t.expect.value('talk', 'b', 'controller.x', AHEAD, op='!=')
        t.expect.value('after', 'b', 'cine', 1)
        if keep:
            t.expect.value('after', 'b', 'controller.x', AHEAD, op='!=')
        else:
            t.expect.value('after', 'b', 'controller.x', AHEAD)
    case.__name__ = '%s_head_%s' % ('ai' if cls.startswith('ai') else 'ss', 'keep' if keep else 'reset')
    return case


for _c in ('scripted_sequence', 'aiscripted_sequence'):
    head_case(_c, True)
    head_case(_c, False)
