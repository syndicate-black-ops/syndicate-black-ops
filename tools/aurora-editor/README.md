# Aurora particle editor (web)

A browser editor for SBO's Aurora particle scripts (`aurora/*.aur`, the `env_particle` entity). Level designers open a shipped script or a template, change it in a form or as text, and see it
run the way the game runs it; then download the `.aur` (or a zip with the sprites they uploaded) and put an
`env_particle` in the map.

## How the preview matches the game

The preview **is the game's particle code**: `src/cl_dll/aurora/particlesys.cpp`, `particlemgr.cpp` and
`src/cl_dll/parsemsg.cpp`, unchanged, compiled to WebAssembly (emscripten) with a thin engine shim
(`core/shim.cpp`, `core/include/`). The editor sends the systems the same `Aurora` messages `env_particle` sends
(placement, on / off, Spawn Use copies, a followed target with attachments), so the parser, its console warnings,
the limits (100 / 2048 per system, 8192 alive, 64 types, 4 overlay layers, 16 children a frame), spray timing,
drag, wind, bounce / die traces, draw / kill conditions, `lightmodel`, one-shot lifetimes and the 0.1 s frame cap
are the game's own. Nothing is re-implemented in JavaScript except reading the text structure for the form.

What the shim gives that code (GoldSrc behaviour unless said):

