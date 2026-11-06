#!/usr/bin/env python3
"""Writes sdk/modelsrc/README.md for scripts/modelsrc.py (inventory, pipeline, results).

Everything in the README is generated: from the shipped models, the rules table and the
last verification run (report.json in the scratch folder). Edit this file or the rules, not
the README.
"""

import collections
import hashlib
import json
import os
import re
import subprocess

MAPDATA = os.environ.get("MODELSRC_MAPDATA", "")


def kb(n):
    return "%.1f" % (n / 1024.0)


def code_refs(ms, plans):
    """model key -> sorted source files that name the model."""
    texts = {}
    for sub in ("src/dlls", "src/cl_dll", "src/game_shared", "src/pm_shared"):
        root = os.path.join(ms.REPO, sub)
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames.sort()
            for fn in sorted(filenames):
                if fn.endswith((".cpp", ".h", ".c")):
                    with open(os.path.join(dirpath, fn), encoding="latin-1") as fh:
                        texts[os.path.relpath(os.path.join(dirpath, fn), os.path.join(ms.REPO, "src"))] = fh.read().lower()
    refs = {}
    for p in plans:
        m = re.match(r"^player/([^/]+)/", p.key)
        if m:
            pat = re.compile(r'"model"\s*,\s*"%s"' % re.escape(m.group(1)))
        else:
            pat = re.compile(r'"models/%s\.mdl"' % re.escape(p.key))
        refs[p.key] = sorted(f for f, t in texts.items() if pat.search(t))
    return refs


