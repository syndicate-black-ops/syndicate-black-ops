//========= SBO world-space VGUI2 screens =========//
//
// Purpose: OpenGL render target + world quad for the world screens. See worldscreen_gl.h and docs/WORLD_SCREENS.md.
//
// Every GL function is called through a pointer resolved in the current context (Init), on both platforms: no GL
// headers or GL import library are needed, and nothing is called before a context exists.
//   Windows: wglGetCurrentContext / wglGetProcAddress; the GL 1.1 functions, which wglGetProcAddress does not
//            return, from opengl32.dll (linked, so it stays loaded; the engine loads it anyway).
//   Linux:   SDL_GL_GetCurrentContext / SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp); the engine's
//            window and GL context are SDL2's.
//
// Compiled with SBO's client flags (sbo_compat.h); no SBO or Source SDK headers.
//===============================================================//

#include "worldscreen_gl.h"
#include "worldscreen.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define WSGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define WSGL_APIENTRY
#endif

namespace
{
// GL types and the enums used here (GL 1.1, 1.3 multitexture, 3.0 / ARB / EXT framebuffer objects)
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef unsigned char GLubyte;

const GLenum GL_NO_ERROR = 0;
const GLenum GL_TRIANGLE_FAN = 0x0006;
const GLenum GL_QUADS = 0x0007;
const GLenum GL_CURRENT_COLOR = 0x0B00;
const GLenum GL_CULL_FACE = 0x0B44;
const GLenum GL_FOG = 0x0B60;
const GLenum GL_DEPTH_TEST = 0x0B71;
const GLenum GL_DEPTH_WRITEMASK = 0x0B72;
const GLenum GL_ALPHA_TEST = 0x0BC0;
const GLenum GL_BLEND = 0x0BE2;
const GLenum GL_VIEWPORT = 0x0BA2;
const GLenum GL_MODELVIEW_MATRIX = 0x0BA6;
const GLenum GL_PROJECTION_MATRIX = 0x0BA7;
const GLenum GL_SCISSOR_BOX = 0x0C10;
const GLenum GL_SCISSOR_TEST = 0x0C11;
const GLenum GL_TEXTURE_2D = 0x0DE1;
const GLenum GL_UNSIGNED_BYTE = 0x1401;
const GLenum GL_RGBA = 0x1908;
const GLenum GL_VENDOR = 0x1F00;
const GLenum GL_RENDERER = 0x1F01;
const GLenum GL_VERSION = 0x1F02;
const GLenum GL_TEXTURE_ENV_MODE = 0x2200;
const GLenum GL_TEXTURE_ENV = 0x2300;
const GLenum GL_LINEAR = 0x2601;
const GLenum GL_REPEAT = 0x2901;
const GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
const GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
const GLenum GL_TEXTURE_WRAP_S = 0x2802;
const GLenum GL_TEXTURE_WRAP_T = 0x2803;
const GLenum GL_RGBA8 = 0x8058;
const GLenum GL_UNPACK_ROW_LENGTH = 0x0CF2;
const GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
const GLenum GL_TEXTURE_BINDING_2D = 0x8069;
const GLenum GL_CLAMP_TO_EDGE = 0x812F;
const GLenum GL_TEXTURE0 = 0x84C0;
const GLenum GL_TEXTURE1 = 0x84C1;
const GLenum GL_ACTIVE_TEXTURE = 0x84E0;
const GLenum GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6; // == GL_FRAMEBUFFER_BINDING(_EXT)
const GLenum GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
const GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
const GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
const GLenum GL_FRAMEBUFFER = 0x8D40; // == GL_FRAMEBUFFER_EXT

// Entry points (qgl, as in Quake's and the engine's own GL code)
struct GlFuncs
{
	void(WSGL_APIENTRY *GetIntegerv)(GLenum pname, GLint *params);
	void(WSGL_APIENTRY *GetFloatv)(GLenum pname, GLfloat *params);
	GLboolean(WSGL_APIENTRY *IsEnabled)(GLenum cap);
	GLenum(WSGL_APIENTRY *GetError)(void);
	const GLubyte *(WSGL_APIENTRY *GetString)(GLenum name);
	void(WSGL_APIENTRY *Enable)(GLenum cap);
	void(WSGL_APIENTRY *Disable)(GLenum cap);
	void(WSGL_APIENTRY *Viewport)(GLint x, GLint y, GLsizei w, GLsizei h);
	void(WSGL_APIENTRY *GenTextures)(GLsizei n, GLuint *textures);
	void(WSGL_APIENTRY *DeleteTextures)(GLsizei n, const GLuint *textures);
	void(WSGL_APIENTRY *BindTexture)(GLenum target, GLuint texture);
	void(WSGL_APIENTRY *TexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h,
	    GLint border, GLenum format, GLenum type, const void *pixels);
	void(WSGL_APIENTRY *TexParameteri)(GLenum target, GLenum pname, GLint param);
	void(WSGL_APIENTRY *TexSubImage2D)(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h,
	    GLenum format, GLenum type, const void *pixels);
	void(WSGL_APIENTRY *PixelStorei)(GLenum pname, GLint param);
	void(WSGL_APIENTRY *GetTexEnviv)(GLenum target, GLenum pname, GLint *params);
	void(WSGL_APIENTRY *Begin)(GLenum mode);
	void(WSGL_APIENTRY *End)(void);
	void(WSGL_APIENTRY *TexCoord2f)(GLfloat s, GLfloat t);
	void(WSGL_APIENTRY *Vertex3fv)(const GLfloat *v);
	void(WSGL_APIENTRY *Color4f)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
	void(WSGL_APIENTRY *DepthMask)(GLboolean flag);
	// GL 1.3 / ARB_multitexture (optional)
	void(WSGL_APIENTRY *ActiveTexture)(GLenum texture);
	// framebuffer objects
	void(WSGL_APIENTRY *GenFramebuffers)(GLsizei n, GLuint *ids);
	void(WSGL_APIENTRY *DeleteFramebuffers)(GLsizei n, const GLuint *ids);
	void(WSGL_APIENTRY *BindFramebuffer)(GLenum target, GLuint id);
	void(WSGL_APIENTRY *FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
	    GLint level);
	GLenum(WSGL_APIENTRY *CheckFramebufferStatus)(GLenum target);
};
GlFuncs qgl;

bool s_bResolved = false;      // entry points looked up (for s_pContext)
bool s_bAvailable = false;     // GL 1.1 + FBO entry points present
bool s_bSplitBindings = false; // GL3/ARB: separate read/draw framebuffer bindings (we only bind the draw one)
GLenum s_FboTarget = GL_FRAMEBUFFER;
const char *s_pszFboFlavor = "none";
void *s_pContext = nullptr;

// One render target per screen slot
struct Target
{
	void *context = nullptr; // the GL context the names belong to
	GLuint tex = 0;
	GLuint fbo = 0;
	int wide = 0;
	int tall = 0;
	bool ok = false;
	bool hasContent = false;
	bool ownTexture = false; // glGenTextures (vs. engine-allocated name)
	GLenum status = 0;
};
Target s_Targets[WORLDSCREEN_MAX];
int s_iRenderSlot = -1;

Target *GetTarget(int slot)
{
	return (slot >= 0 && slot < WORLDSCREEN_MAX) ? &s_Targets[slot] : nullptr;
}

// BeginRender / EndRender
bool s_bInRender = false;
GLint s_PrevDrawFbo = 0;
GLint s_PrevViewport[4] = {};

// worldscreen_debug: engine state at our call points, for worldscreen_status and the expectation checks
struct CapturedState
{
	bool valid = false;
	GLint viewport[4] = {};
	GLfloat projection[16] = {};
	GLfloat modelview[16] = {};
	GLboolean scissor = 0;
	GLint scissorBox[4] = {};
	GLboolean depthTest = 0;
	GLboolean depthMask = 0;
	GLboolean blend = 0;
	GLboolean alphaTest = 0;
	GLboolean cullFace = 0;
	GLboolean fog = 0;
	GLboolean tex2D = 0;      // unit 0
	GLboolean tex2DUnit1 = 0; // lightmap unit
	GLint activeTexture = 0;
	GLint texEnvMode = 0;
	GLint drawFbo = 0;
	GLint readFbo = 0;
	GLint tex = 0;
	int screenWide = 0;
	int screenTall = 0;
};
CapturedState s_Vgui2; // at BeginRender (the engine's VGUI2 paint state)
CapturedState s_Inner; // inside a VGUI2 Paint() while the FBO is bound
CapturedState s_World; // at DrawWorldQuad (HUD_DrawNormalTriangles, after the triangle API render mode)
unsigned int s_iWarned = 0; // expectation mismatches already printed (bits)

void *CurrentContext()
{
#ifdef _WIN32
	return (void *)wglGetCurrentContext();
#else
	return (void *)SDL_GL_GetCurrentContext();
#endif
}

void *GetProc(const char *name)
{
#ifdef _WIN32
	PROC p = wglGetProcAddress(name);
	// some drivers return small integers instead of NULL
	if (p == nullptr || p == (PROC)1 || p == (PROC)2 || p == (PROC)3 || p == (PROC)-1)
	{
		// GL 1.1 functions are exported by opengl32.dll itself
		HMODULE gl = GetModuleHandleA("opengl32.dll");
		p = gl ? GetProcAddress(gl, name) : nullptr;
	}
	return (void *)p;
#else
	return SDL_GL_GetProcAddress(name);
#endif
}

template <typename T> bool Resolve(T &fn, const char *name)
{
	fn = (T)GetProc(name);
	return fn != nullptr;
}

void Capture(CapturedState &st)
{
	st.valid = true;
	qgl.GetIntegerv(GL_VIEWPORT, st.viewport);
	qgl.GetFloatv(GL_PROJECTION_MATRIX, st.projection);
	qgl.GetFloatv(GL_MODELVIEW_MATRIX, st.modelview);
	st.scissor = qgl.IsEnabled(GL_SCISSOR_TEST);
	qgl.GetIntegerv(GL_SCISSOR_BOX, st.scissorBox);
	st.depthTest = qgl.IsEnabled(GL_DEPTH_TEST);
	GLint depthMask = 0;
	qgl.GetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
	st.depthMask = depthMask != 0;
	st.blend = qgl.IsEnabled(GL_BLEND);
	st.alphaTest = qgl.IsEnabled(GL_ALPHA_TEST);
	st.cullFace = qgl.IsEnabled(GL_CULL_FACE);
	st.fog = qgl.IsEnabled(GL_FOG);
	st.activeTexture = GL_TEXTURE0;
	if (qgl.ActiveTexture)
	{
		qgl.GetIntegerv(GL_ACTIVE_TEXTURE, &st.activeTexture);
		qgl.ActiveTexture(GL_TEXTURE1);
		st.tex2DUnit1 = qgl.IsEnabled(GL_TEXTURE_2D);
		qgl.ActiveTexture(GL_TEXTURE0);
	}
	st.tex2D = qgl.IsEnabled(GL_TEXTURE_2D);
	qgl.GetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &st.texEnvMode);
	qgl.GetIntegerv(GL_TEXTURE_BINDING_2D, &st.tex);
	if (qgl.ActiveTexture)
		qgl.ActiveTexture((GLenum)st.activeTexture);
	qgl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &st.drawFbo);
	st.readFbo = st.drawFbo;
	if (s_bSplitBindings)
		qgl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &st.readFbo);
}

