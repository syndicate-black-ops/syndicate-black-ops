//=========================================================
// SBO 2.1 info_waypoint, client side (waypoint.h; docs/INFO_WAYPOINT.md). Each marker is an icon (the map's sprite,
// or sprites/waypoint.spr), its label and its distance in meters, drawn with the HUD over everything (through walls).
// A spot off the screen puts the icon on the screen's edge with an arrow (sprites/waypoint_arrow.spr) toward it.
//
// Where: the main view projects every marker as it is drawn (tri.cpp, Waypoint_ViewDrawn: the triangle API's
// WorldToScreen with that view's matrices, and the view's vectors for a spot behind the eye); HUD_Redraw draws them
// (Waypoint_Redraw) with the HUD elements, so hud_draw 0, a camera's "Hide HUD", a server menu and the intermission
// hide them. A frame whose main view is not a world view (the spectator's full-screen overview map) draws none.
// A marker with a Follow Entity sits on that entity as the client draws it (its origin + the offset) while the entity
// is in the client's update, and where the server last put it otherwise.
//
// Size: a fixed icon of WAYPOINT_ICON_SIZE units of the 640 HUD, times the HUD's scale: hud_scale's factor
// (hud_scale.cpp) and the HUD sprite set's resolution against the 640 set (CHud::m_iRes), as env_overlay sizes its
// pictures. The text is the console font (its own size, as the HUD's other console text with hud_scale). Icons need
// the OpenGL renderer; the software renderer draws the text only.
//
// Cost: nothing while no marker is on; per frame, per marker, one projection and a few quads.
//=========================================================

#include <math.h>	// before cl_util.h (its fabs macro)
#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "cl_entity.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "waypoint.h"
#include "waypoint_core.h"
#include "pm_shared.h"		// OBS_IN_EYE

extern engine_studio_api_t IEngineStudio;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAYPOINT_SPRITE			"sprites/waypoint.spr"
#define WAYPOINT_ARROW_SPRITE	"sprites/waypoint_arrow.spr"
#define WAYPOINT_ICON_SIZE		32.0f	// 640 HUD units
#define WAYPOINT_ARROW_SIZE		20.0f
#define WAYPOINT_EDGE_GAP		6.0f	// between the screen's edge and the arrow
#define WAYPOINT_MAX_MODELS		512		// the engine's model precache list (MAX_MODELS)

// the views a marker is drawn in: the main view, and a spectator's first-person inset
#define WAYPOINT_VIEW_MAIN		0
#define WAYPOINT_VIEW_INSET		1
#define WAYPOINT_VIEWS			2

struct client_waypoint_t
{
	waypoint::Marker	m;
	bool				placed[ WAYPOINT_VIEWS ];	// projected by this frame's view, in range, shown in it
	waypoint::Place		place[ WAYPOINT_VIEWS ];
	float				dist[ WAYPOINT_VIEWS ];		// units from that view's eye
};

static client_waypoint_t s_Markers[ WAYPOINT_MAX ];
static int s_iMarkers = 0;
static bool s_bViewDrawn[ WAYPOINT_VIEWS ];	// this frame's view projected the markers
static int s_iViewRect[ WAYPOINT_VIEWS ][4];	// x, y, w, h of the view on the screen

static HSPRITE s_hIcon = 0, s_hArrow = 0;

// the map's sprites drawn added (an additive sprite file); -1 not looked at yet
static signed char s_SpriteAdditive[ WAYPOINT_MAX_MODELS ];

static void Waypoint_Remove( int id )
{
	for ( int i = 0; i < s_iMarkers; i++ )
	{
		if ( s_Markers[i].m.id == id )
		{
			s_Markers[i] = s_Markers[ --s_iMarkers ];
			return;
		}
	}
}

static int MsgFunc_Waypoint( const char *pszName, int iSize, void *pbuf )
{
	waypoint::Marker m;
	if ( !waypoint::Parse( (const unsigned char *)pbuf, iSize, m ) )
	{
		gEngfuncs.Con_DPrintf( "info_waypoint: bad message (%d bytes)\n", iSize );
		return 1;
	}

	if ( !( m.flags & WAYPOINT_FLAG_ON ) )
	{
		if ( m.id == 0 )
			s_iMarkers = 0;	// the server sends every marker that is on next
		else
			Waypoint_Remove( m.id );
		return 1;
	}

	// the same marker again (a change, a fullupdate): update it
	for ( int i = 0; i < s_iMarkers; i++ )
	{
		if ( s_Markers[i].m.id == m.id )
		{
			s_Markers[i].m = m;
			memset( s_Markers[i].placed, 0, sizeof( s_Markers[i].placed ) );
			return 1;
		}
	}
	if ( s_iMarkers >= WAYPOINT_MAX )
	{
		gEngfuncs.Con_DPrintf( "info_waypoint: more than %d markers on, %d not shown\n", WAYPOINT_MAX, m.id );
		return 1;
	}
	s_Markers[ s_iMarkers ].m = m;
	memset( s_Markers[ s_iMarkers ].placed, 0, sizeof( s_Markers[ s_iMarkers ].placed ) );
	s_iMarkers++;
	return 1;
}

