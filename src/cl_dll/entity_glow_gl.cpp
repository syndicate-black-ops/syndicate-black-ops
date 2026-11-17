//=========================================================
// SBO 2.1 env_entity_glow: the outline and halo pass (entity_glow_gl.h, docs/ENV_ENTITY_GLOW.md "How it is drawn").
//
// Source SDK 2013's CGlowObjectManager (game/client/glow_outline_effect.cpp, TF2's glows) in GLSL 1.20:
//  1. the targets are drawn in their glow color into off-screen masks the size of the view, one for the outline style
//     and one for the halo style (rgb: the color times its alpha, alpha 1: "the target is here"). Which pixels of a
//     target count depends on its mode: all (Always), the hidden ones (Only when occluded) or the seen ones (Only
//     when visible). Source used the depth test and the stencil buffer; here the program reads the view's depth
//     (copied by fogzone_gl.cpp, resolved when multisampled) and compares it with the target's eye distance;
//  2. halo: the mask is reduced to a quarter of the size and blurred (Source: dev/glow_downsample, glow_blur_x,
//     glow_blur_y; a Gaussian across 9 texels each way);
//  3. both are added to the view with one full-screen quad, never on a pixel of a target (Source: stencil test
//     "no glow object here"): the outline is the largest of 4 samples 2 pixels away diagonally (haloaddoutline_ps20),
//     the halo the blurred mask (haloadd_ps20); alpha = the brightest channel (pow 0.8 for the halo), blended
//     one / one minus source alpha, the outline over the halo, as Source's two passes.
//
// The GL functions are looked up in the current context as colorcorrection_gl.cpp and fogzone_gl.cpp do (no GL header
// or import library). No glPushAttrib: every value changed is read first and put back exactly; the engine caches its
// active texture unit, the textures bound on each unit and its GLSL program.
//=========================================================

#include "entity_glow_gl.h"
#include "fogzone_gl.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define EGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define EGL_APIENTRY
#endif

