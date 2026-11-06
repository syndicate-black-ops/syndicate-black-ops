// sbo_image_decode.cpp on real overview files, cut and damaged copies, and small hand-made images (run_test.sh)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "sbo_image_decode.h"

static std::vector<uint8_t> ReadFile(const char *path)
{
	std::vector<uint8_t> data;
	FILE *f = fopen(path, "rb");
	if (!f)
		return data;
	fseek(f, 0, SEEK_END);
	const long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n > 0)
	{
		data.resize((size_t)n);
		if (fread(data.data(), 1, (size_t)n, f) != (size_t)n)
			data.clear();
	}
	fclose(f);
	return data;
}

int main(int argc, char **argv)
{
	int failed = 0, decoded = 0;
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;

	// 2x2 uncompressed 24-bit, bottom row first: blue, green / red, white
	const uint8_t tiny[] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 24, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255 };
	if (!sbo_image::DecodeTgaScaled(tiny, sizeof(tiny), 16, 16, rgba, w, h) || w != 2 || h != 2 || rgba[0] != 255 || rgba[2] != 0
	    || rgba[8] != 0 || rgba[10] != 255 || rgba[13] != 255)
	{
		printf("FAIL: tiny image\n");
		failed++;
	}
	// cut short and too small
	if (sbo_image::DecodeTgaScaled(tiny, sizeof(tiny) - 1, 16, 16, rgba, w, h) || sbo_image::DecodeTgaScaled(tiny, 10, 16, 16, rgba, w, h))
	{
		printf("FAIL: cut tiny image accepted\n");
		failed++;
	}

	srand(4242);
	for (int i = 1; i < argc; i++)
	{
		std::vector<uint8_t> data = ReadFile(argv[i]);
		if (data.empty())
			continue;
		const size_t len = strlen(argv[i]);
		const bool jpeg = len > 4 && !strcmp(argv[i] + len - 4, ".jpg");
		auto decode = [jpeg](const uint8_t *d, size_t n, int mw, int mh, std::vector<uint8_t> &out, int &ow, int &oh) {
			return jpeg ? sbo_image::DecodeJpegScaled(d, n, mw, mh, out, ow, oh) : sbo_image::DecodeTgaScaled(d, n, mw, mh, out, ow, oh, true);
		};
		if (!decode(data.data(), data.size(), 512, 384, rgba, w, h))
		{
			printf("%s: not decoded: %s\n", jpeg ? "FAIL" : "note (not truecolor?)", argv[i]);
			failed += jpeg ? 1 : 0;
			continue;
		}
		decoded++;
		if (w > 512 || h > 384 || rgba.size() != (size_t)w * h * 4)
		{
			printf("FAIL: size %dx%d for %s\n", w, h, argv[i]);
			failed++;
		}
		// cut short at random places: must fail cleanly
		for (int k = 0; k < 4; k++)
		{
			const size_t cut = (size_t)rand() % data.size();
			decode(data.data(), cut, 512, 384, rgba, w, h);
		}
		// random bytes changed (header included): must not crash
		for (int k = 0; k < 4; k++)
		{
			std::vector<uint8_t> bad = data;
			for (int b = 0; b < 32; b++)
				bad[(size_t)rand() % (b < 8 ? 18 : bad.size())] = (uint8_t)rand();
			decode(bad.data(), bad.size(), 1 + rand() % 600, 1 + rand() % 600, rgba, w, h);
		}
	}
	if (decoded == 0)
	{
		printf("FAIL: no overview decoded\n");
		failed++;
	}
	if (failed)
	{
		printf("%d check(s) failed\n", failed);
		return 1;
	}
	printf("PASS (%d image files decoded)\n", decoded);
	return 0;
}
