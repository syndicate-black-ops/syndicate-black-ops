"""Touch triggers on a real server (GAPS.md tier 1 "trigger_once / multiple", "Masters on the classes"): trigger_once
and trigger_multiple (flags 1 Monsters, 2 No Clients, 4 Pushables, 8 Everything else; delay, killtarget, master,
message), trigger_hurt (dmg, damagetype, flags 1 / 2 / 8 / 16 / 32, delay, master) and trigger_teleport (master,
flags 1 / 2, landmark). What each checks is the FGD's description (sdk/syndicate.fgd, bases Trigger, Targetx,
Master).

Monsters, pushables and grenades touch a trigger only when they move (the engine links them with touches only in
their physics): each is teleported above its trigger and let fall into it (FL_ONGROUND cleared)."""

from etest import Suite

SUITE = Suite('triggers', 'Touch triggers: trigger_once / multiple / hurt / teleport (flags, delay, killtarget, master)')
S = SUITE

DMG_BURN = 8
TRIG = dict(size=(96, 96, 72))


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


def drop(t, ent, at, height=120):
    """Lets an entity fall into a trigger at `at` from `height` above the floor."""
    t.teleport(ent, (at[0], at[1], height))
    t.flags(ent, '-FL_ONGROUND')


def lock(t, name='lock', key='key'):
    """A multisource `name` that is locked until `key` (its only member, a trigger_relay) is fired."""
    t.point('multisource', targetname=name)
    t.point('trigger_relay', targetname=key, target=name, triggerstate='1')


# ------------------------------------------------------------------------------------------------ trigger_once

@S.test('trigger_once: a player walking in fires its target once (player as activator), then it is removed', players=1,
        covers=('trigger_once', 'trigger_once.target'))
def once_player(t):
    t.trigger('trigger_once', at=(0, 120, 0), targetname='once', target='out', **TRIG)
    info(t, 'out')
    t.wait(1.0)     # first test: touches and +use right after the suite start were not taken (seen 2026-11-07)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.6)
    t.teleport('player1', (0, -120, 0))
    t.wait(0.2)
    t.teleport('player1', (0, 120, 0))
    t.wait(0.4)
    t.expect.fired('out', activator='player1', count=1, after='in', within=(0, 0.2))
    t.expect.removed('once', count=1, after='in', within=(0, 0.4))


@S.test('trigger_once: delay: the target fires "delay" seconds after the touch', players=1,
        covers=('trigger_once.delay',))
def once_delay(t):
    t.trigger('trigger_once', at=(0, 120, 0), target='out', delay='0.8', **TRIG)
    info(t, 'out')
    t.wait(0.2)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(1.3)
    t.expect.fired('out', activator='player1', count=1, after='in', within=(0.7, 1.0))


@S.test('trigger_once: killtarget removes the named entity when it fires (after its delay)', players=1,
        covers=('trigger_once.killtarget',))
def once_killtarget(t):
    t.trigger('trigger_once', at=(0, 120, 0), target='out', killtarget='victim', delay='0.6', **TRIG)
    info(t, 'out')
    info(t, 'victim', at=(100, -100, 0))
    t.wait(0.2)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(1.0)
    t.expect.not_removed('victim', after='in', within=(0, 0.5))
    t.expect.removed('victim', count=1, after='in', within=(0.5, 0.8))
    t.expect.fired('out', count=1, after='in', within=(0.5, 0.8))


@S.test('trigger_once: message is shown to the player who touched it', players=1, covers=('trigger_once.message',))
def once_message(t):
    t.trigger('trigger_once', at=(0, 120, 0), target='out', message='ET_ONCE_MESSAGE', **TRIG)
    info(t, 'out')
    t.wait(0.2)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.4)
    t.expect.msg('HudText', to='player1', fields={'text': 'ET_ONCE_MESSAGE'}, count=1, after='in')


@S.test('trigger_once: master: nothing while the multisource is locked; it fires once the master is unlocked', players=1,
        covers=('trigger_once.master',))
