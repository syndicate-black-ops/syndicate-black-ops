//========= SBO world-space VGUI2 screens =========//
//
// Purpose: The OpenGL part of the world screens (see worldscreen.h): per screen slot a texture + framebuffer
//          object to render into, and the world-space quad. Talks to OpenGL directly, through entry points looked
//          up at run time in the engine's GL context: Windows wglGetProcAddress (opengl32.dll for the GL 1.1
//          functions), Linux SDL_GL_GetProcAddress (the engine's window and context are SDL2's). FBOs: GL 3.0 /
//          ARB_framebuffer_object, else EXT_framebuffer_object. Every function is a no-op without a GL context
//          (software renderer) or without the FBO functions, so the client never crashes on such a setup.
//
// GL state: no glPushAttrib. Each function reads the state it changes (one glGet / glIsEnabled per value), changes
// only that, and puts back exactly what it read (docs/WORLD_SCREENS.md, "GL state"): the engine caches the active
// texture unit, each unit's bound texture and which units texture, and its state at our call points varies (seen:
// texture unit 1 active in HUD_DrawNormalTriangles). With worldscreen_debug 1 the engine state is also captured at
// both points for worldscreen_status, and differences from the usual state are printed once.
//
// Plain C++ header (no GL types): included from both header worlds.
//===============================================================//

#ifndef SBO_WORLDSCREEN_GL_H
#define SBO_WORLDSCREEN_GL_H

namespace wsgl
{
//! Resolves the GL entry points once a GL context is current (call from a render/paint callback). Returns false
//! (and does nothing else) without a context or without FBO support. Also notices a new GL context (video mode
//! change): the targets of the old one are then created again when next used.
bool Init();
bool IsAvailable();

//! The 2D texture currently bound (GL_TEXTURE_BINDING_2D), 0 if unavailable. Render target setup only.
unsigned int GetBoundTexture2D();
//! Restores the 2D texture binding (after the engine's VGUI2 surface uploaded a texture for us).
void BindTexture2D(unsigned int tex);

//! Creates the render target of screen slot `slot` (0 .. WORLDSCREEN_MAX - 1): wide x tall RGBA texture + FBO.
//! texName != 0: use that existing GL texture name (allocated by the engine's VGUI2 surface, so it can't collide
//! with the engine's own texture numbering); 0: glGenTextures. Returns true when the FBO is complete.
bool CreateTarget(int slot, unsigned int texName, int wide, int tall);
//! The slot has a complete target in the current GL context (of that size, if wide > 0).
bool HasTarget(int slot, int wide = 0, int tall = 0);
//! Deletes the slot's FBO (and its texture, if glGenTextures made it).
void ResetTarget(int slot);

//! VGUI2 paint pass only. Binds the FBO as the draw framebuffer and sets glViewport(0, tall - screenTall,
//! screenWide, screenTall): with the engine's VGUI2 projection (an ortho over the VGUI2 screen), screen pixel
//! (0,0)-(wide,tall) lands exactly on the texture. Returns false (nothing changed) without a complete target.
bool BeginRender(int slot, int screenWide, int screenTall);
//! Puts back the engine's draw framebuffer and viewport (read in BeginRender). Marks the texture as having content.
void EndRender();
//! A movie screen's picture (worldscreen_video.cpp): wide x tall RGBA pixels, BOTTOM row first, into the slot's own
//! texture (made, or made again at a new size or in a new GL context; no FBO). Texture unit 0 and its binding are
//! put back. Marks the slot as having content. False without a GL context.
bool UploadPicture(int slot, const unsigned char *rgba, int wide, int tall);
//! A render of the slot has finished at least once since its target was created.
bool HasContent(int slot);

//! worldscreen_debug: records GL state from inside a VGUI2 Paint() while rendering into the FBO, for
//! worldscreen_status.
void CaptureInnerState();

//! 3D pass (HUD_DrawNormalTriangles). Draws the slot's texture on a world quad (corners: top-left, top-right,
//! bottom-right, bottom-left): opaque, both sides, depth tested, on texture unit 0 with unit 1 off; everything it
//! changes is put back.
void DrawWorldQuad(int slot, const float corners[4][3]);

//! One polygon of a bound screen (vgui_screen "surface"), in world coordinates: count corners (xyz, 3 floats each)
//! and their panel coordinates (uv, 2 floats each: u from the panel's left edge, v down from its top edge, 1 = the
//! whole panel). repeat: the coordinates leave 0..1 (the mapper's alignment tiles the panel), else clamped.
struct WorldPolygon
{
	const float *xyz;
	const float *uv;
	int count;
	bool repeat;
};
//! 3D pass: draws the slot's texture on the polygons (triangle fans), with the same state handling as
//! DrawWorldQuad. blank, or no content rendered yet: untextured black (an inactive bound screen: its faces are
//! invisible in the game, so something must be drawn there).
void DrawWorldPolygons(int slot, const WorldPolygon *polys, int count, bool blank);

//! worldscreen_debug: prints and clears pending GL errors (where = context string); returns the number of errors.
int CheckErrors(const char *where);

//! worldscreen_status: GL strings, entry points, target, and the state captured with worldscreen_debug.
void PrintStatus();
} // namespace wsgl

#endif // SBO_WORLDSCREEN_GL_H
