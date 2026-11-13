//=========================================================
// SBO 2.0 color_correction: the OpenGL pass (colorcorrection_gl.h, docs/COLOR_CORRECTION.md "How it is drawn").
//
// The 3D view inside the viewport is copied into a texture (resolved first when the engine draws into a multisampled
// framebuffer, as the server menus' model preview copies it: server_menu_preview_gl.cpp), then drawn back over itself
// through a GLSL 1.20 program that looks each pixel up in up to four 3D tables and mixes them by weight.
//
// The GL functions are looked up in the current context, as env_mirror and the model preview do, so no GL header or
// import library is needed:
//   Windows: wglGetCurrentContext, the GL 1.1 functions from opengl32.dll (the engine has it loaded; looked up with
//            GetModuleHandle, not linked), wglGetProcAddress for the rest
//   Linux:   SDL_GL_GetCurrentContext / SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp)
//
// No glPushAttrib: every value changed is read first and put back exactly (docs/WORLD_SCREENS.md "GL state"); the
// engine caches its active texture unit, the textures bound on each unit and its GLSL program.
//=========================================================

#include "colorcorrection_gl.h"
#include "colorcorrection_core.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define CGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define CGL_APIENTRY
#endif

namespace
{
typedef unsigned int CGLenum;
typedef unsigned char CGLboolean;
typedef unsigned int CGLbitfield;
typedef int CGLint;
typedef int CGLsizei;
typedef float CGLfloat;
typedef unsigned int CGLuint;
typedef char CGLchar;

const CGLenum CGL_DEPTH_TEST = 0x0B71;
const CGLenum CGL_DEPTH_WRITEMASK = 0x0B72;
const CGLenum CGL_STENCIL_TEST = 0x0B90;
const CGLenum CGL_VIEWPORT = 0x0BA2;
const CGLenum CGL_ALPHA_TEST = 0x0BC0;
const CGLenum CGL_BLEND = 0x0BE2;
const CGLenum CGL_SCISSOR_TEST = 0x0C11;
const CGLenum CGL_COLOR_WRITEMASK = 0x0C23;
const CGLenum CGL_CULL_FACE = 0x0B44;
const CGLenum CGL_POLYGON_MODE = 0x0B40;
const CGLenum CGL_UNPACK_ALIGNMENT = 0x0CF5;
const CGLenum CGL_PACK_ALIGNMENT = 0x0D05;
const CGLenum CGL_TEXTURE_2D = 0x0DE1;
const CGLenum CGL_TEXTURE_3D = 0x806F;
const CGLenum CGL_TEXTURE_BINDING_2D = 0x8069;
const CGLenum CGL_TEXTURE_BINDING_3D = 0x806A;
const CGLenum CGL_TEXTURE_MIN_FILTER = 0x2801;
const CGLenum CGL_TEXTURE_MAG_FILTER = 0x2800;
const CGLenum CGL_TEXTURE_WRAP_S = 0x2802;
const CGLenum CGL_TEXTURE_WRAP_T = 0x2803;
const CGLenum CGL_TEXTURE_WRAP_R = 0x8072;
const CGLint CGL_LINEAR = 0x2601;
const CGLint CGL_NEAREST = 0x2600;
const CGLint CGL_CLAMP_TO_EDGE = 0x812F;
const CGLenum CGL_RGB = 0x1907;
const CGLint CGL_RGB8 = 0x8051;
const CGLenum CGL_UNSIGNED_BYTE = 0x1401;
const CGLenum CGL_QUADS = 0x0007;
const CGLenum CGL_FRONT = 0x0404;
const CGLenum CGL_BACK = 0x0405;
const CGLenum CGL_FILL = 0x1B02;
const CGLbitfield CGL_COLOR_BUFFER_BIT = 0x00004000;
const CGLenum CGL_TEXTURE0 = 0x84C0;
const CGLenum CGL_ACTIVE_TEXTURE = 0x84E0;
const CGLenum CGL_FRAGMENT_SHADER = 0x8B30;
const CGLenum CGL_VERTEX_SHADER = 0x8B31;
const CGLenum CGL_COMPILE_STATUS = 0x8B81;
const CGLenum CGL_LINK_STATUS = 0x8B82;
const CGLenum CGL_CURRENT_PROGRAM = 0x8B8D;
const CGLenum CGL_PIXEL_UNPACK_BUFFER = 0x88EC;
const CGLenum CGL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;
const CGLenum CGL_PIXEL_PACK_BUFFER = 0x88EB;
const CGLenum CGL_PIXEL_PACK_BUFFER_BINDING = 0x88ED;
const CGLenum CGL_READ_FRAMEBUFFER = 0x8CA8;
const CGLenum CGL_DRAW_FRAMEBUFFER = 0x8CA9;
const CGLenum CGL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const CGLenum CGL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const CGLenum CGL_SAMPLE_BUFFERS = 0x80A8;
const CGLenum CGL_COLOR_ATTACHMENT0 = 0x8CE0;

struct
{
	// GL 1.1
	void (CGL_APIENTRY *Enable)(CGLenum cap);
	void (CGL_APIENTRY *Disable)(CGLenum cap);
	CGLboolean (CGL_APIENTRY *IsEnabled)(CGLenum cap);
	void (CGL_APIENTRY *GetIntegerv)(CGLenum pname, CGLint *params);
	void (CGL_APIENTRY *GetBooleanv)(CGLenum pname, CGLboolean *params);
	void (CGL_APIENTRY *Viewport)(CGLint x, CGLint y, CGLsizei w, CGLsizei h);
	void (CGL_APIENTRY *DepthMask)(CGLboolean flag);
	void (CGL_APIENTRY *ColorMask)(CGLboolean r, CGLboolean g, CGLboolean b, CGLboolean a);
	void (CGL_APIENTRY *PolygonMode)(CGLenum face, CGLenum mode);
	void (CGL_APIENTRY *PixelStorei)(CGLenum pname, CGLint param);
	void (CGL_APIENTRY *GenTextures)(CGLsizei n, CGLuint *textures);
	void (CGL_APIENTRY *DeleteTextures)(CGLsizei n, const CGLuint *textures);
	void (CGL_APIENTRY *BindTexture)(CGLenum target, CGLuint texture);
	void (CGL_APIENTRY *TexParameteri)(CGLenum target, CGLenum pname, CGLint param);
	void (CGL_APIENTRY *TexImage2D)(CGLenum target, CGLint level, CGLint internalformat, CGLsizei width,
		CGLsizei height, CGLint border, CGLenum format, CGLenum type, const void *pixels);
	void (CGL_APIENTRY *CopyTexSubImage2D)(CGLenum target, CGLint level, CGLint xoffset, CGLint yoffset, CGLint x,
		CGLint y, CGLsizei width, CGLsizei height);
	void (CGL_APIENTRY *Begin)(CGLenum mode);
	void (CGL_APIENTRY *End)(void);
	void (CGL_APIENTRY *TexCoord2f)(CGLfloat s, CGLfloat t);
	void (CGL_APIENTRY *Vertex2f)(CGLfloat x, CGLfloat y);
	// GL 1.2 / 1.3
	void (CGL_APIENTRY *TexImage3D)(CGLenum target, CGLint level, CGLint internalformat, CGLsizei width,
		CGLsizei height, CGLsizei depth, CGLint border, CGLenum format, CGLenum type, const void *pixels);
	void (CGL_APIENTRY *ActiveTexture)(CGLenum texture);
	// GL 1.5 (optional: only to make sure no pixel buffer is bound while uploading)
	void (CGL_APIENTRY *BindBuffer)(CGLenum target, CGLuint buffer);
	// GL 1.1, only with the debug checks (ColorCorrGL_SetDebug)
	CGLenum (CGL_APIENTRY *GetError)(void);
	// GL 1.1, only for the editor's copy of the view (ColorCorrGL_SetSnapshot)
	void (CGL_APIENTRY *GetTexImage)(CGLenum target, CGLint level, CGLenum format, CGLenum type, void *pixels);
	// GL 2.0
	CGLuint (CGL_APIENTRY *CreateShader)(CGLenum type);
	void (CGL_APIENTRY *ShaderSource)(CGLuint shader, CGLsizei count, const CGLchar *const *string, const CGLint *length);
	void (CGL_APIENTRY *CompileShader)(CGLuint shader);
	void (CGL_APIENTRY *GetShaderiv)(CGLuint shader, CGLenum pname, CGLint *params);
	void (CGL_APIENTRY *GetShaderInfoLog)(CGLuint shader, CGLsizei bufSize, CGLsizei *length, CGLchar *infoLog);
	void (CGL_APIENTRY *DeleteShader)(CGLuint shader);
	CGLuint (CGL_APIENTRY *CreateProgram)(void);
	void (CGL_APIENTRY *AttachShader)(CGLuint program, CGLuint shader);
	void (CGL_APIENTRY *LinkProgram)(CGLuint program);
	void (CGL_APIENTRY *GetProgramiv)(CGLuint program, CGLenum pname, CGLint *params);
	void (CGL_APIENTRY *GetProgramInfoLog)(CGLuint program, CGLsizei bufSize, CGLsizei *length, CGLchar *infoLog);
	void (CGL_APIENTRY *UseProgram)(CGLuint program);
	CGLint (CGL_APIENTRY *GetUniformLocation)(CGLuint program, const CGLchar *name);
	void (CGL_APIENTRY *Uniform1i)(CGLint location, CGLint v0);
	void (CGL_APIENTRY *Uniform1f)(CGLint location, CGLfloat v0);
	void (CGL_APIENTRY *Uniform4f)(CGLint location, CGLfloat v0, CGLfloat v1, CGLfloat v2, CGLfloat v3);
	// GL 3.0 framebuffers (optional): the engine may draw the 3D view into its own framebuffer (HL25), which may be
	// multisampled; the copy is read from it (resolved through a framebuffer of our own when multisampled)
	void (CGL_APIENTRY *BindFramebuffer)(CGLenum target, CGLuint framebuffer);
	void (CGL_APIENTRY *GenFramebuffers)(CGLsizei n, CGLuint *framebuffers);
	void (CGL_APIENTRY *FramebufferTexture2D)(CGLenum target, CGLenum attachment, CGLenum textarget, CGLuint texture,
		CGLint level);
	void (CGL_APIENTRY *BlitFramebuffer)(CGLint sx0, CGLint sy0, CGLint sx1, CGLint sy1, CGLint dx0, CGLint dy0,
		CGLint dx1, CGLint dy1, CGLbitfield mask, CGLenum filter);
	bool fbo;
} qgl;

void *s_pContext = NULL;	// the context the pointers were looked up in
bool s_bAvailable = false;
int s_iGeneration = 0;
char s_szWhy[256] = "no GL context";

CGLuint s_Program = 0;
CGLint s_uScreen = -1, s_uLut[ colorcorr::BLEND_MAX ], s_uWeight = -1, s_uIdentity = -1, s_uScale = -1, s_uOffset = -1;
CGLuint s_ScreenTex = 0;		// the copy of the view
int s_iScreenW = 0, s_iScreenH = 0;
CGLuint s_ResolveFbo = 0;
CGLuint s_IdentityTex = 0;		// bound on the units of tables not used this time

// debug checks (developer): GL errors around the pass, GL objects alive
bool s_bDebug = false;
unsigned int s_LastError = 0, s_LastEngineError = 0;
unsigned int s_LastTableError = 0;	// after the last ColorCorrGL_CreateLut
int s_iLiveTables = 0;			// tables made by ColorCorrGL_CreateLut and not deleted
int s_iObjectsMade = 0;			// the program, the screen texture, the identity table, the resolve framebuffer

// the editor's copy of the view (ColorCorrGL_SetSnapshot)
std::vector<unsigned char> *s_pSnapshot = NULL;
int *s_pSnapshotW = NULL, *s_pSnapshotH = NULL;

const char *const VERTEX_SHADER =
	"#version 120\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"}\n";

