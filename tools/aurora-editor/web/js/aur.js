// .aur scripts: the engine's tokenizer and SBO's parser structure (src/cl_dll/aurora/particlesys.cpp) with token
// positions, so the editor can show what the game reads and change one value without touching the rest of the text
// (comments, unknown keys, formatting stay). The text is the source of truth; nothing here rebuilds a script.

// COM_ParseFile: whitespace and bytes >= 128 skipped (the engine's signed char), // comments, "quoted" tokens,
// { } ( ) ' : as single tokens. Returns { text, start, end, quoted } or null at the end (the engine's NULL).
export function nextToken(src, pos) {
	const n = src.length;
	for (;;) {
		while (pos < n) {
			const c = src.charCodeAt(pos);
			if (c > 32 && c < 128) break;
			pos++;
		}
		if (pos >= n) return null;
		if (src[pos] === '/' && src[pos + 1] === '/') {
			while (pos < n && src[pos] !== '\n') pos++;
			continue;
		}
		break;
	}
	const start = pos;
	const c = src[pos];
	if (c === '"') {
		pos++;
		let text = '';
		while (pos < n && src[pos] !== '"') text += src[pos++];
		const end = pos < n ? pos + 1 : pos;
		return { text, start, end, quoted: true, unterminated: pos >= n };
	}
	if ('{}()\':'.includes(c)) return { text: c, start, end: pos + 1, quoted: false };
	let text = '';
	do {
		text += src[pos++];
		if (pos >= n) break;
		const d = src[pos];
		if ('{}()\':'.includes(d)) break;
		const code = src.charCodeAt(pos);
		if (!(code > 32 && code < 128)) break;
	} while (true);
	return { text, start, end: pos, quoted: false };
}

const TOP_KEYS = new Set(['particles', 'maintype', 'lightmodel', 'killcondition']);
export const TYPE_KEYS = new Set(['name', 'gravity', 'windyaw', 'windstrength', 'sprite', 'startalpha', 'endalpha',
	'startred', 'endred', 'startgreen', 'endgreen', 'startblue', 'endblue', 'startsize', 'sizedelta', 'endsize',
	'startangle', 'angledelta', 'startframe', 'endframe', 'framerate', 'lifetime', 'spraytype', 'overlaytype',
	'sprayrate', 'sprayforce', 'spraypitch', 'sprayyaw', 'drag', 'bounce', 'bouncefriction', 'collision',
	'rendermode', 'drawcondition']);

// The script's structure as the game walks it. options.xashKeys: the core reads lightmodel / killcondition.
//   top: [{ key, keyTok, valTok, known }]   blocks: [{ open, close, entries: [...], name }]
//   unknown: entries the game skips with a warning
export function parse(src, options = {}) {
	const xash = options.xashKeys !== false;
	const typeKill = options.typeKillcondition !== false;
	const doc = { src, top: [], blocks: [], problems: [] };
	let pos = 0;
	let tok = nextToken(src, pos);
	const read = () => {
		const t = nextToken(src, pos);
		if (t) pos = t.end;
		return t;
	};
	if (tok) pos = tok.end;
	while (tok) {
		const key = tok.text.toLowerCase();
		if (tok.text === '{') {
			const block = { open: tok, close: null, entries: [], name: '' };
			doc.blocks.push(block);
			let t = read();
			while (true) {
				if (!t) break;
				if (t.text === '}') { block.close = t; break; }
				const k = t.text.toLowerCase();
				if (TYPE_KEYS.has(k) || (typeKill && k === 'killcondition')) {
					const v = read();
					block.entries.push({ key: k, keyTok: t, valTok: v, known: true, block });
					if (k === 'name' && v) block.name = v.text.slice(0, 29);
					if (!v) break;
				} else if (t.text) {
					const v = read();
					block.entries.push({ key: k, keyTok: t, valTok: v, known: false, block });
					if (v && v.text === '}') { block.close = v; block.closedByValue = true; break; }
					if (!v) break;
				}
				t = read();
			}
			tok = block.close ? read() : null;
			continue;
		}
		if (TOP_KEYS.has(key) && (xash || key === 'particles' || key === 'maintype')) {
			const v = read();
			doc.top.push({ key, keyTok: tok, valTok: v, known: true, block: null });
			if (!v) break;
		} else if (tok.text) {
			const v = read();
			doc.top.push({ key, keyTok: tok, valTok: v, known: false, block: null });
			if (!v) break;
		}
		tok = read();
	}
	return doc;
}

