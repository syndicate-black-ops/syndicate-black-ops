"""Half-Life Unified SDK ports (fix/hlu-batch-2): trigger_teleport without a target, classname can't be changed,
the Osprey's classification goes to its Feds."""

from etest import Suite

SUITE = Suite('hlu_fixes_2', 'Half-Life Unified SDK ports: empty teleport target, classname kept, Osprey Feds class',
              players=1)
SUITE.server_args = ('+developer', '1')    # the classname refusal is a developer message
S = SUITE
R_AL, R_HT = -2, 2


@S.test('trigger_teleport without a target does nothing: a monster and the player in it stay where they are (the '
        'empty target matched an entity whose name is an empty string, and they were sent there)',
        players=1, covers=('trigger_teleport', 'trigger_teleport.target', 'trigger_teleport:1'))
def teleport_no_target(t):
    t.flags('player1', '+FL_NOTARGET')
    t.trigger('trigger_teleport', targetname='tp', size=(64, 64, 96), at=(0, -150, 0), spawnflags='1')
    t.trigger('trigger_teleport', targetname='tp2', size=(64, 64, 96), at=(-150, -150, 0))
    t.point('info_target', targetname='start', at=(0, -150, 0))
    t.point('info_target', targetname='start2', at=(-150, -150, 0))
    t.point('monster_zombie', targetname='z', at=(150, 150, 0), spawnflags='16')
    # an entity named "" (the compiler drops empty keys; trigger_changevalue without a New value writes ""): what an
    # empty target matched
    t.point('info_target', targetname='blank', at=(150, -150, 0))
    t.point('trigger_changevalue', targetname='unname', target='blank', m_iszValueName='targetname', m_iszValueType='0')
    t.teleport('player1', (-150, 150, 0))
    t.wait(0.3)
    t.fire('unname')
    t.wait(0.2)
    t.teleport('z', (0, -150, 0))
    t.teleport('player1', (-150, -150, 0))
    t.wait(0.5)
    t.snap('in', 'z', 'origin')
    t.snap('in', 'start', 'origin')
    t.snap('in', 'player1', 'origin')
    t.snap('in', 'start2', 'origin')
    t.expect.distance('in', 'z', 'start', 0, 20)
    t.expect.distance('in', 'player1', 'start2', 0, 20)


@S.test('trigger_changevalue / trigger_copyvalue on "classname" leave the class as it is (a console note); the same '
        'entities still change another key',
        covers=('trigger_changevalue', 'trigger_changevalue.m_iszValueName', 'trigger_copyvalue',
                'trigger_copyvalue.m_iszDstValueName'))
def classname_kept(t):
    t.point('info_target', targetname='tgt', message='old')
    t.point('info_target', targetname='src', message='info_null')
    t.point('trigger_changevalue', targetname='cv_class', target='tgt', m_iszValueName='classname',
            m_iszNewValue='info_null', m_iszValueType='0')
    t.point('trigger_copyvalue', targetname='cp_class', target='src', m_iszSrcValueName='message',
            m_iszDstValueName='classname', m_iszValueType='0')
    t.point('trigger_changevalue', targetname='cv_msg', target='tgt', m_iszValueName='message', m_iszNewValue='new',
            m_iszValueType='0')
    t.wait(0.2)
    t.fire('cv_class')
    t.wait(0.1)
    t.snap('changed', 'tgt', 'kv:classname')
    t.fire('cp_class')
    t.wait(0.1)
    t.snap('copied', 'src', 'kv:classname')
    t.fire('cv_msg')
    t.wait(0.1)
    t.snap('msg', 'tgt', 'kv:message')
    t.expect.value('changed', 'tgt', 'kv:classname', 'info_target')
    t.expect.value('copied', 'src', 'kv:classname', 'info_target')
    t.expect.value('msg', 'tgt', 'kv:message', 'new')
    t.expect.console('classname "info_null" ignored')
    t.expect.absent('@info_null')


def osprey_drop(t, **osprey_keys):
    """Four Feds die, the Osprey drops two; each dropped Fed is named "dropped" (trigger_on_deploy_grunt fires a
    trigger_changevalue on !activator) and snapshot "drop" holds its relationship to player1."""
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (220, -220, 0))
    for k in range(4):
        t.point('monster_human_grunt', targetname='og%d' % k, at=(-180 + 120 * k, -150, 0), angles='0 90 0',
                spawnflags='16')
    t.point('path_corner', targetname='op1', at=(0, 150, 600), target='op2', speed='300')
    t.point('path_corner', targetname='op2', at=(0, 0, 600), target='op3', speed='0')
    # a closed path: at the end of an open one the Osprey's position became NaN in some runs (a NaN beam)
    t.point('path_corner', targetname='op3', at=(0, 150, 600), target='op1', speed='300')
    t.point('monster_osprey', targetname='osp', at=(0, -150, 600), angles='0 90 0', target='op1', spawnflags='64',
            num='2', trigger_on_deploy_grunt='name_it', **osprey_keys)
    t.point('trigger_changevalue', targetname='name_it', target='!activator', m_iszValueName='targetname',
            m_iszNewValue=t.ref('dropped'), m_iszValueType='0')
    t.wait(0.5)
    t.fire('osp', use='on')
    t.wait(0.5)
    for k in range(4):
        t.damage('og%d' % k, 1000)
    t.mark('dead')
    t.wait(12.0)
    t.snap('drop', 'dropped', 'rel')
    t.expect.used('name_it', min=1, after='dead')


@S.test('monster_osprey without "classify" / "is_player_ally": the Feds it drops keep their own class (they hate the '
        'player)', size=(512, 512, 1024), players=1, covers=('monster_osprey', 'monster_osprey.is_player_ally=0'))
def osprey_default_feds(t):
    osprey_drop(t)
    t.expect.value('drop', 'dropped', 'rel', R_HT)


@S.test('monster_osprey "is_player_ally" 1: the Feds it drops are player allies too',
        size=(512, 512, 1024), players=1, covers=('monster_osprey.is_player_ally', 'monster_osprey.is_player_ally=1'))
def osprey_ally_feds(t):
    osprey_drop(t, is_player_ally='1')
    t.expect.value('drop', 'dropped', 'rel', R_AL)


@S.test('monster_osprey "classify" 11 (Player ally): the Feds it drops fight as player allies',
        size=(512, 512, 1024), players=1, covers=('monster_osprey.classify', 'monster_osprey.classify=11'))
def osprey_classify_feds(t):
    osprey_drop(t, classify='11')
    t.expect.value('drop', 'dropped', 'rel', R_AL)
