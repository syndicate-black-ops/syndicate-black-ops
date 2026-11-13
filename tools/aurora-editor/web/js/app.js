// SBO Aurora particle editor: the page. The preview is the game's own particle code (engine.js); this file holds
// the script text and its history, the panels, the scene, uploads and the main loop.

import * as aur from './aur.js';
import { parseSprite, drawFrame, groupImage, SPR_FORMATS, SPR_TYPES } from './spr.js';
import { AuroraEngine, P, MSG, TARGET_ENTITY } from './engine.js';
import { Renderer, angleVectors } from './renderer.js';
import { TEMPLATES, NEW_TYPE_LINES } from './templates.js';
import * as util from './util.js';

const $ = (id) => document.getElementById(id);
const MAIN_ID = 1;
const COMPARE_ID = 3;
const KEY_PREFIX = 'sbo-aurora-editor:';
const NAME_RE = /^[a-z0-9_\-][a-z0-9_.\-]{0,59}$/;

const DEFAULT_SCENE = {
	background: '#1b1f24', grid: true, player: true, floor: true, floorZ: 0,
	water: false, waterX: 0, waterY: 0, waterSize: 256, waterTop: 40,
	wall: false, wallX: 160, wallWidth: 512, wallHeight: 256,
	lightColor: '#ffffff', lightLevel: 255,
	lamp: false, lampPos: [96, 96, 96], lampRadius: 320, lampColor: '#ffc080',
	pvs: true, fov: 90,
};
const DEFAULT_EMITTER = {
	mode: 'static', origin: [0, 0, 64], angles: [0, 0, 0],
	path: 'circle', radius: 160, period: 8, length: 512, height: 64, turn: true,
	offset: [0, 0, 0], attachment: -1, attOffset: [24, 0, 8], targetVisible: true, targetStudio: true,
};
const DEFAULT_PLAY = { playing: true, speed: 1, fps: 100, mode: 'loop', on: true, autoTrigger: 3, seed: 1 };

const S = {
	name: 'steamvent',
	text: '',
	doc: null,
	selected: 'top',
	scene: { ...DEFAULT_SCENE },
	emitter: structuredClone(DEFAULT_EMITTER),
	play: { ...DEFAULT_PLAY },
	compare: { enabled: false, name: '', text: '', offset: 256, doc: null },
	schema: null,
	assets: { scripts: [], sprites: [] },
	uploads: new Map(),		// 'sprites/x.spr' -> ArrayBuffer, 'aurora/x.aur' -> string
	sprites: new Map(),		// name -> Promise<{ state, spr, source, error }>
	spriteInfo: new Map(),	// name -> resolved info
	gameLog: [],
	coreHasXash: true,
	selectedSprite: null,
};

let engine, renderer;
let simTime = 1;
let nextTrigger = 0;
const handleNames = new Map();

//=============================================================================
// toast / dialog
//=============================================================================

let toastTimer = 0;
function toast(text, err = false) {
	const t = $('toast');
	t.textContent = text;
	t.classList.toggle('err', err);
	t.classList.add('show');
	clearTimeout(toastTimer);
	toastTimer = setTimeout(() => t.classList.remove('show'), err ? 6000 : 3000);
}

function dialog(html, buttons = [['Close', 'close']], onOpen) {
	const d = $('dialog');
	$('dialogBody').innerHTML = html;
	const menu = $('dialogButtons');
	menu.innerHTML = '';
	return new Promise((resolve) => {
		for (const [label, value, cls] of buttons) {
			const b = document.createElement('button');
			b.textContent = label;
			b.value = value;
			if (cls) b.className = cls;
			menu.appendChild(b);
		}
		d.onclose = () => resolve(d.returnValue);
		d.returnValue = '';
		d.showModal();
		if (onOpen) onOpen($('dialogBody'), (v) => { d.close(v); });
	});
}

//=============================================================================
// the script text, its history and drafts
//=============================================================================

const history = { list: [], index: -1, lastKey: '', lastTime: 0 };

function pushHistory(text, coalesce) {
	const now = Date.now();
	if (coalesce && coalesce === history.lastKey && now - history.lastTime < 1200 && history.index === history.list.length - 1 && history.index > 0) {
		history.list[history.index] = { text, name: S.name };
	} else {
		history.list.splice(history.index + 1);
		history.list.push({ text, name: S.name });
		if (history.list.length > 300) history.list.shift();
		history.index = history.list.length - 1;
	}
	history.lastKey = coalesce || '';
	history.lastTime = now;
	updateUndoButtons();
}

function updateUndoButtons() {
	$('btnUndo').disabled = history.index <= 0;
	$('btnRedo').disabled = history.index >= history.list.length - 1;
}

function undo() {
	if (history.index <= 0) return;
	history.index--;
	history.lastKey = '';
	applyText(history.list[history.index].text, 'history');
	updateUndoButtons();
}

function redo() {
	if (history.index >= history.list.length - 1) return;
	history.index++;
	history.lastKey = '';
	applyText(history.list[history.index].text, 'history');
	updateUndoButtons();
}

let rebuildTimer = 0;
let draftTimer = 0;
let formStale = false;

// every change of the script goes through here. source: 'text' | 'form' | 'tree' | 'load' | 'history'
function parseOptions() {
	return { xashKeys: S.coreHasXash, typeKillcondition: S.coreTypeKill };
}

function setText(text, source, coalesce) {
	if (text === S.text && source !== 'load') return;
	applyText(text, source);
	pushHistory(text, coalesce);
}

function applyText(text, source) {
	S.text = text;
	S.doc = aur.parse(text, parseOptions());
	if (typeof S.selected === 'number' && S.selected >= S.doc.blocks.length) S.selected = S.doc.blocks.length ? S.doc.blocks.length - 1 : 'top';
	if (source !== 'text') setTextarea(text);
	updateLineNumbers();
	renderTree();
	if (source === 'form' && $('form').contains(document.activeElement)) formStale = true;
	else renderForm();
	clearTimeout(rebuildTimer);
	rebuildTimer = setTimeout(rebuild, source === 'text' ? 300 : 60);
	clearTimeout(draftTimer);
	draftTimer = setTimeout(saveDraft, 800);
}

function saveDraft() {
	const drafts = util.store.get(KEY_PREFIX + 'drafts') || {};
	drafts[S.name] = { text: S.text, ts: Date.now() };
	const names = Object.keys(drafts).sort((a, b) => drafts[b].ts - drafts[a].ts);
	for (const n of names.slice(20)) delete drafts[n];
	util.store.set(KEY_PREFIX + 'drafts', drafts);
	util.store.set(KEY_PREFIX + 'last', S.name);
}

function saveSettings() {
	util.store.set(KEY_PREFIX + 'settings', { scene: S.scene, emitter: S.emitter, play: { ...S.play, playing: true } });
}

function loadScript(name, text, note) {
	S.name = cleanScriptName(name) || 'effect';
	$('scriptName').value = S.name;
	S.selected = 'top';
	history.list = [];
	history.index = -1;
	setText(text, 'load');
	if (note) toast(note);
	clearTimeout(loadScript.timer);
	frameCamera();
	loadScript.timer = setTimeout(() => frameCamera(true), 1500);
}

function cleanScriptName(n) {
	return String(n || '').toLowerCase().replace(/\.aur$/, '').replace(/^.*[\\/]/, '').replace(/[^a-z0-9_\-.]/g, '_').slice(0, 54);
}

function scriptPath() {
	return `aurora/${S.name || 'effect'}.aur`;
}

//=============================================================================
// sprites: the uploads first, then the game's files (SBO, then Half-Life)
//=============================================================================

function normSprite(n) {
	return String(n || '').toLowerCase().replace(/\\/g, '/');
}

function knownSprite(name) {
	const n = normSprite(name);
	return S.uploads.has(n) || S.assets.sprites.some((s) => s.name === n);
}

function ensureSprite(name) {
	const n = normSprite(name);
	if (S.sprites.has(n)) return S.sprites.get(n);
	const p = (async () => {
		let buf = null;
		let source = null;
		if (S.uploads.has(n)) {
			buf = S.uploads.get(n);
			source = 'upload';
		} else if (S.assets.sprites.some((a) => a.name === n) && /^[a-z0-9_\-./]+$/.test(n) && !n.includes('..')) {
			try {
				const r = await fetch(`assets/${n}`);
				if (r.ok) {
					buf = await r.arrayBuffer();
					source = S.assets.sprites.find((a) => a.name === n).source;
				}
			} catch { buf = null; }
		}
		let info;
		if (!buf) info = { state: 'missing', name: n };
		else {
			try {
				const spr = parseSprite(buf);
				renderer.uploadSprite(n, spr);
				engine.setSprite(n, spr.numframes);
				info = { state: 'ok', spr, source, name: n, size: buf.byteLength };
			} catch (e) {
				info = { state: 'bad', error: e.message, name: n, source };
			}
		}
		S.spriteInfo.set(n, info);
		return info;
	})();
	S.sprites.set(n, p);
	return p;
}

function forgetSprite(name) {
	S.sprites.delete(normSprite(name));
	S.spriteInfo.delete(normSprite(name));
}

//=============================================================================
// building the systems: a new "map" each time the script changes, then the entity's messages
//=============================================================================

let rebuilding = false;
let rebuildAgain = false;

async function rebuild() {
	if (rebuilding) { rebuildAgain = true; return; }
	rebuilding = true;
	try {
		const docs = [S.doc];
		if (S.compare.enabled && S.compare.doc) docs.push(S.compare.doc);
		const names = new Set();
		for (const d of docs) for (const s of aur.spritesOf(d)) names.add(s);
		await Promise.all([...names].map(ensureSprite));

		engine.vidinit();
		handleNames.clear();
		engine.takeLog();
		engine.seed(S.play.seed);
		engine.setFile(scriptPath(), S.text);
		applySceneParams();
		updateTarget(simTime);
		sendMain();
		const log = engine.takeLog();
		if (S.compare.enabled && S.compare.doc) {
			engine.setFile(comparePath(), S.compare.text);
			sendCompare();
			engine.takeLog();
		}
		S.gameLog = log.split('\n').filter((l) => l.trim());
		nextTrigger = simTime;
		renderProblems();
	} finally {
		rebuilding = false;
		if (rebuildAgain) { rebuildAgain = false; rebuild(); }
	}
}

function comparePath() {
	return `aurora/compare-${cleanScriptName(S.compare.name) || 'b'}.aur`;
}

function placement(extra = [0, 0, 0]) {
	const em = S.emitter;
	if (em.mode === 'static') return { flags: 0, origin: em.origin.map((v, i) => v + extra[i]), target: 0, attachment: -1 };
	return { flags: MSG.FOLLOW, origin: em.offset.map((v, i) => v + extra[i]), target: TARGET_ENTITY, attachment: em.attachment | 0 };
}

function sendMain() {
	const pl = placement();
	const on = S.play.mode === 'loop' && S.play.on ? MSG.ON : 0;
	engine.message(pl.flags | on, MAIN_ID, pl.origin, S.emitter.angles, scriptPath(), pl.target, pl.attachment);
}

function sendCompare() {
	const pl = placement([0, -S.compare.offset, 0]);
	const on = S.play.mode === 'loop' && S.play.on ? MSG.ON : 0;
	engine.message(pl.flags | on, COMPARE_ID, pl.origin, S.emitter.angles, comparePath(), pl.target, pl.attachment);
}

function resendPlacement() {
	sendMain();
	if (S.compare.enabled && S.compare.doc) sendCompare();
}

function trigger() {
	const w = emitterWorld();
	engine.message(MSG.ON | MSG.ONESHOT, 0, w.origin, w.angles, scriptPath());
	if (S.compare.enabled && S.compare.doc) {
		engine.message(MSG.ON | MSG.ONESHOT, 0, [w.origin[0], w.origin[1] - S.compare.offset, w.origin[2]], w.angles, comparePath());
	}
	engine.takeLog();
}

//=============================================================================
// the scene and the followed target
//=============================================================================

function hexToRgb(h) {
	const v = parseInt(h.slice(1), 16);
	return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
}

function rgbToHex(r, g, b) {
	const c = (x) => Math.max(0, Math.min(255, Math.round(x))).toString(16).padStart(2, '0');
	return `#${c(r)}${c(g)}${c(b)}`;
}