// the last value of a key in a block (or at the top level with block = null): the one the game keeps
export function getEntry(doc, block, key) {
	const list = block ? block.entries : doc.top;
	let found = null;
	for (const e of list) if (e.key === key && e.known) found = e;
	return found;
}

export function getValue(doc, block, key) {
	const e = getEntry(doc, block, key);
	return e && e.valTok ? e.valTok.text : undefined;
}

// the type the game ends up with for a name (the last block of that name; a later definition replaces it)
export function findBlock(doc, name) {
	const n = (name || '').toLowerCase().slice(0, 29);
	let found = null;
	for (const b of doc.blocks) if (b.name && b.name.toLowerCase() === n) found = b;
	return found;
}

// C atof: the longest number at the start, else 0
export function atof(s) {
	const m = /^\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/.exec(s || '');
	return m ? parseFloat(m[0]) : 0;
}

// RandomRange( const char * ): "min..max" or one number
export function parseRange(s) {
	if (s === undefined) return null;
	const i = s.indexOf('..');
	if (i >= 0) return { min: atof(s.slice(0, Math.min(i, 63))), max: atof(s.slice(i + 2)), range: true };
	const v = atof(s);
	return { min: v, max: v, range: false };
}

export function formatNumber(v) {
	if (!isFinite(v)) return '0';
	const r = Math.round(v * 10000) / 10000;
	return String(r);
}

export function formatRange(min, max) {
	if (min === max) return formatNumber(min);
	return `${formatNumber(min)}..${formatNumber(max)}`;
}

//=============================================================================
// edits: each returns the new text
//=============================================================================

function quote(v) {
	return `"${String(v).replace(/"/g, '')}"`;
}

function lineStart(src, i) {
	while (i > 0 && src[i - 1] !== '\n') i--;
	return i;
}

function lineEnd(src, i) {
	while (i < src.length && src[i] !== '\n') i++;
	return i;
}

function indentOf(src, tok) {
	const s = lineStart(src, tok.start);
	const m = /^[ \t]*/.exec(src.slice(s, tok.start));
	return m && m[0].length === tok.start - s ? m[0] : null;
}

function blockIndent(doc, block) {
	for (const e of block.entries) {
		const ind = indentOf(doc.src, e.keyTok);
		if (ind !== null) return ind;
	}
	return '  ';
}

// set key = value in a block (null: the top level): the last occurrence is changed, or a line is added
export function setValue(doc, block, key, value) {
	const src = doc.src;
	const e = getEntry(doc, block, key);
	if (e && e.valTok) {
		return src.slice(0, e.valTok.start) + quote(value) + src.slice(e.valTok.end);
	}
	if (e && !e.valTok) {
		return src.slice(0, e.keyTok.end) + ' ' + quote(value) + src.slice(e.keyTok.end);
	}
	const line = `${quote(key)} ${quote(value)}`;
	if (block) {
		if (!block.close) return src + '\n' + blockIndent(doc, block) + line + '\n}\n';
		const ind = blockIndent(doc, block);
		const closeLine = lineStart(src, block.close.start);
		if (/^[ \t]*$/.test(src.slice(closeLine, block.close.start))) {
			return src.slice(0, closeLine) + ind + line + '\n' + src.slice(closeLine);
		}
		return src.slice(0, block.close.start) + '\n' + ind + line + '\n' + src.slice(block.close.start);
	}
	// top level: after the last top-level entry, or before the first block, or at the end
	const last = doc.top.filter((t) => t.valTok).pop();
	if (last) {
		const at = lineEnd(src, last.valTok.end);
		return src.slice(0, at) + '\n' + line + src.slice(at);
	}
	if (doc.blocks.length) {
		const at = lineStart(src, doc.blocks[0].open.start);
		return src.slice(0, at) + line + '\n\n' + src.slice(at);
	}
	return (src && !src.endsWith('\n') ? src + '\n' : src) + line + '\n';
}

// remove every occurrence of a key in a block (or at the top level)
export function removeKey(doc, block, key) {
	const list = (block ? block.entries : doc.top).filter((e) => e.key === key);
	let src = doc.src;
	for (const e of list.reverse()) {
		const endTok = e.valTok && e.valTok.text !== '}' ? e.valTok : e.keyTok;
		let s = e.keyTok.start;
		let t = endTok.end;
		const ls = lineStart(src, s);
		const le = lineEnd(src, t);
		const before = src.slice(ls, s);
		const after = src.slice(t, le);
		if (/^[ \t]*$/.test(before) && /^[ \t]*(\/\/.*)?$/.test(after)) {
			s = ls;
			t = le < src.length ? le + 1 : le;
		}
		src = src.slice(0, s) + src.slice(t);
	}
	return src;
}

