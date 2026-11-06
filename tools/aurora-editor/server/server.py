#!/usr/bin/env python3
"""SBO Aurora particle editor: static files, the shipped assets and per-session uploads.

Python standard library only (tools/aurora-editor/README.md). Routes:
  GET  /                                   the editor (web/)
  GET  /api/assets                         the scripts and sprites the image carries (JSON)
  GET  /assets/<aurora|sprites>/<name>     one of them; the mod's files before Half-Life's (as the game searches)
  POST /api/session                        a new upload session -> {"id": ...}
  GET  /api/session/<id>                   that session's files (JSON)
  PUT  /api/session/<id>/<aurora|sprites>/<name>   upload (raw body)
  GET / DELETE /api/session/<id>/<aurora|sprites>/<name>

Uploads live in AURORA_DATA (a temporary folder in the container), are checked (name, type, size) and go away
when the session is idle for SESSION_TTL seconds or the container stops.
"""

import json
import os
import re
import secrets
import shutil
import threading
import time
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlsplit

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
ASSETS = os.path.join(ROOT, "assets")
# search order of the game: the mod, then Half-Life
ASSET_SOURCES = [("sbo", os.path.join(ASSETS, "sbo")), ("valve", os.path.join(ASSETS, "valve"))]
DATA = os.environ.get("AURORA_DATA", "/tmp/aurora-sessions")
PORT = int(os.environ.get("AURORA_PORT", "8095"))

MAX_SCRIPT = 256 * 1024
MAX_SPRITE = 4 * 1024 * 1024
MAX_SESSION_BYTES = 32 * 1024 * 1024
MAX_SESSION_FILES = 64
MAX_SESSIONS = 200
SESSION_TTL = 12 * 3600

KINDS = {"aurora": (".aur", MAX_SCRIPT), "sprites": (".spr", MAX_SPRITE)}
NAME_RE = re.compile(r"^[a-z0-9_\-][a-z0-9_.\-]{0,59}$")
SESSION_RE = re.compile(r"^[A-Za-z0-9_\-]{16,64}$")

_lock = threading.Lock()


def clean_name(name, kind):
    """A plain lower-case file name with the kind's extension, or None (no folders, no '..', no odd characters)."""
    name = name.strip().lower()
    ext = KINDS[kind][0]
    if not NAME_RE.match(name) or ".." in name or not name.endswith(ext) or len(name) <= len(ext):
        return None
    return name


def check_content(kind, body):
    if kind == "sprites":
        if len(body) < 42 or body[:4] != b"IDSP":
            return "not a GoldSrc sprite (no IDSP header)"
        version = int.from_bytes(body[4:8], "little")
        if version != 2:
            return "sprite version %d: only version 2 (Half-Life) sprites are read" % version
        return None
    if b"\0" in body:
        return "not a text file"
    return None


def session_dir(sid):
    return os.path.join(DATA, sid)


# HUD / UI sprites (hidden in the sprite browser unless "Show HUD / UI sprites"): the ones a hud.txt or weapon_*.txt
# names, and these names (HUD sheets and resolution folders, crosshairs, scopes, logos, icons, screen borders, test
# sprites). README.md "Sprites".
UI_SPRITE_RE = re.compile(
    r"^(?:\d{3,4}/|(?:\d{3,4})(?:hud|_logo|_pain|_train|nvgbat|crosshairs|top|bottom|left|right)|hud_|weapon_|"
    r"crosshairs|centre|nvg|snipe|scope_|afkstatus|hblack|vblack|projections?_|logo|small_logo|voiceicon|iflag|"
    r"iplayer|iunknown|camera|arrow1|bm1|agrunt1|cnt1|tile\.|shellchrome|xffloor|oriented|vp_parallel)")


def hud_listed(folder):
    """Sprite names (lower case, no extension) that the HUD lists in hud.txt / weapon_*.txt files of a folder."""
    names = set()
    try:
        files = os.listdir(folder)
    except OSError:
        return names
    for f in files:
        if not f.lower().endswith(".txt"):
            continue
        try:
            with open(os.path.join(folder, f), "r", encoding="latin-1") as fh:
                for line in fh:
                    parts = line.split()
                    if len(parts) >= 3:
                        names.add(parts[2].lower())
        except OSError:
            pass
    return names


