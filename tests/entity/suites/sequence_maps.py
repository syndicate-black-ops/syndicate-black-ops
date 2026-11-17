"""Sequence files across map loads (docs/SEQUENCES.md, trigger_sequence.cpp Sequence_LevelInit): each map load reads
that map's own sequences/<map>.seq (and global.seq) and nothing of the last map's. On the first load and after a
changelevel, an entry runs with its text and its pause, as the Sequences page's first example promises.

The last test changes the map twice: to a stock map that has a sequence file of its own (a test-only file), then back to
the suite's map. It must stay last."""

from etest import Suite

SUITE = Suite('sequence_maps', 'Sequence files: the map\'s own file on the first load and after a changelevel')
S = SUITE
SQ = 'SEQUENCES.md'
TE = 'svc:23'                       # TE_TEXTMESSAGE, what a "#text" line sends
SUITE_MAP = SUITE.map_prefix + 'et_' + SUITE.name
SECOND_MAP = 'co_carnage'           # a stock map; its sequence file exists only in the test's game folder

# The second map's file: an entry of its own, with the docs example's shape (text, pause, firetargets)
SUITE.add_file('sequences/%s.seq' % SECOND_MAP, (
    '%et_second_entry\n'
    '{\n'
    '    #text = "ET_SEQ_SECOND"\n'
    '    #pause = 2\n'
    '    #firetargets = "et_second_done"\n'
    '}\n').encode())


def text(**fields):
    f = {'te': 29}
    f.update(fields)
    return f


def vault_entry(t, name, message, target, position=', position = -1 -1'):
    """docs-site mapping/features/sequences.md "Your first sequence", Step 1, with this test's names (position='':
    the text at the default position)."""
    return t.sequence(name, '    #text = "%s"%s\n    #pause = 3\n    #firetargets = "%s"'
                      % (message, position, t.name(target)))


def rejoin(t):
    """After a changelevel: the test player is dropped (fake clients are) and added again."""
    t.wait_frames(20)
    t.console('test_player add et1')
    t.wait_frames(2)
    t.console('test_join player1')
    t.wait(0.5)
    t.console('test_round start')
    t.console('wait_for alive player1 20')
    t.wait(0.5)
    t.flags('player1', '+FL_NOTARGET')      # the stock map's monsters leave it alone


@S.test('trigger_sequence on the map\'s first load: the docs\' first example shows its text at once, centered, and '
        'fires its target 3 seconds later', doc=SQ + '#your-first-sequence', players=1,
        covers=('trigger_sequence', 'trigger_sequence.sequence_id'))
def first_load(t):
    seq = vault_entry(t, 'vault_open', 'ET_SEQ_FIRST', 'vault_door')
    t.point('trigger_sequence', targetname='seq_vault', sequence_id=seq)
    t.point('info_target', targetname='vault_door')
    t.mark('go')
    t.fire('seq_vault', activator='player1')
    t.wait(3.6)
    t.expect.msg(TE, to='player1', fields=text(text='ET_SEQ_FIRST', x=-1, y=-1), count=1, after='go',
                 within=(0, 0.1))
    t.expect.fired('vault_door', caller='seq_vault', count=1, after='go', within=(2.95, 3.15))


@S.test('trigger_sequence after a changelevel: the new map\'s own file runs (text, then its target after the pause), '
        'the last map\'s entries are gone, and back on the first map its file runs again; a text without a position '
        'starts at the middle of the screen (0.5 0.5, as in CZ: Deleted Scenes)', players=1,
        covers=('trigger_sequence', 'trigger_sequence.sequence_id'))
def after_changelevel(t):
    seq = vault_entry(t, 'back', 'ET_SEQ_BACK', 'back_door', position='')
    t.point('trigger_sequence', targetname='seq_back', sequence_id=seq)
    t.point('info_target', targetname='back_door')

    # the second map: its own entry runs; this map's entry is not there
    t.console('changelevel %s' % SECOND_MAP)
    rejoin(t)
    t.console('test_create trigger_sequence 0 0 0 0 targetname et_second_seq sequence_id et_second_entry')
    t.console('test_create trigger_sequence 0 0 0 0 targetname et_stale_seq sequence_id %s' % seq)
    t.wait(0.2)
    t.mark('second')
    t.fire('=et_second_seq', activator='player1')
    t.fire('=et_stale_seq', activator='player1')
    t.wait(2.6)
    t.expect.msg(TE, to='player1', fields=text(text='ET_SEQ_SECOND', x=0.5, y=0.5), count=1, after='second', within=(0, 0.1))
    t.expect.fired('=et_second_done', caller='=et_second_seq', count=1, after='second', within=(1.95, 2.15))
    t.expect.console('trigger_sequence "et_stale_seq": unknown sequence "%s", not firing' % seq)

    # back on the suite's map: its own file again, nothing of the second map's
    t.console('changelevel %s' % SUITE_MAP)
    rejoin(t)
    t.console('test_create trigger_sequence 0 0 0 0 targetname et_second_seq sequence_id et_second_entry')
    t.wait(0.2)
    t.mark('back')
    t.fire('seq_back', activator='player1')
    t.fire('=et_second_seq', activator='player1')
    t.wait(3.6)
    t.expect.msg(TE, to='player1', fields=text(text='ET_SEQ_BACK', x=0.5, y=0.5), count=1, after='back', within=(0, 0.1))
    t.expect.fired('back_door', caller='seq_back', count=1, after='back', within=(2.95, 3.15))
    t.expect.msg(TE, fields=text(text='ET_SEQ_SECOND'), count=0, after='back')
    t.expect.fired('=et_second_done', count=1)          # only the one on the second map
    t.expect.fired('back_door', count=1)                # only the one back on this map
