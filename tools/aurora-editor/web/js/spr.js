// GoldSrc sprites (.spr, "IDSP" version 2): header, palette, frames and frame groups, and the pixels as the
// OpenGL renderer uploads them for each texture format:
//   0 normal, 1 additive: the palette colors, opaque
//   2 indexalpha: the last palette color everywhere, the index as alpha
//   3 alphatest: index 255 fully transparent, the rest opaque

export const SPR_TYPES = ['parallel upright', 'facing upright', 'parallel', 'oriented', 'parallel oriented'];
export const SPR_FORMATS = ['normal', 'additive', 'indexalpha', 'alphatest'];

export function parseSprite(buffer) {
	const dv = new DataView(buffer);
	const u8 = new Uint8Array(buffer);
	if (buffer.byteLength < 42) throw new Error('too short for a sprite');
	const ident = String.fromCharCode(u8[0], u8[1], u8[2], u8[3]);
	if (ident !== 'IDSP') throw new Error('not a sprite (no IDSP header)');
	const version = dv.getInt32(4, true);
	if (version !== 2) throw new Error(`sprite version ${version}: only Half-Life (version 2) sprites are read`);
	const spr = {
		version,
		type: dv.getInt32(8, true),
		texFormat: dv.getInt32(12, true),
		radius: dv.getFloat32(16, true),
		width: dv.getInt32(20, true),
		height: dv.getInt32(24, true),
		numframes: dv.getInt32(28, true),
		beamlength: dv.getFloat32(32, true),
		synctype: dv.getInt32(36, true),
		frames: [],	// { group: bool, intervals: [cumulative s], images: [index into images] }
		images: [],	// { w, h, ox, oy, rgba: Uint8ClampedArray }
	};
	let p = 40;
	const palCount = dv.getInt16(p, true);
	p += 2;
	if (palCount < 1 || palCount > 256 || p + palCount * 3 > buffer.byteLength) throw new Error('bad palette');
	const pal = new Uint8Array(256 * 3);
	pal.set(u8.subarray(p, p + palCount * 3));
	p += palCount * 3;
	spr.palette = pal;

	const readImage = () => {
		if (p + 16 > buffer.byteLength) throw new Error('cut off in a frame header');
		const ox = dv.getInt32(p, true), oy = dv.getInt32(p + 4, true);
		const w = dv.getInt32(p + 8, true), h = dv.getInt32(p + 12, true);
		p += 16;
		if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || p + w * h > buffer.byteLength) throw new Error('bad frame size');
		const idx = u8.subarray(p, p + w * h);
		p += w * h;
		const rgba = new Uint8ClampedArray(w * h * 4);
		const fmt = spr.texFormat;
		for (let i = 0; i < w * h; i++) {
			const c = idx[i];
			const o = i * 4;
			if (fmt === 2) {
				rgba[o] = pal[255 * 3]; rgba[o + 1] = pal[255 * 3 + 1]; rgba[o + 2] = pal[255 * 3 + 2];
				rgba[o + 3] = c;
			} else if (fmt === 3 && c === 255) {
				rgba[o] = rgba[o + 1] = rgba[o + 2] = 0;
				rgba[o + 3] = 0;
			} else {
				rgba[o] = pal[c * 3]; rgba[o + 1] = pal[c * 3 + 1]; rgba[o + 2] = pal[c * 3 + 2];
				rgba[o + 3] = 255;
			}
		}
		spr.images.push({ w, h, ox, oy, rgba, indices: idx });
		return spr.images.length - 1;
	};

	for (let f = 0; f < spr.numframes; f++) {
		if (p + 4 > buffer.byteLength) throw new Error('cut off before frame ' + f);
		const ftype = dv.getInt32(p, true);
		p += 4;
		if (ftype === 0) {
			spr.frames.push({ group: false, intervals: [0.1], images: [readImage()] });
		} else {
			const n = dv.getInt32(p, true);
			p += 4;
			if (n <= 0 || n > 1024) throw new Error('bad frame group');
			const intervals = [];
			for (let i = 0; i < n; i++) intervals.push(dv.getFloat32(p + i * 4, true));
			p += n * 4;
			const images = [];
			for (let i = 0; i < n; i++) images.push(readImage());
			spr.frames.push({ group: true, intervals, images });
		}
	}
	return spr;
}

// R_GetSpriteFrame: a group's picture by the client time (the intervals are cumulative)
export function groupImage(frame, time) {
	if (!frame.group) return frame.images[0];
	const n = frame.intervals.length;
	const full = frame.intervals[n - 1];
	if (!(full > 0)) return frame.images[0];
	const t = time - Math.floor(time / full) * full;
	for (let i = 0; i < n - 1; i++) if (frame.intervals[i] > t) return frame.images[i];
	return frame.images[n - 1];
}

// a frame as a canvas (thumbnails and the sprite browser); background: 'check' | color
export function frameCanvas(spr, imageIndex, size, background = 'check') {
	const img = spr.images[imageIndex];
	const c = document.createElement('canvas');
	const scale = Math.min(size / img.w, size / img.h, 4);
	c.width = Math.max(1, Math.round(img.w * scale));
	c.height = Math.max(1, Math.round(img.h * scale));
	drawFrame(c, spr, imageIndex, background);
	return c;
}

export function drawFrame(canvas, spr, imageIndex, background = 'check') {
	const img = spr.images[imageIndex];
	const ctx = canvas.getContext('2d');
	const src = document.createElement('canvas');
	src.width = img.w;
	src.height = img.h;
	const sctx = src.getContext('2d');
	const data = new ImageData(new Uint8ClampedArray(img.rgba), img.w, img.h);
	// additive sprites: what they add over black
	if (spr.texFormat === 1) for (let i = 3; i < data.data.length; i += 4) data.data[i] = 255;
	sctx.putImageData(data, 0, 0);
	ctx.clearRect(0, 0, canvas.width, canvas.height);
	if (background === 'check') {
		const s = 6;
		for (let y = 0; y < canvas.height; y += s) for (let x = 0; x < canvas.width; x += s) {
			ctx.fillStyle = ((x / s + y / s) & 1) ? '#2a2f36' : '#3a414b';
			ctx.fillRect(x, y, s, s);
		}
	} else if (background) {
		ctx.fillStyle = background;
		ctx.fillRect(0, 0, canvas.width, canvas.height);
	}
	if (spr.texFormat === 1) ctx.globalCompositeOperation = 'lighter';
	ctx.imageSmoothingEnabled = false;
	ctx.drawImage(src, 0, 0, canvas.width, canvas.height);
	ctx.globalCompositeOperation = 'source-over';
}
