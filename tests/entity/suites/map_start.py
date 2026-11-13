"""What a new map starts with when the server changes to it (world.cpp, CWorld::Precache): an empty global state table
and, for a map without a "skyname" (the suite's map has none), Half-Life's default sky (Half-Life Unified SDK). Each
test reloads the suite's own map with "changelevel", as the map cycle does, and checks the reloaded map. The engine
drops test players on a map change, so the suite has none."""

from etest import Suite

SUITE = Suite('map_start', 'a new map: global state cleared, default sky (changelevel to the same map)', players=0)
S = SUITE
MAP = SUITE.map_prefix + 'et_' + SUITE.name


def reload_map(t):
    """The suite's map again, as the map cycle changes maps; the steps go on there (its clock starts again)."""
    t.console('changelevel ' + MAP)
    t.wait_frames(20)


@S.test('a func_breakable with a "globalname" that was broken is there again when the map comes back',
        covers=('func_breakable', 'func_breakable.globalname'))
def globalname_back(t):
    t.brush('func_breakable', size=(32, 32, 32), at=(0, 0, 16), targetname='crate', globalname='et_ms_crate',
            health='1', material='1')
    t.wait(0.2)
    t.mark('break')
    t.damage('crate', 50, 0, 'world')
    t.wait(0.5)
    t.expect.removed('crate', after='break')
    reload_map(t)
    t.snap('back', 'crate', 'health', 'solid')
    t.expect.value('back', 'crate', 'health', 1)


@S.test('a map without a "skyname" gets the default sky (desert), not the sky of the map before it',
        covers=('worldspawn',))
def sky_reset(t):
    t.console('sv_skyname space')      # the sky a map before it set
    reload_map(t)
    t.console('sv_skyname')            # prints the value
    t.wait_frames(2)
    t.expect.console('"sv_skyname" is "desert"')