function applySceneParams() {
	const sc = S.scene;
	engine.set(P.FLOOR_ON, sc.floor ? 1 : 0, sc.floorZ);
	const half = sc.waterSize / 2;
	engine.set(P.WATER_ON, sc.water ? 1 : 0, sc.waterX - half, sc.waterY - half, sc.floorZ - 1, sc.waterX + half, sc.waterY + half, sc.waterTop);
	const ww = sc.wallWidth / 2;
	engine.set(P.WALL_ON, sc.wall ? 1 : 0, sc.wallX, -ww, sc.floorZ - 1, sc.wallX + 16, ww, sc.floorZ + sc.wallHeight);
	const l = hexToRgb(sc.lightColor).map((c) => c * sc.lightLevel / 255);
	engine.set(P.LIGHT, ...l);
	engine.set(P.LAMP_ON, sc.lamp ? 1 : 0, ...sc.lampPos, sc.lampRadius, ...hexToRgb(sc.lampColor));
	engine.set(P.PVS, sc.pvs ? 1 : 0);
	engine.set(P.TARGET_VISIBLE, S.emitter.targetVisible ? 1 : 0);
	engine.set(P.TARGET_STUDIO, S.emitter.targetStudio ? 1 : 0);
	renderer.fovX = sc.fov;
}

// the followed entity on its path at time t: origin and angles
function targetAt(t) {
	const em = S.emitter;
	const T = Math.max(0.5, em.period);
	const a = (t / T) * Math.PI * 2;
	let pos, dir;
	if (em.path === 'circle') {
		pos = [Math.cos(a) * em.radius, Math.sin(a) * em.radius, em.height];
		dir = [-Math.sin(a), Math.cos(a)];
	} else if (em.path === 'line') {
		const u = (t / T) % 1;
		const tri = u < 0.5 ? u * 2 : 2 - u * 2;
		pos = [(tri - 0.5) * em.length, 0, em.height];
		dir = [u < 0.5 ? 1 : -1, 0];
	} else if (em.path === 'figure8') {
		pos = [Math.sin(a) * em.radius, Math.sin(a) * Math.cos(a) * em.radius, em.height];
		dir = [Math.cos(a), Math.cos(2 * a)];
	} else if (em.path === 'spin') {
		pos = [0, 0, em.height];
		dir = [Math.cos(a), Math.sin(a)];
	} else {
		pos = [0, 0, em.height];
		dir = [1, 0];
	}
	const yaw = em.turn || em.path === 'spin' ? Math.atan2(dir[1], dir[0]) * 180 / Math.PI : 0;
	return { pos, angles: [0, yaw, 0] };
}

function rotateOffset(angles, off) {
	const { forward, right, up } = angleVectors(angles);
	return [0, 1, 2].map((i) => forward[i] * off[0] - right[i] * off[1] + up[i] * off[2]);
}

let targetNow = { pos: [0, 0, 0], angles: [0, 0, 0] };

function updateTarget(t) {
	if (S.emitter.mode === 'static') return;
	targetNow = targetAt(t);
	engine.set(P.TARGET_POS, ...targetNow.pos);
	engine.set(P.TARGET_ANGLES, ...targetNow.angles);
	for (let k = 0; k < 4; k++) {
		const off = [S.emitter.attOffset[0], S.emitter.attOffset[1] + k * 6, S.emitter.attOffset[2]];
		const r = rotateOffset(targetNow.angles, off);
		engine.set(P.ATTACHMENTS + k * 3, targetNow.pos[0] + r[0], targetNow.pos[1] + r[1], targetNow.pos[2] + r[2]);
	}
}

// where the emitter is now, as GetEmitter places it
function emitterWorld() {
	const em = S.emitter;
	if (em.mode === 'static') return { origin: em.origin.slice(), angles: em.angles.slice() };
	const angles = [0, 1, 2].map((i) => em.angles[i] + targetNow.angles[i]);
	if (em.attachment >= 0 && em.attachment < 4 && em.targetStudio) {
		const k = P.ATTACHMENTS + em.attachment * 3;
		return { origin: [engine.get(k), engine.get(k + 1), engine.get(k + 2)], angles };
	}
	const r = rotateOffset(targetNow.angles, em.offset);
	return { origin: [targetNow.pos[0] + r[0], targetNow.pos[1] + r[1], targetNow.pos[2] + r[2]], angles };
}

//=============================================================================
// camera
//=============================================================================

const cam = { target: [0, 0, 64], dist: 360, yaw: 205, pitch: 18, fly: false, origin: [0, 0, 0], angles: [0, 0, 0], keys: new Set() };

function cameraUpdate(dt) {
	if (cam.fly) {
		const { forward, right, up } = angleVectors(cam.angles);
		const speed = (cam.keys.has('shift') ? 600 : 220) * dt;
		const mv = (v, s) => { for (let i = 0; i < 3; i++) cam.origin[i] += v[i] * s; };
		if (cam.keys.has('w')) mv(forward, speed);
		if (cam.keys.has('s')) mv(forward, -speed);
		if (cam.keys.has('d')) mv(right, speed);
		if (cam.keys.has('a')) mv(right, -speed);
		if (cam.keys.has('e')) mv(up, speed);
		if (cam.keys.has('q')) mv(up, -speed);
		return;
	}
	cam.angles = [cam.pitch, cam.yaw, 0];
	const { forward } = angleVectors(cam.angles);
	cam.origin = [0, 1, 2].map((i) => cam.target[i] - forward[i] * cam.dist);
}

// F: the emitter, or with fit the particles drawn now (their bounds)
function frameCamera(fit = false) {
	const w = emitterWorld();
	cam.fly = false;
	cam.target = [w.origin[0], w.origin[1] - (S.compare.enabled ? S.compare.offset / 2 : 0), w.origin[2] + 24];
	cam.dist = S.compare.enabled ? 520 : 300;
	cam.pitch = 15;
	const q = lastQuads;
	if (fit && q.length) {
		const mins = [Infinity, Infinity, Infinity], maxs = [-Infinity, -Infinity, -Infinity];
		for (let o = 0; o < q.length; o += 28) {
			for (let v = 0; v < 4; v++) for (let i = 0; i < 3; i++) {
				const x = q[o + v * 5 + i];
				if (x < mins[i]) mins[i] = x;
				if (x > maxs[i]) maxs[i] = x;
			}
		}
		const size = Math.max(maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]);
		if (isFinite(size)) {
			cam.target = [0, 1, 2].map((i) => (mins[i] + maxs[i]) / 2);
			cam.dist = Math.max(96, Math.min(3000, size * 1.25 + 48));
		}
	}
	if ($('flyMode')) $('flyMode').checked = false;
}

function bindCamera() {
	const c = $('view');
	let drag = null;
	c.addEventListener('contextmenu', (e) => e.preventDefault());
	c.addEventListener('pointerdown', (e) => {
		c.focus();
		c.setPointerCapture(e.pointerId);
		drag = { x: e.clientX, y: e.clientY, pan: e.button === 2 || e.shiftKey || e.button === 1 };
	});
	c.addEventListener('pointermove', (e) => {
		if (!drag) return;
		const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
		drag.x = e.clientX;
		drag.y = e.clientY;
		if (cam.fly) {
			cam.angles[1] -= dx * 0.25;
			cam.angles[0] = Math.max(-89, Math.min(89, cam.angles[0] + dy * 0.25));
			return;
		}
		if (drag.pan) {
			const { right, up } = angleVectors([cam.pitch, cam.yaw, 0]);
			const s = cam.dist / 600;
			for (let i = 0; i < 3; i++) cam.target[i] += (-right[i] * dx + up[i] * dy) * s;
		} else {
			cam.yaw -= dx * 0.3;
			cam.pitch = Math.max(-89, Math.min(89, cam.pitch + dy * 0.3));
		}
	});
	const end = () => { drag = null; };
	c.addEventListener('pointerup', end);
	c.addEventListener('pointercancel', end);
	c.addEventListener('wheel', (e) => {
		e.preventDefault();
		if (cam.fly) {
			const { forward } = angleVectors(cam.angles);
			for (let i = 0; i < 3; i++) cam.origin[i] -= forward[i] * e.deltaY * 0.5;
		} else cam.dist = Math.max(16, Math.min(6000, cam.dist * Math.pow(1.0015, e.deltaY)));
	}, { passive: false });
	c.addEventListener('keydown', (e) => {
		const k = e.key.toLowerCase();
		if (cam.fly && 'wasdqe'.includes(k) && !e.ctrlKey && !e.metaKey) { cam.keys.add(k); e.preventDefault(); e.stopPropagation(); }
		if (e.key === 'Shift') cam.keys.add('shift');
	});
	c.addEventListener('keyup', (e) => {
		cam.keys.delete(e.key.toLowerCase());
		if (e.key === 'Shift') cam.keys.delete('shift');
	});
	c.addEventListener('blur', () => cam.keys.clear());
}

//=============================================================================
// the main loop
//=============================================================================

let lastNow = performance.now();
let stepOnce = false;
let fpsSmooth = 60;
let lastQuads = new Float32Array(0);

function spriteOfHandle(h) {
	let n = handleNames.get(h);
	if (n === undefined) {
		n = engine.spriteName(h);
		handleNames.set(h, n);
	}
	return n;
}

function simFrame(t) {
	updateTarget(t);
	if (S.play.mode === 'oneshot' && S.play.autoTrigger > 0 && t >= nextTrigger) {
		trigger();
		nextTrigger = t + S.play.autoTrigger;
	}
	engine.set(P.CAM, ...cam.origin);
	engine.set(P.CAM_ANGLES, ...cam.angles);
	return engine.frame(t);
}

function loop(now) {
	const real = Math.min(0.25, (now - lastNow) / 1000);
	lastNow = now;
	if (real > 0) fpsSmooth = fpsSmooth * 0.95 + (1 / real) * 0.05;
	cameraUpdate(real);
	let quads = null;
	const fps = S.play.fps;
	if (S.play.playing || stepOnce) {
		if (stepOnce && !S.play.playing) {
			simTime += fps ? 1 / fps : 1 / 60;
			quads = simFrame(simTime);
		} else if (!fps) {
			simTime += real * S.play.speed;
			quads = simFrame(simTime);
		} else {
			loop.acc = (loop.acc || 0) + real * S.play.speed;
			const h = 1 / fps;
			let n = 0;
			while (loop.acc >= h && n < 60) {
				loop.acc -= h;
				simTime += h;
				quads = simFrame(simTime);
				n++;
			}
			if (n >= 60) loop.acc = 0;
		}
		stepOnce = false;
	}
	if (!quads) quads = simFrame(simTime);	// the same time: nothing moves, the view is drawn
	lastQuads = quads;
	const mvp = renderer.render({ origin: cam.origin, angles: cam.angles }, sceneGeometry(), quads, spriteOfHandle, simTime);
	updateHud(quads.length / 28, mvp);
	requestAnimationFrame(loop);
}