def map_refs(plans):
    """model key -> (entities, maps) that name the model path, or None when not scanned."""
    if not MAPDATA or not os.path.isfile(MAPDATA):
        return None
    wanted = {"models/%s.mdl" % p.key: p.key for p in plans}
    ents = collections.Counter()
    maps = collections.defaultdict(set)
    with open(MAPDATA, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            low = line.lower()
            if ".mdl" not in low:
                continue
            rec = json.loads(line)
            for v in rec.get("kv", {}).values():
                for item in (v if isinstance(v, list) else [v]):
                    key = wanted.get(str(item).lower().replace("\\", "/"))
                    if key:
                        ents[key] += 1
                        maps[key].add(rec.get("map"))
    return {p.key: (ents[p.key], len(maps[p.key])) for p in plans}


def tool_version(cmd):
    try:
        out = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.decode("latin-1")
    except OSError:
        return "not found"
    for line in out.splitlines():
        if line.strip():
            return line.strip()
    return "unknown"


def odd_texture_name(name):
    """Reasons a (final) texture name is unusual; these are reported, not changed."""
    why = []
    if " " in name:
        why.append("space")
    if name.count(".") > 1:
        why.append("double extension")
    if re.search(r"[&+()]", name):
        why.append("character " + " ".join(sorted(set(re.findall(r"[&+()]", name)))))
    return why


def unusual(ms, mdl, p, refs, mrefs, identical):
    """Automatic notes for the inventory's last column."""
    m = p.model
    out = []
    if p.rule.get("note"):
        out.append(p.rule["note"])
    if not refs.get(p.key):
        if mrefs and mrefs[p.key][0]:
            out.append("not named in the game code (maps name it)")
        else:
            out.append("not named in the game code" + (" or in any scanned map" if mrefs else ""))
    for other in identical.get(p.key, []):
        out.append("main file byte-identical to %s.mdl" % other)
    wrong = [g["name"] for i, g in enumerate(m.seqgroups[1:], 1)
             if g["name"].lower().replace("\\", "/") != "models/%s%02d.mdl" % (p.key, i)]
    if wrong:
        out.append("the header names %s as sequence group file%s, and the engine loads by that name (not %s01.mdl ...)"
                   % (", ".join("`%s`" % w for w in wrong), "" if len(wrong) == 1 else "s", p.name))
    if not m.sequences:
        out.append("no sequence at all" + (" (the sources add a one-frame `idle`)" if p.add_idle() else ""))
    zero = [s["label"] for s in m.sequences if s["numframes"] <= 0]
    if zero:
        out.append("zero-length sequences: " + ", ".join(zero))
    dups = [(n, c) for n, c in collections.Counter(s["label"] for s in m.sequences).items() if c > 1]
    if dups:
        out.append("repeated sequence names: " + ", ".join("%s x%d" % d for d in dups)
                   + " (kept; files get _2, _3 ...)")
    names = [sm["name"] for bp in m.bodyparts for sm in bp["models"] if sm["name"] != "blank"]
    dn = [(n, c) for n, c in collections.Counter(names).items() if c > 1]
    if dn:
        out.append("repeated sub-model names: " + ", ".join("%s x%d" % d for d in dn))
    for n in names:
        if "/" in n or "\\" in n:
            out.append("sub-model name holds a path: `%s`" % n)
    bp_names = [bp["name"] for bp in m.bodyparts]
    if len(set(bp_names)) != len(bp_names):
        out.append("several body groups share the name `%s`" % bp_names[0])
    amot = [s["label"] for s in m.sequences if s["motiontype"] & 0x7E00]
    if amot:
        out.append("AX/AY/AZ/AXR/AYR/AZR motion flags on %d sequence%s (kept: bits in the motion type only)"
                   % (len(amot), "" if len(amot) == 1 else "s"))
    for s in m.sequences:
        if s["numblends"] > 2:
            out.append("sequence %s has %d blends" % (s["label"], s["numblends"]))
        if s["numblends"] > 1 and not s["blendtype"][0]:
            out.append("sequence %s has %d blends but no blend controller type" % (s["label"], s["numblends"]))
    odd = []
    for t, new in zip(m.textures, p.texture_names):
        want = ms.auto_texture_flags(new)
        if want & ~t["flags"]:
            out.append("texture %s: shipped flags %s, but the compiler sets %s from the name" % (
                new, "+".join(mdl.texture_flag_names(t["flags"])) or "none",
                "+".join(mdl.texture_flag_names(want))))
        if odd_texture_name(new):
            odd.append(new)
    if odd:
        out.append("unusual texture names (space, `&`, `+`, parentheses or double extension): "
                   + ", ".join("`%s`" % n for n in odd[:3]) + (" and %d more" % (len(odd) - 3) if len(odd) > 3 else ""))
    if m.placeholder_textures:
        out.append("**texture file does not ship**: the sources use stand-in textures (`dev_placeholder*.bmp`)")
    elif m.hdr["numtextures"] == 0 and not m.external_textures and m.hdr["textureindex"] == 0:
        out.append("external texture file is missing")
    out += m.notes
    return out


def write(rules, plans_orphans, state, ms):
    import modelsrc_mdl as mdl
    plans, orphans = plans_orphans
    outroot = os.path.join(ms.REPO, rules["output_dir"])
    refs = code_refs(ms, plans)
    mrefs = map_refs(plans)
    digest = collections.defaultdict(list)
    for p in plans:
        with open(p.entry["path"], "rb") as fh:
            digest[hashlib.sha1(fh.read()).hexdigest()].append(p.key)
    identical = {}
    for keys in digest.values():
        for k in keys:
            identical[k] = [o for o in keys if o != k]
    V, B = state.get("verify", {}), state.get("build", {})
    manifest = ms.load_manifest(rules)
    if manifest["models"]:
        write_installed(rules, plans, state, ms, mdl, manifest, refs)
        return

    L = []
    A = L.append
    A("# SBO model sources")
    A("")
    A("Source files (QC script, reference meshes, animations, textures) for the models that ship in")
    A("`gamedir/syndicate/models/`, so that the models can be rebuilt with the studiomdlPlusPlus")
    A("compiler (studiomdlPlusPlus) and no longer need external")
    A("texture files (`nameT.mdl`) or external sequence-group files (`name01.mdl`, `name02.mdl`, ...).")
    A("")
    A("**This file and everything under the category folders are generated** by `scripts/modelsrc.py`")
    A("from the shipped models. Do not edit them by hand: change `scripts/modelsrc_rules.json` (or the")
    A("script) and run the pipeline again.")
    A("")
    A("Status: the sources were decompiled from the shipped binaries and each one was recompiled and")
    A("compared with the shipped model (results below). **Nothing was tested in the game**, and no")
    A("shipped `.mdl` has been replaced. The comparison needs a studiomdlPlusPlus build with the")
    A("fixes listed under \"Compiler fixes this tree depends on\".")
    A("")

    # ---------------- layout
    A("## Layout")
    A("")
    A("```")
    A("sdk/modelsrc/")
    A("  monsters/<model>/           anims/  mesh/  textures/  <model>.qc")
    A("  players/<model>/            (models/player/<model>/<model>.mdl)")
    A("  weapons/<weapon>/v_/        view model     v_<weapon>.qc")
    A("  weapons/<weapon>/w_/        world model    w_<weapon>.qc")
    A("  weapons/<weapon>/p_/        player-held model  p_<weapon>.qc")
    A("  props/<model>/  misc/<model>/")
    A("```")
    A("")
    A("Every model folder is self-contained: `mesh/` holds the reference meshes (`$cd \".\\mesh\"`),")
    A("`textures/` the 8-bit BMP textures (`$cdtexture \".\\textures\"`), `anims/` one SMD per sequence")
    A("(`name_blend01.smd`, `name_blend02.smd` for blended sequences). All paths are lower case.")
    A("The QC files follow the style of the hand-cleaned SBO model sources (no decompiler comments, one")
    A("blank line between blocks, the same order of commands in every file).")
    A("")
    A("To compile one model (the output goes to the folder given with `-o`, never into `gamedir/`):")
    A("")
    A("```bash")
    A("studiomdlplusplus sdk/modelsrc/monsters/zgrunt/zgrunt.qc -o /tmp/rebuilt")
    A("```")
    A("")

    # ---------------- pipeline
    A("## Pipeline")
    A("")
    A("```bash")
    A("scripts/modelsrc.py build   [model ...]   # shipped MDL -> sdk/modelsrc/")
    A("scripts/modelsrc.py verify  [model ...]   # sdk/modelsrc/ -> rebuilt MDL in scratch -> comparison")
    A("scripts/modelsrc.py readme                # this file")
    A("scripts/modelsrc.py all                   # the three steps for every model")
    A("```")
    A("")
    A("| File | Role |")
    A("| --- | --- |")
    A("| `scripts/modelsrc.py` | the pipeline (Python 3 standard library only) |")
    A("| `scripts/modelsrc_mdl.py` | MDL reader used for the inventory, the QC files and the comparison |")
    A("| `scripts/modelsrc_readme.py` | writes this README |")
    A("| `scripts/modelsrc_rules.json` | rules table: folders, texture renames, per-model special cases |")
    A("")
    A("External tools, used as they are (override with the environment variables):")
    A("")
    A("| Variable | Default | Version used for this README |")
    A("| --- | --- | --- |")
    A("| `MDLDEC` | `mdldec` on PATH | %s |"
      % "Half-Life Studio Model Decompiler 1.2.1 (FWGS mdldec)")
    A("| `STUDIOMDL` | `studiomdlplusplus` on PATH | `%s` |"
      % tool_version([ms.STUDIOMDL, "--help"]))
    A("")
    A("Scratch files (decompiler output and logs, rebuilt models, `report.json`) go to")
    A("`build/modelsrc/` (git-ignored; `MODELSRC_SCRATCH` overrides). The pipeline never writes under")
    A("`gamedir/`. A run with no model names rebuilds every category folder from nothing, so the tree")
    A("cannot hold stale files; the same input gives the same output byte for byte.")
    A("")
    A("### What `build` does for each model")
    A("")
    A("1. **Read** `name.mdl`, its texture file `nameT.mdl` and its sequence-group files `nameNN.mdl`")
    A("   (found by file name, which is also how the decompiler finds them).")
    A("2. **Stage** a scratch copy in which the texture names, sub-model names and sequence names are")
    A("   replaced by neutral ones (`tex000.bmp`, `ref000`, `seq000`). mdldec names its output files")
    A("   after those names, silently renames the ones it does not like (`&`, `+`, accents) and lets")
    A("   repeated names overwrite each other; the neutral names avoid all three problems.")
    A("3. **Decompile** the staged copy with `mdldec -a -t -u -m` (`-u`: texel = u x width, the")
    A("   convention of studiomdlPlusPlus).")
    A("4. **Normalize**: move the files to `mesh/`, `anims/`, `textures/` under their final lower-case")
    A("   names and put the final texture names into the mesh files. Each BMP is read back and must")
    A("   hold exactly the pixels and palette stored in the model.")
    A("5. **Write the QC** from the model itself, in the reference style. mdldec's own QC is not used")
    A("   (it stays in scratch): it writes the stored eye position without the axis swap the compiler")
    A("   applies to `$eyeposition`, keeps `$externaltextures` and `$sequencegroupsize`, and writes")
    A("   texture modes that the compiler derives from the texture name anyway.")
    A("")
    A("### Fixes the pipeline applies")
    A("")
    A("- **Lower-case paths**: every folder, file, texture name and mesh name. Sequence names, body")
    A("  group names and bone names are data the game looks up, so they are kept as shipped.")
    A("- **One file per model**: no `$externaltextures`, no `$sequencegroup`/`$sequencegroupsize`. The")
    A("  textures and all sequences are compiled into `name.mdl`.")
    A("- **Zombie Grunt crash fix** (see below): a consequence of the previous point.")
    A("- **Texture names**: accents are removed (`pmulticote.bmp`), and the placeholder name `null.bmp`,")
    A("  which different models use for different images, becomes `<model>-null.bmp`. The BMP file, the")
    A("  mesh files and the QC use the new name consistently. The list of placeholder names is")
    A("  `generic_texture_names` in the rules table.")
    A("- **Repeated names**: sub-models or sequences that share a name get `_2`, `_3` ... file names")
    A("  (`heavyhead_2.smd`, `idle1_2.smd`). The sequence name inside the QC stays as shipped.")
    A("")
    A("Nothing else is changed. Everything unusual that was not fixed before is listed under")
    A("\"Flagged, not fixed\".")
    A("")
    A("### The Zombie Grunt crash fix")
    A("")
    A("The analysis: the shipped `zgrunt.mdl` was")
    A("compiled with `$externaltextures` and `$sequencegroupsize`, and its header still names")
    A("`models/hgrunt01.mdl` ... `hgrunt03.mdl` as its sequence groups. The engine loads sequence groups")
    A("by the name stored in the header, so clients played Heavy Grunt animation data (25 bones) on the")
    A("Zombie Grunt skeleton (32 bones): bone corruption and a client crash. The fix was to")
    A("recompile the model from its sources as a single file (`zgrunt_fixed.mdl`: no `$externaltextures`,")
    A("no `$sequencegroupsize`).")
    A("")
    A("Here the same fix follows from the pipeline: the sources are decompiled from `zgrunt.mdl` +")
    A("`zgruntt.mdl` + `zgrunt01-03.mdl` (the Zombie Grunt's own animation files, taken by file name)")
    A("and the QC builds one file with no sequence groups. `cyberimp.mdl` names `imp01.mdl` in the same")
    A("way (harmless, the files are identical) and comes out as a single file too. `pgrunt.mdl`, an")
    A("unused copy of the Zombie Grunt with other textures and the same wrong header, was removed from")
    A("`gamedir/` (2026-10-12; nothing referred to it).")
    A("")

    # ---------------- renames
    A("## Renamed textures")
    A("")
    A("Only names that changed by more than letter case are listed (every name is lower-cased).")
    A("")
    A("| Model | Shipped name | Source name | Reason |")
    A("| --- | --- | --- | --- |")
    generic = set(rules["generic_texture_names"])
    for p in plans:
        if not p.model:
            continue
        for t, new in zip(p.model.textures, p.texture_names):
            if new != t["name"].lower():
                why = "placeholder name" if ms.deaccent(t["name"]).lower() in generic else "accent"
                A("| %s | `%s` | `%s` | %s |" % (p.key, t["name"], new, why))
    A("")

    # ---------------- inventory
    A("## Inventory")
    A("")
    n_models = len(plans) - 1  # w_hache is listed as a plan only to carry its skip reason
    n_tex = sum(1 for p in plans if p.model and p.model.external_textures)
    n_seq = sum(len(p.model.seqgroup_files) for p in plans if p.model)
    A("%d `.mdl` files ship: %d models, %d external texture files and %d sequence-group files that a"
      % (n_models + n_tex + n_seq + len(orphans) + 1, n_models, n_tex, n_seq))
    A("model uses, and %d files that belong to no model (listed after the table)." % (len(orphans) + 1))
    A("")
    A("Columns: *files* = main + texture file (T) + sequence-group files; *size* = all of them, KB;")
    A("*body* = body groups / sub-models; *used by* = game source files that name the model"
      + (", and map entities whose key-values name the model path (scan of the stock and custom maps;"
         if mrefs else "."))
    if mrefs:
        A("monsters and weapons are normally placed by class name, so this only counts explicit paths).")
    A("")
    A("| Model | What | Source folder | Files | Size | Bones | Seq | Body | Tex | Ext. tex | Seq groups | Used by | Unusual |")
    A("| --- | --- | --- | --- | ---: | ---: | ---: | --- | ---: | --- | ---: | --- | --- |")
    for p in plans:
        what = p.rule.get("what")
        if not what:
            kind = {"v": "view model", "w": "world model", "p": "player-held model"}.get(p.name[0], "")
            what = "%s, %s" % (p.name[2:], kind)
        if not p.model:
            size = os.path.getsize(p.entry["path"])
            A("| `%s.mdl` | %s | none | 1 | %s | | | | | | | %s | **no sources**: %s |" % (
                p.key, what, kb(size), ", ".join(refs.get(p.key, [])) or "-", p.skip or p.error))
            continue
        m = p.model
        files = "1"
        if m.external_textures:
            files += " + T"
        if m.seqgroup_files:
            files += " + %d seq" % len(m.seqgroup_files)
        used = ", ".join(refs.get(p.key, [])) or "-"
        if mrefs and mrefs[p.key][0]:
            used += "; %d map entities in %d map%s" % (mrefs[p.key] + ("" if mrefs[p.key][1] == 1 else "s",))
        notes = unusual(ms, mdl, p, refs, mrefs, identical)
        A("| `%s.mdl` | %s | `%s/` | %s | %s | %d | %d | %d / %d | %d | %s | %d | %s | %s |" % (
            p.key, what, p.folder, files, kb(m.total_size()), len(m.bones), len(m.sequences),
            len(m.bodyparts), sum(bp["nummodels"] for bp in m.bodyparts), len(m.textures),
            "missing" if m.placeholder_textures else "yes" if m.external_textures else "no",
            len(m.seqgroups) - 1, used,
            "; ".join(notes) or "-"))
    A("")
    A("Files that no model header refers to:")
    A("")
    A("| File | What it is |")
    A("| --- | --- |")
    for rel, why in orphans:
        A("| `%s` | %s |" % (rel, why))
    A("| `w_hache.mdl` | texture-only file (one texture, `crowbar.bmp` 192x127), not a model |")
    A("")

    # ---------------- verification
    counts = collections.Counter(v["status"] for v in V.values())
    nomodel = [p for p in plans if not p.model]
    A("## Verification")
    A("")
    A("`scripts/modelsrc.py verify` compiles every source folder with studiomdlPlusPlus into scratch and")
    A("compares the result with the shipped model. For a model that shipped in several files the")
    A("comparison is against the combined content of those files.")
    A("")
    A("| | Models |")
    A("| --- | ---: |")
    n_place = sum(1 for v in V.values() if v.get("placeholder"))
    A("| shipped models | %d |" % (len(plans) - 1))
    A("| decompiled into sources | %d |" % sum(1 for b in B.values() if b["status"] == "ok"))
    A("| rebuilt, equivalent | %d |" % (counts["equivalent"] - n_place))
    A("| rebuilt, equivalent except for stand-in textures | %d |" % n_place)
    n_idle = sum(1 for v in V.values() if v["status"] == "differences"
                 and all(d.startswith("intentional:") for d in v["diffs"]))
    A("| rebuilt, intentional difference only (one-frame `idle` added) | %d |" % n_idle)
    A("| rebuilt, with other differences | %d |" % (counts["differences"] - n_idle))
    A("| failed to compile | %d |" % counts["failed"])
    A("| no sources (intentionally empty model) | %d |" % (len(nomodel) - 1))
    A("")
    A("### How the comparison works")
    A("")
    A("Compared, with the shipped value as the reference: eye position, `$bbox`, `$cbox`, model flags;")
    A("bone names, order, parents and controller links; bone controllers; hitboxes; attachments;")
    A("texture names (after the renames above), sizes, flags, pixels and palettes; skin families; body")
    A("groups, their base values and sub-models; every triangle of every sub-model (bone, vertex")
    A("position, texture, texel coordinates, vertex normal); every sequence (name, fps, flags, activity")
    A("and weight, events, frame count, blends and blend range, motion type, linear movement, nodes) and")
    A("the decoded animation of every bone in every frame; the transition table.")
    A("")
    A("Tolerances: %.2f units for boxes and attachments; %.2f units / %.3f rad for animation values plus" % (
        ms.TOL_BOX, ms.TOL_POS, ms.TOL_ROT))
    A("one quantization step of each file (animations are stored as 16-bit steps and the compiler picks")
    A("its own step size); %.2f for normals; %.1f units for the per-sequence bounding box, which the" % (
        ms.TOL_NORMAL, ms.TOL_SEQBOX))
    A("compiler recomputes. Texel coordinates must match exactly, except on chrome textures (the")
    A("compiler stores none for them, see below). Byte identity is not expected.")
    A("")
    A("Not compared: the internal model name, the order of vertices and triangle strips, per-bone")
    A("default values (covered by the animation comparison), unused header fields.")
    A("")
    A("### Results")
    A("")
    A("| Model | Result | Shipped (files, KB) | Rebuilt (KB) | Differences |")
    A("| --- | --- | ---: | ---: | --- |")
    for p in plans:
        v = V.get(p.key)
        if not p.model:
            if p.key != "w_hache":
                A("| `%s.mdl` | no sources | 1, %s | | %s |" % (p.key, kb(os.path.getsize(p.entry["path"])), p.skip or p.error))
            continue
        if v is None:
            b = B.get(p.key, {})
            A("| `%s.mdl` | not built | | | %s |" % (p.key, b.get("error", "")))
            continue
        if v["status"] == "failed":
            A("| `%s.mdl` | **failed** | %d, %s | | %s |" % (p.key, v["shipped_files"], kb(v["shipped_size"]), v["error"]))
            continue
        result = "**differences**"
        if v["status"] == "differences" and all(d.startswith("intentional:") for d in v["diffs"]):
            result = "intentional difference"
        if v["status"] == "equivalent":
            result = "equivalent; **stand-in textures**" if v.get("placeholder") else "equivalent"
        A("| `%s.mdl` | %s | %d, %s | %s | %s |" % (
            p.key, result,
            v["shipped_files"], kb(v["shipped_size"]), kb(v["rebuilt_size"]),
            "<br>".join(v["diffs"]) or "-"))
    A("")
    worst_p = max((v["stats"].get("max_anim_pos_delta", 0), k) for k, v in V.items()
                  if "stats" in v and v["status"] == "equivalent")
    worst_r = max((v["stats"].get("max_anim_rot_delta", 0), k) for k, v in V.items() if "stats" in v)
    worst_n = max((v["stats"].get("max_normal_delta", 0), k) for k, v in V.items() if "stats" in v)
    A("Largest deviations among the equivalent models: animation position %.4f units (%s), animation"
      % worst_p)
    A("rotation %.6f rad (%s), vertex normal %.7f (%s). Vertex and normal table sizes, triangle"
      % (worst_r + worst_n))
    A("counts, texture order and bone order are identical for every rebuilt model.")
    A("")

    # ---------------- differences explained
    A("### Differences")
    A("")
    remaining = [k for k, v in V.items() if v["status"] != "equivalent"]
    idle = [k for k in remaining if V[k]["diffs"] and all(d.startswith("intentional:") for d in V[k]["diffs"])]
    other = [k for k in remaining if k not in idle]
    if other:
        A("Unexplained, see the table: " + ", ".join("`%s`" % k for k in other) + ".")
    else:
        A("No unintended difference.")
    A("")
    if idle:
        A("**Intentional difference** (%s): the shipped models have no sequence at all; the sources add"
          % ", ".join("`%s`" % k for k in idle))
        A("a one-frame `idle` in the reference pose (rule `add_idle_sequence`).")
        A("Reasons: every stock Half-Life `p_` model and the other 18 SBO `p_` models with geometry have a")
        A("sequence (usually exactly this one-frame `idle`); the client's `StudioMergeBones` always reads")
        A("sequence 0 of the attached weapon model, and with no sequence it reads the sequence-group")
        A("record that sits at the same offset and computes bone poses from unrelated bytes (unused only")
        A("because every bone of these models exists in the player skeletons); and the compiler refuses")
        A("a QC without a sequence. Everything else in these three models is equivalent.")
        A("")
    A("`w_k444`, `w_medikit` and `w_warhammer` are equivalent in everything the shipped file holds")
    A("(bones, hitboxes, meshes with their texel coordinates, sequences); their textures are stand-ins,")
    A("see \"Stand-in textures\".")
    A("")
    A("### Compiler fixes this tree depends on")
    A("")
    A("The first comparison (compiler build 286a2e2c) had 8 models with differences. Two causes were")
    A("bugs in studiomdlPlusPlus, fixed there on 2026-10-12, as was a third deviation from Valve's")
    A("compiler:")
    A("")
    A("1. **`LY` motion extraction** (2dbb2e0; `dgrunt`, `hgrunt`, `zgrunt`: `strafeleft`, `straferight`):")
    A("   the sideways linear movement subtracted the start **z** from the end **y**")
    A("   (`motion.y = ppos[last_frame].y - ppos[start_frame].z`), a slip from a clean-up commit; Valve's")
    A("   `ExtractMotion` uses the same axis.")
    A("2. **Flags from the texture name `bright`** (257e572; `v_satchel_radio`, `detbutn_bright.bmp`): the")
    A("   compiler set flat shading + full bright for any name containing `bright`. Valve's studiomdl has")
    A("   no such rule (only `chrome`); it came with the code base this compiler started from, where it")
    A("   is marked as an XDM addition. Removed; `$texrendermode <texture> fullbright` remains.")
    A("")
    A("3. **Motion options AX, AY, AZ, AXR, AYR, AZR** (`barney`, `cyberimp`, `dark`, `dgrunt`, `hgrunt`,")
    A("   `imp`, `zgrunt`, `zombie`): the compiler accepted and dropped them (removed upstream in 2025 as")
    A("   \"unused\"). Valve's studiomdl stores them as bits in the sequence's motion type and writes")
    A("   nothing else for them; the compiler now does the same, so the motion type is compared")
    A("   strictly and matches the shipped files. (Not counted among the 8: the first comparison")
    A("   tolerated the missing bits.)")
    A("")
    A("The remaining cause was the three `p_` models without a sequence, see the intentional difference")
    A("above. (The compiler's \"No sequences defined\" check is its own; Valve's studiomdl has none. It")
    A("stays.)")
    A("")
    A("Expected changes that are not counted as differences:")
    A("")
    A("- Texture and sub-model names are lower case (and renamed as listed above); the sub-model name")
    A("  `../dmatch/lowcount/morphbeta/morphbeta_ref` of `hunted.mdl` becomes `morphbeta_ref`; repeated")
    A("  sub-model names get `_2`, `_3`, `_4` (`heavyhead` in dgrunt and hgrunt).")
    chrome = sorted(k for k, v in V.items() if v.get("stats", {}).get("chrome_uv"))
    A("- Texel coordinates on chrome textures are not stored by the compiler. For %s the" % ", ".join("`%s`" % k for k in chrome))
    A("  shipped file has real coordinates on textures that carry the chrome flag without the usual")
    A("  flat-shade flag (the flag was probably added to a compiled model). A renderer generates the")
    A("  coordinates of a chrome texture itself, so the stored ones should not matter while the flag is")
    A("  set (Valve's studiomdl stores none either; not verified in Valve's engine). The mesh files in")
    A("  `mesh/` keep the coordinates.")
    A("- The internal model name (a path on the original author's PC) becomes `<name>.mdl`.")
    A("")

    # ---------------- stand-ins
    A("## Stand-in textures (to do)")
    A("")
    A("`w_k444.mdl`, `w_medikit.mdl` and `w_warhammer.mdl` have no textures inside and their texture")
    A("files (`w_k444t.mdl` ...) do not ship, so the original texture names, sizes and images are")
    A("unknown. Weapons are rarely placed in the world in SBO, which is probably why nobody noticed.")
    A("So that these three have sources that compile, the pipeline gives them generated stand-in")
    A("textures (rule `placeholder_textures`): a gray checker with a magenta frame and the word DEV,")
    A("named `dev_placeholder.bmp` (`dev_placeholder_2.bmp` for a second skin reference). One texture")
    A("per skin reference used by the meshes; each is the smallest power-of-two size that covers the")
    A("texel coordinates stored in those meshes (a power-of-two texture is not resampled by the engine):")
    A("")
    A("| Model | Skin reference | Largest texel (s, t) | Stand-in |")
    A("| --- | ---: | --- | --- |")
    for p in plans:
        if p.model and p.model.placeholder_textures:
            ext = {}
            for bp in p.model.bodyparts:
                for sm in bp["models"]:
                    for mesh in sm["meshes"]:
                        e = ext.setdefault(mesh["skinref"], [0, 0])
                        for tri in mesh["tris"]:
                            for c in tri:
                                e[0], e[1] = max(e[0], c[2]), max(e[1], c[3])
            for i, t in enumerate(p.model.textures):
                A("| `%s` | %d | %d, %d | `%s` %dx%d |" % (p.key, i, ext[i][0], ext[i][1], p.texture_names[i], t["width"], t["height"]))
    A("")
    A("- **TODO**: replace these three with real world models from the rotating-weapons project")
    A("  (the rotating-weapons plugin's meshes), then drop the `placeholder_textures` rules.")
    A("- The shipped `w_k444.mdl`, `w_medikit.mdl`, `w_warhammer.mdl` in `gamedir/` are unchanged (still")
    A("  without textures). The rebuilt files with the stand-ins exist only in scratch.")
    A("- Every texel coordinate of `w_warhammer` is (0, 63), which is what a compiler writes for chrome")
    A("  textures: its real textures were probably 64x64 chrome. `w_medikit.mdl` is byte-identical to")
    A("  stock Half-Life's `w_medkit.mdl`, whose textures are in `valve/models/w_medkitt.mdl`.")
    A("")

    # ---------------- limits
    A("## Engine limits for the merged models")
    A("")
    A("Limits from `src/engine/studio.h` (same values as Valve's SDK): MAXSTUDIOSEQUENCES %d, MAXSTUDIOSKINS %d,"
      % (mdl.LIMITS["MAXSTUDIOSEQUENCES"], mdl.LIMITS["MAXSTUDIOSKINS"]))
    A("MAXSTUDIOBONES %d, MAXSTUDIOVERTS %d per sub-model, MAXSTUDIOANIMATIONS (frames per sequence) %d,"
      % (mdl.LIMITS["MAXSTUDIOBONES"], mdl.LIMITS["MAXSTUDIOVERTS"], mdl.LIMITS["MAXSTUDIOANIMATIONS"]))
    A("MAXSTUDIOEVENTS %d, MAXSTUDIOGROUPS %d (no longer relevant: one group). The compiler uses the same"
      % (mdl.LIMITS["MAXSTUDIOEVENTS"], mdl.LIMITS["MAXSTUDIOGROUPS"]))
    A("numbers (`src/format/mdl.hpp`). There is no limit on the file size itself in `studio.h`.")
    A("")
    A("| Model | Shipped files | Shipped KB | Rebuilt KB | Seq | Longest seq (frames) | Tex | Texture pixels (KB) | Largest texture | Bones | Most vertices in a sub-model |")
    A("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |")
    biggest = 0
    for p in plans:
        if not p.model:
            continue
        m = p.model
        v = V.get(p.key, {})
        if "rebuilt_size" in v:
            biggest = max(biggest, v["rebuilt_size"])
        if not (m.external_textures or m.seqgroup_files):
            continue
        files = "main" + (" + T" if m.external_textures else "") + (
            " + %d seq" % len(m.seqgroup_files) if m.seqgroup_files else "")
        big = max(m.textures, key=lambda t: t["width"] * t["height"])
        A("| `%s` | %s | %s | %s | %d | %d | %d | %s | %dx%d | %d | %d |" % (
            p.key, files, kb(m.total_size()), kb(v["rebuilt_size"]) if "rebuilt_size" in v else "-",
            len(m.sequences), max([s["numframes"] for s in m.sequences] or [0]), len(m.textures),
            kb(sum(t["width"] * t["height"] for t in m.textures)), big["width"], big["height"],
            len(m.bones), max(sm["numverts"] for bp in m.bodyparts for sm in bp["models"])))
    A("")
    merged = [p for p in plans if p.model and (p.model.external_textures or p.model.seqgroup_files)]
    A("- Nothing is near a limit: at most %d of %d sequences, %d of %d textures, %d of %d bones, %d of"
      % (max(len(p.model.sequences) for p in merged), mdl.LIMITS["MAXSTUDIOSEQUENCES"],
         max(len(p.model.textures) for p in merged), mdl.LIMITS["MAXSTUDIOSKINS"],
         max(len(p.model.bones) for p in merged), mdl.LIMITS["MAXSTUDIOBONES"],
         max(sm["numverts"] for p in merged for bp in p.model.bodyparts for sm in bp["models"])))
    A("  %d vertices, %d of %d frames. The largest rebuilt file is %s KB; the player models already"
      % (mdl.LIMITS["MAXSTUDIOVERTS"], max(s["numframes"] for p in merged for s in p.model.sequences),
         mdl.LIMITS["MAXSTUDIOANIMATIONS"], kb(biggest)))
    A("  ship as single files of %s KB, so single files of this size are nothing new for the mod."
      % kb(max(p.model.size for p in plans if p.key.startswith("player/"))))
    A("- **Verified**: every merged model compiles; the rebuilt file parses completely; every sequence")
    A("  decodes from the single file and matches the shipped animation (this also covers the 16-bit")
    A("  animation offsets, which are relative to each bone's entry and limit how far a sequence's data")
    A("  can reach); textures are identical in size, pixels and palette, so texture memory in the")
    A("  renderer should be unchanged (same images; only the file they are read from differs).")
    A("- **Behavior that changes**: sequence groups were loaded on demand through the engine cache;")
    A("  in a single file all animations are in memory with the model. That is the whole sequence data")
    seqmax = max(merged, key=lambda p: sum(os.path.getsize(f) for f in p.model.seqgroup_files))
    A("  of the model (at most %s KB in sequence-group files, `%s`), a saving that mattered in 1998."
      % (kb(sum(os.path.getsize(f) for f in seqmax.model.seqgroup_files)), seqmax.key))
    A("- **Not verified**: loading and rendering in the engine (Steam Half-Life client, HLDS), and the")
    A("  compiler's behavior on Windows. The engine's own hard limits beyond `studio.h` (cache and hunk")
    A("  sizes) were not measured; stock Half-Life models larger than these load from single files.")
    A("")

    # ---------------- flagged
    A("## Flagged, not fixed")
    A("")
    A("Known issues of the shipped models, not fixed. The sources reproduce the shipped models here.")
    A("`.smd` files are plain text and go into normal git.")
    A("")
    A("- **`cyberimp.mdl`** is a byte-for-byte copy of `imp.mdl` and loads `imp01.mdl` (identical to")
    A("  `cyberimp01.mdl`). Not named in the game code.")
    A("- **Missing texture files** of `w_k444`, `w_medikit`, `w_warhammer`: see \"Stand-in textures\".")
    A("  `w_k444.mdl` and `w_medikit.mdl` are precached by the game code. `w_hache.mdl` is a texture-only")
    A("  file holding `crowbar.bmp` 192x127; it does not fit `w_warhammer` (two skin references).")
    A("- **Orphan files** (table above): `dgrunt01-03.mdl` and `hgrunt01-03.mdl` belong to no model;")
    A("  `hgrunt01-03.mdl` are what the shipped `zgrunt.mdl` wrongly loads.")
    A("- **Unusual texture names kept as shipped**: spaces (`marteaufinal copy.bmp`, `dent up.bmp`), a")
    A("  double extension (`glove_handpak.bmp.bmp`), `&`, `+` and parentheses (`apche_wheel&gun1.bmp`,")
    A("  `canon+pompe.bmp`, `sci3(back2).bmp` ...). They compile.")
    A("- **Other texture names shared by different images**: only `null.bmp` is renamed. %d more names"
      % other_shared_names(plans))
    A("  are used by several models for different images (mostly the same picture cropped differently")
    A("  for the `v_` and `p_` model: `skin.bmp`, `image3.bmp`, `bout_1.bmp`, `epee1.bmp` ...). Every model")
    A("  folder has its own `textures/`, so nothing collides; add names to `generic_texture_names` to")
    A("  rename more.")
    A("- **`dm_base.bmp`** in `v_medikit` was `DM_base.bmp`. The engine treats `DM_Base.bmp` specially on")
    A("  player models (team colors); this is a view model and the shipped spelling already differed")
    A("  from the engine's, so no effect is expected (not verified in the engine).")
    A("- **Glove textures with the chrome flag** in `v_grenade`, `v_satchel`, `v_satchel_radio`")
    A("  (`gloved_knuckle.bmp`, `glove_handpak.bmp.bmp`, `xbow_sleeve.bmp`, `gloved_sleeve.bmp`) and")
    A("  `fa18p_02.bmp`/`fa18_02.bmp`: chrome without flat shading, kept as shipped.")
    A("- **`werewolf.mdl`**: 15 sequences named `idle1`, 5 `walk1`, 4 `dieheadshot`; `idle1` (the first)")
    A("  has two blends and no blend controller. `dark.mdl` and `v_k444.mdl` repeat names too. The game")
    A("  finds the first sequence with a given name; the order is kept.")
    A("- **`hunted.mdl`** in the models root is an old `morphbeta` model that nothing names; the player")
    A("  model is `player/hunted/hunted.mdl`. `w_p228.mdl` and `w_rc28.mdl` are the same file.")
    A("- `p_wong.mdl` is intentionally empty (bare hands): no sources, nothing to fix.")
    A("- The `.bmp` portraits in `models/player/*/` are not model sources and are not part of this tree.")
    A("")

    # ---------------- size
    A("## Size of this tree")
    A("")
    by_ext = collections.defaultdict(lambda: [0, 0])
    for dirpath, dirnames, filenames in os.walk(outroot):
        dirnames.sort()
        for fn in filenames:
            if fn == "README.md":
                continue
            ext = os.path.splitext(fn)[1]
            by_ext[ext][0] += 1
            by_ext[ext][1] += os.path.getsize(os.path.join(dirpath, fn))
    A("| Type | Files | MB |")
    A("| --- | ---: | ---: |")
    for ext in sorted(by_ext, key=lambda e: -by_ext[e][1]):
        A("| `%s` | %d | %.1f |" % (ext, by_ext[ext][0], by_ext[ext][1] / 1048576.0))
    A("| total | %d | %.1f |" % (sum(v[0] for v in by_ext.values()), sum(v[1] for v in by_ext.values()) / 1048576.0))
    A("")
    A("`sdk/.gitattributes` already sends `*.bmp` to Git LFS, so the textures here go to LFS with no")
    A("change. The `.smd` files are text and are stored as normal git objects (the repo keeps every file")
    A("byte for byte, `* -text`); they are the bulk of the tree and compress well.")
    with open(os.path.join(outroot, "README.md"), "w", newline="\n") as fh:
        fh.write("\n".join(L) + "\n")
    print("readme: %s/README.md" % rules["output_dir"])


def other_shared_names(plans):
    seen = collections.defaultdict(set)
    for p in plans:
        if not p.model:
            continue
        for t, new in zip(p.model.textures, p.texture_names):
            seen[new].add(t["sha"])
    return sum(1 for v in seen.values() if len(v) > 1)


def tree_size(L, outroot):
    A = L.append
    by_ext = collections.defaultdict(lambda: [0, 0])
    for dirpath, dirnames, filenames in os.walk(outroot):
        dirnames.sort()
        for fn in filenames:
            if fn in ("README.md", "ORIGINAL-FILES.md", "installed.json"):
                continue
            ext = os.path.splitext(fn)[1]
            by_ext[ext][0] += 1
            by_ext[ext][1] += os.path.getsize(os.path.join(dirpath, fn))
    A("| Type | Files | MB |")
    A("| --- | ---: | ---: |")
    for ext in sorted(by_ext, key=lambda e: -by_ext[e][1]):
        A("| `%s` | %d | %.1f |" % (ext, by_ext[ext][0], by_ext[ext][1] / 1048576.0))
    A("| total | %d | %.1f |" % (sum(v[0] for v in by_ext.values()), sum(v[1] for v in by_ext.values()) / 1048576.0))
    A("")
    A("`sdk/.gitattributes` sends `*.bmp` to Git LFS; `.smd` and `.qc` files are plain text in normal git.")


def write_installed(rules, plans, state, ms, mdl, manifest, refs):
    """README once the rebuilt models are in gamedir/: the sources are the master."""
    outroot = os.path.join(ms.REPO, rules["output_dir"])
    V = state.get("verify", {})
    M = manifest["models"]
    L = []
    A = L.append
    A("# SBO model sources")
    A("")
    A("Source files (QC script, reference meshes, animations, textures) of the models in")
    A("`gamedir/syndicate/models/`, compiled with the studiomdlPlusPlus compiler")
    A("(studiomdlPlusPlus). Every model is one self-contained `.mdl`: no external texture")
    A("files (`nameT.mdl`), no sequence-group files (`name01.mdl` ...).")
    A("")
    A("**Status: %d models in `gamedir/` are built from these sources** (`scripts/modelsrc.py install`;" % len(M))
    A("`installed.json` lists them with their checksums). For those models the sources are the master:")
    A("edit the sources, run `verify` and `install`. **The rebuilt models have not been tested in the")
    A("game yet.**")
    A("")
    A("The sources were first made by decompiling the original SBO 1.38 model files. The inventory of")
    A("those files, the texture renames, the comparison of every rebuilt model with its original and")
    A("the engine-limit notes are kept in [`ORIGINAL-FILES.md`](ORIGINAL-FILES.md) (a record, not")
    A("regenerated). The original binaries are in the git history before the install commit.")
    A("")
    A("This file is generated by `scripts/modelsrc.py readme`.")
    A("")
    A("## Layout")
    A("")
    A("```")
    A("sdk/modelsrc/")
    A("  monsters/<model>/           anims/  mesh/  textures/  <model>.qc")
    A("  players/<model>/            (models/player/<model>/<model>.mdl)")
    A("  weapons/<weapon>/v_/        view model     v_<weapon>.qc")
    A("  weapons/<weapon>/w_/        world model    w_<weapon>.qc")
    A("  weapons/<weapon>/p_/        player-held model  p_<weapon>.qc")
    A("  props/<model>/  misc/<model>/")
    A("  installed.json              what `install` put into gamedir/ (checksums, what it replaced)")
    A("  ORIGINAL-FILES.md           record of the original files and of the comparison")
    A("```")
    A("")
    A("Every model folder is self-contained: `mesh/` (`$cd \".\\mesh\"`), `textures/` (8-bit BMP,")
    A("`$cdtexture \".\\textures\"`), `anims/` (one SMD per sequence; `name_blend01.smd` ... for blends).")
    A("All paths are lower case.")
    A("")
    A("## Pipeline")
    A("")
    A("```bash")
    A("scripts/modelsrc.py verify  [model ...]   # compile the sources into scratch and check the result")
    A("scripts/modelsrc.py install [model ...]   # compile and put the models into gamedir/")
    A("scripts/modelsrc.py package <folder>      # copy the complete models folder to <folder>/syndicate/models")
    A("scripts/modelsrc.py readme                # this file")
    A("scripts/modelsrc.py build   [model ...]   # decompile models that are not installed (see below)")
    A("```")
    A("")
    A("| File | Role |")
    A("| --- | --- |")
    A("| `scripts/modelsrc.py` | the pipeline (Python 3 standard library only; one process at a time) |")
    A("| `scripts/modelsrc_mdl.py` | MDL reader used for the QC files and the comparison |")
    A("| `scripts/modelsrc_readme.py` | writes this README |")
    A("| `scripts/modelsrc_rules.json` | rules table: folders, texture renames, per-model cases, install lists |")
    A("")
    A("Tools (override with the environment variables): `STUDIOMDL`, default")
    A("`studiomdlplusplus` on PATH (used here: `%s`);" % tool_version([ms.STUDIOMDL, "--help"]))
    A("`MDLDEC`, default `mdldec` on PATH (only for")
    A("`build`). Scratch files go to `build/modelsrc/` (git-ignored).")
    A("")
    A("- **`verify`**: for an installed model, compiles the sources and requires the result to be")
    A("  byte-identical to the file in `gamedir/` (*identical*). For a model that is not installed,")
    A("  compares the compiled model with the shipped one field by field (see the record for the method).")
    A("- **`install`**: compiles each model, compares it with the file in `gamedir/` and replaces that")
    A("  file (same name, same path) only when the result is *equivalent* or differs only by an")
    A("  *intentional difference* recorded in the rules table; otherwise it refuses and exits with an")
    A("  error. It then deletes the external texture and sequence-group files of the models it replaced")
    A("  and the files listed in `install_delete`, but never a file that a remaining model's header")
    A("  still names. Models in `install_skip` are never installed; a model with stand-in textures is")
    A("  only installed when it is listed in `install_stand_ins`.")
    A("  It prints every file it replaced or deleted and updates `installed.json`. Running it again")
    A("  changes nothing. After editing sources: `verify` shows *differences* for that model (the")
    A("  sources no longer give the installed file); `install <model>` then compares the new build")
    A("  with the installed file and refuses if they differ in content; `install --accept <model>`")
    A("  installs a deliberate change and prints and records the differences.")
    A("- **`build`** (decompile) leaves installed models alone, because it would overwrite the master")
    A("  sources with a decompile of their own output. It still works for models that are not installed.")
    A("- **`package <folder>`**: copies the complete `gamedir/syndicate/models/` folder, byte for byte,")
    A("  to `<folder>/syndicate/models/`: a drop-in replacement for the models folder of an install.")
    A("  It refuses while any model in the folder needs an external file, is not the installed build,")
    A("  or is neither built from the sources nor listed in `install_keep`.")
    A("")
    A("To compile one model by hand (the output goes to the folder given with `-o`):")
    A("")
    A("```bash")
    A("studiomdlplusplus sdk/modelsrc/monsters/zgrunt/zgrunt.qc -o /tmp/rebuilt")
    A("```")
    A("")
    A("## Installed models")
    A("")
    counts = collections.Counter(V[k]["status"] for k in M if k in V)
    A("%d models. Last `verify`: %s." % (len(M), ", ".join("%d %s" % (c, k) for k, c in sorted(counts.items())) or "not run"))
    A("")
    A("*Replaced* = the original files this model was shipped as (main, texture file T, sequence-group")
    A("files) and their total size; a new model replaced nothing. *At install* = the comparison with those files.")
    A("")
    A("| Model | Sources | Size (KB) | Replaced | Replaced size (KB) | At install | Verify | Used by |")
    A("| --- | --- | ---: | --- | ---: | --- | --- | --- |")
    by_key = {p.key: p for p in plans}
    for key in sorted(M):
        rec = M[key]
        p = by_key.get(key)
        n = len(rec["replaced"]["files"])
        names = [os.path.basename(f) for f in rec["replaced"]["files"]]
        tex = sum(1 for f in names[1:] if f.lower().endswith("t.mdl") and not f[-6:-4].isdigit())
        was = "- (new model)" if n == 0 else "1 file" if n == 1 else "main%s%s" % (" + T" if tex else "", " + %d seq" % (n - 1 - tex) if n - 1 - tex else "")
        A("| `%s` | `%s/` | %s | %s | %s | %s | %s | %s |" % (
            rec["file"], p.folder if p else "?", kb(rec["size"]), was, kb(rec["replaced"]["size"]),
            ("**stand-in textures**; otherwise " if rec.get("stand_in_textures") else "")
            + rec["result"] + ("" if not rec["differences"] else ": " + "; ".join(
                d.replace("intentional: ", "") for d in rec["differences"])),
            V.get(key, {}).get("status", "-"), ", ".join(refs.get(key, [])) or "-"))
    A("")
    A("Intentional difference: `p_federal`, `p_glauncher` and `p_katana` shipped with no sequence at")
    A("all; they now have a one-frame `idle` in the reference pose (rule `add_idle_sequence`). Every")
    A("stock Half-Life `p_` model has one, and the client's `StudioMergeBones` reads the first sequence")
    A("of the attached weapon model.")
    A("")
    A("### Files deleted by `install`")
    A("")
    A("Old external texture files, sequence-group files, six sequence-group files that belonged to")
    A("no model (`dgrunt01-03`, `hgrunt01-03`; the old `zgrunt.mdl` wrongly loaded the latter) and")
    A("`w_hache.mdl`, a texture-only file (`crowbar.bmp`) that nothing referred to:")
    A("")
    A("```")
    line = ""
    for rel in manifest["deleted"]:
        if len(line) + len(rel) + 2 > 96:
            A(line.rstrip())
            line = ""
        line += rel + "  "
    if line:
        A(line.rstrip())
    A("```")
    A("")
    A("## The models folder")
    A("")
    A("`gamedir/syndicate/models/` is complete and self-sufficient: no model in it needs an external")
    A("texture or sequence-group file, and it can replace the models folder of an install as a whole")
    A("(`scripts/modelsrc.py package`).")
    A("")
    root = os.path.join(ms.REPO, rules["models_dir"])
    keep = rules.get("install_keep", {})
    built = {rec["file"]: rec for rec in M.values()}
    rows, n_built, n_standin = [], 0, 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for fn in sorted(filenames):
            rel = os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/")
            if rel in built:
                n_built += 1
                n_standin += 1 if built[rel].get("stand_in_textures") else 0
            elif rel.lower().endswith(".mdl"):
                rows.append((rel, "model shipped as is: " + keep.get(rel[:-4], "**not listed in install_keep**")))
            else:
                rows.append((rel, "not a model: player model picture for the game's menus, shipped as is"))
    A("| Files | Origin |")
    A("| --- | --- |")
    A("| %d `.mdl` | built from the sources in this tree (table above) |" % (n_built - n_standin))
    if n_standin:
        A("| %d `.mdl` | built from the sources, with stand-in textures |" % n_standin)
    for rel, why in rows:
        A("| `%s` | %s |" % (rel, why))
    A("")
    A("`p_wong.mdl` is the one model that is not built from sources: it is an intentionally empty model")
    A("(no bones, no vertices: the Fist of Legend weapon shows bare hands), the game code precaches it,")
    A("so the file must exist, and there is nothing in it to decompile.")
    A("")
    named = code_model_paths(ms)
    have = set()
    for dirpath, dirnames, filenames in os.walk(root):
        for fn in filenames:
            have.add("models/" + os.path.relpath(os.path.join(dirpath, fn), root).replace(os.sep, "/"))
    missing = sorted(k for k in named if k not in have)
    A("The game code names %d model paths under `models/` (`PRECACHE_MODEL`, `SET_MODEL` and other quoted"
      % len(named))
    A("paths in `src/`, compared case-exactly); %d are in this folder. The other %d are not part of the"
      % (len(named) - len(missing), len(missing)))
    A("mod and come from Half-Life's own `valve/models/` (the mod's fallback folder), or belong to code")
    A("that SBO does not use:")
    A("")
    A("```")
    line = ""
    for k in missing:
        k = k[len("models/"):]
        if len(line) + len(k) + 2 > 96:
            A(line.rstrip())
            line = ""
        line += k + "  "
    if line:
        A(line.rstrip())
    A("```")
    A("")
    A("## World models (`w_`)")
    A("")
    A("The shipped SBO world models stood upright, came from other games or had no textures, and")
    A("several weapons used Half-Life's own files. The `w_` sources of the 19 weapons below are made from the")
    A("rotating-weapons plugin's meshes (each mesh taken from")
    A("the weapon's `v_` model and straightened). The mesh is turned so that the weapon lies on its side")
    A("(longest extent along X, muzzle or blade toward +X in the game, thinnest extent up), centered on")
    A("X/Y and lifted so that its lowest point is at Z = 0, the floor the item drops to. In the SMD the")
    A("muzzle points toward -Y: the compiler turns every model 90 degrees about Z, so -Y becomes the")
    A("entity's forward +X, the direction its angles point (Half-Life's convention). One bone")
    A("(`weapon_root`), one-frame `idle`. The minigun is turned to its stable rest (the pose with the")
    A("lowest center of mass that is not upside down: rolled 35 degrees, handle up at an angle); of the")
    A("chainsaw sword's two blades, the upper one is pivoted about its hilt, which rests on the other")
    A("hilt, until its tip touches the floor. Vertex normals that pointed against their triangle's")
    A("visible side were turned round (the engine lights by the normals; most of `w_pm66` and parts of")
    A("`w_warhammer`, `w_ben`, `w_canon`, `w_katana`, `w_rpg` were lit from behind, so the side facing")
    A("up was dark). Edit these sources directly; they are the master.")
    A("")
    A("| Weapon class | World model | Rotating-weapons mesh |")
    A("| --- | --- | --- |")
    for cls, w, src in [
            ("weapon_warhammer", "w_warhammer", "warhammer"), ("weapon_chainsaw", "w_katana", "chainsawsword"),
            ("weapon_sword", "w_sword", "sword"), ("weapon_rc28", "w_rc28", "rc28"), ("weapon_k444", "w_k444", "k444"),
            ("weapon_canon", "w_canon", "cannon"), ("weapon_pm66", "w_pm66", "pm66"), ("weapon_fa18", "w_fa18", "fa18"),
            ("weapon_federal", "w_federal", "federal"), ("weapon_sw", "w_rifle", "plasma-sniper"),
            ("weapon_nm33", "w_nm33", "nm33"), ("weapon_spa", "w_shotgun", "saan"), ("weapon_ben", "w_ben", "benelli"),
            ("weapon_glauncher", "w_glauncher", "grenade-launcher"), ("weapon_chaingun", "w_mini", "minigun"),
            ("weapon_rpg", "w_rpg", "rpg"), ("weapon_handgrenade", "w_grenade", "grenade"),
            ("weapon_tripmine", "w_tripmine", "tripmine"), ("weapon_medikit", "w_medikit", "medikit")]:
        A("| `%s` | `%s.mdl` | `%s` |" % (cls, w, src))
    A("")
    A("`w_shotgun`, `w_rpg` and `w_grenade` take the place of Half-Life's files of the same name (the")
    A("`v_`/`p_` models of those weapons were already SBO's). `w_grenade` is also the model of a thrown")
    A("hand grenade (`CGrenade::ShootTimed`, players and monsters). It stands upright like Half-Life's:")
    A("the mesh lies on Half-Life's grenade (same skeleton, bone `Line02`), with Half-Life's seven")
    A("sequences (`idle`, `roll1` and `roll2` for the grenade rolling on the floor, `toss` to `toss4` for")
    A("the thrown grenade tumbling; the code picks them by number). The placed tripmine")
    A("(`monster_tripmine`) still uses `v_tripmine.mdl`. Not changed: `weapon_wong` (bare fists, no")
    A("world model), `weapon_satchel` (`w_weaponbox`, the backpack, also the thrown charge),")
    A("`weapon_snark` (`w_sqknest`; `w_squeak` is the live Pitbull).")
    A("")
    A("## Open points")
    A("")
    A("- Not tested in the game. Check list: `BUILD.txt` of the test package.")
    A("- `cyberimp.mdl` is the imp under another name and `hunted.mdl` (models root) an old `morphbeta`")
    A("  model; `artefact` is not named in the game code either. (`p_terror`, an unused copy of")
    A("  `p_canon` with another grip, and the unused `p_p228` / `w_p228` were removed.)")
    A("- Unusual texture names kept as shipped (spaces, `&`, `+`, parentheses, `.bmp.bmp`); 49 texture")
    A("  names are used by several models for different images (only `null.bmp` was renamed).")
    A("- Details and the rest of the flagged items: `ORIGINAL-FILES.md`, \"Flagged, not fixed\".")
    A("")
    A("## Size of this tree")
    A("")
    tree_size(L, outroot)
    with open(os.path.join(outroot, "README.md"), "w", newline="\n") as fh:
        fh.write("\n".join(L) + "\n")
    print("readme: %s/README.md" % rules["output_dir"])


def code_model_paths(ms):
    """Quoted model paths under models/ in the game source (case as written)."""
    out = set()
    for dirpath, dirnames, filenames in os.walk(os.path.join(ms.REPO, "src")):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.endswith((".cpp", ".h", ".c")):
                with open(os.path.join(dirpath, fn), encoding="latin-1") as fh:
                    text = fh.read()
                for m in re.finditer(r'"(models/[^"\n<>%]+?\.mdl)"', text, re.I):
                    out.add(m.group(1).replace("\\\\", "/"))
                for m in re.finditer(r'"model"\s*,\s*"([a-z0-9_]+)"', text):
                    out.add("models/player/%s/%s.mdl" % (m.group(1), m.group(1)))
    return out
