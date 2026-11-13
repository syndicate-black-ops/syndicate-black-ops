"""The cheat and entity debug commands from the Half-Life Unified SDK and cheat_speed (src/dlls/cheat_commands.cpp,
docs/CHEAT_COMMANDS.md): each one typed by a player (test_cmd) with sv_cheats 1 does what the docs say, and with
sv_cheats 0 does nothing. The impulse cheats (210 god, 211 notarget, 200 noclip, 212 speed 2000) keep working and
share their state with the commands. ent_create / npc_create refuse a class whose resources the map has not loaded,
with no precache after the load (an implicit check of every run).

Console replies go to the player's console only (a test player has none), so each command is checked by what it
changes: pev fields (flags, movetype, maxspeed, health, armorvalue, targetname), entities spawned and removed, the
temp entities sent. cheat_infiniteair is not covered here: the generated rooms have no water."""

from etest import Suite

SUITE = Suite('cheat_commands', 'Cheat and entity debug commands (cheat_*, ent_*, npc_create, cheat_speed)',
              players=1)
S = SUITE
DOC = 'CHEAT_COMMANDS.md'

FL_GODMODE, FL_NOTARGET = 64, 128
MOVETYPE_WALK, MOVETYPE_NOCLIP = 3, 8
TE_BOX, TE_FIREFIELD = 31, 123


def ready(t):
    t.console('sv_cheats 1')
    t.wait(2.5)     # past the spawn protection (no damage for 2 s after a spawn)


def snap_flags(t, label):
    t.wait_frames(2)
    t.snap(label, 'player1', 'flags', 'movetype')


@S.test('sv_cheats 0: every command is refused and changes nothing', players=1, doc=DOC + '#rules')
def refused(t):
    t.console('sv_cheats 0')
    t.wait(2.5)
    t.snap('before', 'player1', 'flags', 'movetype', 'maxspeed', 'health')
    for c in ('cheat_god', 'cheat_notarget', 'cheat_noclip', 'cheat_unkillable', 'cheat_infinitearmor',
              'cheat_infiniteair'):
        t.cmd('player1', c)
    t.cmd('player1', 'cheat_speed', '3')
    t.point('info_target', targetname='keep', at=(0, 0, 32))
    t.cmd('player1', 'ent_remove_all', t.name('keep'))
    t.cmd('player1', 'ent_create', 'info_target', 'targetname', 'et_cheat_refused')
    t.cmd('player1', 'npc_create', 'info_target')
    t.cmd('player1', 'ent_show_bbox', t.name('keep'))
    t.mark('cmds')
    t.wait_frames(3)
    t.snap('after', 'player1', 'flags', 'movetype', 'maxspeed', 'health')
    t.expect.same('before', 'after', 'player1', 'flags')
    t.expect.same('before', 'after', 'player1', 'movetype')
    t.expect.same('before', 'after', 'player1', 'maxspeed')
    t.expect.not_removed('keep')
    t.expect.spawned('@info_target', count=0, after='cmds')
    t.expect.no_msg('svc:23', to='player1', after='cmds')
    t.console('sv_cheats 1')


@S.test('cheat_god / cheat_notarget / cheat_noclip: toggle, 0 / 1 force, shared with impulse 210 / 211 / 200',
        players=1, doc=DOC + '#cheats')
