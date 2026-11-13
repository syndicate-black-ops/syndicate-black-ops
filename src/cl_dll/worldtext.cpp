//=========================================================
// SBO 2.0: point_worldtext, client side (worldtext.h). The server sends each text that is on (dlls/point_worldtext.cpp,
// game_shared/worldtext_shared.h); this file keeps them, loads the fonts and draws the texts in the normal
// triangle pass (after the world and the solid entities, before the transparent ones: those in front of a text are
// blended over it). Parsing, decoding and layout: worldtext_core.cpp. docs/POINT_WORLDTEXT.md.
//
// Drawing: one textured quad per glyph from the font's atlas sprite (SPR_INDEXALPHA, tinted with the text's color,
// kRenderTransAlpha), fullbright, fogged like the world, then a depth-only copy of the letters (worldtext_gl.cpp): a
// transparent entity behind a letter stays behind it. A text is drawn only when
//   - the renderer is OpenGL (the software renderer draws no world text: the texts wait),
//   - it is in the potentially visible set of the view (the triangle API's BoxInPVS), and in the view's cone,
//   - a line would be at least WT_MIN_PIXELS high on the screen,
//   - (fixed orientation) the view is in front of it: a text is one-sided.
// The 3D sky pass and the spectator's overview draw none (tri.cpp returns before). Near an env_mirror the text is
// drawn a second time, reflected (mirror.cpp's effect hook), mirror-inverted as in a real mirror.
//=========================================================

#include <math.h>	// before cl_util.h (its fabs, min and max macros)
#include <string.h>
#include <string>
#include <vector>

#include "worldtext_core.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "triangleapi.h"
#include "com_model.h"
#include "r_studioint.h"
#include "mirror.h"
#include "worldtext.h"
#include "worldtext_gl.h"
#include "movewith_client.h"

extern engine_studio_api_t IEngineStudio;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WT_MIN_PIXELS	1.0f	// a line drawn smaller than this on the screen is skipped

// Text styles (the second mask is the glyph widened, scripts/gen_worldtext_fonts.py)
enum
{
	WT_STYLE_PLAIN,
	WT_STYLE_OUTLINE,	// black outline from the widened mask
	WT_STYLE_SHADOW,	// black copy of the glyph, down and to the right
	WT_STYLE_GLOW,		// the widened mask in the text's color, faint
};

// The font key: Source's font numbers (0 .. 12) on SBO's two faces, and SBO's 13. Source's "outline/shadow" fonts
// (even numbers) are outlined here, its plain ones plain. docs/POINT_WORLDTEXT.md has the table.
static const struct
{
	int face;	// index into s_Faces
	int style;
} s_FontTable[WORLDTEXT_FONT_COUNT] =
{
	{ 0, WT_STYLE_OUTLINE },	// 0  Source: TF2 Build
	{ 0, WT_STYLE_PLAIN },		// 1  TF2 Build (no outline/shadow)
	{ 0, WT_STYLE_OUTLINE },	// 2  TF2
	{ 0, WT_STYLE_PLAIN },		// 3  TF2 (no outline/shadow)
	{ 0, WT_STYLE_OUTLINE },	// 4  Liberation Sans
	{ 0, WT_STYLE_PLAIN },		// 5  Liberation Sans (no outline/shadow)
	{ 0, WT_STYLE_OUTLINE },	// 6  TF2 Professor
	{ 0, WT_STYLE_PLAIN },		// 7  TF2 Professor (no outline/shadow)
	{ 1, WT_STYLE_OUTLINE },	// 8  Roboto Mono
	{ 1, WT_STYLE_PLAIN },		// 9  Roboto Mono (no outline/shadow)
	{ 1, WT_STYLE_SHADOW },		// 10 Roboto Mono (shadow only)
	{ 1, WT_STYLE_GLOW },		// 11 Roboto Mono (green glow, soft edges): the glow in the text's color
	{ 0, WT_STYLE_GLOW },		// 12 TF2 Build (soft edges)
	{ 0, WT_STYLE_SHADOW },		// 13 SBO: sans with a drop shadow
};