void Waypoint_Init( void )
{
	gEngfuncs.pfnHookUserMsg( (char *)WAYPOINT_MSG_NAME, MsgFunc_Waypoint );
}

void Waypoint_Reset( void )
{
	s_iMarkers = 0;
	memset( s_bViewDrawn, 0, sizeof( s_bViewDrawn ) );
}

void Waypoint_VidInit( void )
{
	Waypoint_Reset();
	memset( s_SpriteAdditive, -1, sizeof( s_SpriteAdditive ) );
	s_hIcon = SPR_Load( WAYPOINT_SPRITE );
	s_hArrow = SPR_Load( WAYPOINT_ARROW_SPRITE );
}

// where the marker is now: on its Follow Entity while the client has it in this update
static void Waypoint_Origin( const waypoint::Marker &m, float *out )
{
	VectorCopy( m.origin, out );
	if ( !m.follow )
		return;
	cl_entity_t *pEntity = gEngfuncs.GetEntityByIndex( m.follow );
	cl_entity_t *pLocal = gEngfuncs.GetLocalPlayer();
	if ( !pEntity || !pLocal || !pEntity->model )
		return;
	if ( pEntity != pLocal && pEntity->curstate.messagenum != pLocal->curstate.messagenum )
		return;
	VectorAdd( pEntity->origin, m.offset, out );
}

static void Waypoint_Sizes( float &icon, float &arrow, float &gap )
{
	// the HUD's scale: hud_scale and the sprite set's resolution against the 640 set (env_overlay's)
	const float factor = HUD_GetScale() * ( gHUD.m_iRes / 640.0f );
	icon = WAYPOINT_ICON_SIZE * factor;
	arrow = WAYPOINT_ARROW_SIZE * factor;
	gap = WAYPOINT_EDGE_GAP * factor;
}

// a marker shown in this view: everybody's markers and this client's own; a marker of a player a spectator watches
// (owner) only while the view is that player's first-person view (the main view in eye, the first-person inset)
static bool Waypoint_ShownIn( const waypoint::Marker &m, int iView )
{
	if ( !m.owner )
		return true;
	if ( iView == WAYPOINT_VIEW_MAIN )
		return g_iUser1 == OBS_IN_EYE && g_iUser2 == m.owner;
	return g_iUser1 && g_iUser2 == m.owner;	// the inset: projected only when it is first person (tri.cpp)
}

void Waypoint_ViewDrawn( int iView )
{
	if ( !s_iMarkers || iView < 0 || iView >= WAYPOINT_VIEWS || s_bViewDrawn[ iView ] )
		return;	// the main view is the frame's first; a later one (a mirror) is not projected
	s_bViewDrawn[ iView ] = true;

	vec3_t vieworg, up, right, forward;
	IEngineStudio.GetViewInfo( vieworg, up, right, forward );

	// the view's rectangle: the screen, or the spectator's inset window (view.cpp's inset viewport)
	int *rect = s_iViewRect[ iView ];
	if ( iView == WAYPOINT_VIEW_INSET )
	{
		rect[0] = XRES( gHUD.m_Spectator.m_OverviewData.insetWindowX );
		rect[1] = YRES( gHUD.m_Spectator.m_OverviewData.insetWindowY );
		rect[2] = XRES( gHUD.m_Spectator.m_OverviewData.insetWindowWidth );
		rect[3] = YRES( gHUD.m_Spectator.m_OverviewData.insetWindowHeight );
	}
	else
	{
		rect[0] = rect[1] = 0;
		rect[2] = ScreenWidth;
		rect[3] = ScreenHeight;
	}
	if ( rect[2] <= 0 || rect[3] <= 0 )
		return;

	float icon, arrow, gap;
	Waypoint_Sizes( icon, arrow, gap );
	const float margin = icon * 0.5f + arrow + gap;
	// and above the HUD's bottom row (health, armor, ammo: two of its number heights, hud.cpp m_iFontHeight)
	const float hudRow = iView == WAYPOINT_VIEW_MAIN ? 2.0f * gHUD.m_iFontHeight * HUD_GetScale() : 0.0f;
	const float w = (float)rect[2], h = (float)rect[3];

	waypoint::Place places[ WAYPOINT_MAX ];
	int index[ WAYPOINT_MAX ], n = 0;
	for ( int i = 0; i < s_iMarkers; i++ )
	{
		client_waypoint_t &c = s_Markers[i];
		c.placed[ iView ] = false;
		if ( !Waypoint_ShownIn( c.m, iView ) )
			continue;
		vec3_t pos, dir, screen = { 0, 0, 0 };
		Waypoint_Origin( c.m, pos );
		VectorSubtract( pos, vieworg, dir );
		c.dist[ iView ] = Length( dir );
		if ( !waypoint::InRange( c.m, c.dist[ iView ] ) )
			continue;	// closer than its min range or farther than its max: no marker, no edge arrow

		const bool front = DotProduct( dir, forward ) > 1.0f;
		if ( front )
			gEngfuncs.pTriAPI->WorldToScreen( pos, screen );
		const waypoint::Place p = waypoint::Locate( screen[0], screen[1], front, DotProduct( dir, right ),
			DotProduct( dir, up ), w, h, margin, margin, margin + hudRow );
		if ( p.edge && ( c.m.flags & WAYPOINT_FLAG_NO_EDGE ) )
			continue;	// "No edge arrow": only while its spot is in view
		c.place[ iView ] = p;
		c.placed[ iView ] = true;
		places[n] = p;
		index[n++] = i;
	}

	// markers on the edge in about the same direction: apart along the edge (the icon and two lines of text)
	int lineW = 0, lineH = 0;
	gEngfuncs.pfnDrawConsoleStringLen( "M", &lineW, &lineH );
	waypoint::Spread( places, n, icon + 4 * lineW, icon + 2 * lineH + 4, w, h, margin, margin, margin + hudRow );
	for ( int k = 0; k < n; k++ )
	{
		places[k].x += rect[0];
		places[k].y += rect[1];
		s_Markers[ index[k] ].place[ iView ] = places[k];
	}
}