namespace
{
typedef unsigned int EGLenum_t;
typedef unsigned char EGLboolean_t;
typedef unsigned int EGLbitfield_t;
typedef int EGLint_t;
typedef int EGLsizei_t;
typedef float EGLfloat_t;
typedef unsigned int EGLuint_t;
typedef char EGLchar_t;

const EGLenum_t EGL_CULL_FACE = 0x0B44;
const EGLenum_t EGL_POLYGON_MODE = 0x0B40;
const EGLenum_t EGL_DEPTH_TEST = 0x0B71;
const EGLenum_t EGL_DEPTH_WRITEMASK = 0x0B72;
const EGLenum_t EGL_STENCIL_TEST = 0x0B90;
const EGLenum_t EGL_VIEWPORT = 0x0BA2;
const EGLenum_t EGL_ALPHA_TEST = 0x0BC0;
const EGLenum_t EGL_BLEND_DST = 0x0BE0;
const EGLenum_t EGL_BLEND_SRC = 0x0BE1;
const EGLenum_t EGL_BLEND = 0x0BE2;
const EGLenum_t EGL_SCISSOR_TEST = 0x0C11;
const EGLenum_t EGL_COLOR_CLEAR_VALUE = 0x0C22;
const EGLenum_t EGL_COLOR_WRITEMASK = 0x0C23;
const EGLenum_t EGL_UNPACK_ALIGNMENT = 0x0CF5;
const EGLenum_t EGL_TEXTURE_2D = 0x0DE1;
const EGLenum_t EGL_TEXTURE_BINDING_2D = 0x8069;
const EGLenum_t EGL_TEXTURE_MIN_FILTER = 0x2801;
const EGLenum_t EGL_TEXTURE_MAG_FILTER = 0x2800;
const EGLenum_t EGL_TEXTURE_WRAP_S = 0x2802;
const EGLenum_t EGL_TEXTURE_WRAP_T = 0x2803;
const EGLint_t EGL_LINEAR = 0x2601;
const EGLint_t EGL_CLAMP_TO_EDGE = 0x812F;
const EGLenum_t EGL_RGBA = 0x1908;
const EGLint_t EGL_RGBA8 = 0x8058;
const EGLenum_t EGL_UNSIGNED_BYTE = 0x1401;
const EGLenum_t EGL_ONE = 1;
const EGLenum_t EGL_ONE_MINUS_SRC_ALPHA = 0x0303;
const EGLenum_t EGL_QUADS = 0x0007;
const EGLenum_t EGL_FRONT = 0x0404;
const EGLenum_t EGL_BACK = 0x0405;
const EGLenum_t EGL_FILL = 0x1B02;
const EGLbitfield_t EGL_COLOR_BUFFER_BIT = 0x00004000;
const EGLenum_t EGL_TEXTURE0 = 0x84C0;
const EGLenum_t EGL_ACTIVE_TEXTURE = 0x84E0;
const EGLenum_t EGL_FRAGMENT_SHADER = 0x8B30;
const EGLenum_t EGL_VERTEX_SHADER = 0x8B31;
const EGLenum_t EGL_COMPILE_STATUS = 0x8B81;
const EGLenum_t EGL_LINK_STATUS = 0x8B82;
const EGLenum_t EGL_CURRENT_PROGRAM = 0x8B8D;
const EGLenum_t EGL_PIXEL_UNPACK_BUFFER = 0x88EC;
const EGLenum_t EGL_PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;
const EGLenum_t EGL_FRAMEBUFFER = 0x8D40;
const EGLenum_t EGL_READ_FRAMEBUFFER = 0x8CA8;
const EGLenum_t EGL_DRAW_FRAMEBUFFER = 0x8CA9;
const EGLenum_t EGL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
const EGLenum_t EGL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
const EGLenum_t EGL_COLOR_ATTACHMENT0 = 0x8CE0;
const EGLenum_t EGL_FRAMEBUFFER_COMPLETE = 0x8CD5;

struct
{
	// GL 1.1
	void (EGL_APIENTRY *Enable)(EGLenum_t cap);
	void (EGL_APIENTRY *Disable)(EGLenum_t cap);
	EGLboolean_t (EGL_APIENTRY *IsEnabled)(EGLenum_t cap);
	void (EGL_APIENTRY *GetIntegerv)(EGLenum_t pname, EGLint_t *params);
	void (EGL_APIENTRY *GetBooleanv)(EGLenum_t pname, EGLboolean_t *params);
	void (EGL_APIENTRY *GetFloatv)(EGLenum_t pname, EGLfloat_t *params);
	EGLenum_t (EGL_APIENTRY *GetError)(void);
	void (EGL_APIENTRY *Viewport)(EGLint_t x, EGLint_t y, EGLsizei_t w, EGLsizei_t h);
	void (EGL_APIENTRY *DepthMask)(EGLboolean_t flag);
	void (EGL_APIENTRY *ColorMask)(EGLboolean_t r, EGLboolean_t g, EGLboolean_t b, EGLboolean_t a);
	void (EGL_APIENTRY *BlendFunc)(EGLenum_t sfactor, EGLenum_t dfactor);
	void (EGL_APIENTRY *PolygonMode)(EGLenum_t face, EGLenum_t mode);
	void (EGL_APIENTRY *PixelStorei)(EGLenum_t pname, EGLint_t param);
	void (EGL_APIENTRY *ClearColor)(EGLfloat_t r, EGLfloat_t g, EGLfloat_t b, EGLfloat_t a);
	void (EGL_APIENTRY *Clear)(EGLbitfield_t mask);
	void (EGL_APIENTRY *GenTextures)(EGLsizei_t n, EGLuint_t *textures);
	void (EGL_APIENTRY *BindTexture)(EGLenum_t target, EGLuint_t texture);
	void (EGL_APIENTRY *TexParameteri)(EGLenum_t target, EGLenum_t pname, EGLint_t param);
	void (EGL_APIENTRY *TexImage2D)(EGLenum_t target, EGLint_t level, EGLint_t internalformat, EGLsizei_t width,
		EGLsizei_t height, EGLint_t border, EGLenum_t format, EGLenum_t type, const void *pixels);
	void (EGL_APIENTRY *Begin)(EGLenum_t mode);
	void (EGL_APIENTRY *End)(void);
	void (EGL_APIENTRY *Vertex2f)(EGLfloat_t x, EGLfloat_t y);
	void (EGL_APIENTRY *Vertex3fv)(const EGLfloat_t *v);
	// GL 1.3 / 1.5
	void (EGL_APIENTRY *ActiveTexture)(EGLenum_t texture);
	void (EGL_APIENTRY *BindBuffer)(EGLenum_t target, EGLuint_t buffer);	// optional
	// vertex arrays (optional: without them the targets are drawn with glBegin / glVertex)
	void (EGL_APIENTRY *EnableClientState)(EGLenum_t array);
	void (EGL_APIENTRY *DisableClientState)(EGLenum_t array);
	void (EGL_APIENTRY *ClientActiveTexture)(EGLenum_t texture);
	void (EGL_APIENTRY *VertexPointer)(EGLint_t size, EGLenum_t type, EGLsizei_t stride, const void *pointer);
	void (EGL_APIENTRY *GetPointerv)(EGLenum_t pname, void **params);
	void (EGL_APIENTRY *DrawElements)(EGLenum_t mode, EGLsizei_t count, EGLenum_t type, const void *indices);
	void (EGL_APIENTRY *GetVertexAttribiv)(EGLuint_t index, EGLenum_t pname, EGLint_t *params);
	void (EGL_APIENTRY *EnableVertexAttribArray)(EGLuint_t index);
	void (EGL_APIENTRY *DisableVertexAttribArray)(EGLuint_t index);
	void (EGL_APIENTRY *BindVertexArray)(EGLuint_t array);	// optional (GL 3.0)
	bool arrays;
	// GL 2.0
	EGLuint_t (EGL_APIENTRY *CreateShader)(EGLenum_t type);
	void (EGL_APIENTRY *ShaderSource)(EGLuint_t shader, EGLsizei_t count, const EGLchar_t *const *string,
		const EGLint_t *length);
	void (EGL_APIENTRY *CompileShader)(EGLuint_t shader);
	void (EGL_APIENTRY *GetShaderiv)(EGLuint_t shader, EGLenum_t pname, EGLint_t *params);
	void (EGL_APIENTRY *GetShaderInfoLog)(EGLuint_t shader, EGLsizei_t bufSize, EGLsizei_t *length, EGLchar_t *infoLog);
	void (EGL_APIENTRY *DeleteShader)(EGLuint_t shader);
	EGLuint_t (EGL_APIENTRY *CreateProgram)(void);
	void (EGL_APIENTRY *AttachShader)(EGLuint_t program, EGLuint_t shader);
	void (EGL_APIENTRY *LinkProgram)(EGLuint_t program);
	void (EGL_APIENTRY *GetProgramiv)(EGLuint_t program, EGLenum_t pname, EGLint_t *params);
	void (EGL_APIENTRY *GetProgramInfoLog)(EGLuint_t program, EGLsizei_t bufSize, EGLsizei_t *length,
		EGLchar_t *infoLog);
	void (EGL_APIENTRY *UseProgram)(EGLuint_t program);
	EGLint_t (EGL_APIENTRY *GetUniformLocation)(EGLuint_t program, const EGLchar_t *name);
	void (EGL_APIENTRY *Uniform1i)(EGLint_t location, EGLint_t v0);
	void (EGL_APIENTRY *Uniform1f)(EGLint_t location, EGLfloat_t v0);
	void (EGL_APIENTRY *Uniform2f)(EGLint_t location, EGLfloat_t v0, EGLfloat_t v1);
	void (EGL_APIENTRY *Uniform4f)(EGLint_t location, EGLfloat_t v0, EGLfloat_t v1, EGLfloat_t v2, EGLfloat_t v3);
	// GL 3.0 framebuffers (needed: the masks are textures drawn into)
	void (EGL_APIENTRY *BindFramebuffer)(EGLenum_t target, EGLuint_t framebuffer);
	void (EGL_APIENTRY *GenFramebuffers)(EGLsizei_t n, EGLuint_t *framebuffers);
	void (EGL_APIENTRY *FramebufferTexture2D)(EGLenum_t target, EGLenum_t attachment, EGLenum_t textarget,
		EGLuint_t texture, EGLint_t level);
	EGLenum_t (EGL_APIENTRY *CheckFramebufferStatus)(EGLenum_t target);
} qgl;

void *s_pContext = NULL;	// the context the pointers were looked up in
bool s_bAvailable = false;
int s_iGeneration = 0;
char s_szWhy[256] = "no GL context";

// the programs and their uniforms
struct Program
{
	EGLuint_t id;
	EGLint_t u[8];
};
enum { PROG_MASK, PROG_DOWN, PROG_BLUR, PROG_COMPOSITE, NUM_PROGS };
Program s_Prog[NUM_PROGS];

// the masks (full size: outline, halo) and the halo's quarter-size pair (q0 the result, q1 between the two blurs)
enum { TEX_OUTLINE, TEX_HALO, TEX_Q0, TEX_Q1, NUM_TEX };
EGLuint_t s_Tex[NUM_TEX];
EGLuint_t s_Fbo[NUM_TEX];
int s_iW = 0, s_iH = 0, s_iQW = 0, s_iQH = 0;	// the size the textures were made in

// Halo: the blur's step in quarter-size texels per tap and how much the blurred mask is brightened (Source's
// glow_outline_width 6 multiplies its blur; its kernel is narrower). Outline: Source's 2 pixels at 1080 lines and
// below, wider on taller views so it keeps its look.
const float HALO_SPREAD = 1.5f;
const float HALO_AMOUNT = 3.0f;
const float OUTLINE_PIXELS = 2.0f;

// what Begin changed, put back by End
struct
{
	EGLint_t program, activeUnit, bound2D[3], polygonMode[2], blendSrc, blendDst, viewport[4], drawFbo, readFbo;
	EGLboolean_t depthMask, colorMask[4];
	EGLboolean_t depthTest, blend, alphaTest, cull, stencil, scissor;
	EGLfloat_t clearColor[4];
} s_Saved;

// The vertex array state the targets' draws change (set on the first draw of a pass, put back by Restore)
const EGLenum_t EGL_VERTEX_ARRAY = 0x8074;
const EGLenum_t EGL_VERTEX_ARRAY_SIZE = 0x807A;
const EGLenum_t EGL_VERTEX_ARRAY_TYPE = 0x807B;
const EGLenum_t EGL_VERTEX_ARRAY_STRIDE = 0x807C;
const EGLenum_t EGL_VERTEX_ARRAY_POINTER = 0x808E;
const EGLenum_t EGL_CLIENT_ACTIVE_TEXTURE = 0x84E1;
const EGLenum_t EGL_ARRAY_BUFFER = 0x8892;
const EGLenum_t EGL_ELEMENT_ARRAY_BUFFER = 0x8893;
const EGLenum_t EGL_ARRAY_BUFFER_BINDING = 0x8894;
const EGLenum_t EGL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
const EGLenum_t EGL_VERTEX_ARRAY_BINDING = 0x85B5;
const EGLenum_t EGL_VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622;
const EGLenum_t EGL_MAX_VERTEX_ATTRIBS = 0x8869;
const EGLenum_t EGL_TRIANGLES = 0x0004;
const EGLenum_t EGL_UNSIGNED_SHORT = 0x1403;
const EGLenum_t EGL_FLOAT = 0x1406;
// the classic arrays besides the vertex array: normal, color, index, edge flag, fog coordinate, secondary color
const EGLenum_t s_OtherArrays[] = { 0x8075, 0x8076, 0x8077, 0x8079, 0x8457, 0x845E };
const int ARRAYS_UNITS = 4;		// texture coordinate arrays of client units 0-3
const int ARRAYS_ATTRIBS = 16;	// generic attribute arrays checked
const EGLenum_t EGL_TEXTURE_COORD_ARRAY = 0x8078;
struct
{
	bool on;
	EGLint_t vao, arrayBuffer, elementBuffer, clientUnit, size, type, stride;
	void *pointer;
	EGLboolean_t vertex, other[6], texcoord[ARRAYS_UNITS];
	EGLint_t attrib[ARRAYS_ATTRIBS];
	int attribs;
} s_Arrays;

void ArraysOn()
{
	if (s_Arrays.on)
		return;
	s_Arrays.on = true;
	s_Arrays.vao = 0;
	if (qgl.BindVertexArray)
	{
		qgl.GetIntegerv(EGL_VERTEX_ARRAY_BINDING, &s_Arrays.vao);
		if (s_Arrays.vao)
			qgl.BindVertexArray(0);
	}
	qgl.GetIntegerv(EGL_ARRAY_BUFFER_BINDING, &s_Arrays.arrayBuffer);
	qgl.GetIntegerv(EGL_ELEMENT_ARRAY_BUFFER_BINDING, &s_Arrays.elementBuffer);
	if (s_Arrays.arrayBuffer)
		qgl.BindBuffer(EGL_ARRAY_BUFFER, 0);
	if (s_Arrays.elementBuffer)
		qgl.BindBuffer(EGL_ELEMENT_ARRAY_BUFFER, 0);
	qgl.GetIntegerv(EGL_VERTEX_ARRAY_SIZE, &s_Arrays.size);
	qgl.GetIntegerv(EGL_VERTEX_ARRAY_TYPE, &s_Arrays.type);
	qgl.GetIntegerv(EGL_VERTEX_ARRAY_STRIDE, &s_Arrays.stride);
	s_Arrays.pointer = NULL;
	qgl.GetPointerv(EGL_VERTEX_ARRAY_POINTER, &s_Arrays.pointer);
	s_Arrays.vertex = qgl.IsEnabled(EGL_VERTEX_ARRAY);
	for (int i = 0; i < 6; i++)
	{
		s_Arrays.other[i] = qgl.IsEnabled(s_OtherArrays[i]);
		if (s_Arrays.other[i])
			qgl.DisableClientState(s_OtherArrays[i]);
	}
	qgl.GetIntegerv(EGL_CLIENT_ACTIVE_TEXTURE, &s_Arrays.clientUnit);
	for (int i = 0; i < ARRAYS_UNITS; i++)
	{
		qgl.ClientActiveTexture(EGL_TEXTURE0 + i);
		s_Arrays.texcoord[i] = qgl.IsEnabled(EGL_TEXTURE_COORD_ARRAY);
		if (s_Arrays.texcoord[i])
			qgl.DisableClientState(EGL_TEXTURE_COORD_ARRAY);
	}
	qgl.ClientActiveTexture((EGLenum_t)s_Arrays.clientUnit);
	EGLint_t maxAttribs = 0;
	qgl.GetIntegerv(EGL_MAX_VERTEX_ATTRIBS, &maxAttribs);
	s_Arrays.attribs = maxAttribs < ARRAYS_ATTRIBS ? maxAttribs : ARRAYS_ATTRIBS;
	for (int i = 0; i < s_Arrays.attribs; i++)
	{
		s_Arrays.attrib[i] = 0;
		qgl.GetVertexAttribiv((EGLuint_t)i, EGL_VERTEX_ATTRIB_ARRAY_ENABLED, &s_Arrays.attrib[i]);
		if (s_Arrays.attrib[i])
			qgl.DisableVertexAttribArray((EGLuint_t)i);
	}
	if (!s_Arrays.vertex)
		qgl.EnableClientState(EGL_VERTEX_ARRAY);
}

void ArraysOff()
{
	if (!s_Arrays.on)
		return;
	s_Arrays.on = false;
	qgl.VertexPointer(s_Arrays.size, (EGLenum_t)s_Arrays.type, s_Arrays.stride, s_Arrays.pointer);
	if (!s_Arrays.vertex)
		qgl.DisableClientState(EGL_VERTEX_ARRAY);
	for (int i = 0; i < s_Arrays.attribs; i++)
	{
		if (s_Arrays.attrib[i])
			qgl.EnableVertexAttribArray((EGLuint_t)i);
	}
	for (int i = 0; i < ARRAYS_UNITS; i++)
	{
		if (s_Arrays.texcoord[i])
		{
			qgl.ClientActiveTexture(EGL_TEXTURE0 + i);
			qgl.EnableClientState(EGL_TEXTURE_COORD_ARRAY);
		}
	}
	qgl.ClientActiveTexture((EGLenum_t)s_Arrays.clientUnit);
	for (int i = 0; i < 6; i++)
	{
		if (s_Arrays.other[i])
			qgl.EnableClientState(s_OtherArrays[i]);
	}
	if (s_Arrays.arrayBuffer)
		qgl.BindBuffer(EGL_ARRAY_BUFFER, (EGLuint_t)s_Arrays.arrayBuffer);
	if (s_Arrays.elementBuffer)
		qgl.BindBuffer(EGL_ELEMENT_ARRAY_BUFFER, (EGLuint_t)s_Arrays.elementBuffer);
	if (qgl.BindVertexArray && s_Arrays.vao)
		qgl.BindVertexArray((EGLuint_t)s_Arrays.vao);
}
bool s_bBegun = false;
// GL errors: the first passes of each context are checked (the engine's errors drained before, ours read after)
int s_iChecks = 0;
unsigned int s_uError = 0;
const int ENTGLOW_ERROR_CHECKS = 600;
bool s_bUsed[2];		// a mask got a target this view (outline, halo)
int s_iBoundMask = -1;	// the mask framebuffer bound now
int s_iMode = 0, s_iPass = 0;

// --- shaders (GLSL 1.20, as fogzone_gl.cpp) ---

const char *const MASK_VS =
	"#version 120\n"
	"varying float v_dist;\n"
	"void main()\n"
	"{\n"
	"	v_dist = -( gl_ModelViewMatrix * gl_Vertex ).z;\n"
	"	gl_Position = ftransform();\n"
	"}\n";

// u_mode 0: every pixel; 1: the hidden ones; 2: the seen ones; 3: the seen ones cleared (pass 1 of mode 1). Hidden:
// the view's depth there is nearer than the target, by more than a margin for the depth buffer's steps and for the
// engine's own transform of the same vertices.
const char *const MASK_FS =
	"#version 120\n"
	"uniform sampler2D u_depth;\n"
	"uniform vec2 u_size;\n"
	"uniform vec4 u_color;\n"
	"uniform int u_mode;\n"
	"varying float v_dist;\n"
	"void main()\n"
	"{\n"
	"	if ( u_mode != 0 )\n"
	"	{\n"
	"		float d = texture2D( u_depth, gl_FragCoord.xy / u_size ).r;\n"
	"		float ndc = ( d - gl_DepthRange.near ) / gl_DepthRange.diff * 2.0 - 1.0;\n"
	"		float scene = gl_ProjectionMatrix[3][2] / ( ndc + gl_ProjectionMatrix[2][2] );\n"
	"		bool hidden = v_dist > scene + 1.0 + v_dist * 0.003;\n"
	"		if ( u_mode == 1 && !hidden ) discard;\n"
	"		if ( u_mode >= 2 && hidden ) discard;\n"
	"		if ( u_mode == 3 ) { gl_FragColor = vec4( 0.0 ); return; }\n"
	"	}\n"
	"	gl_FragColor = vec4( u_color.rgb * u_color.a, 1.0 );\n"
	"}\n";

const char *const SCREEN_VS =
	"#version 120\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4( gl_Vertex.xy, 0.0, 1.0 );\n"
	"}\n";

// full size -> a quarter: 4 bilinear taps, the average of the 4 x 4 texels under the quarter-size pixel
const char *const DOWN_FS =
	"#version 120\n"
	"uniform sampler2D u_src;\n"
	"uniform vec2 u_px;\n"	// 1 / the source's size
	"void main()\n"
	"{\n"
	"	vec2 uv = gl_FragCoord.xy * 4.0 * u_px;\n"
	"	vec3 c = texture2D( u_src, uv + vec2( -1.0, -1.0 ) * u_px ).rgb;\n"
	"	c += texture2D( u_src, uv + vec2( 1.0, -1.0 ) * u_px ).rgb;\n"
	"	c += texture2D( u_src, uv + vec2( -1.0, 1.0 ) * u_px ).rgb;\n"
	"	c += texture2D( u_src, uv + vec2( 1.0, 1.0 ) * u_px ).rgb;\n"
	"	gl_FragColor = vec4( c * 0.25, 1.0 );\n"
	"}\n";

// one direction of the Gaussian (9 texels in 5 bilinear taps), times u_scale
const char *const BLUR_FS =
	"#version 120\n"
	"uniform sampler2D u_src;\n"
	"uniform vec2 u_px;\n"		// 1 / the size
	"uniform vec2 u_dir;\n"		// the step, in uv
	"uniform float u_scale;\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = gl_FragCoord.xy * u_px;\n"
	"	vec3 c = texture2D( u_src, uv ).rgb * 0.2270270270;\n"
	"	c += ( texture2D( u_src, uv + u_dir * 1.3846153846 ).rgb + texture2D( u_src, uv - u_dir * 1.3846153846 ).rgb ) * 0.3162162162;\n"
	"	c += ( texture2D( u_src, uv + u_dir * 3.2307692308 ).rgb + texture2D( u_src, uv - u_dir * 3.2307692308 ).rgb ) * 0.0702702703;\n"
	"	gl_FragColor = vec4( min( c * u_scale, vec3( 1.0 ) ), 1.0 );\n"
	"}\n";

// Smoothed (the masks are drawn without multisampling): "inside" is the targets' coverage averaged over the 3 x 3
// pixels around (4 bilinear taps half a pixel off), so the glow fades in over the target's edge pixel instead of
// stopping on a stair step; the outline is the largest of 8 bilinear samples in a ring, averaged over two rings half a
// pixel inside and outside its width, so its outer edge fades over a pixel too.
const char *const COMPOSITE_FS =
	"#version 120\n"
	"uniform sampler2D u_outline;\n"
	"uniform sampler2D u_halomask;\n"
	"uniform sampler2D u_halo;\n"
	"uniform vec4 u_viewport;\n"
	"uniform vec2 u_px;\n"			// one pixel, in uv
	"uniform vec2 u_use;\n"			// outline, halo in use
	"uniform float u_width;\n"		// the outline's width, in pixels
	"float Cover( vec2 uv )\n"
	"{\n"
	"	float c = 0.0;\n"
	"	if ( u_use.x > 0.5 ) c = texture2D( u_outline, uv ).a;\n"
	"	if ( u_use.y > 0.5 ) c = max( c, texture2D( u_halomask, uv ).a );\n"
	"	return c;\n"
	"}\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = ( gl_FragCoord.xy - u_viewport.xy ) / u_viewport.zw;\n"
	"	vec2 hp = 0.5 * u_px;\n"
	"	float inside = 0.25 * ( Cover( uv + hp ) + Cover( uv - hp ) + Cover( uv + vec2( hp.x, -hp.y ) )\n"
	"		+ Cover( uv + vec2( -hp.x, hp.y ) ) );\n"
	"	if ( inside >= 0.999 ) discard;\n"
	"	vec4 o = vec4( 0.0 );\n"
	"	if ( u_use.x > 0.5 )\n"
	"	{\n"
	"		vec4 m = vec4( 0.0 );\n"
	"		float ring = 0.0;\n"
	"		for ( int r = 0; r < 2; r++ )\n"
	"		{\n"
	"			float rad = u_width + ( float( r ) - 0.5 );\n"
	"			float best = 0.0;\n"
	"			for ( int i = 0; i < 8; i++ )\n"
	"			{\n"
	"				float an = float( i ) * 0.785398;\n"
	"				vec4 t = texture2D( u_outline, uv + vec2( cos( an ), sin( an ) ) * rad * u_px );\n"
	"				best = max( best, t.a );\n"
	"				m = max( m, t );\n"
	"			}\n"
	"			ring += 0.5 * best;\n"
	"		}\n"
	"		vec3 col = min( m.rgb / max( m.a, 0.001 ), vec3( 1.0 ) );\n"
	"		float a = ring * ( 1.0 - inside );\n"
	"		o.rgb = col * a;\n"
	"		o.a = max( o.r, max( o.g, o.b ) );\n"
	"	}\n"
	"	vec4 h = vec4( 0.0 );\n"
	"	if ( u_use.y > 0.5 )\n"
	"	{\n"
	"		h.rgb = texture2D( u_halo, uv ).rgb * ( 1.0 - inside );\n"
	"		h.a = pow( max( h.r, max( h.g, h.b ) ), 0.8 );\n"
	"	}\n"
	"	if ( o.a + h.a <= 0.0 ) discard;\n"
	"	gl_FragColor = vec4( o.rgb + ( 1.0 - o.a ) * h.rgb, 1.0 - ( 1.0 - o.a ) * ( 1.0 - h.a ) );\n"
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

EGLuint_t Compile(EGLenum_t type, const char *source)
{
	EGLuint_t shader = qgl.CreateShader(type);
	if (!shader)
		return 0;
	qgl.ShaderSource(shader, 1, &source, NULL);
	qgl.CompileShader(shader);
	EGLint_t ok = 0;
	qgl.GetShaderiv(shader, EGL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "a %s shader did not compile: %s",
			type == EGL_VERTEX_SHADER ? "vertex" : "fragment", log);
		qgl.DeleteShader(shader);
		return 0;
	}
	return shader;
}