def once_master(t):
    lock(t)
    t.trigger('trigger_once', at=(0, 120, 0), target='out', master='lock', **TRIG)
    info(t, 'out')
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))       # stands in it: touches it every frame
    t.wait(0.6)
    t.mark('unlock')
    t.fire('key')
    t.wait(0.5)
    t.expect.not_fired('out', after='in', within=(0, 0.6))
    t.expect.fired('out', activator='player1', count=1, after='unlock', within=(0, 0.2))


@S.test('trigger_once: an inverted master (~name) fires while the multisource is locked', players=1,
        covers=('trigger_once.master',))
def once_master_inverted(t):
    lock(t)
    t.trigger('trigger_once', at=(0, 120, 0), target='out', master='~lock', **TRIG)
    info(t, 'out')
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.4)
    t.expect.fired('out', activator='player1', count=1, after='in', within=(0, 0.2))


@S.test('trigger_once flag 1 Monsters: a monster falling into it fires it (monster as activator); without the flag it '
        'does not', covers=('trigger_once:1',))
def once_monsters(t):
    t.trigger('trigger_once', at=(-120, 100, 0), target='out_m', spawnflags='1', **TRIG)
    t.trigger('trigger_once', at=(120, 100, 0), target='out_n', **TRIG)
    info(t, 'out_m')
    info(t, 'out_n')
    t.point('monster_scientist', targetname='sci_m', at=(-120, -150, 0))
    t.point('monster_scientist', targetname='sci_n', at=(120, -150, 0))
    t.wait(0.5)
    t.mark('drop')
    drop(t, 'sci_m', (-120, 100))
    drop(t, 'sci_n', (120, 100))
    t.wait(1.0)
    t.expect.fired('out_m', activator='sci_m', count=1, after='drop')
    t.expect.not_fired('out_n', after='drop')


@S.test('trigger_once flag 2 No Clients: a player standing in it does not fire it; a monster does (flags 1 + 2)',
        players=1, covers=('trigger_once:2',))
def once_noclients(t):
    t.trigger('trigger_once', at=(-120, 100, 0), target='out_p', spawnflags='2', **TRIG)
    t.trigger('trigger_once', at=(120, 100, 0), target='out_m', spawnflags='3', **TRIG)
    info(t, 'out_p')
    info(t, 'out_m')
    t.point('monster_scientist', targetname='sci', at=(120, -150, 0))
    t.wait(0.5)
    t.mark('in')
    t.teleport('player1', (-120, 100, 0))
    drop(t, 'sci', (120, 100))
    t.wait(1.0)
    t.expect.not_fired('out_p', after='in')
    t.expect.fired('out_m', activator='sci', count=1, after='in')


@S.test('trigger_once flag 4 Pushables: a func_pushable falling into it fires it; without the flag it does not',
        covers=('trigger_once:4',))
def once_pushables(t):
    t.trigger('trigger_once', at=(-120, 100, 0), target='out_p', spawnflags='4', **TRIG)
    t.trigger('trigger_once', at=(120, 100, 0), target='out_n', **TRIG)
    info(t, 'out_p')
    info(t, 'out_n')
    t.brush('func_pushable', size=(32, 32, 32), at=(-120, -150, 0), origin=True, targetname='box_p', spawnflags='128')
    t.brush('func_pushable', size=(32, 32, 32), at=(120, -150, 0), origin=True, targetname='box_n', spawnflags='128')
    t.wait(0.5)
    t.mark('drop')
    drop(t, 'box_p', (-120, 100), 140)
    drop(t, 'box_n', (120, 100), 140)
    t.wait(1.0)
    t.snap('after', 'box_p', 'origin')
    t.expect.fired('out_p', count=1, after='drop')
    t.expect.not_fired('out_n', after='drop')


@S.test('trigger_once flag 8 Everything else: a falling grenade fires it; without the flag it does not',
        covers=('trigger_once:8',))
