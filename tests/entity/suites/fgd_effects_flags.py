"""Tier 5, final pass: effect entities' spawnflags, each against the same entity without it, seen in the messages
they send (svc:23 temp entities, ScreenFade) or in their state: env_beam Ring / StartSparks / EndSparks / Shade
Start / Shade End, env_laser StartSparks, env_funnel Reverse, env_fade Modulate, env_explosion No Decal, env_bubbles
Start Off, env_message Play Once, env_render Remove self."""

from etest import Suite

SUITE = Suite('fgd_effects_flags', 'Tier 5: effect entity flags', players=1)
S = SUITE
DOC = 'FGD effect flags'
TE_SPARKS, TE_BEAMRING, TE_LARGEFUNNEL, TE_WORLDDECAL, TE_FIZZ = 9, 24, 100, 116, 105
SVC = 'svc:23'


def info(t, name, at):
    t.point('info_target', targetname=name, at=at)


def beam(t, name, start, end, flags, life='0.5', **kv):
    t.point('env_beam', targetname=name, LightningStart=start, LightningEnd=end, renderamt='100', life=life,
            BoltWidth='10', texture='sprites/laserbeam.spr', StrikeTime='0', spawnflags=str(flags),
            names=('LightningStart', 'LightningEnd'), **kv)


@S.test('env_beam StartSparks (16) / EndSparks (32): a strike sends sparks at its start / end point', players=1,
        doc=DOC, covers=('env_beam:16', 'env_beam:32'))
def beam_sparks(t):
    info(t, 's', (0, -100, 100))
    info(t, 'e', (0, 100, 100))
    beam(t, 'b16', 's', 'e', 16)
    beam(t, 'b32', 's', 'e', 32)
    t.wait(0.2)
    t.mark('a')
    t.fire('b16')
    t.wait(0.3)
    t.mark('b')
    t.fire('b32')
    t.wait(0.3)
    start = [float(v) for v in t._abs((0, -100, 100))]
    end = [float(v) for v in t._abs((0, 100, 100))]
    t.expect.msg(SVC, fields={'te': TE_SPARKS, 'pos': start}, after='a', within=(0, 0.25), min=1)
    t.expect.msg(SVC, fields={'te': TE_SPARKS, 'pos': end}, after='a', within=(0, 0.25), count=0)
    t.expect.msg(SVC, fields={'te': TE_SPARKS, 'pos': end}, after='b', min=1)


@S.test('env_beam Ring (8) between two brush entities: a strike is a beam ring (TE_BEAMRING); between two points '
        'it draws nothing (Half-Life: "don\'t work")', players=1, doc=DOC, covers=('env_beam:8',))
def beam_ring(t):
    t.brush('func_wall', targetname='w1', size=(16, 16, 16), at=(-100, 100, 60))
    t.brush('func_wall', targetname='w2', size=(16, 16, 16), at=(100, 100, 60))
    beam(t, 'ring', 'w1', 'w2', 8)
    t.wait(0.2)
    t.mark('go')
    t.fire('ring')
    t.wait(0.3)
    t.expect.msg(SVC, fields={'te': TE_BEAMRING}, after='go', min=1)


@S.test('env_beam Shade Start (128) / Shade End (256): the beam fades in at its start / out at its end (beam flags '
        'FBEAM_SHADEIN 64 / FBEAM_SHADEOUT 128 in its render mode)', players=1, doc=DOC,
        covers=('env_beam:128', 'env_beam:256'))
def beam_shade(t):
    info(t, 's', (0, -100, 100))
    info(t, 'e', (0, 100, 100))
    beam(t, 'b128', 's', 'e', 128, life='0')
    beam(t, 'b256', 's', 'e', 256, life='0')
    t.wait(0.2)
    t.fire('b128')
    t.fire('b256')
    t.wait(0.2)
    t.snap('s', 'b128', 'rendermode')
    t.snap('s', 'b256', 'rendermode')
    t.expect.value('s', 'b128', 'rendermode', 64, op='&')
    t.expect.value('s', 'b256', 'rendermode', 128, op='&')


@S.test('env_laser StartSparks (16): it sends sparks at its start point; without the flag it does not', players=1,
        doc=DOC, covers=('env_laser:16',))
def laser_sparks(t):
    info(t, 'le', (0, 200, 100))
    t.point('env_laser', targetname='las', at=(0, -100, 100), LaserTarget='le', texture='sprites/laserbeam.spr',
            renderamt='100', width='10', spawnflags=str(1 | 16), names=('LaserTarget',))
    t.point('env_laser', targetname='ctl', at=(150, -100, 100), LaserTarget='le', texture='sprites/laserbeam.spr',
            renderamt='100', width='10', spawnflags='1', names=('LaserTarget',))
    t.wait(1.0)
    t.expect.msg(SVC, fields={'te': TE_SPARKS, 'pos': [float(v) for v in t._abs((0, -100, 100))]}, min=1)
    t.expect.msg(SVC, fields={'te': TE_SPARKS, 'pos': [float(v) for v in t._abs((150, -100, 100))]}, count=0)


