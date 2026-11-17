"""env_entity_glow (docs/ENV_ENTITY_GLOW.md, sdk/syndicate.fgd): the "EntGlow" state message decoded field by field:
on / off, Mode, Style, GlowColor (R G B A), the number of targets (every entity with the name); the FGD defaults;
Start Disabled; On / Off / Toggle; GlowColor, Mode and Style changed by trigger_changevalue; a target removed (the list
is looked up again); sent at a HUD re-initialization (fullupdate: the late-joiner path). The glow itself needs the
client."""

from etest import Suite

SUITE = Suite('entity_glow', 'env_entity_glow: glow state (server side)', players=2, requires=('env_entity_glow',))
S = SUITE
DOC = 'ENV_ENTITY_GLOW.md#keys'


def targets(t, name, n=2):
    """n crates named `name` side by side"""
    for i in range(n):
        t.brush('func_wall', targetname=name, size=(32, 32, 32), at=(-150 + i * 60, 150, 0))


@S.test('keys left out: on, Mode 0 Always, Style 0 Outline, white at full strength; every entity with the name '
        'counts (2 crates); sent again at a fullupdate', players=1, doc=DOC,
        covers=('env_entity_glow', 'env_entity_glow.target', 'env_entity_glow.GlowColor', 'env_entity_glow.Mode=0',
                'env_entity_glow.Style=0', 'env_entity_glow.StartDisabled=0'))
def defaults(t):
    targets(t, 'crate')
    t.point('env_entity_glow', at=(0, 0, 64), target='crate', names=('target',))
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.msg('EntGlow', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'on': 1, 'mode': 0, 'style': 0, 'color': [255, 255, 255, 255], 'count': 2})


@S.test('GlowColor with alpha, Mode 1 Only when occluded, Style 1 Halo; GlowColor without alpha (full strength), '
        'Mode 2 Only when visible; Mode 7 and Style 5 (not Source values) are the defaults; StartDisabled 0 is on',
        players=1, doc=DOC,
        covers=('env_entity_glow.GlowColor', 'env_entity_glow.Mode=1', 'env_entity_glow.Mode=2',
                'env_entity_glow.Style=1', 'env_entity_glow.StartDisabled=0'))
def keys(t):
    targets(t, 'a', 1)
    targets(t, 'b', 1)
    targets(t, 'c', 1)
    t.point('env_entity_glow', at=(0, 0, 64), target='a', GlowColor='255 0 0 128', Mode='1', Style='1',
            StartDisabled='0', names=('target',))
    t.point('env_entity_glow', at=(0, 0, 64), target='b', GlowColor='10 20 30', Mode='2', names=('target',))
    t.point('env_entity_glow', at=(0, 0, 64), target='c', Mode='7', Style='5', names=('target',))
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.expect.msg('EntGlow', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'on': 1, 'mode': 1, 'style': 1, 'color': [255, 0, 0, 128], 'count': 1})
    t.expect.msg('EntGlow', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'on': 1, 'mode': 2, 'style': 0, 'color': [10, 20, 30, 255], 'count': 1})
    t.expect.msg('EntGlow', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'on': 1, 'mode': 0, 'style': 0, 'color': [255, 255, 255, 255], 'count': 1})


@S.test('Start Disabled: not sent at a fullupdate; a trigger turns it on for everybody (Use toggles), On again does '
        'nothing, Off turns it off (on 0)', players=1, doc=DOC,
        covers=('env_entity_glow.StartDisabled=1', 'env_entity_glow.targetname'))
def start_disabled(t):
    targets(t, 'crate', 1)
    t.point('env_entity_glow', at=(0, 0, 64), targetname='g', target='crate', StartDisabled='1', Style='1',
            names=('target',))
    t.point('trigger_relay', targetname='g_on', target='g', triggerstate='1')
    t.point('trigger_relay', targetname='g_off', target='g', triggerstate='0')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.mark('on')
    t.fire('g')
    t.wait(0.2)
    t.mark('again')
    t.fire('g_on')
    t.wait(0.2)
    t.mark('off')
    t.fire('g_off')
    t.wait(0.2)
    t.expect.msg('EntGlow', to='player1', count=0, after='fu', within=(0, 0.45))
    t.expect.msg('EntGlow', count=1, after='on', within=(0, 0.1), fields={'on': 1, 'style': 1, 'count': 1})
    t.expect.msg('EntGlow', count=0, after='again', within=(0, 0.15))
    t.expect.msg('EntGlow', count=1, after='off', within=(0, 0.1), fields={'on': 0})


