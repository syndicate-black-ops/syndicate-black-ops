"""game_messagebox keys (docs/MESSAGE_BOX.md, sdk/syndicate.fgd) the suite sbo does not check: All Players (players
in play) vs the activator, the autoclose distance (not shown when too far, closed when the player moves away; from
the entity or from Autoclose position: an entity or "x y z"), no title, an empty text, one box per entity, at most 8
boxes per player, the open boxes sent again at a HUD re-initialization. The box itself needs the client layer."""

from etest import Suite

SUITE = Suite('msgbox', 'game_messagebox: keys and the MessageBox message', players=2)
S = SUITE
SUITE.server_args = ('+developer', '1')    # the entity's console lines
DOC = 'MESSAGE_BOX.md'


@S.test('flag 1 All Players: every player in play gets the box (title and text); without it only the activator',
        players=2, doc=DOC, covers=('game_messagebox', 'game_messagebox:1', 'game_messagebox.title',
                                    'game_messagebox.message'))
def all_players(t):
    t.point('game_messagebox', targetname='all', spawnflags='1', title='News', message='For everybody')
    t.point('game_messagebox', targetname='one', title='Mine', message='Only you')
    t.mark('go')
    t.fire('all', activator='player1')
    t.fire('one', activator='player2')
    t.wait(0.2)
    for p in ('player1', 'player2'):
        t.expect.msg('MessageBox', to=p, count=1, after='go', fields={'open': 1, 'title': 'News', 'text': 'For everybody'})
    t.expect.msg('MessageBox', to='player2', count=1, after='go', fields={'open': 1, 'title': 'Mine', 'text': 'Only you'})
    t.expect.msg('MessageBox', to='player1', count=0, after='go', fields={'text': 'Only you'})


@S.test('Autoclose distance: shown to a player within it (2D) of the entity; closed (open 0) when he moves farther; a '
        'player already too far gets nothing (console line)', players=2, doc=DOC,
        covers=('game_messagebox.distance',))
def distance(t):
    # player1 stands at (-48, -208), player2 at (0, -208)
    t.point('game_messagebox', targetname='near', at=(-48, -150, 0), distance='100', message='Stay close',
            spawnflags='1')
    t.mark('go')
    t.fire('near', activator='player1')
    t.wait(0.2)
    t.teleport('player2', (200, 200, 0))
    t.wait(0.1)
    t.mark('far')
    t.fire('near', activator='player1')     # player2 now too far; player1 has it open already
    t.wait(0.2)
    t.mark('away')
    t.teleport('player1', (-48, 150, 0))
    t.wait(0.3)
    t.expect.msg('MessageBox', to='player1', count=1, after='go', within=(0, 0.2), fields={'open': 1, 'text': 'Stay close'})
    t.expect.msg('MessageBox', to='player2', count=1, after='go', within=(0, 0.2), fields={'open': 1, 'text': 'Stay close'})
    t.expect.msg('MessageBox', to='player2', count=1, after='go', within=(0, 0.3), fields={'open': 0})
    t.expect.msg('MessageBox', to='player1', count=1, after='away', fields={'open': 0})
    t.expect.console('is open for et1 already')


@S.test('Autoclose position: an entity name (its origin) or "x y z" replaces the entity\'s own place', players=1,
        doc=DOC, covers=('game_messagebox.position',))
def position(t):
    t.point('info_target', targetname='spot', at=(-48, -180, 0))
    t.point('game_messagebox', targetname='ent', at=(200, 200, 0), distance='80', position='spot', message='By name')
    x, y, z = t._abs((-48, -190, 0))
    t.point('game_messagebox', targetname='vec', at=(200, 200, 0), distance='80', position='%g %g %g' % (x, y, z),
            message='By vector', names=('position',))
    t.point('game_messagebox', targetname='here', at=(200, 200, 0), distance='80', message='From here')
    t.mark('go')
    for n in ('ent', 'vec', 'here'):
        t.fire(n, activator='player1')
    t.wait(0.2)
    t.mark('away')
    t.teleport('player1', (-48, 150, 0))
    t.wait(0.3)
    t.expect.msg('MessageBox', to='player1', count=1, after='go', within=(0, 0.2), fields={'open': 1, 'text': 'By name'})
    t.expect.msg('MessageBox', to='player1', count=1, after='go', within=(0, 0.2), fields={'open': 1, 'text': 'By vector'})
    t.expect.msg('MessageBox', to='player1', count=0, after='go', fields={'text': 'From here'})
    t.expect.msg('MessageBox', to='player1', count=2, after='away', fields={'open': 0})
    t.expect.console('is too far from the message box position')


@S.test('no title: an empty title (no title bar); no text: nothing is shown (console line)', players=1, doc=DOC,
        covers=('game_messagebox.title', 'game_messagebox.message'))
def empty(t):
    t.point('game_messagebox', targetname='notitle', message='Plain', distance='400')
    t.point('game_messagebox', targetname='notext', title='Only a title', distance='400')
    t.mark('go')
    t.fire('notitle', activator='player1')
    t.fire('notext', activator='player1')
    t.wait(0.2)
    t.teleport('player1', (-48, 150, 0))
    t.teleport('player1', (0, 0, 0))
    t.wait(0.1)
    t.expect.msg('MessageBox', to='player1', count=1, after='go', fields={'open': 1, 'title': '', 'text': 'Plain'})
    t.expect.msg('MessageBox', to='player1', count=0, after='go', fields={'title': 'Only a title'})
    t.expect.console('game_messagebox: message is empty')


@S.test('a HUD re-initialization (fullupdate) sends the player\'s open boxes again', players=1, doc=DOC,
        covers=('game_messagebox.message',))
def fullupdate(t):
    t.point('game_messagebox', targetname='b', message='Still here', title='T', at=(0, -150, 0), distance='150')
    t.fire('b', activator='player1')
    t.wait(0.2)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.teleport('player1', (-48, 200, 0))
    t.wait(0.2)
    t.expect.msg('MessageBox', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'open': 1, 'title': 'T', 'text': 'Still here'})