// The table lookups as Source's colorcorrection_ps2x.fxc: the color scaled into the table's cell centers
const char *const FRAGMENT_SHADER =
	"#version 120\n"
	"uniform sampler2D u_screen;\n"
	"uniform sampler3D u_lut0;\n"
	"uniform sampler3D u_lut1;\n"
	"uniform sampler3D u_lut2;\n"
	"uniform sampler3D u_lut3;\n"
	"uniform vec4 u_weight;\n"
	"uniform float u_identity;\n"
	"uniform vec4 u_scale;\n"
	"uniform vec4 u_offset;\n"
	"void main()\n"
	"{\n"
	"	vec3 c = texture2D( u_screen, gl_TexCoord[0].xy ).rgb;\n"
	"	vec3 o = c * u_identity;\n"
	"	o += texture3D( u_lut0, c * u_scale.x + u_offset.x ).rgb * u_weight.x;\n"
	"	o += texture3D( u_lut1, c * u_scale.y + u_offset.y ).rgb * u_weight.y;\n"
	"	o += texture3D( u_lut2, c * u_scale.z + u_offset.z ).rgb * u_weight.z;\n"
	"	o += texture3D( u_lut3, c * u_scale.w + u_offset.w ).rgb * u_weight.w;\n"
	"	gl_FragColor = vec4( clamp( o, 0.0, 1.0 ), 1.0 );\n"
	"}\n";

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