def toggles(t):
    ready(t)
    t.cmd('player1', 'cheat_god')
    t.cmd('player1', 'cheat_notarget')
    t.cmd('player1', 'cheat_noclip')
    snap_flags(t, 'on')
    t.cmd('player1', 'cheat_god')
    t.cmd('player1', 'cheat_notarget')
    t.cmd('player1', 'cheat_noclip')
    snap_flags(t, 'off')
    t.cmd('player1', 'cheat_god', '1')
    t.cmd('player1', 'cheat_god', '1')         # forced: stays on
    t.cmd('player1', 'cheat_noclip', '1')
    t.cmd('player1', 'cheat_noclip', '1')
    snap_flags(t, 'forced')
    t.kv('player1', 'impulse', '210')          # the impulse turns the same godmode off (one impulse a frame)
    t.wait_frames(2)
    t.kv('player1', 'impulse', '200')
    t.wait_frames(2)
    t.kv('player1', 'impulse', '211')          # notarget on by impulse, off by the command
    snap_flags(t, 'impulse')
    t.cmd('player1', 'cheat_notarget', '0')
    t.cmd('player1', 'cheat_god', '0')
    snap_flags(t, 'end')
    t.expect.value('on', 'player1', 'flags', FL_GODMODE | FL_NOTARGET, op='&')
    t.expect.value('on', 'player1', 'movetype', MOVETYPE_NOCLIP)
    t.expect.value('off', 'player1', 'flags', FL_GODMODE | FL_NOTARGET, op='!&')
    t.expect.value('off', 'player1', 'movetype', MOVETYPE_WALK)
    t.expect.value('forced', 'player1', 'flags', FL_GODMODE, op='&')
    t.expect.value('forced', 'player1', 'movetype', MOVETYPE_NOCLIP)
    t.expect.value('impulse', 'player1', 'flags', FL_GODMODE, op='!&')
    t.expect.value('impulse', 'player1', 'flags', FL_NOTARGET, op='&')
    t.expect.value('impulse', 'player1', 'movetype', MOVETYPE_WALK)
    t.expect.value('end', 'player1', 'flags', FL_GODMODE | FL_NOTARGET, op='!&')


@S.test('cheat_unkillable: 1000 damage leaves 1 health; off again, the same hit kills', players=1, doc=DOC + '#cheats')
def unkillable(t):
    ready(t)
    t.kv('player1', 'health', '100')
    t.cmd('player1', 'cheat_unkillable')
    t.wait_frames(1)
    t.damage('player1', 1000, 0)
    t.wait_frames(1)
    t.snap('hit', 'player1', 'health', 'deadflag')
    t.damage('player1', 1000, 0)
    t.wait_frames(1)
    t.snap('hit2', 'player1', 'health', 'deadflag')
    t.cmd('player1', 'cheat_unkillable')
    t.wait_frames(1)
    t.damage('player1', 1000, 0)
    t.wait_frames(2)
    t.snap('dead', 'player1', 'health', 'deadflag')
    t.expect.value('hit', 'player1', 'health', 1)
    t.expect.value('hit', 'player1', 'deadflag', 0)
    t.expect.value('hit2', 'player1', 'health', 1)
    t.expect.value('dead', 'player1', 'health', 1, op='<')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')


@S.test('cheat_infinitearmor: the armor still absorbs damage and is not used up', players=1, doc=DOC + '#cheats')
def infinite_armor(t):
    ready(t)
    t.kv('player1', 'health', '1000')
    t.kv('player1', 'max_health', '1000')
    t.kv('player1', 'armorvalue', '100')
    t.cmd('player1', 'cheat_infinitearmor')
    t.wait_frames(1)
    t.console('test_seed 7')
    t.damage('player1', 100, 2, hitgroup=2)    # a chest hit: every armor type covers the chest
    t.wait_frames(1)
    t.snap('on', 'player1', 'health', 'armorvalue')
    t.cmd('player1', 'cheat_infinitearmor', '0')
    t.wait_frames(1)
    t.console('test_seed 7')
    t.kv('player1', 'health', '1000')
    t.damage('player1', 100, 2, hitgroup=2)
    t.wait_frames(1)
    t.snap('off', 'player1', 'health', 'armorvalue')
    t.expect.value('on', 'player1', 'armorvalue', 100)
    t.expect.value('on', 'player1', 'health', 800, op='>')     # the armor took its share of the 200 (chest x2)
    t.expect.value('off', 'player1', 'armorvalue', 100, op='<')


@S.test('cheat_speed: n times the normal speed, capped at 2000; with impulse 212 the last one wins; '
        'cheat_speed / cheat_speed 1 end both', players=1, doc=DOC + '#cheat_speed')
