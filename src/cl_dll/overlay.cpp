//=========================================================
// SBO 2.0 env_overlay, client side: see overlay.h, overlay_core.h, game_shared/overlay_shared.h and
// docs/ENV_OVERLAY.md. Written for SBO from the message format (the design follows Next21's HudSprite message).
//
// Each of the 8 channels holds one overlay: a sprite of the map's precache list (animated at its frame rate), a .tga
// the client has (decoded here, overlay_gl.cpp makes the texture) or, over the whole screen, a plain color. They are
// drawn at the start of HUD_Redraw, channel 1 first, so the HUD and every VGUI2 panel are drawn over them.
//
// Size: the picture's own pixels times "scale", times the HUD's scale: hud_scale's factor (hud_scale.cpp) and the
// HUD sprite set's resolution against the 640 set (CHud::m_iRes), so an overlay keeps its size next to the HUD with
// either. Full screen: stretched over the screen. Placement: game_text's x / y (message.cpp XPosition / YPosition).
// Pictures need the OpenGL renderer; the software renderer draws the plain colors only.
//=========================================================

#include <string.h>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "triangleapi.h"
#include "overlay.h"
#include "overlay_core.h"
#include "overlay_gl.h"
#include "vgui2/sbo_image_decode.h"

#define OVERLAY_MAX_MODELS		512		// the engine's model precache list (MAX_MODELS)
#define OVERLAY_MAX_TGA			16		// .tga pictures kept per map
#define OVERLAY_TGA_MAX_SIDE	2048	// a larger .tga is scaled down to fit

struct overlay_tga_t
{
	char			path[ OVERLAY_MAX_PATH ];
	unsigned int	tex;		// 0: could not be loaded
	void			*context;	// the GL context tex belongs to
	int				wide, tall;
};

struct client_overlay_t
{
	bool			active;
	overlay::Params	p;
	float			start;		// client time it started (the message's elapsed time taken off)
	bool			off;		// turned off: fading out
	float			offTime;
	float			offAlpha;	// opacity when it was turned off
	struct model_s	*sprite;	// a .spr
	int				tga;		// a .tga: index into s_Tga, -1 none
	int				wide, tall;	// the picture's size in pixels
};

static client_overlay_t	s_Overlays[ OVERLAY_CHANNELS ];
static overlay_tga_t	s_Tga[ OVERLAY_MAX_TGA ];
static int				s_iTga;

static bool HasExtension( const char *path, const char *ext )
{
	const size_t len = strlen( path );
	return len > 4 && !stricmp( path + len - 4, ext );
}

// a sprite the server precached (the engine loaded it with the map's model list)
static struct model_s *FindSprite( const char *path )
{
	for ( int i = 1; i < OVERLAY_MAX_MODELS; i++ )
	{
		struct model_s *pModel = gEngfuncs.hudGetModelByIndex( i );
		if ( pModel && pModel->type == mod_sprite && !stricmp( pModel->name, path ) )
			return pModel;
	}
	return NULL;
}

// decodes the file into s_Tga[i]'s texture; its index, or -1
static int LoadTga( int i )
{
	overlay_tga_t &t = s_Tga[i];
	int len = 0;
	byte *data = gEngfuncs.COM_LoadFile( t.path, 5, &len );
	if ( !data )
	{
		gEngfuncs.Con_DPrintf( "env_overlay: can't open \"%s\"\n", t.path );
		return -1;
	}

	std::vector<uint8_t> rgba;
	int wide = 0, tall = 0;
	const bool ok = len > 0 && sbo_image::DecodeTgaScaled( data, (size_t)len, OVERLAY_TGA_MAX_SIDE,
		OVERLAY_TGA_MAX_SIDE, rgba, wide, tall );
	gEngfuncs.COM_FreeFile( data );

	if ( !ok )
	{
		gEngfuncs.Con_DPrintf( "env_overlay: \"%s\" is not a 24 or 32 bit .tga\n", t.path );
		return -1;
	}

	t.tex = OverlayGL_CreateTexture( rgba.data(), wide, tall );
	t.wide = wide;
	t.tall = tall;
	return t.tex ? i : -1;
}