void PrintCaptured(const char *title, const CapturedState &st)
{
	if (!st.valid)
	{
		worldscreen::Printf("  %s: (not captured; worldscreen_debug 1)\n", title);
		return;
	}
	worldscreen::Printf("  %s: viewport %d %d %d %d, scissor %d box %d %d %d %d, fbo draw %d read %d\n", title,
	    st.viewport[0], st.viewport[1], st.viewport[2], st.viewport[3], st.scissor, st.scissorBox[0], st.scissorBox[1],
	    st.scissorBox[2], st.scissorBox[3], st.drawFbo, st.readFbo);
	worldscreen::Printf("    depth test %d mask %d, blend %d, alpha test %d, cull %d, fog %d, texture unit 0x%X, "
	                    "2D on unit 0 %d unit 1 %d, tex %d, texenv 0x%X\n",
	    st.depthTest, st.depthMask, st.blend, st.alphaTest, st.cullFace, st.fog, st.activeTexture, st.tex2D,
	    st.tex2DUnit1, st.tex, st.texEnvMode);
	if (st.screenWide)
		worldscreen::Printf("    vgui2 screen size %d x %d\n", st.screenWide, st.screenTall);
	const GLfloat *p = st.projection;
	worldscreen::Printf("    projection [%g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g]\n", p[0], p[1], p[2],
	    p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
	const GLfloat *m = st.modelview;
	worldscreen::Printf("    modelview  [%g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g]\n", m[0], m[1], m[2],
	    m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
}

// worldscreen_debug: the engine state differs from the usual one (handled; printed once per kind, for diagnosis).
void Expect(bool ok, unsigned int bit, const char *what)
{
	if (ok || (s_iWarned & bit))
		return;
	s_iWarned |= bit;
	worldscreen::DebugPrintf("worldscreen: unexpected engine GL state: %s (see worldscreen_status)\n", what);
}

const char *ErrorName(GLenum e)
{
	switch (e)
	{
	case 0x0500: return "GL_INVALID_ENUM";
	case 0x0501: return "GL_INVALID_VALUE";
	case 0x0502: return "GL_INVALID_OPERATION";
	case 0x0503: return "GL_STACK_OVERFLOW";
	case 0x0504: return "GL_STACK_UNDERFLOW";
	case 0x0505: return "GL_OUT_OF_MEMORY";
	case 0x0506: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	default: return "?";
	}
}

// The draw framebuffer the engine has bound right now (0 = window; HL25 with MSAA: its own multisampled FBO).
GLint CurrentDrawFbo()
{
	GLint fbo = 0;
	qgl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
	return fbo;
}
} // namespace

namespace wsgl
{
bool Init()
{
	void *ctx = CurrentContext();
	if (!ctx)
		return false; // not inside a GL callback, or the software renderer

	if (s_bResolved && ctx == s_pContext)
		return s_bAvailable;

	if (s_bResolved && worldscreen::DebugLevel() > 0)
		worldscreen::Printf("worldscreen: new GL context (%p -> %p), recreating the render target\n", s_pContext, ctx);

	// the targets of the old context are created again when next used (Target::context)
	s_pContext = ctx;
	s_bResolved = true;

	bool gl11 = Resolve(qgl.GetIntegerv, "glGetIntegerv");
	gl11 &= Resolve(qgl.GetFloatv, "glGetFloatv");
	gl11 &= Resolve(qgl.IsEnabled, "glIsEnabled");
	gl11 &= Resolve(qgl.GetError, "glGetError");
	gl11 &= Resolve(qgl.GetString, "glGetString");
	gl11 &= Resolve(qgl.Enable, "glEnable");
	gl11 &= Resolve(qgl.Disable, "glDisable");
	gl11 &= Resolve(qgl.Viewport, "glViewport");
	gl11 &= Resolve(qgl.GenTextures, "glGenTextures");
	gl11 &= Resolve(qgl.DeleteTextures, "glDeleteTextures");
	gl11 &= Resolve(qgl.BindTexture, "glBindTexture");
	gl11 &= Resolve(qgl.TexImage2D, "glTexImage2D");
	gl11 &= Resolve(qgl.TexParameteri, "glTexParameteri");
	gl11 &= Resolve(qgl.TexSubImage2D, "glTexSubImage2D");
	gl11 &= Resolve(qgl.PixelStorei, "glPixelStorei");
	gl11 &= Resolve(qgl.GetTexEnviv, "glGetTexEnviv");
	gl11 &= Resolve(qgl.Begin, "glBegin");
	gl11 &= Resolve(qgl.End, "glEnd");
	gl11 &= Resolve(qgl.TexCoord2f, "glTexCoord2f");
	gl11 &= Resolve(qgl.Vertex3fv, "glVertex3fv");
	gl11 &= Resolve(qgl.Color4f, "glColor4f");
	gl11 &= Resolve(qgl.DepthMask, "glDepthMask");

	if (!Resolve(qgl.ActiveTexture, "glActiveTexture"))
		Resolve(qgl.ActiveTexture, "glActiveTextureARB");

	// GL 3.0 / ARB_framebuffer_object first (separate read/draw bindings), then EXT_framebuffer_object.
	bool fbo = Resolve(qgl.GenFramebuffers, "glGenFramebuffers");
	fbo &= Resolve(qgl.DeleteFramebuffers, "glDeleteFramebuffers");
	fbo &= Resolve(qgl.BindFramebuffer, "glBindFramebuffer");
	fbo &= Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2D");
	fbo &= Resolve(qgl.CheckFramebufferStatus, "glCheckFramebufferStatus");
	s_bSplitBindings = true;
	s_FboTarget = GL_DRAW_FRAMEBUFFER;
	s_pszFboFlavor = "GL3/ARB_framebuffer_object";
	if (!fbo)
	{
		fbo = Resolve(qgl.GenFramebuffers, "glGenFramebuffersEXT");
		fbo &= Resolve(qgl.DeleteFramebuffers, "glDeleteFramebuffersEXT");
		fbo &= Resolve(qgl.BindFramebuffer, "glBindFramebufferEXT");
		fbo &= Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2DEXT");
		fbo &= Resolve(qgl.CheckFramebufferStatus, "glCheckFramebufferStatusEXT");
		s_bSplitBindings = false;
		s_FboTarget = GL_FRAMEBUFFER;
		s_pszFboFlavor = "EXT_framebuffer_object";
	}
	if (!fbo)
		s_pszFboFlavor = "none";

	s_bAvailable = gl11 && fbo;
	if (worldscreen::DebugLevel() > 0 || !gl11)
		worldscreen::Printf("worldscreen: GL context %p, GL 1.1 entry points %s, FBO support: %s\n", ctx,
		    gl11 ? "ok" : "MISSING", s_pszFboFlavor);
	return s_bAvailable;
}

bool IsAvailable()
{
	return s_bResolved && s_bAvailable;
}

unsigned int GetBoundTexture2D()
{
	if (!Init())
		return 0;
	GLint tex = 0;
	qgl.GetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
	return (unsigned int)tex;
}

void BindTexture2D(unsigned int tex)
{
	if (Init())
		qgl.BindTexture(GL_TEXTURE_2D, tex);
}

bool CreateTarget(int slot, unsigned int texName, int wide, int tall)
{
	Target *t = GetTarget(slot);
	if (!t || !Init() || s_bInRender)
		return false;

	ResetTarget(slot);
	t->context = s_pContext;

	// Setup only (once per GL context): the current bindings are read and put back.
	GLint prevTex = 0;
	qgl.GetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
	const GLint prevFbo = CurrentDrawFbo();

	t->ownTexture = (texName == 0);
	if (t->ownTexture)
		qgl.GenTextures(1, &t->tex);
	else
		t->tex = texName;
	t->wide = wide;
	t->tall = tall;

	// (Re)define the storage: RGBA8, linear, no mipmaps, clamped.
	qgl.BindTexture(GL_TEXTURE_2D, t->tex);
	qgl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, wide, tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qgl.BindTexture(GL_TEXTURE_2D, (GLuint)prevTex);

	qgl.GenFramebuffers(1, &t->fbo);
	qgl.BindFramebuffer(s_FboTarget, t->fbo);
	qgl.FramebufferTexture2D(s_FboTarget, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->tex, 0);
	t->status = qgl.CheckFramebufferStatus(s_FboTarget);
	qgl.BindFramebuffer(s_FboTarget, (GLuint)prevFbo);

	t->ok = (t->status == GL_FRAMEBUFFER_COMPLETE);
	if (worldscreen::DebugLevel() > 0)
		CheckErrors("CreateTarget");
	if (worldscreen::DebugLevel() > 0 || !t->ok)
		worldscreen::Printf("worldscreen: screen %d render target tex %u (%s) fbo %u %dx%d, status 0x%04X%s\n", slot,
		    t->tex, t->ownTexture ? "glGenTextures" : "engine VGUI2 surface", t->fbo, wide, tall, t->status,
		    t->ok ? " (complete)" : " (INCOMPLETE: this screen is not drawn)");
	return t->ok;
}

bool UploadPicture(int slot, const unsigned char *rgba, int wide, int tall)
{
	Target *t = GetTarget(slot);
	if (!t || !rgba || wide <= 0 || tall <= 0 || !Init() || s_bInRender)
		return false;

	// texture unit 0, its binding read and put back (the engine caches both)
	GLint prevActive = GL_TEXTURE0;
	if (qgl.ActiveTexture)
	{
		qgl.GetIntegerv(GL_ACTIVE_TEXTURE, &prevActive);
		if (prevActive != (GLint)GL_TEXTURE0)
			qgl.ActiveTexture(GL_TEXTURE0);
	}
	GLint prevTex = 0;
	qgl.GetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

	if (!(t->ok && t->context == s_pContext && t->wide == wide && t->tall == tall && !t->fbo))
	{
		ResetTarget(slot);
		t->context = s_pContext;
		t->ownTexture = true;
		qgl.GenTextures(1, &t->tex);
		t->wide = wide;
		t->tall = tall;
		qgl.BindTexture(GL_TEXTURE_2D, t->tex);
		qgl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, wide, tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		t->ok = true;
		t->status = GL_FRAMEBUFFER_COMPLETE; // no FBO: the texture is filled directly
		if (worldscreen::DebugLevel() > 0)
			worldscreen::Printf("worldscreen: screen %d movie texture %u %dx%d\n", slot, t->tex, wide, tall);
	}
	else
		qgl.BindTexture(GL_TEXTURE_2D, t->tex);

	GLint align = 4, rowLength = 0;
	qgl.GetIntegerv(GL_UNPACK_ALIGNMENT, &align);
	qgl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
	qgl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
	qgl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	// bottom row first: the picture's top row lands at t = 1, as a VGUI2 render puts it (the draw functions expect
	// that); one upload
	qgl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, wide, tall, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	qgl.PixelStorei(GL_UNPACK_ALIGNMENT, align);
	qgl.PixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);

	qgl.BindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
	if (qgl.ActiveTexture && prevActive != (GLint)GL_TEXTURE0)
		qgl.ActiveTexture((GLenum)prevActive);
	t->hasContent = true;
	if (worldscreen::DebugLevel() > 1)
		CheckErrors("UploadPicture");
	return true;
}

bool HasTarget(int slot, int wide, int tall)
{
	const Target *t = GetTarget(slot);
	return t && t->ok && t->context == s_pContext && s_pContext == CurrentContext() &&
	    (wide <= 0 || (t->wide == wide && t->tall == tall));
}

void ResetTarget(int slot)
{
	Target *t = GetTarget(slot);
	if (!t)
		return;
	// The names belong to a context that may be gone (vid mode change): only delete them in the current one.
	if (t->context && t->context == s_pContext && s_pContext == CurrentContext() && s_bAvailable)
	{
		if (t->fbo)
			qgl.DeleteFramebuffers(1, &t->fbo);
		if (t->tex && t->ownTexture)
			qgl.DeleteTextures(1, &t->tex);
	}
	*t = Target();
}

bool BeginRender(int slot, int screenWide, int screenTall)
{
	if (s_bInRender || !HasTarget(slot))
		return false;
	const Target *t = GetTarget(slot);

	if (worldscreen::DebugLevel() > 0)
	{
		CheckErrors("before BeginRender (engine)");
		Capture(s_Vgui2);
		s_Vgui2.screenWide = screenWide;
		s_Vgui2.screenTall = screenTall;
		// The VGUI2 surface clips in software; GL scissoring would cut our render.
		Expect(!s_Vgui2.scissor, 1, "scissor test on in the VGUI2 paint pass");
	}

	// Changed here, put back exactly in EndRender: the draw framebuffer (with MSAA, HL25 draws into its own FBO,
	// whose name only GL knows) and the viewport. Read once per render.
	s_PrevDrawFbo = CurrentDrawFbo();
	qgl.GetIntegerv(GL_VIEWPORT, s_PrevViewport);
	qgl.BindFramebuffer(s_FboTarget, t->fbo);
	qgl.Viewport(0, t->tall - screenTall, screenWide, screenTall);

	s_bInRender = true;
	s_iRenderSlot = slot;
	return true;
}

void EndRender()
{
	if (!s_bInRender)
		return;
	s_bInRender = false;

	qgl.BindFramebuffer(s_FboTarget, (GLuint)s_PrevDrawFbo);
	qgl.Viewport(s_PrevViewport[0], s_PrevViewport[1], s_PrevViewport[2], s_PrevViewport[3]);

	s_Targets[s_iRenderSlot].hasContent = true;
	s_iRenderSlot = -1;
	if (worldscreen::DebugLevel() > 0)
		CheckErrors("EndRender");
}

bool HasContent(int slot)
{
	return HasTarget(slot) && GetTarget(slot)->hasContent;
}

void CaptureInnerState()
{
	if (s_bInRender)
		Capture(s_Inner);
}

namespace
{
// The engine state DrawBegin changes for a world draw, read before and put back by DrawEnd
struct WorldDrawState
{
	GLint prevActive = GL_TEXTURE0;
	GLboolean unit1Tex = 0;
	GLboolean unit0Tex = 0;
	GLint prevTex = 0;
	GLboolean cull = 0;
	GLboolean blend = 0;
	GLboolean alphaTest = 0;
	GLint depthMask = 1;
	GLfloat color[4] = { 1, 1, 1, 1 };
};

// False (nothing changed): no content to draw yet. blank: untextured black instead of the slot's texture (needs no
// content).
bool DrawBegin(int slot, WorldDrawState &st, bool blank = false)
{
	if (s_bInRender || (blank ? !(IsAvailable() && s_pContext == CurrentContext()) : !HasContent(slot)))
		return false;

	if (worldscreen::DebugLevel() > 0)
	{
		Capture(s_World);
		Expect(s_World.activeTexture == (GLint)GL_TEXTURE0, 4, "texture unit 0 not active in HUD_DrawNormalTriangles");
		Expect(s_World.tex2D, 8, "2D texturing off on unit 0 in HUD_DrawNormalTriangles");
		Expect(!s_World.tex2DUnit1, 16, "2D texturing on on unit 1 in HUD_DrawNormalTriangles");
		Expect(s_World.depthTest, 32, "depth test off in HUD_DrawNormalTriangles");
	}

	// Everything changed here is read first and put back exactly: the engine caches the active texture unit, the
	// texture bound on each unit and which units texture (GL_SelectTexture, GL_Bind, GL_EnableMultitexture), and it
	// does not always arrive here with unit 0 active and the lightmap unit off (seen: unit 1 active).
	if (qgl.ActiveTexture)
	{
		qgl.GetIntegerv(GL_ACTIVE_TEXTURE, &st.prevActive);
		// the lightmap unit would modulate the quad with whatever lightmap is bound: off for the quad
		qgl.ActiveTexture(GL_TEXTURE1);
		st.unit1Tex = qgl.IsEnabled(GL_TEXTURE_2D);
		if (st.unit1Tex)
			qgl.Disable(GL_TEXTURE_2D);
		qgl.ActiveTexture(GL_TEXTURE0);
	}
	st.unit0Tex = qgl.IsEnabled(GL_TEXTURE_2D);
	qgl.GetIntegerv(GL_TEXTURE_BINDING_2D, &st.prevTex);
	st.cull = qgl.IsEnabled(GL_CULL_FACE);
	st.blend = qgl.IsEnabled(GL_BLEND);
	st.alphaTest = qgl.IsEnabled(GL_ALPHA_TEST);
	qgl.GetIntegerv(GL_DEPTH_WRITEMASK, &st.depthMask);
	qgl.GetFloatv(GL_CURRENT_COLOR, st.color);

	// opaque, both sides, depth tested (the engine's depth test is on in this pass) and depth writing
	if (blank)
	{
		if (st.unit0Tex)
			qgl.Disable(GL_TEXTURE_2D);
	}
	else if (!st.unit0Tex)
		qgl.Enable(GL_TEXTURE_2D);
	if (st.cull)
		qgl.Disable(GL_CULL_FACE);
	if (st.blend)
		qgl.Disable(GL_BLEND);
	if (st.alphaTest)
		qgl.Disable(GL_ALPHA_TEST);
	if (!st.depthMask)
		qgl.DepthMask(1);
	if (blank)
	{
		qgl.Color4f(0, 0, 0, 1);
		return true;
	}
	qgl.BindTexture(GL_TEXTURE_2D, GetTarget(slot)->tex);
	// Texture object state, not the engine's: the engine created the texture with mipmapped minification, but only
	// level 0 is rendered (farther away the stale lower levels were sampled: dark/bright with distance).
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// White: the texture as it is with the engine's texture environment (replace or modulate).
	qgl.Color4f(1, 1, 1, 1);
	return true;
}

// Our texture's wrapping (texture object state, ours): clamped for a panel shown once, repeated for a bound face
// whose alignment tiles it.
void SetWrap(bool repeat)
{
	const GLint mode = repeat ? (GLint)GL_REPEAT : (GLint)GL_CLAMP_TO_EDGE;
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mode);
	qgl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mode);
}

