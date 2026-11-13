"""Featureful bug fixes on entities SBO shares with it.
Each test sets up the case a fix covers and checks the fixed behavior. The no-activator func_tank test stays last:
without the fix it stops the server."""

from etest import Suite

SUITE = Suite('fixes', 'Featureful bug fixes: tanks, scripts, cameras, squads, gibs, doors, trains, apache, vehicle, '
              'turret, grenade smoke', players=2)
S = SUITE
DOC = 'SBO-2.0-CHANGES.md'
EF_NODRAW = 128
SND_STOP = 32
FL_NOTARGET = 128
MONSTERSTATE_SCRIPT = 6
TE_SMOKE = 5

TANK_AT, PLAYER_AT = (0, -120, 40), (0, -200, 0)
TANK_KEYS = dict(angles='0 90 0', yawrate='60', yawrange='30', pitchrate='30', pitchrange='30', barrel='24',
                 firerate='4', yawtolerance='15', pitchtolerance='5', persistence='1')


def tough(t, player='player1'):
    t.kv(player, 'health', '100000')
    t.kv(player, 'max_health', '100000')


def tank(t, classname='func_tank', **keys):
    k = dict(TANK_KEYS)
    k.update(keys)
    t.brush(classname, size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags='32', **k)


def at_controls(t, player='player1'):
    """The player stands where the tank lets him take its controls (it lets go of one 30 units away)."""
    tough(t, player)
    t.flags(player, '+FL_NOTARGET')
    t.teleport(player, PLAYER_AT, yaw=90)
    t.look(player, 0, 90)
    t.wait(1.0)


# --------------------------------------------------------------------------------------------- func_tank (00405458)
@S.test('func_tank: a player refused by its master (multisource not triggered) is not left holding the tank '
        '(m_pTank)', players=1, doc=DOC, covers=('func_tank', 'func_tank.master'))
def tank_master_refused(t):
    t.point('multisource', targetname='lock')
    t.point('trigger_relay', targetname='key', target='lock')      # an input of the multisource, never fired: locked
    tank(t, master='lock')
    at_controls(t)
    t.fire('tk', activator='player1')
    t.snap('after', 'player1', 'tank')
    t.wait(0.2)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('after', 'player1', 'tank', 0)


@S.test('func_tank: another player\'s use stops the controller; the controller lets go of the tank at once',
        players=2, doc=DOC, covers=('func_tank',))
def tank_controller_replaced(t):
    tank(t)
    at_controls(t)
    t.fire('tk', activator='player1')
    t.wait(0.3)
    t.snap('on', 'player1', 'tank')
    t.fire('tk', activator='player2', use='on')     # CFuncTank::Use with a controller: StopControl
    t.snap('off', 'player1', 'tank')
    t.wait(0.2)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('on', 'player1', 'tank', 1)
    t.expect.value('off', 'player1', 'tank', 0)


@S.test('func_tank removed (killtarget) while a player mans it: the player lets go of it (Half-Life Unified SDK)',
        players=1, doc=DOC, covers=('func_tank',))
def tank_removed_while_manned(t):
    tank(t)
    t.point('trigger_relay', targetname='rm', killtarget='tk')
    at_controls(t)
    t.fire('tk', activator='player1')
    t.wait(0.3)
    t.snap('on', 'player1', 'tank')
    t.mark('rm')
    t.fire('rm')
    t.wait(0.3)
    t.snap('off', 'player1', 'tank')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.removed('tk', after='rm')
    t.expect.value('on', 'player1', 'tank', 1)
    t.expect.value('off', 'player1', 'tank', 0)


# ----------------------------------------------------------------------------------- func_tanklaser (0570769d, c8ebe744)
def laser_probe(t, delay='0'):
    """Each shot of the tank fires its target `shot`: a relay (after `delay`) runs a trigger_copyvalue that copies the
    beam's `effects` into the info_target `probe` (EF_NODRAW until a copy runs). The probe shows the beam's state at
    that moment of every shot, whatever the frame timing."""
    t.point('info_target', targetname='probe')
    t.kv('probe', 'effects', str(EF_NODRAW))
    t.point('trigger_relay', targetname='shot', target='copy', delay=delay)
    t.point('trigger_copyvalue', targetname='copy', netname='beam', m_iszSrcValueName='effects', target='probe',
            m_iszDstValueName='effects', names=('netname',))
    t.point('env_laser', at=(TANK_AT[0], TANK_AT[1], TANK_AT[2] + 16), targetname='beam',
            texture='sprites/laserbeam.spr', damage='0', width='10')


@S.test('func_tanklaser: the beam goes off when the player leaves the controls', players=1,
        doc=DOC, covers=('func_tanklaser', 'func_tanklaser.laserentity', 'func_tanklaser.target'))
