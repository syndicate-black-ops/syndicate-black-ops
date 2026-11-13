// Small helpers: latin1 text, downloads, a store-only zip writer, compressed share links, safe localStorage.

export function latin1Decode(buf) {
	const u8 = new Uint8Array(buf);
	let s = '';
	for (let i = 0; i < u8.length; i += 8192) s += String.fromCharCode.apply(null, u8.subarray(i, i + 8192));
	return s;
}

export function latin1Encode(s) {
	const u8 = new Uint8Array(s.length);
	for (let i = 0; i < s.length; i++) {
		const c = s.charCodeAt(i);
		u8[i] = c < 256 ? c : 63;	// '?' for characters a script cannot hold
	}
	return u8;
}

export function download(name, data, type = 'application/octet-stream') {
	const blob = data instanceof Blob ? data : new Blob([data], { type });
	const a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = name;
	document.body.appendChild(a);
	a.click();
	setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
}

const CRC_TABLE = (() => {
	const t = new Uint32Array(256);
	for (let n = 0; n < 256; n++) {
		let c = n;
		for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
		t[n] = c >>> 0;
	}
	return t;
})();

export function crc32(u8) {
	let c = 0xffffffff;
	for (let i = 0; i < u8.length; i++) c = CRC_TABLE[(c ^ u8[i]) & 0xff] ^ (c >>> 8);
	return (c ^ 0xffffffff) >>> 0;
}

// files: [{ name: 'aurora/x.aur', data: Uint8Array }] -> Blob (zip, stored)
export function makeZip(files) {
	const parts = [];
	const central = [];
	let offset = 0;
	const enc = new TextEncoder();
	const now = new Date();
	const dosTime = (now.getHours() << 11) | (now.getMinutes() << 5) | (now.getSeconds() >> 1);
	const dosDate = ((now.getFullYear() - 1980) << 9) | ((now.getMonth() + 1) << 5) | now.getDate();
	for (const f of files) {
		const name = enc.encode(f.name);
		const crc = crc32(f.data);
		const h = new DataView(new ArrayBuffer(30));
		h.setUint32(0, 0x04034b50, true); h.setUint16(4, 10, true); h.setUint16(6, 0, true); h.setUint16(8, 0, true);
		h.setUint16(10, dosTime, true); h.setUint16(12, dosDate, true); h.setUint32(14, crc, true);
		h.setUint32(18, f.data.length, true); h.setUint32(22, f.data.length, true);
		h.setUint16(26, name.length, true); h.setUint16(28, 0, true);
		parts.push(new Uint8Array(h.buffer), name, f.data);
		const c = new DataView(new ArrayBuffer(46));
		c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 10, true); c.setUint16(8, 0, true);
		c.setUint16(10, 0, true); c.setUint16(12, dosTime, true); c.setUint16(14, dosDate, true); c.setUint32(16, crc, true);
		c.setUint32(20, f.data.length, true); c.setUint32(24, f.data.length, true); c.setUint16(28, name.length, true);
		c.setUint16(30, 0, true); c.setUint16(32, 0, true); c.setUint16(34, 0, true); c.setUint16(36, 0, true);
		c.setUint32(38, 0, true); c.setUint32(42, offset, true);
		central.push(new Uint8Array(c.buffer), name);
		offset += 30 + name.length + f.data.length;
	}
	const size = central.reduce((s, p) => s + p.length, 0);
	const e = new DataView(new ArrayBuffer(22));
	e.setUint32(0, 0x06054b50, true); e.setUint16(8, files.length, true); e.setUint16(10, files.length, true);
	e.setUint32(12, size, true); e.setUint32(16, offset, true);
	return new Blob([...parts, ...central, new Uint8Array(e.buffer)], { type: 'application/zip' });
}

function b64url(u8) {
	let s = '';
	for (let i = 0; i < u8.length; i++) s += String.fromCharCode(u8[i]);
	return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function unb64url(s) {
	const b = atob(s.replace(/-/g, '+').replace(/_/g, '/'));
	const u8 = new Uint8Array(b.length);
	for (let i = 0; i < b.length; i++) u8[i] = b.charCodeAt(i);
	return u8;
}

async function pipe(u8, stream) {
	const out = new Response(new Blob([u8]).stream().pipeThrough(stream));
	return new Uint8Array(await out.arrayBuffer());
}

// state -> "z<base64url deflate-raw JSON>" (or "j<base64url JSON>" without CompressionStream)
export async function packState(obj) {
	const json = new TextEncoder().encode(JSON.stringify(obj));
	if (typeof CompressionStream === 'function') return 'z' + b64url(await pipe(json, new CompressionStream('deflate-raw')));
	return 'j' + b64url(json);
}

export async function unpackState(s) {
	const kind = s[0];
	const u8 = unb64url(s.slice(1));
	const json = kind === 'z' ? await pipe(u8, new DecompressionStream('deflate-raw')) : u8;
	return JSON.parse(new TextDecoder().decode(json));
}

export const store = {
	get(key) {
		try { const v = window.localStorage.getItem(key); return v === null ? null : JSON.parse(v); } catch { return null; }
	},
	set(key, value) {
		try { window.localStorage.setItem(key, JSON.stringify(value)); return true; } catch { return false; }
	},
	remove(key) {
		try { window.localStorage.removeItem(key); } catch { /* storage blocked */ }
	},
};

export async function copyText(text) {
	try {
		await navigator.clipboard.writeText(text);
		return true;
	} catch {
		const ta = document.createElement('textarea');
		ta.value = text;
		document.body.appendChild(ta);
		ta.select();
		let ok = false;
		try { ok = document.execCommand('copy'); } catch { ok = false; }
		ta.remove();
		return ok;
	}
}

export function escapeHtml(s) {
	return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}

// uploads kept in this browser (IndexedDB): name -> ArrayBuffer | string; every call is safe when storage is blocked
const DB_NAME = 'aurora-editor', DB_STORE = 'uploads';

function openDb() {
	return new Promise((resolve, reject) => {
		const req = window.indexedDB.open(DB_NAME, 1);
		req.onupgradeneeded = () => req.result.createObjectStore(DB_STORE);
		req.onsuccess = () => resolve(req.result);
		req.onerror = () => reject(req.error);
	});
}

async function dbRun(mode, fn) {
	const db = await openDb();
	try {
		return await new Promise((resolve, reject) => {
			const tx = db.transaction(DB_STORE, mode);
			const out = fn(tx.objectStore(DB_STORE));
			tx.oncomplete = () => resolve(out && 'result' in out ? out.result : undefined);
			tx.onerror = () => reject(tx.error);
			tx.onabort = () => reject(tx.error);
		});
	} finally {
		db.close();
	}
}

export const uploadStore = {
	async all() {
		try {
			const out = new Map();
			const db = await openDb();
			try {
				await new Promise((resolve, reject) => {
					const tx = db.transaction(DB_STORE, 'readonly');
					const req = tx.objectStore(DB_STORE).openCursor();
					req.onsuccess = () => {
						const c = req.result;
						if (c) { out.set(c.key, c.value); c.continue(); }
					};
					tx.oncomplete = () => resolve();
					tx.onerror = () => reject(tx.error);
				});
			} finally {
				db.close();
			}
			return out;
		} catch { return new Map(); }
	},
	async put(name, value) {
		try { await dbRun('readwrite', (s) => s.put(value, name)); return true; } catch { return false; }
	},
	async remove(name) {
		try { await dbRun('readwrite', (s) => s.delete(name)); return true; } catch { return false; }
	},
	async clear() {
		try { await dbRun('readwrite', (s) => s.clear()); return true; } catch { return false; }
	},
};
