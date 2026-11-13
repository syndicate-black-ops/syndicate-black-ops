"""Map kit for the entity tests: sealed box rooms, brush and point entities, written as a Valve 220 .map.

Every room is its own sealed box (no leaks, no shared walls), laid out on a grid; the lobby holds the spawn points.
Textures come from the mod's own anzymut.wad (walls, tool textures AAATRIGGER and ORIGIN).
"""

WALL = '666CEMENT'
FLOOR = '666SOLCARREAUX1'
CEIL = '666MURGRISEE'
CRATE = '666CAISSE'
METAL = '666MURMETAL'
TRIGGER = 'AAATRIGGER'
ORIGIN = 'ORIGIN'

T = 16          # wall thickness
GAP = 64        # space between two rooms (outside the map)


def box(x0, y0, z0, x1, y1, z1, tex):
    """One axis-aligned box brush (Valve 220), the same texture on every side."""
    assert x0 < x1 and y0 < y1 and z0 < z1, (x0, y0, z0, x1, y1, z1)
    ax_x = '[ 0 1 0 0 ] [ 0 0 -1 0 ] 0 1 1'
    ax_y = '[ 1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1'
    ax_z = '[ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1'
    faces = [
        ((x0, y0, z1), (x0, y0, z0), (x0, y1, z1), ax_x),
        ((x1, y1, z1), (x1, y1, z0), (x1, y0, z1), ax_x),
        ((x1, y1, z1), (x0, y1, z1), (x1, y1, z0), ax_y),
        ((x1, y0, z0), (x0, y0, z0), (x1, y0, z1), ax_y),
        ((x1, y0, z1), (x0, y0, z1), (x1, y1, z1), ax_z),
        ((x1, y1, z0), (x0, y1, z0), (x1, y0, z0), ax_z),
    ]
    lines = ['{']
    for a, b, c, axes in faces:
        lines.append('( %g %g %g ) ( %g %g %g ) ( %g %g %g ) %s %s' % (a + b + c + (tex, axes)))
    lines.append('}')
    return '\n'.join(lines)


def shell(x0, y0, z0, x1, y1, z1):
    """The six walls of a sealed room whose inside is (x0, y0, z0) - (x1, y1, z1)."""
    return [
        box(x0 - T, y0 - T, z0 - T, x1 + T, y1 + T, z0, FLOOR),
        box(x0 - T, y0 - T, z1, x1 + T, y1 + T, z1 + T, CEIL),
        box(x0 - T, y0 - T, z0, x0, y1 + T, z1, WALL),
        box(x1, y0 - T, z0, x1 + T, y1 + T, z1, WALL),
        box(x0, y0 - T, z0, x1, y0, z1, WALL),
        box(x0, y1, z0, x1, y1 + T, z1, WALL),
    ]


def kv_text(kv):
    return ''.join('"%s" "%s"\n' % (k, str(v).replace('"', "'")) for k, v in kv.items())


class MapWriter:
    def __init__(self, wad, world_keys=None):
        self.world = []
        self.ents = []
        self.wad = wad
        self.world_keys = dict(world_keys or {})

    def add_world(self, brushes):
        self.world += brushes

    def add_ent(self, kv, brushes=()):
        self.ents.append((dict(kv), list(brushes)))

    def text(self):
        out = ['{\n"classname" "worldspawn"\n"mapversion" "220"\n"wad" "%s"\n"MaxRange" "8192"\n' % self.wad]
        if self.world_keys:
            out.append(kv_text(self.world_keys))
        out += [b + '\n' for b in self.world]
        out.append('}\n')
        for kv, brushes in self.ents:
            out.append('{\n' + kv_text(kv))
            out += [b + '\n' for b in brushes]
            out.append('}\n')
        return ''.join(out)


def fmt_vec(v):
    return ' '.join('%g' % c for c in v)