bool Build(Program &prog, const char *vsSource, const char *fsSource, const char *const *uniforms, int count)
{
	memset(&prog, 0, sizeof(prog));
	EGLuint_t vs = Compile(EGL_VERTEX_SHADER, vsSource);
	if (!vs)
		return false;
	EGLuint_t fs = Compile(EGL_FRAGMENT_SHADER, fsSource);
	if (!fs)
	{
		qgl.DeleteShader(vs);
		return false;
	}
	EGLuint_t program = qgl.CreateProgram();
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
	EGLint_t ok = 0;
	qgl.GetProgramiv(program, EGL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[160] = "";
		qgl.GetProgramInfoLog(program, sizeof(log), NULL, log);
		snprintf(s_szWhy, sizeof(s_szWhy), "a shader program did not link: %s", log);
		return false;
	}
	prog.id = program;
	for (int i = 0; i < count && i < 8; i++)
		prog.u[i] = qgl.GetUniformLocation(program, uniforms[i]);
	return true;
}

// uniform slots of each program (the order of the names below)
enum { MU_DEPTH, MU_SIZE, MU_COLOR, MU_MODE };
enum { DU_SRC, DU_PX };
enum { BU_SRC, BU_PX, BU_DIR, BU_SCALE };
enum { CU_OUTLINE, CU_HALOMASK, CU_HALO, CU_VIEWPORT, CU_PX, CU_USE, CU_WIDTH };

