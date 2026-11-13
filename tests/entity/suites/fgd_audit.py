"""FGD audit (fgd.py check, code to FGD): flags and keys the game reads that the FGD now offers, and labels that now say
what the code does: func_rot_button Player Can't Use (16384), func_guntarget Start On (1), scripted_action Auto-search (1024) and Try search once (4096), scripted_sentence
flag 8 (others stop talking), func_vehicle dmg (the push-away speed)."""

from etest import Suite

SUITE = Suite('fgd_audit', 'FGD audit: flags and keys the code reads', players=1)
S = SUITE
DOC = 'FGD.md'


def info(t, name, at=(200, -200, 0)):
    t.point('info_target', targetname=name, at=at)


@S.test('func_rot_button "Player Can\'t Use" (16384): the player\'s +use never turns it; the same button without the '
        'flag turns at once', players=1, doc=DOC, covers=('func_rot_button', 'func_rot_button:16384'))
def rot_button_player_cant_use(t):
    info(t, 'ra')
    info(t, 'rb')
    t.brush('func_rot_button', targetname='rba', size=(48, 8, 48), at=(-80, 100, 40), origin=True, target='ra',
            spawnflags=str(16384 | 128), wait='0.3', speed='200', distance='90')
    t.brush('func_rot_button', targetname='rbb', size=(48, 8, 48), at=(80, 100, 40), origin=True, target='rb',
            spawnflags='128', wait='0.3', speed='200', distance='90')
    t.teleport('player1', (80, 60, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.mark('useb')
    for _ in range(3):         # Y axis (128): it turns in its own plane, never into the player
        t.move('player1', 0, 0, 3, '+use')
        t.wait(0.3)
    t.wait_for_fire('rb', 3.0)
    t.teleport('player1', (-80, 60, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.3)
    t.mark('usea')
    for _ in range(3):
        t.move('player1', 0, 0, 3, '+use')
        t.wait(0.5)
    t.expect.used('rb', min=1, after='useb')
    t.expect.not_used('ra', after='usea')


@S.test('func_guntarget "Start On" (1): it sets off along its path at the map start; without the flag it waits for a '
        'trigger', players=1, doc=DOC, covers=('func_guntarget', 'func_guntarget:1'))
def guntarget_start_on(t):
    for n, x, fl in (('gon', -120, '1'), ('goff', 120, '0')):
        t.point('path_corner', targetname=n + '_p1', at=(x, 150, 40), target=n + '_p2')
        t.point('path_corner', targetname=n + '_p2', at=(x, -150, 40), target=n + '_p1')
        t.brush('func_guntarget', targetname=n, size=(32, 8, 32), at=(x, 150, 40), origin=True, target=n + '_p1',
                speed='100', health='100', spawnflags=fl)
    t.snap('a', 'gon', 'origin')
    t.snap('a', 'goff', 'origin')
    t.wait(1.5)
    t.snap('b', 'gon', 'origin')
    t.snap('b', 'goff', 'origin')
    t.expect.moved('a', 'b', 'gon', min_dist=40)
    t.expect.moved('a', 'b', 'goff', delta=(0, 0, 0), tol=1)


def sentence_case(flag8):
    @S.test('scripted_sentence flag 8 %s: while one scientist speaks, a second scripted sentence %s' % (
        ('"Others stop talking"', 'stops him (his voice gets common/null.wav)') if flag8 else
        ('off', 'lets him go on (others keep talking)')), players=1, doc=DOC,
        covers=('scripted_sentence', 'scripted_sentence:8') if flag8 else ('scripted_sentence',))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.point('monster_scientist', targetname='sa', at=(-60, 150, 0), angles='0 270 0', spawnflags='16')
        t.point('monster_scientist', targetname='sb', at=(60, 150, 0), angles='0 270 0', spawnflags='16')
        t.point('scripted_sentence', targetname='say_a', entity='sa', sentence='!SC_HELLO0', duration='3',
                spawnflags='4', names=('entity',))
        t.point('scripted_sentence', targetname='say_b', entity='sb', sentence='!SC_HELLO1', duration='1',
                spawnflags=str(4 | (8 if flag8 else 0)), names=('entity',))
        t.wait(1.0)
        t.fire('say_a')
        t.wait(0.3)
        t.mark('go')
        t.fire('say_b')
        t.wait(0.8)
        t.expect.sound('SC_HELLO1', ent='sb', after='go')
        if flag8:
            t.expect.sound('null.wav', ent='sa', after='go', chan=2)
        else:
            t.expect.sound('null.wav', ent='sa', after='go', chan=2, count=0, min=0)
    case.__name__ = 'sentence_others_stop' if flag8 else 'sentence_others_go_on'
    return case


sentence_case(True)
sentence_case(False)


def vehicle_case(dmg):
    @S.test('func_vehicle "Push-away speed" (dmg) %s: driving into a player %s' % (
        (dmg, 'throws him away from the vehicle fast') if dmg else (dmg, 'only lifts him (300 up)')),
        players=1, doc=DOC, size=(512, 512, 200), covers=('func_vehicle', 'func_vehicle.dmg'))
    def case(t):
        t.point('path_track', at=(-200, 0, 16), targetname='vp1', target='vp2')
        t.point('path_track', at=(200, 0, 16), targetname='vp2')
        t.brush('func_vehicle', size=(64, 32, 16), at=(-200, 0, 0), origin=True, targetname='car', target='vp1',
                speed='300', sounds='0', length='64', width='32', height='4', dmg=str(dmg))
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (-120, 0, 0), yaw=180)
        t.wait(0.3)
        t.snap('a', 'player1', 'origin')
        t.fire('car')
        t.wait(0.6)
        t.snap('b', 'player1', 'origin', 'velocity')
        if dmg:
            t.expect.moved('a', 'b', 'player1', min_dist=80, flat=True)
        else:
            t.expect.value('b', 'player1', 'velocity.x', 250, op='<')
    case.__name__ = 'vehicle_push_%d' % dmg
    return case


vehicle_case(1000)
vehicle_case(0)


def action_case(flag):
    # scripted_action with action 6 (reload) on a Fed made later by a maker; snapshot field cine: in its script
    @S.test('scripted_action flag %d "%s": %s' % (flag, 'Auto-search' if flag == 1024 else 'Try search once',
            'a named action keeps looking from the map start and takes the Fed a maker makes later, without a trigger'
            if flag == 1024 else 'fired before its Fed exists it does not keep looking; without it it takes him once '
            'he appears'), players=1, doc=DOC, covers=('scripted_action', 'scripted_action:%d' % flag))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        for n, x, fl in (('af', -100, flag), ('an', 100, 0)):
            t.point('scripted_action', targetname=n + '_ss', at=(x, 0, 0), m_iszEntity=n + '_kid', m_fAction='6',
                    m_fMoveTo='0', spawnflags=str(fl), names=('m_iszEntity',))
            t.point('monstermaker', targetname=n + '_mk', at=(x, 0, 0), monstertype='monster_human_grunt',
                    monstercount='1', netname=n + '_kid', spawnflags='4', angles='0 270 0', names=('netname',))
        t.wait(0.5)
        if flag == 4096:
            t.fire('af_ss')
            t.fire('an_ss')
            t.wait(1.5)
        t.fire('af_mk')
        t.fire('an_mk')
        t.wait(1.0)
        t.snap('s', 'af_kid', 'cine')
        t.snap('s', 'an_kid', 'cine')
        if flag == 1024:
            t.expect.value('s', 'af_kid', 'cine', 1)
            t.expect.value('s', 'an_kid', 'cine', 0)
        else:
            t.expect.value('s', 'af_kid', 'cine', 0)
            t.expect.value('s', 'an_kid', 'cine', 1)
    case.__name__ = 'action_%d' % flag
    return case


action_case(1024)
action_case(4096)
