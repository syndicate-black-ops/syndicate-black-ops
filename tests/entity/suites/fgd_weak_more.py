"""Tier 5, final pass: keys whose only test was the FGD defaults check (save_same), each now checked by what it does:
env_message volume, env_fade duration / hold time (ScreenFade), how long the AI sounds of env_soundmark and
env_warpball last (CSoundEnt: the sound is there before its duration ends and gone after), func_plat / func_platrot
move-sound volume."""

from etest import Suite

SUITE = Suite('fgd_weak_more', 'Tier 5: message volume, fade times, AI sound durations, plat volume', players=1)
S = SUITE
DOC = 'FEATUREFUL_GUIDE_ENTITIES.md'


@S.test('env_message "Volume 10 = loudest" 5: its sound plays at volume 0.5', players=1, doc=DOC,
        covers=('env_message.messagevolume',))
def message_volume(t):
    t.point('env_message', targetname='msg', message='ET_MSG', messagesound='buttons/blip1.wav', messagevolume='5',
            messageattenuation='0')
    t.wait(0.2)
    t.mark('go')
    t.fire('msg', activator='player1')
    t.wait(0.3)
    t.expect.sound('buttons/blip1.wav', after='go', vol=(0.45, 0.55), min=1)


@S.test('env_fade "Duration" 2 and "Hold Fade" 1.5: the ScreenFade message carries them', players=1, doc=DOC,
        covers=('env_fade.duration', 'env_fade.holdtime'))
def fade_times(t):
    t.point('env_fade', targetname='fade', duration='2', holdtime='1.5', renderamt='255', rendercolor='0 0 0',
            spawnflags='4')         # 4: activator only
    t.wait(0.2)
    t.mark('go')
    t.fire('fade', activator='player1')
    t.wait(0.3)
    t.expect.msg('ScreenFade', to='player1', min=1, after='go', fields={'duration': 2, 'hold': 1.5})


def lasts(t, label_ok, label_gone, ent_at, typ):
    t.expect.aisound(label_ok, 'player1', typ, pos=ent_at, tol=40, min=1)
    t.expect.aisound(label_gone, 'player1', typ, pos=ent_at, tol=40, count=0)


@S.test('env_soundmark "Sound Duration" 2.5: its AI sound is still there 2 s after, gone 3.2 s after', players=1,
        doc=DOC, covers=('env_soundmark.duration',))
def soundmark_duration(t):
    t.point('env_soundmark', targetname='snd', at=(0, 100, 36), type='1', radius='300', duration='2.5')
    t.wait(0.2)
    t.fire('snd')
    t.wait(2.0)
    t.snap('ok', 'player1', 'aisounds')
    t.wait(1.2)
    t.snap('gone', 'player1', 'aisounds')
    lasts(t, 'ok', 'gone', (0, 100, 36), 1)


@S.test('env_warpball "AI Sound Duration" 2.5: its AI sound is still there 2 s after, gone 3.2 s after', players=1,
        doc=DOC, covers=('env_warpball.aisound_duration',))
def warpball_duration(t):
    t.point('env_warpball', targetname='wb', at=(0, 100, 40), aisound='1', aisound_duration='2.5', spawnflags='4')
    t.wait(0.2)
    t.fire('wb')
    t.wait(2.0)
    t.snap('ok', 'player1', 'aisounds')
    t.wait(1.2)
    t.snap('gone', 'player1', 'aisounds')
    lasts(t, 'ok', 'gone', (0, 100, 40), 1)


def plat_case(cls):
    @S.test('%s "Sound Volume 0.0 - 1.0" 0.3: its move sound plays at volume 0.3' % cls, players=1, doc=DOC,
            covers=('%s.volume' % cls,))
    def case(t):
        kv = dict(rotation='90', origin=True) if cls == 'func_platrot' else {}
        origin = kv.pop('origin', False)
        t.brush(cls, targetname='p', size=(64, 64, 16), at=(0, 100, 0), height='64', speed='100', movesnd='1',
                volume='0.3', origin=origin, **kv)
        t.wait(0.5)
        t.mark('go')
        t.fire('p')
        t.wait(0.5)
        t.expect.sound('plats/', ent='p', after='go', vol=(0.25, 0.35), min=1)
    case.__name__ = 'volume_' + cls
    return case


for _c in ('func_plat', 'func_platrot'):
    plat_case(_c)