@S.test('trigger_changevalue sets GlowColor, Style and Mode while it is on: each change goes to everybody at once',
        doc=DOC, covers=('env_entity_glow.GlowColor', 'env_entity_glow.Style=1', 'env_entity_glow.Mode=2'))
def changevalue(t):
    targets(t, 'crate', 1)
    t.point('env_entity_glow', at=(0, 0, 64), targetname='g', target='crate', GlowColor='0 0 1', names=('target',))
    t.point('trigger_changevalue', targetname='cv_col', target='g', m_iszValueName='GlowColor',
            m_iszNewValue='0 200 255 200')
    t.point('trigger_changevalue', targetname='cv_sty', target='g', m_iszValueName='Style', m_iszNewValue='1')
    t.point('trigger_changevalue', targetname='cv_mode', target='g', m_iszValueName='Mode', m_iszNewValue='2')
    t.wait(0.5)
    t.mark('col')
    t.fire('cv_col')
    t.wait(0.2)
    t.mark('sty')
    t.fire('cv_sty')
    t.wait(0.2)
    t.mark('mode')
    t.fire('cv_mode')
    t.wait(0.2)
    t.expect.msg('EntGlow', count=1, after='col', within=(0, 0.1),
                 fields={'on': 1, 'color': [0, 200, 255, 200], 'style': 0, 'mode': 0})
    t.expect.msg('EntGlow', count=1, after='sty', within=(0, 0.1),
                 fields={'on': 1, 'color': [0, 200, 255, 200], 'style': 1, 'mode': 0})
    t.expect.msg('EntGlow', count=1, after='mode', within=(0, 0.1),
                 fields={'on': 1, 'color': [0, 200, 255, 200], 'style': 1, 'mode': 2})


@S.test('a target removed (killtarget): the glow is sent again with one target less within its half-second look',
        doc=DOC, covers=('env_entity_glow.target',))
def target_removed(t):
    t.brush('func_wall', targetname='crate', size=(32, 32, 32), at=(-150, 150, 0))
    t.brush('func_wall', targetname='crate', size=(32, 32, 32), at=(-90, 150, 0))
    t.point('env_entity_glow', at=(0, 0, 64), target='crate', GlowColor='9 9 9', names=('target',))
    t.point('trigger_relay', targetname='kill', killtarget='crate')
    t.wait(0.5)
    t.mark('kill')
    t.fire('kill')
    t.wait(1.0)
    t.expect.msg('EntGlow', count=1, after='kill', within=(0, 0.7), fields={'on': 1, 'color': [9, 9, 9, 255], 'count': 0})


@S.test('flag 1 Only the activator sees it, target !all, PlayerColor, duration 1: player1 turns it on and only he gets '
        'it: the monster in his glow color, player2 in the player color; nothing to player2; off after 1 s',
        players=2, doc=DOC,
        covers=('env_entity_glow:1', 'env_entity_glow.target', 'env_entity_glow.PlayerColor',
                'env_entity_glow.duration'))
def activator_only(t):
    t.point('monster_scientist', at=(200, 200, 0))
    t.point('env_entity_glow', at=(0, 0, 64), targetname='hk', target='!all', spawnflags='1', duration='1',
            GlowColor='255 0 0 255', PlayerColor='0 0 255 255')
    t.wait(0.5)
    t.mark('on')
    t.fire('hk', activator='player1')
    t.wait(1.6)
    t.expect.msg('EntGlow', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'color': [255, 0, 0, 255], 'count': 1})
    t.expect.msg('EntGlow', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'color': [0, 0, 255, 255], 'count': 1})
    t.expect.msg('EntGlow', to='player2', count=0, after='on')
    t.expect.msg('EntGlow', to='player1', min=1, after='on', within=(0.9, 1.6), fields={'on': 0})


@S.test('flag 1 turned on by something that is not a player: it stays off (nothing sent)', players=2, doc=DOC,
        covers=('env_entity_glow:1',))
def activator_only_no_player(t):
    t.point('env_entity_glow', at=(0, 0, 64), targetname='hk2', target='!all', spawnflags='1',
            GlowColor='9 8 7 255')
    t.wait(0.5)
    t.mark('on')
    t.fire('hk2')
    t.wait(0.5)
    t.expect.msg('EntGlow', count=0, after='on', fields={'color': [9, 8, 7, 255]})
