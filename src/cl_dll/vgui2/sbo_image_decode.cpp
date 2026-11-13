//========= SBO VGUI2: image decoding for the server browser (map pictures, overviews) =========//
//
// Purpose: see sbo_image_decode.h.
//=================================================================//

#include <string.h>

#include <algorithm>

#include "sbo_image_decode.h"

// stb_image: JPEG only, from memory, no file access, our size limit
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD // clang-cl: its SSE2 intrinsics need a target feature the client is not built with
#define STBI_MAX_DIMENSIONS 4096
#include "stb_image.h"

namespace
{
//! Area average of an RGBA picture (w x h) down to fit maxWide x maxTall, aspect kept, never larger.
void ScaleToFit(const std::vector<uint8_t> &full, int w, int h, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall)
{
	double scale = 1.0;
	if (w > maxWide)
		scale = (double)maxWide / w;
	if (h * scale > maxTall)
		scale = (double)maxTall / h;
	int dw = (int)(w * scale), dh = (int)(h * scale);
	if (dw < 1)
		dw = 1;
	if (dh < 1)
		dh = 1;

	rgba.assign((size_t)dw * (size_t)dh * 4, 0);
	for (int ty = 0; ty < dh; ty++)
	{
		const int y0 = (int)((long long)ty * h / dh), y1 = (int)((long long)(ty + 1) * h / dh);
		for (int tx = 0; tx < dw; tx++)
		{
			const int x0 = (int)((long long)tx * w / dw), x1 = (int)((long long)(tx + 1) * w / dw);
			unsigned sum[4] = {};
			unsigned count = 0;
			for (int y = y0; y < (y1 > y0 ? y1 : y0 + 1); y++)
			{
				for (int x = x0; x < (x1 > x0 ? x1 : x0 + 1); x++)
				{
					const uint8_t *s = &full[((size_t)y * (size_t)w + (size_t)x) * 4];
					for (int c = 0; c < 4; c++)
						sum[c] += s[c];
					count++;
				}
			}
			uint8_t *d = &rgba[((size_t)ty * (size_t)dw + (size_t)tx) * 4];
			for (int c = 0; c < 4; c++)
				d[c] = (uint8_t)(sum[c] / count);
		}
	}
	wide = dw;
	tall = dh;
}
} // namespace

bool sbo_image::DecodeJpegScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall)
{
	// JPEG starts with the SOI marker FF D8 FF
	if (!data || size < 4 || size > MAX_FILE_BYTES || maxWide < 1 || maxTall < 1 || data[0] != 0xFF || data[1] != 0xD8
	    || data[2] != 0xFF)
		return false;
	int w = 0, h = 0, comp = 0;
	if (!stbi_info_from_memory(data, (int)size, &w, &h, &comp) || w < 1 || h < 1 || w > MAX_SOURCE_SIDE
	    || h > MAX_SOURCE_SIDE)
		return false;
	stbi_uc *pixels = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
	if (!pixels)
		return false;
	std::vector<uint8_t> full(pixels, pixels + (size_t)w * (size_t)h * 4);
	stbi_image_free(pixels);
	ScaleToFit(full, w, h, maxWide, maxTall, rgba, wide, tall);
	return true;
}

bool sbo_image::ReadTgaHeader(const uint8_t *data, size_t size, int &wide, int &tall, int &bits, int &imageType)
{
	if (!data || size < 18)
		return false;
	imageType = data[2];
	wide = data[12] | (data[13] << 8);
	tall = data[14] | (data[15] << 8);
	bits = data[16];
	return (imageType == 2 || imageType == 10) && (bits == 24 || bits == 32) && data[1] <= 1 && wide >= 1 && tall >= 1 &&
	       wide <= MAX_SOURCE_SIDE && tall <= MAX_SOURCE_SIDE;
}