const char *s_pszMissing = NULL;

template <typename T> bool Resolve(T &fn, const char *name)
{
	fn = (T)GetProc(name);
	if (!fn && !s_pszMissing)
		s_pszMissing = name;
	return fn != NULL;
}

CGLuint Compile(CGLenum type, const char *source)
{
	CGLuint shader = qgl.CreateShader(type);
	if (!shader)
		return 0;
	qgl.ShaderSource(shader, 1, &source, NULL);
	qgl.CompileShader(shader);
	CGLint ok = 0;
	qgl.GetShaderiv(shader, CGL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "the %s shader did not compile: %s",
			type == CGL_VERTEX_SHADER ? "vertex" : "fragment", log);
		qgl.DeleteShader(shader);
		return 0;
	}
	return shader;
}

bool BuildProgram()
{
	CGLuint vs = Compile(CGL_VERTEX_SHADER, VERTEX_SHADER);
	if (!vs)
		return false;
	CGLuint fs = Compile(CGL_FRAGMENT_SHADER, FRAGMENT_SHADER);
	if (!fs)
	{
		qgl.DeleteShader(vs);
		return false;
	}
	CGLuint program = qgl.CreateProgram();
	if (!program)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "glCreateProgram failed");
		qgl.DeleteShader(vs);
		qgl.DeleteShader(fs);
		return false;
	}
	qgl.AttachShader(program, vs);
	qgl.AttachShader(program, fs);
	qgl.LinkProgram(program);
	// flagged for deletion: they go with the program
	qgl.DeleteShader(vs);
	qgl.DeleteShader(fs);
	CGLint ok = 0;
	qgl.GetProgramiv(program, CGL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetProgramInfoLog(program, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "the shader program did not link: %s", log);
		return false;
	}
	s_Program = program;
	s_uScreen = qgl.GetUniformLocation(program, "u_screen");
	char name[16];
	for (int i = 0; i < colorcorr::BLEND_MAX; i++)
	{
		snprintf(name, sizeof(name), "u_lut%d", i);
		s_uLut[i] = qgl.GetUniformLocation(program, name);
	}
	s_uWeight = qgl.GetUniformLocation(program, "u_weight");
	s_uIdentity = qgl.GetUniformLocation(program, "u_identity");
	s_uScale = qgl.GetUniformLocation(program, "u_scale");
	s_uOffset = qgl.GetUniformLocation(program, "u_offset");
	return true;
}