def tanklaser_beam_off(t):
    tank(t, 'func_tanklaser', laserentity='beam', firerate='100', target='shot')     # fires every frame
    laser_probe(t)
    at_controls(t)
    t.fire('tk', activator='player1')
    t.wait(0.5)
    t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
    t.wait(1.0)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.snap('firing', 'probe', 'effects')             # the beam at a shot (the copy runs in the shot's frame)
    t.fire('tk', activator='player1', use='off')
    t.wait(1.5)
    t.snap('left', 'beam', 'effects')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.used('copy', min=2)
    t.expect.value('firing', 'probe', 'effects', EF_NODRAW, op='!&')
    t.expect.value('left', 'beam', 'effects', EF_NODRAW, op='&')


@S.test('func_tanklaser (automatic): the beam is still on 0.05 s after a shot (1.38: until the tank\'s next think, '
        '0.1 s later)', players=1, doc=DOC,
        covers=('func_tanklaser', 'func_tanklaser:1', 'func_tanklaser.laserentity', 'func_tanklaser.target'))
def tanklaser_beam_lasts(t):
    k = dict(TANK_KEYS)
    k.update(laserentity='beam', firerate='2', target='shot')
    t.brush('func_tanklaser', size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags='1', **k)
    laser_probe(t, delay='0.05')
    tough(t)
    t.teleport('player1', (0, 120, 0), yaw=270)
    t.wait(3.0)
    t.snap('later', 'probe', 'effects')              # the beam 0.05 s after the last shot
    t.wait(0.5)
    t.snap('after', 'beam', 'effects')
    t.kv('tk', 'spawnflags', '0')                    # stop the tank before the next test
    t.expect.used('copy', min=2)
    t.expect.value('later', 'probe', 'effects', EF_NODRAW, op='!&')


@S.test('func_tanklaser blocked (a player inside it) while its player fires: the beam goes off once he stops '
        '(Featureful 59228c10)', players=2, doc=DOC, covers=('func_tanklaser', 'func_tanklaser.laserentity'))
def tanklaser_blocked_beam_off(t):
    tank(t, 'func_tanklaser', laserentity='beam', firerate='100', target='shot')
    laser_probe(t)
    at_controls(t)
    tough(t, 'player2')
    t.flags('player2', '+FL_NOTARGET')
    t.fire('tk', activator='player1')
    t.wait(0.5)
    t.console('test_move %s 0 0 -1 +attack' % t.ref('player1'))
    t.wait(0.5)
    t.teleport('player2', (TANK_AT[0], TANK_AT[1], TANK_AT[2]), yaw=90)     # inside the tank's brush
    t.look('player1', 0, 110)                        # the tank turns after his view: player2 blocks it
    t.wait(0.5)
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.snap('blocked', 'tk', 'ltime')
    t.snap('blocked', 'probe', 'effects')
    t.wait(1.0)
    t.snap('stopped', 'tk', 'ltime')
    t.snap('stopped', 'beam', 'effects')
    t.fire('tk', activator='player1', use='off')
    t.teleport('player2', (200, -200, 0))
    t.flags('player1', '-FL_NOTARGET')
    t.flags('player2', '-FL_NOTARGET')
    t.expect.value('blocked', 'probe', 'effects', EF_NODRAW, op='!&')      # it fired
    t.expect.ratio('blocked', 'tk', 'stopped', 'tk', 'ltime', 1.0, tol=0.0005)  # its clock stood still: blocked
    t.expect.value('stopped', 'beam', 'effects', EF_NODRAW, op='&')


@S.test('func_tanklaser removed (killtarget) right after a shot: the beam goes off (co_corecity\'s automatic '
        'laser tanks)', players=1, doc=DOC,
        covers=('func_tanklaser', 'func_tanklaser.laserentity', 'func_tanklaser.target'))
def tanklaser_removed_beam_off(t):
    # automatic (Active, not controllable), aimed at the player in front of it. Each shot fires its target (as on
    # co_corecity): here the relay that killtargets the tank, so it is removed in the frame its beam went on
    k = dict(TANK_KEYS)
    k.update(laserentity='beam', target='kill')
    t.brush('func_tanklaser', size=(32, 32, 32), at=TANK_AT, origin=True, targetname='tk', spawnflags='1', **k)
    t.point('env_laser', at=(TANK_AT[0], TANK_AT[1], TANK_AT[2] + 16), targetname='beam',
            texture='sprites/laserbeam.spr', damage='0', width='10')
    t.point('trigger_relay', targetname='kill', killtarget='tk')
    tough(t)
    t.teleport('player1', (0, 120, 0), yaw=270)
    t.wait(3.0)
    t.snap('gone', 'beam', 'effects')
    t.expect.fired('kill')
    t.expect.removed('tk')
    t.expect.value('gone', 'beam', 'effects', EF_NODRAW, op='&')


# ------------------------------------------------------------------------------------ grenade smoke (7c785489)
@S.test('grenade smoke of a 400-damage blast (an RPG rocket rolls 250-400) is the biggest puff, not a byte-wrapped '
        'tiny one (TE_SMOKE scale clamped to 255)', players=1, doc=DOC)
