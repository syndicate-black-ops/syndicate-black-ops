"""Keys changed while the map runs (docs/TRIGGER_CHANGEVALUE.md "What a change does while the map runs",
sdk/syndicate.fgd): a trigger_changevalue / trigger_copyvalue writing a key takes effect at once where the entity
copied it at spawn: fog and fog zones are sent again, an ambient_generic's volume, pitch and sound reach the playing
loop, func_rotating / func_conveyor / trigger_push follow speed and angles, a monster's health and max_health stay
consistent, a monstermaker that ran out starts again, a light that is on shows a new pattern, env_explosion's fireball
follows its magnitude, env_overlay / env_dlight / the active env_sky are sent again, a multi_manager target's delay
updates its entry, func_tank gets Spawn's limits, calc_ratio / calc velocity / env_state keep their values. The
refusals: a light's style, an overlay's channel, an ambient sound the map has not precached, an operation other than
Replace on a key the entity can't read."""

from etest import Suite

SUITE = Suite('runtime_keys', 'Keys changed while the map runs', players=1, requires=('Cheat_SoundPrecached',))
S = SUITE
SUITE.server_args = ('+developer', '1')    # the refusals' console lines (ALERT)
DOC = 'TRIGGER_CHANGEVALUE.md#what-a-change-does-while-the-map-runs'
SND_STOP, SND_CHANGE_VOL, SND_CHANGE_PITCH = 32, 64, 128
TE_EXPLOSION = 3
SPR = 'sprites/640_logo.spr'


def change(t, name, target, key, value, op='0', **kv):
    t.point('trigger_changevalue', targetname=name, target=target, m_iszValueName=key, m_iszNewValue=value,
            m_iszValueType=op, **kv)


def dens(d):
    return {'range': (d * 0.999, d * 1.001)}


def fire_marked(t, names, wait=0.3):
    for n in names:
        t.mark(n)
        t.fire(n)
        t.wait(wait)


@S.test('env_fog: Density and Color changed while it is on reach the player at once (Density Add 0.001 works on the '
        'value read back); env_fogzone: Density sent again in its FogZone entry', players=1, doc=DOC,
        covers=('env_fog.density', 'env_fog.rendercolor', 'env_fogzone.density'))
def fog(t):
    t.point('env_fog', targetname='f', spawnflags='1', rendercolor='10 20 30', density='0.001')
    t.trigger('env_fogzone', targetname='z', size=(96, 96, 96), at=(150, 150, 0), spawnflags='1',
              rendercolor='200 100 50', density='0.003')
    change(t, 'c_dens', 'f', 'density', '0.005')
    change(t, 'c_col', 'f', 'rendercolor', '50 60 70')
    change(t, 'c_add', 'f', 'density', '0.001', op='1')
    change(t, 'c_zone', 'z', 'density', '0.007')
    t.wait(0.2)
    fire_marked(t, ('c_dens', 'c_col', 'c_add', 'c_zone'))
    t.expect.msg('Fog', to='player1', count=1, after='c_dens', within=(0, 0.15),
                 fields={'color': [10, 20, 30], 'density': dens(0.005)})
    t.expect.msg('Fog', to='player1', count=1, after='c_col', within=(0, 0.15),
                 fields={'color': [50, 60, 70], 'density': dens(0.005)})
    t.expect.msg('Fog', to='player1', count=1, after='c_add', within=(0, 0.15),
                 fields={'color': [50, 60, 70], 'density': dens(0.006)})
    t.expect.msg('FogZone', to='player1', count=1, after='c_zone', within=(0, 0.15),
                 fields={'flags': 1, 'color': [200, 100, 50], 'density': dens(0.007)})


@S.test('ambient_generic playing a loop: Volume 5 sends a volume change (0.5), Pitch 150 a pitch change, Pitch Add 20 '
        'gives 170; with a preset the new volume is not sent (the preset sets it)', players=1, doc=DOC,
        covers=('ambient_generic.health', 'ambient_generic.pitch', 'ambient_generic.preset'))