@S.test('momentary_door "Lip" 30: set to 1 it moves its size less 2 less the lip (66: a 96 brush is 98 wide, the '
        'compiler pads it); "Starts Open" (1): it spawns at the open end and set to 1 it moves back 96', players=1,
        doc=DOC,
        covers=('momentary_door.lip', 'momentary_door:1'))
def momentary_lip_open(t):
    t.brush('momentary_door', targetname='lip', size=(96, 16, 96), at=(-100, 150, 0), angles='0 0 0', lip='30')
    t.brush('momentary_door', targetname='open', size=(96, 16, 96), at=(100, -150, 0), angles='0 0 0',
            spawnflags='1')
    t.wait(0.3)
    t.snap('a', 'lip', 'origin')
    t.snap('a', 'open', 'origin')
    t.fire('lip', activator='player1', use='set', value=1)
    t.fire('open', activator='player1', use='set', value=1)
    t.wait(0.5)
    t.snap('b', 'lip', 'origin')
    t.snap('b', 'open', 'origin')
    t.expect.moved('a', 'b', 'lip', delta=(66, 0, 0), tol=1)
    t.expect.value('a', 'open', 'origin', [96, 0, 0], tol=1)
    t.expect.moved('a', 'b', 'open', delta=(-96, 0, 0), tol=1)


TE_FIZZ = 105
TE_BLOODSTREAM, TE_BLOODSPRITE = 101, 115


@S.test('env_bubbles "Bubble density" 7: its TE_FIZZ carries density 7', players=1, doc=DOC,
        covers=('env_bubbles.density',))
def bubbles_density(t):
    t.brush('env_bubbles', targetname='b', size=(64, 64, 64), at=(0, 100, 0), tex='AAATRIGGER', density='7',
            frequency='15', spawnflags='1')     # Start Off: on in this test only
    t.wait(0.2)
    t.mark('go')
    t.fire('b')
    t.wait(1.5)
    t.fire('b')
    t.expect.msg('svc:23', fields={'te': TE_FIZZ, 'density': 7}, min=1, after='go')


@S.test('env_bubbles "Bubble frequency" 15: a TE_FIZZ every 1 s (2.5 - 0.1 x 15): 4 to 6 in 5 s; frequency 2: every '
        '2.3 s, 2 or 3', players=1, doc=DOC, covers=('env_bubbles.frequency',))
def bubbles_frequency(t):
    t.brush('env_bubbles', targetname='f', size=(64, 64, 64), at=(-100, 100, 0), tex='AAATRIGGER', density='11',
            frequency='15', spawnflags='1')
    t.brush('env_bubbles', targetname='s', size=(64, 64, 64), at=(100, 100, 0), tex='AAATRIGGER', density='12',
            frequency='2', spawnflags='1')
    t.wait(0.2)
    t.mark('go')
    t.fire('f')
    t.fire('s')
    t.wait(5.0)
    t.fire('f')
    t.fire('s')
    t.expect.msg('svc:23', fields={'te': TE_FIZZ, 'density': 11}, min=4, max=6, after='go')
    t.expect.msg('svc:23', fields={'te': TE_FIZZ, 'density': 12}, min=2, max=3, after='go')


@S.test('env_bubbles "Speed of Current" 300 / -300: the speed goes to the clients in its render color (300 >> 8, '
        '300 & 255, 1 when negative)', players=1, doc=DOC, covers=('env_bubbles.current',))
def bubbles_current(t):
    t.brush('env_bubbles', targetname='p', size=(64, 64, 64), at=(-100, 100, 0), tex='AAATRIGGER', current='300',
            spawnflags='1')
    t.brush('env_bubbles', targetname='n', size=(64, 64, 64), at=(100, 100, 0), tex='AAATRIGGER', current='-300',
            spawnflags='1')
    t.wait(0.2)
    t.snap('s', 'p', 'rendercolor')
    t.snap('s', 'n', 'rendercolor')
    t.expect.value('s', 'p', 'rendercolor', [1, 44, 0])
    t.expect.value('s', 'n', 'rendercolor', [1, 44, 1])


@S.test('speaker "Volume (10 = loudest)" 4: it says its sentence at volume 0.4 (health x 0.1); 10 at 1.0',
        players=1, doc=DOC, covers=('speaker.health',))
