"""color_correction / color_correction_volume (docs/COLOR_CORRECTION.md): every key in the "ColorCorr" message
(decoded field by field), the defaults, the resend at a HUD re-initialization (fullupdate, the first one flagged),
toggling and the point entity's fade state in the message, StartDisabled, the master, keys changed at run time
(trigger_changevalue's path), the file refused at run time, removal, the precache of the table file and bad files.
The client side (weights, the grade) is tests/colorcorrection."""

from etest import Suite

SUITE = Suite('colorcorrection', 'color_correction, color_correction_volume: color grading state to the clients',
              players=1, requires=('ColorCorr_SendAll',))
S = SUITE
SUITE.server_args = ('+developer', '1')    # the entities' console lines (ALERT)
DOC = 'COLOR_CORRECTION.md'
RAW = 'gfx/colorcorrection/et_test.raw'
CUBE = 'gfx/colorcorrection/et_test.cube'
SUITE.add_file(RAW, bytes(98304))
SUITE.add_file(CUBE, b'LUT_3D_SIZE 2\n' + b'0 0 0\n' * 8)


# Every test's entities are in the suite's one map and a fullupdate sends them all: each test tags its own with a
# gamma of its own, and the expectations name it.


def fullupdate(t, label='fu'):
    t.mark(label)
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)     # the server runs it with the player's next command


@S.test('point: every key in the message, the file precached as a generic resource', players=1, doc=DOC,
        covers=('color_correction', 'color_correction.filename', 'color_correction.maxweight',
                'color_correction.minfalloff', 'color_correction.maxfalloff', 'color_correction.fadeInDuration',
                'color_correction.fadeOutDuration', 'color_correction.exclusive=1', 'color_correction.saturation',
                'color_correction.contrast', 'color_correction.brightness', 'color_correction.gamma',
                'color_correction.tintcolor', 'color_correction.tintamount', 'color_correction.StartDisabled=0'))
def point_keys(t):
    t.point('color_correction', targetname='cc', filename=RAW, maxweight='0.75', minfalloff='64', maxfalloff='512',
            fadeInDuration='1.5', fadeOutDuration='2.25', exclusive='1', saturation='0', contrast='1.4',
            brightness='-0.1', gamma='1.2', tintcolor='255 200 130', tintamount='0.8', StartDisabled='0')
    fullupdate(t)
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu',
                 fields={'enabled': 1, 'volume': 0, 'exclusive': 1, 'first': 1, 'maxweight': 0.75, 'weight': 0.75,
                         'fadein': 1.5, 'fadeout': 2.25, 'minfalloff': 64, 'maxfalloff': 512, 'saturation': 0,
                         'contrast': 1.4, 'brightness': -0.1, 'gamma': 1.2, 'tint_r': 255, 'tint_g': 200,
                         'tint_b': 130, 'tintamount': 0.8, 'file': RAW})
    t.expect.precached(RAW, kind='generic')


@S.test('point, keys left out: on, weight 1, no falloff (-1 / -1), no fades, neutral simple keys, no file', players=1, doc=DOC,
        covers=('color_correction',))
def point_defaults(t):
    t.point('color_correction', targetname='cc')
    fullupdate(t)
    t.expect.msg('ColorCorr', to='player1', min=1, after='fu',
                 fields={'enabled': 1, 'volume': 0, 'exclusive': 0, 'maxweight': 1, 'weight': 1, 'fadein': 0,
                         'fadeout': 0, 'minfalloff': -1, 'maxfalloff': -1, 'saturation': 1, 'contrast': 1,
                         'brightness': 0, 'gamma': 1, 'tint_r': 255, 'tint_g': 255, 'tint_b': 255, 'tintamount': 0,
                         'file': ''})


@S.test('volume: its brush model, fade time in both fade fields (default 1 s), a bare file name is in '
        'gfx/colorcorrection/, a .cube precached', players=1, doc=DOC,
        covers=('color_correction_volume', 'color_correction_volume.fadeDuration', 'color_correction_volume.filename',
                'color_correction_volume.saturation'))