def once_everything(t):
    t.trigger('trigger_once', at=(-120, 100, 0), target='out_e', spawnflags='8', **TRIG)
    t.trigger('trigger_once', at=(120, 100, 0), target='out_n', **TRIG)
    info(t, 'out_e')
    info(t, 'out_n')
    t.point('grenade', targetname='gren_e', at=(-120, -150, 0))
    t.point('grenade', targetname='gren_n', at=(120, -150, 0))
    t.wait(0.5)
    t.mark('drop')
    drop(t, 'gren_e', (-120, 100))
    drop(t, 'gren_n', (120, 100))
    t.wait(1.0)
    t.expect.fired('out_e', count=1, after='drop')
    t.expect.not_fired('out_n', after='drop')


# ------------------------------------------------------------------------------------------------ trigger_multiple

@S.test('trigger_multiple: delay and killtarget on every activation; master locks it until unlocked', players=1,
        covers=('trigger_multiple.delay', 'trigger_multiple.killtarget', 'trigger_multiple.master',
                'trigger_multiple.wait'))
def multiple_master_delay(t):
    lock(t)
    t.trigger('trigger_multiple', at=(0, 120, 0), target='out', master='lock', delay='0.3', killtarget='victim',
              wait='0.5', **TRIG)
    info(t, 'out')
    info(t, 'victim', at=(100, -100, 0))
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.6)
    t.mark('unlock')
    t.fire('key')
    t.wait(1.4)
    t.expect.not_fired('out', after='in', within=(0, 0.6))
    t.expect.fired('out', activator='player1', after='unlock', within=(0.25, 0.45), count=1)
    t.expect.fired('out', activator='player1', after='unlock', min=2)       # again after its wait
    t.expect.removed('victim', count=1, after='unlock', within=(0.25, 0.45))


@S.test('trigger_multiple flag 1 Monsters: a monster falling in fires it, as many times as it touches it after the '
        'wait', covers=('trigger_multiple:1',))
def multiple_monsters(t):
    t.trigger('trigger_multiple', at=(0, 100, 0), target='out', spawnflags='1', wait='0.2', **TRIG)
    info(t, 'out')
    t.point('monster_scientist', targetname='sci', at=(0, -150, 0))
    t.wait(0.5)
    t.mark('drop')
    drop(t, 'sci', (0, 100))
    t.wait(1.0)
    t.expect.fired('out', activator='sci', min=1, after='drop')


# ------------------------------------------------------------------------------------------------ trigger_hurt

@S.test('trigger_hurt: dmg is damage per second (dmg / 2 every half second) of its damagetype', players=1,
        covers=('trigger_hurt', 'trigger_hurt.dmg', 'trigger_hurt.damagetype=8'))
def hurt_dmg(t):
    t.trigger('trigger_hurt', at=(0, 120, 0), dmg='20', damagetype=str(DMG_BURN), **TRIG)
    t.flags('player1', '+FL_GODMODE')       # the damage is still recorded (TakeDamage is called)
    t.wait(0.2)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(1.2)
    t.mark('out')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.6)
    t.flags('player1', '-FL_GODMODE')
    t.expect.damaged('player1', after='in', within=(0, 1.2), total=(20, 30), type=DMG_BURN)
    t.expect.damaged('player1', after='in', within=(0, 0.1), total=(10, 10))
    t.expect.not_damaged('player1', after='out', within=(0.1, 0.6))


@S.test('trigger_hurt flag 2 Start Off: nothing until fired; fired again it stops', players=1,
        covers=('trigger_hurt:2',))
def hurt_start_off(t):
    t.trigger('trigger_hurt', at=(0, 120, 0), targetname='hurt', dmg='10', spawnflags='2', **TRIG)
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.2)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.6)
    t.mark('on')
    t.fire('hurt')
    t.wait(0.8)
    t.mark('off')
    t.fire('hurt')
    t.wait(0.8)
    t.teleport('player1', (0, -150, 0))
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_damaged('player1', after='in', within=(0, 0.5))
    t.expect.damaged('player1', after='on', within=(0, 0.2))
    t.expect.not_damaged('player1', after='off', within=(0.1, 0.8))