// debug: the last GL error flag set (0: none), the flags cleared
unsigned int DrainErrors()
{
	unsigned int last = 0;
	if (s_bDebug && qgl.GetError)
		for (int i = 0; i < 8; i++)
		{
			const unsigned int e = qgl.GetError();
			if (!e)
				break;
			last = e;
		}
	return last;
}

// Uploads go with no pixel buffer bound and rows packed to 1 byte (both put back)
struct UploadState
{
	CGLint alignment = 4, unpackBuffer = 0;
	void Begin()
	{
		qgl.GetIntegerv(CGL_UNPACK_ALIGNMENT, &alignment);
		if (qgl.BindBuffer)
		{
			qgl.GetIntegerv(CGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
			if (unpackBuffer)
				qgl.BindBuffer(CGL_PIXEL_UNPACK_BUFFER, 0);
		}
		qgl.PixelStorei(CGL_UNPACK_ALIGNMENT, 1);
	}
	void End()
	{
		qgl.PixelStorei(CGL_UNPACK_ALIGNMENT, alignment);
		if (qgl.BindBuffer && unpackBuffer)
			qgl.BindBuffer(CGL_PIXEL_UNPACK_BUFFER, (CGLuint)unpackBuffer);
	}
};

// A 3D texture of the table on the active unit (its binding as it was afterwards)
CGLuint MakeLutTexture(int size, const unsigned char *rgb)
{
	CGLint previous = 0;
	qgl.GetIntegerv(CGL_TEXTURE_BINDING_3D, &previous);
	CGLuint tex = 0;
	qgl.GenTextures(1, &tex);
	if (!tex)
		return 0;
	UploadState upload;
	upload.Begin();
	qgl.BindTexture(CGL_TEXTURE_3D, tex);
	qgl.TexParameteri(CGL_TEXTURE_3D, CGL_TEXTURE_MIN_FILTER, CGL_LINEAR);
	qgl.TexParameteri(CGL_TEXTURE_3D, CGL_TEXTURE_MAG_FILTER, CGL_LINEAR);
	qgl.TexParameteri(CGL_TEXTURE_3D, CGL_TEXTURE_WRAP_S, CGL_CLAMP_TO_EDGE);
	qgl.TexParameteri(CGL_TEXTURE_3D, CGL_TEXTURE_WRAP_T, CGL_CLAMP_TO_EDGE);
	qgl.TexParameteri(CGL_TEXTURE_3D, CGL_TEXTURE_WRAP_R, CGL_CLAMP_TO_EDGE);
	qgl.TexImage3D(CGL_TEXTURE_3D, 0, CGL_RGB8, size, size, size, 0, CGL_RGB, CGL_UNSIGNED_BYTE, rgb);
	qgl.BindTexture(CGL_TEXTURE_3D, (CGLuint)previous);
	upload.End();
	return tex;
}
} // namespace

