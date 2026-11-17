//=========================================================
// SBO 2.0 env_fogzone: the per-pixel fog volume pass (fogzone_gl.h, docs/WEATHER.md "env_fogzone").
//
// The fog of a zone is drawn as a volume: for every pixel of the zone's box on screen, the part of the view ray that
// lies inside the box AND in front of what the view shows there (the view's depth) is how much fog the pixel looks
// through; alpha = 1 - exp( -( thickness * density )^2 ), the engine's own fog curve. The thickness goes to nothing at
// the box's outline from any angle, the floor, walls and things inside the zone are fogged by what lies in front of
// them, and a zone that fills a room exactly ends at the room's walls.
//
// How it is drawn:
//  - the view's depth is copied into a texture once per view (FogZoneGL_Begin): glCopyTexSubImage2D, or when the
//    engine draws into a multisampled framebuffer (HL25 MSAA) a depth blit into a framebuffer of our own whose depth
//    format is the engine's (read at run time: a depth blit needs the same format on both sides);
//  - each zone's far faces (the faces the eye is on the inner side of, which cover the box on screen whether the eye
//    is inside or outside, and are never cut by the near plane) with a GLSL 1.20 program, no depth test, alpha
//    blended. The program reads the depth at the pixel, turns it into eye-space distance with the projection matrix
//    and depth range of the view, and intersects the ray with the box.
//
// The GL functions are looked up in the current context as colorcorrection_gl.cpp does (no GL header or import
// library). No glPushAttrib: every value changed is read first and put back exactly; the engine caches its active
// texture unit, the textures bound on each unit and its GLSL program.
//=========================================================

#include "fogzone_gl.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define FGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define FGL_APIENTRY
#endif

