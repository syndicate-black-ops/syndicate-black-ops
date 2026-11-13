"""env_elight (docs/ENV_DLIGHT.md#env_elight): the entity light (models only) on env_dlight's keyed message
"KeyedDLight" with the elight flag (state 0x81) and the attachment byte. On / off, Start on, a followed entity (its
index and attachment sent, the entity kept on every client), an entity without a model (its origin), a missing
target (no light), the target removed (the light goes out), Duration + Only once, and the resend to a player whose
HUD is set up again (fullupdate: the late-joiner path through the send queue)."""

from etest import Suite

SUITE = Suite('elight', 'env_elight: entity light on the keyed dlight message', players=1, requires=('env_elight',))
S = SUITE
DOC = 'ENV_DLIGHT.md#env_elight'
ON = 0x81          # DLIGHT_MSG_ON | DLIGHT_MSG_ELIGHT
CONTROLLER = 'models/controller.mdl'     # Half-Life's alien controller (valve/): 4 attachments


@S.test('a trigger_relay switches it on (elight flag, radius, color, its position, nothing followed) and off',
        doc=DOC, covers=('env_elight', 'env_elight.renderamt', 'env_elight.rendercolor'))
def toggle(t):
    t.point('env_elight', at=(0, 0, 64), targetname='el', renderamt='60', rendercolor='40 255 10', spawnflags='0')
    t.point('trigger_relay', targetname='r_on', target='el', triggerstate='1')
    t.point('trigger_relay', targetname='r_off', target='el', triggerstate='0')
    t.wait(1.5)                              # the joining player's send queue is done (it sends lights that are on)
    t.mark('fire')
    t.fire('r_on')
    t.wait(0.2)
    t.mark('on')
    t.fire('r_on')                           # On again: nothing
    t.wait(0.2)
    t.mark('off')
    t.fire('r_off')
    t.wait(0.2)
    t.expect.msg('KeyedDLight', count=1, after='fire', within=(0, 0.2),
                 fields={'state': ON, 'elight': 1, 'radius': 60, 'r': 40, 'g': 255, 'b': 10,
                         'follow': 0, 'attachment': 0})
    t.expect.msg('KeyedDLight', count=0, after='on', within=(0, 0.2))
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0}, after='off')


@S.test('Radius and Light color left out (and written blank, which the compilers drop): the FGD defaults 64 and '
        'white; a Radius of 0 written in the map is kept (no light)', doc=DOC, covers=('env_elight', 'env_elight.renderamt', 'env_elight.rendercolor'))
def defaults(t):
    t.point('env_elight', at=(0, 0, 64), targetname='el_out', spawnflags='0')
    t.point('env_elight', at=(0, 0, 64), targetname='el_blank', renderamt='', rendercolor='', spawnflags='0')
    t.point('env_elight', at=(0, 0, 64), targetname='el_zero', renderamt='0', rendercolor='0 0 0', spawnflags='0')
    t.wait(1.5)
    t.mark('out')
    t.fire('el_out')
    t.wait(0.2)
    t.mark('blank')
    t.fire('el_blank')
    t.wait(0.2)
    t.mark('zero')
    t.fire('el_zero')
    t.wait(0.2)
    white = {'state': ON, 'elight': 1, 'radius': 64, 'r': 255, 'g': 255, 'b': 255}
    t.expect.msg('KeyedDLight', count=1, after='out', within=(0, 0.1), fields=white)
    t.expect.msg('KeyedDLight', count=1, after='blank', within=(0, 0.1), fields=white)
    t.expect.msg('KeyedDLight', count=1, after='zero', within=(0, 0.1),
                 fields={'state': ON, 'elight': 1, 'radius': 0, 'r': 0, 'g': 0, 'b': 0})


@S.test('Start on (named): on from the map start, the first trigger switches it off', doc=DOC,
        covers=('env_elight', 'env_elight:2'))
def start_on(t):
    t.point('env_elight', at=(0, 0, 64), targetname='el', renderamt='30', spawnflags='2')
    t.fire('el')
    t.wait(0.2)
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0})
    t.expect.msg('KeyedDLight', count=0, fields={'state': ON})


@S.test('Entity to follow: a drawn env_model: its index and the attachment (2) sent; an info_target (no model): its '
        'origin, nothing to follow, no attachment (positions: the test room\'s)', doc=DOC,
        covers=('env_elight', 'env_elight.target', 'env_elight.impulse'))
def follow(t):
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('info_target', at=(-100, 100, 50), targetname='tgt')
    t.point('env_elight', at=(0, 0, 64), targetname='el_m', target='mdl', impulse='2', renderamt='50',
            names=('target',))
    t.point('env_elight', at=(0, 0, 64), targetname='el_t', target='tgt', impulse='2', renderamt='70',
            names=('target',))
    t.fire('el_m')
    t.wait(0.1)
    t.mark('t')
    t.fire('el_t')
    t.wait(0.3)
    t.expect.msg('KeyedDLight', count=1, within=(0, 0.1),
                 fields={'state': ON, 'radius': 50, 'attachment': 2})
    t.expect.msg('KeyedDLight', count=0, within=(0, 0.1), fields={'state': ON, 'radius': 50, 'follow': 0})
    t.expect.msg('KeyedDLight', count=1, after='t',
                 fields={'state': ON, 'radius': 70, 'follow': 0, 'attachment': 0})