def ambient_volume_pitch(t):
    t.point('ambient_generic', targetname='amb', at=(0, 0, 32), message='ambience/drips.wav', health='10', pitch='100',
            spawnflags='2')
    t.point('ambient_generic', targetname='pre', at=(100, 0, 32), message='ambience/alien_hollow.wav', health='10',
            preset='1', spawnflags='2')
    change(t, 'c_vol', 'amb', 'health', '5')
    change(t, 'c_pitch', 'amb', 'pitch', '150')
    change(t, 'c_padd', 'amb', 'pitch', '20', op='1')
    change(t, 'c_pre', 'pre', 'health', '3')
    t.wait(0.2)
    fire_marked(t, ('c_vol', 'c_pitch', 'c_padd', 'c_pre'))
    t.snap('s', 'amb', 'kv:pitch')
    t.expect.sound('drips', ent='amb', flags=SND_CHANGE_VOL, vol=(0.45, 0.55), count=1, after='c_vol',
                   within=(0, 0.15))
    t.expect.sound('drips', ent='amb', flags=SND_CHANGE_PITCH, pitch=150, count=1, after='c_pitch', within=(0, 0.15))
    t.expect.sound('drips', ent='amb', flags=SND_CHANGE_PITCH, pitch=170, count=1, after='c_padd', within=(0, 0.15))
    t.expect.value('s', 'amb', 'kv:pitch', 170)
    # the preset's own ramps send changes; none carries the new volume (0.3)
    t.expect.sound('alien_hollow', ent='pre', vol=(0.25, 0.35), count=0, after='c_pre', within=(0, 0.25))


@S.test('ambient_generic: a new sound the map has precached stops the playing loop and plays the new one, and the next '
        'Off stops the new one; a sound the map has not precached is refused (console line, the old name stays)',
        players=1, doc=DOC, covers=('ambient_generic.message',))
def ambient_message(t):
    t.point('ambient_generic', targetname='a1', at=(0, 0, 32), message='ambience/drips.wav', health='10',
            spawnflags='2')
    t.point('ambient_generic', targetname='a2', at=(100, 0, 32), message='ambience/alien_hollow.wav', health='10',
            spawnflags=str(2 | 16))
    change(t, 'c_new', 'a1', 'message', 'ambience/alien_hollow.wav')
    change(t, 'c_bad', 'a1', 'message', 'ambience/et_not_precached.wav')
    t.wait(0.2)
    fire_marked(t, ('c_new', 'c_bad'))
    t.snap('s', 'a1', 'message')
    t.mark('off')
    t.fire('a1', use='off')
    t.wait(0.2)
    t.expect.sound('drips', ent='a1', flags=SND_STOP, count=1, after='c_new', within=(0, 0.15))
    t.expect.sound('alien_hollow', ent='a1', count=1, after='c_new', within=(0, 0.15))
    t.expect.console('Key "message" of ambient_generic')
    t.expect.value('s', 'a1', 'message', 'ambience/alien_hollow.wav')
    t.expect.sound('alien_hollow', ent='a1', flags=SND_STOP, count=1, after='off', within=(0, 0.15))
    t.expect.sound('drips', ent='a1', flags=SND_STOP, count=0, after='off', within=(0, 0.15))


@S.test('func_rotating spinning at full speed: Speed 250 spins it at 250 at once, Speed 0 stops it', players=1,
        doc=DOC, covers=('func_rotating.speed',))
