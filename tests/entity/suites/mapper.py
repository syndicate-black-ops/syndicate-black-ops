"""Mapper entities (branch feat/mapper-entities): logic (docs/LOGIC_ENTITIES.md), state logic (STATE_LOGIC.md),
locus / calc (LOCUS.md), env_render (ENV_RENDER.md), map_compat (MAP_COMPAT.md), trigger_jump / trigger_safe_fall
(TRIGGER_JUMP.md), trigger_monsterjump (Obey Trigger Mode), trigger_camera's new flags. Round hooks and round_reset: suite mapper_rounds.

Keys are always given explicitly: where a key is missing the code's default differs from the FGD's for some of these
classes (see the report), and maps made with the FGD always carry it."""

from etest import Suite

SUITE = Suite('mapper', 'Mapper entities (feat/mapper-entities)', requires=('RoundHooks_Reset', 'MapCompat_KeyValue'))
S = SUITE
LOGIC = 'LOGIC_ENTITIES.md'
STATE = 'STATE_LOGIC.md'
LOCUS = 'LOCUS.md'
USE_OFF, USE_ON, USE_SET, USE_TOGGLE = 0, 1, 2, 3


def info(t, name, **kv):
    return t.point('info_target', targetname=name, **kv)


def door(t, name, at=(0, 96, 0), **kv):
    """A door moving 88 up at 200 u/s (0.44 s), staying open (wait -1)."""
    kv.setdefault('wait', '-1')
    return t.brush('func_door', size=(64, 16, 96), at=at, tex='666MURMETAL', targetname=name, angles='-90 0 0',
                   speed='200', lip='8', **kv)


# ---------------------------------------------------------------------------------------------- trigger_condition
@S.test('trigger_condition: constant check of a breakable health < 50 fires message, then netname', doc=LOGIC)
def condition_constant(t):
    t.brush('func_breakable', targetname='crate', health='100')
    t.point('trigger_condition', targetname='cond', target='crate', m_iszValueName='health', m_iszCheckValue='50',
            m_iCheckType='2', netname='yes', message='no', m_iCheckBehaviour='0', m_fCheckInterval='0.1',
            spawnflags='1', names=('netname', 'message'))      # Start Off: turned on in the test, not at map start
    info(t, 'yes')
    info(t, 'no')
    t.fire('cond', use='on')
    t.wait(0.3)
    t.mark('hit')
    t.damage('crate', 60)
    t.wait(0.4)
    t.expect.used('no', caller='cond', count=1)                  # first check false: "message"
    t.expect.used('yes', caller='cond', count=1, after='hit', within=(0, 0.25))
    t.expect.not_used('yes', within=(0, 0.25))


@S.test('trigger_condition: cyclic mode evaluates on each trigger', doc=LOGIC)
def condition_cyclic(t):
    info(t, 'var', **{'$i_n': '1'})
    t.point('trigger_condition', targetname='cond', target='var', m_iszValueName='$i_n', m_iszCheckValue='1',
            m_iCheckType='0', netname='yes', message='no', spawnflags='32', names=('netname', 'message'))
    t.point('trigger_changevalue', targetname='set2', target='var', m_iszValueName='$i_n', m_iszNewValue='2')
    info(t, 'yes')
    info(t, 'no')
    t.wait(0.2)
    t.fire('cond')
    t.fire('cond')
    t.fire('set2')
    t.fire('cond')
    t.wait(0.1)
    t.expect.used('yes', caller='cond', count=2)
    t.expect.used('no', caller='cond', count=1)