namespace
{
typedef unsigned int FGLenum;
typedef unsigned char FGLboolean;
typedef unsigned int FGLbitfield;
typedef int FGLint;
typedef int FGLsizei;
typedef float FGLfloat;
typedef unsigned int FGLuint;
typedef char FGLchar;

const FGLenum FGL_NONE = 0;
const FGLenum FGL_CULL_FACE = 0x0B44;
const FGLenum FGL_POLYGON_MODE = 0x0B40;
const FGLenum FGL_DEPTH_TEST = 0x0B71;
const FGLenum FGL_DEPTH_WRITEMASK = 0x0B72;
const FGLenum FGL_DEPTH_BITS = 0x0D56;
const FGLenum FGL_STENCIL_TEST = 0x0B90;
const FGLenum FGL_VIEWPORT = 0x0BA2;
const FGLenum FGL_ALPHA_TEST = 0x0BC0;
const FGLenum FGL_BLEND_DST = 0x0BE0;
const FGLenum FGL_BLEND_SRC = 0x0BE1;
const FGLenum FGL_BLEND = 0x0BE2;
const FGLenum FGL_SCISSOR_TEST = 0x0C11;
const FGLenum FGL_COLOR_WRITEMASK = 0x0C23;
const FGLenum FGL_UNPACK_ALIGNMENT = 0x0CF5;
const FGLenum FGL_TEXTURE_2D = 0x0DE1;
const FGLenum FGL_TEXTURE_BINDING_2D = 0x8069;
const FGLenum FGL_TEXTURE_MIN_FILTER = 0x2801;
const FGLenum FGL_TEXTURE_MAG_FILTER = 0x2800;
const FGLenum FGL_TEXTURE_WRAP_S = 0x2802;
const FGLenum FGL_TEXTURE_WRAP_T = 0x2803;
const FGLenum FGL_TEXTURE_COMPARE_MODE = 0x884C;
const FGLint FGL_NEAREST = 0x2600;
const FGLint FGL_CLAMP_TO_EDGE = 0x812F;
const FGLenum FGL_SRC_ALPHA = 0x0302;
const FGLenum FGL_ONE_MINUS_SRC_ALPHA = 0x0303;
const FGLenum FGL_QUADS = 0x0007;
const FGLenum FGL_FRONT = 0x0404;
const FGLenum FGL_BACK = 0x0405;
const FGLenum FGL_FILL = 0x1B02;
const FGLenum FGL_DEPTH = 0x1801;
const FGLbitfield FGL_DEPTH_BUFFER_BIT = 0x00000100;
const FGLenum FGL_TEXTURE0 = 0x84C0;
const FGLenum FGL_ACTIVE_TEXTURE = 0x84E0;
const FGLenum FGL_FRAGMENT_SHADER = 0x8B30;
const FGLenum FGL_VERTEX_SHADER = 0x8B31;
const FGLenum FGL_COMPILE_STATUS = 0x8B81;
const FGLenum FGL_LINK_STATUS = 0x8B82;
const FGLenum FGL_CURRENT_PROGRAM = 0x8B8D;
const FGLenum FGL_PIXEL_UNPACK_BUFFER = 0x88EC;
const FGLenum FGL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;
const FGLenum FGL_READ_FRAMEBUFFER = 0x8CA8;
const FGLenum FGL_DRAW_FRAMEBUFFER = 0x8CA9;
const FGLenum FGL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const FGLenum FGL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const FGLenum FGL_SAMPLE_BUFFERS = 0x80A8;
const FGLenum FGL_SAMPLES = 0x80A9;
const FGLenum FGL_DEPTH_ATTACHMENT = 0x8D00;
const FGLenum FGL_DEPTH_STENCIL_ATTACHMENT = 0x821A;
const FGLenum FGL_STENCIL_ATTACHMENT = 0x8D20;
const FGLenum FGL_STENCIL = 0x1802;
const FGLenum FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1;
const FGLenum FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0;
const FGLenum FGL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211;
const FGLenum FGL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216;
const FGLenum FGL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217;
const FGLenum FGL_FRAMEBUFFER_COMPLETE = 0x8CD5;
const FGLenum FGL_FLOAT = 0x1406;
const FGLenum FGL_UNSIGNED_INT = 0x1405;
const FGLenum FGL_UNSIGNED_SHORT = 0x1403;
const FGLenum FGL_DEPTH_COMPONENT = 0x1902;
const FGLenum FGL_DEPTH_STENCIL = 0x84F9;
const FGLenum FGL_UNSIGNED_INT_24_8 = 0x84FA;
const FGLenum FGL_FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD;
const FGLint FGL_DEPTH_COMPONENT16 = 0x81A5;
const FGLint FGL_DEPTH_COMPONENT24 = 0x81A6;
const FGLint FGL_DEPTH_COMPONENT32 = 0x81A7;
const FGLint FGL_DEPTH_COMPONENT32F = 0x8CAC;
const FGLint FGL_DEPTH32F_STENCIL8 = 0x8CAD;
const FGLint FGL_DEPTH24_STENCIL8 = 0x88F0;

struct
{
	// GL 1.1
	void (FGL_APIENTRY *Enable)(FGLenum cap);
	void (FGL_APIENTRY *Disable)(FGLenum cap);
	FGLboolean (FGL_APIENTRY *IsEnabled)(FGLenum cap);
	void (FGL_APIENTRY *GetIntegerv)(FGLenum pname, FGLint *params);
	void (FGL_APIENTRY *GetBooleanv)(FGLenum pname, FGLboolean *params);
	FGLenum (FGL_APIENTRY *GetError)(void);
	void (FGL_APIENTRY *DepthMask)(FGLboolean flag);
	void (FGL_APIENTRY *ColorMask)(FGLboolean r, FGLboolean g, FGLboolean b, FGLboolean a);
	void (FGL_APIENTRY *BlendFunc)(FGLenum sfactor, FGLenum dfactor);
	void (FGL_APIENTRY *PolygonMode)(FGLenum face, FGLenum mode);
	void (FGL_APIENTRY *PixelStorei)(FGLenum pname, FGLint param);
	void (FGL_APIENTRY *DrawBuffer)(FGLenum mode);
	void (FGL_APIENTRY *GenTextures)(FGLsizei n, FGLuint *textures);
	void (FGL_APIENTRY *BindTexture)(FGLenum target, FGLuint texture);
	void (FGL_APIENTRY *TexParameteri)(FGLenum target, FGLenum pname, FGLint param);
	void (FGL_APIENTRY *TexImage2D)(FGLenum target, FGLint level, FGLint internalformat, FGLsizei width,
		FGLsizei height, FGLint border, FGLenum format, FGLenum type, const void *pixels);
	void (FGL_APIENTRY *CopyTexSubImage2D)(FGLenum target, FGLint level, FGLint xoffset, FGLint yoffset, FGLint x,
		FGLint y, FGLsizei width, FGLsizei height);
	void (FGL_APIENTRY *Begin)(FGLenum mode);
	void (FGL_APIENTRY *End)(void);
	void (FGL_APIENTRY *Vertex3f)(FGLfloat x, FGLfloat y, FGLfloat z);
	// GL 1.3 / 1.5
	void (FGL_APIENTRY *ActiveTexture)(FGLenum texture);
	void (FGL_APIENTRY *BindBuffer)(FGLenum target, FGLuint buffer);	// optional
	// GL 2.0
	FGLuint (FGL_APIENTRY *CreateShader)(FGLenum type);
	void (FGL_APIENTRY *ShaderSource)(FGLuint shader, FGLsizei count, const FGLchar *const *string, const FGLint *length);
	void (FGL_APIENTRY *CompileShader)(FGLuint shader);
	void (FGL_APIENTRY *GetShaderiv)(FGLuint shader, FGLenum pname, FGLint *params);
	void (FGL_APIENTRY *GetShaderInfoLog)(FGLuint shader, FGLsizei bufSize, FGLsizei *length, FGLchar *infoLog);
	void (FGL_APIENTRY *DeleteShader)(FGLuint shader);
	FGLuint (FGL_APIENTRY *CreateProgram)(void);
	void (FGL_APIENTRY *AttachShader)(FGLuint program, FGLuint shader);
	void (FGL_APIENTRY *LinkProgram)(FGLuint program);
	void (FGL_APIENTRY *GetProgramiv)(FGLuint program, FGLenum pname, FGLint *params);
	void (FGL_APIENTRY *GetProgramInfoLog)(FGLuint program, FGLsizei bufSize, FGLsizei *length, FGLchar *infoLog);
	void (FGL_APIENTRY *UseProgram)(FGLuint program);
	FGLint (FGL_APIENTRY *GetUniformLocation)(FGLuint program, const FGLchar *name);
	void (FGL_APIENTRY *Uniform1i)(FGLint location, FGLint v0);
	void (FGL_APIENTRY *Uniform1f)(FGLint location, FGLfloat v0);
	void (FGL_APIENTRY *Uniform3f)(FGLint location, FGLfloat v0, FGLfloat v1, FGLfloat v2);
	void (FGL_APIENTRY *Uniform4f)(FGLint location, FGLfloat v0, FGLfloat v1, FGLfloat v2, FGLfloat v3);
	// GL 3.0 framebuffers (optional; needed only when the view is multisampled)
	void (FGL_APIENTRY *BindFramebuffer)(FGLenum target, FGLuint framebuffer);
	void (FGL_APIENTRY *GenFramebuffers)(FGLsizei n, FGLuint *framebuffers);
	void (FGL_APIENTRY *FramebufferTexture2D)(FGLenum target, FGLenum attachment, FGLenum textarget, FGLuint texture,
		FGLint level);
	FGLenum (FGL_APIENTRY *CheckFramebufferStatus)(FGLenum target);
	void (FGL_APIENTRY *GetFramebufferAttachmentParameteriv)(FGLenum target, FGLenum attachment, FGLenum pname,
		FGLint *params);
	void (FGL_APIENTRY *BlitFramebuffer)(FGLint sx0, FGLint sy0, FGLint sx1, FGLint sy1, FGLint dx0, FGLint dy0,
		FGLint dx1, FGLint dy1, FGLbitfield mask, FGLenum filter);
	bool fbo;
} qgl;

void *s_pContext = NULL;	// the context the pointers were looked up in
bool s_bAvailable = false;
int s_iGeneration = 0;
char s_szWhy[256] = "no GL context";
char s_szDescribe[160] = "";

FGLuint s_Program = 0;
FGLint s_uDepth = -1, s_uViewport = -1, s_uEye = -1, s_uMins = -1, s_uMaxs = -1, s_uColor = -1, s_uDensity = -1;
FGLint s_uRfMin = -1, s_uRfMax = -1, s_uAddMin = -1, s_uAddMax = -1;

// the thinning band inside each face: this deep, or a quarter of the box's size on that axis if that is less (a thin
// box keeps a full-density middle)
const float FOGZONE_FEATHER = 64.0f;

// the copy of the view's depth: its size and format (the format is the engine's when it is resolved by a blit)
FGLuint s_DepthTex = 0;
int s_iDepthW = 0, s_iDepthH = 0;
FGLint s_iDepthFormat = 0;
FGLuint s_ResolveFbo = 0;
bool s_bResolveChecked = false;	// the first resolve in this context was checked for a GL error
// SBO 2.1: the copy holds this view's depth (FogZoneGL_NewView clears it): env_entity_glow uses it again
bool s_bCopiedThisView = false;
int s_CopiedViewport[4];

// what Begin changed, put back by End
struct
{
	FGLint program, activeUnit, bound2D, polygonMode[2], blendSrc, blendDst;
	FGLboolean depthMask, colorMask[4];
	FGLboolean depthTest, blend, alphaTest, cull, stencil;
} s_Saved;
bool s_bBegun = false;
float s_vecEye[3];

const char *const VERTEX_SHADER =
	"#version 120\n"
	"varying vec3 v_world;\n"
	"varying float v_eyez;\n"
	"void main()\n"
	"{\n"
	"	v_world = gl_Vertex.xyz;\n"
	"	v_eyez = ( gl_ModelViewMatrix * gl_Vertex ).z;\n"
	"	gl_Position = ftransform();\n"
	"}\n";

// The fog over the part of the view ray inside the box and in front of the view's depth. The fragment lies on a far
// face of the box, so the ray eye -> fragment (t 0..1) ends at the box; the view's depth gives the eye-space z of what
// is drawn at the pixel, and t of that point is its z over the fragment's z (same ray).
// The density thins out toward the box's open faces (u_rfMin / u_rfMax: 1 / the band's depth per axis, u_addMin /
// u_addMax: 1 for a face against a wall, which keeps full density there): full from the band's depth inside an open
// face, nothing at the face, a smoothstep between, so the volume has no straight edge where it meets open air. The fog along the ray is summed
// at 24 points (midpoints) of the part inside the box; alpha = 1 - exp( -( sum of density * length )^2 ),
// the engine's fog curve.
const char *const FRAGMENT_SHADER =
	"#version 120\n"
	"uniform sampler2D u_depth;\n"
	"uniform vec4 u_viewport;\n"
	"uniform vec3 u_eye;\n"
	"uniform vec3 u_mins;\n"
	"uniform vec3 u_maxs;\n"
	"uniform vec4 u_color;\n"	// rgb, and how much of the zone's fog shows
	"uniform float u_density;\n"
	"uniform vec3 u_rfMin;\n"
	"uniform vec3 u_rfMax;\n"
	"uniform vec3 u_addMin;\n"
	"uniform vec3 u_addMax;\n"
	"varying vec3 v_world;\n"
	"varying float v_eyez;\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = ( gl_FragCoord.xy - u_viewport.xy ) / u_viewport.zw;\n"
	"	float d = texture2D( u_depth, uv ).r;\n"
	"	float ndc = ( d - gl_DepthRange.near ) / gl_DepthRange.diff * 2.0 - 1.0;\n"
	"	float zScene = -gl_ProjectionMatrix[3][2] / ( ndc + gl_ProjectionMatrix[2][2] );\n"
	"	float tScene = zScene / min( v_eyez, -0.001 );\n"
	"	vec3 dir = v_world - u_eye;\n"
	"	vec3 s = vec3( greaterThanEqual( dir, vec3( 0.0 ) ) ) * 2.0 - 1.0;\n"
	"	vec3 inv = 1.0 / ( s * max( abs( dir ), vec3( 0.0001 ) ) );\n"
	"	vec3 t1 = ( u_mins - u_eye ) * inv;\n"
	"	vec3 t2 = ( u_maxs - u_eye ) * inv;\n"
	"	vec3 tn = min( t1, t2 );\n"
	"	vec3 tf = max( t1, t2 );\n"
	"	float tNear = max( max( tn.x, tn.y ), max( tn.z, 0.0 ) );\n"
	"	float tFar = min( min( tf.x, tf.y ), min( tf.z, tScene ) );\n"
	"	if ( tFar <= tNear ) discard;\n"
	"	float sum = 0.0;\n"
	"	for ( int i = 0; i < 24; i++ )\n"
	"	{\n"
	"		vec3 p = u_eye + dir * mix( tNear, tFar, ( float( i ) + 0.5 ) / 24.0 );\n"
	"		vec3 e = min( ( p - u_mins ) * u_rfMin + u_addMin, ( u_maxs - p ) * u_rfMax + u_addMax );\n"
	"		float g = clamp( min( min( e.x, e.y ), e.z ), 0.0, 1.0 );\n"
	"		sum += g * g * ( 3.0 - 2.0 * g );\n"
	"	}\n"
	"	float thick = ( tFar - tNear ) * length( dir ) * u_density * sum / 24.0;\n"
	"	gl_FragColor = vec4( u_color.rgb, ( 1.0 - exp( -thick * thick ) ) * u_color.a );\n"
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

FGLuint Compile(FGLenum type, const char *source)
{
	FGLuint shader = qgl.CreateShader(type);
	if (!shader)
		return 0;
	qgl.ShaderSource(shader, 1, &source, NULL);
	qgl.CompileShader(shader);
	FGLint ok = 0;
	qgl.GetShaderiv(shader, FGL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "the %s shader did not compile: %s",
			type == FGL_VERTEX_SHADER ? "vertex" : "fragment", log);
		qgl.DeleteShader(shader);
		return 0;
	}
	return shader;
}

