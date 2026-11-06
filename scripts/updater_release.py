#!/usr/bin/env python3
"""Release files for the in-game updater (docs/UPDATER.md).

  updater_release.py installed <syndicate dir> --version V [--commit C] -o installed.json
      the file list a package ships as syndicate/updater/installed.json (path, size, sha256)
  updater_release.py manifest <release zip> --version V [--commit C] [--remove FILE] [--previous FILE ...]
                     -o manifest.json
      every file under syndicate/ in the zip: path, size, sha256 and where its data is in the zip (offset, length,
      store / deflate), so the game downloads exactly the files that changed with HTTP Range requests.
      --previous: earlier releases' file lists (manifest.json, installed.json or a package's manifest.sha256): the
      files players edit (KEEP_USER_CHANGES) list the versions those shipped (shipped_sha256), so an install without
      installed.json still gets an unedited one replaced; their names that differ from this release's only in case
      (a case-only rename) go on the remove list, so a case-sensitive install (Linux) does not keep both names
      --remove: game paths to delete (old files no installed.json lists); a case-only variant of a shipped name is
      allowed (the game removes it only where it is a separate file, never the new file on Windows)
  updater_release.py sign <manifest.json> (--key PEM_FILE | --key-env VAR) [-o manifest.json.sig]
      Ed25519 signature (64 bytes) with openssl; the key is never printed (VAR holds the PEM text, e.g. the CI
      variable SBO_UPDATE_SIGNING_KEY)
  updater_release.py verify <manifest.json> <manifest.json.sig> --pubkey HEX
  updater_release.py pubkey (--key PEM_FILE | --key-env VAR)
      the public key as the 64 hex digits CMake's SBO_UPDATE_PUBKEY takes
  updater_release.py zip <folder> -o <zip>
      a zip of <folder>'s contents (deflate), for test releases
  updater_release.py publish <tag> <dir> --notes NOTES.md [--prerelease] [--title T] [--repo owner/name]
      a draft release <tag> with every sbo-*.zip, SHA256SUMS, manifest.json and manifest.json.sig in <dir>; once
      every asset is on it (name and size checked) it is published in one step and marked Latest
      (releases/latest/download/... points at it); a --prerelease is never Latest; --repo defaults to
      SBO_UPDATE_REPO in src/cl_dll/updater/updater.cmake (the one place the repository is named)

Files players edit keep their changes on update (keep_user_changes): KEEP_USER_CHANGES below.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREFIX = "syndicate/"
# these keep a player's edits; the new version is written beside them as <name>.new
KEEP_USER_CHANGES = {"server.cfg", "listenserver.cfg", "mapcycle.txt", "motd.txt", "user.scr", "settings.scr"}
VERSION_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[0-9A-Za-z.-]+)?$")


def die(msg):
    sys.stderr.write(msg + "\n")
    sys.exit(1)


def check_version(v):
    if not VERSION_RE.match(v):
        die("bad version %r (x.y.z or x.y.z-pre)" % v)


def safe_path(p):
    parts = p.split("/")
    return (p and not p.startswith("/") and "\\" not in p and ":" not in p and len(p) < 200
            and all(x not in ("", ".", "..") and not x.endswith((".", " ")) for x in parts)
            and parts[0].lower() != "updater" and all(0x20 <= ord(c) < 0x7F for c in p))


def file_entry(path, data):
    e = {"path": path, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if path.lower() in KEEP_USER_CHANGES:
        e["keep_user_changes"] = True
    return e


def cmd_installed(a):
    check_version(a.version)
    files = []
    for dirpath, dirs, names in os.walk(a.dir):
        dirs.sort()
        for n in sorted(names):
            full = os.path.join(dirpath, n)
            rel = os.path.relpath(full, a.dir).replace(os.sep, "/")
            if rel.split("/")[0].lower() == "updater" or rel == "config.cfg":
                continue
            if not safe_path(rel):
                die("unsafe path in the game folder: %r" % rel)
            with open(full, "rb") as f:
                files.append(file_entry(rel, f.read()))
    files.sort(key=lambda e: e["path"])
    write_json(a.out, {"format": 1, "version": a.version, "commit": a.commit or "", "files": files})


def write_json(path, obj):
    text = json.dumps(obj, indent=1, sort_keys=False) + "\n"
    with open(path, "w", newline="\n") as f:
        f.write(text)


def cmd_manifest(a):
    check_version(a.version)
    zsize = os.path.getsize(a.zip)
    files, seen = [], set()
    with open(a.zip, "rb") as raw, zipfile.ZipFile(a.zip) as z:
        for info in z.infolist():
            if info.is_dir() or not info.filename.startswith(PREFIX):
                continue
            rel = info.filename[len(PREFIX):]
            if rel.split("/")[0].lower() == "updater" or rel == "config.cfg":
                continue
            if not safe_path(rel):
                die("unsafe path in the zip: %r" % info.filename)
            if rel.lower() in seen:
                die("duplicate (case-insensitive) path: %r" % rel)
            seen.add(rel.lower())
            if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                die("%s: compression %d (only store / deflate)" % (rel, info.compress_type))
            if info.flag_bits & 0x1:
                die("%s: encrypted" % rel)
            # the data starts after the LOCAL header (its name / extra lengths may differ from the central directory)
            raw.seek(info.header_offset)
            hdr = raw.read(30)
            sig, _, _, _, _, _, _, _, _, nlen, xlen = struct.unpack("<IHHHHHIIIHH", hdr)
            if sig != 0x04034B50:
                die("%s: bad local header" % rel)
            off = info.header_offset + 30 + nlen + xlen
            data = z.read(info)
            e = file_entry(rel, data)
            e["zip_offset"] = off
            e["zip_size"] = info.compress_size
            e["method"] = "deflate" if info.compress_type == zipfile.ZIP_DEFLATED else "store"
            if off + info.compress_size > zsize:
                die("%s: data past the end of the zip" % rel)
            files.append(e)
    # earlier shipped versions of the kept files; earlier names that differ from this release's only in case
    exact = {e["path"] for e in files}
    shipped, renamed = {}, {}
    for prev in a.previous or []:
        for path, sha in read_file_list(prev):
            if path.lower() in KEEP_USER_CHANGES:
                shipped.setdefault(path.lower(), set()).add(sha)
            if path.lower() in seen and path not in exact and safe_path(path):
                renamed.setdefault(path, sha)
    for e in files:
        if e.get("keep_user_changes"):
            old = sorted(h for h in shipped.get(e["path"].lower(), ()) if h != e["sha256"])
            if old:
                e["shipped_sha256"] = old
    # remove entries: old files no installed.json lists. A file of this release under another case (a case-only
    # rename) is allowed: the game removes the old name only where it is a file of its own (a case-sensitive file
    # system), never the new file on Windows
    remove = []
    if a.remove:
        with open(a.remove) as f:
            for line in f:
                p = line.strip()
                if not p or p.startswith("#"):
                    continue
                if not safe_path(p) or p in exact:
                    die("bad remove entry %r" % p)
                remove.append({"path": p})
    listed = {r["path"] for r in remove}
    for p in sorted(renamed):
        if p not in listed:
            remove.append({"path": p, "sha256": renamed[p]})
    files.sort(key=lambda e: e["path"])
    write_json(a.out, {"format": 1, "version": a.version, "commit": a.commit or "", "zip": os.path.basename(a.zip),
                       "zip_size": zsize, "files": files, "remove": remove})
    print("%s: %d files, %d to remove, zip %d bytes" % (a.out, len(files), len(remove), zsize))


def read_file_list(path):
    """(path, sha256) pairs of manifest.json / installed.json / manifest.sha256 (sha256sum format)"""
    with open(path) as f:
        text = f.read()
    if text.lstrip().startswith("{"):
        return [(e["path"], e["sha256"]) for e in json.loads(text)["files"]]
    out = []
    for line in text.splitlines():
        m = re.match(r"^([0-9a-f]{64}) [ *](.+)$", line.strip())
        if m:
            out.append((m.group(2), m.group(1)))
    return out


class KeyFile:
    """The private key as a file openssl can read: the given file, or a 0600 temp file from an environment variable"""

    def __init__(self, a):
        self.a, self.tmp = a, None

    def __enter__(self):
        if self.a.key:
            return self.a.key
        pem = os.environ.get(self.a.key_env or "", "")
        if "PRIVATE KEY" not in pem:
            die("no key: %s is not set or holds no PEM private key" % (self.a.key_env or "--key-env"))
        fd, self.tmp = tempfile.mkstemp(prefix="sbo-key-")
        os.chmod(self.tmp, 0o600)
        with os.fdopen(fd, "w") as f:
            f.write(pem if pem.endswith("\n") else pem + "\n")
        return self.tmp

    def __exit__(self, *exc):
        if self.tmp:
            os.remove(self.tmp)


def openssl(args, inp=None):
    if not shutil.which("openssl"):
        die("openssl is needed")
    r = subprocess.run(["openssl"] + args, input=inp, capture_output=True)
    if r.returncode != 0:
        # openssl's own message, never the key
        lines = r.stderr.decode(errors="replace").strip().splitlines()
        die("openssl %s failed: %s" % (args[0], lines[-1] if lines else "?"))
    return r.stdout


def cmd_sign(a):
    out = a.out or a.manifest + ".sig"
    with KeyFile(a) as key:
        openssl(["pkeyutl", "-sign", "-rawin", "-inkey", key, "-in", a.manifest, "-out", out])
    if os.path.getsize(out) != 64:
        die("unexpected signature size")
    print("signed: %s" % out)


def cmd_pubkey(a):
    with KeyFile(a) as key:
        der = openssl(["pkey", "-in", key, "-pubout", "-outform", "DER"])
    print(der[-32:].hex())


def cmd_verify(a):
    pub = bytes.fromhex(a.pubkey)
    if len(pub) != 32:
        die("the public key is 64 hex digits")
    # SubjectPublicKeyInfo for Ed25519 + the raw key
    der = bytes.fromhex("302a300506032b6570032100") + pub
    with tempfile.TemporaryDirectory() as t:
        p = os.path.join(t, "pub.der")
        with open(p, "wb") as f:
            f.write(der)
        r = subprocess.run(["openssl", "pkeyutl", "-verify", "-rawin", "-pubin", "-keyform", "DER", "-inkey", p,
                            "-in", a.manifest, "-sigfile", a.sig], capture_output=True)
    if r.returncode != 0:
        die("signature does NOT match")
    print("signature OK")


def cmd_zip(a):
    n = 0
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for dirpath, dirs, names in os.walk(a.folder):
            dirs.sort()
            for name in sorted(names):
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, a.folder).replace(os.sep, "/")
                z.write(full, rel)
                n += 1
    print("%s: %d files" % (a.out, n))


def default_repo():
    with open(os.path.join(ROOT, "src/cl_dll/updater/updater.cmake")) as f:
        m = re.search(r'set\(SBO_UPDATE_REPO "([^"]*)"', f.read())
    if not m or not m.group(1):
        die("no SBO_UPDATE_REPO default in src/cl_dll/updater/updater.cmake; pass --repo")
    return m.group(1)


def cmd_publish(a):
    repo = a.repo or default_repo()
    files = sorted(os.path.join(a.dir, f) for f in os.listdir(a.dir)
                   if (f.startswith("sbo-") and f.endswith(".zip")) or f in ("manifest.json", "manifest.json.sig", "SHA256SUMS"))
    names = [os.path.basename(f) for f in files]
    if "manifest.json" not in names or "manifest.json.sig" not in names:
        die("%s needs manifest.json and manifest.json.sig" % a.dir)
    with open(os.path.join(a.dir, "manifest.json")) as f:
        m = json.load(f)
    if a.tag != "v" + m["version"]:
        die("tag %s does not match the manifest's version %s" % (a.tag, m["version"]))
    if m["zip"] not in names:
        die("the manifest names %s, which is not in %s" % (m["zip"], a.dir))
    # a draft first (invisible to players and to releases/latest), every asset uploaded and checked, then published
    # in one step: a release is never visible without its assets
    cmd = ["gh", "release", "create", a.tag, "--repo", repo, "--draft", "--title", a.title or m["version"],
           "--notes-file", a.notes] + (["--prerelease"] if a.prerelease else []) + files
    print("gh release create %s --repo %s --draft (%s)" % (a.tag, repo, ", ".join(names)))
    if subprocess.run(cmd).returncode != 0:
        die("gh release create failed; the draft (if any) stays unpublished")
    r = subprocess.run(["gh", "release", "view", a.tag, "--repo", repo, "--json", "assets"], capture_output=True, text=True)
    if r.returncode != 0:
        die("gh release view failed; the draft stays unpublished")
    up = {x["name"]: x["size"] for x in json.loads(r.stdout)["assets"] if x.get("state", "uploaded") == "uploaded"}
    for f in files:
        if up.get(os.path.basename(f)) != os.path.getsize(f):
            die("asset %s missing or of another size on the draft; the draft stays unpublished" % os.path.basename(f))
    pub = ["gh", "release", "edit", a.tag, "--repo", repo, "--draft=false"] + (
        ["--prerelease", "--latest=false"] if a.prerelease else ["--latest"])
    print("publishing %s (%d assets checked)%s" % (a.tag, len(files), "" if a.prerelease else ", marked Latest"))
    sys.exit(subprocess.run(pub).returncode)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("installed"); s.add_argument("dir"); s.add_argument("--version", required=True)
    s.add_argument("--commit"); s.add_argument("-o", "--out", required=True); s.set_defaults(fn=cmd_installed)
    s = sub.add_parser("manifest"); s.add_argument("zip"); s.add_argument("--version", required=True)
    s.add_argument("--commit"); s.add_argument("--remove"); s.add_argument("--previous", action="append")
    s.add_argument("-o", "--out", required=True)
    s.set_defaults(fn=cmd_manifest)
    for name, fn in (("sign", cmd_sign), ("pubkey", cmd_pubkey)):
        s = sub.add_parser(name)
        if name == "sign":
            s.add_argument("manifest"); s.add_argument("-o", "--out")
        g = s.add_mutually_exclusive_group(required=True)
        g.add_argument("--key"); g.add_argument("--key-env")
        s.set_defaults(fn=fn)
    s = sub.add_parser("verify"); s.add_argument("manifest"); s.add_argument("sig"); s.add_argument("--pubkey", required=True)
    s.set_defaults(fn=cmd_verify)
    s = sub.add_parser("zip"); s.add_argument("folder"); s.add_argument("-o", "--out", required=True); s.set_defaults(fn=cmd_zip)
    s = sub.add_parser("publish"); s.add_argument("tag"); s.add_argument("dir"); s.add_argument("--notes", required=True)
    s.add_argument("--prerelease", action="store_true"); s.add_argument("--title"); s.add_argument("--repo")
    s.set_defaults(fn=cmd_publish)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
