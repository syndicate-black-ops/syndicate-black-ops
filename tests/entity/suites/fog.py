"""env_fog and env_fogzone (docs/WEATHER.md, sdk/syndicate.fgd): the "Fog" message (the first env_fog that is on, in
map order; none: color 0, density 0) with each HUD initialization (a respawn) and to every player when one is
switched; Start active, a name without it, each Density choice and a density above 0.01 (no fog); the "FogZone" list:
Active flag, a named zone without it, each Density choice. One suite: env_fog is map-wide state. The fog itself (how
it looks, depth) needs the client test layer."""

from etest import Suite

SUITE = Suite('fog', 'env_fog / env_fogzone: fog state (server side)', players=1)
S = SUITE
DOC = 'WEATHER.md'

DENSITIES = ('0.0005', '0.0007', '0.001', '0.003', '0.005', '0.01')


def dens(d):
    d = float(d)
    return {'range': (d * 0.999, d * 1.001)} if d else 0


@S.test('flag 1 Start active with a name: a respawn sends its color and density', players=1, doc=DOC,
        covers=('env_fog', 'env_fog:1', 'env_fog.rendercolor', 'env_fog.density=0.001'))
def start_active(t):
    t.point('env_fog', targetname='f1', spawnflags='1', rendercolor='10 20 30', density='0.001')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('Fog', to='player1', count=1, after='spawn', fields={'color': [10, 20, 30], 'density': dens('0.001')})


@S.test('a name without Start active starts off; Off on the one that is on: every player gets the next one that is '
        'on in map order (the unnamed one of the last test); On: the new one, which comes first in the map', players=1, doc=DOC, covers=('env_fog.targetname', 'env_fog.density=0.005'))
def switch(t):
    t.point('env_fog', targetname='f2', rendercolor='40 50 60', density='0.005')
    t.mark('off')
    t.fire('=t01_f1', use='off')
    t.wait(0.2)
    t.mark('on')
    t.fire('f2', use='on')
    t.wait(0.2)
    t.mark('again')
    t.fire('f2', use='on')          # on already: nothing
    t.wait(0.2)
    t.fire('f2', use='off')
    t.wait(0.1)
    t.expect.msg('Fog', to='player1', count=1, after='off', within=(0, 0.15), fields={'color': [70, 80, 90], 'density': dens('0.0007')})
    t.expect.msg('Fog', to='player1', count=1, after='on', within=(0, 0.15),
                 fields={'color': [40, 50, 60], 'density': dens('0.005')})
    t.expect.no_msg('Fog', after='again', within=(0, 0.15))


@S.test('each Density choice (Misty .. Thick) is sent as given; above 0.01: no fog (density 0)', players=1, doc=DOC,
        covers=tuple('env_fog.density=%s' % d for d in DENSITIES))
def densities(t):
    for i, d in enumerate(DENSITIES + ('0.02',)):
        t.point('env_fog', targetname='d%d' % i, rendercolor='%d 0 0' % (i + 1), density=d)
    for i, d in enumerate(DENSITIES + ('0.02',)):
        t.mark('m%d' % i)
        t.fire('d%d' % i, use='on')
        t.wait(0.1)
        t.fire('d%d' % i, use='off')
        t.wait(0.1)
        t.expect.msg('Fog', to='player1', count=1, after='m%d' % i, within=(0, 0.05),
                     fields={'color': [i + 1, 0, 0], 'density': dens(d) if d != '0.02' else 0})


@S.test('no name: always on (the first one that is on, when the earlier ones are off): a respawn sends it', players=1,
        doc=DOC, covers=('env_fog', 'env_fog.density=0.0007'))
def unnamed(t):
    t.point('env_fog', rendercolor='70 80 90', density='0.0007')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('Fog', to='player1', count=1, after='spawn', fields={'color': [70, 80, 90], 'density': dens('0.0007')})


@S.test('env_fogzone: flag 1 Active is on, a named zone without it is off (FogZone flags 1 / 0 in the list a respawn '
        'sends); triggering the off one sends it on; each Density choice and None (0) in the message', players=1,
        doc=DOC, covers=('env_fogzone', 'env_fogzone:1', 'env_fogzone.rendercolor', 'env_fogzone.targetname') +
        tuple('env_fogzone.density=%s' % d for d in ('0',) + DENSITIES))
def fogzone(t):
    zs = ('0',) + DENSITIES
    for i, d in enumerate(zs):
        t.brush('env_fogzone', size=(48, 48, 64), at=(-210 + 60 * i, 150, 0), tex='AAATRIGGER',
                targetname='z%d' % i, spawnflags='1' if i % 2 == 0 else '0', rendercolor='%d 5 6' % (i + 1),
                density=d)
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.mark('on')
    t.fire('z1', use='on')
    t.wait(0.2)
    for i, d in enumerate(zs):
        t.expect.msg('FogZone', to='player1', count=1, after='spawn', within=(0, 0.45),
                     fields={'flags': {'range': (1 if i % 2 == 0 else 0, 3 if i % 2 == 0 else 2)}, 'color': [i + 1, 5, 6],
                             'density': dens(d)})
    t.expect.msg('FogZone', to='player1', count=1, after='on', fields={'flags': 1, 'color': [2, 5, 6]})