| Engine call | In the editor |
|---|---|
| `COM_ParseFile` | the engine's tokenizer (whitespace and bytes >= 128 skipped, `//` comments, quoted tokens, `{ } ( ) ' :` single tokens) |
| `RandomFloat` | Numerical Recipes `ran1`, as the engine, with a seed (Scene tab) so a restart repeats |
| `GetClientTime`, view origin / angles | the editor's clock (speed, pause, step; fixed client fps 30 to 200 or the browser's) and camera |
| `PM_PointContents`, `EV_PlayerTrace` (point hull) | a test scene: a floor (solid below), an optional wall box and water box |
| `LightAtPoint` | the Scene tab's world light (color x level) plus an optional lamp (linear falloff) |
| `GetEntityByIndex` | one followed target moving on a path (circle, back and forth, figure eight, turning), visible or not, studio model or brush, 4 attachments |
| `BoxInPVS` | always true, or off ("In the PVS" in the Scene tab) |
| `SPR_Load`, `GetSpritePointer`, `SpriteTexture` | the sprites the browser has (uploads, SBO's, Half-Life's) with their frame counts |
| `RenderMode`, `Color4f`, `Begin` / `Vertex3fv` / `End` | the engine's state rules: `Color4f` multiplies the color by alpha and sends alpha 1 unless the mode is `kRenderTransAlpha` (then bytes); additive blends `src-alpha, one`, solid / texture / color `src-alpha, one-minus-src-alpha`; additive and solid do not write depth, texture and color leave the depth mask as it was. Each quad is recorded with that state and drawn by WebGL2 in the game's order |

Sprites are decoded in the browser (`web/js/spr.js`, `.spr` version 2, frame groups by the client time) as the
OpenGL renderer uploads them: normal and additive formats opaque, indexalpha = the palette's last color with the
index as alpha, alphatest = index 255 transparent. Texture filtering is linear.

**Parity evidence.** WebAssembly runs the same C++ as the game, so there is no second implementation to drift. The
image build also compiles the same sources natively (g++) and runs `tests/parity.mjs`: every shipped script, static
and following a moving / turning target, 300 frames at 100 fps with the same seed, every particle's origin and size
compared frame by frame between the native and the WebAssembly build. Last run: 36 scripts x 2 placements, 721 590
particle states, worst difference 5e-6 (the print precision): identical. The build fails if they differ.

Not shown (by design or not possible in a browser): the map's lightmaps and fog, gamma / brightness settings,
other effects and entities in the map, the software renderer (draws nothing in the game).

## Features

- **Types tree**: the main type, its spray children and overlays (and types nothing uses); add a type, a spray
  child or an overlay, rename (references follow), duplicate, delete, set as main.
- **Properties**: a field for every key the game's parser reads, grouped, with the meaning, default and unit on
  hover (from `core/aurora-keys.json`, which follows the game's parser); sliders, `min..max` ranges,
  start / end color pickers with a color and alpha gradient, type and sprite pickers, reset to default. A form
  change rewrites only that key's value in the text: comments, unknown keys and other editors' keys stay.
- **Script**: the raw text, two-way (typing rebuilds the preview), with line numbers.
- **Problems**: the game's own console lines for the script (unknown keys, missing sprites, undefined types,
  limits) plus hints (a sprayed type without lifetime, a particle budget estimate against `particles`, keys that
  override each other, values the game reads differently than written, pfxEd keys); click one to see it in the text.
- **pfxEd / XashXT scripts**: `lightmodel` and `killcondition` (top level, and in a type block as pfxEd saves it)
  are edited and previewed when the compiled game code reads them (the editor checks at start); pfxEd's `offset`
  and a top-level `attachment` are shown with the game's warning.
- **Preview**: orbit camera (drag, right drag / Shift+drag pan, wheel) or fly camera (W A S D Q E), F to frame the
  particles; floor grid (16 / 64 units) with a 72-unit player; background color and field of view; play, pause,
  step, restart, speed 0.1x to 4x, client fps; live counts against the limits (system / `particles`, all / 8192,
  one-shot copies).
- **Entity**: a point with angles (quick buttons Up / Down / North...), or "Move with entity" on a moving target
  with an offset in its frame and an attachment; looping (entity on, switch with O) or one-shot (Spawn Use copies,
  by hand with T or every N seconds).
- **Scene**: floor, wall and water for `collision`, `drawcondition` and `killcondition`; world light and a lamp for
  `lightmodel`; "In the PVS" off to see a system stand still.
- **Sprites**: a browser of the particle sprites (SBO, Half-Life when the image has it, uploads; HUD / UI sprites behind "Show HUD / UI sprites") with thumbnails, an
  animated preview (frame rate, background), size, frames, texture format; "Use for the selected type".
- **Files**: New from templates (pfxEd's new file, smoke, sparks, trail, blank); Open any shipped script, an
  upload or a browser draft; upload `.aur` and `.spr` (button or drag and drop, several at once); download the
  `.aur` (Ctrl+S) or a zip with `aurora/` and `sprites/` as in the game folder; drafts autosaved in the browser.
- **Share link**: the script and scene in the URL (deflate, base64url); uploaded sprites are not in it.
- **Map entity**: the `env_particle` block for a `.map` (message, angles, spawnflags, target, attachment) and the
  `.res` lines, to copy.
- **Compare**: a second script (a shipped one, an upload, or a snapshot of the current one) beside yours, same
  entity settings; swap to edit the other.
- Undo / redo of the whole script (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z), keyboard keys (H for help).

## Build and run

The image is built from the repository root (the context needs the game sources and assets):

```sh
# a builder with hard limits (BuildKit ignores docker build --memory)
docker buildx create --name aurora-editor-builder --driver docker-container \
  --driver-opt memory=4g --driver-opt cpu-quota=200000 --driver-opt cpu-period=100000
timeout 1500 docker buildx build --builder aurora-editor-builder -f tools/aurora-editor/Dockerfile \
  -t sbo-aurora-editor:local --load .