void DrawEnd(const WorldDrawState &st, const char *where)
{
	// back to exactly what the engine had
	qgl.Color4f(st.color[0], st.color[1], st.color[2], st.color[3]);
	qgl.BindTexture(GL_TEXTURE_2D, (GLuint)st.prevTex);
	if (!st.depthMask)
		qgl.DepthMask(0);
	if (st.alphaTest)
		qgl.Enable(GL_ALPHA_TEST);
	if (st.blend)
		qgl.Enable(GL_BLEND);
	if (st.cull)
		qgl.Enable(GL_CULL_FACE);
	if (st.unit0Tex != qgl.IsEnabled(GL_TEXTURE_2D))
	{
		if (st.unit0Tex)
			qgl.Enable(GL_TEXTURE_2D);
		else
			qgl.Disable(GL_TEXTURE_2D);
	}
	if (qgl.ActiveTexture)
	{
		if (st.unit1Tex)
		{
			qgl.ActiveTexture(GL_TEXTURE1);
			qgl.Enable(GL_TEXTURE_2D);
		}
		qgl.ActiveTexture((GLenum)st.prevActive);
	}

	if (worldscreen::DebugLevel() > 1)
		CheckErrors(where);
}
} // namespace

void DrawWorldQuad(int slot, const float c[4][3])
{
	WorldDrawState st;
	if (!DrawBegin(slot, st))
		return;
	SetWrap(false);

	// The VGUI2 render puts the panel's top row at the top of the FBO, i.e. t = 1.
	const float tTop = 1.0f;
	const float tBottom = 0.0f;
	qgl.Begin(GL_QUADS);
	qgl.TexCoord2f(0, tTop);
	qgl.Vertex3fv(c[0]);
	qgl.TexCoord2f(1, tTop);
	qgl.Vertex3fv(c[1]);
	qgl.TexCoord2f(1, tBottom);
	qgl.Vertex3fv(c[2]);
	qgl.TexCoord2f(0, tBottom);
	qgl.Vertex3fv(c[3]);
	qgl.End();

	DrawEnd(st, "DrawWorldQuad");
}