export function addType(src, name, lines) {
	const body = lines.map(([k, v]) => `  ${quote(k)} ${quote(v)}`).join('\n');
	const sep = src.length === 0 ? '' : src.endsWith('\n\n') ? '' : src.endsWith('\n') ? '\n' : '\n\n';
	return `${src}${sep}{\n  "name" ${quote(name)}\n${body ? body + '\n' : ''}}\n`;
}

export function removeBlock(doc, block) {
	const src = doc.src;
	const s = lineStart(src, block.open.start);
	let t = block.close ? block.close.end : src.length;
	const le = lineEnd(src, t);
	if (/^[ \t]*$/.test(src.slice(t, le))) t = le < src.length ? le + 1 : le;
	// a blank line left behind twice
	let out = src.slice(0, s) + src.slice(t);
	out = out.replace(/\n{3,}/g, '\n\n');
	return out;
}

export function blockText(doc, block) {
	return doc.src.slice(block.open.start, block.close ? block.close.end : doc.src.length);
}

// rename a type and every reference to it (maintype, spraytype, overlaytype)
export function renameType(doc, oldName, newName) {
	const o = oldName.toLowerCase();
	const edits = [];
	for (const e of doc.top) if (e.key === 'maintype' && e.valTok && e.valTok.text.toLowerCase() === o) edits.push(e.valTok);
	for (const b of doc.blocks) {
		for (const e of b.entries) {
			if (!e.valTok) continue;
			if ((e.key === 'name' || e.key === 'spraytype' || e.key === 'overlaytype') && e.valTok.text.toLowerCase().slice(0, 29) === o.slice(0, 29)) edits.push(e.valTok);
		}
	}
	edits.sort((a, b) => b.start - a.start);
	let src = doc.src;
	for (const t of edits) src = src.slice(0, t.start) + quote(newName) + src.slice(t.end);
	return src;
}

//=============================================================================
// the type tree: main type, sprayed and overlaid types
//=============================================================================

export function typeTree(doc) {
	const main = getValue(doc, null, 'maintype');
	const seen = new Set();
	const node = (name, rel, depth) => {
		const block = findBlock(doc, name);
		const key = (name || '').toLowerCase();
		const n = { name, rel, block, children: [], cycle: seen.has(key), missing: !block };
		if (!block || n.cycle || depth > 12) return n;
		seen.add(key);
		const spray = getValue(doc, block, 'spraytype');
		const overlay = getValue(doc, block, 'overlaytype');
		if (overlay) n.children.push(node(overlay, 'overlay', depth + 1));
		if (spray) n.children.push(node(spray, 'spray', depth + 1));
		seen.delete(key);
		return n;
	};
	const roots = [];
	const reached = new Set();
	const walk = (n) => { if (n.block) reached.add(n.block); n.children.forEach(walk); };
	if (main) { const r = node(main, 'main', 0); roots.push(r); walk(r); }
	const unused = doc.blocks.filter((b) => !reached.has(b));
	return { roots, unused };
}

//=============================================================================
// hints beyond the game's own warnings (those come from the game code itself, see engine.js)
//=============================================================================

