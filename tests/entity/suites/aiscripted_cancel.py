"""aiscripted_sequence follows its Interruption policy like scripted_sequence: a hit breaks the monster out of the
script with policy 0 (Any damage), and leaves it in the script with 1 (No Interruptions), 2 (By death only) and the
No Interruptions flag (ScriptEntityCancel, scripted.cpp). A prisoner barney is held by the script in his idle."""

from etest import Suite

SUITE = Suite('aiscripted_cancel', 'aiscripted_sequence Interruption policy', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'
NAMES = ('m_iszEntity',)


def held(t, name, at, **script_kv):
    t.point('monster_barney', targetname=name, at=at, angles='0 270 0', spawnflags='16')
    kv = dict(m_iszEntity=name, m_iszPlay='reload', m_fMoveTo='0', m_flRadius='512', m_iszIdle='idle1')
    kv.update(script_kv)
    t.point('aiscripted_sequence', targetname=name + '_act', at=at, angles='0 270 0', names=NAMES, **kv)


@S.test('aiscripted_sequence: a hit breaks the barney out with Interruption 0; 1, 2 and the No Interruptions flag '
        'keep him in the script', players=1, doc=DOC, covers=('aiscripted_sequence:32',))
def interruption(t):
    t.flags('player1', '+FL_NOTARGET')
    held(t, 'bdf', (-225, 0, 0), m_interruptionPolicy='0')
    held(t, 'bni', (-75, 0, 0), m_interruptionPolicy='1')
    held(t, 'bdo', (75, 0, 0), m_interruptionPolicy='2')
    held(t, 'bfl', (225, 0, 0), spawnflags='32')
    t.wait(2.0)
    for n in ('bdf', 'bni', 'bdo', 'bfl'):
        t.damage(n, 10, attacker='player1')
    t.wait(1.0)
    for n in ('bdf', 'bni', 'bdo', 'bfl'):
        t.snap('s', n, 'cine')
    t.expect.value('s', 'bdf', 'cine', 0)
    t.expect.value('s', 'bni', 'cine', 1)
    t.expect.value('s', 'bdo', 'cine', 1)
    t.expect.value('s', 'bfl', 'cine', 1)
