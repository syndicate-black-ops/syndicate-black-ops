"""Tier 5, final pass: the effect flags that need something to hit, with one fixture: a wall (func_wall) in front of the
emitter. env_beam Decal End (64) and env_laser Decal End (64) leave a decal on the wall their damage hits (TE_DECAL);
env_blood Spray decals (8) leaves a blood decal on the floor it sprays at (TE_WORLDDECAL), Random Direction (1)
sprays away from its angles; env_laser No Camera Punch (4096) leaves the view of the player it hurts still; env_beam
Random Strike (4) strikes at random times within Strike again time, so more often than a beam that waits it out."""

from etest import Suite

SUITE = Suite('fgd_effects_hits', 'Tier 5: effect flags that need a hit', players=1)
S = SUITE
DOC = 'FGD effect flags'
TE_BEAMPOINTS, TE_DECAL, TE_DECALHIGH, TE_WORLDDECAL, TE_WORLDDECALHIGH, TE_BLOODSTREAM = 0, 104, 118, 116, 117, 101
SVC = 'svc:23'


def wall(t, y=150):
    t.brush('func_wall', targetname='wall', size=(400, 16, 128), at=(0, y, 0))


def info(t, name, at):
    t.point('info_target', targetname=name, at=at)


def decal_case(cls):
    @S.test('%s Decal End (64): its damage leaves a decal on the wall it hits (TE_DECAL); without the flag none'
            % cls, players=1, doc=DOC, covers=('%s:64' % cls,))
    def case(t):
        wall(t)
        info(t, 'end', (0, 220, 60))         # behind the wall: the beam's damage trace hits it
        for n, fl in (('dec', 64), ('ctl', 0)):
            if cls == 'env_beam':
                info(t, n + '_s', (0, 0, 60))
                t.point('env_beam', targetname=n, LightningStart=n + '_s', LightningEnd='end', renderamt='100',
                        BoltWidth='10', texture='sprites/laserbeam.spr', life='0', damage='20', spawnflags=str(fl),
                        names=('LightningStart', 'LightningEnd'))
            else:
                t.point('env_laser', targetname=n, at=(0, 0, 60), LaserTarget='end', texture='sprites/laserbeam.spr',
                        renderamt='100', width='10', damage='20', spawnflags=str(fl), names=('LaserTarget',))
        t.wait(0.2)
        for n in ('dec', 'ctl'):
            t.mark(n)
            t.fire(n, use='on')
            t.wait(0.8)
            t.fire(n, use='off')
            t.wait(0.4)
        t.expect.msg(SVC, fields={'te': TE_DECAL}, after='dec', within=(0, 0.8), min=1)
        t.expect.msg(SVC, fields={'te': TE_DECAL}, after='ctl', within=(0, 0.8), count=0)
    case.__name__ = 'decal_' + cls.replace('env_', '')
    return case


decal_case('env_beam')
decal_case('env_laser')


@S.test('env_blood Spray decals (8): blood sprayed down leaves a decal on the floor (TE_WORLDDECAL); without it '
        'none', players=1, doc=DOC, covers=('env_blood:8',))
def blood_decals(t):
    for n, fl in (('bd', 8), ('bn', 0)):
        t.point('env_blood', targetname=n, at=(-100 if n == 'bd' else 100, 100, 40), angles='90 0 0', color='0',
                amount='100', spawnflags=str(fl | 2))
    t.wait(0.2)
    t.mark('d')
    t.fire('bd')
    t.wait(0.4)
    t.mark('n')
    t.fire('bn')
    t.wait(0.4)
    t.expect.msg(SVC, fields={'te': TE_WORLDDECAL}, after='d', within=(0, 0.3), min=1)
    t.expect.msg(SVC, fields={'te': TE_WORLDDECAL}, after='n', within=(0, 0.3), count=0)


@S.test('env_blood Random Direction (1): the stream goes in a random direction, not along its angles (straight down '
        'here); without it straight down', players=1, doc=DOC, covers=('env_blood:1',))
def blood_random(t):
    t.point('env_blood', targetname='br', at=(-100, 100, 60), angles='90 0 0', color='0', amount='100',
            spawnflags=str(1 | 2))
    t.point('env_blood', targetname='bs', at=(100, 100, 60), angles='90 0 0', color='0', amount='100', spawnflags='2')
    t.wait(0.2)
    t.mark('r')
    t.fire('br')
    t.wait(0.4)
    t.mark('s')
    t.fire('bs')
    t.wait(0.4)
    t.expect.msg(SVC, fields={'te': TE_BLOODSTREAM, 'dir': [0.0, 0.0, -1.0]}, after='s', within=(0, 0.3), count=1)
    t.expect.msg(SVC, fields={'te': TE_BLOODSTREAM, 'dir': [0.0, 0.0, -1.0]}, after='r', within=(0, 0.3), count=0)
    t.expect.msg(SVC, fields={'te': TE_BLOODSTREAM}, after='r', within=(0, 0.3), count=1)


@S.test('env_laser No Camera Punch (4096): the player its damage hits keeps his view; a laser without it kicks it',
        players=1, doc=DOC, covers=('env_laser:4096',))
def laser_no_punch(t):
    t.flags('player1', '+FL_GODMODE')
    info(t, 'end', (0, 220, 36))
    t.point('env_laser', targetname='lp', at=(0, -220, 36), LaserTarget='end', texture='sprites/laserbeam.spr',
            renderamt='100', width='10', damage='50', spawnflags=str(4096), names=('LaserTarget',))
    t.point('env_laser', targetname='lk', at=(0, -220, 36), LaserTarget='end', texture='sprites/laserbeam.spr',
            renderamt='100', width='10', damage='50', spawnflags='0', names=('LaserTarget',))
    t.teleport('player1', (0, 0, 0))
    t.wait(0.3)
    t.fire('lp', use='on')
    t.wait(0.5)
    t.snap('np', 'player1', 'punchangle')
    t.fire('lp', use='off')
    t.wait(0.5)
    t.fire('lk', use='on')
    t.wait(0.3)
    t.snap('kp', 'player1', 'punchangle')
    t.fire('lk', use='off')
    t.expect.value('np', 'player1', 'punchangle.x', 0, tol=0.01)
    t.expect.value('np', 'player1', 'punchangle.y', 0, tol=0.01)
    t.expect.value('kp', 'player1', 'punchangle.x', 0, op='!=')


@S.test('env_beam Random Strike (4): with Life 0.1 and Strike again time 2 it strikes at 0.1 + a random 0-2 s, so '
        'more often in 6 s than the same beam without the flag (every 2.1 s: at most 3)', players=1, doc=DOC,
        covers=('env_beam:4',))
def beam_random_strike(t):
    info(t, 's', (0, -100, 100))
    info(t, 'e', (0, 100, 100))
    for n, fl in (('rnd', 4 | 1), ('fix', 1)):
        t.point('env_beam', targetname=n, LightningStart='s', LightningEnd='e', renderamt='100', BoltWidth='10',
                texture='sprites/laserbeam.spr', life='0.1', StrikeTime='2', spawnflags=str(fl),
                rendercolor='255 0 0' if n == 'rnd' else '0 255 0', names=('LightningStart', 'LightningEnd'))
    t.mark('go')
    t.wait(6.0)
    t.expect.msg(SVC, fields={'te': TE_BEAMPOINTS, 'color': [255, 0, 0]}, after='go', min=4)
    t.expect.msg(SVC, fields={'te': TE_BEAMPOINTS, 'color': [0, 255, 0]}, after='go', max=3)
