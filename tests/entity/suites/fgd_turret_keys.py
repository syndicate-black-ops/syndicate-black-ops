"""Tier 5, final pass: the turrets' "Custom Search Time" (maxsleep): once its enemy is out of sight (a wall turned
on between them; a turret goes on firing at a notarget player it can still see), a turret searches (pings) that
long, then spins down and retires (turret.cpp CBaseTurret::SearchThink / Retire: the retract sound,
turret/tu_deploy.wav at pitch 120). With 2 s it retires within 13 s; one left at the default (15 s) later.
"turnrate" (CBaseTurret::MoveTurret): how fast it turns to its enemy; it fires only within 30 degrees of him."""

from etest import Suite

SUITE = Suite('fgd_turret_keys', 'Tier 5: turret search time and turn rate', players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md'


def sleep_case(cls):
    @S.test('%s "maxsleep" 2: it searches 2 s once it lost the player, then retires; without the key it searches '
            '15 s and retires later' % cls, players=1, doc=DOC, covers=('%s.maxsleep' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.flags('player1', '+FL_GODMODE')
        t.teleport('player1', (0, -200, 0), yaw=90)
        t.point(cls, targetname='pre', at=(200, 200, 0), angles='0 270 0', spawnflags='16')
        t.point('trigger_relay', targetname='rm', killtarget='pre')
        t.fire('rm')
        # off at first (Starts Invisible, 1): turned on, it hides the player from both turrets
        t.brush('func_wall_toggle', targetname='wall', size=(512, 16, 256), at=(0, 40, 0), spawnflags='1')
        for name, x, kv in (('quick', -120, ['maxsleep', '2']), ('slow', 120, [])):
            p = t._abs((x, 150, 0))
            t.console(' '.join(['test_create', cls, '%g' % p[0], '%g' % p[1], '%g' % p[2], '270', 'targetname',
                                t.name(name), 'spawnflags', '32'] + kv))      # Autostart (32)
        t.wait(0.5)
        t.flags('player1', '-FL_NOTARGET')
        t.wait(6.0)                     # both see him, deploy, spin up and fire
        t.fire('wall')
        t.flags('player1', '+FL_NOTARGET')
        t.mark('lost')
        t.wait(30.0)
        t.flags('player1', '-FL_GODMODE')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.sound('turret/tu_deploy', ent='quick', pitch=120, after='lost', within=(2.0, 13.0), min=1)
        t.expect.sound('turret/tu_deploy', ent='slow', pitch=120, after='lost', within=(0.0, 13.0), count=0)
        t.expect.sound('turret/tu_deploy', ent='slow', pitch=120, after='lost', within=(13.0, 30.0), min=1)
    case.__name__ = 'maxsleep_' + cls.replace('monster_', '')
    return case


for _c in ('monster_turret', 'monster_miniturret'):
    sleep_case(_c)


def turnrate_case(cls):
    @S.test('%s "turnrate" 1: deployed facing north, it turns so slowly (1 degree a tenth of a second at most) that it '
            'does not hit the player east of it within 5 s; at the default (30) it does' % cls, players=1, doc=DOC,
            covers=('%s.turnrate' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.kv('player1', 'max_health', '50000')
        t.kv('player1', 'health', '50000')
        t.teleport('player1', (220, 0, 0), yaw=180)
        t.point(cls, targetname='pre', at=(-200, 200, 0), angles='0 90 0', spawnflags='16')
        t.point('trigger_relay', targetname='rm', killtarget='pre')
        t.point('trigger_relay', targetname='rm_t', killtarget='tt')
        t.fire('rm')
        # a wall between them until the turret is deployed (Autostart, 32), facing north (90)
        t.brush('func_wall_toggle', targetname='wall', size=(16, 256, 256), at=(150, 0, 0))
        for name, kv in (('slow', ['turnrate', '1']), ('fast', [])):
            p = t._abs((0, 0, 0))
            t.console(' '.join(['test_create', cls, '%g' % p[0], '%g' % p[1], '%g' % p[2], '90', 'targetname',
                                t.name('tt'), 'spawnflags', '32'] + kv))
            t.wait(0.3)
            t.flags('player1', '-FL_NOTARGET')
            t.wait(3.0)                 # deployed and searching
            t.mark(name)
            t.fire('wall')              # off: it sees the player, 90 degrees to its right
            t.wait(5.0)
            t.flags('player1', '+FL_NOTARGET')
            t.fire('wall')
            t.fire('rm_t')
            t.wait(0.5)
        t.flags('player1', '-FL_NOTARGET')
        t.kv('player1', 'health', '100')
        t.expect.not_damaged('player1', after='slow', within=(0, 5.0))
        t.expect.damaged('player1', min=1, after='fast', within=(0, 5.0))
    case.__name__ = 'turnrate_' + cls.replace('monster_', '')
    return case


for _c in ('monster_turret', 'monster_miniturret', 'monster_sentry'):
    turnrate_case(_c)


def target_case(cls):
    @S.test('%s Target: fired On when it is destroyed%s' % (
            cls, '' if cls == 'monster_sentry' else ', On when it switches on and Off when it switches off'),
            players=1, doc=DOC, covers=('%s.target' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (0, -200, 0), yaw=90)
        t.point(cls, targetname='tur', at=(0, 150, 0), angles='0 270 0', target='tur_out')
        t.point('info_target', targetname='tur_out')
        t.wait(0.5)
        if cls != 'monster_sentry':
            t.mark('on')
            t.fire('tur', use='on')
            t.wait(2.0)
            t.mark('off')
            t.fire('tur', use='off')
            t.wait(3.0)
            t.expect.used('tur_out', use=1, count=1, after='on', within=(0, 2.0))
            t.expect.used('tur_out', use=0, count=1, after='off', within=(0, 3.0))
        t.mark('kill')
        t.damage('tur', 5000, attacker='player1')
        t.wait(0.5)
        t.expect.used('tur_out', use=1, count=1, after='kill', within=(0, 0.5))
    case.__name__ = '%s_target' % cls.replace('monster_', '')
    return case


for _c in ('monster_turret', 'monster_miniturret', 'monster_sentry'):
    target_case(_c)