def speaker_volume(t):
    t.point('speaker', targetname='lo', at=(-100, 100, 64), spawnflags='1', health='4', message='C1A2_')
    t.point('speaker', targetname='hi', at=(100, 100, 64), spawnflags='1', health='10', message='C1A2_')
    t.wait(0.2)
    t.mark('lo')
    t.fire('lo', use='on')
    t.wait(1.5)
    t.fire('lo', use='off')
    t.wait(8.5)                 # a speaker that talks holds the others for 5 s
    t.mark('hi')
    t.fire('hi', use='on')
    t.wait(1.5)
    t.fire('hi', use='off')
    t.expect.sound('C1A2_', ent='lo', count=1, vol=(0.39, 0.41), after='lo', within=(0, 1.0))
    t.expect.sound('C1A2_', ent='hi', count=1, vol=(0.99, 1.0), after='hi', within=(0, 1.0))


def jump_case(height, apex, rise, dx):
    auto = ' (0: automatic: the target is not above him, so his 36 + 16)' if height == '0' else ''

    @S.test('trigger_jump "height" %s: the player rises %d units to its apex (at %.2f s), near half the way to its '
            'target 200 units off%s' % (height, rise, apex, auto), doc='TRIGGER_JUMP.md', players=1,
            size=(512, 512, 512), covers=('trigger_jump.height',))
    def case(t):
        t.point('info_target', targetname='land', at=(100, 150, 0))
        t.trigger('trigger_jump', size=(64, 64, 64), at=(-100, 150, 0), target='land', height=height)
        t.teleport('player1', (-100, 0, 0))     # beside the pad
        t.wait(0.5)
        t.snap('pre', 'player1', 'origin')
        t.teleport('player1', (-100, 150, 0))
        t.wait(apex)
        t.snap('apex', 'player1', 'origin')
        t.expect.moved('pre', 'apex', 'player1', delta=(dx, 150, rise), tol=25)
    case.__name__ = 'jump_height_%s' % height
    return case


jump_case('300', 0.85, 300, 95)
jump_case('0', 0.35, 52, 82)


@S.test('trigger_counter "message": the player whose use completes the count (2) gets it (HudText), not on the first',
        players=1, doc=DOC, covers=('trigger_counter.message',))
def counter_message(t):
    t.brush('trigger_counter', size=(32, 32, 32), at=(0, 100, 0), tex='AAATRIGGER', targetname='cnt', count='2',
            message='ET_COUNTER_DONE', target='sink', spawnflags='1')
    t.point('info_target', targetname='sink')
    t.wait(0.2)
    t.mark('one')
    t.fire('cnt', activator='player1')
    t.wait(0.5)
    t.mark('two')
    t.fire('cnt', activator='player1')
    t.wait(0.5)
    t.expect.msg('HudText', to='player1', fields={'text': 'ET_COUNTER_DONE'}, count=0, after='one', within=(0, 0.4))
    t.expect.msg('HudText', to='player1', fields={'text': 'ET_COUNTER_DONE'}, count=1, after='two')
    t.expect.fired('sink', count=1, after='two')


@S.test('env_blood "Amount of blood (damage to simulate)": drips 50 are a blood sprite of scale 10 (x 2 in '
        'multiplayer, / 10), 150 the largest, 16; a Blood Stream (2) of 120 has speed 120', doc='FEATUREFUL_OPTIONS.md',
        covers=('env_blood.amount',))
def blood_amount(t):
    t.point('env_blood', targetname='d50', at=(-150, 0, 48), color='1', amount='50')
    t.point('env_blood', targetname='d150', at=(0, 0, 48), color='1', amount='150')
    t.point('env_blood', targetname='s120', at=(150, 0, 48), color='1', amount='120', spawnflags='2')
    t.fire('d50')
    t.fire('d150')
    t.fire('s120')
    t.wait(0.1)
    for x, scale in ((-150, 10), (0, 16)):
        at = [float(v) for v in t._abs((x, 0, 48))]
        t.expect.msg('svc:23', fields={'te': TE_BLOODSPRITE, 'scale': scale, 'pos': at}, count=1)
    t.expect.msg('svc:23', fields={'te': TE_BLOODSTREAM, 'speed': 120}, count=1)


TE_BREAKMODEL = 108


@S.test('func_pushable "m_iGibs" (Breakable, 128): 7 breaks into 7 gibs (TE_BREAKMODEL count); -1 into none',
        players=1, doc=DOC, covers=('func_pushable.m_iGibs',))