bool BuildPrograms()
{
	static const char *const mask[] = { "u_depth", "u_size", "u_color", "u_mode" };
	static const char *const down[] = { "u_src", "u_px" };
	static const char *const blur[] = { "u_src", "u_px", "u_dir", "u_scale" };
	static const char *const comp[] = { "u_outline", "u_halomask", "u_halo", "u_viewport", "u_px", "u_use", "u_width" };
	return Build(s_Prog[PROG_MASK], MASK_VS, MASK_FS, mask, 4) &&
	       Build(s_Prog[PROG_DOWN], SCREEN_VS, DOWN_FS, down, 2) &&
	       Build(s_Prog[PROG_BLUR], SCREEN_VS, BLUR_FS, blur, 4) &&
	       Build(s_Prog[PROG_COMPOSITE], SCREEN_VS, COMPOSITE_FS, comp, 7);
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
	memset(s_Prog, 0, sizeof(s_Prog));
	memset(s_Tex, 0, sizeof(s_Tex));
	memset(s_Fbo, 0, sizeof(s_Fbo));
	s_iW = s_iH = s_iQW = s_iQH = 0;
	s_iChecks = 0;
	s_pszMissing = NULL;
	s_bAvailable =
		Resolve(qgl.Enable, "glEnable") &&
		Resolve(qgl.Disable, "glDisable") &&
		Resolve(qgl.IsEnabled, "glIsEnabled") &&
		Resolve(qgl.GetIntegerv, "glGetIntegerv") &&
		Resolve(qgl.GetBooleanv, "glGetBooleanv") &&
		Resolve(qgl.GetFloatv, "glGetFloatv") &&
		Resolve(qgl.GetError, "glGetError") &&
		Resolve(qgl.Viewport, "glViewport") &&
		Resolve(qgl.DepthMask, "glDepthMask") &&
		Resolve(qgl.ColorMask, "glColorMask") &&
		Resolve(qgl.BlendFunc, "glBlendFunc") &&
		Resolve(qgl.PolygonMode, "glPolygonMode") &&
		Resolve(qgl.PixelStorei, "glPixelStorei") &&
		Resolve(qgl.ClearColor, "glClearColor") &&
		Resolve(qgl.Clear, "glClear") &&
		Resolve(qgl.GenTextures, "glGenTextures") &&
		Resolve(qgl.BindTexture, "glBindTexture") &&
		Resolve(qgl.TexParameteri, "glTexParameteri") &&
		Resolve(qgl.TexImage2D, "glTexImage2D") &&
		Resolve(qgl.Begin, "glBegin") &&
		Resolve(qgl.End, "glEnd") &&
		Resolve(qgl.Vertex2f, "glVertex2f") &&
		Resolve(qgl.Vertex3fv, "glVertex3fv") &&
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
		Resolve(qgl.Uniform2f, "glUniform2f") &&
		Resolve(qgl.Uniform4f, "glUniform4f") &&
		Resolve(qgl.BindFramebuffer, "glBindFramebuffer") &&
		Resolve(qgl.GenFramebuffers, "glGenFramebuffers") &&
		Resolve(qgl.FramebufferTexture2D, "glFramebufferTexture2D") &&
		Resolve(qgl.CheckFramebufferStatus, "glCheckFramebufferStatus");
	if (!s_bAvailable)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "the driver has no %s (needs OpenGL 3.0 framebuffers and GLSL 1.20)",
			s_pszMissing ? s_pszMissing : "?");
		return false;
	}
	qgl.BindBuffer = (decltype(qgl.BindBuffer))GetProc("glBindBuffer");
	{
		const char *save = s_pszMissing;
		qgl.arrays = qgl.BindBuffer && Resolve(qgl.EnableClientState, "glEnableClientState") &&
		             Resolve(qgl.DisableClientState, "glDisableClientState") &&
		             Resolve(qgl.ClientActiveTexture, "glClientActiveTexture") &&
		             Resolve(qgl.VertexPointer, "glVertexPointer") && Resolve(qgl.GetPointerv, "glGetPointerv") &&
		             Resolve(qgl.DrawElements, "glDrawElements") &&
		             Resolve(qgl.GetVertexAttribiv, "glGetVertexAttribiv") &&
		             Resolve(qgl.EnableVertexAttribArray, "glEnableVertexAttribArray") &&
		             Resolve(qgl.DisableVertexAttribArray, "glDisableVertexAttribArray");
		qgl.BindVertexArray = (decltype(qgl.BindVertexArray))GetProc("glBindVertexArray");
		s_pszMissing = save;
	}

	EGLint_t program = 0;
	qgl.GetIntegerv(EGL_CURRENT_PROGRAM, &program);
	s_bAvailable = BuildPrograms();
	qgl.UseProgram((EGLuint_t)program);
	if (!s_bAvailable)
		return false;
	s_szWhy[0] = 0;
	return true;
}

