"""info_waypoint (docs/INFO_WAYPOINT.md, sdk/syndicate.fgd): the "Waypoint" state message decoded field by field:
on / off, the position, the icon sprite, the color, the label, Show distance, the Follow Entity and its offset; Start
on; On / Off / Toggle; Only the activator sees it (only that player gets it, also at his fullupdate); sent at a HUD
re-initialization (fullupdate: the send queue's clear, then every marker that is on); the label and the Follow Entity
changed by trigger_changevalue; removed by killtarget. The marker on the screen needs the client (the edge placement
is tested offline: tests/waypoint/run_test.sh)."""

from etest import Suite

SUITE = Suite('info_waypoint', 'info_waypoint: HUD marker state (server side)', players=2, requires=('info_waypoint',))
S = SUITE
DOC = 'INFO_WAYPOINT.md#keys'
QUEUE = (0, 0.9)    # the send queue: 0.2 s after the HUD initialization, then a round every 0.1 s


@S.test('Start on, Show distance, a label; the color left out is the FGD default 255 200 40, no Icon Sprite is the '
        'default icon (0); sent at a fullupdate after the clear (id 0)', players=1, doc=DOC,
        covers=('info_waypoint', 'info_waypoint:2', 'info_waypoint:4', 'info_waypoint.message',
                'info_waypoint.rendercolor'))
def defaults(t):
    t.point('info_waypoint', at=(100, 50, 64), message='VAULT', spawnflags='6')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('Waypoint', to='player1', count=1, after='fu', within=QUEUE, fields={'id': 0, 'on': 0})
    t.expect.msg('Waypoint', to='player1', count=1, after='fu', within=QUEUE,
                 fields={'on': 1, 'distance': 1, 'follows': 0, 'label': 'VAULT', 'color': [255, 200, 40],
                         'sprite': 0})


@S.test('without Start on: nothing at a fullupdate; On turns it on for everybody, On again does nothing, Toggle '
        'turns it off, Toggle turns it on again, Off turns it off; a color and an Icon Sprite of the map', players=2,
        doc=DOC,
        covers=('info_waypoint.targetname', 'info_waypoint.rendercolor', 'info_waypoint.model'))
def on_off_toggle(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='wp', rendercolor='10 20 30', model='sprites/glow01.spr')
    t.point('trigger_relay', targetname='wp_on', target='wp', triggerstate='1')
    t.point('trigger_relay', targetname='wp_off', target='wp', triggerstate='0')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('on')
    t.fire('wp_on')
    t.wait(0.2)
    t.mark('again')
    t.fire('wp_on')
    t.wait(0.2)
    t.mark('tog1')
    t.fire('wp')
    t.wait(0.2)
    t.mark('tog2')
    t.fire('wp')
    t.wait(0.2)
    t.mark('off')
    t.fire('wp_off')
    t.wait(0.2)
    t.expect.msg('Waypoint', to='player1', count=0, after='fu', within=QUEUE, fields={'on': 1, 'color': [10, 20, 30]})
    t.expect.msg('Waypoint', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'color': [10, 20, 30], 'custom_sprite': 1, 'distance': 0, 'label': ''})
    t.expect.msg('Waypoint', to='player2', count=1, after='on', within=(0, 0.1), fields={'on': 1})
    t.expect.msg('Waypoint', count=0, after='again', within=(0, 0.15))
    t.expect.msg('Waypoint', to='player1', count=1, after='tog1', within=(0, 0.1), fields={'on': 0})
    t.expect.msg('Waypoint', to='player1', count=1, after='tog2', within=(0, 0.1), fields={'on': 1})
    t.expect.msg('Waypoint', to='player1', count=1, after='off', within=(0, 0.1), fields={'on': 0})


@S.test('flag 1 Only the activator sees it: player1 turns it on and only he gets it, also at his fullupdate; '
        'player2 gets nothing, not even at his own fullupdate; player2 turning it on takes it over (off for player1)',
        players=2, doc=DOC, covers=('info_waypoint:1',))
