//========= SBO world-space VGUI2 screens =========//
//
// Purpose: The faces a bound world screen is drawn on (vgui_screen "surface", docs/WORLD_SCREENS.md "Bound
//          screens"): every face of a brush model ("*N") whose texture is the tool texture {screen
//          (WSCREEN_TOOL_TEXTURE), read from the map's .bsp file (BSP version 30) through the engine's file system.
//
// Not from the engine's model_t / msurface_t: their layout differs between the software renderer, GL before HL25
// and GL on HL25, and the SDK header declares only the software one. The file format has not changed.
//
// Plain C++ (std::vector); SBO world.
//===============================================================//

#ifndef SBO_WORLDSCREEN_BSP_H
#define SBO_WORLDSCREEN_BSP_H

#include <vector>

namespace wsbsp
{
// One {screen face, in the brush model's own coordinates (relative to its origin brush, or world coordinates for a
// model without one).
struct Face
{
	std::vector<float> xyz;  // the polygon, 3 floats a vertex, in the file's order
	float normal[3];         // front side (the plane flipped for the face's side)
	float dist;              // plane: dot(normal, p) == dist
	float vecs[2][4];        // texinfo: s = dot(p, vecs[0]) + vecs[0][3], t likewise
	int texWide, texTall;    // the texture's size: one repeat (s / texWide, t / texTall) = the whole panel
};

//! The {screen faces of submodel `submodel` of the current map (pfnGetLevelName). The file is read once per map
//! (on the first call) and only the tool texture faces of all its brush models are kept. False with a console line
//! when the file cannot be read or is not BSP 30; true with an empty list when the model has no such face.
bool GetFaces(int submodel, std::vector<Face> &out);
//! Forgets the faces read (WorldScreen_Reset, i.e. every map start: a map file changed under the same name is read
//! again).
void Reset();
} // namespace wsbsp

#endif // SBO_WORLDSCREEN_BSP_H