def rotating(t):
    t.brush('func_rotating', targetname='rot', size=(32, 32, 32), at=(0, 0, 64), origin=True, speed='100')
    change(t, 'c_fast', 'rot', 'speed', '250')
    change(t, 'c_stop', 'rot', 'speed', '0')
    t.fire('rot')
    t.wait(0.2)
    t.snap('on', 'rot', 'avelocity')
    fire_marked(t, ('c_fast',))
    t.snap('fast', 'rot', 'avelocity')
    fire_marked(t, ('c_stop',))
    t.snap('stop', 'rot', 'avelocity')
    t.expect.value('on', 'rot', 'avelocity', [0, 100, 0], tol=0.01)
    t.expect.value('fast', 'rot', 'avelocity', [0, 250, 0], tol=0.01)
    t.expect.value('stop', 'rot', 'avelocity', [0, 0, 0], tol=0.01)


@S.test('func_conveyor: the texture scroll (the speed coded in rendercolor) follows Speed 250 and -50; Speed 0 is 100 '
        'as at spawn', players=1, doc=DOC, covers=('func_conveyor.speed',))
def conveyor(t):
    t.brush('func_conveyor', targetname='cv', size=(128, 64, 16), at=(0, 0, 0), speed='100')
    change(t, 'c_250', 'cv', 'speed', '250')
    change(t, 'c_neg', 'cv', 'speed', '-50')
    change(t, 'c_zero', 'cv', 'speed', '0')
    t.wait(0.1)
    for n in ('c_250', 'c_neg', 'c_zero'):
        t.fire(n)
        t.wait(0.1)
        t.snap(n, 'cv', 'rendercolor', 'speed')
    t.expect.value('c_250', 'cv', 'rendercolor', [0, 15, 160])      # 250 x 16 = 4000
    t.expect.value('c_neg', 'cv', 'rendercolor', [1, 3, 32])        # 50 x 16 = 800, backwards
    t.expect.value('c_zero', 'cv', 'speed', 100)
    t.expect.value('c_zero', 'cv', 'rendercolor', [0, 6, 64])       # 1600


@S.test('trigger_push: new angles give a new push direction at once (0 90 0 pushes along +Y); Add 0 90 0 works on '
        'the direction read back (-X)', players=1, doc=DOC, covers=('trigger_push.angles',))
def push_angles(t):
    t.trigger('trigger_push', targetname='tp', size=(96, 96, 96), at=(0, 0, 0), speed='200')
    change(t, 'c_y', 'tp', 'angles', '0 90 0')
    change(t, 'c_add', 'tp', 'angles', '0 90 0', op='1')
    t.wait(0.1)
    t.snap('spawn', 'tp', 'movedir')
    fire_marked(t, ('c_y',), wait=0.1)
    t.snap('y', 'tp', 'movedir', 'angles')
    fire_marked(t, ('c_add',), wait=0.1)
    t.snap('add', 'tp', 'movedir')
    t.expect.value('spawn', 'tp', 'movedir', [1, 0, 0], tol=0.001)
    t.expect.value('y', 'tp', 'movedir', [0, 1, 0], tol=0.001)
    t.expect.value('y', 'tp', 'angles', [0, 0, 0], tol=0.001)
    t.expect.value('add', 'tp', 'movedir', [-1, 0, 0], tol=0.001)


@S.test('a monster\'s health raised above its maximum raises the maximum; a lower health keeps it; a maximum set '
        'below the health brings the health down to it', players=1, doc=DOC)
def monster_health(t):
    t.point('monster_scientist', targetname='sci', at=(100, 100, 0), angles='0 180 0')
    change(t, 'c_up', 'sci', 'health', '500')
    change(t, 'c_down', 'sci', 'health', '50')
    change(t, 'c_max', 'sci', 'max_health', '20')
    t.wait(0.3)
    for n in ('c_up', 'c_down', 'c_max'):
        t.fire(n)
        t.wait(0.1)
        t.snap(n, 'sci', 'health', 'max_health')
    t.expect.value('c_up', 'sci', 'health', 500)
    t.expect.value('c_up', 'sci', 'max_health', 500)
    t.expect.value('c_down', 'sci', 'health', 50)
    t.expect.value('c_down', 'sci', 'max_health', 500)
    t.expect.value('c_max', 'sci', 'health', 20)
    t.expect.value('c_max', 'sci', 'max_health', 20)