@S.test('a followed entity that moves out of the light: no position messages (the clients move it); a missing '
        'target: no light at all', doc=DOC, covers=('env_elight', 'env_elight.target'))
def follow_moving_and_missing(t):
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('env_elight', at=(0, 0, 64), targetname='el', target='mdl', renderamt='50', names=('target',))
    t.point('env_elight', at=(0, 0, 64), targetname='el_x', target='nobody', renderamt='90', names=('target',))
    t.fire('el')
    t.wait(0.2)
    t.mark('move')
    t.kv('mdl', 'origin', '200 120 0')
    t.wait(0.5)
    t.fire('el_x')
    t.wait(0.2)
    t.expect.msg('KeyedDLight', count=1, fields={'state': ON, 'radius': 50})
    t.expect.msg('KeyedDLight', count=0, fields={'state': 2}, after='move')
    t.expect.msg('KeyedDLight', count=0, fields={'radius': 90})


@S.test('the followed entity removed (killtarget): the light goes out', doc=DOC,
        covers=('env_elight', 'env_elight.target'))
def target_removed(t):
    t.brush('func_wall', targetname='wall', size=(32, 32, 32), at=(100, 100, 0))
    t.point('env_elight', at=(0, 0, 64), targetname='el', target='wall', renderamt='40', names=('target',))
    t.point('trigger_relay', targetname='kill', killtarget='wall')
    t.fire('el')
    t.wait(0.3)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.4)
    t.expect.msg('KeyedDLight', count=1, fields={'state': ON, 'radius': 40})
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0}, after='kill', within=(0, 0.3))


@S.test('Duration 0.5 + Only once: off 0.5 s after it turns on, then removed', doc=DOC,
        covers=('env_elight', 'env_elight.health', 'env_elight:1'))
def duration(t):
    t.point('env_elight', at=(0, 0, 64), targetname='el', renderamt='40', health='0.5', spawnflags='1')
    t.mark('on')
    t.fire('el')
    t.wait(1.0)
    t.expect.msg('KeyedDLight', count=1, fields={'state': ON}, after='on', within=(0, 0.2))
    t.expect.msg('KeyedDLight', count=1, fields={'state': 0}, after='on', within=(0.4, 0.8))
    t.expect.removed('el')


@S.test('a player whose HUD is set up again (fullupdate, as a late joiner) gets the lights that are on: the elight '
        'with its followed entity and attachment, the dlight unchanged', players=1, doc=DOC,
        covers=('env_elight', 'env_elight.target', 'env_elight.impulse'))
def late_joiner(t):
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('env_elight', at=(0, 0, 64), targetname='el', target='mdl', impulse='1', renderamt='45',
            rendercolor='255 0 0', names=('target',))
    t.point('env_dlight', at=(0, 0, 64), targetname='dl', renderamt='150')
    t.fire('el')
    t.fire('dl')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(2.0)
    t.expect.msg('KeyedDLight', to='player1', count=1, after='fu',
                 fields={'state': ON, 'radius': 45, 'r': 255, 'g': 0, 'b': 0, 'attachment': 1})
    t.expect.msg('KeyedDLight', to='player1', count=1, after='fu', fields={'state': 1, 'radius': 150, 'elight': 0})


# ---------------------------------------------------------------------------------------------- info_light_origin

@S.test('lightorigin: an env_model, a cycler and an env_static naming an info_light_origin: a player whose HUD is set '
        'up again gets the marker\'s position for each (LightOrigin with the model index; the prop in StaticProp); a '
        'name without a marker and an entity without the key: nothing', players=1,
        doc='ENV_DLIGHT.md#info_light_origin',
        covers=('info_light_origin', 'env_model.lightorigin', 'cycler.lightorigin', 'env_static.lightorigin'))
def light_origin(t):
    t.point('info_light_origin', at=(64, -96, 48), targetname='lo')
    t.point('env_model', targetname='mdl', at=(0, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2',
            lightorigin='lo', names=('lightorigin',))
    t.point('cycler', targetname='cyc', at=(-120, 120, 0), model=CONTROLLER, lightorigin='lo', names=('lightorigin',))
    t.point('env_model', targetname='plain', at=(120, 120, 0), model=CONTROLLER, m_iszSequence_On='idle2')
    t.point('env_model', targetname='lost', at=(120, -120, 0), model=CONTROLLER, m_iszSequence_On='idle2',
            lightorigin='nobody', names=('lightorigin',))
    t.point('env_static', at=(-120, -120, 0), model=CONTROLLER, lightorigin='lo', names=('lightorigin',))
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(2.0)
    # the two entities with the marker (the positions are the room's: the same point for all three)
    t.expect.msg('LightOrigin', to='player1', count=2, after='fu')
    t.expect.msg('StaticProp', to='player1', count=1, after='fu', fields={'type': 4})