@S.test('env_funnel Reverse (1): its TE_LARGEFUNNEL has flag 1 (flows out); without it 0', players=1, doc=DOC,
        covers=('env_funnel:1',))
def funnel_reverse(t):
    t.point('env_funnel', targetname='rev', at=(-100, 100, 50), spawnflags='1')
    t.point('env_funnel', targetname='fwd', at=(100, 100, 50), spawnflags='0')
    t.wait(0.2)
    t.mark('go')
    t.fire('rev')
    t.fire('fwd')
    t.wait(0.3)
    t.expect.msg(SVC, fields={'te': TE_LARGEFUNNEL, 'flags': 1}, after='go', count=1)
    t.expect.msg(SVC, fields={'te': TE_LARGEFUNNEL, 'flags': 0}, after='go', count=1)


@S.test('env_fade Modulate (2): the ScreenFade has FFADE_MODULATE (2) besides FFADE_OUT (1); without it only FFADE_OUT', players=1, doc=DOC,
        covers=('env_fade:2',))
def fade_modulate(t):
    t.point('env_fade', targetname='mod', duration='1', holdtime='0', renderamt='255', rendercolor='0 0 0',
            spawnflags=str(2 | 4))
    t.point('env_fade', targetname='bld', duration='1', holdtime='0', renderamt='255', rendercolor='0 0 0',
            spawnflags='4')
    t.wait(0.2)
    t.mark('m')
    t.fire('mod', activator='player1')
    t.wait(0.5)
    t.mark('b')
    t.fire('bld', activator='player1')
    t.wait(0.5)
    t.expect.msg('ScreenFade', to='player1', fields={'flags': 3}, after='m', within=(0, 0.4), count=1)
    t.expect.msg('ScreenFade', to='player1', fields={'flags': 1}, after='b', within=(0, 0.4), count=1)


@S.test('env_explosion No Decal (16): no scorch on the floor; without it a world decal (TE_WORLDDECAL)', players=1,
        doc=DOC, covers=('env_explosion:16',))
def explosion_no_decal(t):
    t.point('env_explosion', targetname='nd', at=(-100, 100, 16), iMagnitude='0', spawnflags=str(16 | 1))
    t.point('env_explosion', targetname='dc', at=(100, 100, 16), iMagnitude='0', spawnflags='1')
    t.wait(0.2)
    t.mark('nd')
    t.fire('nd')
    t.wait(0.5)
    t.mark('dc')
    t.fire('dc')
    t.wait(0.5)
    t.expect.msg(SVC, fields={'te': TE_WORLDDECAL}, after='nd', within=(0, 0.4), count=0)
    t.expect.msg(SVC, fields={'te': TE_WORLDDECAL}, after='dc', within=(0, 0.4), min=1)


@S.test('env_bubbles Start Off (1): no bubbles until triggered; without it they come from the start', players=1,
        doc=DOC, covers=('env_bubbles:1',))
def bubbles_start_off(t):
    t.brush('env_bubbles', targetname='off', size=(32, 32, 32), at=(-100, 100, 16), density='2', frequency='10',
            current='0', spawnflags='1')
    t.wait(1.0)
    t.mark('on')
    t.fire('off')
    t.wait(1.0)
    t.expect.msg(SVC, fields={'te': TE_FIZZ}, within=(0, 0.9), count=0)
    t.expect.msg(SVC, fields={'te': TE_FIZZ}, after='on', min=1)


@S.test('env_message Play Once (1): it is removed after its first use', players=1, doc=DOC,
        covers=('env_message:1',))
def message_once(t):
    t.point('env_message', targetname='once', message='ONCE_TEST', spawnflags='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('once', activator='player1')
    t.wait(0.3)
    t.expect.removed('once', after='go', count=1)


@S.test('env_render Remove self (64): it is removed after its first use', players=1, doc=DOC,
        covers=('env_render:64',))
def render_remove(t):
    t.brush('func_wall', targetname='w', size=(32, 32, 32), at=(0, 100, 0))
    t.point('env_render', targetname='rs', target='w', renderamt='100', rendermode='2', spawnflags='64')
    t.wait(0.2)
    t.mark('go')
    t.fire('rs')
    t.wait(0.3)
    t.expect.removed('rs', after='go', count=1)
