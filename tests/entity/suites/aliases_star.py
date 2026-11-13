"""A bare "*" as a Target (docs/ALIASES.md "What * means"): still the wildcard that matches every name, aliases or
not. Its own suite: it triggers every named entity in the map. Branch feat/spirit-aliases."""

from etest import Suite

SUITE = Suite('aliases_star', 'Aliases: a bare * is the wildcard for every name', players=1,
              requires=('Alias_FindEdict',))
S = SUITE


@S.test('A Target of "*" alone triggers every named entity, the firing entity excepted', players=1,
        doc='ALIASES.md', covers=('info_alias',))
def bare_star(t):
    t.point('info_target', targetname='a')
    t.point('info_target', targetname='b')
    t.point('info_alias', targetname='al', target='a')
    t.point('trigger_relay', targetname='r', target='=*', triggerstate='2')
    t.fire('r')
    t.wait(0.1)
    t.expect.used('a', caller='r', count=1)
    t.expect.used('b', caller='r', count=1)
    t.expect.used('r', count=1)       # only the test's own trigger: the relay skips itself