struct WTFace
{
	const char		*name;		// sprites/worldtext/<name>.spr and .txt
	int				state;		// 0 not tried, 1 loaded, -1 failed (not tried again until the next map)
	HSPRITE			hSprite;
	WTFontMetrics	metrics;
};

static WTFace s_Faces[] = { { "sans", 0, 0 }, { "mono", 0, 0 } };
#define WT_FACES	( (int)( sizeof( s_Faces ) / sizeof( s_Faces[0] ) ) )

struct WTText
{
	WTState				state;
	std::vector<int>	codepoints;
	WTLayout			layout;
	int					face;
	bool				ready;		// laid out with a loaded font
	// the last frame drawn, for test_cl_worldtext (SBO_TEST_COMMANDS)
	const char			*lastState;
	int					lastGlyphs;	// glyph quads of the fill pass, mirror copies included
};

static std::vector<WTText> s_Texts;
static WTStore s_Store;

//---------------------------------------------------------
// Fonts
//---------------------------------------------------------

static bool LoadFace( int i )
{
	WTFace &f = s_Faces[i];
	if ( f.state )
		return f.state > 0;

	f.state = -1;

	char path[64];
	int len = 0;

	snprintf( path, sizeof( path ), "sprites/worldtext/%s.txt", f.name );
	byte *pFile = gEngfuncs.COM_LoadFile( path, 5, &len );
	if ( !pFile )
	{
		gEngfuncs.Con_DPrintf( "point_worldtext: %s not found, texts in this font are not shown\n", path );
		return false;
	}
	std::string err;
	bool ok = WT_ParseFontMetrics( (const char *)pFile, len, f.metrics, err );
	gEngfuncs.COM_FreeFile( pFile );
	if ( !ok )
	{
		gEngfuncs.Con_DPrintf( "point_worldtext: %s: %s; texts in this font are not shown\n", path, err.c_str() );
		return false;
	}

	// SPR_Load only for a file that is there
	snprintf( path, sizeof( path ), "sprites/worldtext/%s.spr", f.name );
	pFile = gEngfuncs.COM_LoadFile( path, 5, &len );
	if ( !pFile )
	{
		gEngfuncs.Con_DPrintf( "point_worldtext: %s not found, texts in this font are not shown\n", path );
		return false;
	}
	gEngfuncs.COM_FreeFile( pFile );

	f.hSprite = SPR_Load( path );
	if ( f.hSprite <= 0 || SPR_Frames( f.hSprite ) < 2 * f.metrics.frames
		|| SPR_Width( f.hSprite, 0 ) != f.metrics.frameW || SPR_Height( f.hSprite, 0 ) != f.metrics.frameH )
	{
		gEngfuncs.Con_DPrintf( "point_worldtext: %s does not match its metrics file, texts in this font are not shown\n", path );
		f.hSprite = 0;
		return false;
	}

	f.state = 1;
	return true;
}

static void LayOut( WTText &t )
{
	t.face = s_FontTable[t.state.font].face;
	t.ready = false;

	if ( !LoadFace( t.face ) )
		return;

	const bool billboard = t.state.orientation != WORLDTEXT_ORIENT_FIXED;
	WT_Layout( s_Faces[t.face].metrics, t.codepoints, t.state.spacingX, t.state.spacingY, t.state.flags, billboard,
		t.layout );
	t.ready = true;
}

//---------------------------------------------------------
// Messages
//---------------------------------------------------------

