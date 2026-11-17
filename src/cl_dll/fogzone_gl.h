//=========================================================
// SBO 2.0 env_fogzone: the per-pixel fog volume pass (fogzone_gl.cpp). docs/WEATHER.md "env_fogzone".
//=========================================================

#ifndef SBO_CL_DLL_FOGZONE_GL_H
#define SBO_CL_DLL_FOGZONE_GL_H

//! Starts the pass for the view being drawn: copies the view's depth (resolved when multisampled) and sets the GL
//! state the zones are drawn with. Every GL value it changes is read first and put back by FogZoneGL_End. False: the
//! pass can't run in this context (FogZoneGL_Why says why); nothing was changed and FogZoneGL_End must not be called.
bool FogZoneGL_Begin( const float *eye );

//! One zone's fog inside the box mins..maxs, color 0..1, the fog density (engine fog curve), scale 0..1 (how much of
//! it shows: 1 - the strength of the zone's fog the eye already has), open: the faces with open air behind them
//! (min x, y, z, max x, y, z), whose fog thins out toward the face; NULL: none. Between FogZoneGL_Begin and
//! FogZoneGL_End.
void FogZoneGL_Zone( const float *mins, const float *maxs, const float *color, float density, float scale,
	const bool *open );

//! Puts back every GL value FogZoneGL_Begin changed
void FogZoneGL_End( void );

//! SBO 2.1 env_entity_glow (entity_glow_gl.cpp): the view's depth inside viewport x, y, w, h copied into this pass's
//! depth texture (the same copy as FogZoneGL_Begin's, resolved when multisampled; not copied again when the fog zone
//! pass copied this view's depth already) and left bound on the ACTIVE texture unit; the caller saves and restores that unit's binding. 0: the copy can't run here (FogZoneGL_Why says why).
unsigned int FogZoneGL_CopyViewDepth( const int *viewport );
//! A new view is being drawn (tri.cpp HUD_DrawTransparentTriangles): the depth copy of the last one is not reused
void FogZoneGL_NewView( void );

//! Why the pass can't run (empty when it can)
const char *FogZoneGL_Why( void );
//! What the pass uses in this context, for the developer console line (depth bits, multisampled or not)
const char *FogZoneGL_Describe( void );
//! Bumped whenever a new GL context was found (the console line is printed again for it)
int FogZoneGL_Generation( void );

#endif // SBO_CL_DLL_FOGZONE_GL_H
