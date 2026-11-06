// WebGL2 view: the test scene (grid, floor, water, wall, a 72-unit player for scale, markers) and the particle
// quads exactly as the game's triangle API draws them: per quad the engine's render mode, color after Color4f's rule
// and depth mask (core/shim.cpp); texture MODULATE; blend src-alpha/one for additive, src-alpha/one-minus-src-alpha
// for solid, texture and color; depth test on, so the floor and the wall hide particles behind them.
// GoldSrc coordinates: x east, y north, z up; view angles pitch (down positive), yaw, roll.

import { QUAD_FLOATS } from './engine.js';

const kRenderNormal = 0, kRenderTransAdd = 5;

export function angleVectors(angles) {
	const d2r = Math.PI / 180;
	const sy = Math.sin(angles[1] * d2r), cy = Math.cos(angles[1] * d2r);
	const sp = Math.sin(angles[0] * d2r), cp = Math.cos(angles[0] * d2r);
	const sr = Math.sin(angles[2] * d2r), cr = Math.cos(angles[2] * d2r);
	return {
		forward: [cp * cy, cp * sy, -sp],
		right: [-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp],
		up: [cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp],
	};
}

const LINE_VS = `#version 300 es
uniform mat4 uMVP;
in vec3 aPos;
in vec4 aColor;
out vec4 vColor;
void main() { vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); }`;
const LINE_FS = `#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 o;
void main() { o = vColor; }`;

const PART_VS = `#version 300 es
uniform mat4 uMVP;
in vec3 aPos;
in vec2 aUV;
in vec4 aColor;
in float aLayer;
out vec2 vUV;
out vec4 vColor;
out float vLayer;
void main() { vUV = aUV; vColor = aColor; vLayer = aLayer; gl_Position = uMVP * vec4(aPos, 1.0); }`;
const PART_FS = `#version 300 es
precision mediump float;
precision mediump sampler2DArray;
uniform sampler2DArray uTex;
in vec2 vUV;
in vec4 vColor;
in float vLayer;
out vec4 o;
void main() { o = texture(uTex, vec3(vUV, vLayer)) * vColor; }`;

function compile(gl, vs, fs) {
	const p = gl.createProgram();
	for (const [type, src] of [[gl.VERTEX_SHADER, vs], [gl.FRAGMENT_SHADER, fs]]) {
		const s = gl.createShader(type);
		gl.shaderSource(s, src);
		gl.compileShader(s);
		if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s));
		gl.attachShader(p, s);
	}
	gl.linkProgram(p);
	if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
	return p;
}

function mul(a, b) {
	const o = new Float32Array(16);
	for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) {
		let s = 0;
		for (let k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
		o[c * 4 + r] = s;
	}
	return o;
}

export class Renderer {
	constructor(canvas) {
		this.canvas = canvas;
		const gl = canvas.getContext('webgl2', { antialias: true, alpha: false, depth: true, preserveDrawingBuffer: true });
		if (!gl) throw new Error('WebGL2 is not available in this browser');
		this.gl = gl;
		this.lineProg = compile(gl, LINE_VS, LINE_FS);
		this.partProg = compile(gl, PART_VS, PART_FS);
		this.lineBuf = gl.createBuffer();
		this.partBuf = gl.createBuffer();
		this.textures = new Map();	// sprite name -> { tex, layers: [first layer per frame], spr }
		this.fallback = this.makeFallback();
		this.partData = new Float32Array(0);
		this.fovX = 90;
	}

