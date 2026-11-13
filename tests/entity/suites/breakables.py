"""func_breakable in play on a real server (GAPS.md tier 1 "func_breakable"): flags 1 Only Trigger (34 maps), 2 Touch,
4 Pressure, 256 Instant Crowbar (23 maps, a test player's crowbar swing); delay. What each checks is the FGD's
description (sdk/syndicate.fgd, base Breakable)."""

from etest import Suite

SUITE = Suite('breakables', 'func_breakable in play: Only Trigger, Touch, Pressure, Instant Crowbar, delay')
S = SUITE

DMG_BULLET, DMG_CLUB = 2, 128


def info(t, name, at=(0, 0, 0), **kv):
    return t.point('info_target', targetname=name, at=at, **kv)


@S.test('func_breakable flag 1 Only Trigger: damage does not break it; fired, it breaks and fires its target after its '
        'delay', players=1, covers=('func_breakable:1', 'func_breakable.delay', 'func_breakable.target'))
def only_trigger(t):
    t.brush('func_breakable', size=(48, 48, 48), at=(0, 120, 0), targetname='glass', health='20', material='0',
            spawnflags='1', target='broken', delay='0.5')
    info(t, 'broken')
    t.damage('glass', 500, DMG_BULLET, 'player1')
    t.wait(0.3)
    t.snap('shot', 'glass', 'health', 'takedamage')
    t.mark('fire')
    t.fire('glass', activator='player1')
    t.wait(0.8)
    t.expect.value('shot', 'glass', 'health', 20)
    t.expect.value('shot', 'glass', 'takedamage', 0)
    t.expect.removed('glass', count=1, after='fire', within=(0, 0.2))
    t.expect.fired('broken', count=1, after='fire', within=(0.45, 0.65))     # no activator (HL SDK; key target_activator)


@S.test('func_breakable without Only Trigger: damage breaks it (control)', players=1, covers=('func_breakable',))
def damage_breaks(t):
    t.brush('func_breakable', size=(48, 48, 48), at=(0, 120, 0), targetname='glass', health='20', material='0',
            target='broken')
    info(t, 'broken')
    t.mark('shot')
    t.damage('glass', 30, DMG_BULLET, 'player1')
    t.wait(0.3)
    t.expect.removed('glass', count=1, after='shot', within=(0, 0.2))
    t.expect.fired('broken', count=1, after='shot')


@S.test('func_breakable flag 2 Touch: a player running into it breaks it; without the flag he does not', players=1,
        covers=('func_breakable:2',))
def touch_breaks(t):
    # damage = the runner's speed / 100 (250 u/s: 2.5) against health 2
    t.brush('func_breakable', size=(96, 16, 96), at=(-120, 150, 0), targetname='pane', health='2', material='0',
            spawnflags='2')
    t.brush('func_breakable', size=(96, 16, 96), at=(120, 150, 0), targetname='wall', health='2', material='0')
    t.mark('run')
    t.teleport('player1', (-120, 40, 0), yaw=90)
    t.walk('player1', 250, 0, 0.8)
    t.teleport('player1', (120, 40, 0), yaw=90)
    t.walk('player1', 250, 0, 0.8)
    t.wait(0.2)
    t.expect.removed('pane', count=1, after='run')
    t.expect.not_removed('wall', after='run')


@S.test('func_breakable flag 4 Pressure: a player standing on it breaks it after its delay; without the flag it holds',
        players=1, covers=('func_breakable:4',))
def pressure_breaks(t):
    t.brush('func_breakable', size=(96, 96, 32), at=(-120, 100, 0), targetname='floor_p', health='50', material='1',
            spawnflags='4', delay='0.6')
    t.brush('func_breakable', size=(96, 96, 32), at=(120, 100, 0), targetname='floor_n', health='50', material='1')
    t.mark('on')
    t.teleport('player1', (-120, 100, 34))
    t.wait(1.0)
    t.mark('on2')
    t.teleport('player1', (120, 100, 34))
    t.wait(1.0)
    t.expect.sound('debris/bustcrate', ent='floor_p', count=1, after='on', within=(0.55, 0.8))    # the break (wood)
    t.expect.removed('floor_p', count=1, after='on')        # removed the frame after (a frame can be long under load)
    t.expect.not_removed('floor_n', after='on2')


@S.test('func_breakable flag 256 Instant Crowbar: a player\'s melee hit (DMG_CLUB: the fists every player has, '
        'weapon_wong) breaks it whatever its health; without the flag the same hit does not', players=1,
        covers=('func_breakable:256',))
def instant_crowbar(t):
    t.brush('func_breakable', size=(64, 16, 96), at=(-120, 100, 0), targetname='crate_c', health='500', material='1',
            spawnflags='256')
    t.brush('func_breakable', size=(64, 16, 96), at=(120, 100, 0), targetname='crate_n', health='500', material='1')
    t.cmd('player1', 'weapon_wong')
    t.wait(1.0)
    t.teleport('player1', (-120, 70, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.mark('swing')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(1.0)
    t.teleport('player1', (120, 70, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait_frames(3)
    t.mark('swing2')
    t.move('player1', 0, 0, 3, '+attack')
    t.wait(0.6)
    t.snap('after', 'crate_n', 'health')
    t.expect.damaged('crate_c', attacker='player1', type=DMG_CLUB, after='swing', min=1)
    t.expect.removed('crate_c', count=1, after='swing', within=(0, 0.5))
    t.expect.damaged('crate_n', attacker='player1', type=DMG_CLUB, after='swing2', min=1)
    t.expect.not_removed('crate_n', after='swing2')
    t.expect.value('after', 'crate_n', 'health', 450, op='>=')