function sceneGeometry() {
	const sc = S.scene;
	const lines = [];
	const overlay = [];
	const L = (arr, a, b, c) => arr.push(...a, ...c, ...b, ...c);
	const z = sc.floorZ + 0.15;
	if (sc.grid) {
		const minor = [0.22, 0.25, 0.29, 1], major = [0.32, 0.36, 0.42, 1];
		for (let i = -256; i <= 256; i += 16) {
			if (i % 64 === 0) continue;
			L(lines, [i, -256, z], [i, 256, z], minor);
			L(lines, [-256, i, z], [256, i, z], minor);
		}
		for (let i = -1024; i <= 1024; i += 64) {
			L(lines, [i, -1024, z], [i, 1024, z], i === 0 ? [0.25, 0.45, 0.25, 1] : major);
			L(lines, [-1024, i, z], [1024, i, z], i === 0 ? [0.55, 0.25, 0.25, 1] : major);
		}
	}
	if (sc.player) {
		// a 72-unit player: the standing hull (32 x 32 x 72) and a figure inside it
		const px = 96, py = -112, pz = sc.floorZ, col = [0.45, 0.75, 0.95, 1], hull = [0.25, 0.35, 0.45, 1];
		const b = (x0, y0, z0, x1, y1, z1, c) => {
			const p = [[x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0], [x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]];
			for (const [a, bb] of [[0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4], [0, 4], [1, 5], [2, 6], [3, 7]]) L(lines, p[a], p[bb], c);
		};
		b(px - 16, py - 16, pz, px + 16, py + 16, pz + 72, hull);
		const head = pz + 64;
		for (let i = 0; i < 12; i++) {
			const a1 = i / 12 * Math.PI * 2, a2 = (i + 1) / 12 * Math.PI * 2;
			L(lines, [px, py + Math.cos(a1) * 5, head + Math.sin(a1) * 6], [px, py + Math.cos(a2) * 5, head + Math.sin(a2) * 6], col);
		}
		L(lines, [px, py, head - 6], [px, py, pz + 34], col);
		L(lines, [px, py, pz + 34], [px, py - 6, pz], col);
		L(lines, [px, py, pz + 34], [px, py + 6, pz], col);
		L(lines, [px, py, pz + 54], [px, py - 12, pz + 36], col);
		L(lines, [px, py, pz + 54], [px, py + 12, pz + 36], col);
	}
	const em = S.emitter;
	if (em.mode !== 'static') {
		const pc = [0.5, 0.5, 0.6, 1];
		let prev = null;
		for (let i = 0; i <= 64; i++) {
			const p = targetAt(i / 64 * Math.max(0.5, em.period)).pos;
			if (prev) L(lines, prev, p, pc);
			prev = p;
		}
		const tp = targetNow.pos, f = rotateOffset(targetNow.angles, [24, 0, 0]);
		const bc = [0.6, 0.8, 1, 1];
		for (const [a, b] of [[[-8, 0, 0], [8, 0, 0]], [[0, -8, 0], [0, 8, 0]], [[0, 0, -8], [0, 0, 8]]]) L(overlay, tp.map((v, i) => v + a[i]), tp.map((v, i) => v + b[i]), bc);
		L(overlay, tp, tp.map((v, i) => v + f[i]), [0.3, 0.6, 1, 1]);
	}
	const w = emitterWorld();
	const ec = [1, 0.85, 0.3, 1];
	const o = w.origin;
	L(overlay, [o[0] - 6, o[1], o[2]], [o[0] + 6, o[1], o[2]], ec);
	L(overlay, [o[0], o[1] - 6, o[2]], [o[0], o[1] + 6, o[2]], ec);
	L(overlay, [o[0], o[1], o[2] - 6], [o[0], o[1], o[2] + 6], ec);
	// the spray's turn: the entity's angles (pitch down positive) as the direction of a spray with pitch 0 yaw 0
	const d = angleVectors([w.angles[0], w.angles[1], 0]).forward;
	L(overlay, o, o.map((v, i) => v + d[i] * 20), [1, 0.55, 0.2, 1]);
	if (S.compare.enabled) {
		const c2 = [o[0], o[1] - S.compare.offset, o[2]];
		L(overlay, [c2[0] - 6, c2[1], c2[2]], [c2[0] + 6, c2[1], c2[2]], [0.6, 0.9, 1, 1]);
		L(overlay, [c2[0], c2[1], c2[2] - 6], [c2[0], c2[1], c2[2] + 6], [0.6, 0.9, 1, 1]);
	}
	if (sc.lamp) {
		const lp = sc.lampPos, lc = [...hexToRgb(sc.lampColor).map((c) => c / 255), 1];
		for (const v of [[1, 0, 0], [0, 1, 0], [0, 0, 1], [0.7, 0.7, 0], [0.7, -0.7, 0]]) L(overlay, lp.map((p, i) => p - v[i] * 8), lp.map((p, i) => p + v[i] * 8), lc);
	}
	const bg = hexToRgb(sc.background).map((c) => c / 255);
	return {
		background: bg,
		floor: sc.floor ? { z: sc.floorZ, color: [0.13, 0.145, 0.16, 1] } : null,
		wall: sc.wall ? { mins: [sc.wallX, -sc.wallWidth / 2, sc.floorZ], maxs: [sc.wallX + 16, sc.wallWidth / 2, sc.floorZ + sc.wallHeight], color: [0.3, 0.31, 0.33, 1] } : null,
		water: sc.water ? { mins: [sc.waterX - sc.waterSize / 2, sc.waterY - sc.waterSize / 2, sc.floorZ], maxs: [sc.waterX + sc.waterSize / 2, sc.waterY + sc.waterSize / 2, sc.waterTop], color: [0.12, 0.3, 0.45, 0.55] } : null,
		lines,
		overlay,
	};
}

function particleCap(doc) {
	const v = aur.getValue(doc, null, 'particles');
	if (v === undefined) return 100;
	return Math.max(1, Math.min(2048, parseInt(v, 10) || 0));
}

let hudTimer = 0;
function updateHud(quadCount, mvp) {
	const now = performance.now();
	if (now - hudTimer < 120) return;
	hudTimer = now;
	const sys = engine.stat(3, MAIN_ID);
	const cap = particleCap(S.doc);
	const all = engine.stat(0);
	const copies = engine.stat(4);
	const cls = (v, max) => (v >= max ? 'over' : v >= max * 0.9 ? 'near' : '');
	const on = S.play.mode === 'loop' ? (S.play.on ? 'on' : 'off') : 'one-shot';
	let html = `<span class="${cls(sys, cap)}">system   ${String(sys).padStart(5)} / ${cap} particles</span>  (${on})\n`;
	if (S.compare.enabled) html += `compare  ${String(engine.stat(3, COMPARE_ID)).padStart(5)} / ${particleCap(S.compare.doc || S.doc)}\n`;
	html += `copies   ${String(copies).padStart(5)} one-shot (${engine.stat(6)} particles)\n`;
	html += `<span class="${cls(all, 8192)}">all      ${String(all).padStart(5)} / 8192 alive</span>\n`;
	html += `drawn    ${String(quadCount).padStart(5)} quads   ${Math.round(fpsSmooth)} fps   t ${simTime.toFixed(2)} s${S.play.playing ? '' : '  paused'}`;
	$('hud').innerHTML = html;
	// compare labels
	const lab = $('compareLabels');
	if (!S.compare.enabled) { lab.innerHTML = ''; return; }
	const w = emitterWorld().origin;
	const proj = (p) => {
		const x = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8] * p[2] + mvp[12];
		const y = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9] * p[2] + mvp[13];
		const ww = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];
		if (ww <= 0) return null;
		const c = $('view');
		return [(x / ww * 0.5 + 0.5) * c.clientWidth, (1 - (y / ww * 0.5 + 0.5)) * c.clientHeight];
	};
	const a = proj([w[0], w[1], w[2] - 12]), b = proj([w[0], w[1] - S.compare.offset, w[2] - 12]);
	lab.innerHTML = (a ? `<div style="left:${a[0]}px;top:${a[1]}px">${util.escapeHtml(S.name)}</div>` : '') +
		(b ? `<div style="left:${b[0]}px;top:${b[1]}px">${util.escapeHtml(S.compare.name)}</div>` : '');
}

//=============================================================================
// the type tree
//=============================================================================

function blockSprite(block) {
	return block ? aur.getValue(S.doc, block, 'sprite') : undefined;
}

function renderTree() {
	const tree = $('tree');
	const doc = S.doc;
	const t = aur.typeTree(doc);
	tree.innerHTML = '';
	const top = document.createElement('div');
	top.className = 'node' + (S.selected === 'top' ? ' selected' : '');
	top.innerHTML = '<span class="label">Script settings</span><span class="rel">top</span>';
	top.onclick = () => select('top');
	tree.appendChild(top);

	const make = (n, parent) => {
		const el = document.createElement('div');
		const idx = n.block ? doc.blocks.indexOf(n.block) : -1;
		el.className = 'node' + (idx >= 0 && S.selected === idx ? ' selected' : '') + (n.missing ? ' missing' : '');
		el.title = n.missing ? `No block defines "${n.name}"` : n.cycle ? 'Refers back to a type above it' : '';
		const sw = document.createElement('canvas');
		sw.className = 'swatch';
		sw.width = sw.height = 18;
		el.appendChild(sw);
		paintSwatch(sw, blockSprite(n.block));
		const label = document.createElement('span');
		label.className = 'label';
		label.textContent = n.name + (n.cycle ? ' (again)' : '');
		el.appendChild(label);
		const rel = document.createElement('span');
		rel.className = `rel ${n.rel}`;
		rel.textContent = n.rel;
		el.appendChild(rel);
		if (idx >= 0) el.onclick = () => select(idx);
		parent.appendChild(el);
		if (n.children.length && !n.cycle) {
			const ch = document.createElement('div');
			ch.className = 'children';
			n.children.forEach((c) => make(c, ch));
			parent.appendChild(ch);
		}
	};
	t.roots.forEach((r) => make(r, tree));
	if (!t.roots.length) {
		const p = document.createElement('p');
		p.className = 'note';
		p.textContent = 'No "maintype": the system emits nothing. Set one in Script settings or with "Set as main".';
		tree.appendChild(p);
	}
	if (t.unused.length) {
		const h = document.createElement('h4');
		h.textContent = 'Not used by the main type';
		tree.appendChild(h);
		for (const b of t.unused) make({ name: b.name || '(no name)', rel: 'type', block: b, children: [] }, tree);
	}
	const sel = typeof S.selected === 'number';
	for (const id of ['btnAddSpray', 'btnAddOverlay', 'btnRename', 'btnDuplicate', 'btnSetMain', 'btnDelete']) $(id).disabled = !sel;
}

function paintSwatch(canvas, sprite) {
	const ctx = canvas.getContext('2d');
	ctx.fillStyle = '#000';
	ctx.fillRect(0, 0, canvas.width, canvas.height);
	if (!sprite) return;
	ensureSprite(sprite).then((info) => {
		if (info.state !== 'ok') {
			ctx.fillStyle = '#5a2020';
			ctx.fillRect(0, 0, canvas.width, canvas.height);
			return;
		}
		drawFrame(canvas, info.spr, info.spr.frames[0].images[0], '#000');
	});
}

function select(sel) {
	S.selected = sel;
	renderTree();
	renderForm();
	showTab('right', 'props');
}

function selectedBlock() {
	return typeof S.selected === 'number' ? S.doc.blocks[S.selected] : null;
}

function uniqueTypeName(base) {
	let n = base;
	let i = 2;
	while (aur.findBlock(S.doc, n)) n = `${base}${i++}`;
	return n;
}

async function askName(title, value) {
	let result = null;
	await dialog(`<h2>${util.escapeHtml(title)}</h2><input id="askInput" style="width:100%" maxlength="29" spellcheck="false" value="${util.escapeHtml(value)}"><p class="note">29 characters at most; letters, digits and _ are safest.</p>`,
		[['Cancel', 'cancel'], ['OK', 'ok', 'primary']], (body, close) => {
			const i = body.querySelector('#askInput');
			i.select();
			i.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); close('ok'); } });
			$('dialog').addEventListener('close', () => { if ($('dialog').returnValue === 'ok') result = i.value.trim(); }, { once: true });
		});
	return result;
}

async function addType(rel) {
	const parent = selectedBlock();
	const name = await askName(rel === 'spray' ? 'New spray child' : rel === 'overlay' ? 'New overlay type' : 'New type', uniqueTypeName(rel === 'overlay' ? 'overlay' : 'particle'));
	if (!name) return;
	if (aur.findBlock(S.doc, name)) return toast(`A type "${name}" exists already`, true);
	let text = S.text;
	if (parent && rel) {
		const doc = aur.parse(text);
		const pb = doc.blocks[S.selected];
		text = aur.setValue(doc, pb, rel === 'spray' ? 'spraytype' : 'overlaytype', name);
		if (rel === 'spray' && aur.getValue(doc, pb, 'sprayrate') === undefined) {
			const d2 = aur.parse(text);
			text = aur.setValue(d2, d2.blocks[S.selected], 'sprayrate', '10');
		}
	}
	const lines = rel === 'overlay' ? NEW_TYPE_LINES.filter(([k]) => k !== 'lifetime') : NEW_TYPE_LINES;
	text = aur.addType(text, name, lines);
	if (!aur.getValue(aur.parse(text), null, 'maintype')) text = aur.setValue(aur.parse(text), null, 'maintype', name);
	setText(text, 'tree');
	select(aur.parse(text).blocks.length - 1);
}