// an additive sprite file (its header's format 1) is drawn added, every other one blended by its alpha
static bool Waypoint_Additive( int sprite, const struct model_s *pModel )
{
	if ( sprite <= 0 || sprite >= WAYPOINT_MAX_MODELS )
		return false;
	if ( s_SpriteAdditive[ sprite ] < 0 )
	{
		s_SpriteAdditive[ sprite ] = 0;
		int len = 0;
		byte *data = gEngfuncs.COM_LoadFile( (char *)pModel->name, 5, &len );
		if ( data )
		{
			// "IDSP", version, type, format (little endian ints)
			if ( len >= 16 && !memcmp( data, "IDSP", 4 ) )
				s_SpriteAdditive[ sprite ] = ( data[12] == 1 && !data[13] && !data[14] && !data[15] ) ? 1 : 0;
			gEngfuncs.COM_FreeFile( data );
		}
	}
	return s_SpriteAdditive[ sprite ] > 0;
}

// a quad of the sprite's frame 0 around cx, cy, size s, turned by angle (radians, clockwise on the screen)
static bool Waypoint_DrawSprite( const struct model_s *pModel, bool additive, float cx, float cy, float s, float angle,
	int r, int g, int b, int a )
{
	triangleapi_s *tri = gEngfuncs.pTriAPI;
	if ( !pModel || !tri->SpriteTexture( (struct model_s *)pModel, 0 ) )
		return false;

	tri->RenderMode( additive ? kRenderTransAdd : kRenderTransAlpha );
	if ( additive )
		tri->Color4ub( (unsigned char)( r * a / 255 ), (unsigned char)( g * a / 255 ), (unsigned char)( b * a / 255 ), 255 );
	else
		tri->Color4ub( (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a );

	const float ca = cosf( angle ) * s * 0.5f, sa = sinf( angle ) * s * 0.5f;
	// corners (-1,-1) (1,-1) (1,1) (-1,1) turned: x' = x cos - y sin, y' = x sin + y cos
	const float cxs[4] = { -1, 1, 1, -1 }, cys[4] = { -1, -1, 1, 1 };
	const float u[4] = { 0, 1, 1, 0 }, v[4] = { 0, 0, 1, 1 };
	tri->Begin( TRI_QUADS );
	for ( int k = 0; k < 4; k++ )
	{
		tri->TexCoord2f( u[k], v[k] );
		tri->Vertex3f( cx + cxs[k] * ca - cys[k] * sa, cy + cxs[k] * sa + cys[k] * ca, 0.0f );
	}
	tri->End();

	tri->RenderMode( kRenderNormal );
	tri->Color4ub( 255, 255, 255, 255 );
	return true;
}

// console text with a one-pixel shadow, x, y its top left
static void Waypoint_DrawText( int x, int y, const char *text, int r, int g, int b )
{
	gEngfuncs.pfnDrawSetTextColor( 0.0f, 0.0f, 0.0f );
	gEngfuncs.pfnDrawConsoleString( x + 1, y + 1, (char *)text );
	gEngfuncs.pfnDrawSetTextColor( r / 255.0f, g / 255.0f, b / 255.0f );
	gEngfuncs.pfnDrawConsoleString( x, y, (char *)text );
}

static void Waypoint_DrawView( int iView );

void Waypoint_Redraw( float flTime, bool bShow )
{
	bool bViewDrawn[ WAYPOINT_VIEWS ];
	memcpy( bViewDrawn, s_bViewDrawn, sizeof( bViewDrawn ) );
	memset( s_bViewDrawn, 0, sizeof( s_bViewDrawn ) );	// the next frame's views project them again
	if ( !bShow || !s_iMarkers )
		return;
	for ( int v = 0; v < WAYPOINT_VIEWS; v++ )
	{
		if ( bViewDrawn[v] )
			Waypoint_DrawView( v );
	}
}

static void Waypoint_DrawView( int iView )
{

	float icon, arrow, gap;
	Waypoint_Sizes( icon, arrow, gap );
	const bool bHardware = HUD_IsHardware();
	const struct model_s *pArrow = s_hArrow > 0 ? gEngfuncs.GetSpritePointer( s_hArrow ) : NULL;
	const struct model_s *pDefault = s_hIcon > 0 ? gEngfuncs.GetSpritePointer( s_hIcon ) : NULL;
	const int *rect = s_iViewRect[ iView ];
	const int x0 = rect[0], y0 = rect[1], x1 = rect[0] + rect[2], y1 = rect[1] + rect[3];
	const int shadow = icon >= 32.0f ? 2 : 1;

	for ( int i = 0; i < s_iMarkers; i++ )
	{
		const client_waypoint_t &c = s_Markers[i];
		if ( !c.placed[ iView ] )
			continue;
		const waypoint::Marker &m = c.m;
		const waypoint::Place &place = c.place[ iView ];
		const float x = place.x, y = place.y;

		if ( bHardware )
		{
			const struct model_s *pIcon = pDefault;
			bool additive = false;
			if ( m.sprite > 0 && m.sprite < WAYPOINT_MAX_MODELS )
			{
				const struct model_s *pModel = gEngfuncs.hudGetModelByIndex( m.sprite );
				if ( pModel && pModel->type == mod_sprite )
				{
					pIcon = pModel;
					additive = Waypoint_Additive( m.sprite, pModel );
				}
			}
			// a dark shadow under a blended icon keeps it readable on bright walls
			if ( !additive )
				Waypoint_DrawSprite( pIcon, false, x + shadow, y + shadow, icon, 0.0f, 0, 0, 0, 160 );
			Waypoint_DrawSprite( pIcon, additive, x, y, icon, 0.0f, m.r, m.g, m.b, 255 );

			if ( place.edge && pArrow )
			{
				// beside the icon, toward the spot (the arrow sprite points right)
				const float d = icon * 0.5f + arrow * 0.5f + gap * 0.5f;
				const float ax = x + cosf( place.angle ) * d, ay = y + sinf( place.angle ) * d;
				Waypoint_DrawSprite( pArrow, false, ax + shadow, ay + shadow, arrow, place.angle, 0, 0, 0, 160 );
				Waypoint_DrawSprite( pArrow, false, ax, ay, arrow, place.angle, m.r, m.g, m.b, 255 );
			}
		}

		// the label, and under it the distance, below the icon (above it near the screen's bottom)
		char szDist[ 16 ] = "";
		if ( m.flags & WAYPOINT_FLAG_DISTANCE )
			waypoint::FormatDistance( c.dist[ iView ], szDist, sizeof( szDist ) );
		const char *lines[2] = { m.label, szDist };
		int lineW[2] = { 0, 0 }, lineH = 0, n = 0;
		for ( int k = 0; k < 2; k++ )
		{
			if ( !lines[k][0] )
				continue;
			int lw = 0, lh = 0;
			gEngfuncs.pfnDrawConsoleStringLen( lines[k], &lw, &lh );
			lineW[k] = lw;
			if ( lh > lineH )
				lineH = lh;
			n++;
		}
		if ( !n )
			continue;

		int ty = (int)( y + icon * 0.5f + 2 );
		if ( ty + n * lineH > y1 - 2 || ( place.edge && sinf( place.angle ) > 0.7f ) )
			ty = (int)( y - icon * 0.5f - 2 ) - n * lineH;
		if ( ty < y0 + 2 )
			ty = y0 + 2;
		for ( int k = 0; k < 2; k++ )
		{
			if ( !lines[k][0] )
				continue;
			int tx = (int)( x - lineW[k] * 0.5f );
			if ( tx > x1 - lineW[k] - 2 )
				tx = x1 - lineW[k] - 2;
			if ( tx < x0 + 2 )
				tx = x0 + 2;
			Waypoint_DrawText( tx, ty, lines[k], m.r, m.g, m.b );
			ty += lineH;
		}
	}
}
