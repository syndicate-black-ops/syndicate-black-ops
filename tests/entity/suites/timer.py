"""game_timer keys (docs/GAME_TIMER.md, sdk/syndicate.fgd) the suite sbo does not check: the "GameTimer" message's full
state field by field (defaults, HUD X / Y, caption, caption width and lines, progress bar keys, Show Time Remaining,
Warn Time default, Max Length), Start Disabled, Start Paused, Disable On Finish, Show In HUD. How the HUD draws it
needs the client test layer."""

from etest import Suite

SUITE = Suite('timer', 'game_timer: keys and the GameTimer message', players=1)
S = SUITE
DOC = 'GAME_TIMER.md'
DEFAULT_XY = -32768     # "-1": the default place


@S.test('keys left out: enabled by ON, full state with show remaining, length = Timer Length, warn a quarter of it, '
        'bar max / override -1, x / y default, caption width 260, 2 lines, no caption', players=1, doc=DOC,
        covers=('game_timer', 'game_timer.timer_length', 'game_timer.warn_time', 'game_timer.progress_bar_max',
                'game_timer.progress_bar_override', 'game_timer.x', 'game_timer.y', 'game_timer.caption_width',
                'game_timer.caption_lines', 'game_timer.startdisabled=1', 'game_timer.show_time_remaining=1',
                'game_timer.show_in_hud=1'))
def defaults(t):
    t.point('game_timer', targetname='gt', timer_length='20')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.mark('on')
    t.fire('gt', use='on')
    t.wait(0.2)
    t.fire('gt', use='off')
    t.wait(0.1)
    t.expect.msg('GameTimer', count=0, after='fu', within=(0, 0.45), fields={'length': 20})
    t.expect.msg('GameTimer', count=1, after='on', within=(0, 0.1),
                 fields={'enabled': True, 'paused': False, 'show_remaining': True, 'full': True, 'length': 20,
                         'warn': 5, 'bar_max': -1, 'bar_override': -1, 'x': DEFAULT_XY, 'y': DEFAULT_XY,
                         'caption_width': 260, 'caption_lines': 2, 'caption': ''})


@S.test('every display key in the message: HUD X / Y (parts of the screen x 10000), caption, caption width, caption '
        'lines (above 4: 4, below 1: 1), progress bar max / override, warn time, Show elapsed time (0)', players=1,
        doc=DOC, covers=('game_timer.x', 'game_timer.y', 'game_timer.timer_caption', 'game_timer.caption_width',
                         'game_timer.caption_lines', 'game_timer.progress_bar_max', 'game_timer.progress_bar_override',
                         'game_timer.warn_time', 'game_timer.show_time_remaining=0'))
def display_keys(t):
    t.point('game_timer', targetname='a', timer_length='30', x='0.25', y='0.1', timer_caption='Hold the line',
            caption_width='300', caption_lines='9', progress_bar_max='10', progress_bar_override='3', warn_time='7',
            show_time_remaining='0')
    t.point('game_timer', targetname='b', timer_length='31', caption_lines='0')
    t.mark('on')
    t.fire('a', use='on')
    t.fire('b', use='on')
    t.wait(0.2)
    t.fire('a', use='off')
    t.fire('b', use='off')
    t.wait(0.1)
    t.expect.msg('GameTimer', count=1, after='on', within=(0, 0.1),
                 fields={'enabled': True, 'show_remaining': False, 'length': 30, 'warn': 7, 'bar_max': 10,
                         'bar_override': 3, 'x': 2500, 'y': 1000, 'caption_width': 300, 'caption_lines': 4,
                         'caption': 'Hold the line'})
    t.expect.msg('GameTimer', count=1, after='on', within=(0, 0.1), fields={'length': 31, 'caption_lines': 1})


@S.test('Max Length caps the Timer Length (and the length sent); Start Disabled 0: it runs from the map start (a '
        'fullupdate sends it running); Start Paused 1: enabled but paused, ON does not start it', players=1, doc=DOC,
        covers=('game_timer.max_length', 'game_timer.startdisabled=0', 'game_timer.start_paused=1',
                'game_timer.start_paused=0'))