async function renameSelected() {
	const b = selectedBlock();
	if (!b) return;
	const name = await askName('Rename type', b.name);
	if (!name || name === b.name) return;
	if (aur.findBlock(S.doc, name)) return toast(`A type "${name}" exists already`, true);
	setText(aur.renameType(S.doc, b.name, name), 'tree');
}

function duplicateSelected() {
	const b = selectedBlock();
	if (!b) return;
	const name = uniqueTypeName(b.name || 'type');
	let block = aur.blockText(S.doc, b);
	const nd = aur.parse(block);
	block = nd.blocks.length ? aur.renameType(nd, b.name, name) : block;
	if (!b.name) block = block.replace('{', `{\n  "name" "${name}"`);
	const text = S.text.replace(/\s*$/, '\n\n') + block + '\n';
	setText(text, 'tree');
	select(S.doc.blocks.length - 1);
}

function deleteSelected() {
	const b = selectedBlock();
	if (!b) return;
	setText(aur.removeBlock(S.doc, b), 'tree');
	S.selected = 'top';
	renderTree();
	renderForm();
	toast(`Deleted "${b.name}"; references to it stay (see Problems). Undo with Ctrl+Z.`);
}

function setMainSelected() {
	const b = selectedBlock();
	if (!b || !b.name) return;
	setText(aur.setValue(S.doc, null, 'maintype', b.name), 'tree');
}

//=============================================================================
// the properties form
//=============================================================================

function field(spec, value, onSet, onReset, extra = {}) {
	const row = document.createElement('div');
	row.className = 'field' + (value !== undefined ? ' set' : '');
	const label = document.createElement('label');
	label.textContent = spec.label || spec.key;
	const def = spec.default !== undefined ? `\nDefault: ${spec.default === '' ? 'none' : spec.default}` : '';
	label.title = `"${spec.key}"\n${spec.help || ''}${def}${spec.unit ? `\nUnit: ${spec.unit}` : ''}`;
	row.appendChild(label);
	const ctl = document.createElement('div');
	ctl.className = 'ctl';
	row.appendChild(ctl);
	const reset = document.createElement('button');
	reset.className = 'reset';
	reset.type = 'button';
	reset.textContent = '×';
	reset.title = `Remove "${spec.key}" (back to the default)`;
	reset.style.visibility = value !== undefined ? 'visible' : 'hidden';
	reset.onclick = () => onReset();
	row.appendChild(reset);

	const kind = spec.kind;
	if (kind === 'enum' || kind === 'type') {
		const sel = document.createElement('select');
		const opts = kind === 'enum' ? spec.choices : ['', ...extra.types];
		const cur = value === undefined ? '' : value;
		const add = (v, text) => {
			const o = document.createElement('option');
			o.value = v;
			o.textContent = text;
			sel.appendChild(o);
		};
		if (kind === 'enum' && !spec.choices.includes('')) add('', `(default: ${spec.default})`);
		for (const c of opts) add(c, c === '' ? (kind === 'type' ? '(none)' : '(none)') : c);
		if (cur !== '' && !opts.some((o) => o.toLowerCase() === cur.toLowerCase())) add(cur, `${cur} (${kind === 'type' ? 'missing' : 'unknown'})`);
		sel.value = opts.find((o) => o.toLowerCase() === cur.toLowerCase()) ?? cur;
		sel.onchange = () => (sel.value === '' ? onReset() : onSet(sel.value));
		ctl.appendChild(sel);
		if (kind === 'type' && extra.onNew) {
			const b = document.createElement('button');
			b.type = 'button';
			b.textContent = '+';
			b.title = 'A new type for this';
			b.onclick = extra.onNew;
			ctl.appendChild(b);
		}
		return row;
	}

	const input = document.createElement('input');
	input.type = 'text';
	input.spellcheck = false;
	input.value = value === undefined ? '' : value;
	input.placeholder = spec.default !== undefined ? String(spec.default) : '';
	ctl.appendChild(input);

	if (kind === 'sprite') {
		input.setAttribute('list', 'spriteList');
		input.onchange = () => (input.value.trim() === '' ? onReset() : onSet(input.value.trim()));
		const b = document.createElement('button');
		b.type = 'button';
		b.textContent = 'Browse';
		b.onclick = () => { showTab('left', 'sprites'); if (value) showSpriteDetail(normSprite(value)); };
		ctl.appendChild(b);
		if (value !== undefined && !knownSprite(value)) input.classList.add('bad');
		return row;
	}
	if (kind === 'name') {
		input.maxLength = 64;
		input.onchange = () => onSet(input.value.trim());
		return row;
	}

	// numbers and ranges
	let slider = null;
	const r = aur.parseRange(value);
	const check = () => {
		const v = input.value.trim();
		const ok = v === '' || /^\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?\s*(\.\.\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?\s*)?$/.test(v);
		input.classList.toggle('bad', !ok || (kind === 'int' && v !== '' && !/^\s*[+-]?\d+\s*$/.test(v)));
		input.title = ok ? '' : 'The game reads this as ' + aur.formatRange(aur.parseRange(v).min, aur.parseRange(v).max);
	};
	check();
	input.oninput = () => {
		check();
		const v = input.value.trim();
		if (slider && v !== '' && !v.includes('..')) slider.value = aur.atof(v);
		if (v === '') onReset(true); else onSet(v, true);
	};
	if (spec.ui) {
		slider = document.createElement('input');
		slider.type = 'range';
		slider.min = spec.ui[0];
		slider.max = spec.ui[1];
		slider.step = spec.step || 'any';
		const sv = r ? r.min : spec.default ?? spec.ui[0];
		slider.value = sv;
		slider.title = r && r.range ? 'A range: the slider sets one value' : '';
		slider.oninput = () => {
			const v = aur.formatNumber(parseFloat(slider.value));
			input.value = v;
			check();
			onSet(v, true);
		};
		ctl.insertBefore(slider, input);
		input.style.maxWidth = '96px';
	}
	if (spec.unit) {
		const u = document.createElement('span');
		u.className = 'unit';
		u.textContent = spec.unit;
		ctl.appendChild(u);
	}
	return row;
}

// block: the form's block (null = the top level); the edit goes to the selected block of the current text (the
// form is not drawn again while a field has the focus, so its block objects may be from an older parse)
function editKey(block, key, value, live) {
	const b = block ? selectedBlock() : null;
	if (block && !b) return;
	setText(aur.setValue(S.doc, b, key, value), 'form', live ? `form:${S.selected}:${key}` : undefined);
}

function removeKey(block, key, live) {
	const b = block ? selectedBlock() : null;
	if (block && !b) return;
	if (aur.getValue(S.doc, b, key) === undefined && !(b ? b.entries : S.doc.top).some((e) => e.key === key)) return;
	setText(aur.removeKey(S.doc, b, key), 'form', live ? `form:${S.selected}:${key}` : undefined);
	if (!live) renderForm();
}

function typeNames() {
	return S.doc.blocks.map((b) => b.name).filter((n, i, a) => n && a.indexOf(n) === i);
}

function renderForm() {
	formStale = false;
	const form = $('form');
	form.innerHTML = '';
	const schema = S.schema;
	const doc = S.doc;
	if (!schema || !doc) return;
	const block = selectedBlock();
	const datalist = document.createElement('datalist');
	datalist.id = 'spriteList';
	for (const s of allSpriteNames().filter((n) => !isUiSprite(n))) {
		const o = document.createElement('option');
		o.value = s;
		datalist.appendChild(o);
	}
	form.appendChild(datalist);

	if (!block) {
		const h = document.createElement('h3');
		h.textContent = 'Script settings';
		form.appendChild(h);
		for (const spec of schema.top) {
			if (spec.xash && !S.coreHasXash) continue;
			const v = aur.getValue(doc, null, spec.key);
			form.appendChild(field(spec, v, (val, live) => editKey(null, spec.key, val, live), (live) => removeKey(null, spec.key, live),
				{ types: typeNames() }));
		}
		const est = aur.lint(doc, lintContext());
		const p = document.createElement('p');
		p.className = 'info-line';
		p.textContent = `Estimated alive at once: about ${Math.round(est.estimate)} (sprayrate x lifetime along the chain); "particles" is ${est.particles}.`;
		form.appendChild(p);
		const unknown = doc.top.filter((e) => !e.known);
		if (unknown.length) {
			const h2 = document.createElement('h3');
			h2.textContent = 'Keys the game does not read here';
			form.appendChild(h2);
			for (const e of unknown) {
				const f = schema.foreign.find((x) => x.key === e.key && x.scope === 'top');
				const d = document.createElement('div');
				d.className = 'info-line';
				d.innerHTML = `<code>"${util.escapeHtml(e.keyTok.text)}" "${util.escapeHtml(e.valTok ? e.valTok.text : '')}"</code> ${f ? util.escapeHtml(f.help) : 'unknown key: the game skips it with a console line.'}`;
				if (e.key === 'killcondition' || e.key === 'lightmodel') d.textContent += ' (this game build does not read it yet)';
				form.appendChild(d);
			}
		}
		return;
	}

	const groups = schema.groups;
	for (const g of groups) {
		const h = document.createElement('h3');
		h.textContent = g.label;
		form.appendChild(h);
		if (g.id === 'color') form.appendChild(colorRow(block));
		for (const spec of schema.type.filter((t) => t.group === g.id)) {
			if (spec.xash && !S.coreTypeKill) continue;
			const v = aur.getValue(doc, block, spec.key);
			const onSet = spec.key === 'name'
				? (val) => {
					const cur = selectedBlock();
					if (!val || !cur) return;
					if (aur.findBlock(S.doc, val) && val.toLowerCase() !== (cur.name || '').toLowerCase()) return toast(`A type "${val}" exists already`, true);
					setText(cur.name ? aur.renameType(S.doc, cur.name, val) : aur.setValue(S.doc, cur, 'name', val), 'form');
				}
				: (val, live) => editKey(block, spec.key, val, live);
			const extra = { types: typeNames() };
			if (spec.kind === 'type') extra.onNew = () => addType(spec.key === 'spraytype' ? 'spray' : 'overlay');
			form.appendChild(field(spec, v, onSet, (live) => removeKey(block, spec.key, live), extra));
		}
	}
	const unknown = block.entries.filter((e) => !e.known);
	if (unknown.length) {
		const h = document.createElement('h3');
		h.textContent = 'Keys the game does not read';
		form.appendChild(h);
		for (const e of unknown) {
			const f = schema.foreign.find((x) => x.key === e.key && x.scope === 'type');
			const d = document.createElement('div');
			d.className = 'info-line';
			d.innerHTML = `<code>"${util.escapeHtml(e.keyTok.text)}" "${util.escapeHtml(e.valTok ? e.valTok.text : '')}"</code> ${f ? util.escapeHtml(f.help) : 'unknown key: the game skips it (and its value) with a console line.'}`;
			if (e.key === 'killcondition' && e.valTok) {
				const b = document.createElement('button');
				b.textContent = 'Move to the top level';
				b.onclick = () => {
					let t = aur.removeKey(S.doc, selectedBlock(), 'killcondition');
					t = aur.setValue(aur.parse(t), null, 'killcondition', e.valTok.text);
					setText(t, 'form');
					renderForm();
				};
				d.appendChild(document.createTextNode(' '));
				d.appendChild(b);
			}
			form.appendChild(d);
		}
	}
}

