// Aurora editor parity test: every script runs through the native build (g++) and the WebAssembly build of the
// same sources (SBO's particlesys.cpp / particlemgr.cpp + core/shim.cpp) with the same seed and frame times; the
// particles' origins and sizes must agree on every frame. Run by the Dockerfile at build time.
//
//   node parity.mjs <aurora-native> <aurora-core.mjs> <scripts-dir> [frames]
import { execFileSync } from 'node:child_process';
import { readFileSync, readdirSync } from 'node:fs';
import { resolve, join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [nativeBin, coreJs, scriptsDir, framesArg] = process.argv.slice(2);
const FRAMES = parseInt(framesArg || '300', 10);
const DT = 0.01;
const DT_F = Math.fround(DT);	// the native side reads it as a float
const SEED = 7;
const TOLERANCE = 1e-3;

const AuroraCore = (await import(pathToFileURL(resolve(coreJs)).href)).default;

function spriteNames(text) {
	const names = new Set();
	const re = /"sprite"\s+"([^"]+)"/gi;
	let m;
	while ((m = re.exec(text))) names.add(m[1].toLowerCase());
	return [...names];
}

async function runWasm(text, sprites, follow) {
	const M = await AuroraCore();
	const str = (s) => {
		const n = M.lengthBytesUTF8(s) + 1;
		const p = M._malloc(n);
		M.stringToUTF8(s, p, n);
		return p;
	};
	M._ae_init(SEED);
	for (const s of sprites) {
		const p = str(s);
		M._ae_sprite_add(p, 4);
		M._free(p);
	}
	const bytes = new TextEncoder().encode(text);
	const buf = M._malloc(bytes.length + 1);
	M.HEAPU8.set(bytes, buf);
	const name = str('aurora/parity.aur');
	M._ae_file_add(name, buf, bytes.length);
	const P = M._ae_params() >> 2;
	const set = (i, v) => { M.HEAPF32[P + i] = v; };
	set(0, 1); set(1, -256); set(2, 0); set(3, 64); set(4, 0); set(5, 0); set(6, 0); set(7, 1); set(8, 0);
	const frames = [];
	for (let f = 0; f < FRAMES; f++) {
		const t = Math.fround(1 + Math.fround(f * DT_F));
		set(0, t);
		if (follow) {
			const s = Math.fround(t - 1);
			set(35, Math.fround(s * 64)); set(36, Math.fround(s * -24)); set(37, 48);
			set(38, 0); set(39, Math.fround(Math.fround(s * 45) + 0.5)); set(40, 0);
			if (f === 0) M._ae_message(1 | 8, 1, 0, 0, 16, 0, 0, 0, 2, -1, name);
		} else if (f === 0) M._ae_message(1, 1, 0, 0, 64, 0, 0, 0, 0, -1, name);
		M._ae_frame();
		const n = M._ae_dump(1);
		const d = M._ae_dump_ptr() >> 2;
		frames.push(Array.from(M.HEAPF32.subarray(d, d + n * 4)));
	}
	return frames;
}

function runNative(file, sprites, follow) {
	const out = execFileSync(nativeBin, [file, String(FRAMES), String(DT), String(SEED), follow ? '1' : '0',
		...sprites.map((s) => s + ':4')], { maxBuffer: 1 << 28, stdio: ['ignore', 'pipe', 'ignore'] }).toString();
	const frames = [];
	let cur = null;
	for (const line of out.split('\n')) {
		if (!line) continue;
		if (line[0] === 'F') { cur = []; frames.push(cur); continue; }
		for (const v of line.split(' ')) cur.push(parseFloat(v));
	}
	return frames;
}

let failures = 0;
let worst = 0;
let compared = 0;
const scripts = readdirSync(scriptsDir).filter((f) => f.toLowerCase().endsWith('.aur')).sort();
for (const file of scripts) {
	const path = join(scriptsDir, file);
	const text = readFileSync(path, 'latin1');
	const sprites = spriteNames(text);
	for (const follow of [false, true]) {
		const a = runNative(path, sprites, follow);
		const b = await runWasm(text, sprites, follow);
		let maxDiff = 0;
		let bad = '';
		for (let f = 0; f < FRAMES && !bad; f++) {
			if (a[f].length !== b[f].length) { bad = `frame ${f}: ${a[f].length / 4} vs ${b[f].length / 4} particles`; break; }
			for (let i = 0; i < a[f].length; i++) {
				// relative to the size of the value (positions reach thousands of units)
				const d = Math.abs(a[f][i] - b[f][i]) / Math.max(1, Math.abs(a[f][i]));
				if (d > maxDiff) maxDiff = d;
			}
			compared += a[f].length / 4;
		}
		if (!bad && maxDiff > TOLERANCE) bad = `max difference ${maxDiff}`;
		worst = Math.max(worst, maxDiff);
		console.log(`${bad ? 'FAIL' : 'ok  '} ${file} ${follow ? 'follow' : 'static'} max-diff ${maxDiff.toExponential(2)} ${bad}`);
		if (bad) failures++;
	}
}
console.log(`${scripts.length} scripts x 2 placements x ${FRAMES} frames: ${compared} particle states compared, worst relative difference ${worst.toExponential(2)}`);
console.log(failures ? `PARITY FAILED: ${failures}` : 'PARITY OK');
process.exit(failures ? 1 : 0);