docker run -d --name sbo-aurora-editor --memory=1g --cpus=1 --restart=no -p 8095:8095 sbo-aurora-editor:local
# http://<host>:8095/
```

Optional named contexts:

| Context | Default | Use |
|---|---|---|
| `aurora_src=<dir>` | `src/cl_dll/aurora` | build the preview from other particle sources (e.g. a branch's worktree) |
| `extra_assets=<dir>` | none | more files over the shipped ones (`aurora/x.aur`, `sprites/x.spr`) |
| `valve_sprites=<dir>` | none | Half-Life's `valve/sprites` (only the allow-listed particle sprites are copied) |

Only Half-Life sprites that can be particles are copied: the Dockerfile keeps the files named in
**`valve-sprites.allow`** (123 of the 356 in `valve/sprites`) and drops everything else.

| In (the allow-list) | Out |
|---|---|
| explosions (`aexplo` to `hexplo`, `explode1/2`, `fexplo1`, `wxplo1`, `zerogxplode`, `mushroom`, `blast`, `shockwave`, `gwave1`), fire and jets (`fire`, `xfire`, `xfire2`, `xfireball3`, `rjet1`, `bluejet1`), smoke and steam (`ballsmoke`, `smoke`, `steam1`, `stmbal1`, `wallpuff`, `xsmoke1/3/4`, `xssmke1`), sparks (`spark1`, `richo1/2`, `streak`, `xspark1-4`), glows and flares (`glow01-05`, `animglow01`, `hotglow`, `flare1-6`, blue / red / yellow / x flares, `gargeye1`, `nhth1`, `laserdot`, `dot`, `white`, `spotlight01-04`, `moths`), muzzle flashes (`muz1-8`, `muzzleflash`, `muzzleflash1-3`), water (`bubble`, `sdrip1`, `wdrip2`, `wsplash3`), blood and goo (`blood`, `blooddrop`, `bloodspray`, `bigspit`, `tinyspit`, `mommablob`, `mommaspit`, `mommaspout`, `poison`, `bhit`), teleport and energy (`b- c- d- e-tele1`, `tele1`, `enter1`, `exit1`, `portal1`, `plasma`), beams and lightning (`bolt1`, `laserbeam`, `lgtning`, `rope`, `xbeam1-5`, `xenobeam`, `zbeam1-6`) | HUD and weapon-menu sprites (`1280/`, `2560/`, `320*`, `640*`, `1280*`, `2560*`: every sprite a `hud.txt` / `weapon_*.txt` names), crosshairs, logos, scoreboard and spectator icons (`iflag*`, `iplayer*`, `iunknown`), `voiceicon`, `camera`, `arrow1`, `cnt1`, `tile`, `shellchrome`, `xffloor`, `bm1`, `agrunt1` (a picture of an alien grunt), the orientation test sprites (`oriented`, `vp_parallel*`) |

The sprite browser applies the same idea to every source (SBO's own sprites, and a Half-Life folder given some
other way): **HUD / UI sprites are hidden** unless "Show HUD / UI sprites" is ticked. The server marks a sprite as
UI when a `hud.txt` or `weapon_*.txt` next to it names it, or its name is a HUD sheet or resolution folder
(`320hud*`, `640hud*`, `1280/`, `hud_*`, `weapon_*`...), a crosshair, scope (`scope_*`, `snipe*`), night vision
(`nvg*`), screen border (`1024top`, `640left`...), logo, icon, or one of the names above (`server.py`,
`UI_SPRITE_RE`). For SBO that hides 60 of its 79 sprites and shows 19 (`blacksmoke`, `fasmoke2`, `fire`,
`gas_puff_01`, `glow01-05`, `hotglow`, `laserbeam`, `luejet1`, `p_beam`, `smoke3`, `sbo_rain`, `sbo_snow`,
`effects/rain`, `effects/ripple`, `effects/snowflake`). Hidden sprites still work in scripts; uploads always show.

**`valve_sprites` is Valve content: only for a local test image. Never build an image that is pushed, published
or shared with it.** Without it the Half-Life sprites the scripts name (`steam1`, `xfireball3`, `yelflare1`...) are
"sprite not found", as in a game without Half-Life content; mappers can upload them from their own install. A
Half-Life dedicated server (`steamcmd +login anonymous +app_update 90`) has them in `valve/sprites`.

The test image on the workstation was built with `aurora_src` = the `feat/aurora-xashxt-keys` worktree (so
`lightmodel` / `killcondition` preview), `extra_assets` = its `wrecksmoke*.aur`, and `valve_sprites` from an
anonymous HLDS download.

### Development without the image

`core/build.sh stage <repo> <dir>` copies the game sources it compiles; `core/build.sh wasm <dir> <out>` builds
`aurora-core.mjs` / `.wasm` with emscripten (the `emscripten/emsdk:3.1.64` image has it); `core/build.sh native
<dir> <out>` builds the parity test's native side. `web/` is static: put `aurora-core.*` and `core/aurora-keys.json`
in `web/core/` and run `AURORA_PORT=8095 python3 server/server.py` from this folder with `assets/sbo/` and
`assets/valve/` next to it.

### Tests

- `tests/parity.mjs` (in the image build): native vs WebAssembly, every shipped script (above).
- `tests/smoke.mjs` (Playwright, Chromium with software WebGL): opens every shipped script and checks it makes
  particles and causes no console error; checks the form edit (one line changed, comments kept) and undo, upload
  of a sprite and a script, the share link, one-shot copies, `lightmodel` with compare, `killcondition` water, the
  sprite browser; takes screenshots.

```sh
docker run --rm --memory=2g --cpus=1 --network host -v $PWD/tools/aurora-editor/tests:/pw -v /tmp/screens:/screens \
  -w /pw mcr.microsoft.com/playwright:v1.48.2-jammy sh -c 'npm i playwright@1.48.2 && node smoke.mjs http://localhost:8095/ /screens'
