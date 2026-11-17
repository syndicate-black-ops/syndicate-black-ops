//=========================================================
// SBO 2.1 env_entity_glow: the faces of one brush model, read from the map's .bsp file (BSP version 30), for the
// glow of a brush entity (entity_glow.cpp). Plain C++, no engine: tests/entity_glow/run_test.sh runs it under
// ASan/UBSan with truncated and hostile files.
//
// Not from the engine's model_t / msurface_t: their layout differs between the software renderer, GL before HL25 and
// GL on HL25 (worldscreen_bsp.h). The file format has not changed.
//=========================================================

#ifndef SBO_CL_DLL_ENTITY_GLOW_BSP_H
#define SBO_CL_DLL_ENTITY_GLOW_BSP_H

#include <vector>

// The polygons of submodel `submodel` (1 .. the file's model count - 1; 0, the world, is refused) of the BSP file in
// data[0 .. size): xyz holds 3 floats a corner, counts the corners of each polygon in turn, in the model's own
// coordinates (relative to its origin brush, or world coordinates for a model without one). Every offset, count and
// index is checked against the file's size; a face with bad data or more than 64 corners is left out, and at most
// maxFaces faces are read. False (and nothing in the lists) when the data is not a BSP 30 file or the submodel is not
// in it.
bool EntityGlowBsp_ModelFaces( const unsigned char *data, int size, int submodel, int maxFaces,
	std::vector<float> &xyz, std::vector<int> &counts );

#endif // SBO_CL_DLL_ENTITY_GLOW_BSP_H