def activator_only(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='wpa', message='ONLY YOU', spawnflags='1')
    t.wait(0.5)
    t.mark('on')
    t.fire('wpa', activator='player1')
    t.wait(0.3)
    t.mark('fu1')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('fu2')
    t.cmd('player2', 'fullupdate')
    t.wait(1.0)
    t.mark('take')
    t.fire('wpa', activator='player2', use='on')
    t.wait(0.3)
    t.expect.msg('Waypoint', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'label': 'ONLY YOU'})
    t.expect.msg('Waypoint', to='player2', count=0, after='on', within=(0, 1.2), fields={'label': 'ONLY YOU'})
    t.expect.msg('Waypoint', to='player2', count=1, after='fu2', within=QUEUE, fields={'id': 0, 'on': 0})
    t.expect.msg('Waypoint', to='player2', count=0, after='fu2', within=QUEUE, fields={'label': 'ONLY YOU'})
    t.expect.msg('Waypoint', to='player1', count=1, after='fu1', within=QUEUE, fields={'on': 1, 'label': 'ONLY YOU'})
    t.expect.msg('Waypoint', to='player1', count=1, after='take', within=(0, 0.1), fields={'on': 0})
    t.expect.msg('Waypoint', to='player2', count=1, after='take', within=(0, 0.1), fields={'on': 1})


@S.test('flag 1 turned on by something that is not a player: it stays off (nothing sent)', players=2, doc=DOC,
        covers=('info_waypoint:1',))
def activator_only_no_player(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='wpn', message='NOBODY', spawnflags='1')
    t.wait(0.5)
    t.mark('on')
    t.fire('wpn')
    t.wait(0.5)
    t.expect.msg('Waypoint', count=0, after='on', fields={'label': 'NOBODY'})


@S.test('Follow Entity: the marker carries the entity and its offset from it (64 units above the crate); '
        'trigger_changevalue changes the label at once; killtarget removes the marker (off)', players=1, doc=DOC,
        covers=('info_waypoint.target', 'info_waypoint.message'))
def follow_and_changes(t):
    t.brush('func_wall', targetname='crate', size=(32, 32, 32), at=(-150, 150, 0))
    t.point('info_waypoint', at=(0, 0, 64), targetname='wpf', target='crate', message='CARGO', spawnflags='2',
            names=('target',))
    t.point('trigger_changevalue', targetname='cv_label', target='wpf', m_iszValueName='message',
            m_iszNewValue='MOVED')
    t.point('trigger_relay', targetname='kill', killtarget='wpf')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.mark('cv')
    t.fire('cv_label')
    t.wait(0.2)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.2)
    t.expect.msg('Waypoint', to='player1', count=1, after='fu', within=QUEUE,
                 fields={'on': 1, 'follows': 1, 'label': 'CARGO'})
    t.expect.msg('Waypoint', to='player1', count=1, after='cv', within=(0, 0.1),
                 fields={'on': 1, 'follows': 1, 'label': 'MOVED'})
    t.expect.msg('Waypoint', to='player1', count=1, after='kill', within=(0, 0.1), fields={'on': 0})


@S.test('Follow Entity changed by trigger_changevalue: the marker goes onto the new entity at the same offset from '
        'its middle (80 units above the crate); the new entity removed: the marker stays where it was, following '
        'nothing', players=1, doc=DOC, covers=('info_waypoint.target',))