```

## Server

`server/server.py`, Python standard library only (no packages to install or audit): static files, the asset
index (`/api/assets`), the assets (`/assets/sprites/x.spr`: the mod's file before Half-Life's, case-insensitive, as
the game searches), and upload sessions (`/api/session`). Uploads: `.aur` (256 KB, text) and `.spr` (4 MB, an
`IDSP` version 2 header), plain lower-case names (letters, digits, `_ - .`, 60 characters; no folders, no `..`),
32 MB and 64 files per session, 200 sessions; kept in the container's `/tmp`, removed after 12 hours idle or when
the container goes. Runs as an unprivileged user.

## Limits

- One test scene (floor, wall, water box, lights): no BSP, no lightmaps, no fog; `lightmodel` shows the editor's
  light, not a map's.
- The sprite's texture is resampled to the largest frame of the sprite and filtered linearly; the engine's
  power-of-two resampling and `gl_texturemode`, `texgamma` and `brightness` are not reproduced.
- Depth mask of `texture` / `color` modes: the engine leaves it as the previous mode left it; the editor starts
  each frame with it on (the manager ends each frame with `kRenderNormal`).
- The share link holds the script and scene, not uploaded sprites; uploads are per browser tab session.
- An in-game VGUI2 editor can reuse `core/aurora-keys.json` (the key list, groups, help) and the editing rule
  (patch one value's token, keep the rest of the text); `web/js/aur.js` shows it.

## Files

| Path | What |
|---|---|
| `Dockerfile`, `Dockerfile.dockerignore` | stages: the game sources (overridable), emscripten build + parity test, the allow-listed Half-Life sprites, Python runtime |
| `valve-sprites.allow` | the Half-Life sprites the image may carry (particle sprites only) |
| `core/shim.cpp`, `core/include/*.h` | the engine shim and the stubs of the SDK headers the particle code includes |
| `core/native_main.cpp`, `tests/parity.mjs` | parity test |
| `core/aurora-keys.json` | every key: kind, default, range, unit, group, help; limits; other editors' keys |
| `core/build.sh` | stage / wasm / native builds |
| `server/server.py` | the server |
| `web/index.html`, `web/css/app.css` | the page |
| `web/js/app.js` | the editor (state, panels, scene, uploads, loop) |
| `web/js/aur.js` | the script's token structure (the engine's tokenizer and the parser's walk), value edits, lint |
| `web/js/engine.js` | the WebAssembly wrapper |
| `web/js/renderer.js` | WebGL2: scene and particle quads |
| `web/js/spr.js` | `.spr` reader |
| `web/js/templates.js`, `web/js/util.js` | templates; zip writer, share link, storage helpers |
| `tests/smoke.mjs` | headless browser test |

Third-party code: none in the page or the server (no JavaScript libraries; the zip writer, the sprite reader and
the WebGL code are written for this tool). Build and test tools only: emscripten (`emscripten/emsdk:3.1.64`, MIT /
University of Illinois NCSA), Python 3.12 (`python:3.12-slim`, PSF), Playwright 1.48.2 (Apache-2.0) with its
Chromium. The game sources it compiles are SBO's (HL SDK license). No XashXT / PrimeXT code (GPLv3): the pfxEd
template and key list come from a study of its behaviour only.
