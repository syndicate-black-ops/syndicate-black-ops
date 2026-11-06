#!/usr/bin/env python3
"""GoldSrc studio model (MDL v10) reader used by scripts/modelsrc.py.

Python 3 standard library only. Reads a main model (IDST), its optional
external texture file (<name>t.mdl) and its optional sequence-group files
(<name>01.mdl ...; IDSQ) and returns one combined description, which is what
the engine sees once every file is loaded.

Layouts follow the Half-Life SDK's engine/studio.h (studiohdr_t and friends).
"""

import hashlib
import os
import struct

STUDIO_VERSION = 10

# studio.h limits of Valve's SDK (engine/studio.h)
LIMITS = {
    "MAXSTUDIOTRIANGLES": 20000,
    "MAXSTUDIOVERTS": 2048,
    "MAXSTUDIOSEQUENCES": 2048,
    "MAXSTUDIOSKINS": 100,
    "MAXSTUDIOSRCBONES": 512,
    "MAXSTUDIOBONES": 128,
    "MAXSTUDIOMODELS": 32,
    "MAXSTUDIOBODYPARTS": 32,
    "MAXSTUDIOGROUPS": 16,
    "MAXSTUDIOANIMATIONS": 2048,
    "MAXSTUDIOMESHES": 256,
    "MAXSTUDIOEVENTS": 1024,
    "MAXSTUDIOPIVOTS": 256,
    "MAXSTUDIOCONTROLLERS": 8,
}

MOTION_BITS = [
    (0x0001, "X"), (0x0002, "Y"), (0x0004, "Z"),
    (0x0008, "XR"), (0x0010, "YR"), (0x0020, "ZR"),
    (0x0040, "LX"), (0x0080, "LY"), (0x0100, "LZ"),
    (0x0200, "AX"), (0x0400, "AY"), (0x0800, "AZ"),
    (0x1000, "AXR"), (0x2000, "AYR"), (0x4000, "AZR"),
]

TEXTURE_FLAGS = [
    (0x0001, "flatshade"), (0x0002, "chrome"), (0x0004, "fullbright"),
    (0x0008, "nomips"), (0x0010, "alpha"), (0x0020, "additive"),
    (0x0040, "masked"),
]


class MdlError(Exception):
    pass


def cstr(raw):
    """Fixed-size char field -> str. Latin-1 keeps every byte (French names)."""
    return raw.split(b"\0", 1)[0].decode("latin-1")


def motion_names(bits):
    return [n for b, n in MOTION_BITS if bits & b]


def texture_flag_names(bits):
    names = [n for b, n in TEXTURE_FLAGS if bits & b]
    rest = bits & ~0x7F
    if rest:
        names.append("0x%x" % rest)
    return names


HDR_FMT = "<4si64si3f3f3f3f3fi" + "i" * 26
HDR_SIZE = struct.calcsize(HDR_FMT)  # 244


def read_header(data):
    if len(data) < HDR_SIZE:
        raise MdlError("file shorter than a studio header")
    f = struct.unpack_from(HDR_FMT, data, 0)
    keys = ["numbones", "boneindex", "numbonecontrollers", "bonecontrollerindex",
            "numhitboxes", "hitboxindex", "numseq", "seqindex", "numseqgroups",
            "seqgroupindex", "numtextures", "textureindex", "texturedataindex",
            "numskinref", "numskinfamilies", "skinindex", "numbodyparts",
            "bodypartindex", "numattachments", "attachmentindex", "soundtable",
            "soundindex", "soundgroups", "soundgroupindex", "numtransitions",
            "transitionindex"]
    h = {"id": f[0], "version": f[1], "name": cstr(f[2]), "length": f[3],
         "eyeposition": f[4:7], "min": f[7:10], "max": f[10:13],
         "bbmin": f[13:16], "bbmax": f[16:19], "flags": f[19]}
    h.update(zip(keys, f[20:]))
    return h