def grenade_smoke_scale(t):
    t.point('grenade', at=(0, 200, 0), targetname='gr', health='10', takedamage='1')
    t.kv('gr', 'dmg', '400')                # after CGrenade::Spawn (dmg 100)
    tough(t)
    t.teleport('player1', (0, -200, 0), yaw=90)
    t.wait(0.5)
    t.mark('boom')
    t.damage('gr', 100)                     # CGrenade::Killed -> Detonate -> Explode, Smoke 0.3 s later
    t.wait(1.5)
    t.expect.msg('svc:23', fields={'te': TE_SMOKE, 'scale': 255}, count=1, after='boom')


# ------------------------------------------------------------------------------------- scripted_sequence (c55b806d)
@S.test('scripted_sequence removed (killtarget) while its monster waits in its idle: the monster lets go of it '
        '(m_pCine cleared, state out of script)', doc=DOC,
        covers=('scripted_sequence', 'scripted_sequence.m_iszEntity', 'scripted_sequence.m_iszIdle'))
def script_removed_waiting(t):
    t.point('monster_scientist', at=(0, 100, 0), targetname='sci', angles='0 270 0')
    t.point('scripted_sequence', at=(0, 100, 0), targetname='ss', m_iszEntity='sci', m_iszIdle='console',
            m_iszPlay='writeboard', m_fMoveTo='0', names=('m_iszEntity',))
    t.point('trigger_relay', targetname='kill', killtarget='ss')
    t.flags('sci', '+FL_NOTARGET')
    t.wait(0.5)
    t.snap('waiting', 'sci', 'cine', 'state')
    t.fire('kill')
    t.wait(1.0)
    t.snap('gone', 'sci', 'cine', 'state')
    t.expect.value('waiting', 'sci', 'cine', 1)
    t.expect.value('gone', 'sci', 'cine', 0)
    t.expect.value('gone', 'sci', 'state', MONSTERSTATE_SCRIPT, op='<')


@S.test('scripted_sequence removed while its monster plays it: the monster lets go of it', doc=DOC,
        covers=('scripted_sequence', 'scripted_sequence.m_iszEntity'))
def script_removed_playing(t):
    t.point('monster_scientist', at=(0, 100, 0), targetname='sci', angles='0 270 0')
    t.point('scripted_sequence', at=(0, 100, 0), targetname='ss', m_iszEntity='sci', m_iszPlay='studycart',
            m_fMoveTo='0', names=('m_iszEntity',))
    t.point('trigger_relay', targetname='kill', killtarget='ss')
    t.flags('sci', '+FL_NOTARGET')
    t.fire('ss')
    t.wait(1.5)
    t.snap('playing', 'sci', 'cine', 'state')
    t.fire('kill')
    t.wait(1.0)
    t.snap('gone', 'sci', 'cine')
    t.expect.value('playing', 'sci', 'cine', 1)
    t.expect.value('gone', 'sci', 'cine', 0)


# ---------------------------------------------------------------------------------------- trigger_camera (057c8ae5)
@S.test('trigger_camera removed (killtarget) while it has the player\'s view: the player gets his own view back',
        players=1, doc=DOC, covers=('trigger_camera', 'trigger_camera:4'))
def camera_removed(t):
    t.point('info_target', at=(0, 200, 64), targetname='look')
    t.point('trigger_camera', at=(0, -150, 120), targetname='cam', target='look', wait='10', spawnflags='4')
    t.point('trigger_relay', targetname='kill', killtarget='cam')
    tough(t)
    t.fire('cam', activator='player1')
    t.wait(0.5)
    t.mark('kill')
    t.fire('kill')
    t.wait(0.5)
    t.snap('after', 'player1', 'flags')
    t.expect.setview('player1', 'cam', count=1)
    t.expect.setview('player1', 'player1', after='kill', min=1)
    t.expect.value('after', 'player1', 'flags', 4096, op='!&')     # FL_FROZEN


# ------------------------------------------------------------------------------------------- squad (b4502f71)
@S.test('squad: a killed member leaves its leader\'s squad (SquadCount)', doc=DOC,
        covers=('monster_human_grunt', 'monster_human_grunt.netname', 'monster_human_grunt:32'))
def squad_member_removed(t):
    t.point('monster_human_grunt', at=(-100, 150, 0), targetname='lead', netname='sq', spawnflags='32',
            angles='0 270 0', names=('netname',))
    t.point('monster_human_grunt', at=(100, 150, 0), targetname='mem', netname='sq', angles='0 270 0',
            names=('netname',))
    t.flags('lead', '+FL_NOTARGET')
    t.flags('mem', '+FL_NOTARGET')
    t.wait(1.0)
    t.snap('before', 'lead', 'squad')
    t.damage('mem', 1000, 4096)     # DMG_NEVERGIB: its corpse stays (a gibbed one's handle goes NULL)
    t.wait(0.5)
    t.snap('after', 'lead', 'squad')
    t.expect.value('before', 'lead', 'squad', 2)
    t.expect.value('after', 'lead', 'squad', 1)


