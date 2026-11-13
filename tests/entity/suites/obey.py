"""Obey Trigger Mode (m_iObeyTriggerMode, docs/LOGIC_ENTITIES.md "Obey Trigger Mode") on the movers the suite mapper
does not check: func_door_rotating and func_water (1: a still door ignores Off when closed and On when open; 2: On
reopens a closing door), func_rotating (1: Off stops / On spins, also while spinning), func_train (1: On starts, Off
stops, On while running keeps it running)."""

from etest import Suite

SUITE = Suite('obey', 'Obey Trigger Mode on more movers')
S = SUITE
DOC = 'LOGIC_ENTITIES.md#obey-trigger-mode'


def rot_door(t, name, **kv):
    """turns 90 degrees about Z at 360 deg/s: open in 0.25 s"""
    kv.setdefault('speed', '360')
    return t.brush('func_door_rotating', targetname=name, size=(64, 8, 96), at=(0, 100, 0), tex='666MURMETAL',
                   origin=True, distance='90', wait='-1', **kv)


@S.test('func_door_rotating mode 1 (a Toggle door, flag 32): closed, Off does nothing; On opens; open, On does '
        'nothing; Off closes; mode 0 (control): Off opens a closed door', doc=DOC,
        covers=('func_door_rotating.m_iObeyTriggerMode=1', 'func_door_rotating.m_iObeyTriggerMode=0'))
def rot1(t):
    rot_door(t, 'd', m_iObeyTriggerMode='1', spawnflags='32')
    t.brush('func_door_rotating', targetname='c', size=(64, 8, 96), at=(0, -100, 0), tex='666MURMETAL', origin=True,
            speed='360', distance='90', wait='-1')
    t.fire('d', use='off')
    t.fire('c', use='off')
    t.wait(0.5)
    t.snap('a', 'd', 'angles')
    t.snap('a', 'c', 'angles')
    t.fire('d', use='on')
    t.wait(0.5)
    t.snap('b', 'd', 'angles')
    t.fire('d', use='on')
    t.wait(0.5)
    t.snap('c', 'd', 'angles')
    t.fire('d', use='off')
    t.wait(0.5)
    t.snap('e', 'd', 'angles')
    t.expect.value('a', 'd', 'angles.y', 0, tol=0.5)
    t.expect.value('a', 'c', 'angles.y', 90, tol=0.5)
    t.expect.value('b', 'd', 'angles.y', 90, tol=0.5)
    t.expect.value('c', 'd', 'angles.y', 90, tol=0.5)
    t.expect.value('e', 'd', 'angles.y', 0, tol=0.5)


@S.test('func_door_rotating mode 2: On while it is closing opens it again', doc=DOC,
        covers=('func_door_rotating.m_iObeyTriggerMode=2',))
def rot2(t):
    rot_door(t, 'd', m_iObeyTriggerMode='2', speed='90')       # 1 s per way
    t.fire('d', use='on')
    t.wait(1.3)
    t.fire('d', use='off')
    t.wait(0.4)
    t.fire('d', use='on')
    t.wait(1.0)
    t.snap('s', 'd', 'angles')
    t.expect.value('s', 'd', 'angles.y', 90, tol=0.5)


@S.test('func_water mode 1: closed, Off does nothing; On opens', doc=DOC,
        covers=('func_water.m_iObeyTriggerMode=1',))
def water1(t):
    t.brush('func_water', targetname='w', skin='-3', size=(96, 96, 96), at=(0, 100, 0), angles='-90 0 0',
            speed='400', lip='8', wait='-1', m_iObeyTriggerMode='1')
    t.fire('w', use='off')
    t.wait(0.5)
    t.snap('a', 'w', 'origin')
    t.fire('w', use='on')
    t.wait(0.5)
    t.snap('b', 'w', 'origin')
    t.expect.value('a', 'w', 'origin', [0, 0, 0], tol=0.5)
    t.expect.value('b', 'w', 'origin', [0, 0, 88], tol=0.5)


@S.test('func_rotating mode 1: Off when stopped keeps it stopped; On spins it up; On again keeps spinning; Off stops '
        'it', doc=DOC, covers=('func_rotating', 'func_rotating.m_iObeyTriggerMode=1'))
def rotating1(t):
    t.brush('func_rotating', targetname='r', size=(64, 64, 16), at=(0, 100, 0), origin=True, speed='100',
            m_iObeyTriggerMode='1')
    t.fire('r', use='off')
    t.wait(0.5)
    t.snap('a', 'r', 'avelocity')
    t.fire('r', use='on')
    t.wait(1.5)
    t.fire('r', use='on')
    t.wait(0.5)
    t.snap('b', 'r', 'avelocity')
    t.fire('r', use='off')
    t.wait(3.0)
    t.snap('c', 'r', 'avelocity')
    t.expect.value('a', 'r', 'avelocity.y', 0, tol=0.5)
    t.expect.value('b', 'r', 'avelocity.y', 100, tol=1)
    t.expect.value('c', 'r', 'avelocity.y', 0, tol=0.5)


@S.test('func_train mode 1: Off when stopped keeps it still; On starts it; On while moving keeps it moving; Off stops '
        'it', doc=DOC, covers=('func_train', 'func_train.m_iObeyTriggerMode=1'))
def train1(t):
    t.point('path_corner', targetname='p1', at=(-150, 100, 40), target='p2')
    t.point('path_corner', targetname='p2', at=(150, 100, 40), target='p1')
    t.brush('func_train', targetname='tr', size=(32, 32, 32), at=(-150, 100, 40), target='p1', speed='100',
            m_iObeyTriggerMode='1')
    t.wait(0.3)
    t.fire('tr', use='off')
    t.wait(0.5)
    t.snap('a', 'tr', 'velocity')
    t.fire('tr', use='on')
    t.wait(0.5)
    t.fire('tr', use='on')
    t.wait(0.3)
    t.snap('b', 'tr', 'velocity')
    t.fire('tr', use='off')
    t.wait(0.3)
    t.snap('c', 'tr', 'velocity')
    t.expect.value('a', 'tr', 'velocity', [0, 0, 0], tol=0.5)
    t.expect.value('b', 'tr', 'velocity.x', 100, tol=1)
    t.expect.value('c', 'tr', 'velocity', [0, 0, 0], tol=0.5)