def pushable_gibs(t):
    t.brush('func_pushable', targetname='g7', size=(32, 32, 32), at=(-100, 60, 0), health='10', spawnflags='128',
            m_iGibs='7')
    t.brush('func_pushable', targetname='gn', size=(32, 32, 32), at=(100, 60, 0), health='10', spawnflags='128',
            m_iGibs='-1')
    t.wait(0.2)
    t.mark('go')
    t.damage('g7', 100, attacker='player1')
    t.wait(0.3)
    t.mark('none')
    t.damage('gn', 100, attacker='player1')
    t.wait(0.3)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL, 'count': 7}, after='go', within=(0, 0.3), count=1)
    t.expect.msg('svc:23', fields={'te': TE_BREAKMODEL}, after='none', count=0)
    t.expect.removed('@func_pushable', count=1, after='none')     # it broke (Die clears its name)


@S.test('func_illusionary Contents -7 Volumetric Light: its contents go to the clients (skin -7) and a player inside '
        'is not in water (waterlevel 0: the player movement leaves -7 out of the water contents) and walks through; '
        'one with -3 (water, set by hand) still makes him swim', players=1, doc=DOC,
        covers=('func_illusionary.skin=-7',))
def illusionary_light(t):
    t.brush('func_illusionary', targetname='vl', size=(512, 200, 200), at=(0, 60, 0), skin='-7')
    t.brush('func_illusionary', targetname='wt', size=(200, 100, 200), at=(0, -180, 0), skin='-3')
    t.point('info_target', targetname='start', at=(-200, 60, 0))
    t.teleport('player1', (-200, 60, 0), yaw=0)
    t.wait(0.5)
    t.snap('in', 'player1', 'waterlevel', 'watertype')
    t.snap('in', 'vl', 'skin')
    t.walk('player1', 250, 0, 1.2)
    t.snap('walked', 'player1', 'origin', 'waterlevel')
    t.snap('walked', 'start', 'origin')
    t.teleport('player1', (0, -180, 0), yaw=90)
    t.wait(0.5)
    t.snap('water', 'player1', 'waterlevel')
    t.teleport('player1', (0, 200, 0), yaw=90)
    t.wait(0.3)
    t.expect.value('in', 'vl', 'skin', -7)
    t.expect.value('in', 'player1', 'waterlevel', 0)
    t.expect.value('walked', 'player1', 'waterlevel', 0)
    t.expect.distance('walked', 'player1', 'start', 200, 400)     # walking speed, not swimming
    t.expect.value('water', 'player1', 'waterlevel', 2, op='>=')


@S.test('trigger_camera "Initial Speed" 250: on a path whose path_corners set no speed it moves at 250 from the '
        'start; 0: it stays where it is', players=1, doc=DOC, covers=('trigger_camera.speed',))
def camera_speed(t):
    t.point('info_target', targetname='look', at=(0, 200, 40))
    t.point('path_corner', targetname='c1', at=(-200, -100, 100), target='c2', speed='0')
    t.point('path_corner', targetname='c2', at=(200, -100, 100), target='c1', speed='0')
    for name, speed in (('fast', '250'), ('still', '0')):
        t.point('trigger_camera', targetname=name, at=(-200, -100, 100), target='look', moveto='c1', speed=speed,
                wait='10', spawnflags='4')
    t.fire('fast', activator='player1')
    t.wait(0.5)
    t.snap('fast', 'fast', 'speed', 'origin')
    t.fire('fast', activator='player1', use='off')
    t.wait(0.3)
    t.fire('still', activator='player1')
    t.wait(0.5)
    t.snap('still', 'still', 'speed', 'origin')
    t.fire('still', activator='player1', use='off')
    t.wait(0.3)
    t.expect.value('fast', 'fast', 'speed', 250, tol=1)
    t.expect.value('fast', 'fast', 'origin.x', t._abs((-200 + 20, 0, 0))[0], op='>')      # on its way east
    t.expect.value('still', 'still', 'speed', 0)
    t.expect.value('still', 'still', 'origin.x', t._abs((-200, 0, 0))[0], tol=1)


@S.test('trigger_copyvalue "Append spaces (for strings)" 3: the string it copies (Replace) gets 3 spaces after it; '
        'Append adds the copy and 2 spaces', doc='TRIGGER_CHANGEVALUE.md#trigger_copyvalue',
        covers=('trigger_copyvalue.m_iAppendSpaces',))