# --------------------------------------------------------------------------------------------- CGib (24651272)
@S.test('gib that never comes to rest fades and frees its edict 10 s after its lifetime', doc=DOC,
        covers=('env_shooter', 'env_shooter.m_flGibLife'))
def gib_never_rests(t):
    t.point('env_shooter', at=(0, 0, 64), targetname='shoot', m_iGibs='1', delay='0.1', m_flVelocity='0',
            m_flVariance='0', m_flGibLife='1', shootmodel='models/hgibs.mdl', angles='0 0 0')
    t.fire('shoot')
    t.wait(0.3)
    # it keeps moving: noclip at 1 unit a second (WaitTillLand waits for a velocity of 0)
    t.kv('@gib', 'movetype', '8')
    t.kv('@gib', 'velocity', '0 0 1')
    t.wait(16.0)
    t.expect.spawned('@gib', min=1)
    t.expect.removed('@gib', min=1)


@S.test('env_shooter without a Material Sound key: its gibs bounce without a sound (None, the FGD default; Half-Life '
        'took Glass); Glass (0) set: glass sounds', doc=DOC,
        covers=('env_shooter.shootsounds', 'env_shooter.shootsounds=-1', 'env_shooter.shootsounds=0'))
def shooter_no_material(t):
    t.point('env_shooter', at=(-100, 0, 100), targetname='quiet', m_iGibs='4', delay='0.1', m_flVelocity='100',
            m_flVariance='0.2', m_flGibLife='1', shootmodel='models/hgibs.mdl', angles='0 0 0')
    t.wait(0.2)
    t.mark('quiet')
    t.fire('quiet')
    t.wait(6.0)         # they land and fade (life 1 s) before the glass shooter
    t.point('env_shooter', at=(100, 0, 100), targetname='glass', m_iGibs='4', delay='0.1', m_flVelocity='100',
            m_flVariance='0.2', m_flGibLife='1', shootmodel='models/hgibs.mdl', angles='0 0 0', shootsounds='0')
    t.wait(0.2)
    t.mark('glass')
    t.fire('glass')
    t.wait(6.0)
    for s in ('debris/glass', 'debris/wood', 'debris/metal', 'debris/flesh', 'debris/concrete'):
        t.expect.sound(s, count=0, after='quiet', within=(0, 4.0))
    t.expect.sound('debris/glass', after='glass', within=(0, 4.0))


# ---------------------------------------------------------------------- doors: satchel / trip mine (bdd3e74e, f43c39cb)
def sliding_door(t):
    """A door that slides 132 units east along the floor (lip -100) up to a wall (a func_wall from x 44); what lies
    in front of the wall is pinned there (a door pushes what it can)."""
    t.brush('func_door', size=(32, 128, 96), at=(-100, 0, 0), targetname='door', angles='0 0 0', speed='100',
            lip='-100', wait='4')
    t.brush('func_wall', size=(32, 256, 128), at=(60, 0, 0))


@S.test('func_door slides into a satchel charge: the charge goes off and the door goes on', doc=DOC,
        covers=('func_door', 'monster_satchel'))
def satchel_door(t):
    sliding_door(t)
    t.point('monster_satchel', at=(36, 0, 8))
    t.wait(1.0)
    t.snap('closed', 'door', 'origin')
    t.fire('door')
    t.wait(2.5)
    t.snap('open', 'door', 'origin')
    t.expect.removed('@monster_satchel', min=1)
    t.expect.moved('closed', 'open', 'door', (132, 0, 0), tol=2)


@S.test('func_door slides into an armed trip mine: the mine goes off and the door goes on', doc=DOC,
        covers=('func_door', 'monster_tripmine'))
def tripmine_door(t):
    sliding_door(t)
    t.point('monster_tripmine', at=(32, 0, 8), angles='90 0 0', spawnflags='1')     # facing up, armed in 1 s
    t.wait(2.0)
    t.snap('armed', '@monster_tripmine', 'solid')
    t.snap('closed', 'door', 'origin')
    t.fire('door')
    t.wait(2.5)
    t.snap('open', 'door', 'origin')
    t.expect.value('armed', '@monster_tripmine', 'solid', 2)        # SOLID_BBOX: armed
    t.expect.removed('@monster_tripmine', min=1)
    t.expect.moved('closed', 'open', 'door', (132, 0, 0), tol=2)


# ------------------------------------------------------------------------- func_door_rotating Starts Open (638a8e70)
@S.test('func_door_rotating Starts Open + Toggle: fired once it swings shut, fired again it swings open again '
        '(Half-Life left it shut for good)', doc=DOC,
        covers=('func_door_rotating', 'func_door_rotating:1', 'func_door_rotating:32', 'func_door_rotating.distance'))