function colorRow(block) {
	const doc = S.doc;
	const mid = (k, d) => { const r = aur.parseRange(aur.getValue(doc, block, k)); return r ? (r.min + r.max) / 2 : d; };
	const sc = ['startred', 'startgreen', 'startblue'].map((k) => mid(k, 1));
	const ec = ['endred', 'endgreen', 'endblue'].map((k) => mid(k, 1));
	const sa = mid('startalpha', 1), ea = mid('endalpha', 1);
	const row = document.createElement('div');
	row.className = 'colors';
	const mk = (label, rgb, keys) => {
		const l = document.createElement('label');
		l.textContent = label;
		const i = document.createElement('input');
		i.type = 'color';
		i.value = rgbToHex(rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
		i.title = 'Sets the three keys to one value each (ranges are replaced)';
		i.oninput = () => {
			const c = hexToRgb(i.value).map((x) => aur.formatNumber(Math.round(x / 255 * 1000) / 1000));
			let t = S.text;
			keys.forEach((k, j) => { t = aur.setValue(aur.parse(t), aur.parse(t).blocks[S.selected], k, c[j]); });
			setText(t, 'form', `form:${S.selected}:${keys[0]}`);
			grad.style.background = gradient();
		};
		l.appendChild(i);
		row.appendChild(l);
	};
	const gradient = () => {
		const d = S.doc, b = d.blocks[S.selected];
		const g = (k, def) => { const r = aur.parseRange(aur.getValue(d, b, k)); return Math.max(0, Math.min(1, r ? (r.min + r.max) / 2 : def)); };
		const s = ['startred', 'startgreen', 'startblue'].map((k) => g(k, 1) * 255);
		const e = ['endred', 'endgreen', 'endblue'].map((k) => g(k, 1) * 255);
		return `linear-gradient(90deg, rgba(${s.join(',')},${g('startalpha', 1)}), rgba(${e.join(',')},${g('endalpha', 1)})), repeating-conic-gradient(#333 0 25%, #555 0 50%) 0 0/10px 10px`;
	};
	mk('Start', sc, ['startred', 'startgreen', 'startblue']);
	const grad = document.createElement('div');
	grad.className = 'gradient';
	grad.style.background = gradient();
	grad.title = `Color and alpha over the life (middle of each range): alpha ${aur.formatNumber(sa)} to ${aur.formatNumber(ea)}`;
	row.appendChild(grad);
	mk('End', ec, ['endred', 'endgreen', 'endblue']);
	return row;
}

//=============================================================================
// the script text view
//=============================================================================

function setTextarea(text) {
	const ta = $('scriptText');
	if (ta.value === text) return;
	const top = ta.scrollTop, s = ta.selectionStart;
	ta.value = text;
	ta.scrollTop = top;
	try { ta.setSelectionRange(Math.min(s, text.length), Math.min(s, text.length)); } catch { /* not focused */ }
}

function updateLineNumbers() {
	const n = S.text.split('\n').length;
	const ln = $('lineNumbers');
	if (ln.dataset.n !== String(n)) {
		ln.dataset.n = n;
		let s = '';
		for (let i = 1; i <= n; i++) s += i + '\n';
		ln.textContent = s;
	}
	ln.scrollTop = $('scriptText').scrollTop;
}

function revealInText(pos, end) {
	showTab('right', 'text');
	const ta = $('scriptText');
	ta.focus();
	ta.setSelectionRange(pos, end ?? pos);
	const line = S.text.slice(0, pos).split('\n').length;
	ta.scrollTop = Math.max(0, (line - 5) * 18);
	updateLineNumbers();
}

//=============================================================================
// problems: the game's own console lines, then the editor's hints
//=============================================================================

function lintContext() {
	const kinds = {};
	for (const t of S.schema.type) kinds[t.key] = t.kind;
	return { limits: S.schema.limits, foreign: S.schema.foreign, spriteExists: knownSprite, schemaKinds: kinds };
}

function renderProblems() {
	const box = $('problems');
	box.innerHTML = '';
	const add = (cls, lv, html, pos) => {
		const d = document.createElement('div');
		d.className = `problem ${cls}`;
		d.innerHTML = `<span class="lv">${lv}</span><span>${html}</span>`;
		if (pos) {
			d.dataset.pos = pos[0];
			d.title = 'Show in the script';
			d.onclick = () => revealInText(pos[0], pos[1]);
		}
		box.appendChild(d);
	};
	const prefix = `Aurora: ${scriptPath()}: `;
	let gameCount = 0;
	for (const line of S.gameLog) {
		let text = line.startsWith(prefix) ? line.slice(prefix.length) : line.replace(/^Aurora: /, '');
		let pos = null;
		const m = /^unknown key (.+)$/.exec(text);
		if (m) {
			const e = [...S.doc.top, ...S.doc.blocks.flatMap((b) => b.entries)].find((x) => !x.known && x.keyTok.text === m[1]);
			if (e) pos = [e.keyTok.start, (e.valTok || e.keyTok).end];
		}
		add('game', 'game', `${util.escapeHtml(text)}`, pos);
		gameCount++;
	}
	const { hints } = aur.lint(S.doc, lintContext());
	for (const h of hints) {
		let pos = null;
		if (h.where && h.where.keyTok) pos = [h.where.keyTok.start, (h.where.valTok || h.where.keyTok).end];
		else if (h.where && h.where.open) pos = [h.where.open.start, h.where.open.end];
		add(h.level, h.level === 'warn' ? 'check' : 'note', util.escapeHtml(h.text), pos);
	}
	if (!gameCount && !hints.length) add('ok', 'ok', 'The game reads this script without a console line.');
	const badge = $('problemCount');
	const warn = gameCount + hints.filter((h) => h.level === 'warn').length;
	badge.textContent = warn ? String(warn) : '';
	badge.classList.toggle('err', gameCount > 0);
	const note = document.createElement('p');
	note.className = 'note';
	note.textContent = '"game" lines are what the game prints in its console for this script (its own parser, run here). "check" and "note" are the editor\'s hints.';
	box.appendChild(note);
}

//=============================================================================
// scene, emitter and playback settings
//=============================================================================

function sceneControl(form, label, type, get, set, opts = {}) {
	const row = document.createElement('div');
	row.className = 'field row2';
	const l = document.createElement('label');
	l.textContent = label;
	l.title = opts.help || '';
	row.appendChild(l);
	const ctl = document.createElement('div');
	ctl.className = 'ctl';
	row.appendChild(ctl);
	if (type === 'check') {
		const i = document.createElement('input');
		i.type = 'checkbox';
		i.checked = !!get();
		if (opts.id) i.id = opts.id;
		i.onchange = () => set(i.checked);
		ctl.appendChild(i);
	} else if (type === 'select') {
		const s = document.createElement('select');
		for (const [v, t] of opts.choices) {
			const o = document.createElement('option');
			o.value = v;
			o.textContent = t;
			s.appendChild(o);
		}
		s.value = String(get());
		s.onchange = () => set(s.value);
		ctl.appendChild(s);
	} else if (type === 'color') {
		const i = document.createElement('input');
		i.type = 'color';
		i.value = get();
		i.oninput = () => set(i.value);
		ctl.appendChild(i);
	} else if (type === 'vec') {
		const v = document.createElement('div');
		v.className = 'vec';
		const cur = get();
		cur.forEach((c, k) => {
			const i = document.createElement('input');
			i.type = 'number';
			i.step = opts.step || 1;
			i.value = c;
			i.title = (opts.labels || ['x', 'y', 'z'])[k];
			i.oninput = () => { const a = get().slice(); a[k] = parseFloat(i.value) || 0; set(a); };
			v.appendChild(i);
		});
		ctl.appendChild(v);
	} else {
		const i = document.createElement('input');
		i.type = 'number';
		i.step = opts.step || 1;
		if (opts.min !== undefined) i.min = opts.min;
		if (opts.max !== undefined) i.max = opts.max;
		i.value = get();
		i.oninput = () => set(parseFloat(i.value) || 0);
		ctl.appendChild(i);
		if (opts.range) {
			const r = document.createElement('input');
			r.type = 'range';
			r.min = opts.min;
			r.max = opts.max;
			r.step = opts.step || 1;
			r.value = get();
			r.oninput = () => { i.value = r.value; set(parseFloat(r.value)); };
			i.oninput = () => { r.value = i.value; set(parseFloat(i.value) || 0); };
			ctl.insertBefore(r, i);
		}
	}
	form.appendChild(row);
	return row;
}

function renderSceneForm() {
	const f = $('sceneForm');
	f.innerHTML = '';
	const sc = S.scene, em = S.emitter;
	const h = (t) => { const e = document.createElement('h3'); e.textContent = t; f.appendChild(e); };
	const changed = (resend) => { applySceneParams(); saveSettings(); if (resend) resendPlacement(); };

	h('Emitter (the env_particle)');
	sceneControl(f, 'Placement', 'select', () => em.mode, (v) => { em.mode = v; changed(true); renderSceneForm(); frameCamera(); }, {
		choices: [['static', 'A point (no target)'], ['follow', 'Moving target ("Move with entity")']],
		help: 'A point: the entity where it is. Moving target: the entity\'s target key, following a func_train or a monster; the offset and angles turn with it.',
	});
	if (em.mode === 'static') {
		sceneControl(f, 'Origin', 'vec', () => em.origin, (v) => { em.origin = v; changed(true); });
	} else {
		sceneControl(f, 'Path', 'select', () => em.path, (v) => { em.path = v; changed(); }, {
			choices: [['circle', 'Circle'], ['line', 'Back and forth'], ['figure8', 'Figure eight'], ['spin', 'Turn in place'], ['still', 'Standing still']],
		});
		sceneControl(f, 'Radius / length', 'vec', () => [em.radius, em.length], (v) => { em.radius = v[0]; em.length = v[1]; changed(); }, { labels: ['radius', 'length'] });
		sceneControl(f, 'Lap time (s)', 'number', () => em.period, (v) => { em.period = Math.max(0.5, v); changed(); }, { step: 0.5, min: 0.5 });
		sceneControl(f, 'Height', 'number', () => em.height, (v) => { em.height = v; changed(); });
		sceneControl(f, 'Turn with the path', 'check', () => em.turn, (v) => { em.turn = v; changed(); });
		sceneControl(f, 'Offset in its frame', 'vec', () => em.offset, (v) => { em.offset = v; changed(true); }, { help: 'The env_particle\'s position relative to the target, in the target\'s frame (forward, left, up)', labels: ['forward', 'left', 'up'] });
		sceneControl(f, 'Attachment', 'select', () => em.attachment, (v) => { em.attachment = parseInt(v, 10); changed(true); }, {
			choices: [['-1', 'None (origin + offset)'], ['0', 'Attachment 0'], ['1', 'Attachment 1'], ['2', 'Attachment 2'], ['3', 'Attachment 3']],
			help: 'On a studio model the effect sits on an attachment point (here: a point ahead of the target, like a gun\'s muzzle)',
		});
		sceneControl(f, 'Target is a .mdl', 'check', () => em.targetStudio, (v) => { em.targetStudio = v; changed(); }, { help: 'Attachments work on studio models only (a brush entity ignores them)' });
		sceneControl(f, 'Target visible', 'check', () => em.targetVisible, (v) => { em.targetVisible = v; changed(); }, { help: 'A followed entity that is not in the client\'s frame hides the effect' });
	}
	sceneControl(f, 'Angles', 'vec', () => em.angles, (v) => { em.angles = v; changed(true); }, { labels: ['pitch (down +)', 'yaw', 'roll (unused)'], help: 'The entity\'s angles: added to spraypitch / sprayyaw (pitch 90 = straight down). The orange line shows where a spray of pitch 0 yaw 0 goes.' });
	const quick = document.createElement('div');
	quick.className = 'toolbar';
	for (const [t, a] of [['Up', [-90, 0, 0]], ['Down', [90, 0, 0]], ['East', [0, 0, 0]], ['North', [0, 90, 0]], ['West', [0, 180, 0]], ['South', [0, 270, 0]]]) {
		const b = document.createElement('button');
		b.textContent = t;
		b.title = `angles ${a.join(' ')}`;
		b.onclick = () => { em.angles = a.slice(); changed(true); renderSceneForm(); };
		quick.appendChild(b);
	}
	f.appendChild(quick);

	h('World');
	sceneControl(f, 'Floor', 'check', () => sc.floor, (v) => { sc.floor = v; changed(); }, { help: 'Solid below this height: collision traces hit it, drawcondition solid is true under it' });
	sceneControl(f, 'Floor height', 'number', () => sc.floorZ, (v) => { sc.floorZ = v; changed(); });
	sceneControl(f, 'Wall', 'check', () => sc.wall, (v) => { sc.wall = v; changed(); }, { help: 'A 16-unit thick wall facing the origin, for bounce and die' });
	sceneControl(f, 'Wall x / width / height', 'vec', () => [sc.wallX, sc.wallWidth, sc.wallHeight], (v) => { [sc.wallX, sc.wallWidth, sc.wallHeight] = v; changed(); }, { labels: ['x', 'width', 'height'] });
	sceneControl(f, 'Water', 'check', () => sc.water, (v) => { sc.water = v; changed(); }, { help: 'A water volume: drawcondition water / empty and killcondition water. Particles do not collide with water.' });
	sceneControl(f, 'Water x / y / size / top', 'vec', () => [sc.waterX, sc.waterY, sc.waterSize, sc.waterTop], (v) => { [sc.waterX, sc.waterY, sc.waterSize, sc.waterTop] = v; changed(); }, { labels: ['x', 'y', 'size', 'top'] });
	sceneControl(f, 'In the PVS', 'check', () => sc.pvs, (v) => { sc.pvs = v; changed(); }, { help: 'Off: as if the player could not see the place: the system stands still and is not drawn' });

	h('Light ("lightmodel")');
	sceneControl(f, 'World light', 'color', () => sc.lightColor, (v) => { sc.lightColor = v; changed(); }, { help: 'The light LightAtPoint returns (lightmap and dynamic lights in the game). Only scripts with "lightmodel" use it.' });
	sceneControl(f, 'Light level', 'number', () => sc.lightLevel, (v) => { sc.lightLevel = v; changed(); }, { min: 0, max: 255, range: true });
	sceneControl(f, 'Lamp', 'check', () => sc.lamp, (v) => { sc.lamp = v; changed(); }, { help: 'A light added near a point, fading to nothing at its radius' });
	sceneControl(f, 'Lamp position', 'vec', () => sc.lampPos, (v) => { sc.lampPos = v; changed(); });
	sceneControl(f, 'Lamp radius', 'number', () => sc.lampRadius, (v) => { sc.lampRadius = v; changed(); }, { min: 0, max: 2048, range: true, step: 8 });
	sceneControl(f, 'Lamp color', 'color', () => sc.lampColor, (v) => { sc.lampColor = v; changed(); });

	h('View');
	sceneControl(f, 'Background', 'color', () => sc.background, (v) => { sc.background = v; changed(); });
	sceneControl(f, 'Grid (16 / 64 units)', 'check', () => sc.grid, (v) => { sc.grid = v; changed(); });
	sceneControl(f, 'Player for scale (72 units)', 'check', () => sc.player, (v) => { sc.player = v; changed(); });
	sceneControl(f, 'Field of view', 'number', () => sc.fov, (v) => { sc.fov = Math.max(10, Math.min(150, v)); changed(); }, { min: 10, max: 150, range: true, help: 'Horizontal, as default_fov (90)' });
	sceneControl(f, 'Fly camera', 'check', () => cam.fly, (v) => {
		cam.fly = v;
		if (v) { cam.angles = [cam.pitch, cam.yaw, 0]; } else frameCamera();
		toast(v ? 'Fly: drag to look, W A S D to move, Q / E down / up, Shift faster (click the view first)' : 'Orbit camera');
	}, { id: 'flyMode' });
	sceneControl(f, 'Random seed', 'number', () => S.play.seed, (v) => { S.play.seed = v | 0; saveSettings(); rebuild(); }, { help: 'The same seed gives the same particles after Restart' });
	const reset = document.createElement('button');
	reset.textContent = 'Reset the scene';
	reset.onclick = () => {
		S.scene = { ...DEFAULT_SCENE };
		S.emitter = structuredClone(DEFAULT_EMITTER);
		changed(true);
		renderSceneForm();
		frameCamera();
	};
	f.appendChild(reset);
}

//=============================================================================
// compare
//=============================================================================

function renderCompareForm() {
	const f = $('compareForm');
	f.innerHTML = '';
	const p = document.createElement('p');
	p.className = 'note';
	p.textContent = 'Shows a second script next to yours (the same entity settings, placed beside it). Both share the game\'s 8192 particle limit, as in a map.';
	f.appendChild(p);
	sceneControl(f, 'Compare', 'check', () => S.compare.enabled, (v) => {
		if (v && !S.compare.doc) { S.compare.name = S.name + '-before'; S.compare.text = S.text; S.compare.doc = aur.parse(S.text); }
		S.compare.enabled = v;
		rebuild();
		renderCompareForm();
		frameCamera();
	});
	const pick = document.createElement('div');
	pick.className = 'toolbar';
	const sel = document.createElement('select');
	const opt = (v, t) => { const o = document.createElement('option'); o.value = v; o.textContent = t; sel.appendChild(o); };
	opt('', 'Choose a script...');
	opt('@snapshot', 'A copy of the current script (now)');
	for (const s of S.assets.scripts) opt('asset:' + s.name, s.name.replace(/^aurora\//, ''));
	for (const [n] of S.uploads) if (n.startsWith('aurora/')) opt('upload:' + n, n.replace(/^aurora\//, '') + ' (uploaded)');
	sel.onchange = async () => {
		const v = sel.value;
		if (!v) return;
		let name, text;
		if (v === '@snapshot') { name = S.name + '-snapshot'; text = S.text; } else if (v.startsWith('upload:')) {
			name = v.slice(7).replace(/^aurora\//, '').replace(/\.aur$/, '');
			text = S.uploads.get(v.slice(7));
		} else {
			name = v.slice(6).replace(/^aurora\//, '').replace(/\.aur$/, '');
			text = await fetchText(`assets/${v.slice(6)}`);
		}
		S.compare = { ...S.compare, enabled: true, name, text, doc: aur.parse(text) };
		rebuild();
		renderCompareForm();
		frameCamera();
	};
	pick.appendChild(sel);
	f.appendChild(pick);
	if (S.compare.doc) {
		const i = document.createElement('p');
		i.className = 'info-line';
		i.textContent = `Comparing with: ${S.compare.name}`;
		f.appendChild(i);
	}
	sceneControl(f, 'Distance', 'number', () => S.compare.offset, (v) => { S.compare.offset = v; resendPlacement(); }, { min: 32, max: 1024, step: 16, range: true });
	const swap = document.createElement('button');
	swap.textContent = 'Edit the compared script instead';
	swap.disabled = !S.compare.doc;
	swap.onclick = () => {
		const a = { name: S.name, text: S.text };
		loadScript(S.compare.name, S.compare.text);
		S.compare = { ...S.compare, name: a.name, text: a.text, doc: aur.parse(a.text) };
		renderCompareForm();
		rebuild();
	};
	f.appendChild(swap);
}

//=============================================================================
// sprite browser
//=============================================================================

function allSpriteNames() {
	const set = new Set(S.assets.sprites.map((s) => s.name));
	for (const n of S.uploads.keys()) if (n.startsWith('sprites/')) set.add(n);
	return [...set].sort();
}

// a HUD / UI sprite (the server's rule: listed by a hud.txt / weapon_*.txt, or a HUD / icon / border name); uploads
// always show
function isUiSprite(name) {
	if (S.uploads.has(name)) return false;
	const a = S.assets.sprites.find((s) => s.name === name);
	return !!(a && a.ui);
}

function spriteSource(name) {
	if (S.uploads.has(name)) return 'upload';
	const a = S.assets.sprites.find((s) => s.name === name);
	return a ? a.source : '';
}

let thumbObserver = null;

function renderSpriteGrid() {
	const grid = $('spriteGrid');
	const filter = $('spriteFilter').value.trim().toLowerCase();
	const src = $('spriteSource').value;
	grid.innerHTML = '';
	if (thumbObserver) thumbObserver.disconnect();
	thumbObserver = new IntersectionObserver((entries) => {
		for (const en of entries) {
			if (!en.isIntersecting) continue;
			thumbObserver.unobserve(en.target);
			const name = en.target.dataset.name;
			ensureSprite(name).then((info) => {
				const box = en.target.querySelector('.thumb');
				if (info.state !== 'ok') { box.textContent = info.state === 'bad' ? 'bad' : '?'; box.title = info.error || 'not found'; return; }
				const c = document.createElement('canvas');
				const im = info.spr.images[info.spr.frames[0].images[0]];
				const s = Math.min(64 / im.w, 64 / im.h, 2);
				c.width = Math.max(1, Math.round(im.w * s));
				c.height = Math.max(1, Math.round(im.h * s));
				drawFrame(c, info.spr, info.spr.frames[0].images[0], info.spr.texFormat === 1 ? '#000' : 'check');
				box.innerHTML = '';
				box.appendChild(c);
				en.target.title = `${name}\n${im.w} x ${im.h}, ${info.spr.numframes} frame(s), ${SPR_FORMATS[info.spr.texFormat] || info.spr.texFormat}`;
			});
		}
	}, { root: grid.parentElement });
	const showUi = $('spriteShowUi').checked;
	const list = allSpriteNames().filter((n) => (!filter || n.includes(filter)) && (!src || spriteSource(n) === src) &&
		(showUi || !isUiSprite(n) || n === S.selectedSprite));
	for (const name of list) {
		const card = document.createElement('div');
		card.className = 'sprite-card' + (S.selectedSprite === name ? ' selected' : '');
		card.dataset.name = name;
		card.innerHTML = `<div class="thumb"></div><div class="n">${util.escapeHtml(name.replace(/^sprites\//, ''))}</div><div class="src">${({ upload: 'uploaded', sbo: 'SBO', valve: 'Half-Life' })[spriteSource(name)] || ''}</div>`;
		card.onclick = () => showSpriteDetail(name);
		grid.appendChild(card);
		thumbObserver.observe(card);
	}
	if (!list.length) grid.innerHTML = '<p class="note">No sprite matches.</p>';
}

let detailAnim = 0;

async function showSpriteDetail(name) {
	S.selectedSprite = name;
	for (const c of document.querySelectorAll('.sprite-card')) c.classList.toggle('selected', c.dataset.name === name);
	const box = $('spriteDetail');
	box.hidden = false;
	box.innerHTML = `<div class="meta">${util.escapeHtml(name)}: loading...</div>`;
	const info = await ensureSprite(name);
	clearInterval(detailAnim);
	if (info.state !== 'ok') { box.innerHTML = `<div class="meta">${util.escapeHtml(name)}: ${info.state === 'bad' ? util.escapeHtml(info.error) : 'not found'}</div>`; return; }
	const spr = info.spr;
	const im0 = spr.images[spr.frames[0].images[0]];
	const scale = Math.max(1, Math.min(4, Math.floor(192 / Math.max(im0.w, im0.h))));
	box.innerHTML = '';
	const c = document.createElement('canvas');
	c.width = im0.w * scale;
	c.height = im0.h * scale;
	box.appendChild(c);
	const meta = document.createElement('div');
	meta.className = 'meta';
	meta.innerHTML = `<b>${util.escapeHtml(name)}</b> (${({ upload: 'uploaded', sbo: 'SBO', valve: 'Half-Life' })[info.source] || info.source})<br>` +
		`${spr.width} x ${spr.height}, ${spr.numframes} frame(s)${spr.frames.some((f) => f.group) ? ', with frame groups' : ''}<br>` +
		`texture format <b>${SPR_FORMATS[spr.texFormat] || spr.texFormat}</b>, type ${SPR_TYPES[spr.type] || spr.type}. ` +
		`<span title="Particles draw their own square facing the view; the sprite's type does not matter to them">Particles ignore the type.</span>`;
	box.appendChild(meta);
	const bar = document.createElement('div');
	bar.className = 'toolbar';
	const use = document.createElement('button');
	use.textContent = 'Use for the selected type';
	use.className = 'primary';
	use.disabled = !selectedBlock();
	use.onclick = () => { const b = selectedBlock(); if (b) { editKey(b, 'sprite', name); renderForm(); toast(`sprite ${name}`); } };
	const copy = document.createElement('button');
	copy.textContent = 'Copy name';
	copy.onclick = () => util.copyText(name).then(() => toast('Copied'));
	const fpsSel = document.createElement('select');
	for (const v of [5, 10, 15, 24, 30]) { const o = document.createElement('option'); o.value = v; o.textContent = `${v} fps`; fpsSel.appendChild(o); }
	fpsSel.value = 10;
	const bgSel = document.createElement('select');
	for (const [v, t] of [['check', 'checker'], ['#000', 'black'], ['#808080', 'grey'], ['#fff', 'white']]) { const o = document.createElement('option'); o.value = v; o.textContent = t; bgSel.appendChild(o); }
	bgSel.value = spr.texFormat === 1 ? '#000' : 'check';
	bar.append(use, copy, fpsSel, bgSel);
	box.appendChild(bar);
	let f = 0;
	const t0 = performance.now();
	const tick = () => {
		const frame = spr.frames[f % spr.frames.length];
		const img = groupImage(frame, (performance.now() - t0) / 1000);
		const im = spr.images[img];
		if (c.width !== im.w * scale || c.height !== im.h * scale) { c.width = im.w * scale; c.height = im.h * scale; }
		drawFrame(c, spr, img, bgSel.value);
		f++;
	};
	tick();
	const restart = () => { clearInterval(detailAnim); detailAnim = setInterval(tick, 1000 / parseInt(fpsSel.value, 10)); };
	fpsSel.onchange = restart;
	bgSel.onchange = tick;
	restart();
}

//=============================================================================
// uploads
//=============================================================================

async function fetchText(url) {
	const r = await fetch(url);
	if (!r.ok) throw new Error(`${url}: ${r.status}`);
	return util.latin1Decode(await r.arrayBuffer());
}

// uploads stay in this browser (IndexedDB), not on a server: the page is static
const MAX_UPLOAD_BYTES = 32 << 20, MAX_UPLOAD_FILES = 64;

async function loadUploads() {
	const saved = await util.uploadStore.all();
	for (const [name, value] of saved) {
		if (S.uploads.has(name) || !/^(aurora\/[^/]+\.aur|sprites\/[^/]+\.spr)$/.test(name)) continue;
		S.uploads.set(name, value);
		if (name.startsWith('sprites/')) forgetSprite(name);
	}
	if (!saved.size) return;
	renderSpriteGrid();
	if (S.doc) { rebuild(); renderTree(); }
}

function uploadsUsed(except) {
	let bytes = 0, files = 0;
	for (const [n, v] of S.uploads) {
		if (n === except) continue;
		files++;
		bytes += typeof v === 'string' ? v.length : v.byteLength;
	}
	return { bytes, files };
}

async function uploadFiles(files) {
	const scripts = [];
	let sprites = 0;
	const errors = [];
	for (const file of files) {
		const lower = file.name.toLowerCase();
		const isAur = lower.endsWith('.aur'), isSpr = lower.endsWith('.spr');
		if (!isAur && !isSpr) { errors.push(`${file.name}: only .aur and .spr`); continue; }
		const base = lower.replace(/^.*[\\/]/, '').replace(/[^a-z0-9_.\-]/g, '_');
		if (!NAME_RE.test(base) || base.includes('..')) { errors.push(`${file.name}: rename it (letters, digits, _ - .)`); continue; }
		if (file.size > (isSpr ? 4 << 20 : 256 << 10)) { errors.push(`${file.name}: too large`); continue; }
		const buf = await file.arrayBuffer();
		if (isSpr) {
			try { parseSprite(buf); } catch (e) { errors.push(`${file.name}: ${e.message}`); continue; }
		}
		const kind = isSpr ? 'sprites' : 'aurora';
		const name = `${kind}/${base}`;
		const used = uploadsUsed(name);
		if (used.bytes + buf.byteLength > MAX_UPLOAD_BYTES || (used.files >= MAX_UPLOAD_FILES && !S.uploads.has(name))) {
			errors.push(`${file.name}: uploads full (32 MB, 64 files)`);
			continue;
		}
		const value = isSpr ? buf : util.latin1Decode(buf);
		if (!(await util.uploadStore.put(name, value))) errors.push(`${file.name}: kept in this page only (browser storage blocked)`);
		if (isSpr) {
			S.uploads.set(name, value);
			forgetSprite(name);
			sprites++;
		} else {
			S.uploads.set(name, value);
			scripts.push({ name: base, text: value });
		}
	}
	if (scripts.length) loadScript(scripts[0].name, scripts[0].text);
	if (sprites) { renderSpriteGrid(); rebuild(); renderTree(); }
	const parts = [];
	if (scripts.length) parts.push(`${scripts.length} script(s)` + (scripts.length > 1 ? ' (the others are under Open > Uploaded)' : ''));
	if (sprites) parts.push(`${sprites} sprite(s)`);
	if (parts.length) toast(`Uploaded ${parts.join(', ')}${errors.length ? '; ' + errors.join('; ') : ''}`, errors.length > 0);
	else if (errors.length) toast(errors.join('; '), true);
}

//=============================================================================
// dialogs: new, open, entity, share, help
//=============================================================================

async function openNew() {
	const items = TEMPLATES.map((t) => `<button value="${t.id}" title="${util.escapeHtml(t.note)}">${util.escapeHtml(t.label)}<br><small>${util.escapeHtml(t.note)}</small></button>`).join('');
	const v = await dialog(`<h2>New script</h2><div class="pick-list">${items}</div>`, [['Cancel', '']], (body, close) => {
		for (const b of body.querySelectorAll('button[value]')) b.onclick = (e) => { e.preventDefault(); close(b.value); };
	});
	const t = TEMPLATES.find((x) => x.id === v);
	if (t) loadScript(t.name, t.text, `New script from "${t.label}"`);
}

async function openPicker() {
	const drafts = util.store.get(KEY_PREFIX + 'drafts') || {};
	const draftNames = Object.keys(drafts).sort((a, b) => drafts[b].ts - drafts[a].ts);
	const ups = [...S.uploads.keys()].filter((n) => n.startsWith('aurora/'));
	const btn = (v, label, small) => `<button value="${util.escapeHtml(v)}">${util.escapeHtml(label)}${small ? `<br><small>${util.escapeHtml(small)}</small>` : ''}</button>`;
	let html = '<h2>Open</h2>';
	html += '<h3>Shipped with the game (start from one; your copy is a draft until you download it)</h3><div class="pick-list">' +
		S.assets.scripts.map((s) => btn('asset:' + s.name, s.name.replace(/^aurora\//, ''))).join('') + '</div>';
	if (ups.length) html += '<h3>Uploaded in this browser</h3><div class="pick-list">' + ups.map((n) => btn('upload:' + n, n.replace(/^aurora\//, ''))).join('') + '</div>';
	if (draftNames.length) html += '<h3>Drafts in this browser (autosaved)</h3><div class="pick-list">' +
		draftNames.map((n) => btn('draft:' + n, n, new Date(drafts[n].ts).toLocaleString())).join('') + '</div>';
	const v = await dialog(html, [['Cancel', '']], (body, close) => {
		for (const b of body.querySelectorAll('button[value]')) b.onclick = (e) => { e.preventDefault(); close(b.value); };
	});
	if (!v) return;
	try {
		if (v.startsWith('asset:')) {
			const path = v.slice(6);
			loadScript(path.replace(/^aurora\//, ''), await fetchText(`assets/${path}`));
		} else if (v.startsWith('upload:')) {
			loadScript(v.slice(7).replace(/^aurora\//, ''), S.uploads.get(v.slice(7)));
		} else if (v.startsWith('draft:')) {
			const n = v.slice(6);
			loadScript(n, drafts[n].text, `Draft "${n}" restored`);
		}
		frameCamera();
	} catch (e) {
		toast(e.message, true);
	}
}

function entityText() {
	const em = S.emitter;
	const origin = em.mode === 'static' ? em.origin : [0, 0, 0];
	const lines = ['{', '"classname" "env_particle"', `"origin" "${origin.map((v) => Math.round(v)).join(' ')}"`];
	if (em.angles.some((a) => a)) lines.push(`"angles" "${em.angles.map((a) => aur.formatNumber(a)).join(' ')}"`);
	lines.push(`"message" "${scriptPath()}"`);
	if (S.play.mode === 'oneshot') {
		lines.push(`"targetname" "${S.name}_fx"`, '"spawnflags" "2"');
	}
	if (em.mode !== 'static') {
		lines.push('"target" "my_train"');
		if (em.attachment >= 0) lines.push(`"attachment" "${em.attachment}"`);
	}
	lines.push('}');
	return lines.join('\n');
}

async function showEntity() {
	const sprites = aur.spritesOf(S.doc);
	const ups = sprites.filter((s) => S.uploads.has(s));
	const res = [scriptPath(), ...ups].join('\n');
	const html = `<h2>The map entity</h2>
<p>An <code>env_particle</code> for this script, as it appears in a <code>.map</code> file (paste it into the map source, or set the same keys in the editor's entity properties). ${S.play.mode === 'oneshot' ? 'With Spawn Use (spawnflag 2) each trigger of the name shows a one-shot copy at the activator.' : 'Without a name it is always on; give it a name to switch it, and Start On (spawnflag 1) to start on.'}${S.emitter.mode !== 'static' ? ' <b>target</b>: the name of the entity it moves with (here a placeholder).' : ''}</p>
<pre id="entText">${util.escapeHtml(entityText())}</pre>
<h3>Files for the map's .res</h3>
<p class="note">The game adds the script and its sprites to the download list itself; list the map's own files in its .res for distribution packages. Sprites of SBO and Half-Life ship with the game.</p>
<pre id="resText">${util.escapeHtml(res)}</pre>
<h3>FGD</h3>
<p class="note">env_particle is in SBO's <code>sdk/syndicate.fgd</code>: Particle script (message), Angles, Move with entity (target), Attachment point (attachment), spawnflags Start On (1) and Spawn Use (2).</p>`;
	const v = await dialog(html, [['Copy .res list', 'res'], ['Copy entity', 'ent', 'primary'], ['Close', '']]);
	if (v === 'ent') util.copyText(entityText()).then((ok) => toast(ok ? 'Entity copied' : 'Copy failed', !ok));
	if (v === 'res') util.copyText(res).then((ok) => toast(ok ? '.res list copied' : 'Copy failed', !ok));
}

async function share() {
	const state = { v: 1, n: S.name, t: S.text, scene: S.scene, emitter: S.emitter, play: { mode: S.play.mode, fps: S.play.fps, autoTrigger: S.play.autoTrigger, seed: S.play.seed } };
	const packed = await util.packState(state);
	const url = `${location.origin}${location.pathname}#s=${packed}`;
	const ok = await util.copyText(url);
	const ups = aur.spritesOf(S.doc).filter((s) => S.uploads.has(s));
	toast(`${ok ? 'Link copied' : 'Copy failed'} (${url.length} characters)${ups.length ? '. Uploaded sprites are not in the link: send them along.' : ''}`, !ok);
}

function downloadAur() {
	util.download(`${S.name}.aur`, util.latin1Encode(S.text), 'text/plain');
}

function downloadZip() {
	const files = [{ name: scriptPath(), data: util.latin1Encode(S.text) }];
	const missing = [];
	for (const s of aur.spritesOf(S.doc)) {
		if (S.uploads.has(s)) files.push({ name: s, data: new Uint8Array(S.uploads.get(s)) });
		else if (!knownSprite(s)) missing.push(s);
	}
	util.download(`${S.name}.zip`, util.makeZip(files));
	toast(`${S.name}.zip: ${files.map((f) => f.name).join(', ')}${missing.length ? `. Not found: ${missing.join(', ')}` : ''}`, missing.length > 0);
}

function showHelp() {
	const keys = (list) => list.map((k) => `<tr><td>${k.key}</td><td>${util.escapeHtml(k.help || '')}${k.default !== undefined && k.default !== '' ? ` <span class="dim">Default ${util.escapeHtml(String(k.default))}.</span>` : ''}</td></tr>`).join('');
	const html = `<h2>Aurora particle editor</h2>
<p>Scripts for SBO's <code>env_particle</code> (<code>aurora/*.aur</code>). The preview is <b>the game's own particle code</b> (src/cl_dll/aurora, compiled to WebAssembly): the same parser, warnings, limits, spray timing, collisions and one-shot rules, drawn the way the engine's triangle API draws it. What it cannot show: the map's lighting and fog, gamma, other effects in the map.</p>
<h3>Keys</h3>
<table>
<tr><td><kbd>Space</kbd></td><td>play / pause</td></tr>
<tr><td><kbd>.</kbd></td><td>one frame (paused)</td></tr>
<tr><td><kbd>R</kbd></td><td>restart (a new map: the systems are built again)</td></tr>
<tr><td><kbd>T</kbd></td><td>trigger a Spawn Use copy</td></tr>
<tr><td><kbd>O</kbd></td><td>switch the entity on / off</td></tr>
<tr><td><kbd>F</kbd></td><td>frame the particles (Shift+F: the emitter)</td></tr>
<tr><td><kbd>Ctrl Z</kbd> / <kbd>Ctrl Y</kbd></td><td>undo / redo (the whole script, also typing in the Script tab)</td></tr>
<tr><td><kbd>Ctrl S</kbd> / <kbd>Ctrl O</kbd></td><td>download the .aur / open</td></tr>
<tr><td><kbd>H</kbd> or <kbd>?</kbd></td><td>this help</td></tr>
<tr><td>Mouse</td><td>drag: orbit; right drag or Shift+drag: pan; wheel: zoom. Fly camera (Scene tab): drag to look, W A S D Q E to move.</td></tr>
</table>
<h3>Working</h3>
<p>Pick a type in the tree, change its keys in Properties (hover a label for its meaning), or type in the Script tab: the text is the file, and a form change rewrites only that key's line (comments, unknown keys and other editors' keys stay). Problems lists the game's console lines for the script and the editor's hints. Drafts are saved in this browser as you work; upload .aur and .spr files with the Upload button or by dropping them on the page (they stay in this browser only, nothing is sent to a server; 4 MB per sprite, 32 MB in all). Download .aur, or a zip with <code>aurora/</code> and <code>sprites/</code> laid out as in the game folder.</p>
<h3>Drawing, as in the game</h3>
<p>Each particle is a square facing the view, <code>startsize</code> = half its width. Render modes go through the engine's triangle API: <b>additive</b> adds color x alpha (black is invisible); <b>solid</b> blends with the sprite's alpha x the particle's alpha; <b>texture</b> and <b>color</b> also blend, but the engine's Color4f multiplies the color by alpha and draws opaque: alpha darkens instead of fading. Sprites: normal and additive formats are opaque, indexalpha uses the index as alpha, alphatest makes color 255 transparent.</p>
<h3>Top-level keys</h3><table>${keys(S.schema.top)}</table>
<h3>Type keys</h3><table>${keys(S.schema.type)}</table>
<h3>Limits</h3><p>particles 1 to 2048 per system (default 100); 8192 alive in all systems; 64 types per script; overlays 4 deep; 16 children per particle per frame; a Spawn Use copy emits for its main type's lifetime (5 s without, 120 s at most); 32 copies at a time on a client.</p>
<p class="note">Core: ${util.escapeHtml(S.coreInfo || '')}</p>`;
	dialog(html);
}

//=============================================================================
// tabs, keys, transport
//=============================================================================

function showTab(side, tab) {
	for (const b of document.querySelectorAll(`[data-tabs="${side}"] button`)) b.classList.toggle('active', b.dataset.tab === tab);
	const container = $(side);
	for (const p of container.querySelectorAll('.panel')) p.classList.toggle('active', p.dataset.panel === tab);
	if (side === 'left' && tab === 'sprites' && !$('spriteGrid').children.length) renderSpriteGrid();
}

function updateTransport() {
	$('btnPlay').textContent = S.play.playing ? 'Pause' : 'Play';
	$('btnToggle').textContent = S.play.on ? 'Turn off' : 'Turn on';
	$('btnToggle').disabled = S.play.mode !== 'loop';
	$('autoTriggerLabel').style.display = S.play.mode === 'oneshot' ? '' : 'none';
	$('speed').value = String(S.play.speed);
	$('clientFps').value = String(S.play.fps);
	$('playMode').value = S.play.mode;
	$('autoTrigger').value = S.play.autoTrigger;
}

function restart() {
	rebuild();
	toast('Restarted');
}

function bindUi() {
	for (const t of document.querySelectorAll('.tabs')) {
		t.addEventListener('click', (e) => {
			const b = e.target.closest('button[data-tab]');
			if (b) showTab(t.dataset.tabs, b.dataset.tab);
		});
	}
	$('btnNew').onclick = openNew;
	$('btnOpen').onclick = openPicker;
	$('btnUpload').onclick = () => $('fileInput').click();
	$('fileInput').onchange = (e) => { uploadFiles([...e.target.files]); e.target.value = ''; };
	$('btnUndo').onclick = undo;
	$('btnRedo').onclick = redo;
	$('btnSave').onclick = downloadAur;
	$('btnZip').onclick = downloadZip;
	$('btnShare').onclick = share;
	$('btnEntity').onclick = showEntity;
	$('btnHelp').onclick = showHelp;
	$('btnAddType').onclick = () => addType(null);
	$('btnAddSpray').onclick = () => addType('spray');
	$('btnAddOverlay').onclick = () => addType('overlay');
	$('btnRename').onclick = renameSelected;
	$('btnDuplicate').onclick = duplicateSelected;
	$('btnSetMain').onclick = setMainSelected;
	$('btnDelete').onclick = deleteSelected;
	$('scriptName').onchange = (e) => {
		const n = cleanScriptName(e.target.value);
		if (!n) { e.target.value = S.name; return; }
		S.name = n;
		e.target.value = n;
		saveDraft();
		rebuild();
	};
	$('form').addEventListener('focusout', () => setTimeout(() => { if (formStale && !$('form').contains(document.activeElement)) renderForm(); }, 0));
	const ta = $('scriptText');
	ta.addEventListener('input', () => setText(ta.value, 'text', 'text'));
	ta.addEventListener('scroll', updateLineNumbers);
	ta.addEventListener('keydown', (e) => {
		if (e.key === 'Tab' && !e.ctrlKey) {
			e.preventDefault();
			const s = ta.selectionStart;
			ta.setRangeText('\t', s, ta.selectionEnd, 'end');
			setText(ta.value, 'text', 'text');
		}
	});
	$('spriteFilter').oninput = renderSpriteGrid;
	$('spriteSource').onchange = renderSpriteGrid;
	$('spriteShowUi').onchange = renderSpriteGrid;

	$('btnPlay').onclick = () => { S.play.playing = !S.play.playing; updateTransport(); };
	$('btnStep').onclick = () => { S.play.playing = false; stepOnce = true; updateTransport(); };
	$('btnRestart').onclick = restart;
	$('speed').onchange = (e) => { S.play.speed = parseFloat(e.target.value); saveSettings(); };
	$('clientFps').onchange = (e) => { S.play.fps = parseInt(e.target.value, 10); saveSettings(); };
	$('playMode').onchange = (e) => { S.play.mode = e.target.value; updateTransport(); saveSettings(); rebuild(); };
	$('btnToggle').onclick = toggleOn;
	$('btnTrigger').onclick = trigger;
	$('autoTrigger').onchange = (e) => { S.play.autoTrigger = Math.max(0, parseFloat(e.target.value) || 0); nextTrigger = simTime; saveSettings(); };

	document.addEventListener('keydown', (e) => {
		const inField = e.target.closest && e.target.closest('input, textarea, select, [contenteditable]');
		const mod = e.ctrlKey || e.metaKey;
		if (mod && e.key.toLowerCase() === 'z' && !e.shiftKey) { e.preventDefault(); undo(); return; }
		if (mod && (e.key.toLowerCase() === 'y' || (e.key.toLowerCase() === 'z' && e.shiftKey))) { e.preventDefault(); redo(); return; }
		if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); downloadAur(); return; }
		if (mod && e.key.toLowerCase() === 'o') { e.preventDefault(); openPicker(); return; }
		if (inField || mod || e.altKey || $('dialog').open) return;
		if (cam.fly && document.activeElement === $('view') && 'wasdqe'.includes(e.key.toLowerCase())) return;
		switch (e.key) {
		case ' ': e.preventDefault(); $('btnPlay').click(); break;
		case '.': $('btnStep').click(); break;
		case 'r': case 'R': restart(); break;
		case 't': case 'T': trigger(); break;
		case 'o': case 'O': toggleOn(); break;
		case 'f': case 'F': frameCamera(!e.shiftKey); break;
		case 'h': case 'H': case '?': showHelp(); break;
		default: return;
		}
	});

	// drag and drop anywhere
	let dragDepth = 0;
	window.addEventListener('dragenter', (e) => { if (e.dataTransfer && [...e.dataTransfer.types].includes('Files')) { dragDepth++; $('dropzone').hidden = false; } });
	window.addEventListener('dragleave', () => { dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) $('dropzone').hidden = true; });
	window.addEventListener('dragover', (e) => e.preventDefault());
	window.addEventListener('drop', (e) => {
		e.preventDefault();
		dragDepth = 0;
		$('dropzone').hidden = true;
		if (e.dataTransfer && e.dataTransfer.files.length) uploadFiles([...e.dataTransfer.files]);
	});
	bindCamera();
}

function toggleOn() {
	if (S.play.mode !== 'loop') return;
	S.play.on = !S.play.on;
	sendMain();
	if (S.compare.enabled && S.compare.doc) sendCompare();
	updateTransport();
}

//=============================================================================
// boot
//=============================================================================

async function boot() {
	try {
		renderer = new Renderer($('view'));
	} catch (e) {
		document.body.innerHTML = `<p style="padding:20px">This editor needs WebGL2: ${util.escapeHtml(e.message)}</p>`;
		return;
	}
	const [schema, assets] = await Promise.all([
		fetch('core/aurora-keys.json').then((r) => r.json()),
		fetch('assets/index.json').then((r) => r.json()).catch(() => ({ scripts: [], sprites: [] })),
	]);
	S.schema = schema;
	S.assets = assets;
	engine = await new AuroraEngine().init(1);

	// does this build of the game code read the XashXT keys? (an unknown key prints a console line)
	const probe = (text) => {
		engine.setFile('aurora/__probe.aur', text);
		engine.message(0, 99, [0, 0, 0], [0, 0, 0], 'aurora/__probe.aur');
		const log = engine.takeLog();
		engine.vidinit();
		engine.takeLog();
		return !/unknown key/.test(log);
	};
	S.coreHasXash = probe('"lightmodel" "1"\n"killcondition" "water"\n');
	S.coreTypeKill = probe('{\n"name" "a"\n"killcondition" "water"\n}\n');
	S.coreInfo = `SBO particlesys.cpp / particlemgr.cpp in WebAssembly; lightmodel / killcondition ${S.coreHasXash ? 'read' : 'not read by this build (shown as unknown keys)'}${S.coreTypeKill ? ', killcondition also in a type block' : ''}`;

	const settings = util.store.get(KEY_PREFIX + 'settings');
	if (settings) {
		Object.assign(S.scene, settings.scene || {});
		Object.assign(S.emitter, settings.emitter || {});
		Object.assign(S.play, settings.play || {});
	}
	bindUi();
	updateTransport();
	renderSceneForm();
	renderCompareForm();
	loadUploads().then(() => { renderCompareForm(); }).catch((e) => console.warn('uploads:', e.message));

	let loaded = false;
	if (location.hash.startsWith('#s=')) {
		try {
			const st = await util.unpackState(location.hash.slice(3));
			if (st.scene) Object.assign(S.scene, st.scene);
			if (st.emitter) Object.assign(S.emitter, st.emitter);
			if (st.play) Object.assign(S.play, st.play);
			loadScript(st.n, st.t, `Opened the shared script "${st.n}"`);
			renderSceneForm();
			updateTransport();
			loaded = true;
		} catch (e) {
			toast('The link could not be read: ' + e.message, true);
		}
		window.history.replaceState(null, '', location.pathname + location.search);
	}
	if (!loaded) {
		const last = util.store.get(KEY_PREFIX + 'last');
		const drafts = util.store.get(KEY_PREFIX + 'drafts') || {};
		if (last && drafts[last]) {
			loadScript(last, drafts[last].text, `Draft "${last}" restored (Open lists the shipped scripts)`);
		} else {
			const first = S.assets.scripts.find((s) => s.name === 'aurora/steamvent.aur') || S.assets.scripts[0];
			if (first) loadScript(first.name.replace(/^aurora\//, ''), await fetchText(`assets/${first.name}`));
			else loadScript(TEMPLATES[0].name, TEMPLATES[0].text);
		}
	}
	frameCamera();
	cameraUpdate(0);
	requestAnimationFrame((t) => { lastNow = t; loop(t); });
	window.__aurora = { S, engine, renderer, setText, loadScript, rebuild, trigger, cam, frameCamera, get quads() { return lastQuads; } };
}

boot().catch((e) => {
	console.error(e);
	toast('The editor did not start: ' + e.message, true);
});