bool BuildProgram()
{
	FGLuint vs = Compile(FGL_VERTEX_SHADER, VERTEX_SHADER);
	if (!vs)
		return false;
	FGLuint fs = Compile(FGL_FRAGMENT_SHADER, FRAGMENT_SHADER);
	if (!fs)
	{
		qgl.DeleteShader(vs);
		return false;
	}
	FGLuint program = qgl.CreateProgram();
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
	qgl.DeleteShader(vs);
	qgl.DeleteShader(fs);
	FGLint ok = 0;
	qgl.GetProgramiv(program, FGL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetProgramInfoLog(program, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "the shader program did not link: %s", log);
		return false;
	}
	s_Program = program;
	s_uDepth = qgl.GetUniformLocation(program, "u_depth");
	s_uViewport = qgl.GetUniformLocation(program, "u_viewport");
	s_uEye = qgl.GetUniformLocation(program, "u_eye");
	s_uMins = qgl.GetUniformLocation(program, "u_mins");
	s_uMaxs = qgl.GetUniformLocation(program, "u_maxs");
	s_uColor = qgl.GetUniformLocation(program, "u_color");
	s_uDensity = qgl.GetUniformLocation(program, "u_density");
	s_uRfMin = qgl.GetUniformLocation(program, "u_rfMin");
	s_uRfMax = qgl.GetUniformLocation(program, "u_rfMax");
	s_uAddMin = qgl.GetUniformLocation(program, "u_addMin");
	s_uAddMax = qgl.GetUniformLocation(program, "u_addMax");
	return true;
}