@S.test('trigger_hurt flag 8 No clients: a player is not hurt, a monster is', players=1, covers=('trigger_hurt:8',))
def hurt_noclients(t):
    t.trigger('trigger_hurt', at=(-120, 100, 0), dmg='10', spawnflags='8', **TRIG)
    t.trigger('trigger_hurt', at=(120, 100, 0), dmg='10', spawnflags='8', **TRIG)
    t.point('monster_scientist', targetname='sci', at=(120, -150, 0), health='1000')
    t.wait(0.5)
    t.mark('in')
    t.teleport('player1', (-120, 100, 0))
    drop(t, 'sci', (120, 100))
    t.wait(1.0)
    t.teleport('player1', (-120, -150, 0))
    t.expect.not_damaged('player1', after='in')
    t.expect.damaged('sci', after='in')


@S.test('trigger_hurt flag 32 TouchClientOnly: a monster is not hurt, a player is', players=1,
        covers=('trigger_hurt:32',))
def hurt_clientonly_touch(t):
    t.trigger('trigger_hurt', at=(-120, 100, 0), dmg='10', spawnflags='32', **TRIG)
    t.trigger('trigger_hurt', at=(120, 100, 0), dmg='10', spawnflags='32', **TRIG)
    t.point('monster_scientist', targetname='sci', at=(120, -150, 0), health='1000')
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.mark('in')
    t.teleport('player1', (-120, 100, 0))
    drop(t, 'sci', (120, 100))
    t.wait(1.0)
    t.teleport('player1', (-120, -150, 0))
    t.flags('player1', '-FL_GODMODE')
    t.expect.damaged('player1', after='in')
    t.expect.not_damaged('sci', after='in')


@S.test('trigger_hurt flag 16 FireClientOnly: its target fires when it hurts a player, not when it hurts a monster; '
        'flag 1 Target Once: only the first time; delay delays the target', players=1,
        covers=('trigger_hurt:16', 'trigger_hurt:1', 'trigger_hurt.delay', 'trigger_hurt.target'))
def hurt_target_flags(t):
    t.trigger('trigger_hurt', at=(-120, 100, 0), dmg='10', spawnflags='17', target='out_p', delay='0.4', **TRIG)
    t.trigger('trigger_hurt', at=(120, 100, 0), dmg='10', spawnflags='16', target='out_m', **TRIG)
    info(t, 'out_p')
    info(t, 'out_m')
    t.point('monster_scientist', targetname='sci', at=(120, -150, 0), health='1000')
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.5)
    t.mark('in')
    t.teleport('player1', (-120, 100, 0))
    drop(t, 'sci', (120, 100))
    t.wait(1.6)
    t.teleport('player1', (-120, -150, 0))
    t.flags('player1', '-FL_GODMODE')
    t.expect.damaged('player1', min=3, after='in')
    t.expect.damaged('sci', after='in')
    t.expect.fired('out_p', activator='player1', count=1, after='in', within=(0.35, 0.6))
    t.expect.not_fired('out_m', after='in')


@S.test('trigger_hurt master: a locked master keeps it from hurting (HL SDK and 1.38 ignored it; develop b6917d37)',
        players=1, covers=('trigger_hurt.master',))
def hurt_master(t):
    lock(t)
    t.trigger('trigger_hurt', at=(0, 120, 0), dmg='10', master='lock', **TRIG)
    t.flags('player1', '+FL_GODMODE')
    t.wait(0.3)
    t.mark('in')
    t.teleport('player1', (0, 120, 0))
    t.wait(0.6)
    t.teleport('player1', (0, -150, 0))
    t.flags('player1', '-FL_GODMODE')
    t.expect.not_damaged('player1', after='in')


# ------------------------------------------------------------------------------------------------ trigger_teleport

@S.test('trigger_teleport: a player is moved onto the destination and faces its direction', players=1,
        covers=('trigger_teleport', 'trigger_teleport.target'))
def teleport_player(t):
    t.trigger('trigger_teleport', at=(-120, -120, 0), target='dest', **TRIG)
    t.point('info_teleport_destination', targetname='dest', at=(150, 150, 0), angles='0 135 0')
    t.wait(0.2)
    t.teleport('player1', (-120, -120, 0), yaw=0)
    t.wait(0.3)
    t.snap('after', 'player1', 'origin', 'angles')
    t.snap('after', 'dest', 'origin')
    t.expect.distance('after', 'player1', 'dest', 0, 2)
    t.expect.value('after', 'player1', 'angles.y', 135, tol=1)