def rotdoor_starts_open(t):
    t.brush('func_door_rotating', size=(64, 8, 64), at=(150, 150, 0), origin=True, targetname='rd', speed='180',
            distance='90', spawnflags='33', wait='-1')     # Starts Open (1), Toggle (32); Z axis: yaw 90 when open
    t.wait(0.5)
    t.snap('spawn', 'rd', 'angles')
    t.fire('rd')
    t.wait(1.0)
    t.snap('closed', 'rd', 'angles')
    t.fire('rd')
    t.wait(1.0)
    t.snap('reopened', 'rd', 'angles')
    t.expect.value('spawn', 'rd', 'angles', [0, 90, 0], tol=0.5)
    t.expect.value('closed', 'rd', 'angles', [0, 0, 0], tol=0.5)
    t.expect.value('reopened', 'rd', 'angles', [0, 90, 0], tol=0.5)


# ------------------------------------------------------------------- momentary_rot_button retrigger (a522ae20, Valve)
@S.test('momentary_rot_button released halfway and used again: its momentary_door goes back with the handle at '
        'once (Half-Life first moved it on the old way)', players=1, doc=DOC,
        covers=('momentary_rot_button', 'momentary_rot_button.target', 'momentary_rot_button.speed',
                'momentary_rot_button.distance', 'momentary_door'))
def momentary_retrigger(t):
    # the handle turns 30 degrees a second over 90; the door slides +x over 90 units (lip 6) and reaches each value
    # in 0.1 s. Released, the handle coasts 0.2 s on; Half-Life's door stopped 0.2 s short of it, then followed the
    # handle forward again at the next use until the handle came back past it.
    t.brush('momentary_rot_button', size=(8, 16, 16), at=(0, 40, 28), origin=True, targetname='mb', target='md',
            speed='30', distance='90')
    t.brush('momentary_door', size=(96, 16, 96), at=(-100, -150, 0), targetname='md', angles='0 0 0', lip='6')
    tough(t)
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.look('player1', 0, 90)
    t.wait(0.5)
    t.snap('start', 'md', 'origin')
    t.walk('player1', 0, 0, 1.5, '+use')          # held 1.5 s: the handle turns about 45 degrees
    t.wait(0.6)
    t.snap('released', 'md', 'origin', 'velocity')
    t.snap('released', 'mb', 'angles', 'avelocity')
    t.console('test_move %s 0 0 -1 +use' % t.ref('player1'))     # used again: the handle turns back
    t.wait(0.05)
    t.snap('again', 'md', 'origin', 'velocity')
    t.wait(0.1)
    t.snap('again2', 'md', 'origin', 'velocity')
    t.snap('again2', 'mb', 'avelocity')
    t.console('test_move %s 0 0 0' % t.ref('player1'))
    t.expect.moved('start', 'released', 'md', min_dist=20)
    t.expect.value('released', 'md', 'velocity', [0, 0, 0], tol=0.01)
    t.expect.value('released', 'mb', 'avelocity', [0, 0, 0], tol=0.01)
    t.expect.value('again2', 'mb', 'avelocity.y', 0, op='<')        # the handle turns back
    t.expect.value('again', 'md', 'velocity.x', 0, op='<=')         # the door never moves on the old way
    t.expect.value('again2', 'md', 'velocity.x', 0, op='<')         # it goes back with the handle


# --------------------------------------------------------------------------- trigger_cdaudio / target_cdaudio (3785caca)
@S.test('trigger_cdaudio fired: every player gets the track, not only the first; the trigger is then removed',
        players=2, doc=DOC, covers=('trigger_cdaudio', 'trigger_cdaudio.health'))
def cdaudio_trigger_all(t):
    t.trigger('trigger_cdaudio', at=(200, 200, 0), targetname='cd', health='5')
    t.wait(0.3)
    t.mark('go')
    t.fire('cd')
    t.wait(0.3)
    t.expect.clcmd('cd play   5', ent='player1', count=1, after='go')
    t.expect.clcmd('cd play   5', ent='player2', count=1, after='go')
    t.expect.removed('cd')


@S.test('target_cdaudio with a radius: the second player walking in starts the track for both (Half-Life checked the '
        'first player only)', players=2, doc=DOC,
        covers=('target_cdaudio', 'target_cdaudio.radius', 'target_cdaudio.health'))
def cdaudio_target_radius(t):
    t.point('target_cdaudio', at=(150, 150, 0), targetname='cdt', health='-1', radius='64')     # -1: stop the CD
    t.teleport('player1', (-150, -150, 0))
    t.teleport('player2', (-150, 150, 0))
    t.wait(1.5)
    t.mark('in')
    t.teleport('player2', (150, 150, 0))
    t.wait(1.0)
    t.expect.clcmd('cd pause', ent='player1', count=1, after='in')
    t.expect.clcmd('cd pause', ent='player2', count=1, after='in')
    t.expect.removed('cdt')


