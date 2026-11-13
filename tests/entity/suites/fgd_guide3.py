"""Tier 5, batch 6: FGD items of scripted_tanksequence, scripted_following, scripted_schedule, trigger_camera and
scripted_action that no 1.38 map uses and no other test asserted (docs/FEATUREFUL_GUIDE_ENTITIES.md,
docs/FEATUREFUL_OPTIONS.md)."""

from etest import Suite

SUITE = Suite('fgd_guide3', 'Tier 5: tank sequences, following, schedules, camera, scripted_action targets',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'
OPTIONS = 'FEATUREFUL_OPTIONS.md'
SF_TANK_ACTIVE, SF_TANK_CANCONTROL = 1, 32


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def tank(t, name='tank', at=(0, 150, 64), flags=0, target='shot', **kv):
    t.brush('func_tank', size=(32, 32, 32), at=at, origin=True, targetname=name, target=target,
            spawnflags=str(flags), yawrate='300', yawrange='180', yawtolerance='15', pitchrate='300',
            pitchrange='60', pitchtolerance='15', firerate='5', bullet='1', barrel='16', persistence='1',
            angles='0 270 0', **kv)
    info(t, target)


# ---------------------------------------------------------------------------------------------- scripted_tanksequence
def seq(t, name, tank_name, **kv):
    keys = dict(m_iTurn='1', m_iShoot='0', m_iUntil='0', m_fDuration='0.5', m_iActive='0', m_iControllable='0')
    keys.update(kv)
    t.point('scripted_tanksequence', targetname=name, at=(0, 0, 64), angles='0 270 0', m_iszEntity=tank_name,
            **keys)


@S.test('scripted_tanksequence Tank state afterwards 2 (Inactive) / 3 (Toggle), Control afterwards 0 (No change) / '
        '2 (Not Controllable) / 3 (Toggle)', doc=DOC,
        covers=('scripted_tanksequence.m_iActive=2', 'scripted_tanksequence.m_iActive=3',
                'scripted_tanksequence.m_iControllable=0', 'scripted_tanksequence.m_iControllable=2',
                'scripted_tanksequence.m_iControllable=3'))
def tank_after(t):
    # a: active + controllable -> Inactive, Not Controllable; b: inactive, not controllable -> Toggle, Toggle;
    # c: controllable, No change
    tank(t, 'ta', at=(-150, 150, 64), flags=SF_TANK_ACTIVE | SF_TANK_CANCONTROL, target='shota')
    tank(t, 'tb', at=(0, 150, 64), flags=0, target='shotb')
    tank(t, 'tc', at=(150, 150, 64), flags=SF_TANK_CANCONTROL, target='shotc')
    seq(t, 'sa', 'ta', m_iActive='2', m_iControllable='2')
    seq(t, 'sb', 'tb', m_iActive='3', m_iControllable='3')
    seq(t, 'sc', 'tc', m_iActive='0', m_iControllable='0')
    t.wait(0.3)
    for n in ('sa', 'sb', 'sc'):
        t.fire(n)
    t.wait(1.0)
    for n in ('ta', 'tb', 'tc'):
        t.snap('s', n, 'spawnflags')
    t.expect.value('s', 'ta', 'spawnflags', SF_TANK_ACTIVE, op='!&')
    t.expect.value('s', 'ta', 'spawnflags', SF_TANK_CANCONTROL, op='!&')
    t.expect.value('s', 'tb', 'spawnflags', SF_TANK_ACTIVE | SF_TANK_CANCONTROL, op='&')
    t.expect.value('s', 'tc', 'spawnflags', SF_TANK_CANCONTROL, op='&')
    t.expect.value('s', 'tc', 'spawnflags', SF_TANK_ACTIVE, op='!&')


@S.test('scripted_tanksequence Turn to 0 (Don\'t turn) and Fire gun 0 (Don\'t fire): the tank keeps its angle and '
        'fires nothing; the timeout target still fires', doc=DOC,
        covers=('scripted_tanksequence.m_iTurn=0', 'scripted_tanksequence.m_iShoot=0'))
def tank_still(t):
    tank(t)
    seq(t, 'sq', 'tank', m_iTurn='0', m_iShoot='0', netname='timeout', names=('netname',))
    info(t, 'timeout')
    t.wait(0.3)
    t.fire('sq')
    t.wait(1.0)
    t.snap('s', 'tank', 'angles')
    t.expect.value('s', 'tank', 'angles.y', 270, tol=2)
    t.expect.not_used('shot')
    t.expect.used('timeout', count=1)


# ---------------------------------------------------------------------------------------------- scripted_following
@S.test('scripted_following Use type -1 (Make stop following) stops a following scientist (fires on stop); Remove '
        'on fire (1): the entity is gone after one use', players=1, doc=DOC,
        covers=('scripted_following.use_type=-1', 'scripted_following:1'))
def following_stop(t):
    t.point('monster_scientist', targetname='sci', at=(0, 150, 0), angles='0 270 0')
    t.point('scripted_following', targetname='go', entity='sci', use_type='1', on_start_following='started',
            spawnflags='1', names=('entity',))
    t.point('scripted_following', targetname='halt', entity='sci', use_type='-1', on_stop_following='stopped',
            names=('entity',))
    info(t, 'started')
    info(t, 'stopped')
    t.teleport('player1', (0, -100, 0))
    t.wait(1.5)
    t.fire('go', activator='player1')
    t.wait(0.5)
    t.mark('halt')
    t.fire('halt', activator='player1', use='on')
    t.wait(0.3)
    t.expect.used('started', count=1)
    t.expect.used('stopped', activator='player1', count=1, after='halt')
    t.expect.removed('go')
    t.expect.not_removed('halt')


@S.test('scripted_following Make start following: a Cop follows only as a player ally with a Followage policy '
        'set (the follow rule); a hostile Cop and an ally without a policy do not', players=1, doc=DOC,
        covers=('scripted_following', 'scripted_following.entity', 'scripted_following.use_type=1',
                'scripted_following.on_start_following'))
def following_rule(t):
    t.flags('player1', '+FL_NOTARGET')
    cops = (('hostile', -150, {'followage_policy': '0'}), ('nopol', 0, {'is_player_ally': '1'}),
            ('ally', 150, {'is_player_ally': '1', 'followage_policy': '1'}))
    for n, x, kv in cops:
        t.point('monster_barney', targetname='b_' + n, at=(x, 150, 0), angles='0 270 0', **kv)
        t.point('scripted_following', targetname='f_' + n, entity='b_' + n, use_type='1',
                on_start_following='started_' + n, names=('entity',))
        info(t, 'started_' + n)
    t.teleport('player1', (0, -100, 0))
    t.wait(1.5)
    t.mark('go')
    for n, _x, _kv in cops:
        t.fire('f_' + n, activator='player1')
    t.wait(0.5)
    t.expect.used('started_ally', count=1, after='go')
    t.expect.not_used('started_hostile')
    t.expect.not_used('started_nopol')


# ---------------------------------------------------------------------------------------------- scripted_schedule
def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


@S.test('scripted_schedule 3 (Investigate spot): the scientist goes to the spot; Move preference 1 (Walk) and 0 '
        '(Auto) get him there', players=1, doc=DOC,
        covers=('scripted_schedule.schedule=3',))
def schedule_investigate(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, y, pref in (('sw', -100, '1'), ('sa', 100, '0')):
        t.point('monster_scientist', targetname=n, at=(-150, y, 0), angles='0 0 0', spawnflags='16')
        info(t, n + '_spot', at=(150, y, 0))
        t.point('scripted_schedule', targetname=n + '_go', entity=n, schedule='3', spot_entity=n + '_spot',
                move_preference=pref, names=('entity', 'spot_entity'))
    t.wait(1.5)
    t.fire('sw_go')
    t.fire('sa_go')
    t.wait(7.0)
    for n in ('sw', 'sa', 'sw_spot', 'sa_spot'):
        t.snap('s', n, 'origin')
    t.expect.distance('s', 'sw', 'sw_spot', 0, 120)
    t.expect.distance('s', 'sa', 'sa_spot', 0, 120)


@S.test('scripted_schedule 2 (Cover or retreat from spot): the scientist moves away from the spot', players=1,
        doc=DOC, covers=('scripted_schedule.schedule=2',))
def schedule_cover(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    t.point('monster_scientist', targetname='sci', at=(0, 0, 0), angles='0 0 0', spawnflags='16')
    info(t, 'spot', at=(40, 0, 0))
    t.point('scripted_schedule', targetname='hide', entity='sci', schedule='2', spot_entity='spot', mindist='96',
            maxdist='400', names=('entity', 'spot_entity'))
    t.wait(1.5)
    t.fire('hide')
    t.wait(6.0)
    t.snap('s', 'sci', 'origin')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'sci', 'spot', 90, 600)


@S.test('scripted_schedule 0 (Clear suggested schedule) 1.5 s into a Move to spot: the scientist stops where he is',
        players=1, doc=DOC, covers=('scripted_schedule.schedule=0',))
def schedule_clear(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    t.point('monster_scientist', targetname='sci', at=(-150, 0, 0), angles='0 0 0', spawnflags='16')
    info(t, 'start', at=(-150, 0, 0))
    info(t, 'spot', at=(150, 0, 0))
    t.point('scripted_schedule', targetname='move', entity='sci', schedule='5', spot_entity='spot',
            names=('entity', 'spot_entity'))
    t.point('scripted_schedule', targetname='clear', entity='sci', schedule='0', names=('entity',))
    t.wait(1.5)
    t.fire('move')
    t.wait(1.5)                  # walking now
    t.snap('cleared', 'sci', 'origin')
    t.snap('cleared', 'start', 'origin')
    t.fire('clear')
    t.wait(4.0)
    t.snap('end', 'sci', 'origin')
    t.snap('end', 'spot', 'origin')
    t.expect.distance('cleared', 'sci', 'start', 20, 280)       # on the way when cleared
    t.expect.distance('end', 'sci', 'spot', 60, 400)             # did not go on to the spot


@S.test('scripted_schedule Spot preference 1 (Position, taken once) and 2 (Entity), 0 (Auto): the spot entity moved '
        'away after the order: the scientist goes where it was when fired (Move preference Walk for 1, Auto else)', players=1, doc=DOC,
        covers=('scripted_schedule.spot_preference', 'scripted_schedule.spot_preference=0',
                'scripted_schedule.spot_preference=1', 'scripted_schedule.spot_preference=2',
                'scripted_schedule.move_preference=0', 'scripted_schedule.move_preference=1'))
def schedule_spot_pref(t):
    t.flags('player1', '+FL_NOTARGET')
    node_grid(t)
    for n, y, pref in (('s0', -150, '0'), ('s1', 0, '1'), ('s2', 150, '2')):
        t.point('monster_scientist', targetname=n, at=(-150, y, 0), angles='0 0 0', spawnflags='16')
        info(t, n + '_spot', at=(150, y, 0))
        info(t, n + '_was', at=(150, y, 0))
        t.point('scripted_schedule', targetname=n + '_go', entity=n, schedule='5', spot_entity=n + '_spot',
                spot_preference=pref, move_preference='1' if pref == '1' else '0',
                names=('entity', 'spot_entity'))
        t.point('trigger_motion', targetname=n + '_away', target=n + '_spot', m_iszPosition='0 0 300',
                m_iPosMode='1')
    t.wait(1.5)
    for n in ('s0', 's1', 's2'):
        t.fire(n + '_go')
        t.fire(n + '_away')
    t.wait(7.0)
    for n in ('s0', 's1', 's2'):
        t.snap('s', n, 'origin')
        t.snap('s', n + '_was', 'origin')
        t.expect.distance('s', n, n + '_was', 0, 120)


# ---------------------------------------------------------------------------------------------- trigger_camera
@S.test('trigger_camera killtarget: removed when the camera ends; Stop by player input 0: the use key does not end '
        'it', players=1, doc=OPTIONS,
        covers=('trigger_camera.killtarget', 'trigger_camera.stop_by_player_input=0'))
def camera_killtarget(t):
    info(t, 'look', at=(200, 200, 40))
    info(t, 'victim')
    t.point('trigger_camera', targetname='cam', at=(-150, -100, 100), target='look', wait='1.2',
            killtarget='victim', stop_by_player_input='0')
    t.fire('cam', activator='player1')
    t.wait(0.4)
    t.mark('use')
    t.move('player1', 0, 0, 3, '+use')
    t.wait(0.4)
    t.mark('still')
    t.wait(1.0)
    t.expect.setview('player1', 'cam', min=1)
    t.expect.not_removed('victim', after='use', within=(0, 0.6))
    t.expect.removed('victim', after='still', count=1)


# ---------------------------------------------------------------------------------------------- scripted_action
@S.test('scripted_action Delay before trigger 1 s and Kill target: the target fires 1 s after the action, the kill '
        'target is removed then', players=1, doc=DOC,
        covers=('scripted_action.delay', 'scripted_action.killtarget'))
def action_delay_kill(t):
    t.flags('player1', '+FL_NOTARGET')
    t.point('monster_barney', targetname='bar', at=(0, 0, 0), angles='0 270 0', spawnflags='16')
    t.point('scripted_action', targetname='act', at=(0, 0, 0), m_iszEntity='bar', m_fAction='8', m_fMoveTo='0',
            target='done', delay='1', killtarget='victim', names=('m_iszEntity',))
    info(t, 'done')
    info(t, 'victim')
    t.wait(1.0)
    t.mark('go')
    t.fire('act')
    t.wait(2.0)
    t.expect.used('done', count=1, after='go', within=(0.9, 1.3))
    t.expect.removed('victim', after='go', within=(0.9, 1.3))