// The masks in the view's size (bound on the active unit while made). False: a framebuffer is incomplete here.
bool PrepareTextures(int w, int h)
{
	const int qw = (w + 3) / 4, qh = (h + 3) / 4;
	if (s_Tex[0] && s_iW == w && s_iH == h)
		return true;

	EGLint_t alignment = 4, unpackBuffer = 0;
	qgl.GetIntegerv(EGL_UNPACK_ALIGNMENT, &alignment);
	if (qgl.BindBuffer)
	{
		qgl.GetIntegerv(EGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
		if (unpackBuffer)
			qgl.BindBuffer(EGL_PIXEL_UNPACK_BUFFER, 0);
	}
	qgl.PixelStorei(EGL_UNPACK_ALIGNMENT, 4);

	bool ok = true;
	for (int i = 0; i < NUM_TEX; i++)
	{
		if (!s_Tex[i])
			qgl.GenTextures(1, &s_Tex[i]);
		if (!s_Fbo[i])
			qgl.GenFramebuffers(1, &s_Fbo[i]);
		const bool quarter = (i == TEX_Q0 || i == TEX_Q1);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[i]);
		qgl.TexParameteri(EGL_TEXTURE_2D, EGL_TEXTURE_MIN_FILTER, EGL_LINEAR);
		qgl.TexParameteri(EGL_TEXTURE_2D, EGL_TEXTURE_MAG_FILTER, EGL_LINEAR);
		qgl.TexParameteri(EGL_TEXTURE_2D, EGL_TEXTURE_WRAP_S, EGL_CLAMP_TO_EDGE);
		qgl.TexParameteri(EGL_TEXTURE_2D, EGL_TEXTURE_WRAP_T, EGL_CLAMP_TO_EDGE);
		qgl.TexImage2D(EGL_TEXTURE_2D, 0, EGL_RGBA8, quarter ? qw : w, quarter ? qh : h, 0, EGL_RGBA,
			EGL_UNSIGNED_BYTE, NULL);
		qgl.BindFramebuffer(EGL_FRAMEBUFFER, s_Fbo[i]);
		qgl.FramebufferTexture2D(EGL_FRAMEBUFFER, EGL_COLOR_ATTACHMENT0, EGL_TEXTURE_2D, s_Tex[i], 0);
		if (qgl.CheckFramebufferStatus(EGL_FRAMEBUFFER) != EGL_FRAMEBUFFER_COMPLETE)
			ok = false;
	}

	qgl.PixelStorei(EGL_UNPACK_ALIGNMENT, alignment);
	if (qgl.BindBuffer && unpackBuffer)
		qgl.BindBuffer(EGL_PIXEL_UNPACK_BUFFER, (EGLuint_t)unpackBuffer);

	if (!ok)
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "an RGBA8 mask framebuffer is incomplete in this context");
		s_bAvailable = false;	// it won't be later either
		return false;
	}
	s_iW = w;
	s_iH = h;
	s_iQW = qw;
	s_iQH = qh;
	return true;
}

