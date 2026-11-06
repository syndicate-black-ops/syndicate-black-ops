//=========================================================
// SBO 2.0: server menus' model preview: the hardware renderer's backer and frame cut (server_menu_preview_gl.h,
// docs/SERVER_MENUS.md "Model preview"). Each preview model (a card's, the big one) is drawn inside its own frame:
// a scissor at the frame, the frame cleared to the backer color, its depth cleared to the backer's plane.
//
// The GL functions are looked up in the current context, as env_mirror does (mirror_gl.cpp), so no GL header or
// import library is needed:
//   Windows: wglGetCurrentContext, the GL 1.1 functions from opengl32.dll (the engine has it loaded; looked up with
//            GetModuleHandle, not linked)
//   Linux:   SDL_GL_GetCurrentContext / SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp)
//=========================================================

#include "server_menu_preview_gl.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define PGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define PGL_APIENTRY
#endif

namespace
{
typedef unsigned int PGLenum;
typedef unsigned char PGLboolean;
typedef unsigned int PGLbitfield;
typedef int PGLint;
typedef int PGLsizei;
typedef float PGLfloat;
typedef double PGLdouble;

const PGLenum PGL_DEPTH_RANGE = 0x0B70;
const PGLenum PGL_DEPTH_WRITEMASK = 0x0B72;
const PGLenum PGL_DEPTH_CLEAR_VALUE = 0x0B73;
const PGLenum PGL_VIEWPORT = 0x0BA2;
const PGLenum PGL_PROJECTION_MATRIX = 0x0BA7;
const PGLenum PGL_SCISSOR_BOX = 0x0C10;
const PGLenum PGL_SCISSOR_TEST = 0x0C11;
const PGLenum PGL_COLOR_CLEAR_VALUE = 0x0C22;
const PGLenum PGL_COLOR_WRITEMASK = 0x0C23;
const PGLbitfield PGL_DEPTH_BUFFER_BIT = 0x00000100;
const PGLbitfield PGL_COLOR_BUFFER_BIT = 0x00004000;
const PGLbitfield PGL_CURRENT_BIT = 0x00000001;
const PGLbitfield PGL_ENABLE_BIT = 0x00002000;
const PGLbitfield PGL_TEXTURE_BIT = 0x00040000;
const PGLbitfield PGL_TRANSFORM_BIT = 0x00001000;
const PGLenum PGL_TEXTURE_2D = 0x0DE1;
const PGLenum PGL_DEPTH_TEST = 0x0B71;
const PGLenum PGL_BLEND = 0x0BE2;
const PGLenum PGL_ALPHA_TEST = 0x0BC0;
const PGLenum PGL_CULL_FACE = 0x0B44;
const PGLenum PGL_RGB = 0x1907;
const PGLenum PGL_TEXTURE_MIN_FILTER = 0x2801;
const PGLenum PGL_TEXTURE_MAG_FILTER = 0x2800;
const PGLenum PGL_TEXTURE_WRAP_S = 0x2802;
const PGLenum PGL_TEXTURE_WRAP_T = 0x2803;
const PGLint PGL_LINEAR = 0x2601;
const PGLint PGL_CLAMP_TO_EDGE = 0x812F;
const PGLenum PGL_MODELVIEW = 0x1700;
const PGLenum PGL_PROJECTION = 0x1701;
const PGLenum PGL_QUADS = 0x0007;
const PGLenum PGL_TEXTURE_ENV = 0x2300;
const PGLenum PGL_TEXTURE_ENV_MODE = 0x2200;
const PGLint PGL_REPLACE = 0x1E01;
typedef unsigned int PGLuint;
const PGLenum PGL_READ_FRAMEBUFFER = 0x8CA8;
const PGLenum PGL_DRAW_FRAMEBUFFER = 0x8CA9;
const PGLenum PGL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const PGLenum PGL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const PGLenum PGL_SAMPLE_BUFFERS = 0x80A8;
const PGLenum PGL_COLOR_ATTACHMENT0 = 0x8CE0;
const PGLenum PGL_UNSIGNED_BYTE = 0x1401;
const PGLenum PGL_NEAREST = 0x2600;

struct
{
	void (PGL_APIENTRY *Enable)(PGLenum cap);
	void (PGL_APIENTRY *Disable)(PGLenum cap);
	PGLboolean (PGL_APIENTRY *IsEnabled)(PGLenum cap);
	void (PGL_APIENTRY *GetIntegerv)(PGLenum pname, PGLint *params);
	void (PGL_APIENTRY *GetFloatv)(PGLenum pname, PGLfloat *params);
	void (PGL_APIENTRY *GetBooleanv)(PGLenum pname, PGLboolean *params);
	void (PGL_APIENTRY *Scissor)(PGLint x, PGLint y, PGLsizei w, PGLsizei h);
	void (PGL_APIENTRY *ClearColor)(PGLfloat r, PGLfloat g, PGLfloat b, PGLfloat a);
	void (PGL_APIENTRY *ClearDepth)(PGLdouble depth);
	void (PGL_APIENTRY *Clear)(PGLbitfield mask);
	void (PGL_APIENTRY *DepthMask)(PGLboolean flag);
	void (PGL_APIENTRY *ColorMask)(PGLboolean r, PGLboolean g, PGLboolean b, PGLboolean a);
	// the card copies
	void (PGL_APIENTRY *GenTextures)(PGLsizei n, PGLuint *textures);
	void (PGL_APIENTRY *BindTexture)(PGLenum target, PGLuint texture);
	void (PGL_APIENTRY *TexParameteri)(PGLenum target, PGLenum pname, PGLint param);
	void (PGL_APIENTRY *TexEnvi)(PGLenum target, PGLenum pname, PGLint param);
	void (PGL_APIENTRY *CopyTexSubImage2D)(PGLenum target, PGLint level, PGLint xoffset, PGLint yoffset, PGLint x,
		PGLint y, PGLsizei width, PGLsizei height);
	void (PGL_APIENTRY *PushAttrib)(PGLbitfield mask);
	void (PGL_APIENTRY *PopAttrib)(void);
	void (PGL_APIENTRY *MatrixMode)(PGLenum mode);
	void (PGL_APIENTRY *PushMatrix)(void);
	void (PGL_APIENTRY *PopMatrix)(void);
	void (PGL_APIENTRY *LoadIdentity)(void);
	void (PGL_APIENTRY *Ortho)(PGLdouble l, PGLdouble r, PGLdouble b, PGLdouble t, PGLdouble n, PGLdouble f);
	void (PGL_APIENTRY *Begin)(PGLenum mode);
	void (PGL_APIENTRY *End)(void);
	void (PGL_APIENTRY *TexCoord2f)(PGLfloat s, PGLfloat t);
	void (PGL_APIENTRY *Vertex2f)(PGLfloat x, PGLfloat y);
	void (PGL_APIENTRY *Color4f)(PGLfloat r, PGLfloat g, PGLfloat b, PGLfloat a);
	void (PGL_APIENTRY *TexImage2D)(PGLenum target, PGLint level, PGLint internalformat, PGLsizei width,
		PGLsizei height, PGLint border, PGLenum format, PGLenum type, const void *pixels);
	// GL 3.0 framebuffers (optional): the engine may draw the 3D view into its own framebuffer (HL25), which may
	// be multisampled; the copy is read from it (resolved through a framebuffer of our own when multisampled)
	void (PGL_APIENTRY *BindFramebuffer)(PGLenum target, PGLuint framebuffer);
	void (PGL_APIENTRY *GenFramebuffers)(PGLsizei n, PGLuint *framebuffers);
	void (PGL_APIENTRY *FramebufferTexture2D)(PGLenum target, PGLenum attachment, PGLenum textarget, PGLuint texture,
		PGLint level);
	void (PGL_APIENTRY *BlitFramebuffer)(PGLint sx0, PGLint sy0, PGLint sx1, PGLint sy1, PGLint dx0, PGLint dy0,
		PGLint dx1, PGLint dy1, PGLbitfield mask, PGLenum filter);
	bool fbo;
} qgl;
PGLuint s_ResolveFbo = 0;

// the frames' copies (ServerMenuPreviewGL_End / DrawCopy)
struct Copy
{
	PGLuint texture = 0;
	int texW = 0, texH = 0; // the texture's storage (allocated again only when the frame's size changes)
	bool valid = false;
	int x = 0, y = 0, w = 0, h = 0; // window pixels, from the bottom left (the 3D view's viewport)
	int vpw = 0, vph = 0;
	float stamp = -1000.0f;
};
Copy s_Copies[SERVER_MENU_PREVIEW_SLOTS];
int s_iActiveX = 0, s_iActiveY = 0, s_iActiveW = 0, s_iActiveH = 0, s_iActiveVpW = 0, s_iActiveVpH = 0;

void *s_pContext = NULL; // the context the pointers were looked up in
bool s_bAvailable = false;

// ServerMenuPreviewGL_Begin / End
bool s_bActive = false;
PGLboolean s_bScissorWasOn = 0;
PGLint s_SavedScissor[4];

void *CurrentContext()
{
#ifdef _WIN32
	typedef HGLRC(WINAPI * pfnGetCurrentContext)(void);
	static pfnGetCurrentContext s_pfn = NULL;
	if (!s_pfn)
	{
		HMODULE gl = GetModuleHandleA("opengl32.dll");
		s_pfn = gl ? (pfnGetCurrentContext)GetProcAddress(gl, "wglGetCurrentContext") : NULL;
		if (!s_pfn)
			return NULL;
	}
	return (void *)s_pfn();
#else
	return (void *)SDL_GL_GetCurrentContext();
#endif
}

void *GetProc(const char *name)
{
#ifdef _WIN32
	// GL 1.1 functions are exported by opengl32.dll itself; wglGetProcAddress knows the newer ones (as mirror_gl.cpp)
	HMODULE gl = GetModuleHandleA("opengl32.dll");
	if (!gl)
		return NULL;
	FARPROC p = GetProcAddress(gl, name);
	if (!p)
	{
		typedef PROC(WINAPI * pfnGetProcAddress)(LPCSTR);
		pfnGetProcAddress wglGet = (pfnGetProcAddress)GetProcAddress(gl, "wglGetProcAddress");
		PROC q = wglGet ? wglGet(name) : NULL;
		if (q == (PROC)1 || q == (PROC)2 || q == (PROC)3 || q == (PROC)-1)
			q = NULL;
		p = (FARPROC)q;
	}
	return (void *)p;
#else
	return SDL_GL_GetProcAddress(name);
#endif
}

template <typename T> bool Resolve(T &fn, const char *name)
{
	fn = (T)GetProc(name);
	return fn != NULL;
}
} // namespace