export function lint(doc, ctx = {}) {
	const out = [];
	const add = (level, text, where) => out.push({ level, text, where });
	const limits = ctx.limits || { maxPerSystem: 2048, defaultParticles: 100, maxTypeName: 29 };
	const spriteExists = ctx.spriteExists || (() => true);

	for (const e of doc.top) {
		if (!e.known) {
			const f = (ctx.foreign || []).find((x) => x.key === e.key && x.scope === 'top');
			if (f) add('info', `${e.key}: ${f.help}`, e);
		}
		if (e.valTok && e.valTok.unterminated) add('warn', `the quote after ${e.key} is not closed`, e);
	}
	for (const b of doc.blocks) {
		if (!b.close) add('warn', `the block of "${b.name || '(no name)'}" is not closed with }`, b);
		if (b.closedByValue) add('warn', `a key without a value just before } in "${b.name}"`, b);
		if (!b.name) add('warn', 'a type without "name": nothing can refer to it', b);
		const nameEntry = getEntry(doc, b, 'name');
		if (nameEntry && nameEntry.valTok && nameEntry.valTok.text.length > limits.maxTypeName) {
			add('warn', `type name "${nameEntry.valTok.text}" is cut to ${limits.maxTypeName} characters`, nameEntry);
		}
		const counts = {};
		for (const e of b.entries) {
			if (!e.known) {
				const f = (ctx.foreign || []).find((x) => x.key === e.key && x.scope === 'type');
				if (f) add('info', `${b.name}: ${e.key}: ${f.help}`, e);
				continue;
			}
			counts[e.key] = (counts[e.key] || 0) + 1;
			if (e.valTok && e.valTok.unterminated) add('warn', `the quote after ${e.key} is not closed`, e);
			if (e.valTok && ctx.schemaKinds && ctx.schemaKinds[e.key] === 'range') {
				const v = e.valTok.text;
				const parts = v.split('..');
				const clean = (s) => /^\s*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?\s*$/.test(s);
				if (!(parts.length <= 2 && parts.every(clean))) {
					const r = parseRange(v);
					add('warn', `${b.name}: ${e.key} "${v}" is read as ${formatRange(r.min, r.max)}`, e);
				}
			}
		}
		for (const [k, c] of Object.entries(counts)) if (c > 1 && k !== 'name') add('info', `${b.name}: ${k} is given ${c} times; the last one counts`, b);
		if (getValue(doc, b, 'endsize') !== undefined && getValue(doc, b, 'sizedelta') !== undefined) add('info', `${b.name}: endsize is set, so sizedelta is ignored`, b);
		if (getValue(doc, b, 'endframe') !== undefined && getValue(doc, b, 'framerate') !== undefined) add('info', `${b.name}: endframe is set, so framerate is ignored`, b);
		const sprite = getValue(doc, b, 'sprite');
		if (sprite !== undefined && !spriteExists(sprite)) add('warn', `${b.name}: sprite ${sprite} is not in the game files the editor has (SBO, Half-Life, your uploads)`, b);
		const spray = getValue(doc, b, 'spraytype');
		if (spray && !(parseRange(getValue(doc, b, 'sprayrate') || '0').max > 0)) add('info', `${b.name}: sprays ${spray} but has no sprayrate`, b);
		const cm = (getValue(doc, b, 'rendermode') || '').toLowerCase();
		if ((cm === 'texture' || cm === 'color') && sprite) add('info', `${b.name}: rendermode ${cm}: in the game the triangle API turns alpha into darkening for this mode (no see-through fade); "solid" fades`, b);
	}

	// sprayed types that die at once, and a particle budget estimate
	const main = getValue(doc, null, 'maintype');
	const tree = typeTree(doc);
	let estimate = 0;
	const visit = (n, parentRate, mainNode) => {
		if (!n.block || n.cycle) return;
		const life = parseRange(getValue(doc, n.block, 'lifetime') || '0');
		const lifeAvg = (life.min + life.max) / 2;
		let alive = mainNode ? 1 : parentRate * lifeAvg;
		if (!mainNode && n.rel === 'spray' && lifeAvg <= 0) add('warn', `${n.name}: sprayed but its lifetime is 0: each particle dies at once`, n.block);
		if (n.rel === 'overlay') alive = 0;	// counted with its particle below
		estimate += alive;
		for (const c of n.children) {
			if (c.rel === 'overlay') { estimate += mainNode ? 1 : parentRate * lifeAvg; visit(c, 0, false); continue; }
			const r = parseRange(getValue(doc, n.block, 'sprayrate') || '0');
			visit(c, ((r.min + r.max) / 2) * (mainNode ? 1 : parentRate * lifeAvg), false);
		}
	};
	for (const r of tree.roots) visit(r, 1, true);
	if (main && !findBlock(doc, main)) add('warn', `the main type "${main}" has no block`, null);
	for (const b of tree.unused) add('info', `type "${b.name || '(no name)'}" is not used by the main type`, b);
	const pv = getValue(doc, null, 'particles');
	const particles = pv === undefined ? limits.defaultParticles : Math.max(1, Math.min(limits.maxPerSystem, parseInt(pv, 10) || 0));
	if (pv !== undefined && (parseInt(pv, 10) || 0) > limits.maxPerSystem) add('warn', `particles ${pv} is lowered to ${limits.maxPerSystem}`, null);
	if (isFinite(estimate) && estimate > particles * 1.05) add('info', `about ${Math.round(estimate)} particles would be alive at once (rate x lifetime), "particles" is ${particles}: sprays wait for free particles`, null);
	return { hints: out, estimate, particles };
}

export function spritesOf(doc) {
	const set = new Set();
	for (const b of doc.blocks) for (const e of b.entries) if (e.key === 'sprite' && e.valTok) set.add(e.valTok.text.toLowerCase().replace(/\\/g, '/'));
	return [...set];
}
