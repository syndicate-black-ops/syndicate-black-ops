#!/usr/bin/env python3
"""Append lumps from one WAD3 file to another, keeping the target's existing lumps, their order and their data.

    scripts/wad_append.py <target.wad> <source.wad> <lump name>... [--check]

Decal WADs: the engine numbers decals by their position in decals.wad and the server sends those numbers, so
existing entries must never move; new ones only go at the end. The target is rewritten in place: its bytes up to its
directory stay as they are, the new lumps' data follow, then the directory (old entries unchanged, new ones last).

--check: only list what would be appended. Lump names are matched case-insensitively; a name the target already
has is refused (exit 1), as is a name the source lacks. Standard library only.
"""
import struct
import sys


def read_wad(path):
    data = open(path, 'rb').read()
    if len(data) < 12 or data[:4] not in (b'WAD3', b'WAD2'):
        sys.exit('%s: not a WAD2/WAD3 file' % path)
    magic, count, diroffset = struct.unpack_from('<4sii', data, 0)
    if count < 0 or diroffset < 12 or diroffset + count * 32 > len(data):
        sys.exit('%s: bad directory (count %d, offset %d, size %d)' % (path, count, diroffset, len(data)))
    entries = []
    for i in range(count):
        filepos, disksize, size, typ, comp, pad1, pad2 = struct.unpack_from('<iiibbbb', data, diroffset + 32 * i)
        name = data[diroffset + 32 * i + 16:diroffset + 32 * i + 32]
        if filepos < 0 or disksize < 0 or filepos + disksize > len(data):
            sys.exit('%s: lump %d out of the file' % (path, i))
        entries.append({'filepos': filepos, 'disksize': disksize, 'size': size, 'type': typ, 'comp': comp,
                        'raw_name': name, 'name': name.split(b'\0')[0].decode('latin-1')})
    return data, magic, diroffset, entries


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    check = '--check' in sys.argv
    if len(args) < 3:
        sys.exit(__doc__)
    target, source, names = args[0], args[1], args[2:]

    tdata, tmagic, tdir, tentries = read_wad(target)
    sdata, _, _, sentries = read_wad(source)
    have = {e['name'].lower() for e in tentries}
    by_name = {e['name'].lower(): e for e in sentries}

    new = []
    for n in names:
        if n.lower() in have:
            sys.exit('%s already has %s: refused (existing decals keep their place)' % (target, n))
        if n.lower() not in by_name:
            sys.exit('%s has no lump %s' % (source, n))
        if any(x['name'].lower() == n.lower() for x in new):
            sys.exit('%s given twice' % n)
        new.append(by_name[n.lower()])

    print('%s: %d lumps; appending %d: %s' % (target, len(tentries), len(new), ' '.join(e['name'] for e in new)))
    if check:
        return

    # keep everything before the directory if the directory is the file's tail, else the whole file
    end_of_dir = tdir + 32 * len(tentries)
    body = tdata[:tdir] if end_of_dir == len(tdata) else tdata
    out = bytearray(body)
    added = []
    for e in new:
        lump = sdata[e['filepos']:e['filepos'] + e['disksize']]
        while len(out) % 4:
            out += b'\0'
        added.append(dict(e, filepos=len(out)))
        out += lump
    while len(out) % 4:
        out += b'\0'
    diroffset = len(out)
    for e in tentries + added:
        out += struct.pack('<iiibbbb', e['filepos'], e['disksize'], e['size'], e['type'], e['comp'], 0, 0)
        out += e['raw_name'][:16].ljust(16, b'\0')
    struct.pack_into('<4sii', out, 0, tmagic, len(tentries) + len(added), diroffset)
    open(target, 'wb').write(bytes(out))

    # verify: the old entries are unchanged and in place, the new ones read back
    data, _, _, entries = read_wad(target)
    for i, e in enumerate(tentries):
        f = entries[i]
        assert f['name'] == e['name'] and tdata[e['filepos']:e['filepos'] + e['disksize']] == \
            data[f['filepos']:f['filepos'] + f['disksize']], e['name']
    for i, e in enumerate(new):
        f = entries[len(tentries) + i]
        assert f['name'] == e['name'] and sdata[e['filepos']:e['filepos'] + e['disksize']] == \
            data[f['filepos']:f['filepos'] + f['disksize']], e['name']
    print('%s: %d lumps, the first %d unchanged' % (target, len(entries), len(tentries)))


if __name__ == '__main__':
    main()