# ---------------------------------------------------------------------------------------------- trigger_random / timer
@S.test('trigger_random: each trigger fires exactly one of its targets', doc=LOGIC)
def random_one(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', target3='c', target_count='3')
    for n in 'abc':
        info(t, n)
    for i in range(12):
        t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('a', min=0)
    for n in 'abc':
        t.expect.fired(n, caller='rnd', min=0)
    t.expect.used('@info_target', caller='rnd', count=12)


@S.test('trigger_random "not repetitive" with two targets alternates', doc=LOGIC)
def random_alternate(t):
    t.point('trigger_random', targetname='rnd', target1='a', target2='b', target_count='2', spawnflags='32')
    info(t, 'a')
    info(t, 'b')
    for i in range(6):
        t.fire('rnd')
    t.wait(0.1)
    t.expect.fired('a', count=3)
    t.expect.fired('b', count=3)


@S.test('trigger_random_unique: every target once, then trigger_on_exhaust, then nothing', doc=LOGIC)
def random_unique(t):
    t.point('trigger_random_unique', targetname='rnd', target1='a', target2='b', target3='c', target_count='3',
            trigger_on_exhaust='empty')
    for n in 'abc':
        info(t, n)
    info(t, 'empty')
    for i in range(4):
        t.fire('rnd')
    t.wait(0.1)
    for n in 'abc':
        t.expect.fired(n, count=1)
    t.expect.fired('empty', caller='rnd', count=1)


@S.test('trigger_timer: fires every 0.2 s, stops at trigger_number 3 with trigger_on_limit', doc=LOGIC)
def timer(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.2', max_delay='0.2', trigger_number='3',
            trigger_on_limit='limit')
    info(t, 'tick')
    info(t, 'limit')
    t.mark('go')
    t.fire('tm', use='on')
    t.wait(1.2)
    t.expect.fired('tick', caller='tm', count=3)
    t.expect.fired('tick', count=1, after='go', within=(0.15, 0.25))
    t.expect.fired('limit', count=1, after='go', within=(0.55, 0.7))
    t.expect.order('tick', 'limit')


@S.test('trigger_timer: OFF stops it', doc=LOGIC)
def timer_off(t):
    t.point('trigger_timer', targetname='tm', target='tick', min_delay='0.2', max_delay='0.2')
    info(t, 'tick')
    t.fire('tm', use='on')
    t.wait(0.7)
    t.fire('tm', use='off')
    t.mark('off')
    t.wait(0.6)
    t.expect.fired('tick', min=2)
    t.expect.not_fired('tick', after='off')


# ---------------------------------------------------------------------------------------------- iterator
@S.test('trigger_entity_iterator: fires its target once per matching entity, activator = the entity', doc=LOGIC)
def iterator(t):
    for i in range(3):
        t.point('info_target', at=(-100 + 100 * i, 0, 0), targetname='item')
    info(t, 'other')
    info(t, 'hit')
    # name_filter is a target_destination in the FGD: the DSL gives it the test's prefix (t.name() would add a second)
    t.point('trigger_entity_iterator', targetname='it', name_filter='item', target='hit', triggerstate='2',
            run_mode='0', trigger_after_run='done')
    info(t, 'done')
    t.fire('it')
    t.wait(0.1)
    t.expect.fired('hit', caller='it', activator='item', count=3)
    t.expect.fired('done', count=1)
    t.expect.order('hit', 'done')


# ---------------------------------------------------------------------------------------------- trigger_relay flags
@S.test('trigger_relay: flag 64 forwards the activator, flag 128 passes the caller, none = the relay', doc=LOGIC)
def relay_flags(t):
    info(t, 'act')
    info(t, 'cal')
    t.point('trigger_relay', targetname='fwd', target='o1', triggerstate='2', spawnflags='64')
    t.point('trigger_relay', targetname='pass', target='o2', triggerstate='2', spawnflags='128')
    t.point('trigger_relay', targetname='plain', target='o3', triggerstate='2')
    for n in ('o1', 'o2', 'o3'):
        info(t, n)
    for r in ('fwd', 'pass', 'plain'):
        t.fire(r, activator='act', caller='cal')
    t.wait(0.1)
    t.expect.fired('o1', activator='act', count=1)
    t.expect.fired('o2', activator='cal', count=1)
    t.expect.fired('o3', activator='plain', count=1)


@S.test('trigger_relay: Copy Input (-1) passes ON / OFF on, Reverse Input (-2) swaps them', doc=LOGIC)
def relay_copy(t):
    t.point('trigger_relay', targetname='copy', target='o1', triggerstate='-1')
    t.point('trigger_relay', targetname='rev', target='o2', triggerstate='-2')
    info(t, 'o1')
    info(t, 'o2')
    t.fire('copy', use='on')
    t.fire('copy', use='off')
    t.fire('rev', use='on')
    t.fire('rev', use='off')
    t.wait(0.1)
    t.expect.fired('o1', use=USE_ON, count=1)
    t.expect.fired('o1', use=USE_OFF, count=1)
    t.expect.fired('o2', use=USE_OFF, count=1)
    t.expect.fired('o2', use=USE_ON, count=1)


# ---------------------------------------------------------------------------------------------- obey trigger mode
@S.test('Obey Trigger Mode 0 (default, stock): a closed door opens on OFF', doc=LOGIC)
def obey0(t):
    door(t, 'd')
    t.fire('d', use='off')
    t.wait(0.8)
    t.snap('s', 'd', 'origin')
    t.expect.value('s', 'd', 'origin', [0, 0, 88], tol=0.5)


@S.test('Obey Trigger Mode 1: a closed door ignores OFF, opens on ON', doc=LOGIC)
def obey1(t):
    door(t, 'd', m_iObeyTriggerMode='1')
    t.fire('d', use='off')
    t.wait(0.8)
    t.snap('after_off', 'd', 'origin')
    t.fire('d', use='on')
    t.wait(0.8)
    t.snap('after_on', 'd', 'origin')
    t.expect.value('after_off', 'd', 'origin', [0, 0, 0], tol=0.5)
    t.expect.value('after_on', 'd', 'origin', [0, 0, 88], tol=0.5)


@S.test('Obey Trigger Mode 2: OFF while opening reverses a Toggle door (flag 32)', doc=LOGIC)
def obey2(t):
    door(t, 'd', m_iObeyTriggerMode='2', spawnflags='32')
    t.fire('d', use='on')
    t.wait(0.2)
    t.fire('d', use='off')
    t.wait(0.8)
    t.snap('s', 'd', 'origin')
    t.expect.value('s', 'd', 'origin', [0, 0, 0], tol=0.5)


@S.test('Obey Trigger Mode 1 on trigger_hurt: ON twice leaves it on, OFF turns it off', doc=LOGIC)
def obey_hurt(t):
    t.trigger('trigger_hurt', size=(64, 64, 64), at=(0, 120, 0), targetname='h', dmg='0', m_iObeyTriggerMode='1')
    t.trigger('trigger_hurt', size=(64, 64, 64), at=(100, 120, 0), targetname='h0', dmg='0')
    t.fire('h', use='on')
    t.fire('h', use='on')
    t.fire('h0', use='on')
    t.wait(0.1)
    t.snap('on', 'h', 'solid')
    t.snap('on', 'h0', 'solid')
    t.fire('h', use='off')
    t.wait(0.1)
    t.snap('off', 'h', 'solid')
    t.expect.value('on', 'h', 'solid', 1)       # SOLID_TRIGGER: still on
    t.expect.value('on', 'h0', 'solid', 0)      # mode 0 (stock): ON toggles it off
    t.expect.value('off', 'h', 'solid', 0)


# ---------------------------------------------------------------------------------------------- env_state, calc_state, ~master
@S.test('env_state: turnontime 0.5 fires target ON and noise1 after the delay; OFF fires target OFF', doc=STATE)
def env_state(t):
    t.point('env_state', targetname='st', target='out', noise1='on_snd', noise2='off_snd', turnontime='0.5',
            turnofftime='0', names=('noise1', 'noise2'))
    for n in ('out', 'on_snd', 'off_snd'):
        info(t, n)
    t.mark('go')
    t.fire('st', use='on')
    t.wait(0.8)
    t.mark('off')
    t.fire('st', use='off')
    t.wait(0.2)
    t.expect.fired('out', use=USE_ON, count=1, after='go', within=(0.45, 0.6))
    t.expect.fired('on_snd', count=1, after='go', within=(0.45, 0.6))
    t.expect.fired('out', use=USE_OFF, count=1, after='off', within=(0, 0.1))
    t.expect.fired('off_snd', count=1, after='off', within=(0, 0.1))


@S.test('env_state as master: a door is locked while the state is off and opens when it is on', doc=STATE)
def state_master(t):
    t.point('env_state', targetname='st')
    door(t, 'd', master='st')
    t.fire('d')
    t.wait(0.7)
    t.snap('locked', 'd', 'origin')
    t.fire('st', use='on')
    t.wait(0.1)
    t.fire('d')
    t.wait(0.7)
    t.snap('open', 'd', 'origin')
    t.expect.value('locked', 'd', 'origin', [0, 0, 0], tol=0.5)
    t.expect.value('open', 'd', 'origin', [0, 0, 88], tol=0.5)


@S.test('~master: a door with master "~st" opens while st is off, is locked while it is on', doc=STATE)
def not_master(t):
    t.point('env_state', targetname='st')
    t.point('env_state', targetname='st2', spawnflags='1')
    door(t, 'd1', at=(-80, 96, 0), master='~st')
    door(t, 'd2', at=(80, 96, 0), master='~st2')
    t.fire('d1')
    t.fire('d2')
    t.wait(0.7)
    t.snap('s', 'd1', 'origin')
    t.snap('s', 'd2', 'origin')
    t.expect.value('s', 'd1', 'origin', [0, 0, 88], tol=0.5)
    t.expect.value('s', 'd2', 'origin', [0, 0, 0], tol=0.5)


@S.test('calc_state: AND / OR of two env_states fire target ON or OFF and fire_when_true / false', doc=STATE)
def calc_state(t):
    t.point('env_state', targetname='a', spawnflags='1')
    t.point('env_state', targetname='b')
    t.point('calc_state', targetname='and', left_operand='a', right_operand='b', operation='0', target='o_and',
            fire_when_false='and_false', names=('left_operand', 'right_operand', 'fire_when_false'))
    t.point('calc_state', targetname='or', left_operand='a', right_operand='b', operation='1', target='o_or',
            fire_when_true='or_true', names=('left_operand', 'right_operand', 'fire_when_true'))
    for n in ('o_and', 'o_or', 'and_false', 'or_true'):
        info(t, n)
    t.fire('and')
    t.fire('or')
    t.wait(0.1)
    t.expect.fired('o_and', use=USE_OFF, count=1)
    t.expect.fired('and_false', count=1)
    t.expect.fired('o_or', use=USE_ON, count=1)
    t.expect.fired('or_true', count=1)


# ---------------------------------------------------------------------------------------------- locus / calc
@S.test('calc_eval_number 7 x 6 stores 42 in a game_number (SET), trigger_compare passes', doc=LOCUS)
def calc_eval(t):
    t.point('game_number', targetname='num', frags='0')
    t.point('calc_eval_number', targetname='calc', left_operand='7', right_operand='6', operation='2',
            store_result='num', target='cmp', names=('store_result',))
    t.point('trigger_compare', targetname='cmp', initial_value='num', compare_value='42', testtype='0',
            pass_target='pass', fail_target='fail', names=('initial_value', 'pass_target', 'fail_target'))
    info(t, 'pass')
    info(t, 'fail')
    t.fire('calc')
    t.wait(0.1)
    t.snap('s', 'num', 'frags')
    t.expect.value('s', 'num', 'frags', 42)
    t.expect.fired('num', use=USE_SET, count=1)
    t.expect.fired('pass', count=1)
    t.expect.not_fired('fail')


@S.test('calc_eval_number divide by zero fires trigger_on_fail, stores nothing', doc=LOCUS)
def calc_div0(t):
    t.point('game_number', targetname='num', frags='5')
    t.point('calc_eval_number', targetname='calc', left_operand='7', right_operand='0', operation='3',
            store_result='num', trigger_on_fail='failed', names=('store_result', 'trigger_on_fail'))
    info(t, 'failed')
    t.fire('calc')
    t.wait(0.1)
    t.snap('s', 'num', 'frags')
    t.expect.value('s', 'num', 'frags', 5)
    t.expect.fired('failed', count=1)


@S.test('game_number: ON adds 1, OFF subtracts 1, SET sets the value', doc=LOCUS)
def game_number(t):
    t.point('game_number', targetname='num', frags='10')
    t.fire('num', use='on')
    t.fire('num', use='on')
    t.fire('num', use='off')
    t.snap('a', 'num', 'frags')
    t.fire('num', use='set', value=3)
    t.snap('b', 'num', 'frags')
    t.expect.value('a', 'num', 'frags', 11)
    t.expect.value('b', 'num', 'frags', 3)


@S.test('locus: env_explosion at a named position (mid-air: exact)', doc=LOCUS)
def locus_explosion(t):
    info(t, 'spot', at=(40, 20, 200))
    t.point('env_explosion', targetname='boom', target='spot', iMagnitude='50', spawnflags='1')
    t.fire('boom')
    t.wait(0.05)
    t.snap('s', 'boom', 'origin')
    t.snap('s', 'spot', 'origin')
    t.expect.distance('s', 'boom', 'spot', 0, 0.5, flat=False)


@S.test('calc_position impulse 11 (Body): a scientist\'s body target, 42 above its origin (center 36, eyes 60)',
        doc=LOCUS)
def calc_position_body(t):
    t.point('monster_scientist', at=(150, 100, 0), targetname='sci')
    t.point('calc_position', targetname='pos', netname='sci', impulse='11', names=('netname',))
    t.point('env_explosion', targetname='boom', target='pos', iMagnitude='50', spawnflags='1')
    t.wait(0.5)                                         # settled on the floor
    t.fire('boom')
    t.wait(0.05)
    t.snap('s', 'boom', 'origin')
    t.snap('s', 'sci', 'origin')
    t.expect.distance('s', 'boom', 'sci', 0, 0.5)                      # straight above it
    # 0.75 x center 36 + 0.25 x eyes 60 (MonsterInit takes the eyes from scientist.mdl, not Spawn's 50)
    t.expect.distance('s', 'boom', 'sci', 41.5, 42.5, flat=False)


@S.test('locus: monstermaker spawns its child at spawnorigin', doc=LOCUS)
def locus_maker(t):
    info(t, 'spot', at=(150, 100, 0))
    t.point('monstermaker', targetname='mk', monstertype='monster_scientist', netname='kid', spawnorigin='spot',
            monstercount='1', names=('netname', 'spawnorigin'))
    t.fire('mk')
    t.wait(0.5)
    t.snap('s', 'kid', 'origin')
    t.snap('s', 'spot', 'origin')
    t.expect.spawned('@monster_scientist', min=1)
    t.expect.distance('s', 'kid', 'spot', 0, 2)


# ---------------------------------------------------------------------------------------------- env_render
@S.test('env_render fade: end values in pev at once, netname fired at the end, RenderFade sent', doc='ENV_RENDER.md')
def render_fade(t):
    t.brush('func_wall', at=(0, 96, 0), targetname='wall', rendermode='0', renderamt='255')
    t.point('env_render', targetname='rf', target='wall', rendermode='2', renderamt='100', frags='1',
            netname='faded', spawnflags='0', names=('netname',))
    info(t, 'faded')
    t.mark('go')
    t.fire('rf')
    t.wait(0.05)
    t.snap('during', 'wall', 'rendermode', 'renderamt')
    t.wait(1.2)
    t.expect.value('during', 'wall', 'rendermode', 2)
    t.expect.value('during', 'wall', 'renderamt', 100)
    t.expect.fired('faded', count=1, after='go', within=(0.95, 1.15))
    t.expect.msg('RenderFade', min=1, after='go', within=(0, 0.1))


@S.test('env_render: Remove targets (32) removes an info_target target after its fade', doc='ENV_RENDER.md')
def render_remove_info(t):
    t.point('info_target', at=(0, 96, 0), targetname='prop', rendermode='2', renderamt='255')
    t.point('env_render', targetname='rf', target='prop', rendermode='2', renderamt='0', frags='0.5', spawnflags='32')
    t.fire('rf')
    t.wait(0.9)
    t.expect.removed('prop')


@S.test('env_render: Remove targets (32) removes a cycler (model) target after its fade', doc='ENV_RENDER.md')
def render_remove_point(t):
    t.point('cycler', at=(0, 96, 0), targetname='prop', model='models/zombie.mdl', rendermode='2', renderamt='255')
    t.point('env_render', targetname='rf', target='prop', rendermode='2', renderamt='0', frags='0.5', spawnflags='32')
    t.fire('rf')
    t.wait(0.9)
    t.expect.removed('prop')


@S.test('env_render: Remove targets (32) removes a monster target after its fade', doc='ENV_RENDER.md')
def render_remove_monster(t):
    t.point('monster_scientist', at=(0, 96, 0), targetname='sci', rendermode='2', renderamt='255')
    t.point('env_render', targetname='rf', target='sci', rendermode='2', renderamt='0', frags='0.5', spawnflags='32')
    t.fire('rf')
    t.wait(0.9)
    t.expect.removed('sci')


@S.test('env_render: Remove targets (32) removes a brush target after its fade', doc='ENV_RENDER.md')
def render_remove(t):
    t.brush('func_wall', at=(0, 96, 0), targetname='wall', rendermode='2', renderamt='255')
    t.point('env_render', targetname='rf', target='wall', rendermode='2', renderamt='0', frags='0.5', spawnflags='32')
    t.fire('rf')
    t.wait(0.9)
    t.expect.removed('wall')


# ---------------------------------------------------------------------------------------------- map_compat
@S.test('map_compat: an entry for this map (its CRC) sets one key and drops another at load', doc='MAP_COMPAT.md')
def compat(t):
    # an unnamed light is removed at spawn and its edict reused: the selectors must still find their entities
    t.point('light', at=(0, 0, 100), _light='255 255 255 100')
    info(t, 'c_set', health='10')
    t.point('light', at=(50, 0, 100), _light='255 255 255 100')
    info(t, 'c_drop', renderamt='50')
    info(t, 'c_other', health='10')
    info(t, 'c_after', health='10')      # the next info_target: must keep its own value
    t.point('light', at=(-50, 0, 100), _light='255 255 255 100')
    info(t, 'c_idx', health='10')
    info(t, 'c_idx_after', health='10')
    t.map_compat('set', 'info_target', 'c_set', 'health', '77')
    t.map_compat('drop', 'info_target', 'c_drop', 'renderamt')
    t.map_compat('set', 'info_target', 'c_other', 'health', '99', crc='00000000')     # another version of the map
    t.map_compat('set', 'info_target', '#@c_idx', 'health', '55')                    # by entity-lump index
    for n in ('c_set', 'c_other', 'c_after', 'c_idx', 'c_idx_after'):
        t.snap('s', n, 'health')
    t.snap('s', 'c_drop', 'renderamt')
    t.expect.value('s', 'c_set', 'health', 77)
    t.expect.value('s', 'c_drop', 'renderamt', 0)
    t.expect.value('s', 'c_other', 'health', 10)
    t.expect.value('s', 'c_after', 'health', 10)
    t.expect.value('s', 'c_idx', 'health', 55)
    t.expect.value('s', 'c_idx_after', 'health', 10)


# ---------------------------------------------------------------------------------------------- trigger_jump / safe fall
@S.test('trigger_jump launches a player onto its target and fires trigger_on_jump', doc='TRIGGER_JUMP.md', players=1,
        size=(512, 512, 512))
def jump(t):
    t.brush('func_wall', size=(96, 96, 128), at=(150, 150, 0), tex='666CAISSE', targetname='ledge')
    info(t, 'land', at=(150, 150, 128 + 36))
    t.trigger('trigger_jump', size=(64, 64, 64), at=(-100, -100, 0), target='land', trigger_on_jump='jumped')
    info(t, 'jumped')
    t.mark('in')
    t.teleport('player1', (-100, -100, 0))
    t.wait(2.5)
    t.snap('end', 'player1', 'origin')
    t.snap('end', 'land', 'origin')
    t.expect.fired('jumped', activator='player1', count=1, after='in', within=(0, 0.3))
    t.expect.distance('end', 'player1', 'land', 0, 80)       # on the ledge (96 wide): it lands near the point
    t.expect.value('end', 'player1', 'origin.z', 128 + 36 - 1, op='>=')


def fall(t, safe):
    if safe:
        t.trigger('trigger_safe_fall', size=(128, 128, 200), at=(0, 0, 0))
    t.kv('player1', 'health', '1000')
    t.mark('drop')
    t.teleport('player1', (0, 0, 700))
    t.wait(2.0)
    t.snap('end', 'player1', 'origin', 'health')


@S.test('a fall of 700 units hurts (control for trigger_safe_fall)', players=1, size=(512, 512, 1024),
        doc='TRIGGER_JUMP.md')
def fall_hurts(t):
    fall(t, False)
    t.expect.damaged('player1', min=1, after='drop')


@S.test('trigger_safe_fall: the same fall through the volume does not hurt', players=1, size=(512, 512, 1024),
        doc='TRIGGER_JUMP.md')
def fall_safe(t):
    fall(t, True)
    t.expect.not_damaged('player1', after='drop')


def monsterjump(t, **kv):
    """A monstermaker (Cyclic) whose scientist walks a path_corner east into a trigger_monsterjump at the edge of a
    32-unit gap; the far side is 24 lower, a trigger_multiple (monsters only) on it fires 'landed'. The trigger is
    named (so it starts off) with Obey Trigger Mode 1 and the default speed and height (200, 150) unless kv sets them.
    A named monster would wait for a trigger and drop its path, so the walker is the maker's unnamed child
    (childtarget)."""
    t.brush('func_wall', size=(224, 160, 64), at=(-128, 0, 0), tex='666CAISSE')         # near side x -240..-16
    t.brush('func_wall', size=(224, 160, 40), at=(128, 0, 0), tex='666CAISSE')          # far side x 16..240
    t.point('path_corner', targetname='p1', at=(-44, 0, 72), target='p2')
    t.point('path_corner', targetname='p2', at=(200, 0, 48))
    t.point('monstermaker', targetname='mk', at=(-200, 0, 65), monstertype='monster_scientist', childtarget='p1',
            monstercount='-1', m_imaxlivechildren='9', spawnflags='4')
    t.trigger('trigger_monsterjump', size=(32, 160, 72), at=(-32, 0, 64), targetname='mj', angles='0 0 0',
              m_iObeyTriggerMode='1', **kv)
    t.trigger('trigger_multiple', size=(200, 160, 48), at=(130, 0, 40), target='landed', spawnflags='3', wait='0.5')
    info(t, 'landed')


@S.test('trigger_monsterjump throws a monster walking into it over a gap, then switches itself off', players=1,
        doc=LOGIC)
def monsterjump_walk(t):
    monsterjump(t)
    t.fire('mj', use='on')
    t.wait(0.1)
    t.snap('on', 'mj', 'solid')
    t.mark('go')
    t.fire('mk')
    t.wait(8)
    t.snap('after', 'mj', 'solid')
    t.expect.value('on', 'mj', 'solid', 1)          # SOLID_TRIGGER
    t.expect.used('landed', min=1, after='go')
    t.expect.value('after', 'mj', 'solid', 0)       # SOLID_NOT: Half-Life's trigger turns itself off after a throw


@S.test('trigger_monsterjump off: the monster stops at the edge; On (Obey Trigger Mode) throws it at once', players=1,
        doc=LOGIC)
def monsterjump_on(t):
    monsterjump(t)
    t.fire('mk')                                    # named: the trigger starts off
    t.wait(8)
    t.mark('on')
    t.fire('mj', use='off')                         # Obey Trigger Mode: Off while off does nothing
    t.wait(0.1)
    t.snap('off', 'mj', 'solid')
    t.fire('mj', use='on')
    t.wait(2)
    t.expect.value('off', 'mj', 'solid', 0)
    t.expect.not_used('landed', within=(0, 8))      # waited at the edge
    t.expect.used('landed', min=1, after='on', within=(0, 2))


@S.test('trigger_monsterjump speed and height keys: speed 1, height 1 does not throw the monster over the gap',
        players=1, doc=LOGIC)
def monsterjump_keys(t):
    monsterjump(t, speed='1', height='1')
    t.fire('mj', use='on')
    t.wait(0.1)
    t.snap('on', 'mj', 'solid', 'speed')
    t.mark('go')
    t.fire('mk')
    t.wait(8)
    t.expect.value('on', 'mj', 'speed', 1)          # 1.38 (as Half-Life): 200 whatever the map said
    t.expect.not_used('landed', after='go')         # 1.38: 200 / 150 threw it across


# ---------------------------------------------------------------------------------------------- nuclear_explosion
@S.test('nuclear_explosion: the afterglow is scheduled (glow lifetime v8) and the entity stays to be fired again',
        players=1)
def nuke_afterglow(t):
    """1.38 set the AfterGlow think without a nextthink: it never ran. Now it runs v8 tenths of a second after the
    Use; the entity is kept (co_zylom's button fires its garg_boom every 3 s). No damage: v11 0."""
    t.point('nuclear_explosion', targetname='nuke', at=(200, 200, 0), v1='10', v2='255', v3='10', v4='5', v5='8',
            v6='200', v7='0', v8='10', v9='16', v10='190', v11='0', v12='64', v13='64', v14='10', v15='16', v16='200')
    t.fire('nuke')
    t.wait(0.1)
    t.snap('fired', 'nuke', 'nextthink')
    t.wait(1.5)
    t.snap('after', 'nuke', 'classname', 'nextthink')
    t.fire('nuke')
    t.wait(0.1)
    t.snap('again', 'nuke', 'nextthink')
    t.expect.value('fired', 'nuke', 'nextthink', 0, op='>')        # 1.38: 0, the afterglow never came
    t.expect.value('after', 'nuke', 'classname', 'nuclear_explosion')
    t.expect.value('after', 'nuke', 'nextthink', 0)                # the afterglow ran, nothing more to do
    t.expect.value('again', 'nuke', 'nextthink', 0, op='>')        # fired a second time: afterglow again


# ---------------------------------------------------------------------------------------------- trigger_camera
def camera(t, flags, **kv):
    info(t, 'look', at=(200, 200, 40))
    # the camera starts at its first corner's line: it travels p1 -> p2 (Move: the first corner is passed at once)
    t.point('path_corner', targetname='p1', at=(-150, -100, 100), target='p2', speed='100')
    t.point('path_corner', targetname='p2', at=(-150, 200, 100), speed='100')
    t.point('trigger_camera', targetname='cam', at=(-150, -100, 100), target='look', moveto='p1', wait='1',
            spawnflags=str(4 | flags), speed='100', acceleration='500', deceleration='500', **kv)
    t.fire('cam', activator='player1')
    t.wait(0.1)
    t.snap('run', 'cam', 'velocity', 'angles', 'avelocity', 'origin')
    t.snap('run', 'player1', 'flags')
    t.wait(1.3)
    t.snap('after', 'player1', 'flags')
    t.expect.used('look', caller='cam', min=1)             # its target is fired at the end
    t.expect.value('run', 'player1', 'flags', 4096, op='&')       # Freeze Player (4): FL_FROZEN while it runs
    t.expect.value('after', 'player1', 'flags', 4096, op='!&')    # cleared at the camera's end


@S.test('trigger_camera "Instant Move" (64): full path speed from the first frame', players=1, doc=LOGIC)
def camera_instant(t):
    camera(t, 64)
    t.expect.value('run', 'cam', 'velocity', [0, 100, 0], tol=1)


@S.test('trigger_camera without Instant Move (control): accelerates', players=1, doc=LOGIC)
def camera_accel(t):
    camera(t, 0)
    t.expect.value('run', 'cam', 'velocity.y', 90, op='<')      # 500 u/s^2 for 0.1 s: about 50


@S.test('trigger_camera "Instant Follow Turn" (65536): faces its target at once, no avelocity', players=1, doc=LOGIC)
def camera_turn(t):
    camera(t, 65536)
    t.expect.value('run', 'cam', 'avelocity', [0, 0, 0], tol=0.01)


@S.test('trigger_camera turnspeed 80 turns twice as fast as the default 40', players=1, doc=LOGIC)
def camera_turnspeed(t):
    # the same place and start angles: the first turn rate is proportional to turnspeed
    info(t, 'look', at=(200, 200, 40))
    t.point('trigger_camera', targetname='c40', at=(-150, -150, 100), target='look', wait='0.3', spawnflags='0')
    t.point('trigger_camera', targetname='c80', at=(-150, -150, 100), target='look', wait='0.3', spawnflags='0',
            turnspeed='80')
    # the first think comes 0 or 1 frame after the trigger: compare after 3 frames each
    t.fire('c40', activator='player1')
    t.wait_frames(3)
    t.snap('a', 'c40', 'avelocity', 'angles')
    t.wait(0.5)
    t.fire('c80', activator='player1')
    t.wait_frames(3)
    t.snap('b', 'c80', 'avelocity', 'angles')
    t.expect.ratio('a', 'c40', 'b', 'c80', 'avelocity.y', 2.0, tol=0.15)


# ---------------------------------------------------------------------------------------------- editor placeholder models
@S.test('env_model with the FGD default model (models/editor/env_model.mdl): removed at spawn, never precached',
        doc='ENV_MODEL.md')
def env_model_editor_placeholder(t):
    t.point('env_model', targetname='placeholder', model='models/editor/env_model.mdl')
    t.point('env_model', at=(0, 96, 0), targetname='real', model='models/zombie.mdl')    # control
    t.wait(0.1)
    t.expect.absent('placeholder')
    t.expect.not_precached('models/editor/env_model.mdl')
    t.expect.precached('models/zombie.mdl', kind='model')


@S.test('env_static with the FGD default model (models/editor/env_static.mdl): removed at spawn, never offered for '
        'download', doc='ENV_STATIC.md')
def env_static_editor_placeholder(t):
    t.point('env_static', targetname='placeholder', model='models/editor/env_static.mdl')
    t.point('env_static', at=(0, 96, 0), targetname='real', model='models/zombie.mdl')   # control
    t.wait(0.1)
    t.expect.absent('placeholder')
    t.expect.not_precached('models/editor/env_static.mdl')
    t.expect.not_precached('models/editor/env_staticT.mdl')
    t.expect.precached('models/zombie.mdl', kind='generic')