bool Available()
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
	s_DepthTex = 0;
	s_iDepthW = s_iDepthH = 0;
	s_iDepthFormat = 0;
	s_ResolveFbo = 0;
	s_bResolveChecked = false;
	s_bCopiedThisView = false;
	s_szDescribe[0] = 0;
	s_pszMissing = NULL;
	s_bAvailable =
		Resolve(qgl.Enable, "glEnable") &&
		Resolve(qgl.Disable, "glDisable") &&
		Resolve(qgl.IsEnabled, "glIsEnabled") &&
		Resolve(qgl.GetIntegerv, "glGetIntegerv") &&
		Resolve(qgl.GetBooleanv, "glGetBooleanv") &&
		Resolve(qgl.GetError, "glGetError") &&
		Resolve(qgl.DepthMask, "glDepthMask") &&
		Resolve(qgl.ColorMask, "glColorMask") &&
		Resolve(qgl.BlendFunc, "glBlendFunc") &&
		Resolve(qgl.PolygonMode, "glPolygonMode") &&
		Resolve(qgl.PixelStorei, "glPixelStorei") &&
		Resolve(qgl.DrawBuffer, "glDrawBuffer") &&
		Resolve(qgl.GenTextures, "glGenTextures") &&
		Resolve(qgl.BindTexture, "glBindTexture") &&
		Resolve(qgl.TexParameteri, "glTexParameteri") &&
		Resolve(qgl.TexImage2D, "glTexImage2D") &&
		Resolve(qgl.CopyTexSubImage2D, "glCopyTexSubImage2D") &&
		Resolve(qgl.Begin, "glBegin") &&
		Resolve(qgl.End, "glEnd") &&
		Resolve(qgl.Vertex3f, "glVertex3f") &&
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
		Resolve(qgl.Uniform3f, "glUniform3f") &&
		Resolve(qgl.Uniform4f, "glUniform4f");
	if (!s_bAvailable)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "the driver has no %s (needs OpenGL 2.0)", s_pszMissing ? s_pszMissing : "?");
		return false;
	}
	qgl.BindBuffer = (decltype(qgl.BindBuffer))GetProc("glBindBuffer");
	qgl.fbo = Resolve(qgl.BindFramebuffer, "glBindFramebuffer") && Resolve(qgl.GenFramebuffers, "glGenFramebuffers") &&
	          Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2D") &&
	          Resolve(qgl.CheckFramebufferStatus, "glCheckFramebufferStatus") &&
	          Resolve(qgl.GetFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv") &&
	          Resolve(qgl.BlitFramebuffer, "glBlitFramebuffer");

	FGLint program = 0;
	qgl.GetIntegerv(FGL_CURRENT_PROGRAM, &program);
	s_bAvailable = BuildProgram();
	qgl.UseProgram((FGLuint)program);
	if (!s_bAvailable)
		return false;
	s_szWhy[0] = 0;
	return true;
}