def speed(t):
    ready(t)
    t.wait_frames(2)
    t.snap('base', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed', '2')
    t.wait_frames(2)
    t.snap('x2', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed', '0.5')
    t.wait_frames(2)
    t.snap('half', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed', '100')
    t.wait_frames(2)
    t.snap('cap', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed')
    t.wait_frames(2)
    t.snap('reset1', 'player1', 'maxspeed')
    # 212 then cheat_speed 2: the multiplier replaces the 2000
    t.kv('player1', 'impulse', '212')
    t.wait_frames(2)
    t.snap('i212', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed', '2')
    t.wait_frames(2)
    t.snap('i212_x2', 'player1', 'maxspeed')
    # cheat_speed 2 then 212: 2000; 212 again: off, normal speed
    t.kv('player1', 'impulse', '212')
    t.wait_frames(2)
    t.snap('x2_i212', 'player1', 'maxspeed')
    t.kv('player1', 'impulse', '212')
    t.wait_frames(2)
    t.snap('i212_off', 'player1', 'maxspeed')
    # 212 on, then cheat_speed 1 ends it
    t.kv('player1', 'impulse', '212')
    t.wait_frames(2)
    t.cmd('player1', 'cheat_speed', '1')
    t.wait_frames(2)
    t.snap('reset2', 'player1', 'maxspeed')
    # a speed the game sets meanwhile (a leg hit) is the new normal speed, multiplied; off gives it back
    t.cmd('player1', 'cheat_speed', '2')
    t.wait_frames(2)
    t.console('test_seed 7')
    t.kv('player1', 'health', '1000')
    t.damage('player1', 1, 2, hitgroup=6)
    t.wait_frames(2)
    t.snap('leg_x2', 'player1', 'maxspeed')
    t.cmd('player1', 'cheat_speed')
    t.wait_frames(2)
    t.snap('leg', 'player1', 'maxspeed')
    # invalid: nothing changes
    t.cmd('player1', 'cheat_speed', '-3')
    t.wait_frames(2)
    t.snap('bad', 'player1', 'maxspeed')
    t.expect.ratio('base', 'player1', 'x2', 'player1', 'maxspeed', 2.0, tol=0.01)
    t.expect.ratio('base', 'player1', 'half', 'player1', 'maxspeed', 0.5, tol=0.01)
    t.expect.value('cap', 'player1', 'maxspeed', 2000)
    t.expect.ratio('base', 'player1', 'reset1', 'player1', 'maxspeed', 1.0, tol=0.01)
    t.expect.value('i212', 'player1', 'maxspeed', 2000)
    t.expect.ratio('base', 'player1', 'i212_x2', 'player1', 'maxspeed', 2.0, tol=0.01)
    t.expect.value('x2_i212', 'player1', 'maxspeed', 2000)
    t.expect.ratio('base', 'player1', 'i212_off', 'player1', 'maxspeed', 1.0, tol=0.01)
    t.expect.ratio('base', 'player1', 'reset2', 'player1', 'maxspeed', 1.0, tol=0.01)
    t.expect.ratio('leg', 'player1', 'leg_x2', 'player1', 'maxspeed', 2.0, tol=0.01)
    t.expect.same('leg', 'bad', 'player1', 'maxspeed')
    t.console('test_round restart')
    t.console('wait_for alive player1 20')


@S.test('ent_create / ent_setname / ent_fire / ent_remove / ent_remove_all', players=1, doc=DOC + '#entity-commands')
def entities(t):
    ready(t)
    t.point('trigger_relay', targetname='relay', target='done')
    t.point('info_target', targetname='done', at=(0, 0, 32))
    t.point('info_target', targetname='gone', at=(-100, 0, 32))
    t.point('info_target', targetname='many', at=(100, 0, 32))
    t.point('info_target', targetname='many', at=(100, 50, 32))
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait_frames(2)
    t.mark('go')
    t.cmd('player1', 'ent_create', 'info_target', 'targetname', 'et_cheat_made')
    t.wait_frames(2)
    t.cmd('player1', 'ent_setname', 'et_cheat_renamed', 'et_cheat_made')
    t.wait_frames(1)
    t.snap('made', '=et_cheat_renamed', 'classname', 'targetname')
    t.cmd('player1', 'ent_fire', t.name('relay'))
    t.cmd('player1', 'ent_remove', t.name('gone'))
    t.cmd('player1', 'ent_remove_all', t.name('many'))
    t.cmd('player1', 'ent_remove_all', 'player')       # players are never removed
    t.cmd('player1', 'ent_remove_all', 'et_cheat_renamed')
    t.wait_frames(3)
    t.snap('alive', 'player1', 'health', 'deadflag')
    t.expect.spawned('@info_target', count=1, after='go')
    t.expect.value('made', '=et_cheat_renamed', 'targetname', 'et_cheat_renamed')
    t.expect.used('relay', count=1, after='go')
    t.expect.fired('done', count=1, after='go')
    t.expect.removed('gone', after='go')
    t.expect.removed('many', count=2, after='go')
    t.expect.removed('@info_target', count=4, after='go')
    t.expect.value('alive', 'player1', 'deadflag', 0)


@S.test('ent_create / npc_create: a class whose model (a file the game has) the map has not loaded is refused, with no precache after '
        'the load', players=1, doc=DOC + '#entity-commands')
def precache_guard(t):
    ready(t)
    t.teleport('player1', (0, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait_frames(2)
    t.mark('go')
    t.cmd('player1', 'ent_create', 'env_sprite', 'model', 'sprites/editor/ambient_generic.spr')
    t.cmd('player1', 'ent_create', 'cycler', 'model', 'models/editor/cycler.mdl')
    t.cmd('player1', 'npc_create', 'monster_et_no_such_class')
    t.cmd('player1', 'ent_create', 'worldspawn')
    t.wait_frames(3)
    t.expect.not_precached('sprites/editor/ambient_generic.spr')
    t.expect.not_precached('models/editor/cycler.mdl')
    t.expect.spawned('@env_sprite', count=0, after='go')
    t.expect.spawned('@cycler', count=0, after='go')


@S.test('npc_create: a monster the map has loaded stands on the floor ahead', players=1, doc=DOC + '#entity-commands')
def npc(t):
    ready(t)
    t.point('monster_scientist', at=(-200, -200, 0))     # the map loads the class
    t.teleport('player1', (-150, 0, 0), yaw=0)
    t.look('player1', 0, 0)
    t.wait_frames(2)
    t.mark('go')
    t.cmd('player1', 'npc_create', 'monster_scientist', 'targetname', 'et_cheat_npc')
    t.wait(0.5)
    t.snap('npc', '=et_cheat_npc', 'origin')
    t.expect.spawned('@monster_scientist', count=1, after='go')
    t.cmd('player1', 'ent_remove_all', 'et_cheat_npc')


@S.test('ent_show_bbox / ent_show_center / ent_show_origin: a box and two dots for the player who asked',
        players=1, doc=DOC + '#entity-commands')
def show(t):
    ready(t)
    t.brush('func_wall', targetname='wall', size=(64, 64, 64), at=(0, 100, 32))
    t.mark('go')
    t.cmd('player1', 'ent_show_bbox', t.name('wall'))
    t.cmd('player1', 'ent_show_center', t.name('wall'))
    t.cmd('player1', 'ent_show_origin', 'func_wall')
    t.cmd('player1', 'ent_show_origin', 'et_cheat_nothing')
    t.wait_frames(2)
    t.expect.msg('svc:23', to='player1', fields={'te': TE_BOX}, count=1, after='go')
    t.expect.msg('svc:23', to='player1', fields={'te': TE_FIREFIELD}, count=2, after='go')


@S.test('ent_list / ent_find_by_classname / ent_find_by_targetname: run with pages, no error', players=1, doc=DOC + '#entity-commands')
def lists(t):
    ready(t)
    t.point('info_target', targetname='found', at=(0, 0, 32))
    t.cmd('player1', 'ent_list')
    t.cmd('player1', 'ent_list', '3')
    t.cmd('player1', 'ent_list', '9999')
    t.cmd('player1', 'ent_find_by_classname', 'info_target')
    t.cmd('player1', 'ent_find_by_classname', 'info_target', '2')
    t.cmd('player1', 'ent_find_by_targetname', t.name('found'))
    t.cmd('player1', 'ent_find_by_targetname')
    t.wait_frames(2)
    t.snap('alive', 'player1', 'deadflag')
    t.expect.value('alive', 'player1', 'deadflag', 0)


@S.test('Cheats message: each player gets the sv_cheats value as it is, 1 and 2 apart (the in-game editors need 2)',
        players=1, doc=DOC + '#rules')
def cheats_value(t):
    t.console('sv_cheats 0')
    t.wait(0.5)
    t.mark('two')
    t.console('sv_cheats 2')
    t.wait(0.5)
    t.mark('one')
    t.console('sv_cheats 1')
    t.wait(0.5)
    t.expect.msg('Cheats', to='player1', fields={'value': 2}, count=1, after='two')
    t.expect.msg('Cheats', to='player1', fields={'value': 1}, count=1, after='one')
    t.expect.msg('Cheats', to='player1', count=2, after='two')     # sent on a change only, not every frame