def list_assets():
    scripts, sprites = {}, {}
    for source, base in ASSET_SOURCES:
        for kind, table in (("aurora", scripts), ("sprites", sprites)):
            folder = os.path.join(base, kind)
            if not os.path.isdir(folder):
                continue
            listed = hud_listed(folder) if kind == "sprites" else set()
            for dirpath, _dirs, files in os.walk(folder):
                for f in files:
                    if not f.lower().endswith(KINDS[kind][0]):
                        continue
                    rel = os.path.relpath(os.path.join(dirpath, f), base).replace(os.sep, "/")
                    key = rel.lower()
                    if key in table:
                        continue  # the mod's file hides Half-Life's
                    entry = {"name": key, "source": source, "size": os.path.getsize(os.path.join(dirpath, f))}
                    if kind == "sprites":
                        sub = key[len("sprites/"):]
                        entry["ui"] = bool(UI_SPRITE_RE.match(sub)) or sub[:-4] in listed
                    table[key] = entry
    return {"scripts": sorted(scripts.values(), key=lambda e: e["name"]),
            "sprites": sorted(sprites.values(), key=lambda e: e["name"])}


_ASSET_INDEX = None


def asset_index():
    global _ASSET_INDEX
    if _ASSET_INDEX is None:
        _ASSET_INDEX = list_assets()
    return _ASSET_INDEX


def find_asset(rel):
    """rel: 'sprites/x.spr' or 'aurora/x.aur' (case-insensitive, as the game). Returns (path, source) or None."""
    rel = rel.replace("\\", "/").lower()
    parts = rel.split("/")
    if len(parts) < 2 or parts[0] not in KINDS or any(p in ("", ".", "..") for p in parts):
        return None
    for source, base in ASSET_SOURCES:
        folder = base
        ok = True
        for p in parts:
            try:
                match = next((e for e in os.listdir(folder) if e.lower() == p), None)
            except OSError:
                match = None
            if match is None:
                ok = False
                break
            folder = os.path.join(folder, match)
        if ok and os.path.isfile(folder):
            real = os.path.realpath(folder)
            if real.startswith(os.path.realpath(base) + os.sep):
                return real, source
    return None


def reap_sessions():
    while True:
        time.sleep(600)
        now = time.time()
        try:
            for sid in os.listdir(DATA):
                d = session_dir(sid)
                try:
                    if now - os.path.getmtime(d) > SESSION_TTL:
                        shutil.rmtree(d, ignore_errors=True)
                except OSError:
                    pass
        except OSError:
            pass


