// The game's particle code (SBO's particlesys.cpp / particlemgr.cpp, compiled to WebAssembly with core/shim.cpp).
// The editor sends it "Aurora" messages as env_particle does, sets the clock, view, scene and light, and gets back
// the quads the game would draw with the triangle API (and the state the engine would draw them with).

// the shim's parameter slots (core/shim.cpp, enum P_*)
export const P = {
	TIME: 0, CAM: 1, CAM_ANGLES: 4, FLOOR_ON: 7, FLOOR_Z: 8, WATER_ON: 9, WATER_MINS: 10, WATER_MAXS: 13,
	WALL_ON: 16, WALL_MINS: 17, WALL_MAXS: 20, LIGHT: 23, LAMP_ON: 26, LAMP_POS: 27, LAMP_RADIUS: 30, LAMP_COLOR: 31,
	TARGET_VISIBLE: 34, TARGET_POS: 35, TARGET_ANGLES: 38, ATTACHMENTS: 41, PVS: 53, TARGET_STUDIO: 54,
};

export const MSG = { ON: 1, ONESHOT: 2, REMOVE: 4, FOLLOW: 8 };
export const TARGET_ENTITY = 2;
export const QUAD_FLOATS = 28;

export class AuroraEngine {
	async init(seed = 1) {
		const mod = await import('../core/aurora-core.mjs');
		this.M = await mod.default({ locateFile: (f) => new URL(`../core/${f}`, import.meta.url).href });
		this.M._ae_init(seed);
		this.params = this.M._ae_params() >> 2;
		this.sprites = new Map();	// name -> frames
		this.files = new Map();
		return this;
	}

	str(s) {
		const n = this.M.lengthBytesUTF8(s) + 1;
		const p = this.M._malloc(n);
		this.M.stringToUTF8(s, p, n);
		return p;
	}

	withStr(s, fn) {
		const p = this.str(s);
		try { return fn(p); } finally { this.M._free(p); }
	}

	set(slot, ...values) {
		for (let i = 0; i < values.length; i++) this.M.HEAPF32[this.params + slot + i] = values[i];
	}

	get(slot) {
		return this.M.HEAPF32[this.params + slot];
	}

	seed(s) { this.M._ae_seed(s | 0); }

	// a script file the game can open (latin1 text, as the game reads bytes)
	setFile(name, text) {
		const bytes = new Uint8Array(text.length);
		for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i) & 0xff;
		const p = this.M._malloc(bytes.length + 1);
		this.M.HEAPU8.set(bytes, p);
		this.withStr(name, (n) => this.M._ae_file_add(n, p, bytes.length));
		this.M._free(p);
		this.files.set(name.toLowerCase(), text);
	}

	setSprite(name, numframes) {
		const key = name.toLowerCase();
		if (this.sprites.get(key) === numframes) return;
		this.sprites.set(key, numframes);
		this.withStr(key, (n) => this.M._ae_sprite_add(n, numframes));
	}

	// a new map: every system goes, the scripts and sprites are read again on the next message
	vidinit() { this.M._ae_vidinit(); }

	message(flags, id, origin, angles, script, target = 0, attachment = -1) {
		this.withStr(script, (s) => this.M._ae_message(flags, id, origin[0], origin[1], origin[2],
			angles[0], angles[1], angles[2], target, attachment, s));
	}

	// one client frame at time t; returns a Float32Array of quads (QUAD_FLOATS each), valid until the next call
	frame(t) {
		this.set(P.TIME, t);
		const n = this.M._ae_frame();
		if (!n) return new Float32Array(0);
		const p = this.M._ae_quads() >> 2;
		return this.M.HEAPF32.subarray(p, p + n * QUAD_FLOATS);
	}

	spriteName(h) { return this.M.UTF8ToString(this.M._ae_sprite_name(h)); }

	takeLog() {
		const s = this.M.UTF8ToString(this.M._ae_log());
		this.M._ae_log_clear();
		return s;
	}

	stat(which, id = 0) { return this.M._ae_stat(which, id); }
}