def follow_change(t):
    t.brush('func_wall', targetname='fa', size=(32, 32, 32), at=(-150, 150, 0))
    t.brush('func_wall', targetname='fb', size=(32, 32, 32), at=(150, 150, 0))
    t.point('info_waypoint', at=(-150, 150, 96), targetname='wpc', target='fa', message='MOVES', spawnflags='2',
            names=('target',))
    t.point('trigger_changevalue', targetname='cv_tgt', target='wpc', m_iszValueName='target', m_iszNewValue='fb',
            names=('m_iszNewValue',))
    t.point('trigger_relay', targetname='kill_fb', killtarget='fb')
    t.wait(0.5)
    t.mark('cv')
    t.fire('cv_tgt')
    t.wait(0.3)
    t.mark('kill')
    t.fire('kill_fb')
    t.wait(0.8)
    t.expect.spread('Waypoint', t._abs((150, 150, 96)), 0, 1, fields={'label': 'MOVES', 'follows': 1}, after='cv',
                    within=(0, 0.1))
    t.expect.spread('Waypoint', t._abs((150, 150, 96)), 0, 1, fields={'label': 'MOVES', 'follows': 0}, after='kill',
                    within=(0, 0.7))


@S.test('a moving Follow Entity (a train) that is removed: the marker stays where the train was last seen, not where '
        'it was when the marker turned on', players=1, doc=DOC, covers=('info_waypoint.target',))
def follow_removed_moving(t):
    t.point('path_corner', targetname='pa', target='pb', at=(-200, -150, 8))
    t.point('path_corner', targetname='pb', target='pa', at=(200, -150, 8))
    t.brush('func_train', targetname='tr', target='pa', speed='150', size=(32, 32, 32), at=(-200, -150, 8),
            names=('target',))
    t.point('info_waypoint', at=(-200, -150, 120), targetname='wpt', target='tr', message='TRAIN', spawnflags='2',
            names=('target',))
    t.point('trigger_relay', targetname='kill_tr', killtarget='tr')
    t.wait(0.3)
    t.fire('tr')    # a named train waits for its first trigger
    t.wait(1.7)
    t.mark('kill')
    t.fire('kill_tr')
    t.wait(0.8)
    t.expect.spread('Waypoint', t._abs((-200, -150, 120)), 120, 600, fields={'label': 'TRAIN', 'follows': 0},
                    after='kill', within=(0, 0.7))


@S.test('an Icon Sprite the server does not have: the marker is removed with a console line (the map still loads; '
        'a precache of a missing file would end it)', players=1, doc=DOC, covers=('info_waypoint.model',))
def missing_sprite(t):
    t.point('info_waypoint', at=(0, 0, 64), message='GONE', model='sprites/no_such_waypoint.spr', spawnflags='2')
    t.wait(0.5)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(1.0)
    t.expect.msg('Waypoint', to='player1', count=0, after='fu', fields={'label': 'GONE'})


@S.test('Reached Radius 64, Target When Reached: player2 standing away does nothing; player1 stepping onto the spot '
        'turns the marker off for everybody and fires the target with player1 as the activator', players=2, doc=DOC,
        covers=('info_waypoint.radius', 'info_waypoint.netname'))
def reached(t):
    t.point('info_waypoint', at=(180, 180, 40), targetname='wpr', message='REACH', radius='64', netname='reachhit',
            spawnflags='2', names=('netname',))
    t.point('info_target', targetname='reachhit')
    t.teleport('player1', (-180, -180, 0))
    t.teleport('player2', (-180, 180, 0))
    t.wait(0.5)
    t.mark('far')
    t.wait(0.5)
    t.mark('reach')
    t.teleport('player1', (180, 180, 0))
    t.wait(0.5)
    t.expect.msg('Waypoint', count=0, after='far', within=(0, 0.45), fields={'on': 0})
    t.expect.msg('Waypoint', to='player1', count=1, after='reach', within=(0, 0.3), fields={'on': 0})
    t.expect.msg('Waypoint', to='player2', count=1, after='reach', within=(0, 0.3), fields={'on': 0})
    t.expect.fired('reachhit', activator='player1', count=1, after='reach', within=(0, 0.3))


@S.test('Reached Radius with Only the activator sees it: player2 on the spot does nothing to player1\'s marker; '
        'player1 on the spot turns it off for player1', players=2, doc=DOC,
        covers=('info_waypoint.radius', 'info_waypoint:1'))
