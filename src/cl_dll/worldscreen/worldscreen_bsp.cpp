//========= SBO world-space VGUI2 screens =========//
//
// Purpose: Reads the {screen faces of the map's brush models from its .bsp file (see worldscreen_bsp.h).
//
// BSP version 30 (bspfile.h of the Half-Life tools): lumps PLANES, TEXTURES, VERTEXES, TEXINFO, FACES, EDGES,
// SURFEDGES, MODELS. Every offset, count and index is checked against the file's size: a broken or hostile map can't
// make the client read outside the buffer. The structures are read with memcpy (no alignment assumed).
//
// SBO world: gEngfuncs.COM_LoadFile (the engine's file system: the mod folder, its fallbacks and
// syndicate_downloads), pfnGetLevelName.
//===============================================================//

// the standard headers first: cl_util.h defines min / max macros
#include <string.h>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"

#include "vgui_screen_shared.h"
#include "worldscreen.h"
#include "worldscreen_bsp.h"

namespace
{
const int BSP_VERSION = 30;
enum
{
	LUMP_PLANES = 1,
	LUMP_TEXTURES = 2,
	LUMP_VERTEXES = 3,
	LUMP_TEXINFO = 6,
	LUMP_FACES = 7,
	LUMP_EDGES = 12,
	LUMP_SURFEDGES = 13,
	LUMP_MODELS = 14,
	NUM_LUMPS = 15
};
// record sizes in the file
const int PLANE_SIZE = 20;    // float normal[3], float dist, int type
const int VERTEX_SIZE = 12;   // float point[3]
const int TEXINFO_SIZE = 40;  // float vecs[2][4], int miptex, int flags
const int FACE_SIZE = 20;     // short planenum, short side, int firstedge, short numedges, short texinfo,
                              // byte styles[4], int lightofs
const int EDGE_SIZE = 4;      // unsigned short v[2]
const int MODEL_SIZE = 64;    // float mins[3], maxs[3], origin[3], int headnode[4], int visleafs, firstface, numfaces
const int MIPTEX_HEADER = 40; // char name[16], unsigned width, height, offsets[4]

const int MAX_FACE_VERTS = 64; // a face with more is skipped (the compilers stay far below)

struct Lump
{
	int ofs = 0, len = 0;
};

class Reader
{
public:
	Reader(const unsigned char *data, int size) : m_data(data), m_size(size) {}

