#!/usr/bin/env python3
"""SBO Aurora particle editor: the page and the shipped assets, or the same as a static site.

Python standard library only (tools/aurora-editor/README.md). The page is static: uploads stay in the browser.
  server.py                  serve on AURORA_PORT:
                               GET /                          the editor (web/)
                               GET /assets/index.json         the scripts and sprites it carries (JSON)
                               GET /assets/<aurora|sprites>/<name>  one of them; the mod's files before Half-Life's
                                                              (as the game searches), case-insensitive
  server.py --export <dir>   write the same as static files (web/, assets/index.json, every asset under its lower-case
                             index name, the mod's file where both have one) for a static host
"""

import json
import os
import re
import shutil
import sys
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlsplit

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
ASSETS = os.path.join(ROOT, "assets")
# search order of the game: the mod, then Half-Life
ASSET_SOURCES = [("sbo", os.path.join(ASSETS, "sbo")), ("valve", os.path.join(ASSETS, "valve"))]
PORT = int(os.environ.get("AURORA_PORT", "8095"))

MAX_SCRIPT = 256 * 1024
MAX_SPRITE = 4 * 1024 * 1024
KINDS = {"aurora": (".aur", MAX_SCRIPT), "sprites": (".spr", MAX_SPRITE)}


# HUD / UI sprites (hidden in the sprite browser unless "Show HUD / UI sprites"): the ones a hud.txt or weapon_*.txt
# names, and these names (HUD sheets and resolution folders, crosshairs, scopes, logos, icons, screen borders, test
# sprites). README.md "Sprites".
# folders that are never listed or served: the map editor's entity icons (sprites/editor/) are not particle sprites
EXCLUDED_DIRS = ("sprites/editor/",)


def excluded(rel):
    return any(rel.startswith(d) for d in EXCLUDED_DIRS)


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
                    if key in table or excluded(key):
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
    if len(parts) < 2 or parts[0] not in KINDS or any(p in ("", ".", "..") for p in parts) or excluded(rel):
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
        if not self.path.startswith("/assets/"):
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

    # ---- methods ----
    def do_GET(self):
        parts = self.route()
        if parts == ["assets", "index.json"]:
            return self.send_json(asset_index())
        if parts and parts[0] == "assets":
            found = find_asset("/".join(parts[1:]))
            if not found:
                return self.send_error_json(HTTPStatus.NOT_FOUND, "not found")
            path, _source = found
            ctype = self.extensions_map.get(os.path.splitext(path)[1].lower(), "application/octet-stream")
            return self.send_file(path, ctype, {"Cache-Control": "max-age=3600"})
        return super().do_GET()


def export(out):
    """The static site: web/, assets/index.json and each indexed asset at its lower-case name."""
    shutil.copytree(WEB, out, dirs_exist_ok=True)
    idx = asset_index()
    for entry in idx["scripts"] + idx["sprites"]:
        path, _source = find_asset(entry["name"])
        dest = os.path.join(out, "assets", *entry["name"].split("/"))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(path, dest)
    with open(os.path.join(out, "assets", "index.json"), "w") as f:
        json.dump(idx, f, separators=(",", ":"))
    counts = {}
    for e in idx["sprites"]:
        counts[e["source"]] = counts.get(e["source"], 0) + 1
    print("exported to %s: %d scripts, sprites %s" % (out, len(idx["scripts"]), counts), flush=True)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--export":
        return export(sys.argv[2])
    idx = asset_index()
    print("Aurora editor on port %d: %d scripts, %d sprites" % (PORT, len(idx["scripts"]), len(idx["sprites"])), flush=True)
    httpd = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    httpd.daemon_threads = True
    httpd.serve_forever()


if __name__ == "__main__":
    main()