void DrawWorldPolygons(int slot, const WorldPolygon *polys, int count, bool blank)
{
	WorldDrawState st;
	if (count <= 0)
		return;
	if (!blank && !HasContent(slot))
		blank = true; // not rendered yet: black rather than a hole where the invisible faces are
	if (!DrawBegin(slot, st, blank))
		return;

	int wrap = -1; // unknown: set by the first polygon
	for (int i = 0; i < count; i++)
	{
		const WorldPolygon &p = polys[i];
		if (p.count < 3)
			continue;
		if (!blank && wrap != (int)p.repeat)
		{
			wrap = (int)p.repeat;
			SetWrap(p.repeat);
		}
		qgl.Begin(GL_TRIANGLE_FAN);
		for (int k = 0; k < p.count; k++)
		{
			// panel v runs down from the top row; the FBO has the top row at t = 1
			qgl.TexCoord2f(p.uv[k * 2], 1.0f - p.uv[k * 2 + 1]);
			qgl.Vertex3fv(p.xyz + k * 3);
		}
		qgl.End();
	}

	DrawEnd(st, "DrawWorldPolygons");
}

int CheckErrors(const char *where)
{
	if (!Init())
		return 0;
	int n = 0;
	GLenum e;
	while ((e = qgl.GetError()) != GL_NO_ERROR && n < 16)
	{
		worldscreen::Printf("worldscreen: GL error 0x%04X %s (%s)\n", e, ErrorName(e), where);
		n++;
	}
	return n;
}