def copy_append_spaces(t):
    t.point('info_target', targetname='src', **{'$s_s': 'ab'})
    t.point('info_target', targetname='dr', **{'$s_d': 'x'})
    t.point('info_target', targetname='da', **{'$s_d': 'cd'})
    t.point('trigger_copyvalue', targetname='cr', netname='src', m_iszSrcValueName='$s_s', target='dr',
            m_iszDstValueName='$s_d', m_iszValueType='0', m_iAppendSpaces='3')
    t.point('trigger_copyvalue', targetname='ca', netname='src', m_iszSrcValueName='$s_s', target='da',
            m_iszDstValueName='$s_d', m_iszValueType='11', m_iAppendSpaces='2')
    t.fire('cr')
    t.fire('ca')
    t.wait(0.1)
    t.snap('s', 'dr', 'kv:$s_d')
    t.snap('s', 'da', 'kv:$s_d')
    t.expect.value('s', 'dr', 'kv:$s_d', 'ab   ')
    t.expect.value('s', 'da', 'kv:$s_d', 'cdab  ')


TE_WORLDDECAL = 116
RED, YELLOW = range(27, 33), range(216, 222)      # decals.wad: {blood1 .. 6, {yblood1 .. 6


@S.test('env_shooter "Gib Blood Color": 247 (red) gibs leave red blood decals where they bounce, 195 (yellow) '
        'yellow ones, 0 (Default) none', players=0, size=(512, 512, 256), doc=DOC,
        covers=('env_shooter.bloodcolor', 'env_shooter.bloodcolor=0', 'env_shooter.bloodcolor=247',
                'env_shooter.bloodcolor=195'))
def shooter_bloodcolor(t):
    for n, c in (('s0', '0'), ('s247', '247'), ('s195', '195')):
        t.point('env_shooter', at=(0, 0, 150), targetname=n, m_iGibs='4', delay='0.1', m_flVelocity='150',
                m_flVariance='0.3', m_flGibLife='1', shootmodel='models/hgibs.mdl', angles='90 0 0', bloodcolor=c)
    t.wait(0.2)
    for n in ('s0', 's247', 's195'):
        t.mark(n)
        t.fire(n)
        t.wait(4.0)             # they fall, bounce and fade (life 1 s) before the next shooter
    t.expect.msg('svc:23', fields={'te': TE_WORLDDECAL}, count=0, after='s0', within=(0, 4.0))
    t.expect.msg('svc:23', fields={'te': TE_WORLDDECAL}, min=1, after='s247', within=(0, 4.0))
    t.expect.msg('svc:23', fields={'te': TE_WORLDDECAL}, min=1, after='s195', within=(0, 4.0))
    for i in YELLOW:
        t.expect.msg('svc:23', fields={'te': TE_WORLDDECAL, 'index': i}, count=0, after='s247', within=(0, 4.0))
    for i in RED:
        t.expect.msg('svc:23', fields={'te': TE_WORLDDECAL, 'index': i}, count=0, after='s195', within=(0, 4.0))


@S.test('game_player_team "Remove On fire" (1) and "master": fired by the player while its master is locked it does '
        'nothing and stays; unlocked, fired, it is removed', players=1, doc=DOC,
        covers=('game_player_team', 'game_player_team:1', 'game_player_team.master'))
def player_team_master(t):
    t.point('multisource', targetname='gate')
    t.point('game_player_team', targetname='pt', master='gate', spawnflags='1')
    t.point('trigger_relay', targetname='open', target='gate')
    t.wait(0.2)
    t.mark('locked')
    t.fire('pt', activator='player1')
    t.wait(0.5)
    t.mark('open')
    t.fire('open', activator='player1')
    t.wait(0.2)
    t.fire('pt', activator='player1')
    t.wait(0.5)
    t.expect.not_removed('pt', after='locked', within=(0, 0.5))
    t.expect.removed('pt', after='open')


@S.test('monster_furniture "lightorigin": a player whose HUD is set up again gets the info_light_origin\'s position '
        'for it (LightOrigin); one without the key: nothing', players=1, doc='ENV_DLIGHT.md#info_light_origin',
        covers=('monster_furniture.lightorigin',))
def furniture_light_origin(t):
    t.point('info_light_origin', at=(64, -96, 48), targetname='lo')
    t.point('monster_furniture', targetname='fl', at=(-100, 120, 0), model='models/controller.mdl', lightorigin='lo',
            names=('lightorigin',))
    t.point('monster_furniture', targetname='fp', at=(100, 120, 0), model='models/controller.mdl')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(2.0)
    t.expect.msg('LightOrigin', to='player1', count=1, after='fu')


