//=========================================================
// SBO 2.1 env_entity_glow: the faces of one brush model from the .bsp file (entity_glow_bsp.h). The layout and the
// checks follow worldscreen/worldscreen_bsp.cpp (bspfile.h of the Half-Life tools: lumps VERTEXES, FACES, EDGES,
// SURFEDGES, MODELS); the structures are read with memcpy (no alignment assumed).
//=========================================================

#include "entity_glow_bsp.h"

#include <string.h>

namespace
{
const int BSP_VERSION = 30;
enum
{
	LUMP_VERTEXES = 3,
	LUMP_FACES = 7,
	LUMP_EDGES = 12,
	LUMP_SURFEDGES = 13,
	LUMP_MODELS = 14,
	NUM_LUMPS = 15
};
const int VERTEX_SIZE = 12;   // float point[3]
const int FACE_SIZE = 20;     // short planenum, short side, int firstedge, short numedges, short texinfo, byte styles[4],
                              // int lightofs
const int EDGE_SIZE = 4;      // unsigned short v[2]
const int MODEL_SIZE = 64;    // float mins[3], maxs[3], origin[3], int headnode[4], int visleafs, firstface, numfaces
const int MAX_FACE_VERTS = 64;

int Int(const unsigned char *p)
{
	int v;
	memcpy(&v, p, 4);
	return v;
}
short Short(const unsigned char *p)
{
	short v;
	memcpy(&v, p, 2);
	return v;
}
unsigned short UShort(const unsigned char *p)
{
	unsigned short v;
	memcpy(&v, p, 2);
	return v;
}
float Float(const unsigned char *p)
{
	float v;
	memcpy(&v, p, 4);
	return v;
}
} // namespace

bool EntityGlowBsp_ModelFaces(const unsigned char *data, int size, int submodel, int maxFaces, std::vector<float> &xyz,
	std::vector<int> &counts)
{
	xyz.clear();
	counts.clear();
	if (!data || size < 4 + NUM_LUMPS * 8 || Int(data) != BSP_VERSION || submodel <= 0)
		return false;

	int ofs[NUM_LUMPS], len[NUM_LUMPS];
	for (int i = 0; i < NUM_LUMPS; i++)
	{
		ofs[i] = Int(data + 4 + i * 8);
		len[i] = Int(data + 8 + i * 8);
		if (ofs[i] < 0 || len[i] < 0 || ofs[i] > size || len[i] > size - ofs[i])
			return false;
	}

	const int numVerts = len[LUMP_VERTEXES] / VERTEX_SIZE;
	const int numFaces = len[LUMP_FACES] / FACE_SIZE;
	const int numEdges = len[LUMP_EDGES] / EDGE_SIZE;
	const int numSurfedges = len[LUMP_SURFEDGES] / 4;
	const int numModels = len[LUMP_MODELS] / MODEL_SIZE;
	if (submodel >= numModels)
		return false;

	const unsigned char *model = data + ofs[LUMP_MODELS] + submodel * MODEL_SIZE;
	const int firstFace = Int(model + 56), faceCount = Int(model + 60);
	if (firstFace < 0 || faceCount < 0 || firstFace > numFaces || faceCount > numFaces - firstFace)
		return false;

	int read = 0;
	for (int f = firstFace; f < firstFace + faceCount && read < maxFaces; f++)
	{
		const unsigned char *face = data + ofs[LUMP_FACES] + f * FACE_SIZE;
		const int firstEdge = Int(face + 4);
		const int corners = Short(face + 8);
		if (corners < 3 || corners > MAX_FACE_VERTS || firstEdge < 0 || firstEdge > numSurfedges - corners)
			continue;

		float poly[MAX_FACE_VERTS * 3];
		bool ok = true;
		for (int e = 0; e < corners; e++)
		{
			// surfedge >= 0: the edge's first vertex; < 0: the reversed edge's second one
			const int se = Int(data + ofs[LUMP_SURFEDGES] + (firstEdge + e) * 4);
			if (se <= -numEdges || se >= numEdges)
			{
				ok = false;
				break;
			}
			const unsigned char *edge = data + ofs[LUMP_EDGES] + (se >= 0 ? se : -se) * EDGE_SIZE;
			const int v = se >= 0 ? UShort(edge) : UShort(edge + 2);
			if (v >= numVerts)
			{
				ok = false;
				break;
			}
			for (int k = 0; k < 3; k++)
				poly[e * 3 + k] = Float(data + ofs[LUMP_VERTEXES] + v * VERTEX_SIZE + k * 4);
		}
		if (!ok)
			continue;
		xyz.insert(xyz.end(), poly, poly + corners * 3);
		counts.push_back(corners);
		read++;
	}
	return true;
}