bool ColorCorrGL_Available()
{
	void *context = CurrentContext();
	if (!context)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "no OpenGL context (software renderer?)");
		return false;
	}
	if (context == s_pContext)
		return s_bAvailable;

	// a new context: everything made in the old one is gone with it
	s_pContext = context;
	s_iGeneration++;
	s_Program = 0;
	s_ScreenTex = 0;
	s_iScreenW = s_iScreenH = 0;
	s_ResolveFbo = 0;
	s_IdentityTex = 0;
	s_pszMissing = NULL;
	s_bAvailable =
		Resolve(qgl.Enable, "glEnable") &&
		Resolve(qgl.Disable, "glDisable") &&
		Resolve(qgl.IsEnabled, "glIsEnabled") &&
		Resolve(qgl.GetIntegerv, "glGetIntegerv") &&
		Resolve(qgl.GetBooleanv, "glGetBooleanv") &&
		Resolve(qgl.Viewport, "glViewport") &&
		Resolve(qgl.DepthMask, "glDepthMask") &&
		Resolve(qgl.ColorMask, "glColorMask") &&
		Resolve(qgl.PolygonMode, "glPolygonMode") &&
		Resolve(qgl.PixelStorei, "glPixelStorei") &&
		Resolve(qgl.GenTextures, "glGenTextures") &&
		Resolve(qgl.DeleteTextures, "glDeleteTextures") &&
		Resolve(qgl.BindTexture, "glBindTexture") &&
		Resolve(qgl.TexParameteri, "glTexParameteri") &&
		Resolve(qgl.TexImage2D, "glTexImage2D") &&
		Resolve(qgl.CopyTexSubImage2D, "glCopyTexSubImage2D") &&
		Resolve(qgl.Begin, "glBegin") &&
		Resolve(qgl.End, "glEnd") &&
		Resolve(qgl.TexCoord2f, "glTexCoord2f") &&
		Resolve(qgl.Vertex2f, "glVertex2f") &&
		Resolve(qgl.TexImage3D, "glTexImage3D") &&
		Resolve(qgl.ActiveTexture, "glActiveTexture") &&
		Resolve(qgl.CreateShader, "glCreateShader") &&
		Resolve(qgl.ShaderSource, "glShaderSource") &&
		Resolve(qgl.CompileShader, "glCompileShader") &&
		Resolve(qgl.GetShaderiv, "glGetShaderiv") &&
		Resolve(qgl.GetShaderInfoLog, "glGetShaderInfoLog") &&
		Resolve(qgl.DeleteShader, "glDeleteShader") &&
		Resolve(qgl.CreateProgram, "glCreateProgram") &&
		Resolve(qgl.AttachShader, "glAttachShader") &&
		Resolve(qgl.LinkProgram, "glLinkProgram") &&
		Resolve(qgl.GetProgramiv, "glGetProgramiv") &&
		Resolve(qgl.GetProgramInfoLog, "glGetProgramInfoLog") &&
		Resolve(qgl.UseProgram, "glUseProgram") &&
		Resolve(qgl.GetUniformLocation, "glGetUniformLocation") &&
		Resolve(qgl.Uniform1i, "glUniform1i") &&
		Resolve(qgl.Uniform1f, "glUniform1f") &&
		Resolve(qgl.Uniform4f, "glUniform4f");
	if (!s_bAvailable)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "the driver has no %s (needs OpenGL 2.0)", s_pszMissing ? s_pszMissing : "?");
		return false;
	}
	qgl.BindBuffer = (decltype(qgl.BindBuffer))GetProc("glBindBuffer");
	qgl.GetError = (decltype(qgl.GetError))GetProc("glGetError");
	qgl.GetTexImage = (decltype(qgl.GetTexImage))GetProc("glGetTexImage");
	s_iLiveTables = 0;
	qgl.fbo = Resolve(qgl.BindFramebuffer, "glBindFramebuffer") && Resolve(qgl.GenFramebuffers, "glGenFramebuffers") &&
	          Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2D") &&
	          Resolve(qgl.BlitFramebuffer, "glBlitFramebuffer");

	// the program, with the engine's program and the active unit's 3D binding as they were
	CGLint program = 0;
	qgl.GetIntegerv(CGL_CURRENT_PROGRAM, &program);
	s_bAvailable = BuildProgram();
	if (s_bAvailable)
		s_iObjectsMade++;
	qgl.UseProgram((CGLuint)program);
	if (!s_bAvailable)
		return false;

	colorcorr::Lut identity;
	colorcorr::IdentityLut(2, identity);
	s_IdentityTex = MakeLutTexture(identity.size, identity.rgb.data());
	if (s_IdentityTex)
		s_iObjectsMade++;
	if (!s_IdentityTex)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "could not make a 3D texture");
		s_bAvailable = false;
		return false;
	}
	s_szWhy[0] = 0;
	return true;
}

