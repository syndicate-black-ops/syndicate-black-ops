"""Tier 4 batch 6, real server: a monster's path (Target = a path_corner), trigger_camera deceleration through a
path_corner wait, Gag on talking monsters, scripted_sequence Move to Position 2 (Run) and Action Animation,
momentary_door move / stop sounds. Each test checks the FGD's description (sdk/syndicate.fgd)."""

from etest import Suite

SUITE = Suite('map_used8', 'Tier 4 batch 6: monster paths, camera deceleration, Gag, scripted run / animation, '
              'momentary_door sounds')
S = SUITE

SF_GAG = 2
SF_PRISONER = 16


# ------------------------------------------------------------------------------------------------ monster paths

def path_room(t, cls, named):
    t.point('path_corner', targetname='pc1', at=(150, 150, 0), target='pc2')
    t.point('path_corner', targetname='pc2', at=(150, -150, 0), target='pc1')
    kv = dict(angles='0 0 0', target='pc1', spawnflags=str(SF_PRISONER))     # it fights nobody meanwhile
    if named:
        kv['targetname'] = 'mon'
    t.point(cls, at=(-150, 150, 0), **kv)
    t.point('info_target', targetname='start', at=(-150, 150, 0))
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (-200, -200, 0))


@S.test('monster_dark with a Target path_corner and no name walks its path from the start (it is well away from where '
        'it stood after 6 s)', players=1, covers=('monster_dark.target',))
def dark_path_unnamed(t):
    path_room(t, 'monster_dark', named=False)
    t.wait(6.0)
    t.snap('later', '@monster_dark', 'origin')
    t.snap('later', 'start', 'origin')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.distance('later', '@monster_dark', 'start', 150, 1000)


@S.test('monster_dark with a Target path_corner and a name waits where it stands until triggered (Half-Life: a named '
        'monster waits for a trigger; triggered, it goes on alert)', players=1, covers=('monster_dark.target',))
def dark_path_named(t):
    path_room(t, 'monster_dark', named=True)
    t.wait(8.0)                                     # past the 5 s wait of its idle schedule
    t.snap('waiting', 'mon', 'origin', 'schedule')
    t.snap('waiting', 'start', 'origin')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.distance('waiting', 'mon', 'start', 0, 16)


# ------------------------------------------------------------------------------------------------ camera deceleration

@S.test('trigger_camera Deceleration 100: once it is on the leg to a path_corner with a wait (3 s) it slows down by '
        '100 a second (from 400: about 300 a second later)', players=1, covers=('trigger_camera.deceleration',))
def camera_deceleration(t):
    # Half-Life: reaching a path_corner it takes the next one's wait and slows down to 0 for it
    t.point('info_target', targetname='look', at=(0, 200, 40))
    t.point('path_corner', targetname='c1', at=(-200, -100, 100), target='c2', speed='400')
    t.point('path_corner', targetname='c2', at=(-100, -100, 100), target='c3')
    t.point('path_corner', targetname='c3', at=(200, -100, 100), wait='3')
    t.point('trigger_camera', targetname='cam', at=(-200, -100, 100), target='look', moveto='c1', speed='0',
            wait='10', acceleration='5000', deceleration='100', spawnflags='4')
    t.fire('cam', activator='player1')
    t.wait(0.25)
    t.snap('fast', 'cam', 'speed')
    t.wait(1.0)
    t.snap('slower', 'cam', 'speed')
    t.fire('cam', activator='player1', use='off')
    t.wait(0.3)
    t.expect.value('fast', 'cam', 'speed', 400, tol=5)
    t.expect.value('slower', 'cam', 'speed', 370, op='<=')      # slowing down (about 100 a second; less on a loaded
    t.expect.value('slower', 'cam', 'speed', 250, op='>=')      # machine: the camera thinks every frame)


# ------------------------------------------------------------------------------------------------ Gag

def make_gag_tests(cls):
    """Each in a room of its own: a gagged monster still answers another one that talks to it (Half-Life: Gag stops
    idle speech only), so the gagged one is alone."""
    def gagged(t):
        t.point(cls, targetname='mon', at=(0, 100, 0), angles='0 270 0', spawnflags=str(SF_GAG))
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -100, 0), yaw=90)
        t.mark('in')
        t.wait(60.0)
        t.flags('player1', '-FL_GODMODE')
        t.expect.sound('', ent='mon', chan=2, count=0, after='in')

    def talker(t):
        t.point(cls, targetname='mon', at=(0, 100, 0), angles='0 270 0')
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -100, 0), yaw=90)
        t.mark('in')
        t.wait(60.0)
        t.flags('player1', '-FL_GODMODE')
        t.expect.sound('', ent='mon', chan=2, min=1, after='in')

    gagged.__name__ = 'gag_' + cls
    talker.__name__ = 'nogag_' + cls
    S.test('%s flag 2 Gag: alone near a player for a minute it says nothing (no voice sound)' % cls, players=1,
           covers=(cls + ':2',))(gagged)
    S.test('%s without Gag: alone near a player for a minute it speaks (a voice sound)' % cls, players=1,
           covers=(cls,))(talker)