@S.test('monstermaker that made its last monster: a new Number of Monsters starts it again (it was on: it makes the '
        'next one); Add works on the count read back', players=1, doc=DOC, covers=('monstermaker.monstercount',))
def maker_refill(t):
    t.point('monstermaker', targetname='mk', at=(0, 150, 0), angles='0 270 0', monstertype='monster_scientist',
            monstercount='1', m_imaxlivechildren='5', delay='0.2', netname='kid', names=('netname',))
    change(t, 'c_two', 'mk', 'monstercount', '2')
    change(t, 'c_add', 'mk', 'monstercount', '3', op='1')
    t.mark('on')
    t.fire('mk', use='on')
    t.wait(1.0)
    t.snap('out', 'mk', 'kv:monstercount')
    t.damage('kid', 1000, 8192)         # gibbed: the spot is free
    t.wait(0.2)
    t.mark('refill')
    t.fire('c_two')
    t.wait(1.0)
    t.snap('after', 'mk', 'kv:monstercount')
    t.fire('c_add')
    t.wait(0.1)
    t.snap('add', 'mk', 'kv:monstercount')
    t.expect.spawned('kid', count=1, after='on', within=(0, 0.9))
    t.expect.value('out', 'mk', 'kv:monstercount', 0)
    t.expect.spawned('kid', min=1, after='refill', within=(0, 0.9))
    t.expect.value('after', 'mk', 'kv:monstercount', 1)
    t.expect.value('add', 'mk', 'kv:monstercount', 4)


@S.test('light that is on: a new Pattern shows at once; one that is off keeps it for its next turn-on; Style can\'t '
        'change at run time (console line, the style stays)', players=1, doc=DOC,
        covers=('light.pattern', 'light.style'))
def light_pattern(t):
    # the map compiler gives each named light its own style number (32-63): read back, not the map's value
    t.point('light', targetname='lon', at=(0, 0, 64), pattern='m')
    t.point('light', targetname='loff', at=(64, 0, 64), pattern='m', spawnflags='1')
    change(t, 'c_on', 'lon', 'pattern', 'aazz')
    change(t, 'c_off', 'loff', 'pattern', 'bbyy')
    change(t, 'c_style', 'lon', 'style', '12')
    t.wait(0.1)
    t.snap('before', 'lon', 'kv:style')
    fire_marked(t, ('c_on', 'c_off', 'c_style'), wait=0.1)
    t.snap('after', 'lon', 'kv:style')
    t.mark('turn_on')
    t.fire('loff')
    t.wait(0.1)
    t.expect.lightstyle('aazz', style=range(32, 64), count=1, after='c_on', within=(0, 0.1))
    t.expect.lightstyle('bbyy', style=range(32, 64), count=0, after='c_off', within=(0, 0.1))
    t.expect.lightstyle('bbyy', style=range(32, 64), count=1, after='turn_on', within=(0, 0.1))
    t.expect.lightstyle(style=12, count=0, after='c_style')
    t.expect.console('Key "style" of light')
    t.expect.same('before', 'after', 'lon', 'kv:style')


@S.test('an operation other than Replace on a key the entity can\'t read (a func_door\'s Lip) is refused with a '
        'console line and leaves the key alone', players=1, doc=DOC)
def op_unreadable(t):
    t.brush('func_door', targetname='door', size=(64, 16, 96), at=(0, 150, 0), lip='8')
    change(t, 'c_lip', 'door', 'lip', '5', op='1')
    t.wait(0.1)
    t.fire('c_lip')
    t.wait(0.1)
    t.snap('s', 'door', 'save')
    t.expect.console('can\'t be read, so only Replace works on it')
    t.expect.value('s', 'door', 'save.m_flLip', 8)


@S.test('env_explosion: a new Magnitude sizes the fireball at the next explosion (100 -> scale 30, 200 -> 90)',
        players=1, doc=DOC, covers=('env_explosion.iMagnitude',))