void BindMask(int style)
{
	if (s_iBoundMask == style)
		return;
	qgl.BindFramebuffer(EGL_FRAMEBUFFER, s_Fbo[style]);
	s_iBoundMask = style;
	if (!s_bUsed[style])
	{
		s_bUsed[style] = true;
		qgl.Clear(EGL_COLOR_BUFFER_BIT);
	}
}

void ScreenQuad()
{
	qgl.Begin(EGL_QUADS);
	qgl.Vertex2f(-1.0f, -1.0f);
	qgl.Vertex2f(1.0f, -1.0f);
	qgl.Vertex2f(1.0f, 1.0f);
	qgl.Vertex2f(-1.0f, 1.0f);
	qgl.End();
}

void Restore()
{
	ArraysOff();
	qgl.BindFramebuffer(EGL_DRAW_FRAMEBUFFER, (EGLuint_t)s_Saved.drawFbo);
	qgl.BindFramebuffer(EGL_READ_FRAMEBUFFER, (EGLuint_t)s_Saved.readFbo);
	qgl.Viewport(s_Saved.viewport[0], s_Saved.viewport[1], s_Saved.viewport[2], s_Saved.viewport[3]);
	qgl.ClearColor(s_Saved.clearColor[0], s_Saved.clearColor[1], s_Saved.clearColor[2], s_Saved.clearColor[3]);
	qgl.PolygonMode(EGL_FRONT, (EGLenum_t)s_Saved.polygonMode[0]);
	qgl.PolygonMode(EGL_BACK, (EGLenum_t)s_Saved.polygonMode[1]);
	qgl.ColorMask(s_Saved.colorMask[0], s_Saved.colorMask[1], s_Saved.colorMask[2], s_Saved.colorMask[3]);
	qgl.DepthMask(s_Saved.depthMask);
	qgl.BlendFunc((EGLenum_t)s_Saved.blendSrc, (EGLenum_t)s_Saved.blendDst);
	if (s_Saved.blend)
		qgl.Enable(EGL_BLEND);
	else
		qgl.Disable(EGL_BLEND);
	if (s_Saved.scissor)
		qgl.Enable(EGL_SCISSOR_TEST);
	if (s_Saved.stencil)
		qgl.Enable(EGL_STENCIL_TEST);
	if (s_Saved.cull)
		qgl.Enable(EGL_CULL_FACE);
	if (s_Saved.alphaTest)
		qgl.Enable(EGL_ALPHA_TEST);
	if (s_Saved.depthTest)
		qgl.Enable(EGL_DEPTH_TEST);
	qgl.UseProgram((EGLuint_t)s_Saved.program);
	for (int i = 2; i >= 0; i--)
	{
		qgl.ActiveTexture(EGL_TEXTURE0 + i);
		qgl.BindTexture(EGL_TEXTURE_2D, (EGLuint_t)s_Saved.bound2D[i]);
	}
	qgl.ActiveTexture((EGLenum_t)s_Saved.activeUnit);
}
} // namespace