static int MsgFunc_WorldText( const char *pszName, int iSize, void *pbuf )
{
	WTState state;
	bool on = false;
	if ( !s_Store.OnMain( pbuf, iSize, state, on ) )
		return 1;

	size_t i;
	for ( i = 0; i < s_Texts.size(); i++ )
	{
		if ( s_Texts[i].state.id == state.id )
			break;
	}

	if ( !on )
	{
		if ( i < s_Texts.size() )
		{
			s_Texts[i] = s_Texts.back();
			s_Texts.pop_back();
		}
		return 1;
	}

	if ( i == s_Texts.size() )
	{
		s_Texts.push_back( WTText() );
		s_Texts.back().lastState = NULL;
		s_Texts.back().lastGlyphs = 0;
	}

	WTText &t = s_Texts[i];
	t.state = state;
	WT_DecodeText( (const unsigned char *)t.state.text.c_str(), (int)t.state.text.size(), t.codepoints, WORLDTEXT_MAX_TEXT );
	LayOut( t );
	return 1;
}

static int MsgFunc_WorldTextP( const char *pszName, int iSize, void *pbuf )
{
	s_Store.OnPart( pbuf, iSize );
	return 1;
}

// test_cl_worldtext (test builds only: SBO_TEST_COMMANDS, docs/dev/ENTITY_TESTS.md): every text the client has and what
// the last frame did with it, to the console (the test of the demo map).
#ifdef SBO_TEST_COMMANDS
static void WorldText_Dump( void )
{
	static const char *s_Orient[] = { "fixed", "face", "upright" };
	gEngfuncs.Con_Printf( "test_cl_worldtext: %d texts, renderer %s\n", (int)s_Texts.size(), HUD_IsHardware() ? "gl" : "software" );
	for ( size_t i = 0; i < s_Texts.size(); i++ )
	{
		const WTText &t = s_Texts[i];
		// the engine's console line holds about 1 KB: a long text is cut (its length is printed)
		std::string text = t.state.text.substr( 0, 400 );
		for ( size_t k = 0; k < text.size(); k++ )
		{
			if ( text[k] == '\n' )
				text[k] = '|';
		}
		gEngfuncs.Con_Printf( "test_cl_worldtext: ent %d font %d size %g orient %s color %d %d %d %d rainbow %d origin %g %g %g "
			"angles %g %g %g state \"%s\" glyphs %d bytes %d text \"%s\"\n", t.state.id, t.state.font, t.state.size,
			s_Orient[t.state.orientation], t.state.color[0], t.state.color[1], t.state.color[2], t.state.color[3],
			( t.state.flags & WORLDTEXT_FLAG_RAINBOW ) ? 1 : 0, t.state.origin[0], t.state.origin[1], t.state.origin[2],
			t.state.angles[0], t.state.angles[1], t.state.angles[2], t.lastState ? t.lastState : "not drawn yet",
			t.lastGlyphs, (int)t.state.text.size(), text.c_str() );
	}
	gEngfuncs.Con_Printf( "test_cl_worldtext: end\n" );
}
#endif

void WorldText_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "WorldText", MsgFunc_WorldText );
	gEngfuncs.pfnHookUserMsg( "WorldTextP", MsgFunc_WorldTextP );
#ifdef SBO_TEST_COMMANDS
	gEngfuncs.pfnAddCommand( "test_cl_worldtext", WorldText_Dump );
#endif
}

void WorldText_VidInit( void )
{
	s_Texts.clear();
	s_Store.Clear();
	for ( int i = 0; i < WT_FACES; i++ )
	{
		s_Faces[i].state = 0;
		s_Faces[i].hSprite = 0;
	}
}

void WorldText_Reset( void )
{
	s_Texts.clear();
	s_Store.Clear();
}

//---------------------------------------------------------
// Drawing
//---------------------------------------------------------

static void HsvToRgb( float h, float s, float v, float *rgb )
{
	h = fmodf( h, 360.0f );
	if ( h < 0 )
		h += 360.0f;
	h /= 60.0f;
	const int i = (int)h;
	const float f = h - i;
	const float p = v * ( 1 - s ), q = v * ( 1 - s * f ), u = v * ( 1 - s * ( 1 - f ) );
	switch ( i )
	{
	case 0: rgb[0] = v; rgb[1] = u; rgb[2] = p; break;
	case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;
	case 2: rgb[0] = p; rgb[1] = v; rgb[2] = u; break;
	case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;
	case 4: rgb[0] = u; rgb[1] = p; rgb[2] = v; break;
	default: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;
	}
}