def explosion(t):
    t.point('env_explosion', targetname='ex', at=(0, 0, 64), iMagnitude='100', spawnflags=str(1 | 2))
    change(t, 'c_mag', 'ex', 'iMagnitude', '200')
    t.mark('first')
    t.fire('ex')
    t.wait(0.5)
    t.fire('c_mag')
    t.wait(0.1)
    t.mark('second')
    t.fire('ex')
    t.wait(0.5)
    t.expect.msg('svc:23', fields={'te': TE_EXPLOSION, 'scale': 30}, count=1, after='first', within=(0, 0.2))
    t.expect.msg('svc:23', fields={'te': TE_EXPLOSION, 'scale': 90}, count=1, after='second', within=(0, 0.2))


@S.test('env_overlay showing on a player\'s screen: a new X is sent to that player at once; Channel can\'t change at '
        'run time (console line)', players=1, doc=DOC, covers=('env_overlay.x', 'env_overlay.channel'))
def overlay(t):
    t.point('env_overlay', targetname='ov', sprite=SPR, x='0.25', y='0.25', holdtime='0', channel='2')
    change(t, 'c_x', 'ov', 'x', '0.5')
    change(t, 'c_ch', 'ov', 'channel', '5')
    t.fire('ov', activator='player1')
    t.wait(0.2)
    fire_marked(t, ('c_x', 'c_ch'), wait=0.2)
    t.expect.msg('Overlay', to='player1', count=1, after='c_x', within=(0, 0.15),
                 fields={'channel': 2, 'on': 1, 'x': 0.5, 'y': 0.25, 'image': SPR})
    t.expect.console('Key "channel" of env_overlay')
    t.expect.msg('Overlay', to='player1', count=1, after='c_ch', within=(0, 0.15), fields={'channel': 2})


@S.test('env_dlight that is on: a new radius and color go to every client at once; a radius above 1023 is 1023',
        players=1, doc=DOC, covers=('env_dlight.renderamt', 'env_dlight.rendercolor'))
def dlight(t):
    t.point('env_dlight', targetname='dl', at=(0, 0, 64), renderamt='100', rendercolor='255 0 0')
    change(t, 'c_rad', 'dl', 'renderamt', '300')
    change(t, 'c_col', 'dl', 'rendercolor', '0 255 0')
    change(t, 'c_big', 'dl', 'renderamt', '5000')
    t.fire('dl', use='on')
    t.wait(0.2)
    fire_marked(t, ('c_rad', 'c_col', 'c_big'), wait=0.2)
    t.expect.msg('KeyedDLight', count=1, after='c_rad', within=(0, 0.15),
                 fields={'state': 1, 'radius': 300, 'r': 255, 'g': 0})
    t.expect.msg('KeyedDLight', count=1, after='c_col', within=(0, 0.15),
                 fields={'state': 1, 'radius': 300, 'r': 0, 'g': 255})
    t.expect.msg('KeyedDLight', count=1, after='c_big', within=(0, 0.15), fields={'state': 1, 'radius': 1023})


@S.test('env_sky that is the sky: a new parallax scale (frags) goes to every client at once', players=1, doc=DOC,
        covers=('env_sky.frags',))
def sky(t):
    t.point('env_sky', targetname='sky', at=(0, 0, 100), frags='16')
    change(t, 'c_scale', 'sky', 'frags', '8')
    t.wait(0.2)
    fire_marked(t, ('c_scale',), wait=0.2)
    t.expect.msg('SetSky', count=1, after='c_scale', within=(0, 0.15),
                 fields={'mode': 1, 'origin': list(t._abs((0, 0, 100))), 'scale': 8})


@S.test('multi_manager: a new delay for a target updates its entry (fired once, at the new time, after the other); '
        'Add works on a delay read back', players=1, doc=DOC, covers=('multi_manager',))
