#!/usr/bin/env python3
"""SBO model source pipeline: shipped MDL -> sdk/modelsrc/ -> rebuilt MDL -> comparison.

    scripts/modelsrc.py build   [model ...]   decompile, normalize, write the QC files
    scripts/modelsrc.py verify  [model ...]   recompile into scratch and compare with the shipped model
    scripts/modelsrc.py readme                regenerate sdk/modelsrc/README.md (inventory + results)
    scripts/modelsrc.py all                   build + verify + readme
    scripts/modelsrc.py install [model ...]   compile the sources and put the models into gamedir/
    scripts/modelsrc.py package <folder>      copy the complete models folder to <folder>/syndicate/models

Once a model is installed, the file in gamedir/ is the compiler's output and the sources are
the master: build leaves that model alone, verify checks that the sources still compile to
exactly the installed file (sdk/modelsrc/installed.json records what was installed).

Python 3 standard library only. External tools (never modified, never built here):

    MDLDEC     mdldec decompiler      (default: mdldec on PATH)
    STUDIOMDL  studiomdlPlusPlus      (default: studiomdlplusplus on PATH)

Scratch files (decompiler output, rebuilt models, logs, report.json) go to
build/modelsrc/ (git-ignored), or $MODELSRC_SCRATCH. Only `install` writes under
gamedir/. The special cases live in scripts/modelsrc_rules.json. Everything runs
sequentially (one tool process at a time).

Same input, same output: no timestamps, sorted traversal, fixed number format.
"""

import argparse
import collections
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import modelsrc_mdl as mdl  # noqa: E402

HOME = os.path.expanduser("~")
MDLDEC = os.environ.get("MDLDEC", shutil.which("mdldec") or "mdldec")
STUDIOMDL = os.environ.get("STUDIOMDL", shutil.which("studiomdlplusplus") or "studiomdlplusplus")
SCRATCH = os.environ.get("MODELSRC_SCRATCH", os.path.join(REPO, "build", "modelsrc"))
RULES_PATH = os.path.join(HERE, "modelsrc_rules.json")
ACTIVITY_H = os.path.join(REPO, "src", "dlls", "activity.h")

# Tolerances of the comparison (see README "How the comparison works").
TOL_POS = 0.02        # units: animation positions, vertex positions
TOL_ROT = 0.002       # radians: animation rotations
TOL_BOX = 0.01        # units: hitboxes, bbox/cbox, attachments, eye position
TOL_SEQBOX = 0.5      # units: per-sequence bounding box (recomputed by the compiler)
TOL_NORMAL = 0.02     # per component of a unit normal
TOL_MOVE = 0.02       # units: linear movement of a sequence


def die(msg):
    sys.stderr.write("modelsrc: " + msg + "\n")
    sys.exit(1)


def load_rules():
    with open(RULES_PATH, encoding="utf-8") as fh:
        return json.load(fh)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def discover(rules):
    """Every .mdl under models_dir, grouped. Returns (models, orphans).

    models:  key -> {"path", "texture_file", "seq_files"}  (key = lower-case path without .mdl)
    orphans: [(relative path, reason)] for companion-looking files no model header refers to
    """
    root = os.path.join(REPO, rules["models_dir"])
    found = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.lower().endswith(".mdl"):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                with open(full, "rb") as fh:
                    data = fh.read()
                found[rel[:-4].lower()] = (full, rel, data)
    for key, (full, rel, data) in found.items():
        if data[:4] not in (b"IDST", b"IDSQ"):
            die("%s is not a studio model (Git LFS pointer? first bytes %r)" % (rel, data[:16]))

    models, claimed = {}, set()
    texture_only = set()
    for key, (full, rel, data) in found.items():
        if data[:4] == b"IDSQ":
            continue
        hdr = mdl.read_header(data)
        if hdr["numbodyparts"] == 0 and hdr["numbones"] == 0 and hdr["numseq"] == 0:
            texture_only.add(key)
            continue
        models[key] = {"path": full, "rel": rel, "hdr": hdr, "texture_file": None, "seq_files": []}
    for key, m in models.items():
        hdr = m["hdr"]
        if hdr["numtextures"] == 0 and hdr["textureindex"] == 0 and key + "t" in found:
            m["texture_file"] = found[key + "t"][1]
            claimed.add(key + "t")
        for i in range(1, hdr["numseqgroups"]):
            ck = "%s%02d" % (key, i)
            if ck in found:
                m["seq_files"].append(found[ck][1])
                claimed.add(ck)
    orphans = []
    for key, (full, rel, data) in sorted(found.items()):
        if key in models or key in claimed:
            continue
        if data[:4] == b"IDSQ":
            base = key[:-2]
            why = "sequence-group file (IDSQ) that %s.mdl's header does not use" % base
            if base in models and models[base]["hdr"]["numseqgroups"] == 1:
                why += " (%s.mdl has no sequence groups)" % base
        elif key in texture_only:
            if key in rules["models"] and "skip" in rules["models"][key]:
                models[key] = {"path": full, "rel": rel, "hdr": mdl.read_header(data),
                               "texture_file": None, "seq_files": []}
                continue
            why = "texture-only file that no model uses"
        else:
            why = "unclassified"
        orphans.append((rel, why))
    return dict(sorted(models.items())), orphans


def folder_for(key, rules):
    if key in rules["folders"]:
        return rules["folders"][key]
    m = re.match(r"^player/([^/]+)/\1$", key)
    if m:
        return "players/" + m.group(1)
    m = re.match(r"^([vwp])_([a-z0-9_]+)$", key)
    if m:
        return "weapons/%s/%s_" % (m.group(2), m.group(1))
    die("no folder rule for model '%s' (add it to 'folders' in %s)" % (key, RULES_PATH))


# ---------------------------------------------------------------------------
# Names
# ---------------------------------------------------------------------------

def deaccent(s):
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")


def file_stem(name):
    """A source file stem from a name stored in the MDL: lower case, no accents, no folders."""
    s = deaccent(name).lower().replace("\\", "/").split("/")[-1].strip()
    return re.sub(r'[\x00-\x1f"<>:|?*]', "_", s) or "unnamed"


def unique(stems, limit):
    """Make a list of stems unique with _2, _3 ... suffixes, each at most `limit` bytes."""
    out, seen = [], set()
    for s in stems:
        cand, n = s[:limit], 1
        while cand in seen:
            n += 1
            suffix = "_%d" % n
            cand = s[:limit - len(suffix)] + suffix
        seen.add(cand)
        out.append(cand)
    return out


# --- stand-in textures -------------------------------------------------------

PLACEHOLDER_NAME = "dev_placeholder"
PLACEHOLDER_PALETTE = [(64, 64, 64), (112, 112, 112), (255, 128, 0), (255, 0, 255)]
PLACEHOLDER_FONT = {  # 5x7 glyphs, one string per row
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
}