const char *ColorCorrGL_Why()
{
	return s_szWhy;
}

void *ColorCorrGL_Context()
{
	return s_pContext;
}

int ColorCorrGL_Generation()
{
	return s_iGeneration;
}

bool ColorCorrGL_GetViewport(int vp[4])
{
	if (!ColorCorrGL_Available())
		return false;
	CGLint v[4] = { 0, 0, 0, 0 };
	qgl.GetIntegerv(CGL_VIEWPORT, v);
	for (int i = 0; i < 4; i++)
		vp[i] = v[i];
	return v[2] > 0 && v[3] > 0;
}

unsigned int ColorCorrGL_CreateLut(const colorcorr::Lut &lut)
{
	if (!ColorCorrGL_Available())
		return 0;
	if (lut.size < colorcorr::CUBE_MIN_SIZE || lut.size > colorcorr::CUBE_MAX_SIZE ||
		lut.rgb.size() != (size_t)lut.size * lut.size * lut.size * 3)
		return 0;
	const unsigned int before = DrainErrors();
	if (before)
		s_LastEngineError = before;
	const unsigned int tex = MakeLutTexture(lut.size, lut.rgb.data());
	s_LastTableError = DrainErrors();
	if (tex)
		s_iLiveTables++;
	return tex;
}

void ColorCorrGL_DeleteLut(unsigned int tex)
{
	if (tex && CurrentContext() == s_pContext && s_bAvailable)
	{
		qgl.DeleteTextures(1, &tex);
		s_iLiveTables--;
	}
}