def reached_activator(t):
    t.point('info_waypoint', at=(180, -180, 40), targetname='wpra', message='MINE', radius='64', spawnflags='1')
    t.teleport('player1', (-180, 180, 0))
    t.teleport('player2', (-180, -180, 0))
    t.wait(0.3)
    t.fire('wpra', activator='player1')
    t.wait(0.3)
    t.mark('p2')
    t.teleport('player2', (180, -180, 0))
    t.wait(0.5)
    t.teleport('player2', (-180, -180, 0))
    t.wait(0.2)
    t.mark('p1')
    t.teleport('player1', (180, -180, 0))
    t.wait(0.5)
    t.expect.msg('Waypoint', to='player1', count=0, after='p2', within=(0, 0.6), fields={'on': 0})
    t.expect.msg('Waypoint', to='player1', count=1, after='p1', within=(0, 0.3), fields={'on': 0})


@S.test('Hide Closer Than / Hide Farther Than and No edge arrow go to the clients (the client hides the marker per '
        'viewer); Duration 1: off by itself one second after each turn-on', players=1, doc=DOC,
        covers=('info_waypoint.min_range', 'info_waypoint.max_range', 'info_waypoint:8', 'info_waypoint.duration'))
def ranges_duration(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='wpd', message='RANGED', min_range='200', max_range='3000',
            duration='1', spawnflags='8')
    t.wait(0.3)
    t.mark('on')
    t.fire('wpd', activator='player1')
    t.wait(1.5)
    t.mark('again')
    t.fire('wpd', activator='player1')
    t.wait(1.5)
    t.expect.msg('Waypoint', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'label': 'RANGED', 'min_range': 200, 'max_range': 3000, 'no_edge': 1})
    t.expect.msg('Waypoint', to='player1', count=1, after='on', within=(0.9, 1.3), fields={'on': 0})
    t.expect.msg('Waypoint', to='player1', count=1, after='again', within=(0.9, 1.3), fields={'on': 0})


@S.test('Only the activator sees it and spectators: player2 spectating player1 gets player1\'s marker tagged with '
        'player1 (owner), player3 in the game gets nothing; player2 switching to watch player3 gets it off', players=3,
        doc=DOC, covers=('info_waypoint:1',))
def spectators(t):
    t.point('info_waypoint', at=(0, 0, 64), targetname='wps', message='SPECD', spawnflags='1')
    t.wait(0.3)
    t.cmd('player2', 'spectate')
    t.wait(1.0)
    t.cmd('player2', 'specmode', '4')
    t.wait(0.3)
    t.cmd('player2', 'follownext', '0')     # from player3 (the first found) on to player1
    t.wait(0.3)
    t.snap('spec', 'player2', 'iuser1', 'iuser2')
    t.snap('p1', 'player1', 'iuser1')
    t.mark('on')
    t.fire('wps', activator='player1')
    t.wait(1.0)
    t.mark('back')
    t.cmd('player2', 'follownext', '0')     # on to player3
    t.wait(1.0)
    t.snap('other', 'player2', 'iuser2')
    t.expect.msg('Waypoint', to='player1', count=1, after='on', within=(0, 0.1),
                 fields={'on': 1, 'label': 'SPECD', 'owner': 0})
    t.expect.msg('Waypoint', to='player2', count=1, after='on', within=(0, 0.5),
                 fields={'on': 1, 'label': 'SPECD', 'owned': 1})
    t.expect.msg('Waypoint', to='player3', count=0, after='on', fields={'label': 'SPECD'})
    t.expect.msg('Waypoint', to='player2', count=1, after='back', within=(0, 0.5), fields={'on': 0})
    t.expect.value('spec', 'player2', 'iuser1', 4)     # in eye, watching player1 (entity 1)
    t.expect.value('spec', 'player2', 'iuser2', 1)
    t.expect.value('other', 'player2', 'iuser2', 3)
