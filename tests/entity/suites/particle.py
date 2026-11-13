"""env_particle (docs/AURORA.md "The entity", sdk/syndicate.fgd): the "Aurora" state message decoded field by field
(on / off, origin, angles, script, Move with entity offset and attachment, Spawn Use copies, removal), the Start On
rule, the downloads (script and its sprites), the resend at a HUD re-initialization (fullupdate) and the console
lines for a bad script / target / attachment. What the client draws needs the client test layer."""

from etest import Suite

SUITE = Suite('particle', 'env_particle: Aurora particle systems (server side)', players=1)
S = SUITE
SUITE.server_args = ('+developer', '1')    # the entity's console lines (ALERT)
DOC = 'AURORA.md#the-entity'
AUR = 'aurora/steamvent.aur'       # names sprites/steam1.spr


def near(v, tol=1.0):
    """a vector within tol per component (decoded fields are compared with 0.01 otherwise)"""
    return [{'range': (c - tol, c + tol)} for c in v]


@S.test('no name: always on; a HUD re-initialization (fullupdate) sends its state (on, origin, angles, script); '
        'the script and its sprites are downloads', players=1, doc=DOC,
        covers=('env_particle', 'env_particle.message', 'env_particle.angles'))
def unnamed_on(t):
    t.point('env_particle', at=(10, 20, 64), message=AUR, angles='-90 45 0')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('Aurora', to='player1', count=1, after='fu',
                 fields={'on': 1, 'follow': 0, 'oneshot': 0, 'origin': near(t._abs((10, 20, 64)), 0.01),
                         'angles': [-90, 45, 0], 'script': AUR})
    t.expect.precached(AUR, kind='generic')
    t.expect.precached('sprites/steam1.spr', kind='generic')


@S.test('a name without Start On starts off; each trigger toggles it, sent to everybody at once; On when on and Off '
        'when off change nothing', players=1, doc=DOC, covers=('env_particle.targetname',))