# --------------------------------------------------------------------------- xen_tree / xen_spore_large (65e51398)
@S.test('xen_tree removed (killtarget): its trigger in front of it (xen_ttrigger) goes with it', doc=DOC,
        covers=('xen_tree',))
def xen_tree_trigger_removed(t):
    t.point('xen_tree', at=(0, 0, 0), targetname='tree', angles='0 90 0')
    t.point('trigger_relay', targetname='kill', killtarget='tree')
    t.wait(0.5)
    t.fire('kill')
    t.wait(0.5)
    t.expect.removed('tree')
    t.expect.removed('@xen_ttrigger', min=1)


@S.test('xen_spore_large removed (killtarget): its five leg hulls (xen_hull) go with it', size=(512, 512, 320), doc=DOC,
        covers=('xen_spore_large',))
def xen_spore_hulls_removed(t):
    t.point('xen_spore_large', at=(0, 0, 0), targetname='spore')
    t.point('trigger_relay', targetname='kill', killtarget='spore')
    t.wait(0.5)
    t.fire('kill')
    t.wait(0.5)
    t.expect.removed('spore')
    t.expect.removed('@xen_hull', min=5)


# ------------------------------------------------------------------------------------- player swim (a5565b68)
@S.test('player who jumps into deep water: his animation becomes the swim / tread-water one, the same as a player '
        'put in the water without a jump (Half-Life kept the jump animation)', players=1, size=(512, 512, 256),
        doc=DOC)
def swim_after_jump(t):
    t.brush('func_water', size=(200, 400, 120), at=(120, 0, 0), skin='-3')
    tough(t)
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (120, 0, 0), yaw=0)       # in the water, no jump
    t.wait(1.0)
    t.snap('tread', 'player1', 'sequence', 'waterlevel')
    t.teleport('player1', (-150, 0, 0), yaw=0)      # dry floor
    t.wait(0.5)
    t.move('player1', 0, 0, 3, '+jump')
    t.wait(0.15)
    t.snap('hop', 'player1', 'sequence', 'flags')
    t.teleport('player1', (120, 0, 0), yaw=0)       # still in the jump, into the water
    t.wait(1.0)
    t.snap('swim', 'player1', 'sequence', 'waterlevel')
    t.flags('player1', '-FL_NOTARGET')
    t.expect.value('tread', 'player1', 'waterlevel', 3)
    t.expect.value('swim', 'player1', 'waterlevel', 3)
    t.expect.same('tread', 'swim', 'player1', 'sequence')


# ------------------------------------------------------------------------------------------ func_train (ac445351)
@S.test('func_train stopped by a trigger stops its move sound (SND_STOP)', doc=DOC,
        covers=('func_train', 'func_train.movesnd=1'))
def train_stop_sound(t):
    t.point('path_corner', at=(-200, 0, 64), targetname='c1', target='c2')
    t.point('path_corner', at=(200, 0, 64), targetname='c2', target='c1')
    t.brush('func_train', size=(32, 32, 32), at=(-200, 0, 64), targetname='train', target='c1', speed='50',
            movesnd='1')
    t.fire('train')
    t.wait(1.0)
    t.mark('stop')
    t.fire('train')
    t.wait(0.5)
    t.expect.sound('bigmove1', ent='train', flags=SND_STOP, after='stop', within=(0, 0.3), min=1)


# ---------------------------------------------------------------------------------- monster_apache (3951ca2b, 5142f7fb)
@S.test('monster_apache shot down above a player: the wreck explodes when it lands on him', players=1, size=(512, 512, 512), doc=DOC,
        covers=('monster_apache',))
def apache_wreck_on_player(t):
    t.point('monster_apache', at=(0, 0, 300), targetname='heli', angles='0 0 0', spawnflags='4')    # Wait For Trigger
    tough(t)
    t.flags('player1', '+FL_NOTARGET')
    t.teleport('player1', (0, 0, 0), yaw=90)
    t.wait(0.5)
    t.mark('kill')
    t.damage('heli', 10000)
    t.wait(4.0)
    t.flags('player1', '-FL_NOTARGET')
    t.expect.sound('mortarhit', ent='heli', after='kill', within=(0, 3.0), min=1)


# -------------------------------------------------------------------------------------------- func_vehicle (6e82302b)
@S.test('func_vehicle pitch event: volume, sound and pitch as they are (fparam1, iparam1, iparam2); the stop names the sound',
        doc=DOC, covers=('func_vehicle', 'func_vehicle.sounds=3'))
