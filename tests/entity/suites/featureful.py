"""Featureful entities (feat/featureful-entities): trigger_playerfreeze, trigger_killmonster, trigger_hurt_remote,
trigger_look (docs/LOGIC_ENTITIES.md), env_dlight (docs/ENV_DLIGHT.md), locus_beam and player_calc_ratio
(docs/LOCUS.md). Each setting, flag and mode, and the entities that usually drive or read them (func_button,
trigger_relay, multisource, calc_position, locus_variable). Its own suite: the last test leaves player2 in
spectator mode.

Players get SBO's random damage scaling (0.25x to 2x per hit, CBaseMonster / CBasePlayer::TakeDamage), so damage to
players and monsters is checked by ranges; func_breakable takes exact damage."""

from etest import Suite

SUITE = Suite('featureful', 'Featureful entities: player freeze, kill monster, hurt remote, look, dlight, '
              'locus beam, player calc ratio (feat/featureful-entities)', players=2,
              requires=('CTriggerPlayerFreeze', 'CEnvDLight', 'CLocusBeam', 'CPlayerCalcRatio'))
S = SUITE
DOC_LOGIC = 'LOGIC_ENTITIES.md'
DOC_DLIGHT = 'ENV_DLIGHT.md'
DOC_LOCUS = 'LOCUS.md'
FL_FROZEN = 4096
TE_SPARKS = 9


def tough(t, player='player1', health='1000'):
    t.kv(player, 'max_health', health)
    t.kv(player, 'health', health)


def lv(t, name, ratio):
    """A locus_variable that records the number [LR] `ratio` (a player_calc_ratio's name) when fired."""
    t.point('locus_variable', targetname=name, m_iszRatio=ratio, m_iszPosition='', m_iszVelocity='',
            names=('m_iszRatio',))


# ---------------------------------------------------------------------------------------------- trigger_playerfreeze

@S.test('trigger_playerfreeze Freeze / Unfreeze: only the activator; he stays frozen (PreThink keeps him frozen)',
        players=2, doc=DOC_LOGIC + '#trigger_playerfreeze')