def invert_hit_case(cls, east):
    # each class's lasers shoot their own way (the hit points of the other test are still in the map)
    way, back = ('east', 'west') if east else ('north', 'south')
    fwd = [1, 0, 0] if east else [0, 1, 0]

    @S.test('%s "Invert Hit Vector" (16): a laser shooting %s at it fires Trigger when hit with a Locus point whose '
            'direction (velocity) is turned around, %s; without the flag %s' % (cls, way, back, way), players=1,
            doc=DOC, covers=('%s:16' % cls,))
    def case(t):
        for n, k, fl in (('inv', -100, 16), ('nrm', 100, 0)):
            start, end, at = ((-200, k, 40), (200, k, 40), (60, k, 8)) if east else ((k, -150, 40), (k, 200, 40),
                                                                                      (k, 60, 8))
            t.point('info_target', targetname=n + '_end', at=end)
            t.point('env_laser', targetname=n + '_las', at=start, LaserTarget=n + '_end',
                    texture='sprites/laserbeam.spr', damage='1', names=('LaserTarget',))
            if cls == 'func_pushable':
                fl |= 128
            t.brush(cls, targetname=n, size=(64, 64, 64), at=at, material='1', health='100000',
                    whenhit=n + '_hit', spawnflags=str(fl), names=('whenhit',))
            t.point('info_target', targetname=n + '_hit')
        t.wait(0.2)
        t.fire('inv_las', use='on')
        t.fire('nrm_las', use='on')
        t.wait(0.5)
        t.snap('s', '@info_target', 'velocity')
        t.fire('inv_las', use='off')
        t.fire('nrm_las', use='off')
        t.expect.used('inv_hit', min=1)
        t.expect.used('nrm_hit', min=1)
        # the hit points (info_targets the breakables made): one heading back, one on
        bwd = [-c for c in fwd]
        t.expect.value('s', '@info_target', 'velocity', bwd, where={'velocity': bwd})
        t.expect.value('s', '@info_target', 'velocity', fwd, where={'velocity': fwd})
    case.__name__ = cls.replace('func_', '') + '_invert_hit'
    return case


invert_hit_case('func_breakable', False)
invert_hit_case('func_pushable', True)


@S.test('ambient_extraspeaker "Start Silent" (16) on a named entity: silent at the map start, so the first trigger '
        'starts it on that entity (on its extra_speaker); without the flag it played from the map start and the '
        'first trigger only stops it', players=1, doc=DOC, covers=('ambient_extraspeaker:16',))
def es_start_silent(t):
    t.point('info_target', targetname='host', at=(-60, 100, 32))
    t.point('info_target', targetname='host2', at=(60, 100, 32))
    t.point('ambient_extraspeaker', targetname='es', message='ambience/alien_beacon.wav', target='host',
            spawnflags='20')
    t.point('ambient_extraspeaker', targetname='es2', message='ambience/alien_blipper.wav', target='host2',
            spawnflags='4')
    t.wait(0.5)
    t.mark('go')
    t.fire('es')
    t.fire('es2')
    t.wait(0.3)
    t.expect.sound('alien_beacon', after='go', within=(0, 0.1), count=1)
    t.expect.sound('alien_blipper', after='go', within=(0, 0.1), count=1, flags=32)
    t.expect.sound('alien_blipper', after='go', within=(0, 0.1), count=1)


@S.test('func_rainvolume "Start Active" (1) with a name: on in the list a player gets when he spawns (type 1); one '
        'without the flag: off (type 0)', players=1, doc='WEATHER.md#rain-and-snow-volumes',
        covers=('func_rainvolume:1',))
def rain_start_active(t):
    t.brush('func_rainvolume', targetname='ron', size=(64, 64, 64), at=(-100, 0, 0), tex='AAATRIGGER', intensity='13',
            spawnflags='1')
    t.brush('func_rainvolume', targetname='roff', size=(64, 64, 64), at=(100, 0, 0), tex='AAATRIGGER',
            intensity='14')
    t.mark('spawn')
    t.console('test_spawn player1')
    t.wait(0.5)
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45),
                 fields={'type': 1, 'intensity': 13})
    t.expect.msg('WeatherVol', to='player1', count=1, after='spawn', within=(0, 0.45),
                 fields={'type': 0, 'intensity': 14})


@S.test('scripted_tanksequence "Dump player" (1): fired while the player mans the tank, it takes it from him and runs '
        '(the tank fires once at the end: its Target fires); one without the flag leaves the manned tank alone',
        players=1, doc='FEATUREFUL_OPTIONS.md', covers=('scripted_tanksequence:1',))