bool sbo_image::DecodeTgaScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall, bool keyGreen)
{
	if (!data || size < 18 || maxWide < 1 || maxTall < 1)
		return false;
	const int idLength = data[0];
	const int colorMapType = data[1];
	const int imageType = data[2];
	const int colorMapLength = data[5] | (data[6] << 8);
	const int colorMapBits = data[7];
	const int w = data[12] | (data[13] << 8);
	const int h = data[14] | (data[15] << 8);
	const int bits = data[16];
	const bool topFirst = (data[17] & 0x20) != 0;
	if ((imageType != 2 && imageType != 10) || (bits != 24 && bits != 32) || w < 1 || h < 1 || w > MAX_SOURCE_SIDE
	    || h > MAX_SOURCE_SIDE || colorMapType > 1)
		return false;

	size_t pos = 18 + (size_t)idLength;
	if (colorMapType == 1)
		pos += (size_t)colorMapLength * (size_t)((colorMapBits + 7) / 8); // a color map a truecolor image ignores
	if (pos > size)
		return false;

	// decode to RGBA, top row first
	const int bpp = bits / 8;
	const size_t pixels = (size_t)w * (size_t)h;
	std::vector<uint8_t> full(pixels * 4);
	size_t n = 0;
	auto put = [&](const uint8_t *p) {
		const size_t row = n / (size_t)w, col = n % (size_t)w;
		const size_t y = topFirst ? row : (size_t)h - 1 - row;
		uint8_t *d = &full[(y * (size_t)w + col) * 4];
		d[0] = p[2];
		d[1] = p[1];
		d[2] = p[0];
		d[3] = bpp == 4 ? p[3] : 255;
		if (keyGreen && d[0] == 0 && d[1] == 255 && d[2] == 0)
			d[0] = d[1] = d[2] = d[3] = 0;
		n++;
	};
	while (n < pixels)
	{
		if (imageType == 2)
		{
			if (pos + (size_t)bpp > size)
				return false;
			put(data + pos);
			pos += (size_t)bpp;
			continue;
		}
		if (pos >= size)
			return false;
		const int header = data[pos++];
		const size_t count = (size_t)(header & 0x7f) + 1;
		if (n + count > pixels)
			return false;
		if (header & 0x80)
		{
			if (pos + (size_t)bpp > size)
				return false;
			for (size_t i = 0; i < count; i++)
				put(data + pos);
			pos += (size_t)bpp;
		}
		else
		{
			if (pos + count * (size_t)bpp > size)
				return false;
			for (size_t i = 0; i < count; i++)
			{
				put(data + pos);
				pos += (size_t)bpp;
			}
		}
	}

	ScaleToFit(full, w, h, maxWide, maxTall, rgba, wide, tall);
	return true;
}