def multi_manager(t):
    t.point('multi_manager', targetname='mm', **{'ra': '0.1', 'rb': '0.3'})
    t.point('trigger_relay', targetname='ra', target='ra_done')
    t.point('trigger_relay', targetname='rb', target='rb_done')
    t.point('info_target', targetname='ra_done')
    t.point('info_target', targetname='rb_done')
    change(t, 'c_a', 'mm', t.name('ra'), '0.6')
    change(t, 'c_b', 'mm', t.name('rb'), '0.1', op='1')
    t.fire('c_a')
    t.fire('c_b')
    t.wait(0.1)
    t.snap('s', 'mm', 'kv:' + t.name('rb'))
    t.mark('go')
    t.fire('mm')
    t.wait(1.0)
    t.expect.value('s', 'mm', 'kv:' + t.name('rb'), 0.4, tol=0.001)
    t.expect.fired('ra', count=1, after='go', within=(0.5, 0.75))
    t.expect.fired('rb', count=1, after='go', within=(0.3, 0.5))
    t.expect.order('rb', 'ra')


@S.test('func_tank: Rate of Fire 0 and an out-of-range Bullet accuracy at run time get Spawn\'s limits (1 shot a '
        'second, accuracy 0); Multiply works on the rate read back', players=1, doc=DOC,
        covers=('func_tank.firerate', 'func_tank.firespread'))
def tank(t):
    t.brush('func_tank', targetname='tank', size=(32, 32, 32), at=(0, 0, 64), origin=True, firerate='5',
            firespread='1')
    change(t, 'c_zero', 'tank', 'firerate', '0')
    change(t, 'c_spread', 'tank', 'firespread', '9')
    change(t, 'c_mul', 'tank', 'firerate', '3', op='2')
    t.wait(0.1)
    for n in ('c_zero', 'c_spread', 'c_mul'):
        t.fire(n)
        t.wait(0.1)
        t.snap(n, 'tank', 'save')
    t.expect.value('c_zero', 'tank', 'save.m_fireRate', 1, tol=0.001)
    t.expect.value('c_spread', 'tank', 'save.m_spread', 0)
    t.expect.value('c_mul', 'tank', 'save.m_fireRate', 3, tol=0.001)


@S.test('calc_ratio Minimum / Maximum, calc_velocity_path Factor and env_state\'s fire-on target change at run time '
        '(not refused as sounds); the new fire-on target fires when the env_state turns on', players=1, doc=DOC,
        covers=('calc_ratio.noise', 'calc_ratio.noise1', 'calc_velocity_path.noise', 'env_state.noise1'))
def value_fields(t):
    t.point('calc_ratio', targetname='cr', target='1', noise='0', noise1='10')
    t.point('calc_velocity_path', targetname='cvp', noise='2')
    t.point('env_state', targetname='es', noise1='on_old')
    t.point('info_target', targetname='on_old')
    t.point('info_target', targetname='on_new')
    change(t, 'c_min', 'cr', 'noise', '3')
    change(t, 'c_max', 'cr', 'noise1', '7')
    change(t, 'c_fac', 'cvp', 'noise', '0.5')
    change(t, 'c_es', 'es', 'noise1', t.name('on_new'))
    t.wait(0.1)
    for n in ('c_min', 'c_max', 'c_fac', 'c_es'):
        t.fire(n)
        t.wait(0.05)
    t.snap('s', 'cr', 'kv:noise', 'kv:noise1')
    t.snap('s', 'cvp', 'kv:noise')
    t.mark('on')
    t.fire('es', use='on')
    t.wait(0.2)
    t.expect.no_console('of calc_ratio')
    t.expect.no_console('of env_state')
    t.expect.value('s', 'cr', 'kv:noise', '3')
    t.expect.value('s', 'cr', 'kv:noise1', '7')
    t.expect.value('s', 'cvp', 'kv:noise', '0.5')
    t.expect.used('on_new', count=1, after='on')
    t.expect.not_used('on_old', after='on')