// a .tga the client has (precached by the server, so downloaded), loaded once per map
static int FindTga( const char *path )
{
	void *context = OverlayGL_Context();
	if ( !context )
		return -1;	// no GL: no pictures

	for ( int i = 0; i < s_iTga; i++ )
	{
		if ( stricmp( s_Tga[i].path, path ) )
			continue;
		if ( s_Tga[i].context == context )
			return s_Tga[i].tex ? i : -1;
		// a new GL context (video mode change): load it again into this one
		s_Tga[i].tex = 0;
		s_Tga[i].context = context;
		return LoadTga( i );
	}

	if ( s_iTga >= OVERLAY_MAX_TGA )
	{
		gEngfuncs.Con_DPrintf( "env_overlay: more than %d .tga pictures on this map; \"%s\" not shown\n", OVERLAY_MAX_TGA, path );
		return -1;
	}

	overlay_tga_t &t = s_Tga[ s_iTga ];
	memset( &t, 0, sizeof( t ) );
	strncpy( t.path, path, sizeof( t.path ) - 1 );
	t.context = context;
	return LoadTga( s_iTga++ );
}

static float CurrentAlpha( const client_overlay_t &o, float now )
{
	if ( o.off )
		return overlay::AlphaOff( o.p, o.offAlpha, now - o.offTime );
	return overlay::Alpha( o.p, now - o.start );
}

static int MsgFunc_Overlay( const char *pszName, int iSize, void *pbuf )
{
	overlay::Params p;
	if ( !overlay::Parse( (const unsigned char *)pbuf, iSize, p ) )
		return 1;

	// the HUD's clock (CHud::Redraw's flTime), as game_text and the ammo history time their messages
	const float now = gHUD.m_flTime;

	if ( !( p.flags & OVERLAY_FLAG_ON ) )
	{
		for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
		{
			if ( p.channel && p.channel != ch + 1 )
				continue;

			client_overlay_t &o = s_Overlays[ch];
			if ( !o.active )
				continue;

			if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0.0f )
				gEngfuncs.Con_Printf( "env_overlay: channel %d off%s at %.2f\n", ch + 1,
					( p.flags & OVERLAY_FLAG_NOW ) ? " now" : "", now );

			if ( p.flags & OVERLAY_FLAG_NOW )
				o.active = false;
			else if ( !o.off )
			{
				const float a = CurrentAlpha( o, now );
				o.off = true;
				o.offTime = now;
				o.offAlpha = a > 0.0f ? a : 0.0f;
				if ( !( o.p.fadeout > 0.0f ) || a <= 0.0f )
					o.active = false;
			}
		}
		return 1;
	}

	client_overlay_t &o = s_Overlays[ p.channel - 1 ];
	memset( &o, 0, sizeof( o ) );
	o.p = p;
	o.start = now - p.elapsed;
	o.tga = -1;

	if ( p.image[0] )
	{
		if ( HasExtension( p.image, ".spr" ) )
		{
			o.sprite = FindSprite( p.image );
			if ( !o.sprite )
			{
				gEngfuncs.Con_DPrintf( "env_overlay: \"%s\" is not a precached sprite\n", p.image );
				return 1;
			}
			o.wide = (int)( o.sprite->maxs[0] - o.sprite->mins[0] );
			o.tall = (int)( o.sprite->maxs[2] - o.sprite->mins[2] );
		}
		else if ( HasExtension( p.image, ".tga" ) )
		{
			o.tga = FindTga( p.image );
			if ( o.tga < 0 )
				return 1;
			o.wide = s_Tga[ o.tga ].wide;
			o.tall = s_Tga[ o.tga ].tall;
		}
		else
		{
			gEngfuncs.Con_DPrintf( "env_overlay: \"%s\" is not a .spr or .tga\n", p.image );
			return 1;
		}

		if ( o.wide <= 0 || o.tall <= 0 )
			return 1;
	}

	o.active = true;

	if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0.0f )
		gEngfuncs.Con_Printf( "env_overlay: channel %d \"%s\" %dx%d%s at %.2f (client %.2f)\n", p.channel, p.image,
			o.wide, o.tall, o.sprite ? "" : ( o.tga >= 0 ? " (tga)" : " (color)" ), now, gEngfuncs.GetClientTime() );
	return 1;
}

void Overlay_Init( void )
{
	gEngfuncs.pfnHookUserMsg( OVERLAY_MSG_NAME, MsgFunc_Overlay );
}

void Overlay_Reset( void )
{
	memset( s_Overlays, 0, sizeof( s_Overlays ) );
}

void Overlay_VidInit( void )
{
	Overlay_Reset();

	void *context = OverlayGL_Context();
	for ( int i = 0; i < s_iTga; i++ )
	{
		if ( s_Tga[i].tex && s_Tga[i].context == context )
			OverlayGL_DeleteTexture( s_Tga[i].tex );
	}
	memset( s_Tga, 0, sizeof( s_Tga ) );
	s_iTga = 0;
}