def start_state(t):
    t.point('game_timer', targetname='run', timer_length='600', StartDisabled='0')
    t.point('game_timer', targetname='held', timer_length='601', StartDisabled='0', start_paused='1')
    t.point('game_timer', targetname='cap', timer_length='20', max_length='10')
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.mark('on')
    t.fire('held', use='on')
    t.fire('cap', use='on')
    t.wait(0.2)
    t.snap('s', 'cap', 'kv:time_remaining')
    for n in ('run', 'held', 'cap'):
        t.fire(n, use='off')
    t.wait(0.1)
    t.expect.msg('GameTimer', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'enabled': True, 'paused': False, 'length': 600})
    t.expect.msg('GameTimer', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'enabled': True, 'paused': True, 'length': 601, 'time': 601})
    t.expect.msg('GameTimer', count=0, after='on', within=(0, 0.15), fields={'length': 601})
    t.expect.msg('GameTimer', count=1, after='on', within=(0, 0.15), fields={'enabled': True, 'length': 10})
    t.expect.value('s', 'cap', 'kv:time_remaining', 10, tol=0.3)


@S.test('Disable On Finish 1 (default): at the end the timer is taken off the HUD (flags 0) and a fullupdate does not '
        'send it; 0: it stays shown, paused at 0', players=1, doc=DOC,
        covers=('game_timer.disable_on_finish=1', 'game_timer.disable_on_finish=0', 'game_timer.target'))
def disable_on_finish(t):
    t.point('game_timer', targetname='a', timer_length='0.5', target='done_a')
    t.point('game_timer', targetname='b', timer_length='0.6', target='done_b', disable_on_finish='0')
    t.point('info_target', targetname='done_a')
    t.point('info_target', targetname='done_b')
    t.mark('on')
    t.fire('a', use='on')
    t.fire('b', use='on')
    t.wait(1.0)
    t.mark('fu')
    t.cmd('player1', 'fullupdate')
    t.wait(0.5)
    t.fire('b', use='off')
    t.wait(0.1)
    t.expect.used('done_a', count=1)
    t.expect.used('done_b', count=1)
    t.expect.msg('GameTimer', count=1, after='on', within=(0.4, 0.9), fields={'flags': 0})
    t.expect.msg('GameTimer', count=1, after='on', within=(0.5, 0.9), fields={'enabled': True, 'paused': True, 'time': 0})
    t.expect.msg('GameTimer', to='player1', count=1, after='fu', within=(0, 0.45),
                 fields={'enabled': True, 'paused': True, 'length': 0.6, 'time': 0})
    t.expect.msg('GameTimer', to='player1', count=0, after='fu', within=(0, 0.45), fields={'length': 0.5})


@S.test('Show In HUD 0: nothing is sent, the targets still fire; set to 1 at run time it is sent', players=1, doc=DOC,
        covers=('game_timer.show_in_hud=0', 'game_timer.resumetarget', 'game_timer.target'))
def hidden(t):
    t.point('game_timer', targetname='gt', timer_length='0.5', show_in_hud='0', target='done', resumetarget='go')
    t.point('info_target', targetname='done')
    t.point('info_target', targetname='go')
    t.point('game_timer', targetname='late', timer_length='40', show_in_hud='0')
    t.mark('on')
    t.fire('gt', use='on')
    t.fire('late', use='on')
    t.wait(0.8)
    t.mark('show')
    t.kv('late', 'show_in_hud', '1')
    t.wait(0.2)
    t.fire('late', use='off')
    t.wait(0.1)
    t.expect.used('go', count=1)
    t.expect.used('done', count=1)
    t.expect.no_msg('GameTimer', after='on', within=(0, 0.75))
    t.expect.msg('GameTimer', count=1, after='show', within=(0, 0.1), fields={'enabled': True, 'full': True, 'length': 40})