for _c in ('monster_scientist',):
    make_gag_tests(_c)


# ------------------------------------------------------------------------------------------------ scripted_sequence

def make_script_room(t, moveto, play):
    t.point('monster_scientist', targetname='sci', at=(-200, 0, 0), angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('info_target', targetname='start', at=(-200, 0, 0))
    t.point('info_target', targetname='mark', at=(200, 0, 0))
    t.point('scripted_sequence', targetname='ss', at=(200, 0, 0), angles='0 0 0', m_iszEntity='sci',
            m_fMoveTo=str(moveto), m_iszPlay=play, names=('m_iszEntity',))
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (-200, -200, 0))
    t.wait(0.5)
    t.fire('ss')


@S.test('scripted_sequence Move to Position 2 Run: the scientist runs to the mark (more than 200 units in 1.5 s); 1 '
        'Walk covers less in the same time', players=1,
        covers=('scripted_sequence.m_fmoveto=2', 'scripted_sequence.m_fmoveto=1'))
def script_run(t):
    make_script_room(t, 2, 'idle1')
    t.point('monster_scientist', targetname='sci2', at=(-200, 150, 0), angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('info_target', targetname='start2', at=(-200, 150, 0))
    t.point('scripted_sequence', targetname='ss2', at=(200, 150, 0), angles='0 0 0', m_iszEntity='sci2',
            m_fMoveTo='1', m_iszPlay='idle1', names=('m_iszEntity',))
    t.fire('ss2')
    t.wait(1.5)
    t.snap('s', 'sci', 'origin')
    t.snap('s', 'start', 'origin')
    t.snap('s', 'sci2', 'origin')
    t.snap('s', 'start2', 'origin')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.distance('s', 'sci', 'start', 200, 1000)
    t.expect.distance('s', 'sci2', 'start2', 10, 199)


@S.test('scripted_sequence Action Animation (m_iszPlay): the scientist plays that animation (pull_needle, '
        'give_shot)', players=1, covers=('scripted_sequence.m_iszplay',))
def script_play(t):
    make_script_room(t, 0, 'pull_needle')
    t.point('monster_scientist', targetname='sci2', at=(-200, 150, 0), angles='0 0 0', spawnflags=str(SF_PRISONER))
    t.point('scripted_sequence', targetname='ss2', at=(-200, 150, 0), angles='0 0 0', m_iszEntity='sci2',
            m_fMoveTo='0', m_iszPlay='give_shot', names=('m_iszEntity',))
    t.fire('ss2')
    t.wait(1.5)
    t.snap('s', 'sci', 'sequence', 'cine')
    t.snap('s', 'sci2', 'sequence', 'cine')
    t.flags('player1', '-FL_NOTARGET')
    # scientist.mdl: 28 pull_needle, 30 give_shot
    t.expect.value('s', 'sci', 'sequence', 28)
    t.expect.value('s', 'sci2', 'sequence', 30)


# ------------------------------------------------------------------------------------------------ momentary_door

def make_momentary_door_test(movesnd, sample):
    def door_sound(t):
        t.brush('momentary_rot_button', size=(8, 32, 32), at=(0, 40, 28), origin=True, targetname='mb', target='md',
                speed='90', distance='90')
        t.brush('momentary_door', size=(96, 16, 96), at=(-100, -150, 0), targetname='md', angles='0 0 0', lip='6',
                movesnd=str(movesnd), stopsnd='0', speed='100')
        t.teleport('player1', (0, 0, 0), yaw=90)
        t.look('player1', 0, 90)
        t.wait_frames(3)
        t.mark('hold')
        t.walk('player1', 0, 0, 0.5, '+use')
        t.wait(0.5)
        if sample:
            t.expect.sound(sample, ent='md', after='hold')
        else:
            t.expect.sound('doors/', ent='md', count=0, after='hold')
            t.expect.used('md', caller='mb', after='hold', min=1)

    door_sound.__name__ = 'momentary_door_sound_%d' % movesnd
    S.test('momentary_door Move Sound %d: moved by a momentary_rot_button it plays %s; Stop Sound 0 None' % (
        movesnd, sample or 'nothing'), players=1,
        covers=('momentary_door.movesnd', 'momentary_door.movesnd=%d' % movesnd, 'momentary_door.stopsnd',
                'momentary_door.stopsnd=0'))(door_sound)


for _m, _s in ((0, None), (2, 'doors/doormove2.wav'), (4, 'doors/doormove4.wav')):
    make_momentary_door_test(_m, _s)
