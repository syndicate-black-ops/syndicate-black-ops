//========= SBO VGUI2: image decoding for the server browser (map pictures, overviews) =========//
//
// Purpose: see sbo_image_decode.h.
//=================================================================//

#include <string.h>

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