bool sbo_image::DecodeSprScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall)
{
	if (!data || size < 42 || size > MAX_FILE_BYTES || maxWide < 1 || maxTall < 1)
		return false;
	auto i32 = [data](size_t at) {
		return (int32_t)((uint32_t)data[at] | ((uint32_t)data[at + 1] << 8) | ((uint32_t)data[at + 2] << 16) |
		                 ((uint32_t)data[at + 3] << 24));
	};
	if (memcmp(data, "IDSP", 4) != 0 || i32(4) != 2)
		return false;
	const int texFormat = i32(12);
	const int numFrames = i32(28);
	if (numFrames < 1)
		return false;
	// the palette after the 40-byte header: a short count, then count RGB triples
	size_t at = 40;
	const int colors = data[at] | (data[at + 1] << 8);
	at += 2;
	if (colors < 1 || colors > 256 || at + (size_t)colors * 3 > size)
		return false;
	uint8_t pal[256][3] = {};
	memcpy(pal, data + at, (size_t)colors * 3);
	at += (size_t)colors * 3;
	// the frames: type 0 a single frame, 1 a group (count, intervals, then its frames: its first is used). An
	// animation's first frame is often empty or tiny (smoke growing from nothing): the frame with the most to see
	// among the first 256 (32 of them looked at, evenly spread) is shown.
	struct Frame
	{
		size_t pixels;
		int w, h;
	};
	std::vector<Frame> frames;
	for (int f = 0; f < numFrames && f < 256; f++)
	{
		if (at + 4 > size)
			break;
		const int frameType = i32(at);
		at += 4;
		bool group = false;
		if (frameType != 0)
		{
			if (at + 4 > size)
				break;
			const int groupFrames = i32(at);
			at += 4;
			if (groupFrames < 1 || groupFrames > 4096 || at + (size_t)groupFrames * 4 > size)
				break;
			at += (size_t)groupFrames * 4; // intervals
			group = true;
		}
		if (at + 16 > size)
			break;
		const int fw = i32(at + 8), fh = i32(at + 12);
		at += 16;
		if (fw < 1 || fh < 1 || fw > MAX_SOURCE_SIDE || fh > MAX_SOURCE_SIDE || at + (size_t)fw * (size_t)fh > size)
			break;
		frames.push_back({ at, fw, fh });
		at += (size_t)fw * (size_t)fh;
		if (group)
			break; // the rest of a group's frames are not walked
	}
	if (frames.empty())
		return false;
	const size_t step = (frames.size() + 31) / 32;
	size_t best = 0;
	unsigned long long bestScore = 0;
	for (size_t f = 0; f < frames.size(); f += step)
	{
		unsigned long long score = 0;
		const uint8_t *px = data + frames[f].pixels;
		for (size_t i = 0; i < (size_t)frames[f].w * (size_t)frames[f].h; i++)
		{
			const uint8_t idx = px[i];
			if (texFormat == 2)
				score += idx;
			else if (texFormat == 3)
				score += idx != 255 ? 255u : 0u;
			else
				score += (unsigned)pal[idx][0] + pal[idx][1] + pal[idx][2];
		}
		if (score > bestScore)
		{
			bestScore = score;
			best = f;
		}
	}
	at = frames[best].pixels;
	const int w = frames[best].w, h = frames[best].h;
	std::vector<uint8_t> full((size_t)w * (size_t)h * 4);
	for (size_t i = 0; i < (size_t)w * (size_t)h; i++)
	{
		const uint8_t idx = data[at + i];
		uint8_t *d = &full[i * 4];
		switch (texFormat)
		{
		case 2: // index alpha: one color, the index is its alpha
			d[0] = pal[255][0];
			d[1] = pal[255][1];
			d[2] = pal[255][2];
			d[3] = idx;
			break;
		case 3: // alpha test: the last color is a hole
			d[0] = pal[idx][0];
			d[1] = pal[idx][1];
			d[2] = pal[idx][2];
			d[3] = idx == 255 ? 0 : 255;
			break;
		default:
			d[0] = pal[idx][0];
			d[1] = pal[idx][1];
			d[2] = pal[idx][2];
			d[3] = 255;
			break;
		}
	}
	// additive and index alpha sprites (glows, smoke, steam) are faint on black: brightest pixel to full white
	if (texFormat == 1 || texFormat == 2)
	{
		int top = 0;
		for (size_t i = 0; i < full.size(); i += 4)
			top = std::max(top, (int)std::max(full[i], std::max(full[i + 1], full[i + 2])));
		if (top > 0 && top < 255)
			for (size_t i = 0; i < full.size(); i += 4)
				for (int c = 0; c < 3; c++)
					full[i + c] = (uint8_t)std::min(255, full[i + c] * 255 / top);
	}
	if (w >= maxWide || h >= maxTall)
	{
		ScaleToFit(full, w, h, maxWide, maxTall, rgba, wide, tall);
		return true;
	}
	// smaller than the box: scaled up to fill it, aspect kept (nearest pixel)
	const double scale = std::min((double)maxWide / w, (double)maxTall / h);
	const int dw = std::max(1, std::min(maxWide, (int)(w * scale + 0.5))),
	          dh = std::max(1, std::min(maxTall, (int)(h * scale + 0.5)));
	rgba.assign((size_t)dw * (size_t)dh * 4, 0);
	for (int y = 0; y < dh; y++)
	{
		const int sy = std::min(h - 1, (int)((long long)y * h / dh));
		for (int x = 0; x < dw; x++)
		{
			const int sx = std::min(w - 1, (int)((long long)x * w / dw));
			memcpy(&rgba[((size_t)y * dw + x) * 4], &full[((size_t)sy * w + sx) * 4], 4);
		}
	}
	wide = dw;
	tall = dh;
	return true;
}
