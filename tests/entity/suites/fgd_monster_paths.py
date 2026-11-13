"""Tier 5, final pass: a monster's Target (FGD base Monster: its first path_corner). StartMonster (monsters.cpp) sets
the path_corner as its goal; named, it waits for a trigger, then walks to the corner. The walker is made during its
test (test_create; a placed one would start at the map start, before its test); a copy
without a target is placed so the map precaches the class. A monsters-only trigger_once on the path_corner ends the
wait when the walker arrives. The player is notarget, so the monsters do not fight."""

from etest import Suite

SUITE = Suite('fgd_monster_paths', 'Tier 5: monster Target walks the path', players=1)
S = SUITE
CLASSES = ('dark_grunt', 'monster_alien_slave', 'monster_barney', 'monster_cyberminotor', 'monster_cyborg',
           'monster_gargantua', 'monster_generic', 'monster_grunt_hunter', 'monster_human_assassin',
           'monster_human_grunt', 'monster_hunter', 'monster_imp', 'monster_imp_hunter', 'monster_scientist',
           'monster_zombie', 'monster_zombie_grunt',
           'monster_werewolf', 'monster_werewolf_hunter', 'monster_zombie_hunter')
EXTRA = {'monster_generic': {'model': 'models/scientist.mdl'}}
XFAIL = {}


def path_case(cls):
    @S.test('%s Target: it walks to its first path_corner' % cls, players=1, doc='FGD Monster base target',
            covers=('%s.target' % cls,), xfail=XFAIL.get(cls))
    def case(t):
        t.flags('player1', '+FL_NOTARGET')
        t.teleport('player1', (-200, -200, 0))
        t.point('path_corner', targetname='pc', at=(0, 180, 0))
        # monsters only, no clients (3): fires 'arrived' when the walker reaches the corner
        t.trigger('trigger_once', size=(64, 64, 72), at=(0, 180, 0), target='arrived', spawnflags='3')
        # a copy so the map precaches the class, removed before the walker is made (so '@class' is the walker)
        t.point(cls, targetname='pre', at=(-200, 200, 0), angles='0 270 0', **EXTRA.get(cls, {}))
        t.point('trigger_relay', targetname='rm', killtarget='pre')
        t.fire('rm')
        t.wait(0.2)
        p = t._abs((0, -100, 0))
        # unnamed: a named monster waits for a trigger instead of starting its path (StartMonster)
        # Prisoner (16): the walkers of the other tests stay in the map, and hunters see enemies through walls; a
        # prisoner sees nobody and nobody attacks it, so it is left to its path
        words = ['test_create', cls, '%g' % p[0], '%g' % p[1], '%g' % p[2], '90', 'target', t.name('pc'),
                 'spawnflags', '16', 'netname', t.name('squad')]   # a squad of its own
        for k, v in EXTRA.get(cls, {}).items():
            words += [k, v]
        t.console(' '.join(words))
        t.wait(0.3)
        t.snap('a', '@' + cls, 'origin')
        t.wait_for_fire('arrived', 25.0)
        t.snap('b', '@' + cls, 'origin', 'state', 'schedule', 'enemy')
        t.snap('b', 'pc', 'origin')
        t.expect.distance('b', '@' + cls, 'pc', 0, 80)
    case.__name__ = 'path_' + cls.replace('monster_', '')
    return case


for _c in CLASSES:
    path_case(_c)