def volume(t):
    t.trigger('color_correction_volume', size=(128, 128, 128), targetname='a', fadeDuration='2.5', saturation='0.5',
              gamma='1.01')
    t.trigger('color_correction_volume', size=(64, 64, 64), at=(160, 0, 0), targetname='b', filename='et_test.cube')
    fullupdate(t)
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu',
                 fields={'volume': 1, 'enabled': 1, 'fadein': 2.5, 'fadeout': 2.5, 'saturation': 0.5, 'gamma': 1.01,
                         'model': {'range': (1, 512)}, 'file': ''})
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu',
                 fields={'volume': 1, 'fadein': 1, 'fadeout': 1, 'file': CUBE})
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu', fields={'first': 1})
    t.expect.precached(CUBE, kind='generic')


@S.test('toggle: Use turns it off and on, a message each time; the point\'s weight in it is where its fade is', players=1, doc=DOC,
        covers=('color_correction', 'color_correction.fadeOutDuration', 'color_correction.fadeInDuration'))
def toggle(t):
    t.point('color_correction', targetname='cc', fadeInDuration='2', fadeOutDuration='2', saturation='0', gamma='1.02')
    t.mark('off')
    t.fire('cc')
    t.wait(1.0)
    fullupdate(t, 'mid')        # 1 s into a 2 s fade out: about half
    t.wait(1.5)
    t.mark('on')
    t.fire('cc', use='on')
    t.wait(0.2)
    t.mark('again')
    t.fire('cc', use='on')      # on already: nothing
    t.wait(0.2)
    t.expect.msg('ColorCorr', to='player1', count=1, after='off', within=(0, 0.2), fields={'enabled': 0, 'weight': 1})
    t.expect.msg('ColorCorr', to='player1', count=1, after='mid',
                 fields={'gamma': 1.02, 'enabled': 0, 'weight': {'range': (0.3, 0.7)}})
    t.expect.msg('ColorCorr', to='player1', count=1, after='on', within=(0, 0.15), fields={'enabled': 1, 'weight': 0})
    t.expect.no_msg('ColorCorr', to='player1', after='again')


@S.test('StartDisabled: off from the start (weight 0), on fades from 0', players=1, doc=DOC,
        covers=('color_correction', 'color_correction.StartDisabled=1', 'color_correction_volume.StartDisabled=1'))
def start_disabled(t):
    t.point('color_correction', targetname='cc', StartDisabled='1', fadeInDuration='2', gamma='1.03')
    t.trigger('color_correction_volume', size=(64, 64, 64), targetname='vol', StartDisabled='1', gamma='1.03')
    fullupdate(t)
    t.mark('on')
    t.fire('cc')
    t.fire('vol')
    t.wait(1.0)
    fullupdate(t, 'mid')
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu', within=(0, 0.25),
                 fields={'gamma': 1.03, 'volume': 0, 'enabled': 0, 'weight': 0})
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu', within=(0, 0.25),
                 fields={'gamma': 1.03, 'volume': 1, 'enabled': 0})
    t.expect.msg('ColorCorr', to='player1', count=1, after='on', within=(0, 0.2), fields={'volume': 1, 'enabled': 1})
    t.expect.msg('ColorCorr', to='player1', count=1, after='mid',
                 fields={'gamma': 1.03, 'volume': 0, 'enabled': 1, 'weight': {'range': (0.3, 0.7)}})


@S.test('master: off while the master is off, a message when it changes either way', players=1, doc=DOC,
        covers=('color_correction', 'color_correction.master', 'color_correction_volume.master', 'multisource'))
def master(t):
    t.point('multisource', targetname='ms')
    t.point('trigger_relay', targetname='on', target='ms', triggerstate='1')
    t.point('color_correction', targetname='cc', master='ms', gamma='1.04')
    t.trigger('color_correction_volume', size=(64, 64, 64), targetname='vol', master='ms', gamma='1.04')
    t.wait(0.3)
    fullupdate(t)
    t.mark('open')
    t.fire('on')        # the relay is the multisource's member: the master opens
    t.wait(0.4)
    t.expect.msg('ColorCorr', to='player1', count=2, after='fu', within=(0, 0.25), fields={'gamma': 1.04, 'enabled': 0})
    t.expect.msg('ColorCorr', to='player1', count=1, after='open', fields={'volume': 0, 'enabled': 1})
    t.expect.msg('ColorCorr', to='player1', count=1, after='open', fields={'volume': 1, 'enabled': 1})


@S.test('keys changed at run time: one message with the new values; the file can\'t change (precache)', players=1, doc=DOC,
        covers=('color_correction', 'color_correction.saturation', 'color_correction.tintamount',
                'color_correction.filename', 'trigger_changevalue'))