def tankseq_dump(t):
    t.flags('player1', '+FL_NOTARGET')
    t.brush('func_tank', size=(32, 32, 32), at=(0, 100, 40), origin=True, targetname='gun', spawnflags='32',
            yawrate='30', yawrange='180', pitchrate='30', pitchrange='30', barrel='16', firerate='2', bullet='1',
            bullet_damage='0', target='shot')
    t.point('info_target', targetname='shot')
    t.brush('func_tankcontrols', size=(64, 64, 96), at=(0, 0, 0), target='gun', targetname='tc')
    for n, fl in (('keep', '0'), ('dump', '1')):
        t.point('scripted_tanksequence', targetname=n, at=(0, 0, 64), m_iszEntity='gun', m_iTurn='0', m_iShoot='1',
                m_iUntil='0', m_fDuration='0.1', spawnflags=fl, names=('m_iszEntity',))
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(1.0)
    t.fire('tc', activator='player1')     # he mans it
    t.wait(1.0)
    t.mark('keep')
    t.fire('keep')
    t.wait(1.5)
    t.mark('dump')
    t.fire('dump')
    t.wait(1.5)
    t.expect.not_used('shot', after='keep', within=(0, 1.4))
    t.expect.used('shot', min=1, after='dump')


ELIGHT_ON, DLIGHT_POS = 0x81, 2       # KeyedDLight state: on + the elight flag; a position update


@S.test('env_elight "Position" with Position is Locus Entity (8): on a func_wall (drawn: the client follows it, its '
        'index sent); on an info_target (no model): its origin, nothing to follow', doc='ENV_DLIGHT.md#env_elight',
        covers=('env_elight.message', 'env_elight:8'))
def elight_entity(t):
    t.brush('func_wall', targetname='wall', size=(32, 32, 32), at=(100, 100, 0))
    t.point('info_target', at=(-100, 100, 50), targetname='tgt')
    t.point('env_elight', at=(0, 0, 64), targetname='el_w', message='wall', renderamt='100', spawnflags='8',
            names=('message',))
    t.point('env_elight', at=(0, 0, 64), targetname='el_t', message='tgt', renderamt='120', spawnflags='8',
            names=('message',))
    t.fire('el_w')
    t.wait(0.1)
    t.mark('t')
    t.fire('el_t')
    t.wait(0.1)
    t.expect.msg('KeyedDLight', count=1, fields={'state': ELIGHT_ON, 'radius': 100}, within=(0, 0.1))
    t.expect.msg('KeyedDLight', count=0, fields={'state': ELIGHT_ON, 'radius': 100, 'follow': 0}, within=(0, 0.1))
    t.expect.msg('KeyedDLight', count=1, fields={'state': ELIGHT_ON, 'radius': 120, 'follow': 0}, after='t')


@S.test('env_elight "Position" !activator with Track position updates (4): the light moves with the player '
        '(position messages), none while he stands still', players=1, doc='ENV_DLIGHT.md#env_elight',
        covers=('env_elight.message', 'env_elight:4'))
def elight_track(t):
    t.point('env_elight', at=(0, 0, 64), targetname='el', message='!activator', renderamt='100', spawnflags='4')
    t.teleport('player1', (-100, 0, 0))
    t.wait(0.3)
    t.fire('el', activator='player1')
    t.wait(0.5)
    t.mark('still')
    t.wait(0.5)
    t.mark('move')
    t.teleport('player1', (100, 0, 0))
    t.wait(0.5)
    t.fire('el', use='off')
    t.expect.msg('KeyedDLight', count=1, fields={'state': ELIGHT_ON})
    t.expect.msg('KeyedDLight', count=0, fields={'state': DLIGHT_POS}, after='still', within=(0, 0.45))
    t.expect.msg('KeyedDLight', min=1, fields={'state': DLIGHT_POS}, after='move', within=(0, 0.5))


@S.test('func_mortar_field Targeting 2 (Table): the shell lands at the X / Y Controllers\' positions (their '
        'ideal_yaw, 0 .. 1 across the field: 0.25, 0.75); 0 (Random): anywhere inside the field', players=1,
        doc=DOC, covers=('func_mortar_field.m_fControl=2', 'func_mortar_field.m_fControl=0',
                         'func_mortar_field.m_iszXController', 'func_mortar_field.m_iszYController'))
