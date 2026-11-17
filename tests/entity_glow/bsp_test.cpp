// env_entity_glow: EntityGlowBsp_ModelFaces on a BSP 30 file built here (tests/entity_glow/run_test.sh)
#include "entity_glow_bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static int s_iFailed = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); s_iFailed++; } } while (0)

namespace
{
void Put(std::vector<unsigned char> &b, int ofs, const void *p, int n)
{
	memcpy(b.data() + ofs, p, n);
}

// a file with 3 models: 0 the world (no faces), 1 a quad + a triangle, 2 one quad (reversed edges)
std::vector<unsigned char> MakeBsp()
{
	std::vector<float> verts = { 0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0, 5, 5, 5 };
	std::vector<unsigned short> edges = { 0, 0, 0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 4, 1 };	// edge 0 unused (as in the compilers)
	std::vector<int> surfedges = { 1, 2, 3, 4, 1, 6, -5, -4, -3, -2, -1 };
	struct Face { short plane, side; int firstedge; short numedges, texinfo; unsigned char styles[4]; int light; };
	Face faces[3] = { { 0, 0, 0, 4, 0, { 0 }, -1 }, { 0, 0, 4, 3, 0, { 0 }, -1 }, { 0, 0, 7, 4, 0, { 0 }, -1 } };
	struct Model { float mins[3], maxs[3], origin[3]; int head[4], visleafs, firstface, numfaces; };
	Model models[3] = {};
	models[1].firstface = 0;
	models[1].numfaces = 2;
	models[2].firstface = 2;
	models[2].numfaces = 1;

	const int header = 4 + 15 * 8;
	int ofs[15] = {}, len[15] = {};
	int at = header;
	auto lump = [&](int i, int n) { ofs[i] = at; len[i] = n; at += n; };
	lump(3, (int)(verts.size() * 4));
	lump(7, (int)sizeof(faces));
	lump(12, (int)(edges.size() * 2));
	lump(13, (int)(surfedges.size() * 4));
	lump(14, (int)sizeof(models));
	std::vector<unsigned char> b(at, 0);
	const int version = 30;
	Put(b, 0, &version, 4);
	for (int i = 0; i < 15; i++)
	{
		if (!len[i])
			ofs[i] = header;
		Put(b, 4 + i * 8, &ofs[i], 4);
		Put(b, 8 + i * 8, &len[i], 4);
	}
	Put(b, ofs[3], verts.data(), len[3]);
	Put(b, ofs[7], faces, len[7]);
	Put(b, ofs[12], edges.data(), len[12]);
	Put(b, ofs[13], surfedges.data(), len[13]);
	Put(b, ofs[14], models, len[14]);
	return b;
}
} // namespace

int main()
{
	std::vector<unsigned char> bsp = MakeBsp();
	std::vector<float> xyz;
	std::vector<int> counts;

	// the good file
	CHECK(EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), 1, 100, xyz, counts));
	CHECK(counts.size() == 2 && counts[0] == 4 && counts[1] == 3 && xyz.size() == 21);
	CHECK(xyz.size() >= 12 && xyz[3] == 10 && xyz[4] == 0 && xyz[9] == 0 && xyz[10] == 10);
	CHECK(xyz.size() == 21 && xyz[18] == 5 && xyz[20] == 5);	// the triangle's last corner: vertex 4 (edge 6's first)
	CHECK(EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), 2, 100, xyz, counts));
	CHECK(counts.size() == 1 && counts[0] == 4 && xyz[0] == 0 && xyz[4] == 10);	// -4: edge 4 (3, 0) reversed: vertex 0; -3: edge 3 (2, 3) reversed: vertex 3
	CHECK(EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), 1, 1, xyz, counts) && counts.size() == 1);

	// refused: the world, a model not in the file, no data, not BSP 30
	CHECK(!EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), 0, 100, xyz, counts) && counts.empty());
	CHECK(!EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), 3, 100, xyz, counts));
	CHECK(!EntityGlowBsp_ModelFaces(bsp.data(), (int)bsp.size(), -1, 100, xyz, counts));
	CHECK(!EntityGlowBsp_ModelFaces(NULL, 0, 1, 100, xyz, counts));
	{
		std::vector<unsigned char> b = bsp;
		b[0] = 29;
		CHECK(!EntityGlowBsp_ModelFaces(b.data(), (int)b.size(), 1, 100, xyz, counts));
	}

	// every truncation: never a read outside (ASan), never more than the good file
	for (size_t n = 0; n <= bsp.size(); n++)
	{
		std::vector<unsigned char> b(bsp.begin(), bsp.begin() + n);
		EntityGlowBsp_ModelFaces(b.empty() ? NULL : b.data(), (int)n, 1, 100, xyz, counts);
		CHECK(counts.size() <= 2);
	}

	// hostile values in every int / short slot of the file
	const int hostile[] = { -1, 0x7fffffff, (int)0x80000000, 0x7fff, 0x8000, 0xffff, 65, 1000000 };
	for (size_t at = 0; at + 4 <= bsp.size(); at += 2)
	{
		for (int h : hostile)
		{
			std::vector<unsigned char> b = bsp;
			memcpy(b.data() + at, &h, 4);
			for (int m = 1; m <= 2; m++)
			{
				EntityGlowBsp_ModelFaces(b.data(), (int)b.size(), m, 100, xyz, counts);
				size_t total = 0;
				for (int c : counts)
				{
					CHECK(c >= 3 && c <= 64);
					total += (size_t)c * 3;
				}
				CHECK(total == xyz.size());
			}
		}
	}

	// random byte changes
	srand(1);
	for (int i = 0; i < 20000; i++)
	{
		std::vector<unsigned char> b = bsp;
		for (int k = 0; k < 4; k++)
			b[rand() % b.size()] = (unsigned char)rand();
		EntityGlowBsp_ModelFaces(b.data(), (int)b.size(), 1 + rand() % 3, 100, xyz, counts);
	}

	printf(s_iFailed ? "entity_glow bsp: %d FAILED\n" : "entity_glow bsp: OK\n", s_iFailed);
	return s_iFailed ? 1 : 0;
}
