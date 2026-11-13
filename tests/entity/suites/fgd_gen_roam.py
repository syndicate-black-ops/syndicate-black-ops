"""Tier 5, credit audit, generated: "freeroam" and "active_alert" on the monsters whose own GetSchedule decides the
idle / alert states before the shared code reads the keys (CBaseMonster::GetSchedule; etest/code_paths.py own_cases):
the Cop, the assassin, the imp, the scientist. Freeroam Always (2): an idle one walks from node to node; Map default
(0, worldspawn freeroam off) and Never (1): it stands. Active after combat Always (2): once its enemy is dead it goes to
the gunfire it hears or walks around once; Default (0) and Never (1): it never roams. The player is notarget."""

from etest import Suite

SUITE = Suite('fgd_gen_roam', 'Tier 5: freeroam and active_alert on classes with their own idle / alert schedule',
              players=1)
S = SUITE
DOC = 'FEATUREFUL_OPTIONS.md#monster-ai-keys'
CLASSES = ('monster_barney', 'monster_human_assassin', 'monster_imp', 'monster_scientist')
WAKE = 6.0      # a named monster stands its first 5 s (SCHED_WAIT_TRIGGER) before it looks around
MIDS = 75         # a snapshot every 0.2 s for 15 s: a roam a sound cuts short comes again at the next alert pick
MID_LABELS = ('after',) + tuple('mid%d' % k for k in range(MIDS))
ROAMS = ('Free Roaming (alert)', 'InvestigateSound')


def node_grid(t, step=96, lo=-200, hi=200):
    for x in range(lo, hi + 1, step):
        for y in range(lo, hi + 1, step):
            t.point('info_node', at=(x, y, 16))


def short(cls):
    return cls.replace('monster_', '')


def freeroam_case(cls):
    @S.test('%s "freeroam" Always (2): idle, it walks from node to node; Map default (0, the world\'s freeroam off) '
            'and Never (1): it stands' % cls, players=1, doc=DOC,
            covers=tuple('%s.freeroam=%d' % (cls, v) for v in (0, 1, 2)) + ('%s.freeroam' % cls,))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        node_grid(t)
        spots = {'2': (-150, -150, 0), '0': (150, 150, 0), '1': (150, -150, 0)}
        for v, at in spots.items():
            # Gag (2): no chatter; Prisoner to Player (prisonerto 2): it takes no interest in the player
            t.point(cls, targetname='m%s' % v, at=at, angles='0 90 0', spawnflags='2', freeroam=v,
                    prisonerto='2')
            t.point('info_target', targetname='m%s0' % v, at=at)
        t.teleport('player1', (-200, 200, 0))
        t.wait(WAKE + 9.0)        # the Cop walks slowly between nodes
        for v in spots:
            t.snap('b', 'm' + v, 'origin')
            t.snap('b', 'm%s0' % v, 'origin')
        t.expect.distance('b', 'm2', 'm20', 48, 1000)
        t.expect.distance('b', 'm0', 'm00', 0, 16)
        t.expect.distance('b', 'm1', 'm10', 0, 16)
    case.__name__ = 'freeroam_' + short(cls)
    return case


def alert_case(cls, value):
    roams = value == '2'

    @S.test('%s "active_alert" %s: after its enemy (a monster_target counted as an insect) is gone, it %s' % (
                cls, {'2': 'Always (2)', '0': 'Default (0)', '1': 'Never (1)'}[value],
                'goes to the gunfire it hears or walks around (a roam a sound cuts short comes again) and by the end '
                'it has walked off' if roams else 'never roams'),
            players=1, doc=DOC, covers=('%s.active_alert=%s' % (cls, value), '%s.active_alert' % cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        node_grid(t)
        # prisonerto 2: it never takes the player as its enemy (it could see him passing and keep him)
        t.point(cls, targetname='mon', at=(-150, 0, 0), angles='0 0 0', active_alert=value, prisonerto='2',
                spawnflags='2')
        # the enemy: a monster_target counted as an insect (every class hates insects); it fires nothing back, so
        # no gunfire but its own is heard after the fight
        t.point('monster_target', targetname='victim', at=(150, 0, 32), frags='10')
        t.point('trigger_relay', targetname='kill', killtarget='victim')
        t.wait(0.3)
        t.kv('mon', 'health', '100000')
        t.teleport('player1', (200, -200, 0))
        t.wait(WAKE)              # it sees the target and fights (or fears) it
        fights = []
        for k in range(4):
            t.wait(1.0)
            fights.append('fight%d' % k)
            t.snap('fight%d' % k, 'mon', 'enemy', 'state')
        t.fire('kill')            # the enemy is gone: the fight is over
        t.wait(0.5)
        t.snap('after', 'mon', 'origin', 'state', 'schedule', 'enemy')
        for k in range(MIDS):
            t.wait(0.2)
            t.snap('mid%d' % k, 'mon', 'state', 'schedule')
        t.snap('end', 'mon', 'origin')
        t.flags('player1', '-FL_NOTARGET')
        t.expect.value_any(fights, 'mon', 'enemy', 'monster_target')
        if roams:
            t.expect.value_any(MID_LABELS, 'mon', 'schedule', list(ROAMS))
            t.expect.moved('after', 'end', 'mon', min_dist=64)        # the end state: it walked off, not stood
        else:
            for lab in MID_LABELS:
                t.expect.value(lab, 'mon', 'schedule', 'Free Roaming (alert)', op='!=')
    case.__name__ = 'active_alert_%s_%s' % (value, short(cls))
    return case


for _c in CLASSES:
    freeroam_case(_c)
    for _v in ('2', '0', '1'):
        alert_case(_c, _v)