	bool Ok(int ofs, int len) const { return ofs >= 0 && len >= 0 && ofs <= m_size && len <= m_size - ofs; }
	int Int(int ofs) const
	{
		int v;
		memcpy(&v, m_data + ofs, 4);
		return v;
	}
	short Short(int ofs) const
	{
		short v;
		memcpy(&v, m_data + ofs, 2);
		return v;
	}
	unsigned short UShort(int ofs) const
	{
		unsigned short v;
		memcpy(&v, m_data + ofs, 2);
		return v;
	}
	float Float(int ofs) const
	{
		float v;
		memcpy(&v, m_data + ofs, 4);
		return v;
	}
	const unsigned char *Ptr(int ofs) const { return m_data + ofs; }

private:
	const unsigned char *m_data;
	int m_size;
};

bool IsToolTexture(const unsigned char *name16)
{
	const char *want = WSCREEN_TOOL_TEXTURE;
	int i = 0;
	for (; i < 16 && want[i]; i++)
	{
		char c = (char)name16[i];
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		if (c != want[i])
			return false;
	}
	return i < 16 ? name16[i] == 0 : true;
}

// The current map's tool texture faces, per submodel
std::string s_MapPath;
bool s_bLoaded = false;
bool s_bFailed = false;
std::vector<std::vector<wsbsp::Face>> s_Faces;

bool Parse(const Reader &r, int size, const char *path)
{
	if (size < 4 + NUM_LUMPS * 8 || r.Int(0) != BSP_VERSION)
	{
		worldscreen::Printf("worldscreen: %s is not a BSP version %d file, bound screens are not shown\n", path,
		    BSP_VERSION);
		return false;
	}
	Lump l[NUM_LUMPS];
	for (int i = 0; i < NUM_LUMPS; i++)
	{
		l[i].ofs = r.Int(4 + i * 8);
		l[i].len = r.Int(8 + i * 8);
		if (!r.Ok(l[i].ofs, l[i].len))
		{
			worldscreen::Printf("worldscreen: %s: lump %d outside the file, bound screens are not shown\n", path, i);
			return false;
		}
	}

	const int numPlanes = l[LUMP_PLANES].len / PLANE_SIZE;
	const int numVerts = l[LUMP_VERTEXES].len / VERTEX_SIZE;
	const int numTexinfo = l[LUMP_TEXINFO].len / TEXINFO_SIZE;
	const int numFaces = l[LUMP_FACES].len / FACE_SIZE;
	const int numEdgesTotal = l[LUMP_EDGES].len / EDGE_SIZE;
	const int numSurfedges = l[LUMP_SURFEDGES].len / 4;
	const int numModels = l[LUMP_MODELS].len / MODEL_SIZE;

	// The textures: which miptex indices are the tool texture (and their sizes)
	const Lump &tl = l[LUMP_TEXTURES];
	int numMiptex = 0;
	if (tl.len >= 4)
		numMiptex = r.Int(tl.ofs);
	if (numMiptex < 0 || numMiptex > (tl.len - 4) / 4)
		numMiptex = 0;
	std::vector<int> toolWide(numMiptex, 0), toolTall(numMiptex, 0);
	bool anyTool = false;
	for (int i = 0; i < numMiptex; i++)
	{
		const int rel = r.Int(tl.ofs + 4 + i * 4);
		if (rel < 0 || rel > tl.len - MIPTEX_HEADER)
			continue; // -1: a texture the compiler did not find
		const int ofs = tl.ofs + rel;
		if (!IsToolTexture(r.Ptr(ofs)))
			continue;
		const int w = r.Int(ofs + 16), h = r.Int(ofs + 20);
		if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
			continue;
		toolWide[i] = w;
		toolTall[i] = h;
		anyTool = true;
	}

	s_Faces.assign(numModels > 0 ? numModels : 0, std::vector<wsbsp::Face>());
	if (!anyTool)
		return true; // the map uses no {screen: no bound screen can show (the caller says so per screen)

	int skipped = 0;
	for (int m = 1; m < numModels; m++) // 0 is the world: not a carrier
	{
		const int mofs = l[LUMP_MODELS].ofs + m * MODEL_SIZE;
		const int firstFace = r.Int(mofs + 56), count = r.Int(mofs + 60);
		if (firstFace < 0 || count < 0 || firstFace > numFaces || count > numFaces - firstFace)
			continue;
		for (int f = firstFace; f < firstFace + count; f++)
		{
			const int fofs = l[LUMP_FACES].ofs + f * FACE_SIZE;
			const int planenum = (unsigned short)r.Short(fofs);
			const int side = r.Short(fofs + 2);
			const int firstEdge = r.Int(fofs + 4);
			const int numEdges = r.Short(fofs + 8);
			const int texinfo = r.Short(fofs + 10);
			if (texinfo < 0 || texinfo >= numTexinfo)
				continue;
			const int tofs = l[LUMP_TEXINFO].ofs + texinfo * TEXINFO_SIZE;
			const int miptex = r.Int(tofs + 32);
			if (miptex < 0 || miptex >= numMiptex || !toolWide[miptex])
				continue;
			if (planenum >= numPlanes || numEdges < 3 || numEdges > MAX_FACE_VERTS || firstEdge < 0 ||
			    firstEdge > numSurfedges - numEdges)
			{
				skipped++;
				continue;
			}

			wsbsp::Face face;
			const int pofs = l[LUMP_PLANES].ofs + planenum * PLANE_SIZE;
			const float flip = side ? -1.0f : 1.0f;
			for (int k = 0; k < 3; k++)
				face.normal[k] = r.Float(pofs + k * 4) * flip;
			face.dist = r.Float(pofs + 12) * flip;
			for (int a = 0; a < 2; a++)
			{
				for (int k = 0; k < 4; k++)
					face.vecs[a][k] = r.Float(tofs + (a * 4 + k) * 4);
			}
			face.texWide = toolWide[miptex];
			face.texTall = toolTall[miptex];

			// the corners: surfedge >= 0 = edge's first vertex, < 0 = the reversed edge's second one
			bool ok = true;
			face.xyz.reserve(numEdges * 3);
			for (int e = 0; e < numEdges && ok; e++)
			{
				const int se = r.Int(l[LUMP_SURFEDGES].ofs + (firstEdge + e) * 4);
				if (se <= -numEdgesTotal || se >= numEdgesTotal)
				{
					ok = false;
					break;
				}
				const int eofs = l[LUMP_EDGES].ofs + (se >= 0 ? se : -se) * EDGE_SIZE;
				const int v = se >= 0 ? r.UShort(eofs) : r.UShort(eofs + 2);
				if (v >= numVerts)
				{
					ok = false;
					break;
				}
				for (int k = 0; k < 3; k++)
					face.xyz.push_back(r.Float(l[LUMP_VERTEXES].ofs + v * VERTEX_SIZE + k * 4));
			}
			if (!ok)
			{
				skipped++;
				continue;
			}
			s_Faces[m].push_back(face);
		}
	}
	if (skipped)
		worldscreen::Printf("worldscreen: %s: %d %s faces skipped (bad data or more than %d corners)\n", path,
		    skipped, WSCREEN_TOOL_TEXTURE, MAX_FACE_VERTS);
	return true;
}
} // namespace

namespace wsbsp
{
void Reset()
{
	s_MapPath.clear();
	s_bLoaded = s_bFailed = false;
	s_Faces.clear();
}

bool GetFaces(int submodel, std::vector<Face> &out)
{
	out.clear();
	const char *level = gEngfuncs.pfnGetLevelName ? gEngfuncs.pfnGetLevelName() : nullptr;
	if (!level || !level[0])
		return false;

	if (s_MapPath != level)
	{
		s_MapPath = level;
		s_bLoaded = s_bFailed = false;
		s_Faces.clear();
	}
	if (!s_bLoaded && !s_bFailed)
	{
		int size = 0;
		unsigned char *data = gEngfuncs.COM_LoadFile((char *)level, 5, &size);
		if (!data)
		{
			worldscreen::Printf("worldscreen: can't read %s, bound screens are not shown\n", level);
			s_bFailed = true;
		}
		else
		{
			s_bLoaded = Parse(Reader(data, size), size, level);
			s_bFailed = !s_bLoaded;
			gEngfuncs.COM_FreeFile(data);
			if (worldscreen::DebugLevel() > 0 && s_bLoaded)
			{
				int n = 0;
				for (const auto &v : s_Faces)
					n += (int)v.size();
				worldscreen::Printf("worldscreen: %s: %d %s faces in %d brush models\n", level, n,
				    WSCREEN_TOOL_TEXTURE, (int)s_Faces.size());
			}
		}
	}
	if (!s_bLoaded)
		return false;
	if (submodel > 0 && submodel < (int)s_Faces.size())
		out = s_Faces[submodel];
	return true;
}
} // namespace wsbsp
