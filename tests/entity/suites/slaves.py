"""The alien slave family keeps the map's spawnflags: monster_alien_slave (hunter.cpp, CISlave),
monster_hunter (hunter2.cpp), monster_werewolf (werewolf.cpp) and monster_werewolf_hunter (werewolf_hunter.cpp).
Their GetSchedule has the spawnflags clear commented out, as Half-Life's islave.cpp and SBO's dark.cpp, so Prisoner
(16), Fade corpse (512) and the other flags stay after the first schedule.

Each test places one monster with Prisoner + Fade corpse (528, as co_bloc_b's slave had), wakes it (a named monster
starts in SCHED_WAIT_TRIGGER; Use ends it, as in suite `cyborg`), lets it think for two seconds and reads its
spawnflags and schedule (Alert Stand: GetSchedule has run)."""

from etest import Suite

SUITE = Suite('slaves', 'alien slave family: the map\'s spawnflags stay after the first schedule')
S = SUITE
ROOM = (512, 512, 512)
SF_PRISONER = 16
SF_FADECORPSE = 512
FLAGS = SF_PRISONER | SF_FADECORPSE

SLAVES = (
    ('alien_slave', 'monster_alien_slave'),
    ('hunter', 'monster_hunter'),
    ('werewolf', 'monster_werewolf'),
    ('werewolf_hunter', 'monster_werewolf_hunter'),
)


def make_test(key, classname):
    def keeps_flags(t):
        t.point(classname, at=(-200, 0, 0), targetname='npc', angles='0 0 0', spawnflags=str(FLAGS))
        t.point('trigger_relay', targetname='kill_npc', killtarget='npc')
        t.console('wait_for alive player1 20')
        t.teleport('player1', (150, 0, 0), yaw=180)
        t.kv('player1', 'health', '100000')
        t.kv('player1', 'max_health', '100000')
        t.snap('start', 'npc', 'spawnflags', 'schedule')
        t.fire('npc', use='on')
        t.wait(2.0)
        t.snap('a', 'npc', 'spawnflags', 'schedule', 'enemy')
        t.wait(1.0)
        t.snap('b', 'npc', 'spawnflags', 'schedule')
        t.expect.value('a', 'npc', 'schedule', 'Alert Stand')         # GetSchedule has run since
        t.expect.value('start', 'npc', 'spawnflags', FLAGS, tol=0)
        t.expect.value('a', 'npc', 'spawnflags', FLAGS, tol=0)     # 1.38: 0 after the first schedule
        t.expect.value('b', 'npc', 'spawnflags', FLAGS, tol=0)
        t.expect.value('a', 'npc', 'enemy', '')                    # a prisoner takes nobody as its enemy
        t.fire('kill_npc')
        t.wait(0.2)

    keeps_flags.__name__ = 'keeps_flags_' + key
    S.test('%s with Prisoner + Fade corpse (528): the flags stay after its first schedules' % classname,
           players=1, size=ROOM, covers=(classname, classname + ':16', classname + ':512'))(keeps_flags)


for _key, _classname in SLAVES:
    make_test(_key, _classname)