def mortar_table(t):
    t.flags('player1', '+FL_NOTARGET')
    t.flags('player1', '+FL_GODMODE')
    t.teleport('player1', (-220, -220, 0))
    # the table field: 400 x 400 around the room's middle, 64 above the floor
    t.brush('func_mortar_field', targetname='tbl', size=(400, 400, 32), at=(0, 0, 64), tex='AAATRIGGER',
            m_fControl='2', m_iszXController='cx', m_iszYController='cy', m_flSpread='0', m_iCount='1',
            names=('m_iszXController', 'm_iszYController'))
    t.point('info_target', targetname='cx', ideal_yaw='0.25')
    t.point('info_target', targetname='cy', ideal_yaw='0.75')
    t.point('info_target', targetname='spot', at=(-100, 100, 0))
    t.brush('func_mortar_field', targetname='rnd', size=(64, 64, 32), at=(150, -150, 64), tex='AAATRIGGER',
            m_fControl='0', m_flSpread='0', m_iCount='1')
    t.point('info_target', targetname='rspot', at=(150, -150, 0))
    t.wait(0.2)
    t.mark('tbl')
    t.fire('tbl')
    t.wait(0.1)
    t.snap('t', '@monster_mortar', 'origin')
    t.snap('t', 'spot', 'origin')
    t.wait(3.5)                 # it has gone off
    t.mark('rnd')
    t.fire('rnd')
    t.wait(0.1)
    t.snap('r', '@monster_mortar', 'origin')
    t.snap('r', 'rspot', 'origin')
    t.wait(3.5)
    t.flags('player1', '-FL_GODMODE')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.distance('t', '@monster_mortar', 'spot', 0, 2, flat=True)
    t.expect.distance('r', '@monster_mortar', 'rspot', 0, 46, flat=True)     # inside the 64 x 64 field


@S.test('game_zone_player "outcount": fired, it sets the game_counter to the number of players outside the zone (1: '
        'the player); its In Count one to those inside (0)', players=1, doc=DOC,
        covers=('game_zone_player.outcount',))
def zone_outcount(t):
    t.point('game_counter', targetname='oc', health='100')
    t.point('game_counter', targetname='ic', health='100', frags='5')
    t.trigger('game_zone_player', size=(128, 128, 96), at=(0, 120, 0), targetname='zone', outcount='oc',
              incount='ic')
    t.teleport('player1', (0, -150, 0))
    t.wait(0.3)
    t.mark('go')
    t.fire('zone', activator='player1')
    t.wait(0.2)
    t.snap('s', 'oc', 'frags')
    t.snap('s', 'ic', 'frags')
    t.expect.fired('oc', use=2, after='go', count=1)
    t.expect.value('s', 'oc', 'frags', 1)
    t.expect.value('s', 'ic', 'frags', 0)


@S.test('func_train "Damage on crush" 25: coming down on a monster it can\'t push into the floor, it hurts him 25 a '
        'hit (DMG_CRUSH, every 0.5 s); left out: Half-Life\'s 2; 0 with No default damage (64): no damage', players=1,
        doc=DOC, size=(512, 512, 320), covers=('func_train.dmg',))
def train_crush(t):
    t.flags('player1', '+FL_NOTARGET')
    for n, x, kv in (('d25', -150, {'dmg': '25'}), ('d2', 0, {}), ('d0', 150, {'dmg': '0', 'spawnflags': '64'})):
        t.point('monster_barney', targetname=n + '_b', at=(x, 0, 0), angles='0 270 0', spawnflags='16')
        t.point('path_corner', targetname=n + '_up', at=(x, 0, 200), target=n + '_dn')
        t.point('path_corner', targetname=n + '_dn', at=(x, 0, 40), target=n + '_up', wait='1')
        t.brush('func_train', targetname=n, size=(64, 64, 16), at=(x, 0, 192), target=n + '_up', speed='150', **kv)
    t.wait(0.3)
    for n in ('d25', 'd2', 'd0'):
        t.kv(n + '_b', 'health', '100000')
    t.mark('go')
    for n in ('d25', 'd2', 'd0'):
        t.fire(n)
    t.wait(3.0)
    for n in ('d25', 'd2', 'd0'):
        t.fire(n)                       # stop
    t.expect.damaged('d25_b', attacker='d25', type=1, min=1, after='go')
    t.expect.damaged('d25_b', attacker='d25', type=1, total=(25, 175), after='go')
    t.expect.damaged('d2_b', attacker='d2', type=1, total=(2, 14), after='go')
    t.expect.damaged('d0_b', type=1, total=(0, 0), min=0, after='go')