// The engine's depth buffer format (read framebuffer bound to it): the texture format a depth blit needs, with its
// pixel format and type for glTexImage2D. False: unknown (no depth buffer, or a size there is no texture format for).
bool EngineDepthFormat(FGLuint fbo, FGLint &internal, FGLenum &format, FGLenum &type, int &bits, int &stencil, bool &flt)
{
	const FGLenum attachment = fbo ? FGL_DEPTH_ATTACHMENT : FGL_DEPTH;
	FGLint objectType = 0, depthSize = 0, stencilSize = 0, componentType = 0;
	qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, attachment, FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
		&objectType);
	if (objectType == (FGLint)FGL_NONE)
		return false;
	qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, attachment, FGL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
		&depthSize);
	qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, attachment,
		FGL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &componentType);
	// a packed depth + stencil buffer: the stencil slot holds the same object (a window's depth buffer with stencil
	// bits is packed); a stencil buffer of its own leaves the depth format plain
	if (fbo)
	{
		FGLint stencilType = 0, depthName = 0, stencilName = 0;
		qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, FGL_STENCIL_ATTACHMENT,
			FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &stencilType);
		if (stencilType != (FGLint)FGL_NONE)
		{
			qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, FGL_DEPTH_ATTACHMENT,
				FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depthName);
			qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, FGL_STENCIL_ATTACHMENT,
				FGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &stencilName);
			if (depthName == stencilName)
				qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, FGL_STENCIL_ATTACHMENT,
					FGL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilSize);
		}
	}
	else
	{
		qgl.GetFramebufferAttachmentParameteriv(FGL_READ_FRAMEBUFFER, FGL_STENCIL,
			FGL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilSize);
	}
	bits = depthSize;
	stencil = stencilSize;
	flt = componentType == (FGLint)FGL_FLOAT;
	if (flt)
	{
		if (depthSize != 32)
			return false;
		internal = stencilSize ? FGL_DEPTH32F_STENCIL8 : FGL_DEPTH_COMPONENT32F;
		format = stencilSize ? FGL_DEPTH_STENCIL : FGL_DEPTH_COMPONENT;
		type = stencilSize ? FGL_FLOAT_32_UNSIGNED_INT_24_8_REV : FGL_FLOAT;
		return true;
	}
	if (stencilSize)
	{
		if (depthSize != 24)
			return false;
		internal = FGL_DEPTH24_STENCIL8;
		format = FGL_DEPTH_STENCIL;
		type = FGL_UNSIGNED_INT_24_8;
		return true;
	}
	format = FGL_DEPTH_COMPONENT;
	switch (depthSize)
	{
	case 16: internal = FGL_DEPTH_COMPONENT16; type = FGL_UNSIGNED_SHORT; return true;
	case 24: internal = FGL_DEPTH_COMPONENT24; type = FGL_UNSIGNED_INT; return true;
	case 32: internal = FGL_DEPTH_COMPONENT32; type = FGL_UNSIGNED_INT; return true;
	}
	return false;
}