bool ServerMenuPreviewGL_Available()
{
	void *context = CurrentContext();
	if (!context)
		return false;
	if (context != s_pContext)
	{
		s_pContext = context;
		s_bAvailable =
			Resolve(qgl.Enable, "glEnable") &&
			Resolve(qgl.Disable, "glDisable") &&
			Resolve(qgl.IsEnabled, "glIsEnabled") &&
			Resolve(qgl.GetIntegerv, "glGetIntegerv") &&
			Resolve(qgl.GetFloatv, "glGetFloatv") &&
			Resolve(qgl.GetBooleanv, "glGetBooleanv") &&
			Resolve(qgl.Scissor, "glScissor") &&
			Resolve(qgl.ClearColor, "glClearColor") &&
			Resolve(qgl.ClearDepth, "glClearDepth") &&
			Resolve(qgl.Clear, "glClear") &&
			Resolve(qgl.DepthMask, "glDepthMask") &&
			Resolve(qgl.ColorMask, "glColorMask") &&
			Resolve(qgl.GenTextures, "glGenTextures") &&
			Resolve(qgl.BindTexture, "glBindTexture") &&
			Resolve(qgl.TexParameteri, "glTexParameteri") &&
			Resolve(qgl.TexEnvi, "glTexEnvi") &&
			Resolve(qgl.CopyTexSubImage2D, "glCopyTexSubImage2D") &&
			Resolve(qgl.PushAttrib, "glPushAttrib") &&
			Resolve(qgl.PopAttrib, "glPopAttrib") &&
			Resolve(qgl.MatrixMode, "glMatrixMode") &&
			Resolve(qgl.PushMatrix, "glPushMatrix") &&
			Resolve(qgl.PopMatrix, "glPopMatrix") &&
			Resolve(qgl.LoadIdentity, "glLoadIdentity") &&
			Resolve(qgl.Ortho, "glOrtho") &&
			Resolve(qgl.Begin, "glBegin") &&
			Resolve(qgl.End, "glEnd") &&
			Resolve(qgl.TexCoord2f, "glTexCoord2f") &&
			Resolve(qgl.Vertex2f, "glVertex2f") &&
			Resolve(qgl.Color4f, "glColor4f") &&
			Resolve(qgl.TexImage2D, "glTexImage2D");
		qgl.fbo = Resolve(qgl.BindFramebuffer, "glBindFramebuffer") && Resolve(qgl.GenFramebuffers, "glGenFramebuffers") &&
		          Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2D") &&
		          Resolve(qgl.BlitFramebuffer, "glBlitFramebuffer");
		s_ResolveFbo = 0;
		// a new context: the old textures are gone with it
		for (Copy &c : s_Copies)
			c = Copy();
	}
	return s_bAvailable;
}