def named_toggle(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('go')
    t.fire('p')                 # toggle: on
    t.wait(0.3)
    t.mark('on2')
    t.fire('p', use='on')       # already on: nothing
    t.wait(0.3)
    t.mark('off')
    t.fire('p', use='off')
    t.wait(0.3)
    t.mark('off2')
    t.fire('p', use='off')      # already off: nothing
    t.wait(0.3)
    org = near(t._abs((0, 0, 64)), 0.01)
    t.expect.msg('Aurora', to='player1', count=1, after='fu', within=(0, 0.9), fields={'on': 0, 'origin': org})
    t.expect.msg('Aurora', count=1, after='go', within=(0, 0.2), fields={'on': 1, 'origin': org, 'script': AUR})
    t.expect.no_msg('Aurora', after='on2', within=(0, 0.25))
    t.expect.msg('Aurora', count=1, after='off', within=(0, 0.2), fields={'on': 0, 'origin': org})
    t.expect.no_msg('Aurora', after='off2')


@S.test('flag 1 Start On with a name: on at the start; without a name the flag changes nothing (always on)',
        players=1, doc=DOC, covers=('env_particle:1',))
def start_on(t):
    t.point('env_particle', targetname='p', at=(-100, 0, 64), message=AUR, spawnflags='1')
    t.point('env_particle', at=(100, 0, 64), message=AUR, spawnflags='1')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('Aurora', to='player1', count=1, after='fu', fields={'on': 1, 'origin': near(t._abs((-100, 0, 64)), 0.01)})
    t.expect.msg('Aurora', to='player1', count=1, after='fu', fields={'on': 1, 'origin': near(t._abs((100, 0, 64)), 0.01)})


@S.test('switched several times in one frame: at most one state every 0.1 s, the last one wins', players=1, doc=DOC,
        covers=('env_particle.targetname',))
def rate_limit(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR)
    t.mark('go')
    t.fire('p', use='on')
    t.fire('p', use='off')
    t.fire('p', use='on')
    t.fire('p', use='off')
    t.wait(0.5)
    org = near(t._abs((0, 0, 64)), 0.01)
    t.expect.msg('Aurora', count=2, after='go', fields={'origin': org})
    t.expect.msg('Aurora', count=1, after='go', within=(0, 0.05), fields={'on': 1, 'origin': org})
    t.expect.msg('Aurora', count=1, after='go', within=(0.05, 0.3), fields={'on': 0, 'origin': org})


@S.test('flag 2 Spawn Use: a trigger sends a one-shot copy at the activator (its origin and angles, entity 0); the '
        'entity itself stays off', players=1, doc=DOC, covers=('env_particle:2',))
def spawn_use(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR, spawnflags='2')
    t.mark('go')
    t.fire('p', activator='player1')
    t.wait(0.3)
    t.expect.msg('Aurora', count=1, after='go',
                 fields={'on': 1, 'oneshot': 1, 'follow': 0, 'ent': 0, 'script': AUR,
                         'origin': near(t._abs((-48, -208, 36)), 2)})
    t.expect.msg('Aurora', count=0, after='go', fields={'oneshot': 0})


@S.test('flag 2 Spawn Use: more than 10 copies a second are dropped (12 triggers in one frame: 10 copies)', players=1,
        doc=DOC, covers=('env_particle:2',))
def spawn_use_limit(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR, spawnflags='2')
    t.mark('burst')
    for _ in range(12):
        t.fire('p', activator='player1')
    t.wait(0.3)
    t.expect.msg('Aurora', count=10, after='burst', fields={'oneshot': 1})


@S.test('Move with entity: the offset is measured in the target\'s frame (target turned 90 degrees), the angles '
        'relative to it; with attachment -1 the effect sits on the origin plus the offset', players=1, doc=DOC,
        covers=('env_particle.target', 'env_particle.attachment=-1'))
def follow_offset(t):
    t.point('info_target', targetname='mover', at=(0, 0, 64), angles='0 90 0')
    t.point('env_particle', targetname='p', at=(10, 20, 94), message=AUR, target='mover', angles='0 135 0',
            attachment='-1')
    t.mark('go')
    t.fire('p', use='on')
    t.wait(0.3)
    # world delta (10, 20, 30) in a frame turned 90 degrees: forward (0 1 0), left (-1 0 0): (20, -10, 30)
    t.expect.msg('Aurora', count=1, after='go',
                 fields={'on': 1, 'follow': 1, 'attachment': -1, 'origin': [20, -10, 30], 'angles': [0, 45, 0]})


@S.test('attachment 0-3 on a studio model target is sent; on a target that is not a model it is ignored (-1, '
        'console line); out of range (5) is -1 with a console line', players=1, doc=DOC,
        covers=('env_particle.attachment=0', 'env_particle.attachment=1', 'env_particle.attachment=2',
                'env_particle.attachment=3'))
def attachments(t):
    t.point('monster_furniture', targetname='mdl', at=(0, 100, 0), model='models/scientist.mdl')
    t.point('info_target', targetname='pt', at=(0, -100, 0))
    for i in range(4):
        t.point('env_particle', targetname='a%d' % i, at=(-150 + 100 * i, 100, 64), message=AUR, target='mdl',
                attachment=str(i))
    t.point('env_particle', targetname='bad', at=(0, -100, 64), message=AUR, target='pt', attachment='2')
    t.point('env_particle', targetname='far', at=(0, 0, 64), message=AUR, target='mdl', attachment='5')
    t.mark('go')
    for n in ('a0', 'a1', 'a2', 'a3', 'bad', 'far'):
        t.fire(n, use='on')
    t.wait(0.5)
    for i in range(4):
        t.expect.msg('Aurora', count=1, after='go', fields={'on': 1, 'follow': 1, 'attachment': i})
    t.expect.msg('Aurora', count=2, after='go', fields={'on': 1, 'follow': 1, 'attachment': -1})
    t.expect.console('is not a studio model, attachment ignored')
    t.expect.console('attachment 5, only 0 to 3')


@S.test('Move with entity that does not exist: hidden (sent off, no follow), console line; the target removed: '
        'hidden until an entity of that name exists again', players=1, doc=DOC, covers=('env_particle.target',))
def follow_missing(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR, target='nobody', spawnflags='1')
    t.point('info_target', targetname='mover', at=(100, 0, 64))
    t.point('env_particle', targetname='q', at=(100, 0, 64), message=AUR, target='mover', spawnflags='1')
    t.point('trigger_relay', targetname='kill', killtarget='mover')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('kill')
    t.fire('kill')
    t.wait(1.5)
    t.mark('fu2')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.console('target "%s" not found, the effect is hidden until it exists' % t.name('nobody'))
    t.expect.msg('Aurora', to='player1', count=1, after='fu', within=(0, 0.9),
                 fields={'on': 0, 'follow': 0, 'origin': near(t._abs((0, 0, 64)), 0.01)})
    t.expect.msg('Aurora', to='player1', count=1, after='fu', within=(0, 0.9),
                 fields={'on': 1, 'follow': 1, 'origin': [0, 0, 0]})
    t.expect.msg('Aurora', to='player1', count=1, after='fu2',
                 fields={'on': 0, 'follow': 0, 'origin': near(t._abs((100, 0, 64)), 0.01)})


@S.test('removed (killtarget): every client is told (removed flag); a bad script name (..) sends nothing, console '
        'line', players=1, doc=DOC, covers=('env_particle.message',))
def removed_and_bad(t):
    t.point('env_particle', targetname='p', at=(0, 0, 64), message=AUR, spawnflags='1')
    t.point('env_particle', targetname='bad', at=(50, 0, 64), message='../liblist.gam', spawnflags='1')
    t.point('trigger_relay', targetname='kill', killtarget='p')
    t.mark('go')
    t.fire('bad', use='toggle')
    t.fire('kill')
    t.wait(0.3)
    t.expect.msg('Aurora', count=1, after='go', fields={'removed': 1})
    t.expect.msg('Aurora', count=1, after='go')
    t.expect.not_precached('../liblist.gam')
    t.expect.console('no usable particle script')