def freeze_activator(t):
    t.point('trigger_playerfreeze', targetname='frz', use_type='1')
    t.point('trigger_playerfreeze', targetname='unfrz', use_type='-1')
    t.fire('frz', activator='player1')
    t.wait(0.5)
    t.snap('on', 'player1', 'flags')
    t.snap('on', 'player2', 'flags')
    t.fire('unfrz', activator='player1')
    t.wait(0.2)
    t.snap('off', 'player1', 'flags')
    t.expect.value('on', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.value('on', 'player2', 'flags', FL_FROZEN, op='!&')
    t.expect.value('off', 'player1', 'flags', FL_FROZEN, op='!&')


@S.test('trigger_playerfreeze Toggle: frozen, free, frozen; a world activator freezes nobody', players=1,
        doc=DOC_LOGIC + '#trigger_playerfreeze')
def freeze_toggle(t):
    t.point('trigger_playerfreeze', targetname='tg', use_type='0')
    for i in range(3):
        t.fire('tg', activator='player1')
        t.wait(0.2)
        t.snap('s%d' % i, 'player1', 'flags')
    t.fire('tg', activator='world')
    t.wait(0.2)
    t.snap('w', 'player1', 'flags')
    t.expect.value('s0', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.value('s1', 'player1', 'flags', FL_FROZEN, op='!&')
    t.expect.value('s2', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.value('w', 'player1', 'flags', FL_FROZEN, op='&')     # unchanged
    t.fire('tg', activator='player1')       # leave him free


@S.test('trigger_playerfreeze Copy input: On freezes, Off unfreezes, Toggle toggles (trigger_relay with each state)',
        players=1, doc=DOC_LOGIC + '#trigger_playerfreeze')
def freeze_copy(t):
    t.point('trigger_playerfreeze', targetname='cp', use_type='2')
    # Forward activator (64): the targets get the relay's activator, the player
    t.point('trigger_relay', targetname='r_on', target='cp', triggerstate='1', spawnflags='64')
    t.point('trigger_relay', targetname='r_off', target='cp', triggerstate='0', spawnflags='64')
    t.point('trigger_relay', targetname='r_tg', target='cp', triggerstate='2', spawnflags='64')
    for lab, relay in (('on', 'r_on'), ('on2', 'r_on'), ('off', 'r_off'), ('tg', 'r_tg'), ('tg2', 'r_tg')):
        t.fire(relay, activator='player1')
        t.wait(0.2)
        t.snap(lab, 'player1', 'flags')
    t.expect.value('on', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.value('on2', 'player1', 'flags', FL_FROZEN, op='&')    # On again: still frozen
    t.expect.value('off', 'player1', 'flags', FL_FROZEN, op='!&')
    t.expect.value('tg', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.value('tg2', 'player1', 'flags', FL_FROZEN, op='!&')


@S.test('trigger_playerfreeze All players (SBO): both players frozen, both unfrozen, toggled; fired by the world',
        players=2, doc=DOC_LOGIC + '#trigger_playerfreeze')
def freeze_all(t):
    t.point('trigger_playerfreeze', targetname='all_on', use_type='1', spawnflags='1')
    t.point('trigger_playerfreeze', targetname='all_off', use_type='-1', spawnflags='1')
    t.point('trigger_playerfreeze', targetname='all_tg', use_type='0', spawnflags='1')
    t.fire('all_on', activator='world')
    t.wait(0.3)
    t.snap('on', 'player1', 'flags')
    t.snap('on', 'player2', 'flags')
    t.fire('all_off', activator='world')
    t.wait(0.2)
    t.snap('off', 'player1', 'flags')
    t.snap('off', 'player2', 'flags')
    t.fire('all_tg', activator='player1')
    t.wait(0.2)
    t.snap('tg', 'player1', 'flags')
    t.snap('tg', 'player2', 'flags')
    t.fire('all_off')
    for p in ('player1', 'player2'):
        t.expect.value('on', p, 'flags', FL_FROZEN, op='&')
        t.expect.value('off', p, 'flags', FL_FROZEN, op='!&')
        t.expect.value('tg', p, 'flags', FL_FROZEN, op='&')


# ---------------------------------------------------------------------------------------------- trigger_killmonster

@S.test('trigger_killmonster by name: the named zombies die (death animation, corpse stays), another lives; '
        'Remove on fire: the entity is gone', doc=DOC_LOGIC + '#trigger_killmonster')
def kill_by_name(t):
    t.point('monster_zombie', at=(-120, 120, 0), targetname='zm')
    t.point('monster_zombie', at=(120, 120, 0), targetname='zm')
    t.point('monster_zombie', at=(0, -150, 0), targetname='other')
    t.point('trigger_killmonster', targetname='km', target='zm', spawnflags='1')
    t.wait(0.5)
    t.fire('km')
    t.wait(1.0)
    t.snap('after', 'zm', 'deadflag', 'health')
    t.snap('after', 'other', 'deadflag')
    t.expect.value('after', 'zm', 'deadflag', 0, op='>')
    t.expect.value('after', 'other', 'deadflag', 0)
    t.expect.removed('km')
    t.expect.damaged('zm', attacker='km', min=2)


@S.test('trigger_killmonster by class name, by squad name, by name and class together; without Remove on fire it '
        'stays and works again', doc=DOC_LOGIC + '#trigger_killmonster')
def kill_filters(t):
    t.point('monster_imp', at=(-160, 160, 0), targetname='ki_a')
    t.point('monster_zombie', at=(0, 160, 0), targetname='kz_a', netname='squad_k')
    t.point('monster_scientist', at=(160, 160, 0), targetname='ks_a', netname='squad_k')
    t.point('monster_scientist', at=(-160, -160, 0), targetname='mix')
    t.point('monster_zombie', at=(160, -160, 0), targetname='mix')
    t.point('trigger_killmonster', targetname='k_cls', message='monster_imp', spawnflags='0')
    t.point('trigger_killmonster', targetname='k_sq', netname='squad_k', message='monster_zombie', spawnflags='0')
    t.point('trigger_killmonster', targetname='k_mix', target='mix', message='monster_scientist', spawnflags='0')
    t.wait(0.5)
    t.fire('k_cls')
    t.fire('k_sq')
    t.fire('k_mix')
    t.wait(1.0)
    t.snap('a', 'ki_a', 'deadflag')
    t.snap('a', 'kz_a', 'deadflag')
    t.snap('a', 'ks_a', 'deadflag')
    t.snap('a', '@monster_scientist', 'deadflag', 'targetname')
    t.snap('a', '@monster_zombie', 'deadflag', 'targetname')
    t.fire('k_cls')
    t.wait(0.2)
    t.expect.value('a', 'ki_a', 'deadflag', 0, op='>')
    t.expect.value('a', 'kz_a', 'deadflag', 0, op='>')       # zombie in the squad
    t.expect.value('a', 'ks_a', 'deadflag', 0)                # scientist in the squad: not a zombie
    t.expect.damaged('mix', attacker='k_mix', count=1)        # only the scientist of the two "mix"
    t.expect.used('k_cls', count=2)
    t.expect.not_damaged('kz_a', attacker='k_cls')


@S.test('trigger_killmonster Always gib: the monster is gibbed (removed), no corpse', doc=DOC_LOGIC + '#trigger_killmonster')
def kill_gib(t):
    t.point('monster_zombie', at=(0, 120, 0), targetname='gz')
    t.point('trigger_killmonster', targetname='kg', target='gz', spawnflags='17')
    t.wait(0.5)
    t.fire('kg')
    t.wait(1.0)
    t.expect.removed('gz')
    t.expect.damaged('gz', attacker='kg')


# ---------------------------------------------------------------------------------------------- trigger_hurt_remote

@S.test('trigger_hurt_remote once: 30 damage to a func_breakable per trigger, exact; Off does nothing',
        doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_once(t):
    t.brush('func_breakable', targetname='crate', health='100', at=(0, 100, 0))
    t.point('trigger_hurt_remote', targetname='hr', target='crate', dmg='30', damagetype='0', spawnflags='0')
    t.fire('hr')
    t.wait(0.1)
    t.snap('one', 'crate', 'health')
    t.fire('hr', use='off')
    t.wait(0.1)
    t.snap('off', 'crate', 'health')
    t.fire('hr')
    t.wait(0.1)
    t.snap('two', 'crate', 'health')
    t.expect.value('one', 'crate', 'health', 70)
    t.expect.value('off', 'crate', 'health', 70)
    t.expect.value('two', 'crate', 'health', 40)


@S.test('trigger_hurt_remote Constant: a button switches it on (10 every 0.5 s, the presser the attacker), Off stops '
        'it; Start on: hurting since the map start', players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_constant(t):
    t.brush('func_breakable', targetname='crate', health='500', at=(0, 100, 0))
    t.point('trigger_hurt_remote', targetname='hr', target='crate', dmg='10', delay='0.5', spawnflags='2')
    t.brush('func_button', targetname='btn', target='hr', size=(16, 16, 16), at=(-150, 0, 40), spawnflags='1', wait='0')
    t.brush('func_breakable', targetname='crate_on', health='5000', at=(150, 100, 0))
    t.point('trigger_hurt_remote', targetname='hr_on', target='crate_on', dmg='1', delay='0.5', spawnflags='6')
    t.snap('begin', 'crate_on', 'health')
    t.snap('begin', 'crate', 'health')
    t.fire('btn', activator='player1')
    t.wait(1.2)
    t.fire('hr', use='off')
    t.wait(0.1)
    t.snap('stopped', 'crate', 'health')
    t.wait(1.0)
    t.snap('still', 'crate', 'health')
    t.fire('hr_on', use='off')
    t.expect.value('begin', 'crate', 'health', 500)
    t.expect.value('begin', 'crate_on', 'health', 5000, op='<')    # Start on: hurt since the map start
    t.expect.value('stopped', 'crate', 'health', 470, op='==', tol=0.01)
    t.expect.same('stopped', 'still', 'crate', 'health')
    t.expect.damaged('crate', attacker='player1', count=3)      # at 0, 0.5 and 1.0 s


@S.test('trigger_hurt_remote Target class name: every entity of the class (func_pushable) is hurt',
        doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_class(t):
    t.brush('func_pushable', targetname='pa', health='100', at=(-100, 100, 0), spawnflags='128')     # Breakable
    t.brush('func_pushable', targetname='pb', health='100', at=(100, 100, 0), spawnflags='128')
    t.point('trigger_hurt_remote', targetname='hr', targetclass='func_pushable', dmg='25', spawnflags='0')
    t.fire('hr')
    t.wait(0.1)
    t.snap('s', '@func_pushable', 'health')
    t.expect.value('s', 'pa', 'health', 75)
    t.expect.value('s', 'pb', 'health', 75)


@S.test('trigger_hurt_remote heals: negative damage +20 on !activator; Instant kill with negative damage: full health',
        players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_heal(t):
    t.point('trigger_hurt_remote', targetname='heal', target='!activator', dmg='-20', spawnflags='0')
    t.point('trigger_hurt_remote', targetname='full', target='!activator', dmg='-1', spawnflags='1')
    t.kv('player1', 'max_health', '100')
    t.kv('player1', 'health', '50')
    t.fire('heal', activator='player1')
    t.wait(0.1)
    t.snap('h', 'player1', 'health')
    t.fire('full', activator='player1')
    t.wait(0.1)
    t.snap('f', 'player1', 'health')
    t.expect.value('h', 'player1', 'health', 70)
    t.expect.value('f', 'player1', 'health', 100)


@S.test('trigger_hurt_remote armor: Do armor damage takes 20 armor (no health damage at 0), -30 gives armor up to 100',
        players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_armor(t):
    t.point('trigger_hurt_remote', targetname='drain', target='!activator', dmg='0', armordmg='20', spawnflags='8')
    t.point('trigger_hurt_remote', targetname='give', target='!activator', dmg='0', armordmg='-30', spawnflags='8')
    t.point('trigger_hurt_remote', targetname='noflag', target='!activator', dmg='0', armordmg='20', spawnflags='0')
    t.kv('player1', 'armorvalue', '50')
    t.fire('drain', activator='player1')
    t.wait(0.1)
    t.snap('d', 'player1', 'armorvalue')
    t.fire('noflag', activator='player1')
    t.wait(0.1)
    t.snap('n', 'player1', 'armorvalue')
    t.fire('give', activator='player1')
    t.fire('give', activator='player1')
    t.fire('give', activator='player1')
    t.wait(0.1)
    t.snap('g', 'player1', 'armorvalue')
    t.expect.value('d', 'player1', 'armorvalue', 30)
    t.expect.value('n', 'player1', 'armorvalue', 30)
    t.expect.value('g', 'player1', 'armorvalue', 100)


@S.test('trigger_hurt_remote Ignore armor: the armor stays, the health takes the damage; No camera punch: the view '
        'is not kicked', players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_ignore_armor(t):
    t.point('trigger_hurt_remote', targetname='ign', target='!activator', dmg='20', spawnflags='256')
    t.point('trigger_hurt_remote', targetname='plain', target='!activator', dmg='20', spawnflags='512')
    tough(t)
    t.kv('player1', 'armorvalue', '100')
    t.fire('ign', activator='player1')
    t.wait(0.05)
    t.snap('i', 'player1', 'armorvalue', 'health', 'kv:punchangle')
    t.wait(1.0)                              # the punch has worn off
    t.fire('plain', activator='player1')
    t.wait(0.1)
    t.snap('p', 'player1', 'armorvalue', 'health', 'kv:punchangle')
    t.expect.value('i', 'player1', 'armorvalue', 100)
    t.expect.value('i', 'player1', 'health', 995, op='<=')
    t.expect.value('i', 'player1', 'health', 960, op='>=')
    t.expect.value('i', 'player1', 'kv:punchangle.x', 0, op='<')         # kicked (CBasePlayer::TakeDamage)
    t.expect.value('p', 'player1', 'kv:punchangle', [0, 0, 0], tol=0.01)
    t.expect.damaged('player1', attacker='player1', min=2)     # his own activation: he is the attacker


@S.test('trigger_hurt_remote Minimum health 70: 50 damage takes the player to 70, a second hit leaves him at 70',
        players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_min_health(t):
    t.point('trigger_hurt_remote', targetname='hr', target='!activator', dmg='50', min_health='70', spawnflags='256')
    t.kv('player1', 'max_health', '100')
    t.kv('player1', 'health', '100')
    t.fire('hr', activator='player1')
    t.wait(0.1)
    t.snap('one', 'player1', 'health')
    t.fire('hr', activator='player1')
    t.wait(0.1)
    t.snap('two', 'player1', 'health')
    t.expect.value('one', 'player1', 'health', 70)
    t.expect.value('two', 'player1', 'health', 70)
    tough(t)


@S.test('trigger_hurt_remote Constant poison gas with Minimum health 25 (the showcase switch): health never goes '
        'below 25 and stops changing at Off (no lasting poison damage)', players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_min_health_poison(t):
    t.point('trigger_hurt_remote', targetname='gas', target='!activator', dmg='5', damagetype='131072', delay='0.2',
            min_health='25', spawnflags=str(2 | 512))
    t.kv('player1', 'max_health', '100')
    t.kv('player1', 'health', '40')
    t.kv('player1', 'armorvalue', '0')
    t.fire('gas', activator='player1')
    t.wait(1.5)
    t.snap('on', 'player1', 'health')
    t.fire('gas', activator='player1')          # toggle: off
    t.wait(0.1)
    t.snap('off', 'player1', 'health')
    t.wait(5.0)                                 # a poison hit would go on every 2 s for 10 s
    t.snap('later', 'player1', 'health')
    t.expect.value('on', 'player1', 'health', 25, op='>=')
    t.expect.value('on', 'player1', 'health', 40, op='<')
    t.expect.value('later', 'player1', 'health', 25, op='>=')
    t.expect.same('off', 'later', 'player1', 'health')
    tough(t)


@S.test('trigger_hurt_remote Instant kill: a zombie gibs (non-players always gib), "Never gib" leaves a corpse; the '
        'player who fired it is the attacker', players=1, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_instant(t):
    t.point('monster_zombie', at=(-120, 150, 0), targetname='z_gib')
    t.point('monster_zombie', at=(120, 150, 0), targetname='z_keep')
    t.point('trigger_hurt_remote', targetname='kg', target='z_gib', dmg='1', spawnflags='1')
    t.point('trigger_hurt_remote', targetname='kk', target='z_keep', dmg='1', spawnflags='1', gib_policy='2')
    t.wait(0.5)
    t.fire('kg', activator='player1')
    t.fire('kk', activator='player1')
    t.wait(1.0)
    t.snap('k', 'z_keep', 'deadflag')
    t.expect.removed('z_gib')
    t.expect.value('k', 'z_keep', 'deadflag', 0, op='>')
    t.expect.damaged('z_gib', attacker='player1')
    t.expect.damaged('z_keep', attacker='player1')


@S.test('trigger_hurt_remote corpses: without "Do damage to dead" a corpse is left alone; with it a CLUB hit gibs it, '
        'and so does an Instant kill', doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_corpse(t):
    t.point('monster_zombie', at=(-100, 150, 0), targetname='zc')
    t.point('monster_zombie', at=(100, 150, 0), targetname='zi')
    t.point('trigger_killmonster', targetname='km', message='monster_zombie', spawnflags='0')
    t.point('trigger_hurt_remote', targetname='alive_only', target='zc', dmg='200', damagetype='128', spawnflags='0')
    t.point('trigger_hurt_remote', targetname='dead_too', target='zc', dmg='200', damagetype='128', spawnflags='16')
    t.point('trigger_hurt_remote', targetname='dead_kill', target='zi', dmg='1', spawnflags='17')
    t.wait(0.5)
    t.fire('km')
    t.wait(2.5)
    t.mark('dead')
    t.fire('alive_only')
    t.wait(0.2)
    t.mark('then')
    t.fire('dead_too')
    t.fire('dead_kill')
    t.wait(0.5)
    t.expect.not_damaged('zc', attacker='alive_only')
    t.expect.damaged('zc', attacker='dead_too', after='then')
    t.expect.removed('zc')
    t.expect.removed('zi')


@S.test('trigger_hurt_remote fired by player1 on player2: the entity is the attacker (friendly fire rules), '
        'player2 is hurt', players=2, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_other_player(t):
    t.point('trigger_hurt_remote', targetname='hr', targetclass='player', dmg='10', spawnflags='256')
    tough(t, 'player1')
    tough(t, 'player2')
    t.fire('hr', activator='player1')
    t.wait(0.1)
    t.expect.damaged('player2', attacker='hr', count=1)
    t.expect.damaged('player1', attacker='player1', count=1)


@S.test('trigger_hurt_remote kills in the kill feed: weapon "trigger_hurt_remote" (the entity is the inflictor) for a '
        'player killed by Instant kill and by damage, and for NPCs (SboDeathMsg, the activator the killer); a hit '
        'that does not kill does not push the player', players=2, doc=DOC_LOGIC + '#trigger_hurt_remote')
def hurt_killfeed(t):
    t.console('mp_friendlyfire 1')
    t.console('mp_killfeed_npc 1')
    t.point('trigger_hurt_remote', targetname='self_kill', target='!activator', dmg='1', spawnflags='1')
    t.point('trigger_hurt_remote', targetname='p2_hurt', targetclass='player', dmg='50', spawnflags='256')
    t.point('trigger_hurt_remote', targetname='p1_hit', target='!activator', dmg='20', spawnflags='256')
    t.point('monster_barney', at=(-120, 150, 0), targetname='zk')     # not a zombie: hurt_corpse kills them all
    t.point('monster_barney', at=(120, 150, 0), targetname='zd')
    t.point('trigger_hurt_remote', targetname='zk_kill', target='zk', dmg='1', spawnflags='1')
    t.point('trigger_hurt_remote', targetname='zd_hurt', target='zd', dmg='500', spawnflags='0')
    t.wait(0.5)
    tough(t, 'player1')
    t.kv('player1', 'velocity', '0 0 0')
    t.fire('p1_hit', activator='player1')
    t.wait(0.05)
    t.snap('hit', 'player1', 'health', 'velocity')
    t.flags('player1', '+FL_NOTARGET')
    t.mark('npc')
    t.fire('zk_kill', activator='player1')
    t.fire('zd_hurt', activator='player1')
    t.wait(0.5)
    t.kv('player2', 'health', '1')
    t.mark('p2')
    t.fire('p2_hurt', activator='player1')      # player1 (tough) and player2: the entity is the attacker
    t.wait(0.5)
    t.console('test_round restart')             # player2 alive again before player1 dies (not both dead)
    t.console('wait_for alive player2 20')
    t.wait(2.5)                                 # past the 2 s spawn protection (CCoop::PlayerSpawn)
    t.kv('player1', 'max_health', '100')
    t.kv('player1', 'health', '100')
    t.mark('self')
    t.fire('self_kill', activator='player1')
    t.wait(0.5)
    t.console('test_round restart')
    t.console('wait_for alive player1 20')
    t.console('wait_for alive player2 20')
    t.wait(2.5)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('hit', 'player1', 'health', 1000, op='<')
    t.expect.value('hit', 'player1', 'velocity', [0, 0, 0], tol=0.01)       # no push
    t.expect.msg('SboDeathMsg', fields={'killer': 1, 'victim': 0, 'weapon': 'trigger_hurt_remote'}, after='npc',
                 count=2)
    t.expect.msg('DeathMsg', fields={'killer': 0, 'victim': 2, 'weapon': 'trigger_hurt_remote'}, after='p2', count=1)
    t.expect.msg('DeathMsg', fields={'killer': 1, 'victim': 1, 'weapon': 'trigger_hurt_remote'}, after='self',
                 count=1)


# ---------------------------------------------------------------------------------------------- trigger_look

def look_room(t, **kv):
    """A trigger_look brush where player1 stands, the look target 200 units north, a hit target."""
    t.point('info_target', at=(0, 200, 64), targetname='spot')
    t.point('info_target', at=(-200, -200, 32), targetname='hit')
    keys = dict(netname='spot', target='hit', look_time='0.5', field_of_view='0.7', spawnflags='0')
    keys.update(kv)
    t.trigger('trigger_look', size=(160, 160, 128), at=(0, 0, 0), targetname='lk', **keys)


@S.test('trigger_look: looking at the target for 0.5 s fires it, the player as activator; looking away never does',
        players=1, doc=DOC_LOGIC + '#trigger_look')
def look_basic(t):
    look_room(t)
    t.teleport('player1', (0, 0, 0), yaw=270)
    t.look('player1', 0, 270)                # away
    t.wait(1.0)
    t.mark('look')
    t.look('player1', 0, 90)                 # at it
    t.wait(1.2)
    t.teleport('player1', (-200, 0, 0))
    t.expect.not_fired('hit', within=(0, 1.0))
    t.expect.fired('hit', activator='player1', caller='lk', min=1, after='look', within=(0.4, 1.2))


@S.test('trigger_look Field of view: 60 degrees off fails with 0.7 (about 45), fires with 0.0 (90 degrees)',
        players=1, doc=DOC_LOGIC + '#trigger_look')
def look_fov(t):
    look_room(t, field_of_view='0.7', target='hit')
    t.point('info_target', at=(200, -200, 32), targetname='hit90')
    t.trigger('trigger_look', size=(160, 160, 128), at=(0, 0, 0), targetname='lk90', netname='spot', target='hit90',
              look_time='0.5', field_of_view='0.0', spawnflags='0')
    t.teleport('player1', (0, 0, 0), yaw=150)
    t.look('player1', 0, 150)
    t.wait(1.5)
    t.teleport('player1', (-200, 0, 0))
    t.expect.not_fired('hit')
    t.expect.fired('hit90', activator='player1', min=1)


@S.test('trigger_look Remove on fire: fires once and is removed; without it: again every look time',
        players=1, doc=DOC_LOGIC + '#trigger_look')
def look_once(t):
    look_room(t, spawnflags='1')
    t.point('info_target', at=(200, -200, 32), targetname='hit2')
    t.trigger('trigger_look', size=(160, 160, 128), at=(0, 0, 0), targetname='lk2', netname='spot', target='hit2',
              look_time='0.5', field_of_view='0.7', spawnflags='0')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(2.0)
    t.teleport('player1', (-200, 0, 0))
    t.expect.fired('hit', count=1)
    t.expect.removed('lk')
    t.expect.fired('hit2', min=3)


@S.test('trigger_look Master: nothing while the multisource is off; a relay turns it on: fires; Delay 0.5 and Kill '
        'target', players=1, doc=DOC_LOGIC + '#trigger_look')
def look_master(t):
    t.point('multisource', targetname='ms')
    t.point('trigger_relay', targetname='unlock', target='ms', triggerstate='1')
    t.point('info_target', at=(100, -200, 32), targetname='victim')
    look_room(t, master='ms', delay='0.5', killtarget='victim')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.2)
    t.mark('unlock')
    t.fire('unlock')
    t.wait(1.6)
    t.teleport('player1', (-200, 0, 0))
    t.expect.not_fired('hit', within=(0, 1.2))
    t.expect.fired('hit', activator='player1', min=1, after='unlock', within=(0.9, 1.6))
    t.expect.removed('victim')


@S.test('trigger_look per player: player2 in the brush looking away does not reset player1\'s time (SBO)',
        players=2, doc=DOC_LOGIC + '#trigger_look')
def look_two_players(t):
    look_room(t, look_time='1.0')
    t.teleport('player1', (-40, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.teleport('player2', (40, 0, 0), yaw=270)
    t.look('player2', 0, 270)
    t.wait(1.6)
    t.teleport('player1', (-200, 0, 0))
    t.teleport('player2', (200, 0, 0))
    t.expect.fired('hit', activator='player1', min=1)
    t.expect.fired('hit', activator='player2', count=0)


# ---------------------------------------------------------------------------------------------- env_dlight

@S.test('env_dlight: a trigger_relay switches it on (KeyedDLight on: radius, color, its position) and off',
        doc=DOC_DLIGHT)
def dlight_toggle(t):
    t.point('env_dlight', at=(0, 0, 64), targetname='dl', renderamt='200', rendercolor='255 40 10', spawnflags='0')
    t.point('trigger_relay', targetname='r_on', target='dl', triggerstate='1')
    t.point('trigger_relay', targetname='r_off', target='dl', triggerstate='0')
    t.fire('r_on')
    t.wait(0.2)
    t.mark('on')
    t.fire('r_on')                           # On again: nothing
    t.wait(0.2)
    t.mark('off')
    t.fire('r_off')
    t.wait(0.2)
    t.expect.msg('KeyedDLight', count=1, fields={'state': 1, 'radius': 200, 'r': 255, 'g': 40, 'b': 10, 'follow': 0},
                 within=(0, 0.2))
    t.expect.msg('KeyedDLight', count=0, after='on', within=(0, 0.2))
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0}, after='off')


@S.test('env_dlight Start on (named): it is on from the map start: the first trigger switches it off', doc=DOC_DLIGHT)
def dlight_start_on(t):
    t.point('env_dlight', at=(0, 0, 64), targetname='dl', renderamt='100', spawnflags='2')
    t.fire('dl')
    t.wait(0.2)
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0})
    t.expect.msg('KeyedDLight', count=0, fields={'state': 1})


@S.test('env_dlight Duration 0.5 + Only once: off 0.5 s after a func_button turns it on, then removed', players=1,
        doc=DOC_DLIGHT)
def dlight_duration(t):
    t.point('env_dlight', at=(0, 0, 64), targetname='dl', renderamt='150', health='0.5', spawnflags='1')
    t.brush('func_button', targetname='btn', target='dl', size=(16, 16, 16), at=(-150, 0, 40), spawnflags='1', wait='0')
    t.mark('press')
    t.fire('btn', activator='player1')
    t.wait(1.0)
    t.expect.msg('KeyedDLight', count=1, fields={'state': 1}, after='press', within=(0, 0.2))
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0}, after='press', within=(0.4, 0.8))
    t.expect.removed('dl')


@S.test('env_dlight Position is Locus Entity: on a func_wall (drawn: the client follows it, its index sent); on an '
        'info_target (no model): its origin, nothing to follow', doc=DOC_DLIGHT)
def dlight_entity(t):
    t.brush('func_wall', targetname='wall', size=(32, 32, 32), at=(100, 100, 0))
    t.point('info_target', at=(-100, 100, 50), targetname='tgt')
    t.point('env_dlight', at=(0, 0, 64), targetname='dl_w', message='wall', renderamt='100', spawnflags='8',
            names=('message',))
    t.point('env_dlight', at=(0, 0, 64), targetname='dl_t', message='tgt', renderamt='120', spawnflags='8',
            names=('message',))
    t.fire('dl_w')
    t.wait(0.1)
    t.mark('t')
    t.fire('dl_t')
    t.wait(0.1)
    t.expect.msg('KeyedDLight', count=1, fields={'state': 1, 'radius': 100}, within=(0, 0.1))
    t.expect.msg('KeyedDLight', count=0, fields={'state': 1, 'radius': 100, 'follow': 0}, within=(0, 0.1))
    t.expect.msg('KeyedDLight', count=1, fields={'state': 1, 'radius': 120, 'follow': 0}, after='t')


@S.test('env_dlight Track position updates on !activator (a position): the light moves with the player (position '
        'messages), none while he stands still', players=1, doc=DOC_DLIGHT)
def dlight_track(t):
    t.point('env_dlight', at=(0, 0, 64), targetname='dl', message='!activator', renderamt='100', spawnflags='4')
    t.teleport('player1', (-100, 0, 0))
    t.wait(0.3)
    t.fire('dl', activator='player1')
    t.wait(0.5)
    t.mark('still')
    t.wait(0.5)
    t.mark('move')
    t.teleport('player1', (100, 0, 0))
    t.wait(0.5)
    t.fire('dl', use='off')
    t.expect.msg('KeyedDLight', count=1, fields={'state': 1})
    t.expect.msg('KeyedDLight', count=0, fields={'state': 2}, after='still', within=(0, 0.45))
    t.expect.msg('KeyedDLight', min=1, fields={'state': 2}, after='move', within=(0, 0.5))


# ---------------------------------------------------------------------------------------------- locus_beam

@S.test('locus_beam Entity & Entity with !activator: a beam from an info_target to the activator, named, its target '
        'fired with the beam as activator', players=1, doc=DOC_LOCUS + '#locus_beam')
def beam_ents(t):
    t.point('info_target', at=(0, 150, 64), targetname='src')
    t.point('info_target', at=(-200, -200, 32), targetname='after')
    t.point('locus_beam', targetname='lb', m_iszStart='src', m_iszEnd='!activator', impulse='0', m_iWidth='20',
            renderamt='200', rendercolor='0 200 255', m_iszTargetName='bm', target='after', m_fDuration='0.5',
            names=('m_iszStart', 'm_iszEnd'))
    t.fire('lb', activator='player1')
    t.wait(0.1)
    t.snap('b', 'bm', 'skin', 'sequence', 'scale', 'renderamt')
    t.wait(0.8)
    t.expect.spawned('bm', count=1)
    t.expect.fired('after', activator='bm', caller='lb', count=1)
    t.expect.value('b', 'bm', 'skin', 1)            # end entity: player1 (index 1)
    t.expect.value('b', 'bm', 'scale', 20)          # width
    t.expect.value('b', 'bm', 'renderamt', 200)
    t.expect.removed('bm')                          # Duration 0.5


@S.test('locus_beam Entity & Entity between two info_targets and to the activator: point entities are positions '
        '(the clients never get them), as env_beam: points / point-entity beams that the clients draw', players=1,
        doc=DOC_LOCUS + '#locus_beam')
def beam_point_entities(t):
    t.point('info_target', at=(-150, 150, 64), targetname='pa')
    t.point('info_target', at=(150, 150, 64), targetname='pb')
    t.point('locus_beam', targetname='lpp', m_iszStart='pa', m_iszEnd='pb', impulse='0', m_iszTargetName='bpp',
            m_fDuration='0.5', names=('m_iszStart', 'm_iszEnd'))
    t.point('locus_beam', targetname='lme', m_iszStart='pa', m_iszEnd='!activator', impulse='0',
            m_iszTargetName='bme', m_fDuration='0.5', spawnflags='32', names=('m_iszStart',))
    t.fire('lpp')
    t.fire('lme', activator='player1')
    t.wait(0.1)
    t.snap('b', 'bpp', 'rendermode', 'origin', 'angles')
    t.snap('b', 'bme', 'rendermode', 'origin', 'skin')
    t.snap('b', 'pa', 'origin')
    t.snap('b', 'pb', 'origin')
    t.expect.spawned('bpp', count=1)
    t.expect.spawned('bme', count=1)
    t.expect.value('b', 'bpp', 'rendermode', 0, op='==')    # BEAM_POINTS (rendermode's low bits: the beam type; no flags)
    t.expect.value('b', 'bme', 'rendermode', 1, op='==')    # BEAM_ENTPOINT: from the info_target to player1
    t.expect.value('b', 'bme', 'skin', 1)
    t.expect.distance('b', 'bpp', 'pa', 0, 1)               # start position: the first info_target
    t.expect.distance('b', 'bme', 'pa', 0, 1)
    t.wait(0.6)
    t.expect.removed('bpp')
    t.expect.removed('bme')


@S.test('locus_beam Position & Position from calc_position (an entity\'s origin + offset) and Position & Direction',
        doc=DOC_LOCUS + '#locus_beam')
def beam_points(t):
    t.point('info_target', at=(-100, 100, 32), targetname='anchor')
    t.point('calc_position', targetname='cp', netname='anchor', impulse='0', message='0 0 48', names=('netname',))
    t.point('locus_beam', targetname='lpp', m_iszStart='cp', m_iszEnd='100 100 80', impulse='2',
            m_iszTargetName='bpp', names=('m_iszStart',))
    t.point('locus_beam', targetname='lpd', m_iszStart='cp', m_iszEnd='0 0 100', impulse='3', m_iszTargetName='bpd',
            names=('m_iszStart',))
    t.fire('lpp')
    t.fire('lpd')
    t.wait(0.1)
    t.snap('p', 'anchor', 'origin')
    t.snap('p', 'bpp', 'origin', 'angles')
    t.snap('p', 'bpd', 'origin', 'angles')
    t.expect.spawned('bpp', count=1)
    t.expect.spawned('bpd', count=1)
    # positions are world coordinates: compare the beam with the anchor (calc_position adds 0 0 48)
    t.expect.distance('p', 'bpp', 'anchor', 0, 1)
    t.expect.distance('p', 'bpd', 'anchor', 0, 1)


@S.test('locus_beam Damage 100 per second, Damage type Shock: a func_breakable in the beam takes about 100 a second '
        'from the activator and breaks; Decal end and End sparks: sparks at the end', players=1,
        doc=DOC_LOCUS + '#locus_beam')
def beam_damage(t):
    t.brush('func_breakable', targetname='crate', health='90', at=(0, 100, 0), size=(48, 48, 96))
    t.point('info_target', at=(-200, 100, 48), targetname='b_a')
    t.point('info_target', at=(200, 100, 48), targetname='b_b')
    t.point('locus_beam', targetname='lb', m_iszStart='b_a', m_iszEnd='b_b', impulse='0', m_fDamage='100',
            m_iDamageType='256', m_iszTargetName='bd', m_fDuration='1.4', spawnflags='96',
            names=('m_iszStart', 'm_iszEnd'))
    t.fire('lb', activator='player1')
    t.wait(0.5)
    t.snap('half', 'crate', 'health')
    t.wait(1.1)
    t.expect.damaged('crate', attacker='player1', min=3)
    t.expect.value('half', 'crate', 'health', 70, op='<=')
    t.expect.value('half', 'crate', 'health', 30, op='>=')
    t.expect.removed('crate')
    t.expect.msg('svc:23', min=3, fields={'te': TE_SPARKS})


@S.test('locus_beam without damage, sparks or decals: a plain beam (no think, no sparks); Start sparks alone: sparks',
        doc=DOC_LOCUS + '#locus_beam')
def beam_plain_vs_sparks(t):
    t.point('locus_beam', targetname='plain', m_iszStart='-100 0 48', m_iszEnd='100 0 48', impulse='2',
            m_iszTargetName='bplain')
    t.point('locus_beam', targetname='spk', m_iszStart='-100 50 48', m_iszEnd='100 50 48', impulse='2',
            m_iszTargetName='bspk', spawnflags='16')
    t.fire('plain')
    t.wait(0.5)
    t.mark('s')
    t.fire('spk')
    t.wait(0.5)
    t.snap('x', 'bplain', 'nextthink')
    t.expect.msg('svc:23', count=0, fields={'te': TE_SPARKS}, within=(0, 0.5))
    t.expect.msg('svc:23', min=2, fields={'te': TE_SPARKS}, after='s')
    t.expect.value('x', 'bplain', 'nextthink', 0)


@S.test('locus_beam: a sprite name changed after the load (trigger_changevalue) is not precached late: the beam uses '
        'the map-load sprite; trigger_hurt_remote Delay set to -1 hurts every 0.1 s, not every frame',
        doc=DOC_LOCUS + '#locus_beam')
def runtime_keys(t):
    t.point('locus_beam', targetname='lb', m_iszStart='-100 0 48', m_iszEnd='100 0 48', impulse='2',
            m_iszTargetName='brt', m_fDuration='0.5')
    t.point('trigger_changevalue', targetname='cv', target='lb', m_iszValueName='m_iszSprite',
            m_iszNewValue='sprites/not_precached_xyz.spr', m_iszValueType='0')
    t.brush('func_breakable', targetname='crate', health='5000', at=(0, 100, 0))
    t.point('trigger_hurt_remote', targetname='hr', target='crate', dmg='1', delay='1', spawnflags='2')
    t.point('trigger_changevalue', targetname='cvd', target='hr', m_iszValueName='delay', m_iszNewValue='-1',
            m_iszValueType='0')
    t.fire('cv')
    t.wait(0.1)
    t.fire('lb')
    t.wait(0.2)
    t.fire('cvd')
    t.fire('hr', use='on')
    t.mark('on')
    t.wait(1.0)
    t.fire('hr', use='off')
    t.expect.spawned('brt', count=1)
    t.expect.not_precached('sprites/not_precached_xyz.spr')
    t.expect.damaged('crate', after='on', total=(8, 13))     # about 10 in 1 s, not one per frame


# ---------------------------------------------------------------------------------------------- player_calc_ratio

@S.test('player_calc_ratio health and armor, absolute and fraction (read through locus_variable)', players=1,
        doc=DOC_LOCUS + '#player_calc_ratio')
def calc_health_armor(t):
    for n, p, v in (('ph', '0', '0'), ('phf', '0', '1'), ('pa', '1', '0'), ('paf', '1', '1')):
        t.point('player_calc_ratio', targetname=n, calc_param=p, value_notion=v)
        lv(t, 'lv_' + n, n)
    t.kv('player1', 'max_health', '200')
    t.kv('player1', 'health', '150')
    t.kv('player1', 'armorvalue', '40')
    for n in ('ph', 'phf', 'pa', 'paf'):
        t.fire('lv_' + n, activator='player1')
    t.wait(0.1)
    for n in ('ph', 'phf', 'pa', 'paf'):
        t.snap('v', 'lv_' + n, 'frags')
    t.expect.value('v', 'lv_ph', 'frags', 150)
    t.expect.value('v', 'lv_phf', 'frags', 0.75)
    t.expect.value('v', 'lv_pa', 'frags', 40)
    t.expect.value('v', 'lv_paf', 'frags', 0.4)
    tough(t)


@S.test('player_calc_ratio ammo of the loadout weapon (fa18m): reserve, with the clip (more), fractions in 0..1; '
        'maximum speed', players=1, doc=DOC_LOCUS + '#player_calc_ratio')
def calc_ammo_speed(t):
    for n, p, v in (('am', '2', '0'), ('amc', '3', '0'), ('amf', '2', '1'), ('amcf', '3', '1')):
        t.point('player_calc_ratio', targetname=n, calc_param=p, value_notion=v, ammo_name='fa18m')
        lv(t, 'lv_' + n, n)
    t.point('player_calc_ratio', targetname='sp', calc_param='5', value_notion='0')
    lv(t, 'lv_sp', 'sp')
    for n in ('am', 'amc', 'amf', 'amcf', 'sp'):
        t.fire('lv_' + n, activator='player1')
    t.wait(0.1)
    for n in ('am', 'amc', 'amf', 'amcf', 'sp'):
        t.snap('v', 'lv_' + n, 'frags')
    t.snap('v', 'player1', 'kv:maxspeed')
    t.expect.value('v', 'lv_am', 'frags', 0, op='>')
    t.expect.value('v', 'lv_amc', 'frags', 0, op='>')
    t.expect.value('v', 'lv_amf', 'frags', 0, op='>')
    t.expect.value('v', 'lv_amf', 'frags', 1, op='<=')
    t.expect.value('v', 'lv_amcf', 'frags', 1, op='<=')
    t.expect.value('v', 'lv_sp', 'frags', 0, op='>')


@S.test('player_calc_ratio fails without a player in play: the world as activator, no ammo name, the inventory type',
        players=1, doc=DOC_LOCUS + '#player_calc_ratio')
def calc_fail(t):
    t.point('player_calc_ratio', targetname='ph', calc_param='0', value_notion='0')
    t.point('player_calc_ratio', targetname='noammo', calc_param='2', value_notion='0')
    t.point('player_calc_ratio', targetname='inv', calc_param='4', value_notion='0')
    lv(t, 'lv_w', 'ph')
    lv(t, 'lv_na', 'noammo')
    lv(t, 'lv_inv', 'inv')
    t.fire('lv_w', activator='world')
    t.fire('lv_na', activator='player1')
    t.fire('lv_inv', activator='player1')
    t.wait(0.1)
    for n in ('lv_w', 'lv_na', 'lv_inv'):
        t.snap('v', n, 'frags')
        t.expect.value('v', n, 'frags', 0)


# ---------------------------------------------------------------------------------------------- spectators (last)

@S.test('spectators are not players: All players freeze, hurt by class "player", trigger_look and player_calc_ratio '
        'leave player2 alone once he spectates', players=2, doc=DOC_LOGIC)
def spectator(t):
    t.point('trigger_playerfreeze', targetname='frz', use_type='1', spawnflags='1')
    t.point('trigger_playerfreeze', targetname='unfrz', use_type='-1', spawnflags='1')
    t.point('trigger_hurt_remote', targetname='hr', targetclass='player', dmg='5', spawnflags='256')
    t.point('player_calc_ratio', targetname='ph', calc_param='0', value_notion='0')
    lv(t, 'lv', 'ph')
    look_room(t)
    tough(t, 'player1')
    tough(t, 'player2')
    t.teleport('player1', (-200, 0, 0))
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.mark('spec')
    t.teleport('player2', (0, 0, 0), yaw=90)
    t.look('player2', 0, 90)
    t.fire('frz', activator='world')
    t.fire('hr', activator='world')
    t.fire('lv', activator='player2')
    t.wait(1.0)
    t.snap('s', 'player2', 'flags')
    t.snap('s', 'player1', 'flags')
    t.snap('s', 'lv', 'frags')
    t.fire('unfrz', activator='world')
    t.expect.value('s', 'player2', 'flags', FL_FROZEN, op='!&')
    t.expect.value('s', 'player1', 'flags', FL_FROZEN, op='&')
    t.expect.not_damaged('player2', after='spec')
    t.expect.damaged('player1', attacker='world', after='spec', count=1)     # the activator (the world)
    t.expect.fired('hit', activator='player2', count=0)
    t.expect.value('s', 'lv', 'frags', 0)