def vehicle_pitch(t):
    t.point('path_track', at=(-150, 0, 16), targetname='vp1', target='vp2')
    t.point('path_track', at=(150, 0, 16), targetname='vp2')
    t.brush('func_vehicle', size=(64, 32, 16), at=(-150, 0, 0), origin=True, targetname='car', target='vp1',
            speed='300', sounds='3', length='64', width='32', height='4')
    t.wait(0.5)
    t.mark('go')
    t.fire('car')           # speed 300: pitch 60 + 300 * 140 / 1500 = 88; with nobody at the wheel it slows down 20
    t.wait_frames(1)        # every 0.1 s (86 after one step); stopped again while it still moves
    t.mark('stop')
    t.fire('car')
    t.wait(0.3)
    t.expect.event('vehicle', ent='car', fields={'iparam1': 3, 'iparam2': (84, 89), 'bparam1': 0}, after='go',
                   min=1)
    t.expect.event('vehicle', ent='car', fields={'iparam1': 3, 'bparam1': 1}, after='stop', min=1)


@S.test('func_vehicle Initial speed 220: at map start it rolls forward from its first path_track, past the next one '
        '(a vehicle does not stop at its track), and slows down to a stop (nobody at the wheel); no wall in its way',
        doc=DOC, size=(512, 512, 200),
        covers=('func_vehicle.startspeed',))
def vehicle_startspeed(t):
    t.point('path_track', at=(-200, 0, 16), targetname='vp1', target='vp2')
    t.point('path_track', at=(100, 0, 16), targetname='vp2')
    t.brush('func_vehicle', size=(64, 32, 16), at=(-200, 0, 0), origin=True, targetname='car', target='vp1',
            speed='300', sounds='3', length='64', width='32', height='4', startspeed='220')
    t.wait(0.2)
    for n in ('car', 'vp1', 'vp2'):
        t.snap('s', n, 'origin', *(('speed',) if n == 'car' else ()))
    t.expect.value('s', 'car', 'speed', 0)
    t.expect.distance('s', 'car', 'vp1', 320, 440)      # about 390 units forward: past vp2 (300 ahead of vp1)
    t.expect.distance('s', 'car', 'vp2', 20, 140)


@S.test('func_vehicle against walls in front and behind: the third bounce in a row stops it (ReGameDLL); it does '
        'not shake back and forth', doc=DOC, covers=('func_vehicle', 'func_vehicle.speed'))
def vehicle_bounce_stop(t):
    t.point('path_track', at=(0, 0, 16), targetname='vp1', target='vp2')
    t.point('path_track', at=(200, 0, 16), targetname='vp2')
    t.brush('func_vehicle', size=(64, 32, 16), at=(0, 0, 0), origin=True, targetname='car', target='vp1',
            speed='300', sounds='3', length='64', width='32', height='4')
    # 4 units from each end, 64 thick: a step of a loaded server (0.15 s at 300) cannot carry it through
    t.brush('func_wall', size=(64, 128, 96), at=(68, 0, 0))
    t.brush('func_wall', size=(64, 128, 96), at=(-68, 0, 0))
    t.wait(0.5)
    t.fire('car')
    t.wait(0.5)
    t.snap('s', 'car', 'speed', 'velocity')
    t.expect.value('s', 'car', 'speed', 0)
    t.expect.value('s', 'car', 'velocity', [0, 0, 0])


# ------------------------------------------------------------------------------------- turret death notice (342671f5)
@S.test('monstermaker of turrets with 1 live child: a destroyed turret frees the slot, the next one is made '
        '(NotifyOwnerOfDeath from TakeDamage)', doc=DOC,
        covers=('monstermaker', 'monstermaker.monstertype', 'monstermaker.monstercount',
                'monstermaker.m_imaxlivechildren'))
def turret_maker(t):
    t.point('monstermaker', at=(0, 100, 0), targetname='mk', monstertype='monster_turret', monstercount='2',
            m_imaxlivechildren='1', delay='0.5', netname='tur', spawnflags='0', names=('netname',))
    t.fire('mk')
    t.wait(1.0)
    t.damage('tur', 100000)
    t.wait(4.0)
    t.expect.spawned('@monster_turret', count=2)


@S.test('monstermaker with infinite children (monstercount -1): its count stays -1 as it makes them (Half-Life '
        'Unified SDK; it counted down and stopped after about 4.29 billion)', doc=DOC,
        covers=('monstermaker.monstercount',))
def maker_infinite_count(t):
    t.point('monstermaker', at=(0, 100, 0), targetname='mk', monstertype='monster_turret', monstercount='-1',
            m_imaxlivechildren='1', delay='0.3', netname='inf', spawnflags='0', names=('netname',))
    t.point('trigger_relay', targetname='rm', killtarget='inf')     # makes room for the next child
    t.fire('mk')
    for _ in range(3):
        t.wait(0.6)
        t.fire('rm')
    t.wait(0.6)
    t.snap('made', 'mk', 'save')
    t.fire('mk', use='off')
    t.expect.spawned('@monster_turret', min=3)
    t.expect.value('made', 'mk', 'save.m_cNumMonsters', -1)


@S.test('monstermaker makes a working monster_turret (the FGD choice "Auto Turret"): with childspawnflags 32 '
        '(Auto-activate) it deploys and shoots a player in sight', players=1, doc=DOC,
        covers=('monstermaker.monstertype=monster_turret', 'monstermaker.childspawnflags'))