static void DrawQuad( int x0, int y0, int x1, int y1 )
{
	triangleapi_s *tri = gEngfuncs.pTriAPI;
	tri->Begin( TRI_QUADS );
	tri->TexCoord2f( 0.0f, 0.0f );
	tri->Vertex3f( (float)x0, (float)y0, 0.0f );
	tri->TexCoord2f( 1.0f, 0.0f );
	tri->Vertex3f( (float)x1, (float)y0, 0.0f );
	tri->TexCoord2f( 1.0f, 1.0f );
	tri->Vertex3f( (float)x1, (float)y1, 0.0f );
	tri->TexCoord2f( 0.0f, 1.0f );
	tri->Vertex3f( (float)x0, (float)y1, 0.0f );
	tri->End();
}

static void DrawOverlay( client_overlay_t &o, float flTime, int alpha )
{
	const overlay::Params &p = o.p;
	const bool bAdditive = ( p.flags & OVERLAY_FLAG_ADDITIVE ) != 0;

	int x0, y0, x1, y1;
	if ( p.flags & OVERLAY_FLAG_FULLSCREEN )
	{
		x0 = 0;
		y0 = 0;
		x1 = ScreenWidth;
		y1 = ScreenHeight;
	}
	else
	{
		// the HUD's scale: hud_scale and the sprite set's resolution against the 640 set
		const float factor = HUD_GetScale() * ( gHUD.m_iRes / 640.0f );
		const int wide = (int)( o.wide * p.scale * factor + 0.5f );
		const int tall = (int)( o.tall * p.scale * factor + 0.5f );
		if ( wide <= 0 || tall <= 0 )
			return;
		x0 = overlay::Place( p.x, wide, ScreenWidth );
		y0 = overlay::Place( p.y, tall, ScreenHeight );
		x1 = x0 + wide;
		y1 = y0 + tall;
	}

	int r = p.r, g = p.g, b = p.b;

	if ( !o.sprite && o.tga < 0 )
	{
		// a plain color: blended, or added (FillRGBA adds)
		if ( bAdditive )
			gEngfuncs.pfnFillRGBA( x0, y0, x1 - x0, y1 - y0, r, g, b, alpha );
		else
			gEngfuncs.pfnFillRGBABlend( x0, y0, x1 - x0, y1 - y0, r, g, b, alpha );
		return;
	}

	if ( !HUD_IsHardware() )
		return;

	// a picture keeps its own colors with FX Color 0 0 0 (as the engine draws sprites)
	if ( !r && !g && !b )
		r = g = b = 255;

	triangleapi_s *tri = gEngfuncs.pTriAPI;

	if ( o.sprite )
	{
		const int frame = overlay::Frame( p, flTime - o.start, o.sprite->numframes );
		if ( !tri->SpriteTexture( o.sprite, frame ) )
			return;
	}

	tri->RenderMode( bAdditive ? kRenderTransAdd : kRenderTransAlpha );
	if ( bAdditive )
	{
		// added light: the amount scales the color
		tri->Color4ub( (unsigned char)( r * alpha / 255 ), (unsigned char)( g * alpha / 255 ),
			(unsigned char)( b * alpha / 255 ), 255 );
	}
	else
		tri->Color4ub( (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)alpha );

	if ( o.sprite )
		DrawQuad( x0, y0, x1, y1 );
	else
	{
		const unsigned int previous = OverlayGL_Bind( s_Tga[ o.tga ].tex );
		DrawQuad( x0, y0, x1, y1 );
		OverlayGL_Restore( previous );
	}

	tri->RenderMode( kRenderNormal );
	tri->Color4ub( 255, 255, 255, 255 );
}

void Overlay_Redraw( float flTime )
{
	for ( int ch = 0; ch < OVERLAY_CHANNELS; ch++ )
	{
		client_overlay_t &o = s_Overlays[ch];
		if ( !o.active )
			continue;

		const float a = CurrentAlpha( o, flTime );
		if ( a < 0.0f )
		{
			o.active = false;	// ended or faded out
			if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0.0f )
				gEngfuncs.Con_Printf( "env_overlay: channel %d ended at %.2f (started %.2f)\n", ch + 1, flTime, o.start );
			continue;
		}

		int alpha = (int)( a * o.p.a + 0.5f );
		if ( alpha > 255 )
			alpha = 255;
		if ( alpha <= 0 )
			continue;

		DrawOverlay( o, flTime, alpha );
	}
}