void PrintStatus()
{
	worldscreen::Printf("worldscreen status:\n");
	if (!CurrentContext() || !s_bResolved || s_pContext != CurrentContext() || !qgl.GetString)
	{
		worldscreen::Printf("  (no GL context current right now, or not resolved yet; showing cached values)\n");
	}
	else
	{
		const char *ven = (const char *)qgl.GetString(GL_VENDOR);
		const char *ren = (const char *)qgl.GetString(GL_RENDERER);
		const char *ver = (const char *)qgl.GetString(GL_VERSION);
		worldscreen::Printf("  GL_VENDOR %s\n  GL_RENDERER %s\n  GL_VERSION %s\n", ven ? ven : "?", ren ? ren : "?",
		    ver ? ver : "?");
	}
	worldscreen::Printf("  context %p, FBO entry points: %s, glActiveTexture %s\n", s_pContext, s_pszFboFlavor,
	    qgl.ActiveTexture ? "yes" : "no");
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		const Target &t = s_Targets[i];
		if (!t.context)
			continue;
		worldscreen::Printf("  screen %d target: tex %u (%s) fbo %u %dx%d status 0x%04X ok %d, has content %d%s\n", i,
		    t.tex, t.ownTexture ? "glGenTextures" : "engine VGUI2 surface", t.fbo, t.wide, t.tall, t.status, t.ok,
		    t.hasContent, t.context == s_pContext ? "" : " (old GL context)");
	}
	PrintCaptured("engine state at BeginRender (VGUI2 paint)", s_Vgui2);
	PrintCaptured("state inside the VGUI2 Paint()", s_Inner);
	PrintCaptured("engine state at DrawWorldQuad (HUD_DrawNormalTriangles)", s_World);
}
} // namespace wsgl