bool ServerMenuPreviewGL_Begin(float x0, float y0, float x1, float y1, const unsigned char rgb[3], float backerDist)
{
	s_bActive = false;
	if (!ServerMenuPreviewGL_Available())
		return false;

	// the frame in window pixels: the 3D view's viewport covers the screen the window's fractions are of
	PGLint vp[4] = { 0, 0, 0, 0 };
	qgl.GetIntegerv(PGL_VIEWPORT, vp);
	if (vp[2] <= 0 || vp[3] <= 0)
		return false;
	const int px0 = vp[0] + (int)(x0 * vp[2] + 0.5f), px1 = vp[0] + (int)(x1 * vp[2] + 0.5f);
	const int py0 = vp[1] + (int)((1.0f - y1) * vp[3] + 0.5f), py1 = vp[1] + (int)((1.0f - y0) * vp[3] + 0.5f);
	if (px1 <= px0 || py1 <= py0)
		return false;
	s_iActiveX = px0;
	s_iActiveY = py0;
	s_iActiveW = px1 - px0;
	s_iActiveH = py1 - py0;
	s_iActiveVpW = vp[0] + vp[2];
	s_iActiveVpH = vp[1] + vp[3];

	// the backer plane's window depth through this frame's projection and depth range (gl_ztrick turns the range)
	PGLfloat p[16], range[2] = { 0.0f, 1.0f };
	qgl.GetFloatv(PGL_PROJECTION_MATRIX, p);
	qgl.GetFloatv(PGL_DEPTH_RANGE, range);
	const float ze = -backerDist;
	const float cz = p[10] * ze + p[14], cw = p[11] * ze + p[15];
	if (cw <= 0.0f)
		return false;
	float depth = range[0] + (range[1] - range[0]) * (cz / cw + 1.0f) * 0.5f;
	depth = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth);

	s_bScissorWasOn = qgl.IsEnabled(PGL_SCISSOR_TEST);
	qgl.GetIntegerv(PGL_SCISSOR_BOX, s_SavedScissor);
	PGLfloat clearColor[4], clearDepth = 1.0f;
	PGLboolean depthMask = 1, colorMask[4] = { 1, 1, 1, 1 };
	qgl.GetFloatv(PGL_COLOR_CLEAR_VALUE, clearColor);
	qgl.GetFloatv(PGL_DEPTH_CLEAR_VALUE, &clearDepth);
	qgl.GetBooleanv(PGL_DEPTH_WRITEMASK, &depthMask);
	qgl.GetBooleanv(PGL_COLOR_WRITEMASK, colorMask);

	qgl.Enable(PGL_SCISSOR_TEST);
	qgl.Scissor(px0, py0, px1 - px0, py1 - py0);
	qgl.DepthMask(1);
	qgl.ColorMask(1, 1, 1, 1);
	qgl.ClearColor(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, 1.0f);
	qgl.ClearDepth(depth);
	qgl.Clear(PGL_COLOR_BUFFER_BIT | PGL_DEPTH_BUFFER_BIT);

	qgl.ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
	qgl.ClearDepth(clearDepth);
	qgl.DepthMask(depthMask);
	qgl.ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	s_bActive = true;
	return true;
}