struct WTView
{
	float	origin[3], forward[3], right[3], up[3];
	float	cosCone;		// the view's cone (half the diagonal, with a margin)
	float	sinCone;
	float	pixelsPerUnit;	// on the screen, per world unit at distance 1
};

// The glyph quads of one text in one pass. mirror: reflect every vertex across that mirror. depthOnly: no color, the
// depth of the glyphs' solid pixels (worldtext_gl.cpp).
static void DrawPass( const WTText &t, const WTFace &face, const float *org, const float *right, const float *up,
	float scale, int frameOffset, float dx, float dy, int colorMode, float alpha, const sbo_mirror_t *mirror,
	bool depthOnly = false )
{
	triangleapi_s *tri = gEngfuncs.pTriAPI;
	model_s *pModel = (model_s *)gEngfuncs.GetSpritePointer( face.hSprite );
	if ( !pModel )
		return;

	const float flTime = gEngfuncs.GetClientTime();
	const bool rainbow = ( t.state.flags & WORLDTEXT_FLAG_RAINBOW ) && colorMode != 0;
	const unsigned char a = (unsigned char)( alpha * t.state.color[3] );
	if ( !a )
		return;

	for ( int f = 0; f < face.metrics.frames; f++ )
	{
		const int first = t.layout.frameStart[f], last = t.layout.frameStart[f + 1];
		if ( first >= last )
			continue;
		if ( !tri->SpriteTexture( pModel, f + frameOffset ) )
			return;

		tri->RenderMode( kRenderTransAlpha );
		if ( depthOnly && !WorldTextGL_DepthBegin() )
			return;
		tri->CullFace( TRI_NONE );
		tri->Begin( TRI_QUADS );

		if ( colorMode == 0 )
			tri->Color4ub( 0, 0, 0, a );
		else if ( !rainbow )
			tri->Color4ub( t.state.color[0], t.state.color[1], t.state.color[2], a );

		for ( int k = first; k < last; k++ )
		{
			const WTQuad &q = t.layout.quads[k];

			if ( rainbow )
			{
				// Source: the hue turns 100 degrees a second, 15 degrees per character; saturation 0.7
				float rgb[3];
				HsvToRgb( flTime * 100.0f + q.index * 15.0f, 0.7f, 1.0f, rgb );
				tri->Color4ub( (unsigned char)( rgb[0] * 255 ), (unsigned char)( rgb[1] * 255 ),
					(unsigned char)( rgb[2] * 255 ), a );
			}

			const float xs[4] = { q.x0, q.x0, q.x1, q.x1 };
			const float ys[4] = { q.y0, q.y1, q.y1, q.y0 };
			const float ss[4] = { q.s0, q.s0, q.s1, q.s1 };
			const float ts[4] = { q.t0, q.t1, q.t1, q.t0 };
			for ( int v = 0; v < 4; v++ )
			{
				const float x = ( xs[v] + dx ) * scale, y = ( ys[v] + dy ) * scale;
				float p[3];
				for ( int c = 0; c < 3; c++ )
					p[c] = org[c] + right[c] * x + up[c] * y;
				if ( mirror )
				{
					float m[3];
					Mirror_ReflectPoint( mirror, p, m );
					p[0] = m[0];
					p[1] = m[1];
					p[2] = m[2];
				}
				tri->TexCoord2f( ss[v], ts[v] );
				tri->Vertex3fv( p );
			}
		}

		tri->End();
		if ( depthOnly )
			WorldTextGL_DepthEnd();
	}
}

