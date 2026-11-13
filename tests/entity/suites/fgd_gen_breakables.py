"""Tier 5, final pass, generated: the CBreakable keys on func_breakable and func_pushable (Breakable 128): Spark When
Hit 0 / 1 and Trigger when hit (a laser hitting it), Switch Texture When Damaged 0 / 1 (frame at half health), Target's
Activator 0 / 1 / 2 (its target's activator when player1 breaks it), Gib Model (the TE_BREAKMODEL model); func_pushable
Toggleable push 0 / 1 (a trigger switches pushing off)."""

from etest import Suite

SUITE = Suite('fgd_gen_breakables', 'Tier 5: breakable keys on func_breakable and func_pushable (generated)',
              players=1)
S = SUITE
DOC = 'FGD Breakable'
CLASSES = ('func_breakable', 'func_pushable')
TE_SPARKS, TE_BREAKMODEL = 9, 108
SF_PUSHABLE_DISABLED = 1 << 24


def crate(t, cls, name, at, **kv):
    if cls == 'func_pushable':
        kv['spawnflags'] = str(int(kv.get('spawnflags', '0')) | 128)
    return t.brush(cls, targetname=name, size=(32, 64, 64), at=at, material='1', **kv)


def info(t, name, at=(0, 0, 0)):
    t.point('info_target', targetname=name, at=at)


def cases(cls):
    short = cls.replace('func_', '')

    @S.test('%s Spark When Hit 1: a laser hitting the wood crate makes sparks; 0 (only computer material): none; '
            'Trigger when hit fires at each hit' % cls, players=1, doc=DOC,
            covers=('%s.spark_when_hit=0' % cls, '%s.spark_when_hit=1' % cls, '%s.whenhit' % cls))
    def sparks(t):
        for n, v, x in (('sp1', '1', -100), ('sp0', '0', 100)):
            info(t, n + '_end', (x, 200, 40))
            t.point('env_laser', targetname=n + '_las', at=(x, -150, 40), LaserTarget=n + '_end',
                    texture='sprites/laserbeam.spr', damage='1', names=('LaserTarget',))
            crate(t, cls, n, (x, 60, 8), health='100000', spark_when_hit=v, whenhit=n + '_hit', names=('whenhit',))
            info(t, n + '_hit')
        t.wait(0.2)
        t.mark('one')
        t.fire('sp1_las', use='on')
        t.wait(1.0)
        t.fire('sp1_las', use='off')
        t.mark('zero')
        t.fire('sp0_las', use='on')
        t.wait(1.0)
        t.fire('sp0_las', use='off')
        t.wait(0.2)
        t.expect.msg('svc:23', fields={'te': TE_SPARKS}, after='one', within=(0, 1.0), min=1)
        t.expect.msg('svc:23', fields={'te': TE_SPARKS}, after='zero', within=(0.1, 1.0), count=0)
        t.expect.used('sp1_hit', min=2)
        t.expect.used('sp0_hit', min=2)
    sparks.__name__ = short + '_sparks'

    @S.test('%s Switch Texture When Damaged 1: at half its health or less it shows its alternate texture (frame 1); '
            '0: frame 0' % cls, players=1, doc=DOC,
            covers=('%s.switch_texture_when_damaged=0' % cls, '%s.switch_texture_when_damaged=1' % cls))
    def switch_tex(t):
        crate(t, cls, 'tx1', (-100, 60, 8), health='100', switch_texture_when_damaged='1')
        crate(t, cls, 'tx0', (100, 60, 8), health='100', switch_texture_when_damaged='0')
        t.wait(0.2)
        t.damage('tx1', 60, attacker='player1')
        t.damage('tx0', 60, attacker='player1')
        t.wait(0.2)
        t.snap('s', 'tx1', 'frame')
        t.snap('s', 'tx0', 'frame')
        t.expect.value('s', 'tx1', 'frame', 1)
        t.expect.value('s', 'tx0', 'frame', 0)
    switch_tex.__name__ = short + '_switch_texture'

    @S.test('%s Target\'s Activator: 0 (No) fires its target with no activator, 1 with the breakable, 2 with the '
            'player who broke it' % cls, players=1, doc=DOC,
            covers=tuple('%s.target_activator=%d' % (cls, v) for v in (0, 1, 2)))
    def activator(t):
        for v, x in ((0, -150), (1, 0), (2, 150)):
            crate(t, cls, 'ta%d' % v, (x, 60, 8), health='10', target='out%d' % v, target_activator=str(v))
            info(t, 'out%d' % v)
        t.wait(0.2)
        t.mark('go')
        for v in (0, 1, 2):
            t.damage('ta%d' % v, 100, attacker='player1')
        t.wait(0.3)
        t.expect.fired('out0', after='go', count=1)
        t.expect.fired('out0', activator='player1', after='go', count=0)
        t.expect.fired('out0', activator='@' + cls, after='go', count=0)
        # the broken one has lost its name when it fires (Die clears targetname): matched by class
        t.expect.fired('out1', activator='@' + cls, after='go', count=1)
        t.expect.fired('out2', activator='player1', after='go', count=1)
    activator.__name__ = short + '_target_activator'

    @S.test('%s Gib Model: its gibs are that model (TE_BREAKMODEL)' % cls, players=1, doc=DOC,
            covers=('%s.gibmodel' % cls,))
    def gibmodel(t):
        crate(t, cls, 'gm', (0, 60, 8), health='10', gibmodel='models/woodgibs.mdl')
        t.wait(0.2)
        t.mark('go')
        t.damage('gm', 100, attacker='player1')
        t.wait(0.3)
        t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'model': 'model:models/woodgibs.mdl'}, after='go',
                     count=1)
    gibmodel.__name__ = short + '_gibmodel'


for _c in CLASSES:
    cases(_c)


@S.test('func_pushable Toggleable push 1: a trigger switches pushing off (and on); 0: a trigger does not', players=1,
        doc=DOC, covers=('func_pushable.toggleable_push=0', 'func_pushable.toggleable_push=1'))
def toggleable(t):
    t.brush('func_pushable', targetname='tg1', size=(32, 32, 32), at=(-100, 60, 0), toggleable_push='1')
    t.brush('func_pushable', targetname='tg0', size=(32, 32, 32), at=(100, 60, 0), toggleable_push='0')
    t.wait(0.2)
    t.fire('tg1')
    t.fire('tg0')
    t.wait(0.1)
    t.snap('s', 'tg1', 'spawnflags')
    t.snap('s', 'tg0', 'spawnflags')
    t.expect.value('s', 'tg1', 'spawnflags', SF_PUSHABLE_DISABLED, op='&')
    t.expect.value('s', 'tg0', 'spawnflags', SF_PUSHABLE_DISABLED, op='!&')


@S.test('func_pushable Instant Crowbar (256, with Breakable): one club hit by a player (DMG_CLUB) breaks it whatever '
        'its health; without the flag the same hit only hurts it', players=1, doc=DOC, covers=('func_pushable:256',))
def pushable_crowbar(t):
    crate(t, 'func_pushable', 'cb1', (-100, 60, 8), health='100', spawnflags='256')
    crate(t, 'func_pushable', 'cb0', (100, 60, 8), health='100')
    t.wait(0.2)
    t.mark('hit')
    t.damage('cb1', 10, 128, attacker='player1')
    t.damage('cb0', 10, 128, attacker='player1')
    t.wait(0.3)
    t.snap('s', 'cb0', 'health')
    t.expect.removed('cb1', after='hit', count=1)
    t.expect.value('s', 'cb0', 'health', 80, tol=0.5)       # breakables take double club damage (CBreakable::TakeDamage)