void ServerMenuPreviewGL_End(int slot, float stamp)
{
	if (!s_bActive)
		return;
	s_bActive = false;
	qgl.Scissor(s_SavedScissor[0], s_SavedScissor[1], s_SavedScissor[2], s_SavedScissor[3]);
	if (!s_bScissorWasOn)
		qgl.Disable(PGL_SCISSOR_TEST);
	if (slot < 0 || slot >= SERVER_MENU_PREVIEW_SLOTS)
		return;

	// the frame's pixels into the slot's texture (the bound texture as it was)
	Copy &c = s_Copies[slot];
	qgl.PushAttrib(PGL_TEXTURE_BIT);
	if (!c.texture)
	{
		qgl.GenTextures(1, &c.texture);
		c.texW = c.texH = 0;
	}
	qgl.BindTexture(PGL_TEXTURE_2D, c.texture);
	// the storage once per size (every frame copies up to 25 frames: no new allocation each time)
	const bool fresh = c.texW != s_iActiveW || c.texH != s_iActiveH;
	if (fresh)
	{
		qgl.TexParameteri(PGL_TEXTURE_2D, PGL_TEXTURE_MIN_FILTER, PGL_LINEAR);
		qgl.TexParameteri(PGL_TEXTURE_2D, PGL_TEXTURE_MAG_FILTER, PGL_LINEAR);
		qgl.TexParameteri(PGL_TEXTURE_2D, PGL_TEXTURE_WRAP_S, PGL_CLAMP_TO_EDGE);
		qgl.TexParameteri(PGL_TEXTURE_2D, PGL_TEXTURE_WRAP_T, PGL_CLAMP_TO_EDGE);
		qgl.TexImage2D(PGL_TEXTURE_2D, 0, PGL_RGB, s_iActiveW, s_iActiveH, 0, PGL_RGB, PGL_UNSIGNED_BYTE, NULL);
		c.texW = s_iActiveW;
		c.texH = s_iActiveH;
	}
	PGLint drawFbo = 0, readFbo = 0, multisampled = 0;
	if (qgl.fbo)
	{
		qgl.GetIntegerv(PGL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		qgl.GetIntegerv(PGL_READ_FRAMEBUFFER_BINDING, &readFbo);
		qgl.GetIntegerv(PGL_SAMPLE_BUFFERS, &multisampled);
	}
	if (qgl.fbo && multisampled)
	{
		// a multisampled 3D view: resolved into the texture through our framebuffer
		if (!s_ResolveFbo)
			qgl.GenFramebuffers(1, &s_ResolveFbo);
		qgl.BindFramebuffer(PGL_READ_FRAMEBUFFER, (PGLuint)drawFbo);
		qgl.BindFramebuffer(PGL_DRAW_FRAMEBUFFER, s_ResolveFbo);
		qgl.FramebufferTexture2D(PGL_DRAW_FRAMEBUFFER, PGL_COLOR_ATTACHMENT0, PGL_TEXTURE_2D, c.texture, 0);
		qgl.BlitFramebuffer(s_iActiveX, s_iActiveY, s_iActiveX + s_iActiveW, s_iActiveY + s_iActiveH, 0, 0, s_iActiveW,
			s_iActiveH, PGL_COLOR_BUFFER_BIT, PGL_NEAREST);
		qgl.BindFramebuffer(PGL_DRAW_FRAMEBUFFER, (PGLuint)drawFbo);
		qgl.BindFramebuffer(PGL_READ_FRAMEBUFFER, (PGLuint)readFbo);
	}
	else
	{
		// read where the 3D view was drawn (the engine's framebuffer, or the window's)
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(PGL_READ_FRAMEBUFFER, (PGLuint)drawFbo);
		qgl.CopyTexSubImage2D(PGL_TEXTURE_2D, 0, 0, 0, s_iActiveX, s_iActiveY, s_iActiveW, s_iActiveH);
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(PGL_READ_FRAMEBUFFER, (PGLuint)readFbo);
	}
	qgl.PopAttrib();
	c.valid = true;
	c.x = s_iActiveX;
	c.y = s_iActiveY;
	c.w = s_iActiveW;
	c.h = s_iActiveH;
	c.vpw = s_iActiveVpW;
	c.vph = s_iActiveVpH;
	c.stamp = stamp;
}

bool ServerMenuPreviewGL_DrawCopy(int slot, float now, float x0f, float y0f, float x1f, float y1f)
{
	if (slot < 0 || slot >= SERVER_MENU_PREVIEW_SLOTS || !ServerMenuPreviewGL_Available())
		return false;
	const Copy &c = s_Copies[slot];
	if (!c.valid || !c.texture || c.w <= 0 || c.h <= 0 || now - c.stamp > 0.25f || now < c.stamp - 0.01f)
		return false;
	// the same pixels as Begin computes them (the viewport starts at 0 0 for the 3D view)
	const int px0 = (int)(x0f * c.vpw + 0.5f), px1 = (int)(x1f * c.vpw + 0.5f);
	const int py0 = (int)((1.0f - y1f) * c.vph + 0.5f), py1 = (int)((1.0f - y0f) * c.vph + 0.5f);
	if (abs(px0 - c.x) > 1 || abs(py0 - c.y) > 1 || abs(px1 - (c.x + c.w)) > 1 || abs(py1 - (c.y + c.h)) > 1)
		return false;

	// at the window pixels it came from, in its own projection; everything as it was afterwards (VGUI2's own state,
	// its bound texture)
	qgl.PushAttrib(PGL_ENABLE_BIT | PGL_TEXTURE_BIT | PGL_CURRENT_BIT | PGL_COLOR_BUFFER_BIT | PGL_TRANSFORM_BIT);
	qgl.MatrixMode(PGL_PROJECTION);
	qgl.PushMatrix();
	qgl.LoadIdentity();
	qgl.Ortho(0.0, (PGLdouble)c.vpw, 0.0, (PGLdouble)c.vph, -1.0, 1.0);
	qgl.MatrixMode(PGL_MODELVIEW);
	qgl.PushMatrix();
	qgl.LoadIdentity();
	qgl.Disable(PGL_DEPTH_TEST);
	qgl.Disable(PGL_BLEND);
	qgl.Disable(PGL_ALPHA_TEST);
	qgl.Disable(PGL_CULL_FACE);
	qgl.Disable(PGL_SCISSOR_TEST);
	qgl.Enable(PGL_TEXTURE_2D);
	qgl.BindTexture(PGL_TEXTURE_2D, c.texture);
	qgl.TexEnvi(PGL_TEXTURE_ENV, PGL_TEXTURE_ENV_MODE, PGL_REPLACE);
	qgl.Color4f(1.0f, 1.0f, 1.0f, 1.0f);
	const float x0 = (float)c.x, y0 = (float)c.y, x1 = (float)(c.x + c.w), y1 = (float)(c.y + c.h);
	qgl.Begin(PGL_QUADS);
	qgl.TexCoord2f(0.0f, 0.0f); qgl.Vertex2f(x0, y0);
	qgl.TexCoord2f(1.0f, 0.0f); qgl.Vertex2f(x1, y0);
	qgl.TexCoord2f(1.0f, 1.0f); qgl.Vertex2f(x1, y1);
	qgl.TexCoord2f(0.0f, 1.0f); qgl.Vertex2f(x0, y1);
	qgl.End();
	qgl.MatrixMode(PGL_PROJECTION);
	qgl.PopMatrix();
	qgl.MatrixMode(PGL_MODELVIEW);
	qgl.PopMatrix();
	qgl.PopAttrib();
	return true;
}