bool EntityGlowGL_Begin(void)
{
	if (s_bBegun || !Available())
		return false;

	EGLint_t viewport[4] = { 0, 0, 0, 0 };
	qgl.GetIntegerv(EGL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return false;

	if (s_iChecks < ENTGLOW_ERROR_CHECKS)
	{
		for (int i = 0; i < 8 && qgl.GetError(); i++)
			;	// the engine's, not ours
	}

	// --- read everything that is changed below
	qgl.GetIntegerv(EGL_CURRENT_PROGRAM, &s_Saved.program);
	qgl.GetIntegerv(EGL_ACTIVE_TEXTURE, &s_Saved.activeUnit);
	for (int i = 0; i < 3; i++)
	{
		qgl.ActiveTexture(EGL_TEXTURE0 + i);
		qgl.GetIntegerv(EGL_TEXTURE_BINDING_2D, &s_Saved.bound2D[i]);
	}
	qgl.ActiveTexture(EGL_TEXTURE0);
	for (int i = 0; i < 4; i++)
		s_Saved.viewport[i] = viewport[i];
	qgl.GetIntegerv(EGL_DRAW_FRAMEBUFFER_BINDING, &s_Saved.drawFbo);
	qgl.GetIntegerv(EGL_READ_FRAMEBUFFER_BINDING, &s_Saved.readFbo);
	qgl.GetIntegerv(EGL_POLYGON_MODE, s_Saved.polygonMode);
	qgl.GetIntegerv(EGL_BLEND_SRC, &s_Saved.blendSrc);
	qgl.GetIntegerv(EGL_BLEND_DST, &s_Saved.blendDst);
	qgl.GetFloatv(EGL_COLOR_CLEAR_VALUE, s_Saved.clearColor);
	qgl.GetBooleanv(EGL_DEPTH_WRITEMASK, &s_Saved.depthMask);
	qgl.GetBooleanv(EGL_COLOR_WRITEMASK, s_Saved.colorMask);
	s_Saved.depthTest = qgl.IsEnabled(EGL_DEPTH_TEST);
	s_Saved.blend = qgl.IsEnabled(EGL_BLEND);
	s_Saved.alphaTest = qgl.IsEnabled(EGL_ALPHA_TEST);
	s_Saved.cull = qgl.IsEnabled(EGL_CULL_FACE);
	s_Saved.stencil = qgl.IsEnabled(EGL_STENCIL_TEST);
	s_Saved.scissor = qgl.IsEnabled(EGL_SCISSOR_TEST);

	// --- the view's depth on unit 0 (fogzone_gl.cpp: the same copy, MSAA resolved)
	const int vp[4] = { viewport[0], viewport[1], viewport[2], viewport[3] };
	if (!FogZoneGL_CopyViewDepth(vp))
	{
		snprintf(s_szWhy, sizeof(s_szWhy), "the view's depth can't be copied: %s", FogZoneGL_Why());
		qgl.BindTexture(EGL_TEXTURE_2D, (EGLuint_t)s_Saved.bound2D[0]);
		qgl.ActiveTexture((EGLenum_t)s_Saved.activeUnit);
		return false;
	}

	// --- the masks (made on unit 1, so the depth stays bound on unit 0)
	qgl.ActiveTexture(EGL_TEXTURE0 + 1);
	if (!PrepareTextures(viewport[2], viewport[3]))
	{
		Restore();
		return false;
	}
	qgl.ActiveTexture(EGL_TEXTURE0);

	// --- the state the masks are drawn with
	if (s_Saved.depthTest)
		qgl.Disable(EGL_DEPTH_TEST);	// the program compares with the depth copy
	if (s_Saved.alphaTest)
		qgl.Disable(EGL_ALPHA_TEST);
	if (s_Saved.cull)
		qgl.Disable(EGL_CULL_FACE);		// both sides: the shape is what counts
	if (s_Saved.stencil)
		qgl.Disable(EGL_STENCIL_TEST);
	if (s_Saved.scissor)
		qgl.Disable(EGL_SCISSOR_TEST);
	if (s_Saved.blend)
		qgl.Disable(EGL_BLEND);
	qgl.DepthMask(0);
	qgl.ColorMask(1, 1, 1, 1);
	qgl.PolygonMode(EGL_FRONT, EGL_FILL);
	qgl.PolygonMode(EGL_BACK, EGL_FILL);
	qgl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	qgl.Viewport(0, 0, viewport[2], viewport[3]);

	const Program &p = s_Prog[PROG_MASK];
	qgl.UseProgram(p.id);
	qgl.Uniform1i(p.u[MU_DEPTH], 0);
	qgl.Uniform2f(p.u[MU_SIZE], (float)viewport[2], (float)viewport[3]);

	s_bUsed[0] = s_bUsed[1] = false;
	s_iBoundMask = -1;
	s_bBegun = true;
	return true;
}

void EntityGlowGL_Target(int style, const float *color, int mode, int pass)
{
	if (!s_bBegun)
		return;
	style = (style == ENTGLOW_GL_HALO) ? ENTGLOW_GL_HALO : ENTGLOW_GL_OUTLINE;
	BindMask(style);
	const Program &p = s_Prog[PROG_MASK];
	int iMode = (mode == ENTGLOW_GL_OCCLUDED) ? 1 : ((mode == ENTGLOW_GL_VISIBLE) ? 2 : 0);
	if (iMode == 1 && pass == 1)
		iMode = 3;
	qgl.Uniform4f(p.u[MU_COLOR], color[0], color[1], color[2], color[3]);
	qgl.Uniform1i(p.u[MU_MODE], iMode);
}

void EntityGlowGL_BeginPrimitive(int primitive)
{
	if (s_bBegun)
		qgl.Begin((EGLenum_t)primitive);
}

void EntityGlowGL_Vertex(const float *xyz)
{
	qgl.Vertex3fv(xyz);
}

void EntityGlowGL_DrawIndexed(const float *xyz, int numverts, const unsigned short *indices, int count)
{
	if (!s_bBegun || !xyz || !indices || count <= 0)
		return;
	if (qgl.arrays)
	{
		ArraysOn();
		qgl.VertexPointer(3, EGL_FLOAT, 0, xyz);
		qgl.DrawElements(EGL_TRIANGLES, count, EGL_UNSIGNED_SHORT, indices);
		return;
	}
	qgl.Begin(EGL_TRIANGLES);
	for (int i = 0; i < count; i++)
	{
		if (indices[i] < numverts)
			qgl.Vertex3fv(xyz + indices[i] * 3);
	}
	qgl.End();
}

void EntityGlowGL_EndPrimitive(void)
{
	if (s_bBegun)
		qgl.End();
}

void EntityGlowGL_End(void)
{
	if (!s_bBegun)
		return;
	s_bBegun = false;

	if (s_bUsed[ENTGLOW_GL_HALO])
	{
		// the halo mask -> a quarter (q0), blurred across (q1) and up (q0)
		qgl.Viewport(0, 0, s_iQW, s_iQH);

		qgl.BindFramebuffer(EGL_FRAMEBUFFER, s_Fbo[TEX_Q0]);
		qgl.UseProgram(s_Prog[PROG_DOWN].id);
		qgl.Uniform1i(s_Prog[PROG_DOWN].u[DU_SRC], 0);
		qgl.Uniform2f(s_Prog[PROG_DOWN].u[DU_PX], 1.0f / s_iW, 1.0f / s_iH);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_HALO]);
		ScreenQuad();

		const Program &b = s_Prog[PROG_BLUR];
		qgl.UseProgram(b.id);
		qgl.Uniform1i(b.u[BU_SRC], 0);
		qgl.Uniform2f(b.u[BU_PX], 1.0f / s_iQW, 1.0f / s_iQH);

		qgl.BindFramebuffer(EGL_FRAMEBUFFER, s_Fbo[TEX_Q1]);
		qgl.Uniform2f(b.u[BU_DIR], HALO_SPREAD / s_iQW, 0.0f);
		qgl.Uniform1f(b.u[BU_SCALE], 1.0f);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_Q0]);
		ScreenQuad();

		qgl.BindFramebuffer(EGL_FRAMEBUFFER, s_Fbo[TEX_Q0]);
		qgl.Uniform2f(b.u[BU_DIR], 0.0f, HALO_SPREAD / s_iQH);
		qgl.Uniform1f(b.u[BU_SCALE], HALO_AMOUNT);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_Q1]);
		ScreenQuad();
	}

	if (s_bUsed[0] || s_bUsed[1])
	{
		// onto the view, outside the targets
		qgl.BindFramebuffer(EGL_DRAW_FRAMEBUFFER, (EGLuint_t)s_Saved.drawFbo);
		qgl.BindFramebuffer(EGL_READ_FRAMEBUFFER, (EGLuint_t)s_Saved.readFbo);
		qgl.Viewport(s_Saved.viewport[0], s_Saved.viewport[1], s_Saved.viewport[2], s_Saved.viewport[3]);
		qgl.Enable(EGL_BLEND);
		qgl.BlendFunc(EGL_ONE, EGL_ONE_MINUS_SRC_ALPHA);

		const Program &c = s_Prog[PROG_COMPOSITE];
		qgl.UseProgram(c.id);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_OUTLINE]);
		qgl.ActiveTexture(EGL_TEXTURE0 + 1);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_HALO]);
		qgl.ActiveTexture(EGL_TEXTURE0 + 2);
		qgl.BindTexture(EGL_TEXTURE_2D, s_Tex[TEX_Q0]);
		qgl.ActiveTexture(EGL_TEXTURE0);
		qgl.Uniform1i(c.u[CU_OUTLINE], 0);
		qgl.Uniform1i(c.u[CU_HALOMASK], 1);
		qgl.Uniform1i(c.u[CU_HALO], 2);
		qgl.Uniform4f(c.u[CU_VIEWPORT], (float)s_Saved.viewport[0], (float)s_Saved.viewport[1],
			(float)s_Saved.viewport[2], (float)s_Saved.viewport[3]);
		float px = floorf(s_iH / 540.0f + 0.5f);
		if (px < OUTLINE_PIXELS)
			px = OUTLINE_PIXELS;
		qgl.Uniform2f(c.u[CU_PX], 1.0f / s_iW, 1.0f / s_iH);
		qgl.Uniform1f(c.u[CU_WIDTH], px);
		qgl.Uniform2f(c.u[CU_USE], s_bUsed[0] ? 1.0f : 0.0f, s_bUsed[1] ? 1.0f : 0.0f);
		ScreenQuad();
	}

	Restore();

	if (s_iChecks < ENTGLOW_ERROR_CHECKS)
	{
		s_iChecks++;
		const EGLenum_t e = qgl.GetError();
		if (e)
		{
			s_uError = e;
			s_iChecks = ENTGLOW_ERROR_CHECKS;	// once per context
		}
	}
}

unsigned int EntityGlowGL_TakeError(void)
{
	const unsigned int e = s_uError;
	s_uError = 0;
	return e;
}

const char *EntityGlowGL_Why(void)
{
	return s_szWhy;
}

int EntityGlowGL_Generation(void)
{
	return s_iGeneration;
}