def runtime_keys(t):
    t.point('color_correction', targetname='cc', filename=RAW)
    for name, key, value in (('cv1', 'saturation', '0.25'), ('cv2', 'tintamount', '0.5'), ('cv3', 'filename', 'other.raw')):
        t.point('trigger_changevalue', targetname=name, target='cc', m_iszValueName=key, m_iszNewValue=value,
                m_iszValueType='0')
    t.wait(0.2)
    t.mark('kv')
    for name in ('cv1', 'cv2', 'cv3'):
        t.fire(name)
    t.wait(0.4)
    t.expect.msg('ColorCorr', to='player1', count=1, after='kv',
                 fields={'saturation': 0.25, 'tintamount': 0.5, 'file': RAW})
    t.expect.console("its file can't be changed at run time")
    t.expect.not_precached('gfx/colorcorrection/other.raw')


@S.test('killtarget: the clients are told it is gone', players=1, doc=DOC, covers=('color_correction', 'color_correction_volume'))
def removed(t):
    t.point('color_correction', targetname='cc')
    t.trigger('color_correction_volume', size=(64, 64, 64), targetname='vol')
    t.point('trigger_relay', targetname='kill', killtarget='cc')
    t.point('trigger_relay', targetname='kill2', killtarget='vol')
    t.wait(0.2)
    t.mark('go')
    t.fire('kill')
    t.fire('kill2')
    t.wait(0.3)
    t.expect.msg('ColorCorr', to='player1', count=2, after='go', fields={'removed': 1})
    t.expect.removed('cc')


@S.test('bad files (outside gfx/colorcorrection/, not .raw / .cube, "..") and a volume without a brush: an error, '
        'nothing sent, nothing precached', players=1, doc=DOC, covers=('color_correction', 'color_correction.filename'))
def bad_file(t):
    t.point('color_correction', targetname='a', filename='gfx/env/boatft.tga')
    t.point('color_correction', targetname='b', filename='maps/x.raw')
    t.point('color_correction', targetname='c', filename='gfx/colorcorrection/../../x.raw')
    t.point('color_correction_volume', targetname='d')
    fullupdate(t)
    t.mark('go')
    for n in ('a', 'b', 'c', 'd'):
        t.fire(n)
    t.wait(0.2)
    t.expect.no_msg('ColorCorr', to='player1', after='go')
    t.expect.msg('ColorCorr', to='player1', count=0, min=0, after='fu', fields={'file': 'gfx/env/boatft.tga'})
    t.expect.not_precached('maps/x.raw')
    t.expect.console('bad file')
    t.expect.console('without a brush')


@S.test('volume: every key in the message (Exclusive Yes, Start Disabled No); a second volume and a point with '
        'Exclusive No send exclusive 0', players=1, doc=DOC,
        covers=('color_correction_volume.maxweight', 'color_correction_volume.exclusive',
                'color_correction_volume.exclusive=1', 'color_correction_volume.exclusive=0',
                'color_correction_volume.contrast', 'color_correction_volume.brightness',
                'color_correction_volume.gamma', 'color_correction_volume.tintcolor',
                'color_correction_volume.tintamount', 'color_correction_volume.startdisabled=0',
                'color_correction.exclusive=0'))
def volume_keys(t):
    t.trigger('color_correction_volume', size=(128, 128, 128), targetname='a', maxweight='0.6', exclusive='1',
              contrast='1.3', brightness='0.2', gamma='1.31', tintcolor='10 200 30', tintamount='0.4',
              StartDisabled='0')
    t.trigger('color_correction_volume', size=(64, 64, 64), at=(160, 0, 0), targetname='b', exclusive='0',
              gamma='1.32')
    t.point('color_correction', targetname='cc', at=(-160, 0, 0), exclusive='0', gamma='1.33')
    fullupdate(t)
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu',
                 fields={'volume': 1, 'enabled': 1, 'exclusive': 1, 'maxweight': 0.6, 'contrast': 1.3,
                         'brightness': 0.2, 'gamma': 1.31, 'tint_r': 10, 'tint_g': 200, 'tint_b': 30,
                         'tintamount': 0.4})
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu', fields={'volume': 1, 'exclusive': 0, 'gamma': 1.32})
    t.expect.msg('ColorCorr', to='player1', count=1, after='fu', fields={'volume': 0, 'exclusive': 0, 'gamma': 1.33})
