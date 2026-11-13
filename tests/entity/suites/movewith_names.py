"""Entity parenting, names (docs/MOVEWITH.md): a parent name and trigger_changeparent's target and new parent resolve
as every other targeting entity's names do (UTIL_FindEntityByTargetname): "*alias", "group.member", "name*", and
!activator / !caller on trigger_changeparent. The exact name comes first, then "name.N" (an attachment)."""

from etest import Suite

SUITE = Suite('movewith_names', 'Entity parenting: parent names (feat/movewith)', requires=('MoveWith_StartFrame',))
S = SUITE
S.server_args = ('+developer', '1')
DOC = 'MOVEWITH.md'
METAL = '666MURMETAL'


def mover(t, name, at, distance=192, speed=192, size=(32, 32, 32)):
    """A func_door moving +x by `distance` (lip = size - distance), staying there."""
    return t.brush('func_door', size=size, at=at, tex=METAL, targetname=name, angles='0 0 0', speed=str(speed),
                   lip=str(size[0] - distance), wait='-1')


def follows(t, kid, parent, label=''):
    t.snap('a' + label, kid, 'origin')
    t.fire(parent)
    t.wait(1.5)
    t.snap('b' + label, kid, 'origin')
    t.expect.moved('a' + label, 'b' + label, kid, (192, 0, 0), tol=2)


@S.test('trigger_changeparent target !activator: the entity that fired it gets the new parent', doc=DOC)
def changeparent_activator(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid')
    t.point('trigger_changeparent', targetname='cp', target='!activator', m_iszNewParent='p')
    t.wait(0.2)
    t.fire('cp', activator='kid')
    t.wait(0.1)
    follows(t, 'kid', 'p')


@S.test('trigger_changeparent new parent !activator: the target moves with the entity that fired it', doc=DOC)
def changeparent_new_activator(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='kid')
    t.point('trigger_changeparent', targetname='cp', target='kid', m_iszNewParent='!activator')
    t.wait(0.2)
    t.fire('cp', activator='p')
    t.wait(0.1)
    t.expect.no_console('no entity named')
    follows(t, 'kid', 'p')


@S.test('movewith "*alias": the child moves with the entity the info_alias names', doc=DOC)
def parent_alias(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_alias', targetname='al', target='p')
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='*al')
    t.wait(0.2)
    follows(t, 'kid', 'p')


@S.test('movewith "group.member": the child moves with that info_group member', doc=DOC)
def parent_group(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_group', targetname='grp', base='p')
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='grp.base')
    t.wait(0.2)
    follows(t, 'kid', 'p')


@S.test('movewith "door.1" names an entity called "door.1" before an attachment of "door"', doc=DOC)
def parent_exact_dotted(t):
    mover(t, 'door.1', (-150, 150, 0))
    t.point('cycler', at=(150, 150, 0), targetname='door', model='models/apache.mdl')
    t.point('info_target', at=(-150, 100, 16), targetname='kid', movewith='door.1')
    t.wait(0.2)
    follows(t, 'kid', 'door.1')


@S.test('trigger_changeparent target "crate*": every entity whose name starts so gets the new parent', doc=DOC)
def changeparent_prefix(t):
    mover(t, 'p', (-150, 150, 0))
    t.point('info_target', at=(-150, 100, 16), targetname='crate_a')
    t.point('info_target', at=(-150, 60, 16), targetname='crate_b')
    t.point('trigger_changeparent', targetname='cp', target='crate*', m_iszNewParent='p')
    t.wait(0.2)
    t.fire('cp')
    t.wait(0.1)
    t.snap('a', 'crate_a', 'origin')
    t.snap('a', 'crate_b', 'origin')
    t.fire('p')
    t.wait(1.5)
    t.snap('b', 'crate_a', 'origin')
    t.snap('b', 'crate_b', 'origin')
    t.expect.moved('a', 'b', 'crate_a', (192, 0, 0), tol=2)
    t.expect.moved('a', 'b', 'crate_b', (192, 0, 0), tol=2)