def turret_maker_fires(t):
    tough(t)
    t.teleport('player1', (0, -180, 0), yaw=90)
    t.point('monstermaker', at=(0, 120, 0), targetname='mk', monstertype='monster_turret', monstercount='1',
            childspawnflags='32')
    t.mark('make')
    t.fire('mk')
    t.wait(6.0)
    t.expect.spawned('@monster_turret', count=1)
    t.expect.sound('turret/tu_fire1.wav', after='make')
    t.expect.damaged('player1', attacker='@monster_turret', after='make')


# ---------------------------------------------------------------------------- func_rot_button lock sounds (814a09dd)
CHAN_VOICE, CHAN_ITEM = 2, 3
SPARKS = ['buttons/spark%d.wav' % i for i in range(1, 7)]


def rot_button(t, name, at, **kv):
    """A rotating button (use activated) turning 90 degrees at 400 deg/s, back after 1 s."""
    t.brush('func_rot_button', size=(32, 8, 32), at=at, origin=True, tex='+0~ANZYG1', targetname=name,
            speed='400', distance='90', wait='1', **kv)


@S.test('func_rot_button locked by its master plays its locked sound and sentence (CBaseButton::Precache now runs '
        'for it); nothing fires', doc=DOC, players=1,
        covers=('func_rot_button', 'func_rot_button.master', 'func_rot_button.locked_sound=2',
                'func_rot_button.locked_sentence=1'))
def rot_button_locked(t):
    t.point('env_state', targetname='st')                   # off: the master is locked
    t.point('info_target', targetname='out')
    rot_button(t, 'rb', (0, 100, 48), target='out', master='st', locked_sound='2', locked_sentence='1')
    t.wait(0.3)
    t.fire('rb', activator='player1')
    t.wait(0.3)
    t.expect.precached('buttons/button2.wav', kind='sound')
    t.expect.sound('button2.wav', ent='rb', chan=CHAN_ITEM)     # "Access Denied"
    t.expect.sound('NA', ent='rb', chan=CHAN_VOICE)             # sentence group NA: "Gen. Access Denied"
    t.expect.not_fired('out')


@S.test('func_rot_button unlocked plays its unlocked sound and sentence; a custom unlocked sound '
        '(unlocked_sound_override) replaces the numbered one', doc=DOC, players=1,
        covers=('func_rot_button', 'func_rot_button.target', 'func_rot_button.unlocked_sound=3',
                'func_rot_button.unlocked_sentence=1', 'func_rot_button.unlocked_sound_override'))
def rot_button_unlocked(t):
    t.point('info_target', targetname='out')
    t.point('info_target', targetname='out2')
    rot_button(t, 'rb', (-80, 100, 48), target='out', unlocked_sound='3', unlocked_sentence='1')
    rot_button(t, 'rb2', (80, 100, 48), target='out2', unlocked_sound='3',
               unlocked_sound_override='buttons/button10.wav')
    t.wait(0.3)
    t.fire('rb', activator='player1')
    t.fire('rb2', activator='player1')
    t.wait(0.5)
    t.expect.sound('button3.wav', ent='rb', chan=CHAN_ITEM)     # "Access Granted"
    t.expect.sound('EA', ent='rb', chan=CHAN_VOICE)             # sentence group EA: "Gen. Access Granted"
    t.expect.sound('button10.wav', ent='rb2', chan=CHAN_ITEM)
    t.expect.sound('button3.wav', ent='rb2', count=0)
    t.expect.fired('out', count=1)
    t.expect.fired('out2', count=1)


@S.test('func_rot_button with X Axis (64, func_button\'s "Sparks" bit) and no lock keys: turns and fires, loads no '
        'spark sounds and plays none (the precaches are those of 1.38)', doc=DOC, players=1,
        covers=('func_rot_button', 'func_rot_button:64', 'func_rot_button.target'))
def rot_button_x_axis(t):
    t.point('info_target', targetname='out')
    rot_button(t, 'rb', (0, 100, 48), target='out', spawnflags='64')
    t.wait(0.3)
    t.fire('rb', activator='player1')
    t.wait(1.8)
    t.expect.fired('out', count=1)
    for name in SPARKS:
        t.expect.not_precached(name)
    t.expect.sound('spark', count=0)


# ------------------------------------------------------------------------------- func_tank with no activator (066169c5)
@S.test('controllable func_tank fired with no activator: ignored with a warning', doc=DOC, covers=('func_tank',))
def tank_null_activator(t):
    tank(t)
    t.wait(0.2)
    t.fire('tk')        # test_fire tk - -: no activator, no caller
    t.wait(0.5)
    t.snap('alive', 'tk', 'solid')
    t.expect.value('alive', 'tk', 'solid', 4)      # the server is still up (SOLID_BSP)