bool ColorCorrGL_Apply(const int vp[4], float identity, int count, const unsigned int *tex, const int *sizes,
	const float *weights)
{
	if (!ColorCorrGL_Available() || !vp || vp[2] <= 0 || vp[3] <= 0 || count < 0 || count > colorcorr::BLEND_MAX)
		return false;
	const int x = vp[0], y = vp[1], w = vp[2], h = vp[3];

	// debug: what the engine left in the error flags is not ours (noted apart, then cleared)
	s_LastError = 0;
	s_LastEngineError = DrainErrors();

	// --- read everything that is changed below
	CGLint program = 0, activeUnit = CGL_TEXTURE0, bound2D = 0, bound3D[colorcorr::BLEND_MAX] = { 0 };
	CGLint viewport[4] = { 0, 0, 0, 0 }, polygonMode[2] = { (CGLint)CGL_FILL, (CGLint)CGL_FILL };
	CGLint drawFbo = 0, readFbo = 0, multisampled = 0;
	CGLboolean depthMask = 1, colorMask[4] = { 1, 1, 1, 1 };
	qgl.GetIntegerv(CGL_CURRENT_PROGRAM, &program);
	qgl.GetIntegerv(CGL_ACTIVE_TEXTURE, &activeUnit);
	qgl.ActiveTexture(CGL_TEXTURE0);
	qgl.GetIntegerv(CGL_TEXTURE_BINDING_2D, &bound2D);
	for (int i = 0; i < colorcorr::BLEND_MAX; i++)
	{
		qgl.ActiveTexture(CGL_TEXTURE0 + 1 + i);
		qgl.GetIntegerv(CGL_TEXTURE_BINDING_3D, &bound3D[i]);
	}
	qgl.ActiveTexture(CGL_TEXTURE0);
	qgl.GetIntegerv(CGL_VIEWPORT, viewport);
	qgl.GetIntegerv(CGL_POLYGON_MODE, polygonMode);
	qgl.GetBooleanv(CGL_DEPTH_WRITEMASK, &depthMask);
	qgl.GetBooleanv(CGL_COLOR_WRITEMASK, colorMask);
	const CGLboolean depthTest = qgl.IsEnabled(CGL_DEPTH_TEST), blend = qgl.IsEnabled(CGL_BLEND),
		alphaTest = qgl.IsEnabled(CGL_ALPHA_TEST), cull = qgl.IsEnabled(CGL_CULL_FACE),
		scissor = qgl.IsEnabled(CGL_SCISSOR_TEST), stencil = qgl.IsEnabled(CGL_STENCIL_TEST);
	if (qgl.fbo)
	{
		qgl.GetIntegerv(CGL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		qgl.GetIntegerv(CGL_READ_FRAMEBUFFER_BINDING, &readFbo);
		qgl.GetIntegerv(CGL_SAMPLE_BUFFERS, &multisampled);
	}

	// --- the view into the screen texture (unit 0)
	if (scissor)
		qgl.Disable(CGL_SCISSOR_TEST);	// the resolve blit is scissored
	if (!s_ScreenTex)
	{
		qgl.GenTextures(1, &s_ScreenTex);
		s_iObjectsMade++;
		s_iScreenW = s_iScreenH = 0;
	}
	qgl.BindTexture(CGL_TEXTURE_2D, s_ScreenTex);
	if (s_iScreenW != w || s_iScreenH != h)
	{
		UploadState upload;
		upload.Begin();
		qgl.TexParameteri(CGL_TEXTURE_2D, CGL_TEXTURE_MIN_FILTER, CGL_NEAREST);
		qgl.TexParameteri(CGL_TEXTURE_2D, CGL_TEXTURE_MAG_FILTER, CGL_NEAREST);
		qgl.TexParameteri(CGL_TEXTURE_2D, CGL_TEXTURE_WRAP_S, CGL_CLAMP_TO_EDGE);
		qgl.TexParameteri(CGL_TEXTURE_2D, CGL_TEXTURE_WRAP_T, CGL_CLAMP_TO_EDGE);
		qgl.TexImage2D(CGL_TEXTURE_2D, 0, CGL_RGB8, w, h, 0, CGL_RGB, CGL_UNSIGNED_BYTE, NULL);
		upload.End();
		s_iScreenW = w;
		s_iScreenH = h;
	}
	if (qgl.fbo && multisampled)
	{
		// a multisampled view: resolved into the texture through our framebuffer
		if (!s_ResolveFbo)
		{
			qgl.GenFramebuffers(1, &s_ResolveFbo);
			s_iObjectsMade++;
		}
		qgl.BindFramebuffer(CGL_READ_FRAMEBUFFER, (CGLuint)drawFbo);
		qgl.BindFramebuffer(CGL_DRAW_FRAMEBUFFER, s_ResolveFbo);
		qgl.FramebufferTexture2D(CGL_DRAW_FRAMEBUFFER, CGL_COLOR_ATTACHMENT0, CGL_TEXTURE_2D, s_ScreenTex, 0);
		qgl.BlitFramebuffer(x, y, x + w, y + h, 0, 0, w, h, CGL_COLOR_BUFFER_BIT, CGL_NEAREST);
		qgl.BindFramebuffer(CGL_DRAW_FRAMEBUFFER, (CGLuint)drawFbo);
		qgl.BindFramebuffer(CGL_READ_FRAMEBUFFER, (CGLuint)readFbo);
	}
	else
	{
		// read where the view was drawn (the engine's framebuffer, or the window's)
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(CGL_READ_FRAMEBUFFER, (CGLuint)drawFbo);
		qgl.CopyTexSubImage2D(CGL_TEXTURE_2D, 0, 0, 0, x, y, w, h);
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(CGL_READ_FRAMEBUFFER, (CGLuint)readFbo);
	}

	// --- the editor's copy of the view, read back from the screen texture (rows packed to 1 byte, no pack buffer)
	if (s_pSnapshot)
	{
		s_pSnapshot->clear();
		if (qgl.GetTexImage)
		{
			CGLint packAlignment = 4, packBuffer = 0;
			qgl.GetIntegerv(CGL_PACK_ALIGNMENT, &packAlignment);
			if (qgl.BindBuffer)
			{
				qgl.GetIntegerv(CGL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
				if (packBuffer)
					qgl.BindBuffer(CGL_PIXEL_PACK_BUFFER, 0);
			}
			qgl.PixelStorei(CGL_PACK_ALIGNMENT, 1);
			s_pSnapshot->resize((size_t)w * h * 3);
			qgl.GetTexImage(CGL_TEXTURE_2D, 0, CGL_RGB, CGL_UNSIGNED_BYTE, s_pSnapshot->data());
			qgl.PixelStorei(CGL_PACK_ALIGNMENT, packAlignment);
			if (qgl.BindBuffer && packBuffer)
				qgl.BindBuffer(CGL_PIXEL_PACK_BUFFER, (CGLuint)packBuffer);
			if (s_pSnapshotW)
				*s_pSnapshotW = w;
			if (s_pSnapshotH)
				*s_pSnapshotH = h;
		}
		s_pSnapshot = NULL;
	}

	// --- the tables on units 1 .. 4 (the identity table where none is used)
	float weight[colorcorr::BLEND_MAX] = { 0, 0, 0, 0 }, scale[colorcorr::BLEND_MAX], offset[colorcorr::BLEND_MAX];
	for (int i = 0; i < colorcorr::BLEND_MAX; i++)
	{
		const bool used = i < count && tex && tex[i] && sizes && sizes[i] >= 2;
		const int size = used ? sizes[i] : 2;
		qgl.ActiveTexture(CGL_TEXTURE0 + 1 + i);
		qgl.BindTexture(CGL_TEXTURE_3D, used ? tex[i] : s_IdentityTex);
		weight[i] = used && weights ? weights[i] : 0.0f;
		scale[i] = (size - 1) / (float)size;
		offset[i] = 0.5f / size;
	}
	qgl.ActiveTexture(CGL_TEXTURE0);

	// --- the pass: one quad over the viewport, in clip space (no matrices)
	qgl.UseProgram(s_Program);
	qgl.Uniform1i(s_uScreen, 0);
	for (int i = 0; i < colorcorr::BLEND_MAX; i++)
		qgl.Uniform1i(s_uLut[i], 1 + i);
	qgl.Uniform4f(s_uWeight, weight[0], weight[1], weight[2], weight[3]);
	qgl.Uniform1f(s_uIdentity, identity);
	qgl.Uniform4f(s_uScale, scale[0], scale[1], scale[2], scale[3]);
	qgl.Uniform4f(s_uOffset, offset[0], offset[1], offset[2], offset[3]);

	qgl.Viewport(x, y, w, h);
	if (depthTest)
		qgl.Disable(CGL_DEPTH_TEST);
	if (blend)
		qgl.Disable(CGL_BLEND);
	if (alphaTest)
		qgl.Disable(CGL_ALPHA_TEST);
	if (cull)
		qgl.Disable(CGL_CULL_FACE);
	if (stencil)
		qgl.Disable(CGL_STENCIL_TEST);
	qgl.DepthMask(0);
	qgl.ColorMask(1, 1, 1, 1);
	qgl.PolygonMode(CGL_FRONT, CGL_FILL);
	qgl.PolygonMode(CGL_BACK, CGL_FILL);

	qgl.Begin(CGL_QUADS);
	qgl.TexCoord2f(0.0f, 0.0f); qgl.Vertex2f(-1.0f, -1.0f);
	qgl.TexCoord2f(1.0f, 0.0f); qgl.Vertex2f(1.0f, -1.0f);
	qgl.TexCoord2f(1.0f, 1.0f); qgl.Vertex2f(1.0f, 1.0f);
	qgl.TexCoord2f(0.0f, 1.0f); qgl.Vertex2f(-1.0f, 1.0f);
	qgl.End();

	// --- everything as it was
	qgl.PolygonMode(CGL_FRONT, (CGLenum)polygonMode[0]);
	qgl.PolygonMode(CGL_BACK, (CGLenum)polygonMode[1]);
	qgl.ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	qgl.DepthMask(depthMask);
	if (stencil)
		qgl.Enable(CGL_STENCIL_TEST);
	if (cull)
		qgl.Enable(CGL_CULL_FACE);
	if (alphaTest)
		qgl.Enable(CGL_ALPHA_TEST);
	if (blend)
		qgl.Enable(CGL_BLEND);
	if (depthTest)
		qgl.Enable(CGL_DEPTH_TEST);
	if (scissor)
		qgl.Enable(CGL_SCISSOR_TEST);
	qgl.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	qgl.UseProgram((CGLuint)program);
	for (int i = 0; i < colorcorr::BLEND_MAX; i++)
	{
		qgl.ActiveTexture(CGL_TEXTURE0 + 1 + i);
		qgl.BindTexture(CGL_TEXTURE_3D, (CGLuint)bound3D[i]);
	}
	qgl.ActiveTexture(CGL_TEXTURE0);
	qgl.BindTexture(CGL_TEXTURE_2D, (CGLuint)bound2D);
	qgl.ActiveTexture((CGLenum)activeUnit);

	s_LastError = DrainErrors();
	return true;
}

void ColorCorrGL_SetSnapshot(std::vector<unsigned char> *out, int *wide, int *tall)
{
	s_pSnapshot = out;
	s_pSnapshotW = wide;
	s_pSnapshotH = tall;
}

void ColorCorrGL_SetDebug(bool on)
{
	s_bDebug = on;
}

unsigned int ColorCorrGL_LastTableError()
{
	return s_LastTableError;
}

unsigned int ColorCorrGL_LastError(unsigned int *engineError)
{
	if (engineError)
		*engineError = s_LastEngineError;
	return s_LastError;
}

void ColorCorrGL_Counts(int *liveTables, int *objectsMade)
{
	if (liveTables)
		*liveTables = s_iLiveTables;
	if (objectsMade)
		*objectsMade = s_iObjectsMade;
}