	makeFallback() {
		const gl = this.gl;
		const tex = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D_ARRAY, tex);
		gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.RGBA8, 1, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([255, 0, 255, 255]));
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
		return { tex, layerOf: () => 0 };
	}

	// every picture of the sprite in one texture array (frames of other sizes are resampled to the largest)
	uploadSprite(name, spr) {
		const gl = this.gl;
		const old = this.textures.get(name);
		if (old) gl.deleteTexture(old.tex);
		let w = 1, h = 1;
		for (const im of spr.images) { w = Math.max(w, im.w); h = Math.max(h, im.h); }
		const layers = spr.images.length;
		const data = new Uint8Array(w * h * 4 * layers);
		spr.images.forEach((im, l) => {
			const base = l * w * h * 4;
			for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
				const sx = Math.min(im.w - 1, Math.floor(x * im.w / w));
				const sy = Math.min(im.h - 1, Math.floor(y * im.h / h));
				const si = (sy * im.w + sx) * 4, di = base + (y * w + x) * 4;
				data[di] = im.rgba[si]; data[di + 1] = im.rgba[si + 1]; data[di + 2] = im.rgba[si + 2]; data[di + 3] = im.rgba[si + 3];
			}
		});
		const tex = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D_ARRAY, tex);
		gl.texImage3D(gl.TEXTURE_2D_ARRAY, 0, gl.RGBA8, w, h, layers, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D_ARRAY, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		this.textures.set(name, { tex, spr });
	}

	resize() {
		const dpr = Math.min(window.devicePixelRatio || 1, 2);
		const w = Math.max(1, Math.round(this.canvas.clientWidth * dpr));
		const h = Math.max(1, Math.round(this.canvas.clientHeight * dpr));
		if (this.canvas.width !== w || this.canvas.height !== h) { this.canvas.width = w; this.canvas.height = h; }
	}

	// GoldSrc's view: horizontal field of view fovX at the window's aspect
	matrices(origin, angles) {
		const { forward, right, up } = angleVectors(angles);
		const view = new Float32Array([
			right[0], up[0], -forward[0], 0,
			right[1], up[1], -forward[1], 0,
			right[2], up[2], -forward[2], 0,
			0, 0, 0, 1,
		]);
		view[12] = -(right[0] * origin[0] + right[1] * origin[1] + right[2] * origin[2]);
		view[13] = -(up[0] * origin[0] + up[1] * origin[1] + up[2] * origin[2]);
		view[14] = forward[0] * origin[0] + forward[1] * origin[1] + forward[2] * origin[2];
		const aspect = this.canvas.width / this.canvas.height;
		const near = 4, far = 16384;
		const tx = Math.tan(this.fovX * Math.PI / 360);
		const ty = tx / aspect;
		const proj = new Float32Array([
			1 / tx, 0, 0, 0,
			0, 1 / ty, 0, 0,
			0, 0, -(far + near) / (far - near), -1,
			0, 0, -2 * far * near / (far - near), 0,
		]);
		return { mvp: mul(proj, view), forward, right, up };
	}

	drawLines(mvp, verts, mode) {
		const gl = this.gl;
		if (!verts.length) return;
		gl.useProgram(this.lineProg);
		gl.uniformMatrix4fv(gl.getUniformLocation(this.lineProg, 'uMVP'), false, mvp);
		gl.bindBuffer(gl.ARRAY_BUFFER, this.lineBuf);
		gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.STREAM_DRAW);
		const aPos = gl.getAttribLocation(this.lineProg, 'aPos');
		const aColor = gl.getAttribLocation(this.lineProg, 'aColor');
		gl.enableVertexAttribArray(aPos);
		gl.enableVertexAttribArray(aColor);
		gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 28, 0);
		gl.vertexAttribPointer(aColor, 4, gl.FLOAT, false, 28, 12);
		gl.drawArrays(mode, 0, verts.length / 7);
		gl.disableVertexAttribArray(aPos);
		gl.disableVertexAttribArray(aColor);
	}

	// scene: { background, floor, water, wall, grid, player, markers: [...], lines: [...] }
	render(view, scene, quads, spriteOf, time) {
		const gl = this.gl;
		this.resize();
		gl.viewport(0, 0, this.canvas.width, this.canvas.height);
		const bg = scene.background;
		gl.clearColor(bg[0], bg[1], bg[2], 1);
		gl.depthMask(true);
		gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
		const { mvp } = this.matrices(view.origin, view.angles);
		gl.enable(gl.DEPTH_TEST);
		gl.depthFunc(gl.LEQUAL);
		gl.disable(gl.CULL_FACE);

		// opaque scene: floor, wall
		gl.disable(gl.BLEND);
		const tris = [];
		const quad = (a, b, c, d, col) => tris.push(...a, ...col, ...b, ...col, ...c, ...col, ...a, ...col, ...c, ...col, ...d, ...col);
		if (scene.floor) {
			const z = scene.floor.z, S = 4096, col = scene.floor.color;
			quad([-S, -S, z], [S, -S, z], [S, S, z], [-S, S, z], col);
		}
		const box = (mins, maxs, col, shade = true) => {
			const [x0, y0, z0] = mins, [x1, y1, z1] = maxs;
			const f = (k) => shade ? [col[0] * k, col[1] * k, col[2] * k, col[3]] : col;
			quad([x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1], f(1));
			quad([x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0], f(0.5));
			quad([x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1], f(0.8));
			quad([x0, y1, z0], [x1, y1, z0], [x1, y1, z1], [x0, y1, z1], f(0.7));
			quad([x0, y0, z0], [x0, y1, z0], [x0, y1, z1], [x0, y0, z1], f(0.9));
			quad([x1, y0, z0], [x1, y1, z0], [x1, y1, z1], [x1, y0, z1], f(0.6));
		};
		if (scene.wall) box(scene.wall.mins, scene.wall.maxs, scene.wall.color);
		for (const b of scene.solids || []) box(b.mins, b.maxs, b.color);
		// pushed back in depth, so the grid on the floor is not lost to depth precision (software WebGL has 16 bits)
		gl.enable(gl.POLYGON_OFFSET_FILL);
		gl.polygonOffset(2, 8);
		this.drawLines(mvp, tris, gl.TRIANGLES);
		gl.disable(gl.POLYGON_OFFSET_FILL);

		// lines (grid, player, path) in the world
		this.drawLines(mvp, scene.lines || [], gl.LINES);

		// water: drawn in the world pass (before the particles) and writes depth, as GoldSrc's opaque water
		if (scene.water) {
			const wt = [];
			const w = scene.water;
			const [x0, y0, z0] = w.mins, [x1, y1, z1] = w.maxs, c = w.color;
			const q = (a, b, cc, d) => wt.push(...a, ...c, ...b, ...c, ...cc, ...c, ...a, ...c, ...cc, ...c, ...d, ...c);
			q([x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]);
			q([x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1]);
			q([x0, y1, z0], [x1, y1, z0], [x1, y1, z1], [x0, y1, z1]);
			q([x0, y0, z0], [x0, y1, z0], [x0, y1, z1], [x0, y0, z1]);
			q([x1, y0, z0], [x1, y1, z0], [x1, y1, z1], [x1, y0, z1]);
			gl.enable(gl.BLEND);
			gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
			gl.depthMask(true);
			this.drawLines(mvp, wt, gl.TRIANGLES);
		}

		this.drawParticles(mvp, quads, spriteOf, time);

		// markers on top of everything (no depth)
		gl.disable(gl.DEPTH_TEST);
		gl.disable(gl.BLEND);
		this.drawLines(mvp, scene.overlay || [], gl.LINES);
		gl.enable(gl.DEPTH_TEST);
		return mvp;
	}

	drawParticles(mvp, quads, spriteOf, time) {
		const gl = this.gl;
		const n = quads.length / QUAD_FLOATS;
		if (!n) return;
		// 6 vertices per quad: x y z u v r g b a layer
		const need = n * 6 * 10;
		if (this.partData.length < need) this.partData = new Float32Array(need * 1.5 | 0);
		const d = this.partData;
		const runs = [];
		let cur = null;
		for (let q = 0; q < n; q++) {
			const o = q * QUAD_FLOATS;
			const hSprite = quads[o + 24], frame = quads[o + 25], mode = quads[o + 26], depth = quads[o + 27];
			const sprite = spriteOf(hSprite);
			const t = sprite ? this.textures.get(sprite) : null;
			let layer = 0;
			if (t) {
				const fr = t.spr.frames[Math.max(0, Math.min(t.spr.frames.length - 1, frame | 0))];
				layer = fr ? imageFor(fr, time) : 0;
			}
			const tex = t ? t.tex : this.fallback.tex;
			if (!cur || cur.tex !== tex || cur.mode !== mode || cur.depth !== depth) {
				cur = { tex, mode, depth, first: q, count: 0 };
				runs.push(cur);
			}
			cur.count++;
			const order = [0, 1, 2, 0, 2, 3];
			for (let k = 0; k < 6; k++) {
				const v = order[k];
				const b = (q * 6 + k) * 10;
				d[b] = quads[o + v * 5]; d[b + 1] = quads[o + v * 5 + 1]; d[b + 2] = quads[o + v * 5 + 2];
				d[b + 3] = quads[o + v * 5 + 3]; d[b + 4] = quads[o + v * 5 + 4];
				d[b + 5] = quads[o + 20]; d[b + 6] = quads[o + 21]; d[b + 7] = quads[o + 22]; d[b + 8] = quads[o + 23];
				d[b + 9] = layer;
			}
		}
		gl.useProgram(this.partProg);
		gl.uniformMatrix4fv(gl.getUniformLocation(this.partProg, 'uMVP'), false, mvp);
		gl.uniform1i(gl.getUniformLocation(this.partProg, 'uTex'), 0);
		gl.bindBuffer(gl.ARRAY_BUFFER, this.partBuf);
		gl.bufferData(gl.ARRAY_BUFFER, d.subarray(0, need), gl.STREAM_DRAW);
		const loc = (a) => gl.getAttribLocation(this.partProg, a);
		const attrs = [['aPos', 3, 0], ['aUV', 2, 12], ['aColor', 4, 20], ['aLayer', 1, 36]];
		for (const [a, size, off] of attrs) {
			const l = loc(a);
			gl.enableVertexAttribArray(l);
			gl.vertexAttribPointer(l, size, gl.FLOAT, false, 40, off);
		}
		gl.activeTexture(gl.TEXTURE0);
		for (const r of runs) {
			gl.bindTexture(gl.TEXTURE_2D_ARRAY, r.tex);
			if (r.mode === kRenderNormal) {
				gl.disable(gl.BLEND);
			} else {
				gl.enable(gl.BLEND);
				if (r.mode === kRenderTransAdd) gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
				else gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
			}
			gl.depthMask(!!r.depth);
			gl.drawArrays(gl.TRIANGLES, r.first * 6, r.count * 6);
		}
		for (const [a] of attrs) gl.disableVertexAttribArray(loc(a));
		gl.depthMask(true);
		gl.disable(gl.BLEND);
	}
}

function imageFor(frame, time) {
	if (!frame.group) return frame.images[0];
	const n = frame.intervals.length;
	const full = frame.intervals[n - 1];
	if (!(full > 0)) return frame.images[0];
	const t = time - Math.floor(time / full) * full;
	for (let i = 0; i < n - 1; i++) if (frame.intervals[i] > t) return frame.images[i];
	return frame.images[n - 1];
}