@S.test('trigger_teleport master: nothing while the multisource is locked; teleports once unlocked', players=1,
        covers=('trigger_teleport.master',))
def teleport_master(t):
    lock(t)
    t.trigger('trigger_teleport', at=(-120, -120, 0), target='dest', master='lock', **TRIG)
    t.point('info_teleport_destination', targetname='dest', at=(150, 150, 0))
    t.wait(0.3)
    t.teleport('player1', (-120, -120, 0))
    t.wait(0.5)
    t.snap('locked', 'player1', 'origin')
    t.snap('locked', 'dest', 'origin')
    t.fire('key')
    t.wait(0.4)
    t.snap('open', 'player1', 'origin')
    t.snap('open', 'dest', 'origin')
    t.expect.distance('locked', 'player1', 'dest', 300, 500)
    t.expect.distance('open', 'player1', 'dest', 0, 2)


@S.test('trigger_teleport flag 1 Monsters: a monster falling in is teleported; without the flag it is not',
        covers=('trigger_teleport:1',))
def teleport_monsters(t):
    t.trigger('trigger_teleport', at=(-150, -100, 0), target='dest_m', spawnflags='1', **TRIG)
    t.trigger('trigger_teleport', at=(150, -100, 0), target='dest_n', **TRIG)
    t.point('info_teleport_destination', targetname='dest_m', at=(-150, 170, 0))
    t.point('info_teleport_destination', targetname='dest_n', at=(150, 170, 0))
    t.point('monster_scientist', targetname='sci_m', at=(-60, 60, 0))
    t.point('monster_scientist', targetname='sci_n', at=(60, 60, 0))
    t.wait(0.5)
    drop(t, 'sci_m', (-150, -100))
    drop(t, 'sci_n', (150, -100))
    t.wait(1.0)
    t.snap('after', 'sci_m', 'origin')
    t.snap('after', 'dest_m', 'origin')
    t.snap('after', 'sci_n', 'origin')
    t.snap('after', 'dest_n', 'origin')
    t.expect.distance('after', 'sci_m', 'dest_m', 0, 4)
    t.expect.distance('after', 'sci_n', 'dest_n', 250, 300)


@S.test('trigger_teleport flag 2 No Clients: a player is not teleported', players=1, covers=('trigger_teleport:2',))
def teleport_noclients(t):
    t.trigger('trigger_teleport', at=(-120, -120, 0), target='dest', spawnflags='2', **TRIG)
    t.point('info_teleport_destination', targetname='dest', at=(150, 150, 0))
    t.wait(0.2)
    t.teleport('player1', (-120, -120, 0))
    t.wait(0.5)
    t.snap('after', 'player1', 'origin')
    t.snap('after', 'dest', 'origin')
    t.expect.distance('after', 'player1', 'dest', 300, 500)


@S.test('trigger_teleport landmark (message): the player keeps its place relative to the landmark, turned by the '
        'destination\'s yaw minus the landmark\'s', players=1, covers=('trigger_teleport.message',))
def teleport_landmark(t):
    t.trigger('trigger_teleport', at=(-150, -150, 0), target='dest', message='mark', **TRIG)
    t.point('info_target', targetname='mark', at=(-150, -150, 0), angles='0 0 0')
    t.point('info_target', targetname='dest', at=(150, 120, 0), angles='0 90 0')
    t.wait(0.2)
    t.teleport('player1', (-120, -150, 0), yaw=0)      # 30 east of the landmark
    t.wait(0.3)
    t.snap('after', 'player1', 'origin', 'angles')
    t.snap('after', 'dest', 'origin')
    # 30 east turned by 90 degrees: 30 north of the destination; the yaw turned by 90
    t.expect.value('after', 'player1', 'angles.y', 90, tol=1)
    t.expect.distance('after', 'player1', 'dest', 28, 32)