class Handler(SimpleHTTPRequestHandler):
    server_version = "AuroraEditor/1"
    extensions_map = dict(SimpleHTTPRequestHandler.extensions_map)
    extensions_map.update({".mjs": "text/javascript", ".js": "text/javascript", ".wasm": "application/wasm",
                           ".json": "application/json", ".aur": "text/plain; charset=iso-8859-1",
                           ".spr": "application/octet-stream", ".svg": "image/svg+xml"})

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB, **kwargs)

    def log_message(self, fmt, *args):
        # one line per request without the client's address
        print("%s %s" % (self.command, fmt % args if args else fmt), flush=True)

    def end_headers(self):
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        if not self.path.startswith("/api/"):
            self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    # ---- helpers ----
    def send_json(self, obj, status=HTTPStatus.OK):
        body = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def send_error_json(self, status, text):
        self.send_json({"error": text}, status)

    def send_file(self, path, ctype, extra=None):
        with open(path, "rb") as f:
            body = f.read()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def route(self):
        return [unquote(p) for p in urlsplit(self.path).path.split("/") if p]

    def session_files(self, sid):
        d = session_dir(sid)
        out = []
        for kind in KINDS:
            folder = os.path.join(d, kind)
            if os.path.isdir(folder):
                for f in sorted(os.listdir(folder)):
                    out.append({"kind": kind, "name": "%s/%s" % (kind, f), "size": os.path.getsize(os.path.join(folder, f))})
        return out

    def get_session(self, sid):
        if not SESSION_RE.match(sid):
            return None
        d = session_dir(sid)
        if not os.path.isdir(d):
            return None
        os.utime(d, None)
        return d

    # ---- methods ----
    def do_GET(self):
        parts = self.route()
        if parts[:2] == ["api", "assets"]:
            return self.send_json(asset_index())
        if parts and parts[0] == "assets":
            found = find_asset("/".join(parts[1:]))
            if not found:
                return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
            path, source = found
            ctype = self.extensions_map.get(os.path.splitext(path)[1].lower(), "application/octet-stream")
            return self.send_file(path, ctype, {"X-Aurora-Source": source, "Cache-Control": "max-age=3600"})
        if parts[:2] == ["api", "session"] and len(parts) >= 3:
            d = self.get_session(parts[2])
            if not d:
                return self.send_error_json(HTTPStatus.NOT_FOUND, "no such session")
            if len(parts) == 3:
                return self.send_json({"id": parts[2], "files": self.session_files(parts[2])})
            if len(parts) == 5 and parts[3] in KINDS:
                name = clean_name(parts[4], parts[3])
                path = os.path.join(d, parts[3], name) if name else None
                if not path or not os.path.isfile(path):
                    return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
                return self.send_file(path, "application/octet-stream", {"Cache-Control": "no-store"})
            return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
        if parts and parts[0] == "api":
            return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
        return super().do_GET()

    def do_POST(self):
        parts = self.route()
        if parts == ["api", "session"]:
            with _lock:
                os.makedirs(DATA, exist_ok=True)
                if len(os.listdir(DATA)) >= MAX_SESSIONS:
                    return self.send_error_json(HTTPStatus.SERVICE_UNAVAILABLE, "too many sessions, try later")
                sid = secrets.token_urlsafe(18)
                os.makedirs(session_dir(sid))
            return self.send_json({"id": sid, "files": []}, HTTPStatus.CREATED)
        return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")

    def do_PUT(self):
        parts = self.route()
        if len(parts) != 5 or parts[:2] != ["api", "session"] or parts[3] not in KINDS:
            return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
        d = self.get_session(parts[2])
        if not d:
            return self.send_error_json(HTTPStatus.NOT_FOUND, "no such session")
        kind = parts[3]
        name = clean_name(parts[4], kind)
        if not name:
            return self.send_error_json(HTTPStatus.BAD_REQUEST,
                                        "file name: letters, digits, '_', '-', '.' (60 at most), ending in %s" % KINDS[kind][0])
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            length = -1
        if length < 0:
            return self.send_error_json(HTTPStatus.LENGTH_REQUIRED, "Content-Length required")
        if length > KINDS[kind][1]:
            return self.send_error_json(HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                                        "too large (%d KB at most)" % (KINDS[kind][1] // 1024))
        body = self.rfile.read(length)
        problem = check_content(kind, body)
        if problem:
            return self.send_error_json(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, problem)
        with _lock:
            files = self.session_files(parts[2])
            path = os.path.join(d, kind, name)
            used = sum(f["size"] for f in files if f["name"] != "%s/%s" % (kind, name))
            if used + len(body) > MAX_SESSION_BYTES or len(files) >= MAX_SESSION_FILES and not os.path.exists(path):
                return self.send_error_json(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "session full (32 MB, 64 files)")
            os.makedirs(os.path.join(d, kind), exist_ok=True)
            tmp = path + ".part"
            with open(tmp, "wb") as f:
                f.write(body)
            os.replace(tmp, path)
        return self.send_json({"name": "%s/%s" % (kind, name), "size": len(body)}, HTTPStatus.CREATED)

    def do_DELETE(self):
        parts = self.route()
        if len(parts) != 5 or parts[:2] != ["api", "session"] or parts[3] not in KINDS:
            return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
        d = self.get_session(parts[2])
        name = clean_name(parts[4], parts[3]) if d else None
        path = os.path.join(d, parts[3], name) if name else None
        if not path or not os.path.isfile(path):
            return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
        os.remove(path)
        return self.send_json({"deleted": "%s/%s" % (parts[3], name)})


def main():
    os.makedirs(DATA, exist_ok=True)
    threading.Thread(target=reap_sessions, daemon=True).start()
    idx = asset_index()
    print("Aurora editor on port %d: %d scripts, %d sprites" % (PORT, len(idx["scripts"]), len(idx["sprites"])), flush=True)
    httpd = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    httpd.daemon_threads = True
    httpd.serve_forever()


if __name__ == "__main__":
    main()