def _anim_value(data, base, frame):
    """Port of the engine's CalcBoneValue walk: raw short for `frame`."""
    pos = base
    k = frame
    valid, total = data[pos], data[pos + 1]
    while total <= k:
        k -= total
        pos += (valid + 1) * 2
        valid, total = data[pos], data[pos + 1]
        if total == 0:
            raise MdlError("corrupt animation run (total 0)")
    if valid > k:
        return struct.unpack_from("<h", data, pos + (k + 1) * 2)[0]
    return struct.unpack_from("<h", data, pos + valid * 2)[0]


def _decode_channel(data, base, numframes):
    """All raw shorts of one compressed channel (faster than per-frame walks)."""
    out = []
    pos = base
    while len(out) < numframes:
        valid, total = data[pos], data[pos + 1]
        if total == 0:
            raise MdlError("corrupt animation run (total 0)")
        vals = struct.unpack_from("<%dh" % valid, data, pos + 2) if valid else (0,)
        for i in range(total):
            out.append(vals[i] if i < valid else vals[-1])
        pos += (valid + 1) * 2
    return out[:numframes]


def _read_tris(data, off, numtris_hint):
    """Decode a mesh's strip/fan command list into triangles of 4-tuples."""
    tris = []
    pos = off
    cmds = 0
    while True:
        n = struct.unpack_from("<h", data, pos)[0]
        pos += 2
        if n == 0:
            break
        cmds += 1
        fan = n < 0
        n = abs(n)
        verts = [struct.unpack_from("<4h", data, pos + 8 * i) for i in range(n)]
        pos += 8 * n
        for i in range(2, n):
            if fan:
                tris.append((verts[0], verts[i - 1], verts[i]))
            elif i % 2 == 0:
                tris.append((verts[i - 2], verts[i - 1], verts[i]))
            else:
                tris.append((verts[i - 1], verts[i - 2], verts[i]))
    return tris, cmds