// The depth texture in this size and format (bound on the active unit, unit 0)
void PrepareDepthTexture(int w, int h, FGLint internal, FGLenum format, FGLenum type)
{
	if (!s_DepthTex)
	{
		qgl.GenTextures(1, &s_DepthTex);
		s_iDepthW = s_iDepthH = 0;
	}
	qgl.BindTexture(FGL_TEXTURE_2D, s_DepthTex);
	if (s_iDepthW == w && s_iDepthH == h && s_iDepthFormat == internal)
		return;

	FGLint alignment = 4, unpackBuffer = 0;
	qgl.GetIntegerv(FGL_UNPACK_ALIGNMENT, &alignment);
	if (qgl.BindBuffer)
	{
		qgl.GetIntegerv(FGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
		if (unpackBuffer)
			qgl.BindBuffer(FGL_PIXEL_UNPACK_BUFFER, 0);
	}
	qgl.PixelStorei(FGL_UNPACK_ALIGNMENT, 1);
	qgl.TexParameteri(FGL_TEXTURE_2D, FGL_TEXTURE_MIN_FILTER, FGL_NEAREST);
	qgl.TexParameteri(FGL_TEXTURE_2D, FGL_TEXTURE_MAG_FILTER, FGL_NEAREST);
	qgl.TexParameteri(FGL_TEXTURE_2D, FGL_TEXTURE_WRAP_S, FGL_CLAMP_TO_EDGE);
	qgl.TexParameteri(FGL_TEXTURE_2D, FGL_TEXTURE_WRAP_T, FGL_CLAMP_TO_EDGE);
	qgl.TexParameteri(FGL_TEXTURE_2D, FGL_TEXTURE_COMPARE_MODE, FGL_NONE);
	qgl.TexImage2D(FGL_TEXTURE_2D, 0, internal, w, h, 0, format, type, NULL);
	qgl.PixelStorei(FGL_UNPACK_ALIGNMENT, alignment);
	if (qgl.BindBuffer && unpackBuffer)
		qgl.BindBuffer(FGL_PIXEL_UNPACK_BUFFER, (FGLuint)unpackBuffer);
	s_iDepthW = w;
	s_iDepthH = h;
	s_iDepthFormat = internal;
}

// The GL errors left set (cleared); 0: none
FGLenum DrainErrors()
{
	FGLenum last = 0;
	for (int i = 0; i < 8; i++)
	{
		const FGLenum e = qgl.GetError();
		if (!e)
			break;
		last = e;
	}
	return last;
}

// Copies the depth of the viewport x, y, w, h into s_DepthTex (bound on unit 0). False: the pass can't run.
bool CopyDepth(int x, int y, int w, int h)
{
	FGLint drawFbo = 0, readFbo = 0, multisampled = 0, samples = 0;
	if (qgl.fbo)
	{
		qgl.GetIntegerv(FGL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		qgl.GetIntegerv(FGL_READ_FRAMEBUFFER_BINDING, &readFbo);
	}
	qgl.GetIntegerv(FGL_SAMPLE_BUFFERS, &multisampled);
	qgl.GetIntegerv(FGL_SAMPLES, &samples);

	const FGLboolean scissor = qgl.IsEnabled(FGL_SCISSOR_TEST);
	if (scissor)
		qgl.Disable(FGL_SCISSOR_TEST);	// a blit is scissored

	bool ok = true;
	const bool check = !s_bResolveChecked;
	if (check)
		DrainErrors();	// the engine's, not ours: only once per context, to see whether the copy works here

	if (multisampled)
	{
		// a multisampled view: a depth blit into our framebuffer, whose texture has the engine's depth format
		if (!qgl.fbo)
		{
			snprintf(s_szWhy, sizeof(s_szWhy), "multisampled view without framebuffer objects (GL 3.0)");
			ok = false;
		}
		else
		{
			FGLint internal = 0;
			FGLenum format = 0, type = 0;
			int bits = 0, stencil = 0;
			bool flt = false;
			qgl.BindFramebuffer(FGL_READ_FRAMEBUFFER, (FGLuint)drawFbo);
			if (!EngineDepthFormat((FGLuint)drawFbo, internal, format, type, bits, stencil, flt))
			{
				snprintf(s_szWhy, sizeof(s_szWhy), "unknown depth format of the multisampled view (%d bits, %d stencil%s)",
					bits, stencil, flt ? ", float" : "");
				ok = false;
			}
			else
			{
				const bool remade = s_iDepthFormat != internal || s_iDepthW != w || s_iDepthH != h;
				PrepareDepthTexture(w, h, internal, format, type);
				if (!s_ResolveFbo)
					qgl.GenFramebuffers(1, &s_ResolveFbo);
				qgl.BindFramebuffer(FGL_DRAW_FRAMEBUFFER, s_ResolveFbo);
				if (remade)
				{
					// no color buffer: the draw buffer of our framebuffer is none (its own state; it is bound only as
					// the draw framebuffer here, so glReadBuffer would set the ENGINE's read framebuffer: never call it)
					qgl.DrawBuffer(FGL_NONE);
					qgl.FramebufferTexture2D(FGL_DRAW_FRAMEBUFFER, stencil ? FGL_DEPTH_STENCIL_ATTACHMENT : FGL_DEPTH_ATTACHMENT,
						FGL_TEXTURE_2D, s_DepthTex, 0);
				}
				if (qgl.CheckFramebufferStatus(FGL_DRAW_FRAMEBUFFER) != FGL_FRAMEBUFFER_COMPLETE)
				{
					snprintf(s_szWhy, sizeof(s_szWhy), "the depth resolve framebuffer is incomplete (%d bits, %d stencil)",
						bits, stencil);
					ok = false;
				}
				else
				{
					qgl.BlitFramebuffer(x, y, x + w, y + h, 0, 0, w, h, FGL_DEPTH_BUFFER_BIT, FGL_NEAREST);
				}
				if (!s_szDescribe[0] || remade)
					snprintf(s_szDescribe, sizeof(s_szDescribe), "per-pixel fog (GLSL), depth %d%s bits%s, %dx MSAA resolved by blit",
						bits, flt ? " float" : "", stencil ? " + stencil" : "", samples);
			}
			qgl.BindFramebuffer(FGL_DRAW_FRAMEBUFFER, (FGLuint)drawFbo);
			qgl.BindFramebuffer(FGL_READ_FRAMEBUFFER, (FGLuint)readFbo);
		}
	}
	else
	{
		// read where the view was drawn (the engine's framebuffer, or the window's)
		FGLint depthBits = 0;
		qgl.GetIntegerv(FGL_DEPTH_BITS, &depthBits);
		PrepareDepthTexture(w, h, depthBits == 16 ? FGL_DEPTH_COMPONENT16 : FGL_DEPTH_COMPONENT24, FGL_DEPTH_COMPONENT,
			FGL_UNSIGNED_INT);
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(FGL_READ_FRAMEBUFFER, (FGLuint)drawFbo);
		qgl.CopyTexSubImage2D(FGL_TEXTURE_2D, 0, 0, 0, x, y, w, h);
		if (qgl.fbo && readFbo != drawFbo)
			qgl.BindFramebuffer(FGL_READ_FRAMEBUFFER, (FGLuint)readFbo);
		if (!s_szDescribe[0])
			snprintf(s_szDescribe, sizeof(s_szDescribe), "per-pixel fog (GLSL), depth %d bits, copied (no MSAA)",
				depthBits);
	}

	if (check && ok)
	{
		const FGLenum e = DrainErrors();
		s_bResolveChecked = true;
		if (e)
		{
			snprintf(s_szWhy, sizeof(s_szWhy), "copying the view's depth failed here (GL error 0x%04X)", e);
			ok = false;
		}
	}

	if (scissor)
		qgl.Enable(FGL_SCISSOR_TEST);

	s_bCopiedThisView = ok;
	s_CopiedViewport[0] = x;
	s_CopiedViewport[1] = y;
	s_CopiedViewport[2] = w;
	s_CopiedViewport[3] = h;

	// a copy that can't work in this context won't later either: the far-face haze from now on
	if (!ok)
		s_bAvailable = false;
	return ok;
}
} // namespace

bool FogZoneGL_Begin(const float *eye)
{
	if (s_bBegun || !eye || !Available())
		return false;

	FGLint viewport[4] = { 0, 0, 0, 0 };
	qgl.GetIntegerv(FGL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return false;

	// --- read everything that is changed below
	qgl.GetIntegerv(FGL_CURRENT_PROGRAM, &s_Saved.program);
	qgl.GetIntegerv(FGL_ACTIVE_TEXTURE, &s_Saved.activeUnit);
	qgl.ActiveTexture(FGL_TEXTURE0);
	qgl.GetIntegerv(FGL_TEXTURE_BINDING_2D, &s_Saved.bound2D);
	qgl.GetIntegerv(FGL_POLYGON_MODE, s_Saved.polygonMode);
	qgl.GetIntegerv(FGL_BLEND_SRC, &s_Saved.blendSrc);
	qgl.GetIntegerv(FGL_BLEND_DST, &s_Saved.blendDst);
	qgl.GetBooleanv(FGL_DEPTH_WRITEMASK, &s_Saved.depthMask);
	qgl.GetBooleanv(FGL_COLOR_WRITEMASK, s_Saved.colorMask);
	s_Saved.depthTest = qgl.IsEnabled(FGL_DEPTH_TEST);
	s_Saved.blend = qgl.IsEnabled(FGL_BLEND);
	s_Saved.alphaTest = qgl.IsEnabled(FGL_ALPHA_TEST);
	s_Saved.cull = qgl.IsEnabled(FGL_CULL_FACE);
	s_Saved.stencil = qgl.IsEnabled(FGL_STENCIL_TEST);

	// --- the view's depth on unit 0
	if (!CopyDepth(viewport[0], viewport[1], viewport[2], viewport[3]))
	{
		qgl.BindTexture(FGL_TEXTURE_2D, (FGLuint)s_Saved.bound2D);
		qgl.ActiveTexture((FGLenum)s_Saved.activeUnit);
		return false;
	}

	// --- the pass's state
	qgl.UseProgram(s_Program);
	qgl.Uniform1i(s_uDepth, 0);
	qgl.Uniform4f(s_uViewport, (float)viewport[0], (float)viewport[1], (float)viewport[2], (float)viewport[3]);
	qgl.Uniform3f(s_uEye, eye[0], eye[1], eye[2]);
	if (s_Saved.depthTest)
		qgl.Disable(FGL_DEPTH_TEST);	// what is in front is in the depth copy
	if (s_Saved.alphaTest)
		qgl.Disable(FGL_ALPHA_TEST);
	if (s_Saved.cull)
		qgl.Disable(FGL_CULL_FACE);	// the far faces are picked by hand
	if (s_Saved.stencil)
		qgl.Disable(FGL_STENCIL_TEST);
	if (!s_Saved.blend)
		qgl.Enable(FGL_BLEND);
	qgl.BlendFunc(FGL_SRC_ALPHA, FGL_ONE_MINUS_SRC_ALPHA);
	qgl.DepthMask(0);
	qgl.ColorMask(1, 1, 1, 1);
	qgl.PolygonMode(FGL_FRONT, FGL_FILL);
	qgl.PolygonMode(FGL_BACK, FGL_FILL);

	for (int i = 0; i < 3; i++)
		s_vecEye[i] = eye[i];
	s_bBegun = true;
	return true;
}

void FogZoneGL_Zone(const float *mins, const float *maxs, const float *color, float density, float scale,
	const bool *open)
{
	if (!s_bBegun || density <= 0.0f || scale <= 0.0f)
		return;

	qgl.Uniform3f(s_uMins, mins[0], mins[1], mins[2]);
	qgl.Uniform3f(s_uMaxs, maxs[0], maxs[1], maxs[2]);
	qgl.Uniform4f(s_uColor, color[0], color[1], color[2], scale);
	qgl.Uniform1f(s_uDensity, density);
	// per face: 1 / the band's depth for an open face, 0 + 1 (full density) for a face against a wall
	float rf[2][3], add[2][3];
	for (int i = 0; i < 3; i++)
	{
		float feather = (maxs[i] - mins[i]) * 0.25f;
		if (feather > FOGZONE_FEATHER)
			feather = FOGZONE_FEATHER;
		if (feather < 1.0f)
			feather = 1.0f;
		for (int side = 0; side < 2; side++)
		{
			const bool bOpen = open && open[side * 3 + i];
			rf[side][i] = bOpen ? 1.0f / feather : 0.0f;
			add[side][i] = bOpen ? 0.0f : 1.0f;
		}
	}
	qgl.Uniform3f(s_uRfMin, rf[0][0], rf[0][1], rf[0][2]);
	qgl.Uniform3f(s_uRfMax, rf[1][0], rf[1][1], rf[1][2]);
	qgl.Uniform3f(s_uAddMin, add[0][0], add[0][1], add[0][2]);
	qgl.Uniform3f(s_uAddMax, add[1][0], add[1][1], add[1][2]);

	// the far faces: the eye is on the inner side of their plane (all six with the eye inside)
	qgl.Begin(FGL_QUADS);
	for (int iAxis = 0; iAxis < 3; iAxis++)
	{
		const int iU = (iAxis + 1) % 3, iV = (iAxis + 2) % 3;
		for (int iSide = 0; iSide < 2; iSide++)
		{
			if (iSide ? s_vecEye[iAxis] >= maxs[iAxis] : s_vecEye[iAxis] <= mins[iAxis])
				continue;

			static const int corner[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
			for (int c = 0; c < 4; c++)
			{
				float p[3];
				p[iAxis] = iSide ? maxs[iAxis] : mins[iAxis];
				p[iU] = corner[c][0] ? maxs[iU] : mins[iU];
				p[iV] = corner[c][1] ? maxs[iV] : mins[iV];
				qgl.Vertex3f(p[0], p[1], p[2]);
			}
		}
	}
	qgl.End();
}

void FogZoneGL_End(void)
{
	if (!s_bBegun)
		return;
	s_bBegun = false;

	qgl.PolygonMode(FGL_FRONT, (FGLenum)s_Saved.polygonMode[0]);
	qgl.PolygonMode(FGL_BACK, (FGLenum)s_Saved.polygonMode[1]);
	qgl.ColorMask(s_Saved.colorMask[0], s_Saved.colorMask[1], s_Saved.colorMask[2], s_Saved.colorMask[3]);
	qgl.DepthMask(s_Saved.depthMask);
	qgl.BlendFunc((FGLenum)s_Saved.blendSrc, (FGLenum)s_Saved.blendDst);
	if (!s_Saved.blend)
		qgl.Disable(FGL_BLEND);
	if (s_Saved.stencil)
		qgl.Enable(FGL_STENCIL_TEST);
	if (s_Saved.cull)
		qgl.Enable(FGL_CULL_FACE);
	if (s_Saved.alphaTest)
		qgl.Enable(FGL_ALPHA_TEST);
	if (s_Saved.depthTest)
		qgl.Enable(FGL_DEPTH_TEST);
	qgl.UseProgram((FGLuint)s_Saved.program);
	qgl.BindTexture(FGL_TEXTURE_2D, (FGLuint)s_Saved.bound2D);
	qgl.ActiveTexture((FGLenum)s_Saved.activeUnit);
}

void FogZoneGL_NewView(void)
{
	s_bCopiedThisView = false;
}

unsigned int FogZoneGL_CopyViewDepth(const int *viewport)
{
	if (s_bBegun || !viewport || viewport[2] <= 0 || viewport[3] <= 0 || !Available())
		return 0;
	// the fog zone pass copied this view's depth already, and nothing has written depth since
	if (s_bCopiedThisView && s_DepthTex && !memcmp(s_CopiedViewport, viewport, sizeof(s_CopiedViewport)))
	{
		qgl.BindTexture(FGL_TEXTURE_2D, s_DepthTex);
		return s_DepthTex;
	}
	return CopyDepth(viewport[0], viewport[1], viewport[2], viewport[3]) ? s_DepthTex : 0;
}

const char *FogZoneGL_Why(void)
{
	return s_szWhy;
}

const char *FogZoneGL_Describe(void)
{
	return s_szDescribe;
}

int FogZoneGL_Generation(void)
{
	return s_iGeneration;
}