def placeholder_image(w, h):
    """An unmistakable stand-in texture: gray checker, magenta frame, the word DEV in
    orange, repeated. Drawn here (not with an external tool) so that every run gives the
    same bytes. Returns (pixels top-down, 768-byte palette)."""
    pix = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            pix[y * w + x] = ((x // 8) + (y // 8)) & 1
    for x in range(w):
        pix[x] = pix[(h - 1) * w + x] = 3
    for y in range(h):
        pix[y * w] = pix[y * w + w - 1] = 3
    scale = 2 if h >= 32 and w >= 48 else 1
    tw, th = (3 * 5 + 2) * scale, 7 * scale
    if h >= th + 1 and w >= tw + 2:
        for oy in range(max(1, min(4, h - th)), h - th + 1, th + 10 * scale):
            for ox in range(4 if w >= tw + 8 else 1, w - tw, tw + 8 * scale):
                for li, ch in enumerate("DEV"):
                    for gy, row in enumerate(PLACEHOLDER_FONT[ch]):
                        for gx, bit in enumerate(row):
                            if bit == "#":
                                for sy in range(scale):
                                    for sx in range(scale):
                                        px = ox + (li * 6 + gx) * scale + sx
                                        py = oy + gy * scale + sy
                                        if px < w and py < h:
                                            pix[py * w + px] = 2
    pal = bytearray(768)
    for i, rgb in enumerate(PLACEHOLDER_PALETTE):
        pal[i * 3:i * 3 + 3] = bytes(rgb)
    return bytes(pix), bytes(pal)


def pow2_cover(n):
    """Smallest power of two (at least 8) that is greater than the largest texel index n."""
    size = 8
    while size <= n:
        size *= 2
    return size


def placeholder_texture_file(model):
    """A texture file (the layout of <name>t.mdl) with one stand-in texture per skin
    reference of a model that ships without its textures. The original names and sizes
    are unknown; each stand-in is the smallest power-of-two size that covers the texel
    coordinates stored in the meshes that use it (power-of-two sizes are never resampled
    by the engine)."""
    extent = {}
    for bp in model.bodyparts:
        for sm in bp["models"]:
            for mesh in sm["meshes"]:
                e = extent.setdefault(mesh["skinref"], [0, 0])
                for tri in mesh["tris"]:
                    for _v, _n, s_, t_ in tri:
                        e[0] = max(e[0], s_)
                        e[1] = max(e[1], t_)
    count = max(extent) + 1
    images, names = [], []
    for i in range(count):
        e = extent.get(i, [0, 0])
        w, h = pow2_cover(e[0]), pow2_cover(e[1])
        images.append((w, h) + placeholder_image(w, h))
        names.append(PLACEHOLDER_NAME + (".bmp" if i == 0 else "_%d.bmp" % (i + 1)))
    textureindex = mdl.HDR_SIZE
    skinindex = textureindex + 80 * count
    dataindex = (skinindex + 2 * count + 3) & ~3
    entries, data, off = b"", b"", dataindex
    for name, (w, h, pix, pal) in zip(names, images):
        entries += struct.pack("<64siiii", name.encode(), 0, w, h, off)
        data += pix + pal
        off += len(pix) + len(pal)
    skins = struct.pack("<%dh" % count, *range(count))
    skins += b"\0" * (dataindex - skinindex - len(skins))
    body = entries + skins + data
    fields = [b"IDST", mdl.STUDIO_VERSION, b"", mdl.HDR_SIZE + len(body)] + [0.0] * 15 + [0] + [0] * 26
    keys = ["numbones", "boneindex", "numbonecontrollers", "bonecontrollerindex",
            "numhitboxes", "hitboxindex", "numseq", "seqindex", "numseqgroups",
            "seqgroupindex", "numtextures", "textureindex", "texturedataindex",
            "numskinref", "numskinfamilies", "skinindex", "numbodyparts",
            "bodypartindex", "numattachments", "attachmentindex", "soundtable",
            "soundindex", "soundgroups", "soundgroupindex", "numtransitions",
            "transitionindex"]
    values = dict(numtextures=count, textureindex=textureindex, texturedataindex=dataindex,
                  numskinref=count, numskinfamilies=1, skinindex=skinindex)
    for i, k in enumerate(keys):
        fields[20 + i] = values.get(k, 0)
    return struct.pack(mdl.HDR_FMT, *fields) + body


class Plan:
    """Everything decided for one model before any tool runs."""

    def __init__(self, key, entry, rules):
        self.key = key
        self.entry = entry
        self.name = key.split("/")[-1]
        self.rule = rules["models"].get(key, {})
        self.skip = self.rule.get("skip")
        self.folder = None if self.skip else folder_for(key, rules)
        self.model = None
        self.error = None
        if self.skip:
            return
        try:
            blob = None
            if self.rule.get("placeholder_textures"):
                bare = mdl.Model(entry["path"], decode_anims=False)
                if not (bare.textures or bare.hdr["textureindex"]):
                    blob = placeholder_texture_file(bare)
                # else: the installed build, which has the stand-in textures inside
            self.model = mdl.Model(entry["path"], texture_data=blob)
        except (mdl.MdlError, struct.error, IndexError) as exc:
            self.error = "cannot read: %s" % exc
            return
        m = self.model
        # textures
        generic = set(rules["generic_texture_names"])
        explicit = rules["texture_renames"].get(key, {})
        self.texture_names = []
        for t in m.textures:
            new = deaccent(t["name"]).lower()
            if t["name"] in explicit:
                new = explicit[t["name"]]
            elif new in generic:
                new = "%s-%s" % (self.name, new)
            self.texture_names.append(new)
        if len(set(self.texture_names)) != len(self.texture_names):
            self.error = "texture names collide after renaming: %r" % self.texture_names
        if any(len(n.encode()) > 63 for n in self.texture_names):
            self.error = "texture name longer than 63 bytes"
        # sub-models
        stems, self.blank = [], []
        for bp in m.bodyparts:
            for sm in bp["models"]:
                is_blank = len(sm["meshes"]) == 0 and sm["numverts"] == 0
                self.blank.append(is_blank)
                stems.append(None if is_blank else file_stem(sm["name"]))
        real = unique([s for s in stems if s is not None], 63)
        it = iter(real)
        self.submodel_stems = [None if s is None else next(it) for s in stems]
        if any(s is not None and s.startswith("blank") for s in self.submodel_stems):
            self.error = "a sub-model with geometry is named blank*: mdldec would skip it"
        # sequences: one file per blend; names must not collide with each other
        seq_stems = unique([file_stem(s["label"]) for s in m.sequences], 31 - 8)
        self.seq_stems = seq_stems
        files = []
        for s, stem in zip(m.sequences, seq_stems):
            files += self.seq_files(s, stem)
        if len(set(files)) != len(files):
            self.error = "sequence file names collide: %r" % sorted(
                f for f, c in collections.Counter(files).items() if c > 1)

    @staticmethod
    def seq_files(seq, stem):
        if seq["numblends"] == 1:
            return [stem]
        return ["%s_blend%02d" % (stem, b + 1) for b in range(seq["numblends"])]

    def add_idle(self):
        """Rule add_idle_sequence, only meaningful for a model that ships with no sequence."""
        return bool(self.rule.get("add_idle_sequence")) and not self.model.sequences

    def needs_keepallbones(self):
        """$keepallbones is only written when the rules table asks for it. The compiler keeps
        every bone that carries geometry or an attachment (and its parents) in the order of
        the reference meshes, which is the shipped bone table for every model so far; verify
        reports a changed bone table if a model ever needs the rule."""
        return bool(self.rule.get("keepallbones"))


# ---------------------------------------------------------------------------
# Decompile
# ---------------------------------------------------------------------------

def patch_name(buf, offset, size, text):
    raw = text.encode("ascii")
    if len(raw) >= size:
        raise ValueError("name too long for the MDL field: %s" % text)
    buf[offset:offset + size] = raw + b"\0" * (size - len(raw))


def staged_copy(plan, indir):
    """Copy the model and its companion files to scratch with neutral names patched in
    (tex000.bmp, ref000, seq000). mdldec names its output files after the names stored in
    the model, replaces names it does not like and overwrites files when two names are the
    same; neutral names avoid all of that. assemble() gives every file its final name.
    Only the scratch copy is patched."""
    m = plan.model
    name = plan.name
    main = bytearray(m.data)
    h = m.hdr
    k = 0
    for i in range(h["numbodyparts"]):
        nummodels, _base, modelindex = struct.unpack_from("<iii", main, h["bodypartindex"] + 76 * i + 64)
        for j in range(nummodels):
            if plan.submodel_stems[k] is not None:
                patch_name(main, modelindex + 112 * j, 64, "ref%03d" % k)
            k += 1
    for i in range(len(plan.seq_stems)):
        patch_name(main, h["seqindex"] + 176 * i, 32, "seq%03d" % i)
    tex = main
    if m.texture_blob is not None:
        tex = bytearray(m.texture_blob)
    th = mdl.read_header(bytes(tex))
    for i in range(len(plan.texture_names)):
        patch_name(tex, th["textureindex"] + 80 * i, 64, "tex%03d.bmp" % i)
    os.makedirs(indir)
    with open(os.path.join(indir, name + ".mdl"), "wb") as fh:
        fh.write(main)
    if m.texture_blob is not None:
        with open(os.path.join(indir, name + "t.mdl"), "wb") as fh:
            fh.write(tex)
    for i, src in enumerate(m.seqgroup_files, 1):
        shutil.copyfile(src, os.path.join(indir, "%s%02d.mdl" % (name, i)))


def read_bmp(path):
    with open(path, "rb") as fh:
        d = fh.read()
    if d[:2] != b"BM":
        raise ValueError("not a BMP")
    off = struct.unpack_from("<I", d, 10)[0]
    hsize, w, h, _planes, bpp = struct.unpack_from("<IiiHH", d, 14)
    ncol = struct.unpack_from("<I", d, 46)[0] or 256
    if bpp != 8:
        raise ValueError("not 8-bit")
    pal = b"".join(bytes((d[14 + hsize + 4 * i + 2], d[14 + hsize + 4 * i + 1], d[14 + hsize + 4 * i]))
                   for i in range(ncol))
    stride = (w + 3) & ~3
    rows = [d[off + stride * y: off + stride * y + w] for y in range(abs(h))]
    if h > 0:
        rows.reverse()
    return w, abs(h), b"".join(rows), pal


def decompile(plan, log):
    """Run mdldec on the staged copy; return the directory with its raw output."""
    work = os.path.join(SCRATCH, "decompile", plan.folder)
    if os.path.isdir(work):
        shutil.rmtree(work)
    indir, outdir = os.path.join(work, "in"), os.path.join(work, "out")
    staged_copy(plan, indir)
    os.makedirs(outdir)
    env = dict(os.environ, MDLDEC_ACT_PATH=os.path.join(os.path.dirname(MDLDEC), "res"), LC_ALL="C")
    # -a anims/ folder, -t textures/ folder, -u texel = u * width (studiomdlPlusPlus
    # convention), -m GoldSrc motion names (only matters for mdldec's own QC, kept as a log)
    cmd = [MDLDEC, "-a", "-t", "-u", "-m", os.path.join(indir, plan.name + ".mdl"), outdir + "/"]
    res = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = res.stdout.decode("latin-1")
    with open(os.path.join(work, "mdldec.log"), "w") as fh:
        fh.write(text)
    if res.returncode != 0 or "ERROR" in text:
        errs = [ln for ln in text.splitlines() if "ERROR" in ln] or ["exit code %d" % res.returncode]
        raise RuntimeError("mdldec: " + "; ".join(errs))
    plan.mdldec_warnings = [ln.strip() for ln in text.splitlines() if "WARNING" in ln]
    return outdir


def assemble(plan, outdir, rules):
    """Move mdldec's output into the reference layout under the final names and write the QC."""
    m = plan.model
    dest = os.path.join(REPO, rules["output_dir"], plan.folder)
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    os.makedirs(os.path.join(dest, "mesh"))
    os.makedirs(os.path.join(dest, "textures"))
    material = {("tex%03d.bmp" % i).encode(): new.encode("ascii") for i, new in enumerate(plan.texture_names)}
    for k, stem in enumerate(plan.submodel_stems):
        if stem is None:
            continue
        src = os.path.join(outdir, "ref%03d.smd" % k)
        if not os.path.isfile(src):
            raise RuntimeError("mdldec did not write the mesh of sub-model %d (%s)" % (k, stem))
        with open(src, "rb") as fh:
            lines = fh.read().split(b"\n")
        start = lines.index(b"triangles")
        for i in range(start + 1, len(lines)):
            if lines[i] in material:
                lines[i] = material[lines[i]]
            elif lines[i].startswith(b"tex") or lines[i].startswith(b"MDLDEC"):
                raise RuntimeError("unexpected material %r in mesh %s" % (lines[i], stem))
        with open(os.path.join(dest, "mesh", stem + ".smd"), "wb") as fh:
            fh.write(b"\n".join(lines))
    if m.sequences or plan.add_idle():
        os.makedirs(os.path.join(dest, "anims"))
    if plan.add_idle():
        # one frame, the reference pose: the skeleton block of the first reference mesh
        first = next(st for st in plan.submodel_stems if st is not None)
        with open(os.path.join(dest, "mesh", first + ".smd"), "rb") as fh:
            ref = fh.read()
        with open(os.path.join(dest, "anims", "idle.smd"), "wb") as fh:
            fh.write(ref[:ref.index(b"triangles\n")])
    for i, (s, stem) in enumerate(zip(m.sequences, plan.seq_stems)):
        for staged, final in zip(plan.seq_files(s, "seq%03d" % i), plan.seq_files(s, stem)):
            src = os.path.join(outdir, "anims", staged + ".smd")
            if not os.path.isfile(src):
                raise RuntimeError("mdldec did not write animation %s (%s)" % (staged, final))
            shutil.copyfile(src, os.path.join(dest, "anims", final + ".smd"))
    for i, (t, new) in enumerate(zip(m.textures, plan.texture_names)):
        src = os.path.join(outdir, "textures", "tex%03d.bmp" % i)
        if not os.path.isfile(src):
            raise RuntimeError("mdldec did not write texture %d (%s)" % (i, new))
        w, h, pix, pal = read_bmp(src)
        if (w, h) != (t["width"], t["height"]) or pix != t["pixels"] or pal != t["palette"]:
            raise RuntimeError("texture %s: the BMP does not hold the model's pixels/palette" % new)
        shutil.copyfile(src, os.path.join(dest, "textures", new))
    with open(os.path.join(dest, plan.name + ".qc"), "w", newline="\n") as fh:
        fh.write(write_qc(plan))
    return dest


# ---------------------------------------------------------------------------
# QC
# ---------------------------------------------------------------------------

def num(v):
    """Shortest fixed notation with up to 6 decimals (the style of the reference QCs)."""
    s = ("%.6f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def vec(v):
    return " ".join(num(x) for x in v)


def load_activities():
    """Activity number -> name from the game's own src/dlls/activity.h."""
    names, value = {}, -1
    with open(ACTIVITY_H, encoding="latin-1") as fh:
        text = fh.read()
    body = text[text.index("typedef enum"):]
    body = body[body.index("{") + 1:body.index("}")]
    body = re.sub(r"//[^\n]*", "", body)
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            item, val = [x.strip() for x in item.split("=")]
            value = int(val, 0)
        else:
            value += 1
        names[value] = item
    return names


ACTIVITIES = None


def activity_name(n):
    global ACTIVITIES
    if ACTIVITIES is None:
        ACTIVITIES = load_activities()
    return ACTIVITIES.get(n, "ACT_%d" % n)


def auto_texture_flags(name):
    """Flags the compiler derives from the texture name: as in Valve's studiomdl, only
    names containing "chrome" (flat shading + chrome)."""
    if "chrome" in name.lower():
        return 0x0001 | 0x0002
    return 0


def motion_word(bits):
    names = mdl.motion_names(bits & 0x7FFF)
    return names[0] if names else None


def write_qc(plan):
    m = plan.model
    h = m.hdr
    L = []
    L.append('$modelname "%s.mdl"' % plan.name)
    L.append('$cd ".\\mesh"')
    L.append('$cdtexture ".\\textures"')
    L.append("$scale 1.0")
    if plan.needs_keepallbones():
        L.append("$keepallbones")
    L.append("")
    k = 0
    for bp in m.bodyparts:
        L.append('$bodygroup "%s"' % bp["name"])
        L.append("{")
        for _sm in bp["models"]:
            stem = plan.submodel_stems[k]
            k += 1
            L.append("\tblank" if stem is None else '\tstudio "%s"' % stem)
        L.append("}")
        L.append("")
    L.append("$flags %d" % h["flags"])
    L.append("")
    eye = h["eyeposition"]
    if any(abs(x) > 1e-9 for x in eye):
        # the compiler stores (-y, x, z)
        L.append("$eyeposition %s" % vec((eye[1], -eye[0], eye[2])))
        L.append("")
    modes = []
    for t, new in zip(m.textures, plan.texture_names):
        extra = t["flags"] & ~auto_texture_flags(new)
        for bit, word in mdl.TEXTURE_FLAGS:
            if extra & bit:
                modes.append('$texrendermode "%s" %s' % (new, word))
    if modes:
        L += modes + [""]
    if m.numskinfamilies > 1:
        L.append('$texturegroup "skinfamilies"')
        L.append("{")
        for f in range(m.numskinfamilies):
            row = m.skins[f * m.numskinref:(f + 1) * m.numskinref]
            base = m.skins[:m.numskinref]
            changed = [j for j in range(m.numskinref)
                       if any(m.skins[g * m.numskinref + j] != base[j] for g in range(m.numskinfamilies))]
            L.append("\t{ " + " ".join('"%s"' % plan.texture_names[row[j]] for j in changed) + " }")
        L.append("}")
        L.append("")
    if m.attachments:
        for i, a in enumerate(m.attachments):
            L.append('$attachment %d "%s" %s' % (i, m.bones[a["bone"]]["name"], vec(a["org"])))
        L.append("")
    L.append("$cbox %s %s" % (vec(h["bbmin"]), vec(h["bbmax"])))
    L.append("")
    L.append("$bbox %s %s" % (vec(h["min"]), vec(h["max"])))
    L.append("")
    if m.hitboxes:
        for hb in m.hitboxes:
            L.append('$hbox %d "%s" %s %s' % (hb["group"], m.bones[hb["bone"]]["name"],
                                              vec(hb["bbmin"]), vec(hb["bbmax"])))
        L.append("")
    if m.controllers:
        for c in m.controllers:
            L.append('$controller %d "%s" %s %s %s' % (
                c["index"], m.bones[c["bone"]]["name"], motion_word(c["type"]) or "X",
                num(c["start"]), num(c["end"])))
        L.append("")
    for s, stem in zip(m.sequences, plan.seq_stems):
        L.append('$sequence "%s" {' % s["label"])
        for f in plan.seq_files(s, stem):
            L.append('\t"..\\anims\\%s"' % f)
        if s["activity"]:
            L.append("\t%s %d" % (activity_name(s["activity"]), s["actweight"]))
        if s["numblends"] > 1 and s["blendtype"][0]:
            L.append("\tblend %s %s %s" % (motion_word(s["blendtype"][0]),
                                           num(s["blendstart"][0]), num(s["blendend"][0])))
        for e in s["events"]:
            if e["options"]:
                L.append('\t{ event %d %d "%s" }' % (e["event"], e["frame"], e["options"]))
            else:
                L.append("\t{ event %d %d }" % (e["event"], e["frame"]))
        L.append("\tfps %s" % num(s["fps"]))
        if s["flags"] & 1:
            L.append("\tloop")
        motion = mdl.motion_names(s["motiontype"])
        if motion:
            L.append("\t" + " ".join(motion))
        if s["entrynode"] or s["exitnode"]:
            if s["nodeflags"] & 1:
                L.append("\trtransition %d %d" % (s["entrynode"], s["exitnode"]))
            elif s["entrynode"] == s["exitnode"]:
                L.append("\tnode %d" % s["entrynode"])
            else:
                L.append("\ttransition %d %d" % (s["entrynode"], s["exitnode"]))
        L.append("}")
    if plan.add_idle():
        L += ['$sequence "idle" {', '\t"..\\anims\\idle"', "\tfps 30", "}"]
    while L and L[-1] == "":
        L.pop()
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------------------
# Rebuild
# ---------------------------------------------------------------------------

IDLE_NOTE = "one-frame 'idle' sequence added by the rules table (the shipped model has no sequence)"


def rebuild(plan, rules):
    """Compile the generated sources into scratch. Returns (mdl path, log, note)."""
    src = os.path.join(REPO, rules["output_dir"], plan.folder)
    out = os.path.join(SCRATCH, "rebuilt", plan.folder)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    qc = os.path.join(src, plan.name + ".qc")
    note = IDLE_NOTE if plan.add_idle() else None
    res = subprocess.run([STUDIOMDL, qc, "-o", out], cwd=out, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, env=dict(os.environ, LC_ALL="C"))
    log = res.stdout.decode("latin-1")
    with open(os.path.join(out, "studiomdl.log"), "w") as fh:
        fh.write(log)
    path = os.path.join(out, plan.name + ".mdl")
    if res.returncode != 0 or not os.path.isfile(path):
        lines = [ln for ln in log.splitlines() if ln.strip()]
        raise RuntimeError("studiomdl exit %d: %s" % (res.returncode, " | ".join(lines[-3:])))
    return path, log, note


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------

def close(a, b, tol):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def rot_delta(a, b):
    d = abs(a - b) % (2 * math.pi)
    return min(d, 2 * math.pi - d)


def canon_tri(corners):
    i = corners.index(min(corners))
    return corners[i:] + corners[:i]


def soup(model, sm, texname):
    """Triangles of a sub-model as comparable tuples, in bone space."""
    out = []
    for mesh in sm["meshes"]:
        tindex = model.skins[mesh["skinref"]] if model.skins else mesh["skinref"]
        tname = texname(tindex)
        # the engine generates the texture coordinates of chrome textures itself and
        # the compiler stores none for them, so they are not compared
        chrome = bool(model.textures[tindex]["flags"] & 0x0002)
        for tri in mesh["tris"]:
            corners = []
            for vi, ni, s, t in tri:
                if chrome:
                    s = t = 0
                p = sm["verts"][vi]
                key = (model.bones[sm["vbone"][vi]]["name"], round(p[0], 2) + 0.0, round(p[1], 2) + 0.0,
                       round(p[2], 2) + 0.0, s, t)
                corners.append((key, p, sm["norms"][ni]))
            keys = [c[0] for c in corners]
            i = keys.index(min(keys))
            corners = corners[i:] + corners[:i]
            out.append((tname, tuple(c[0] for c in corners), corners))
    return out


def compare_soup(o_model, o_sm, n_model, n_sm, o_tex, n_tex, where, diffs, stats):
    for mesh in o_sm["meshes"]:
        tindex = o_model.skins[mesh["skinref"]] if o_model.skins else mesh["skinref"]
        tex = o_model.textures[tindex]
        # real (varying) texture coordinates on a chrome texture: the flag was probably
        # set after compiling; the rebuilt model stores none (the sources keep them)
        if tex["flags"] & 0x0002 and len(set(c[2:] for tri in mesh["tris"] for c in tri)) > 1:
            stats.setdefault("chrome_uv", {})
            stats["chrome_uv"][o_tex(tindex)] = stats["chrome_uv"].get(o_tex(tindex), 0) + len(mesh["tris"])
    a = soup(o_model, o_sm, o_tex)
    b = soup(n_model, n_sm, n_tex)
    if len(a) != len(b):
        diffs.append("%s: %d triangles, rebuilt has %d" % (where, len(a), len(b)))
    groups = collections.defaultdict(list)
    for t in b:
        groups[(t[0], t[1])].append(t)
    left, maxn = [], 0.0
    for t in a:
        g = groups.get((t[0], t[1]))
        if g:
            u = g.pop()
            for ca, cb in zip(t[2], u[2]):
                maxn = max(maxn, max(abs(x - y) for x, y in zip(ca[2], cb[2])))
        else:
            left.append(t)
    rest = [t for g in groups.values() for t in g]
    # second pass with tolerances (rounding boundaries, one-texel UV differences)
    unmatched, uvoff = 0, 0
    for t in left:
        hit = None
        for idx, u in enumerate(rest):
            if u[0] != t[0]:
                continue
            for rot in range(3):
                uc = u[2][rot:] + u[2][:rot]
                if all(ca[0][0] == cb[0][0] and close(ca[1], cb[1], TOL_POS)
                       and abs(ca[0][4] - cb[0][4]) <= 1 and abs(ca[0][5] - cb[0][5]) <= 1
                       for ca, cb in zip(t[2], uc)):
                    hit = (idx, uc)
                    break
            if hit:
                break
        if hit is None:
            unmatched += 1
            continue
        idx, uc = hit
        rest.pop(idx)
        if any(ca[0][4:] != cb[0][4:] for ca, cb in zip(t[2], uc)):
            uvoff += 1
        for ca, cb in zip(t[2], uc):
            maxn = max(maxn, max(abs(x - y) for x, y in zip(ca[2], cb[2])))
    if unmatched or rest:
        diffs.append("%s: %d shipped triangles have no match in the rebuilt mesh (%d rebuilt triangles left over)"
                     % (where, unmatched, len(rest)))
    if uvoff:
        diffs.append("%s: %d triangles differ by one texel in a UV coordinate" % (where, uvoff))
    if maxn > TOL_NORMAL:
        diffs.append("%s: vertex normals differ by up to %.3f" % (where, maxn))
    stats["max_normal_delta"] = max(stats.get("max_normal_delta", 0.0), maxn)
    stats["triangles"] = stats.get("triangles", 0) + len(a)


def compare(plan, new, ignore_sequences=False):
    """-> (diffs, infos, stats). diffs = functional differences; infos = expected/by design."""
    o = plan.model
    diffs, infos, stats = [], [], {}
    oh, nh = o.hdr, new.hdr

    for field, label in (("eyeposition", "eye position"), ("min", "$bbox min"), ("max", "$bbox max"),
                         ("bbmin", "$cbox min"), ("bbmax", "$cbox max")):
        if not close(oh[field], nh[field], TOL_BOX):
            diffs.append("%s: %s, rebuilt %s" % (label, vec(oh[field]), vec(nh[field])))
    if oh["flags"] != nh["flags"]:
        diffs.append("model flags: %d, rebuilt %d" % (oh["flags"], nh["flags"]))
    if new.external_textures or nh["numtextures"] == 0 and o.textures:
        diffs.append("rebuilt model still has external textures")
    if nh["numseqgroups"] != 1:
        diffs.append("rebuilt model has %d sequence groups" % nh["numseqgroups"])

    # bones
    onames = [b["name"] for b in o.bones]
    nnames = [b["name"] for b in new.bones]
    bone_ok = True
    if onames != nnames:
        bone_ok = False
        if sorted(onames) == sorted(nnames):
            diffs.append("bone order changed (same %d bones)" % len(onames))
        else:
            missing = [x for x in onames if x not in nnames]
            extra = [x for x in nnames if x not in onames]
            diffs.append("bones: %d, rebuilt %d; missing %r; extra %r" % (len(onames), len(nnames), missing, extra))
    nidx = {n: i for i, n in enumerate(nnames)}

    def parent_name(model, b):
        return model.bones[b["parent"]]["name"] if b["parent"] >= 0 else None

    for b in o.bones:
        if b["name"] in nidx:
            nb = new.bones[nidx[b["name"]]]
            if parent_name(o, b) != parent_name(new, nb):
                diffs.append("bone %s: parent %s, rebuilt %s" % (b["name"], parent_name(o, b), parent_name(new, nb)))
            if b["flags"] != nb["flags"]:
                diffs.append("bone %s: flags %d, rebuilt %d" % (b["name"], b["flags"], nb["flags"]))
            if tuple(b["controller"]) != tuple(nb["controller"]):
                diffs.append("bone %s: controller links %r, rebuilt %r" % (b["name"], b["controller"], nb["controller"]))

    def ctl(model):
        return [(model.bones[c["bone"]]["name"], c["type"], round(c["start"], 3), round(c["end"], 3),
                 c["rest"], c["index"]) for c in model.controllers]
    if ctl(o) != ctl(new):
        diffs.append("bone controllers: %r, rebuilt %r" % (ctl(o), ctl(new)))

    if len(o.hitboxes) != len(new.hitboxes):
        diffs.append("hitboxes: %d, rebuilt %d" % (len(o.hitboxes), len(new.hitboxes)))
    else:
        for i, (a, b) in enumerate(zip(o.hitboxes, new.hitboxes)):
            if (o.bones[a["bone"]]["name"], a["group"]) != (new.bones[b["bone"]]["name"], b["group"]) \
                    or not close(a["bbmin"] + a["bbmax"], b["bbmin"] + b["bbmax"], TOL_BOX):
                diffs.append("hitbox %d differs" % i)

    if len(o.attachments) != len(new.attachments):
        diffs.append("attachments: %d, rebuilt %d" % (len(o.attachments), len(new.attachments)))
    else:
        for i, (a, b) in enumerate(zip(o.attachments, new.attachments)):
            if o.bones[a["bone"]]["name"] != new.bones[b["bone"]]["name"] or not close(a["org"], b["org"], TOL_BOX):
                diffs.append("attachment %d differs" % i)
            if a["name"] != b["name"]:
                infos.append("attachment %d name %r, rebuilt %r" % (i, a["name"], b["name"]))

    # textures
    def o_tex(i):
        return plan.texture_names[i]

    def n_tex(i):
        return new.textures[i]["name"]

    if len(o.textures) != len(new.textures):
        diffs.append("textures: %d, rebuilt %d" % (len(o.textures), len(new.textures)))
    nt = {t["name"]: t for t in new.textures}
    for t, name in zip(o.textures, plan.texture_names):
        r = nt.get(name)
        if r is None:
            diffs.append("texture %s missing in the rebuilt model" % name)
            continue
        if (t["width"], t["height"]) != (r["width"], r["height"]):
            diffs.append("texture %s: %dx%d, rebuilt %dx%d" % (name, t["width"], t["height"], r["width"], r["height"]))
        elif t["pixels"] != r["pixels"] or t["palette"] != r["palette"]:
            diffs.append("texture %s: pixels or palette differ" % name)
        if t["flags"] != r["flags"]:
            diffs.append("texture %s: flags %s, rebuilt %s" % (
                name, "+".join(mdl.texture_flag_names(t["flags"])) or "none",
                "+".join(mdl.texture_flag_names(r["flags"])) or "none"))
    if [n for n in plan.texture_names] != [t["name"] for t in new.textures] and \
            sorted(plan.texture_names) == sorted(t["name"] for t in new.textures):
        infos.append("texture order differs (same set)")
    if o.numskinfamilies != new.numskinfamilies and (o.numskinfamilies > 1 or new.numskinfamilies > 1):
        diffs.append("skin families: %d, rebuilt %d" % (o.numskinfamilies, new.numskinfamilies))
    stats["textures"] = len(o.textures)
    stats["texture_bytes"] = sum(t["width"] * t["height"] for t in o.textures)

    # bodies
    if [(bp["name"], bp["nummodels"], bp["base"]) for bp in o.bodyparts] != \
            [(bp["name"], bp["nummodels"], bp["base"]) for bp in new.bodyparts]:
        diffs.append("body groups: %r, rebuilt %r" % (
            [(bp["name"], bp["nummodels"], bp["base"]) for bp in o.bodyparts],
            [(bp["name"], bp["nummodels"], bp["base"]) for bp in new.bodyparts]))
    else:
        k = 0
        for bp_o, bp_n in zip(o.bodyparts, new.bodyparts):
            for sm_o, sm_n in zip(bp_o["models"], bp_n["models"]):
                stem = plan.submodel_stems[k]
                k += 1
                where = "body %s/%s" % (bp_o["name"], sm_o["name"])
                expect = "blank" if stem is None else stem
                if sm_n["name"] != expect:
                    diffs.append("%s: rebuilt sub-model is named %r" % (where, sm_n["name"]))
                elif sm_n["name"] != sm_o["name"]:
                    infos.append("sub-model name %r -> %r" % (sm_o["name"], sm_n["name"]))
                if sm_o["numverts"] != sm_n["numverts"] or sm_o["numnorms"] != sm_n["numnorms"]:
                    infos.append("%s: vertex table %d -> %d, normal table %d -> %d (triangles compared separately)"
                                 % (where, sm_o["numverts"], sm_n["numverts"], sm_o["numnorms"], sm_n["numnorms"]))
                if bone_ok or sorted(onames) == sorted(nnames):
                    compare_soup(o, sm_o, new, sm_n, o_tex, n_tex, where, diffs, stats)

    # sequences
    if not ignore_sequences:
        if len(o.sequences) != len(new.sequences):
            diffs.append("sequences: %d, rebuilt %d" % (len(o.sequences), len(new.sequences)))
        max_pos, max_rot = 0.0, 0.0
        worst_pos = worst_rot = None
        for i, (a, b) in enumerate(zip(o.sequences, new.sequences)):
            w = "sequence %d %s" % (i, a["label"])
            for f in ("label", "flags", "activity", "actweight", "numframes", "numblends",
                      "motionbone", "entrynode", "exitnode", "nodeflags", "numpivots"):
                if a[f] != b[f]:
                    diffs.append("%s: %s %r, rebuilt %r" % (w, f, a[f], b[f]))
            if a["motiontype"] != b["motiontype"]:
                diffs.append("%s: motion type %s, rebuilt %s" % (
                    w, " ".join(mdl.motion_names(a["motiontype"])) or "none",
                    " ".join(mdl.motion_names(b["motiontype"])) or "none"))
            if abs(a["fps"] - b["fps"]) > 1e-4:
                diffs.append("%s: fps %s, rebuilt %s" % (w, num(a["fps"]), num(b["fps"])))
            ea = [(e["frame"], e["event"], e["type"], e["options"]) for e in a["events"]]
            eb = [(e["frame"], e["event"], e["type"], e["options"]) for e in b["events"]]
            if ea != eb:
                diffs.append("%s: events %r, rebuilt %r" % (w, ea, eb))
            if not close(a["linearmovement"], b["linearmovement"], TOL_MOVE):
                diffs.append("%s: linear movement %s, rebuilt %s" % (w, vec(a["linearmovement"]), vec(b["linearmovement"])))
            if a["numblends"] > 1 or b["numblends"] > 1:
                if a["blendtype"] != b["blendtype"] or not close(a["blendstart"] + a["blendend"],
                                                                 b["blendstart"] + b["blendend"], 1e-3):
                    diffs.append("%s: blend type/range %r %s..%s, rebuilt %r %s..%s" % (
                        w, a["blendtype"], num(a["blendstart"][0]), num(a["blendend"][0]),
                        b["blendtype"], num(b["blendstart"][0]), num(b["blendend"][0])))
            if not close(a["bbmin"] + a["bbmax"], b["bbmin"] + b["bbmax"], TOL_SEQBOX):
                stats.setdefault("seq_bbox", []).append(
                    "%s: %s / %s, rebuilt %s / %s" % (a["label"], vec(a["bbmin"]), vec(a["bbmax"]),
                                                      vec(b["bbmin"]), vec(b["bbmax"])))
            if a["anim"] is None or b["anim"] is None:
                diffs.append("%s: animation data could not be compared" % w)
                continue
            if a["numframes"] != b["numframes"] or a["numblends"] != b["numblends"]:
                continue
            sp = sr = 0.0
            bad = False
            for bl in range(a["numblends"]):
                for j, bone in enumerate(o.bones):
                    if bone["name"] not in nidx:
                        continue
                    nj = nidx[bone["name"]]
                    nbone = new.bones[nj]
                    for c in range(6):
                        ca = a["anim"][bl][j][c]
                        cb = b["anim"][bl][nj][c]
                        if ca is None and cb is None:
                            d = abs(bone["value"][c] - nbone["value"][c]) if c < 3 else \
                                rot_delta(bone["value"][c], nbone["value"][c])
                        else:
                            va = ca if ca is not None else [bone["value"][c]] * a["numframes"]
                            vb = cb if cb is not None else [nbone["value"][c]] * a["numframes"]
                            if c < 3:
                                d = max(abs(x - y) for x, y in zip(va, vb))
                            else:
                                d = max(rot_delta(x, y) for x, y in zip(va, vb))
                        # allow one quantization step of each model on top of the base tolerance
                        step = abs(bone["scale"][c]) + abs(nbone["scale"][c])
                        if c < 3:
                            sp = max(sp, d)
                            bad = bad or d > TOL_POS + step
                        else:
                            sr = max(sr, d)
                            bad = bad or d > TOL_ROT + step
            if sp > max_pos:
                max_pos, worst_pos = sp, a["label"]
            if sr > max_rot:
                max_rot, worst_rot = sr, a["label"]
            if bad:
                diffs.append("%s: animation differs (max position %.4f units, max rotation %.5f rad)" % (w, sp, sr))
        stats["max_anim_pos_delta"] = round(max_pos, 5)
        stats["max_anim_rot_delta"] = round(max_rot, 6)
        stats["worst_anim"] = [worst_pos, worst_rot]
    if o.transitions != new.transitions:
        diffs.append("transition table differs")
    return diffs, infos, stats


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def make_plans(rules, only=None):
    models, orphans = discover(rules)
    plans = []
    for key, entry in models.items():
        if only and key not in only and key.split("/")[-1] not in only:
            continue
        plans.append(Plan(key, entry, rules))
    if only and not plans:
        die("no model matches %r" % (only,))
    folders = collections.Counter(p.folder for p in plans if p.folder)
    dup = [f for f, c in folders.items() if c > 1]
    if dup:
        die("two models map to the same folder: %r" % dup)
    return plans, orphans


def load_state():
    path = os.path.join(SCRATCH, "report.json")
    if os.path.isfile(path):
        with open(path) as fh:
            return json.load(fh)
    return {"build": {}, "verify": {}}


def save_state(state):
    os.makedirs(SCRATCH, exist_ok=True)
    with open(os.path.join(SCRATCH, "report.json"), "w") as fh:
        json.dump(state, fh, indent=1, sort_keys=True)
        fh.write("\n")


def manifest_path(rules):
    return os.path.join(REPO, rules["output_dir"], "installed.json")


def load_manifest(rules):
    path = manifest_path(rules)
    if os.path.isfile(path):
        with open(path) as fh:
            return json.load(fh)
    return {"models": {}, "deleted": []}


def save_manifest(rules, manifest):
    manifest["deleted"] = sorted(set(manifest["deleted"]))
    with open(manifest_path(rules), "w", newline="\n") as fh:
        json.dump(manifest, fh, indent=1, sort_keys=True)
        fh.write("\n")


def sha1_file(path):
    with open(path, "rb") as fh:
        return hashlib.sha1(fh.read()).hexdigest()


def is_installed(plan, manifest):
    """True when the model in gamedir/ is the file `install` put there."""
    rec = manifest["models"].get(plan.key)
    return bool(rec) and os.path.isfile(plan.entry["path"]) and sha1_file(plan.entry["path"]) == rec["sha1"]


def acceptable(diffs):
    """A comparison result that may be installed: equivalent, or intentional differences only."""
    return all(d.startswith("intentional:") for d in diffs)


def header_references(path):
    """Lower-case file names that a model file needs beside itself: its texture file when
    it has no textures inside, and its sequence-group files, both by the name stored in
    the header (what the engine loads) and by the <name>NN.mdl convention."""
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:4] != b"IDST":
        return set()
    hdr = mdl.read_header(data)
    stem = os.path.basename(path)[:-4].lower()
    out = set()
    if hdr["numbodyparts"] and hdr["numtextures"] == 0 and hdr["textureindex"] == 0:
        out.add(stem + "t.mdl")
    for i in range(1, hdr["numseqgroups"]):
        off = hdr["seqgroupindex"] + 104 * i + 32
        name = mdl.cstr(data[off:off + 64])
        out.add(name.replace("\\", "/").split("/")[-1].lower())
        out.add("%s%02d.mdl" % (stem, i))
    return out


def cmd_install(rules, only, accept=False):
    if not os.access(STUDIOMDL, os.X_OK):
        die("tool not found or not executable: %s" % STUDIOMDL)
    plans, _ = make_plans(rules, only)
    all_plans, _ = make_plans(rules, None)
    state = load_state()
    manifest = load_manifest(rules)
    root = os.path.join(REPO, rules["models_dir"])
    skip = rules.get("install_skip", {})
    todo, refused = [], []
    for p in plans:
        if not p.model:
            print("skip      %-28s %s" % (p.key, p.skip or p.error))
            continue
        standin = p.model.placeholder_textures
        if p.key in skip or (standin and p.key not in rules.get("install_stand_ins", {})):
            print("skip      %-28s %s" % (p.key, skip.get(p.key, "stand-in textures (not in install_stand_ins)")))
            continue
        installed = is_installed(p, manifest)
        v = state["verify"].get(p.key)
        if not installed and not accept and (
                not v or v.get("status") not in ("equivalent", "differences")
                or not acceptable(v.get("diffs", ["?"]))):
            refused.append(p.key)
            print("REFUSED   %-28s last verify result: %s" % (p.key, v.get("status") if v else "none (run verify)"))
            continue
        try:
            path, _log, note = rebuild(p, rules)
            new = mdl.Model(path)
        except (RuntimeError, mdl.MdlError, struct.error, IndexError, OSError) as exc:
            refused.append(p.key)
            print("REFUSED   %-28s %s" % (p.key, exc))
            continue
        if installed and sha1_file(path) == sha1_file(p.entry["path"]):
            print("installed %-28s up to date (the sources compile to the installed file)" % p.key)
            continue
        diffs, _infos, _stats = compare(p, new, ignore_sequences=note is not None)
        if note:
            diffs.insert(0, "intentional: " + note)
        if accept and not acceptable(diffs):
            # deliberate change of the sources, accepted by the caller: recorded as such
            print("accepted  %-28s %d differences from the file in gamedir/:" % (p.key, len(diffs)))
            for d in diffs:
                print("              - " + d)
            diffs = ["intentional: accepted with --accept: " + d for d in diffs]
        if not acceptable(diffs) or new.hdr["numseqgroups"] != 1 or new.external_textures \
                or (p.model.textures and not new.hdr["numtextures"]):
            refused.append(p.key)
            print("REFUSED   %-28s not equivalent: %s" % (p.key, "; ".join(diffs[:3]) or "needs external files"))
            continue
        todo.append((p, path, diffs, standin))
    # files that become dead: companions of the installed models and the listed orphans
    going = set(p.key for p, _path, _d, _s in todo)
    dead = {}
    for p, _path, _d, _s in todo:
        for f in p.model.files[1:]:
            dead[os.path.relpath(f, root).replace(os.sep, "/")] = "was part of %s.mdl" % p.key
    for rel in rules.get("install_delete", []):
        if os.path.isfile(os.path.join(root, rel)):
            dead.setdefault(rel, "orphan (rules: install_delete)")
    # a file is only deleted when no model that stays as it is needs it
    needed = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            full = os.path.join(dirpath, fn)
            if not fn.lower().endswith(".mdl") or full in [t[0].entry["path"] for t in todo]:
                continue
            folder = os.path.relpath(dirpath, root).replace(os.sep, "/")
            folder = "" if folder == "." else folder
            for name in header_references(full):
                needed[(folder + "/" + name).lstrip("/")] = os.path.relpath(full, root)[:-4]
    kept = {}
    for rel in sorted(dead):
        if rel.lower() in needed:
            kept[rel] = needed[rel.lower()]
            del dead[rel]
    record = os.path.join(REPO, rules["output_dir"], "ORIGINAL-FILES.md")
    readme = os.path.join(REPO, rules["output_dir"], "README.md")
    if todo and not manifest["models"] and os.path.isfile(readme):
        # first install: keep the description of the original files and of their comparison
        with open(readme) as fh:
            text = fh.read()
        with open(record, "w", newline="\n") as fh:
            fh.write("> **Record.** This is `README.md` as it was generated just before the rebuilt models\n"
                     "> were installed into `gamedir/`: the inventory of the original SBO 1.38 model files\n"
                     "> and the comparison of every rebuilt model against them. Not regenerated.\n\n" + text)
    for p, path, diffs, standin in todo:
        rel = os.path.relpath(p.entry["path"], root).replace(os.sep, "/")
        before = {"files": [os.path.relpath(f, root).replace(os.sep, "/") for f in p.model.files],
                  "size": p.model.total_size()}
        if p.key in manifest["models"]:
            # re-install after a change of the sources: keep the record of the original files
            before = manifest["models"][p.key]["replaced"]
        shutil.copyfile(path, p.entry["path"])
        manifest["models"][p.key] = {
            "file": rel, "sha1": sha1_file(p.entry["path"]), "size": os.path.getsize(p.entry["path"]),
            "result": "equivalent" if not diffs else "intentional difference", "differences": diffs,
            "replaced": before}
        if standin:
            manifest["models"][p.key]["stand_in_textures"] = rules.get("install_stand_ins", {}).get(p.key, "stand-in textures")
        print("replaced  %-28s %s (%d -> %d bytes, originally %d file%s)%s" % (
            p.key, rel, before["size"], os.path.getsize(p.entry["path"]), len(before["files"]),
            "" if len(before["files"]) == 1 else "s",
            ("  [stand-in textures]" if standin else "") + ("" if not diffs else "  [" + diffs[0] + "]")))
    for rel in sorted(dead):
        os.remove(os.path.join(root, rel))
        manifest["deleted"].append(rel)
        print("deleted   %-28s %s" % (rel, dead[rel]))
    for rel in sorted(kept):
        print("KEPT      %-28s still needed by %s.mdl" % (rel, kept[rel]))
    save_manifest(rules, manifest)
    print("install: %d replaced, %d deleted, %d kept, %d refused; %d models installed in total" % (
        len(todo), len(dead), len(kept), len(refused), len(manifest["models"])))
    if refused:
        sys.exit(1)


def cmd_package(rules, dest):
    """Copy the complete models folder (every file, sub-folders included) to
    dest/syndicate/models: a drop-in replacement for an install's models folder. Refuses
    while a model in the folder still needs an external file or is not what install built."""
    manifest = load_manifest(rules)
    root = os.path.join(REPO, rules["models_dir"])
    keep = rules.get("install_keep", {})
    known = {rec["file"]: rec for rec in manifest["models"].values()}
    files = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            files.append(os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/"))
    for rel in files:
        full = os.path.join(root, rel)
        if not rel.lower().endswith(".mdl"):
            continue
        if header_references(full):
            die("%s still needs external files: %s" % (rel, sorted(header_references(full))))
        if rel in known:
            if sha1_file(full) != known[rel]["sha1"]:
                die("%s is not the installed file any more (run verify / install)" % rel)
        elif rel[:-4] not in keep:
            die("%s is neither built from the sources nor listed in install_keep" % rel)
    out = os.path.join(dest, "syndicate", "models")
    if os.path.isdir(out):
        shutil.rmtree(out)
    for rel in files:
        os.makedirs(os.path.dirname(os.path.join(out, rel)), exist_ok=True)
        shutil.copyfile(os.path.join(root, rel), os.path.join(out, rel))
    old = os.path.join(dest, "DELETE.txt")
    if os.path.isfile(old):
        os.remove(old)
    print("package: %d files (%d built models) in %s" % (len(files), len(known), out))


def cmd_build(rules, only):
    for tool in (MDLDEC,):
        if not os.access(tool, os.X_OK):
            die("tool not found or not executable: %s" % tool)
    plans, _ = make_plans(rules, only)
    state = load_state()
    outroot = os.path.join(REPO, rules["output_dir"])
    manifest = load_manifest(rules)
    if not only:
        if not manifest["models"]:
            # full run: drop every generated category folder so that stale sources cannot stay
            for cat in sorted(set(p.folder.split("/")[0] for p in plans if p.folder)):
                path = os.path.join(outroot, cat)
                if os.path.isdir(path):
                    shutil.rmtree(path)
        state["build"] = {}
    ok = kept = 0
    for p in plans:
        if p.model and is_installed(p, manifest):
            # the file in gamedir/ was compiled from the sources: they are the master now
            state["build"][p.key] = {"status": "installed", "folder": p.folder}
            kept += 1
            continue
        if p.skip:
            state["build"][p.key] = {"status": "skipped", "error": p.skip}
            print("skip    %-28s %s" % (p.key, p.skip))
            continue
        if p.error:
            state["build"][p.key] = {"status": "failed", "error": p.error}
            print("FAILED  %-28s %s" % (p.key, p.error))
            continue
        try:
            out = decompile(p, None)
            assemble(p, out, rules)
        except (RuntimeError, ValueError, OSError) as exc:
            state["build"][p.key] = {"status": "failed", "error": str(exc)}
            print("FAILED  %-28s %s" % (p.key, exc))
            continue
        state["build"][p.key] = {"status": "ok", "folder": p.folder, "mdldec_warnings": p.mdldec_warnings}
        ok += 1
        print("ok      %-28s -> %s/%s" % (p.key, rules["output_dir"], p.folder))
    save_state(state)
    print("build: %d of %d models decompiled%s" % (
        ok, len(plans), "" if not kept else "; %d installed models left alone (the sources are the master)" % kept))


def cmd_verify(rules, only):
    if not os.access(STUDIOMDL, os.X_OK):
        die("tool not found or not executable: %s" % STUDIOMDL)
    plans, _ = make_plans(rules, only)
    state = load_state()
    if not only:
        state["verify"] = {}
    counts = collections.Counter()
    for p in plans:
        b = state["build"].get(p.key, {})
        if p.skip or p.error or b.get("status") not in ("ok", "installed"):
            continue
        rec = {"shipped_size": p.model.total_size(), "shipped_files": len(p.model.files)}
        if b.get("status") == "installed":
            # installed model: the sources must still compile to exactly that file
            try:
                path, log, _note = rebuild(p, rules)
            except (RuntimeError, OSError) as exc:
                rec.update(status="failed", error=str(exc), installed=True)
                state["verify"][p.key] = rec
                counts["failed"] += 1
                print("FAILED      %-28s %s" % (p.key, exc))
                continue
            same = sha1_file(path) == sha1_file(p.entry["path"])
            rec.update(status="identical" if same else "differences", installed=True,
                       diffs=[] if same else ["the sources no longer compile to the installed file"],
                       infos=[], stats={}, placeholder=False, rebuilt_size=os.path.getsize(path),
                       compiler_warnings=[], rebuilt_seqgroups=1, sequences=len(p.model.sequences))
            state["verify"][p.key] = rec
            counts[rec["status"]] += 1
            print("%-11s %-28s" % (rec["status"], p.key))
            continue
        try:
            path, log, note = rebuild(p, rules)
            new = mdl.Model(path)
        except (RuntimeError, mdl.MdlError, struct.error, IndexError, OSError) as exc:
            rec.update(status="failed", error=str(exc))
            state["verify"][p.key] = rec
            counts["failed"] += 1
            print("FAILED      %-28s %s" % (p.key, exc))
            continue
        diffs, infos, stats = compare(p, new, ignore_sequences=note is not None)
        if note:
            diffs.insert(0, "intentional: " + note)
        warnings = sorted(set(ln.strip() for ln in log.splitlines()
                              if re.search(r"warning|WARNING|NOTE:", ln)))
        rec.update(status="equivalent" if not diffs else "differences", diffs=diffs, infos=infos,
                   placeholder=p.model.placeholder_textures,
                   stats=stats, rebuilt_size=os.path.getsize(path), compiler_warnings=warnings,
                   rebuilt_seqgroups=new.hdr["numseqgroups"], sequences=len(new.sequences))
        state["verify"][p.key] = rec
        counts[rec["status"]] += 1
        print("%-11s %-28s %s" % (rec["status"], p.key, "" if not diffs else "(%d)" % len(diffs)))
        for d in diffs[:8]:
            print("              - " + d)
        if len(diffs) > 8:
            print("              ... %d more" % (len(diffs) - 8))
    save_state(state)
    print("verify: " + ", ".join("%d %s" % (c, k) for k, c in sorted(counts.items())))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["build", "verify", "readme", "all", "install", "package"])
    ap.add_argument("models", nargs="*", help="model names (default: every shipped model); "
                    "for package: the destination folder")
    ap.add_argument("--accept", action="store_true",
                    help="install: also install models that differ from the file in gamedir/ "
                    "(deliberate changes of the sources); the differences are printed and recorded")
    args = ap.parse_args()
    rules = load_rules()
    if args.command == "package":
        if len(args.models) != 1:
            die("package needs one destination folder")
        cmd_package(rules, os.path.abspath(args.models[0]))
        return
    only = set(m.lower().replace(".mdl", "") for m in args.models) or None
    if args.command == "install":
        cmd_install(rules, only, args.accept)
        return
    if args.command in ("build", "all"):
        cmd_build(rules, only)
    if args.command in ("verify", "all"):
        cmd_verify(rules, only)
    if args.command in ("readme", "all"):
        import modelsrc_readme
        modelsrc_readme.write(rules, make_plans(rules, None), load_state(), sys.modules[__name__])


if __name__ == "__main__":
    main()