class Model:
    """Combined view of <name>.mdl + <name>t.mdl + <name>NN.mdl."""

    def __init__(self, path, decode_anims=True, texture_data=None):
        """texture_data: bytes of a texture file to use when the model has no textures
        inside and no <name>t.mdl beside it (stand-in textures, see modelsrc.py)."""
        self.path = path
        self.texture_override = texture_data
        self.texture_blob = None
        self.placeholder_textures = False
        self.files = [path]
        self.notes = []
        with open(path, "rb") as fh:
            self.data = fh.read()
        if self.data[:4] != b"IDST":
            raise MdlError("%s: not an IDST studio model (%r)" % (path, self.data[:4]))
        self.hdr = h = read_header(self.data)
        if h["version"] != STUDIO_VERSION:
            raise MdlError("%s: studio version %d" % (path, h["version"]))
        if h["length"] != len(self.data):
            self.notes.append("header length %d != file size %d" % (h["length"], len(self.data)))
        self.size = len(self.data)
        self.external_textures = False
        self.texture_file = None
        self.seqgroup_files = []
        self._read_bones()
        self._read_misc()
        self._read_textures()
        self._read_bodyparts()
        self._read_sequences(decode_anims)

    # -- pieces -----------------------------------------------------------
    def _read_bones(self):
        d, h = self.data, self.hdr
        self.bones = []
        for i in range(h["numbones"]):
            f = struct.unpack_from("<32sii6i6f6f", d, h["boneindex"] + 112 * i)
            self.bones.append({"name": cstr(f[0]), "parent": f[1], "flags": f[2],
                               "controller": f[3:9], "value": f[9:15], "scale": f[15:21]})
        self.controllers = []
        for i in range(h["numbonecontrollers"]):
            f = struct.unpack_from("<iiffii", d, h["bonecontrollerindex"] + 24 * i)
            self.controllers.append({"bone": f[0], "type": f[1], "start": f[2],
                                     "end": f[3], "rest": f[4], "index": f[5]})

    def _read_misc(self):
        d, h = self.data, self.hdr
        self.hitboxes = []
        for i in range(h["numhitboxes"]):
            f = struct.unpack_from("<ii3f3f", d, h["hitboxindex"] + 32 * i)
            self.hitboxes.append({"bone": f[0], "group": f[1], "bbmin": f[2:5], "bbmax": f[5:8]})
        self.attachments = []
        for i in range(h["numattachments"]):
            f = struct.unpack_from("<32sii3f9f", d, h["attachmentindex"] + 88 * i)
            self.attachments.append({"name": cstr(f[0]), "type": f[1], "bone": f[2], "org": f[3:6]})
        self.transitions = b""
        if h["numtransitions"]:
            n = h["numtransitions"]
            self.transitions = d[h["transitionindex"]:h["transitionindex"] + n * n]

    def _companion(self, suffix):
        stem = self.path[:-4]
        folder = os.path.dirname(self.path) or "."
        want = (os.path.basename(stem) + suffix).lower()
        for name in sorted(os.listdir(folder)):
            if name.lower() == want:
                return os.path.join(folder, name)
        return None

    def _read_textures(self):
        h = self.hdr
        td, th = self.data, h
        if h["numtextures"] == 0 and h["textureindex"] == 0:
            tpath = self._companion("t.mdl")
            if tpath:
                self.external_textures = True
                self.texture_file = tpath
                self.files.append(tpath)
                with open(tpath, "rb") as fh:
                    td = fh.read()
                if td[:4] != b"IDST":
                    raise MdlError("%s: texture file is not IDST" % tpath)
                th = read_header(td)
                if th["length"] != len(td):
                    self.notes.append("texture file header length %d != file size %d" % (th["length"], len(td)))
                self.texture_blob = td
            elif self.texture_override is not None:
                td = self.texture_override
                th = read_header(td)
                self.texture_blob = td
                self.placeholder_textures = True
        self.texture_hdr = th
        self.textures = []
        for i in range(th["numtextures"]):
            f = struct.unpack_from("<64siiii", td, th["textureindex"] + 80 * i)
            w, hgt, idx = f[2], f[3], f[4]
            pix = td[idx:idx + w * hgt]
            pal = td[idx + w * hgt:idx + w * hgt + 768]
            self.textures.append({"name": cstr(f[0]), "flags": f[1], "width": w, "height": hgt,
                                  "sha": hashlib.sha1(pix + pal).hexdigest()[:16],
                                  "pixels": pix, "palette": pal})
        self.numskinref = th["numskinref"]
        self.numskinfamilies = th["numskinfamilies"]
        n = self.numskinref * self.numskinfamilies
        self.skins = list(struct.unpack_from("<%dh" % n, td, th["skinindex"])) if n else []

    def _read_bodyparts(self):
        d, h = self.data, self.hdr
        self.bodyparts = []
        for i in range(h["numbodyparts"]):
            f = struct.unpack_from("<64siii", d, h["bodypartindex"] + 76 * i)
            bp = {"name": cstr(f[0]), "nummodels": f[1], "base": f[2], "models": []}
            for j in range(f[1]):
                m = struct.unpack_from("<64sif10i", d, f[3] + 112 * j)
                (name, _type, _radius, nummesh, meshindex, numverts, vertinfoindex,
                 vertindex, numnorms, norminfoindex, normindex, _ng, _gi) = m
                vbone = d[vertinfoindex:vertinfoindex + numverts]
                nbone = d[norminfoindex:norminfoindex + numnorms]
                verts = [struct.unpack_from("<3f", d, vertindex + 12 * k) for k in range(numverts)]
                norms = [struct.unpack_from("<3f", d, normindex + 12 * k) for k in range(numnorms)]
                meshes = []
                for k in range(nummesh):
                    numtris, triindex, skinref, mnorms, _ni = struct.unpack_from("<5i", d, meshindex + 20 * k)
                    tris, cmds = _read_tris(d, triindex, numtris)
                    meshes.append({"numtris": numtris, "skinref": skinref, "numnorms": mnorms,
                                   "tris": tris, "cmds": cmds})
                bp["models"].append({"name": cstr(name), "numverts": numverts, "numnorms": numnorms,
                                     "vbone": vbone, "nbone": nbone, "verts": verts, "norms": norms,
                                     "meshes": meshes,
                                     "numtris": sum(len(x["tris"]) for x in meshes)})
            self.bodyparts.append(bp)

    def _read_sequences(self, decode_anims):
        d, h = self.data, self.hdr
        self.seqgroups = []
        for i in range(h["numseqgroups"]):
            f = struct.unpack_from("<32s64sii", d, h["seqgroupindex"] + 104 * i)
            self.seqgroups.append({"label": cstr(f[0]), "name": cstr(f[1])})
        group_data = {0: d}
        for i in range(1, h["numseqgroups"]):
            gpath = self._companion("%02d.mdl" % i)
            if not gpath:
                self.notes.append("sequence group %d file is missing" % i)
                continue
            with open(gpath, "rb") as fh:
                gd = fh.read()
            if gd[:4] != b"IDSQ":
                raise MdlError("%s: sequence group file is not IDSQ" % gpath)
            group_data[i] = gd
            self.files.append(gpath)
            self.seqgroup_files.append(gpath)
            stored = cstr(gd[8:72])
            self.seqgroups[i]["file_internal_name"] = stored
        self.sequences = []
        nb = h["numbones"]
        for i in range(h["numseq"]):
            f = struct.unpack_from("<32sf10i3fii3f3fii2i2f2fi5i", d, h["seqindex"] + 176 * i)
            s = {"label": cstr(f[0]), "fps": f[1], "flags": f[2], "activity": f[3],
                 "actweight": f[4], "numevents": f[5], "eventindex": f[6], "numframes": f[7],
                 "numpivots": f[8], "motiontype": f[10], "motionbone": f[11],
                 "linearmovement": f[12:15], "bbmin": f[17:20], "bbmax": f[20:23],
                 "numblends": f[23], "animindex": f[24], "blendtype": f[25:27],
                 "blendstart": f[27:29], "blendend": f[29:31], "blendparent": f[31],
                 "seqgroup": f[32], "entrynode": f[33], "exitnode": f[34],
                 "nodeflags": f[35], "nextseq": f[36]}
            s["events"] = []
            for e in range(s["numevents"]):
                ev = struct.unpack_from("<iii64s", d, s["eventindex"] + 76 * e)
                s["events"].append({"frame": ev[0], "event": ev[1], "type": ev[2], "options": cstr(ev[3])})
            s["anim"] = None
            gd = group_data.get(s["seqgroup"])
            if decode_anims and gd is not None:
                try:
                    s["anim"] = self._decode_seq(gd, s, nb)
                except (MdlError, struct.error, IndexError) as exc:
                    self.notes.append("sequence %s: animation data unreadable (%s)" % (s["label"], exc))
            self.sequences.append(s)

    def _decode_seq(self, gd, s, nb):
        """-> [blend][bone] = list of 6 channels, each a list of floats per frame."""
        out = []
        nf = s["numframes"]
        for b in range(s["numblends"]):
            bones = []
            for j in range(nb):
                aoff = s["animindex"] + (b * nb + j) * 12
                offs = struct.unpack_from("<6H", gd, aoff)
                bone = self.bones[j]
                chans = []
                for k in range(6):
                    if offs[k] == 0:
                        chans.append(None)  # constant: the bone's default value
                    else:
                        raw = _decode_channel(gd, aoff + offs[k], nf)
                        chans.append([bone["value"][k] + r * bone["scale"][k] for r in raw])
                bones.append(chans)
            out.append(bones)
        return out

    # -- derived ----------------------------------------------------------
    def total_size(self):
        return sum(os.path.getsize(p) for p in self.files)

    def bones_with_geometry(self):
        used = set()
        for bp in self.bodyparts:
            for m in bp["models"]:
                used.update(m["vbone"])
                used.update(m["nbone"])
        return used

    def unreferenced_bones(self):
        """Bones a compiler would drop: no vertex/normal on them or below them,
        and no attachment/controller/hitbox that pins them."""
        used = set(self.bones_with_geometry())
        used.update(a["bone"] for a in self.attachments)
        keep = set()
        for b in used:
            while b != -1 and b not in keep:
                keep.add(b)
                b = self.bones[b]["parent"]
        return [i for i in range(len(self.bones)) if i not in keep]