static void DrawText( WTText &t, const float *right, const float *up, const sbo_mirror_t *mirror )
{
	t.lastGlyphs += (int)t.layout.quads.size();

	const WTFace &face = s_Faces[t.face];
	const float scale = t.state.size / face.metrics.em;
	const int style = s_FontTable[t.state.font].style;
	const int n = face.metrics.frames;
	const float shadow = face.metrics.em * 0.06f;

	if ( style == WT_STYLE_OUTLINE )
		DrawPass( t, face, t.state.origin, right, up, scale, n, 0, 0, 0, 1.0f, mirror );
	else if ( style == WT_STYLE_SHADOW )
		DrawPass( t, face, t.state.origin, right, up, scale, 0, shadow, -shadow, 0, 0.75f, mirror );
	else if ( style == WT_STYLE_GLOW )
		DrawPass( t, face, t.state.origin, right, up, scale, n, 0, 0, 1, 0.35f, mirror );

	DrawPass( t, face, t.state.origin, right, up, scale, 0, 0, 0, 1, 1.0f, mirror );
	DrawPass( t, face, t.state.origin, right, up, scale, 0, 0, 0, 1, 1.0f, mirror, true );
}

static float Dot( const float *a, const float *b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void WorldText_DrawFrame( void )
{
	if ( s_Texts.empty() )
		return;

	for ( size_t i = 0; i < s_Texts.size(); i++ )
	{
		s_Texts[i].lastState = "culled";
		s_Texts[i].lastGlyphs = 0;
	}

	if ( !HUD_IsHardware() )
	{
		for ( size_t i = 0; i < s_Texts.size(); i++ )
			s_Texts[i].lastState = "software renderer";
		return;
	}

	WTView view;
	IEngineStudio.GetViewInfo( view.origin, view.up, view.right, view.forward );

	// the view's cone: HL25 widens a 4:3 field of view for wider screens (Hor+)
	float fov = (float)gHUD.m_iRenderFOV;	// the FOV the frame is drawn with (CHud::Think)
	if ( !( fov >= 10.0f && fov <= 170.0f ) )
		fov = 90.0f;
	const float aspect = ( ScreenHeight > 0 ) ? (float)ScreenWidth / ScreenHeight : 4.0f / 3.0f;
	const float t = tanf( fov * 0.5f * (float)M_PI / 180.0f );
	const float th = t * ( aspect > 4.0f / 3.0f ? aspect * 0.75f : 1.0f );
	const float tv = th / ( aspect > 0.1f ? aspect : 1.0f );
	const float cone = atanf( sqrtf( th * th + tv * tv ) * 1.15f );
	view.pixelsPerUnit = ( ScreenHeight > 0 ? ScreenHeight : 480 ) * 0.5f / ( tv > 0.01f ? tv : 0.01f );

	int mirrors[MIRROR_EFFECT_MAX];
	const int nMirrors = Mirror_EffectMirrors( mirrors );

	WorldTextGL_Begin();	// texture unit 0 (worldtext_gl.cpp)

	for ( size_t i = 0; i < s_Texts.size(); i++ )
	{
		WTText &txt = s_Texts[i];
		if ( !txt.ready || txt.layout.quads.empty() || txt.state.size <= 0.0f || !txt.state.color[3] )
		{
			txt.lastState = !txt.ready ? "no font" : "empty";
			continue;
		}

		// a text with a parent (movewith): on the parent's drawn pose this frame, so it never lags behind it; while
		// the parent isn't here, where it was last
		if ( txt.state.flags & WORLDTEXT_FLAG_PARENT )
			MoveWith_PlaceOnParent( txt.state.parent, txt.state.localOrigin, txt.state.localAngles, txt.state.origin,
				txt.state.angles );

		const WTFace &face = s_Faces[txt.face];
		const float scale = txt.state.size / face.metrics.em;

		// the text's plane
		float forward[3] = { 1, 0, 0 }, right[3] = { 0, -1, 0 }, up[3] = { 0, 0, 1 };
		if ( txt.state.orientation == WORLDTEXT_ORIENT_FACE )
		{
			for ( int c = 0; c < 3; c++ )
			{
				right[c] = view.right[c];
				up[c] = view.up[c];
			}
		}
		else if ( txt.state.orientation == WORLDTEXT_ORIENT_UPRIGHT )
		{
			// facing the view, turned about the vertical only
			float rx = view.right[0], ry = view.right[1];
			float len = sqrtf( rx * rx + ry * ry );
			if ( len < 0.001f )
			{
				rx = view.forward[1];
				ry = -view.forward[0];
				len = sqrtf( rx * rx + ry * ry );
			}
			if ( len < 0.001f )
				continue;
			right[0] = rx / len;
			right[1] = ry / len;
			right[2] = 0;
		}
		else
		{
			AngleVectors( txt.state.angles, forward, right, up );
		}

		// the block's bounding sphere
		const float cx = ( txt.layout.minX + txt.layout.maxX ) * 0.5f * scale;
		const float cy = ( txt.layout.minY + txt.layout.maxY ) * 0.5f * scale;
		const float hw = ( txt.layout.maxX - txt.layout.minX ) * 0.5f * scale;
		const float hh = ( txt.layout.maxY - txt.layout.minY ) * 0.5f * scale;
		const float radius = sqrtf( hw * hw + hh * hh ) + 1.0f;
		float center[3], dir[3];
		for ( int c = 0; c < 3; c++ )
		{
			center[c] = txt.state.origin[c] + right[c] * cx + up[c] * cy;
			dir[c] = center[c] - view.origin[c];
		}
		const float dist = sqrtf( Dot( dir, dir ) );

		bool visible = true;
		if ( dist > radius )
		{
			// in the view's cone
			const float angle = acosf( fmaxf( -1.0f, fminf( 1.0f, Dot( dir, view.forward ) / dist ) ) );
			if ( angle > cone + asinf( radius / dist ) )
			{
				visible = false;
				txt.lastState = "out of view";
			}
			// big enough on the screen to see
			else if ( txt.state.size * view.pixelsPerUnit / dist < WT_MIN_PIXELS )
			{
				visible = false;
				txt.lastState = "too small";
			}
		}

		// in the view's potentially visible set (the main view: the spectator's camera inset has its own)
		if ( visible && gEngfuncs.pTriAPI->BoxInPVS && !gHUD.m_Spectator.m_bInsetDrawPass )
		{
			float mins[3], maxs[3];
			for ( int c = 0; c < 3; c++ )
			{
				mins[c] = center[c] - radius;
				maxs[c] = center[c] + radius;
			}
			if ( !gEngfuncs.pTriAPI->BoxInPVS( mins, maxs ) )
			{
				txt.lastState = "not in PVS";
				continue;	// nor its reflection (a mirror is in the same room)
			}
		}

		// fixed: one-sided, read looking along the entity's forward
		if ( visible && txt.state.orientation == WORLDTEXT_ORIENT_FIXED )
		{
			float rel[3] = { view.origin[0] - txt.state.origin[0], view.origin[1] - txt.state.origin[1],
				view.origin[2] - txt.state.origin[2] };
			if ( Dot( rel, forward ) >= 0.0f )
			{
				visible = false;
				txt.lastState = "back side";
			}
		}

		if ( visible )
		{
			txt.lastState = "drawn";
			DrawText( txt, right, up, NULL );
		}

		// reflections
		for ( int k = 0; k < nMirrors; k++ )
		{
			const sbo_mirror_t *m = Mirror_Get( mirrors[k] );
			if ( !m || !Mirror_Reflects( m, center, Mirror_EffectViewOrigin() ) )
				continue;
			if ( txt.state.orientation == WORLDTEXT_ORIENT_FIXED )
			{
				float v[3], rel[3];
				Mirror_ReflectPoint( m, view.origin, v );
				for ( int c = 0; c < 3; c++ )
					rel[c] = v[c] - txt.state.origin[c];
				if ( Dot( rel, forward ) >= 0.0f )
					continue;
			}
			if ( !visible )
				txt.lastState = "mirror only";
			Mirror_EffectBegin( m );
			DrawText( txt, right, up, m );
			Mirror_EffectEnd();
		}
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
	gEngfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );
	WorldTextGL_End();
}
