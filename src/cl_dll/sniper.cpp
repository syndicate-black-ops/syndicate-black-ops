//
// sniper.cpp
//
// SBO: implementation of CHudSniper class (sniper scope overlay)
//
// SBO 2.0: the scope's sprites are not in sprites/hud.txt (every entry of it is loaded on every client, drawn or
// not). VidInit loads the one family this client draws, by name:
//  * hardware renderer: one quadrant of the scope picture, sprites/scope_quad_*.spr (scripts/gen_scope_sprites.py),
//    drawn four times, mirrored, as high as the screen, black to the screen edges (DrawScaled);
//  * software renderer (it can't stretch sprites): 1.38's pictures at pixel size with 1.38's code (Draw).
// The renderer doesn't change while the game runs, so a client loads one family.
//

#include "hud.h"
#include "cl_util.h"

// scope_quad_*.spr: 512x512 tiles, the picture inside a border of 1 pixel (gen_scope_sprites.py: TILE, BORDER)
#define SCOPE_TILE			512
#define SCOPE_BORDER		1
#define SCOPE_INNER			( SCOPE_TILE - 2 * SCOPE_BORDER )
#define SCOPE_SMALL_MAX		1020	// a picture up to this high uses scope_quad_small.spr (a picture of 1020 pixels)

// SBO 2.0: loads sprites/<name>.spr; 0 if the file is missing or not a sprite (the engine is only handed valid files)
static HSPRITE ScopeLoadSprite( const char *pszName )
{
	char sz[64];
	sprintf( sz, "sprites/%s.spr", pszName );

	int iLength = 0;
	byte *pFile = gEngfuncs.COM_LoadFile( sz, 5, &iLength );
	const bool bValid = pFile && iLength > 4 && !memcmp( pFile, "IDSP", 4 );

	if ( pFile )
		gEngfuncs.COM_FreeFile( pFile );

	if ( !bValid )
	{
		gEngfuncs.Con_DPrintf( "Sniper scope: missing or invalid sprite %s\n", sz );
		return 0;
	}

	return SPR_Load( sz );
}

// SBO 2.0: one axis of the stretched scope: the quadrant's tiles from one edge of the picture (start) to its middle
// and, mirrored, on to the other edge. Each piece: the screen pixels a..b, the tile, and the tile's texels at a and b.
// Neighbouring pieces share their screen edge and continue each other's texels, so there are no seams; the two
// pieces at the middle end on the mirror axis (gen_scope_sprites.py: "The mirror axes").
struct scope_piece_t
{
	int a, b;
	int tile;
	float ta, tb;
};

static int ScopePieces( int start, int side, int tiles, scope_piece_t *piece )
{
	const int mid = start + side / 2;
	const int end = start + side;
	const float flOuter = SCOPE_BORDER;
	const float flInner = SCOPE_TILE - SCOPE_BORDER;

	if ( tiles == 1 )
	{
		// small set: the axis is the edge of the tile's last pixel
		const scope_piece_t p[2] =
		{
			{ start, mid, 0, flOuter, flInner },
			{ mid, end, 0, flInner, flOuter },
		};
		memcpy( piece, p, sizeof( p ) );
		return 2;
	}

	// 2 tiles: 510 pixels of the first, 510.5 of the second (the axis runs through the middle of its last pixel)
	const float flAxis = SCOPE_TILE - 0.5f;
	const int split = (int)( ( side / 2 ) * SCOPE_INNER / ( 2 * SCOPE_INNER + 0.5f ) + 0.5f );
	const scope_piece_t p[4] =
	{
		{ start, start + split, 0, flOuter, flInner },
		{ start + split, mid, 1, flOuter, flAxis },
		{ mid, end - split, 1, flAxis, flOuter },
		{ end - split, end, 0, flInner, flOuter },
	};
	memcpy( piece, p, sizeof( p ) );
	return 4;
}

// SBO 2.0: true when the scope is the stretched quadrant (DrawScaled): on the hardware renderer, at every resolution.
// The software renderer can't stretch: 1.38's pictures and code.
bool CHudSniper::UseScaled( void )
{
	return HUD_IsHardware();
}

// SBO 2.0: loads the quadrant for this screen: 2x2 tiles (a picture of 2041 pixels) or the small one (1020 pixels)
// by the height of the picture on screen; the other one if a file is missing; none: no overlay.
void CHudSniper::LoadQuads( void )
{
	const bool bSmall = min( ScreenWidth, ScreenHeight ) <= SCOPE_SMALL_MAX;

	m_iQuadTiles = 0;

	for ( int pass = 0; pass < 2 && !m_iQuadTiles; pass++ )
	{
		if ( bSmall == ( pass == 0 ) )
		{
			m_hQuad[0][0] = ScopeLoadSprite( "scope_quad_small" );
			if ( m_hQuad[0][0] )
				m_iQuadTiles = 1;
		}
		else
		{
			m_iQuadTiles = 2;
			for ( int row = 0; row < 2; row++ )
			{
				for ( int col = 0; col < 2; col++ )
				{
					char szName[32];
					sprintf( szName, "scope_quad_%d%d", row + 1, col + 1 );
					m_hQuad[row][col] = ScopeLoadSprite( szName );
					if ( !m_hQuad[row][col] )
						m_iQuadTiles = 0;
				}
			}
		}
	}
}

int CHudSniper::VidInit( void )
{
	m_hCenter = 0;
	m_hTopLeft = 0;
	m_hVBlack = 0;
	m_hHBlack = 0;
	m_hTop = 0;
	m_hTopRight = 0;
	m_hLeft = 0;
	m_hRight = 0;
	m_hBottomLeft = 0;
	m_hBottom = 0;
	m_hBottomRight = 0;
	m_iBorderX = 0;
	m_iBorderY = 0;
	m_iQuadTiles = 0;
	m_bClassic = false;

	if ( UseScaled() )
	{
		LoadQuads();
		return 1;
	}

	// SBO 2.0: 1.38 took these from sprites/hud.txt ("center", "top_left" .., "1top_left" .., "vblack", "hblack");
	// loaded by name now, only the ones this screen draws. The rects are the whole frames, as hud.txt had them.
	m_prcCenter = &m_rcClassic[0];
	m_prcTopLeft = &m_rcClassic[1];
	m_prcTop = &m_rcClassic[2];
	m_prcTopRight = &m_rcClassic[3];
	m_prcLeft = &m_rcClassic[4];
	m_prcRight = &m_rcClassic[5];
	m_prcBottomLeft = &m_rcClassic[6];
	m_prcBottom = &m_rcClassic[7];
	m_prcBottomRight = &m_rcClassic[8];
	m_prcVBlack = &m_rcClassic[9];
	m_prcHBlack = &m_rcClassic[10];
	memset( m_rcClassic, 0, sizeof( m_rcClassic ) );

	if ( ScreenWidth < 640 )
		return 1;

	const bool b1024 = ScreenWidth > 800;
	const bool bVBlack = ScreenWidth >= 800;
	const bool bHBlack = ScreenWidth > 1024 || ScreenWidth == 800;

	m_hCenter = ScopeLoadSprite( "centre" );
	m_hTopLeft = ScopeLoadSprite( b1024 ? "1024top_left" : "640top_left" );
	m_hTop = ScopeLoadSprite( b1024 ? "1024top" : "640top" );
	m_hTopRight = ScopeLoadSprite( b1024 ? "1024top_right" : "640top_right" );
	m_hLeft = ScopeLoadSprite( b1024 ? "1024left" : "640left" );
	m_hRight = ScopeLoadSprite( b1024 ? "1024right" : "640right" );
	m_hBottomLeft = ScopeLoadSprite( b1024 ? "1024bottom_left" : "640bottom_left" );
	m_hBottom = ScopeLoadSprite( b1024 ? "1024bottom" : "640bottom" );
	m_hBottomRight = ScopeLoadSprite( b1024 ? "1024bottom_right" : "640bottom_right" );
	if ( bVBlack )
		m_hVBlack = ScopeLoadSprite( "vblack" );
	if ( bHBlack )
		m_hHBlack = ScopeLoadSprite( "hblack" );

	const HSPRITE sprites[11] =
	{
		m_hCenter, m_hTopLeft, m_hTop, m_hTopRight, m_hLeft, m_hRight, m_hBottomLeft, m_hBottom, m_hBottomRight,
		m_hVBlack, m_hHBlack,
	};

	m_bClassic = true;
	for ( int i = 0; i < 11; i++ )
	{
		if ( sprites[i] )
		{
			m_rcClassic[i].right = gEngfuncs.pfnSPR_Width( sprites[i], 0 );
			m_rcClassic[i].bottom = gEngfuncs.pfnSPR_Height( sprites[i], 0 );
		}
		else if ( i < 9 || ( i == 9 && bVBlack ) || ( i == 10 && bHBlack ) )
		{
			m_bClassic = false;
		}
	}

	if ( ScreenWidth > 1024 )
	{
		m_iBorderY = ( ScreenHeight - 768 ) / 2;
		m_prcHBlack->bottom = m_iBorderY;
		m_iBorderX = ( ScreenWidth - 768 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		return 1;
	}
	else if ( ScreenWidth > 800 )
	{
		m_iBorderX = ( ScreenWidth - 768 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		return 1;
	}
	else if ( ScreenWidth == 800 )
	{
		m_iBorderX = ( ScreenWidth - 640 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		m_iBorderY = ( ScreenHeight - 480 ) / 2;
		m_prcHBlack->bottom = m_iBorderY;
		return 1;
	}
	else if ( ScreenWidth == 600 )
	{
		m_iBorderY = 0;
		m_iBorderX = 0;
	}

	return 1;
}

int CHudSniper::Draw( float flTime )
{
	WEAPON *pw;
	int x, y;
	int i, j;
	wrect_t nullrc;

	// 1.38 has no scope overlay from 1600 pixels wide (the 768x768 art can't cover the screen); SBO 2.0 has, on the
	// hardware renderer (UseScaled), so this is left for the software renderer
	if ( ScreenWidth >= 1600 && !UseScaled() )
		return 1;

	if ( !gHUD.IsZoomed() )
	{
		if ( m_bDrawn )
		{
			// SBO 2.0: a spectator leaving first person (or the zoomed player it watched) gets the mode's crosshair
			// back, not the weapon's; SBO's own code stays for players
			if ( g_iUser1 )
			{
				gHUD.m_Ammo.UpdateCrosshair();
				m_bDrawn = false;
				return 1;
			}

			pw = gHUD.m_Ammo.GetCurWeapon();
			if ( !pw )
				return 1;

			SetCrosshair( pw->hCrosshair, pw->rcCrosshair, 255, 255, 255 );
			m_bDrawn = false;
		}
		return 1;
	}

	if ( UseScaled() )
		return DrawScaled();

	// SBO 2.0: a sprite is missing: no overlay (the weapon's crosshair stays)
	if ( !m_bClassic )
		return 1;

	x = ( ScreenWidth - ( m_prcCenter->right - m_prcCenter->left ) ) / 2;
	y = ( ScreenHeight - ( m_prcCenter->bottom - m_prcCenter->top ) ) / 2;

	if ( ScreenWidth > 800 )
	{
		if ( ScreenWidth > 800 )
		{
			m_prcVBlack->bottom = 256;
			m_prcVBlack->right = m_iBorderX;
			for ( i = 0; i <= 512; i += 256 )
			{
				SPR_Set( m_hVBlack, 255, 255, 255 );
				SPR_Draw( 0, 0, i + m_iBorderY, m_prcVBlack );
				SPR_Draw( 0, m_iBorderX + 768, i + m_iBorderY, m_prcVBlack );
			}
		}
		else
		{
			m_iBorderX = 0;
		}

		if ( ScreenWidth > 1024 )
		{
			m_prcHBlack->right = 256;
			m_prcHBlack->bottom = m_iBorderY;
			// SBO 2.0: no border at 768 high or less (an empty rect must not reach the engine)
			for ( j = 0; j <= 1024 && m_iBorderY > 0; j += 256 )
			{
				SPR_Set( m_hHBlack, 255, 255, 255 );
				SPR_Draw( 0, j, 0, m_prcHBlack );
				SPR_Draw( 0, j, m_iBorderY + 768, m_prcHBlack );
			}
		}
		else
		{
			m_iBorderY = 0;
		}
	}

	if ( ScreenWidth == 800 )
	{
		SPR_Set( m_hVBlack, 255, 255, 255 );
		SPR_Draw( 0, 0, 0, m_prcVBlack );
		SPR_Draw( 0, 0, 256, m_prcVBlack );
		SPR_Draw( 0, 0, 512, m_prcVBlack );
		SPR_Draw( 0, m_iBorderX + 640, 0, m_prcVBlack );
		SPR_Draw( 0, m_iBorderX + 640, 256, m_prcVBlack );
		SPR_Draw( 0, m_iBorderX + 640, 512, m_prcVBlack );

		SPR_Set( m_hHBlack, 255, 255, 255 );
		SPR_Draw( 0, m_iBorderX, 0, m_prcHBlack );
		SPR_Draw( 0, m_iBorderX + 256, 0, m_prcHBlack );
		SPR_Draw( 0, m_iBorderX + 512, 0, m_prcHBlack );
		SPR_Draw( 0, m_iBorderX, m_iBorderY + 480, m_prcHBlack );
		SPR_Draw( 0, m_iBorderX + 256, m_iBorderY + 480, m_prcHBlack );
		SPR_Draw( 0, m_iBorderX + 512, m_iBorderY + 480, m_prcHBlack );
	}

	if ( ScreenWidth == 640 )
	{
		m_iBorderX = 0;
		m_iBorderY = 0;
	}

	if ( ScreenWidth > 800 )
	{
		SPR_Set( m_hTopLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY, m_prcTopLeft );
		SPR_Set( m_hTop, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 256, m_iBorderY, m_prcTop );
		SPR_Set( m_hTopRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 512, m_iBorderY, m_prcTopRight );
		SPR_Set( m_hLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY + 256, m_prcLeft );
		SPR_Set( m_hRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 512, m_iBorderY + 256, m_prcRight );
		SPR_Set( m_hBottomLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY + 512, m_prcBottomLeft );
		SPR_Set( m_hBottom, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 256, m_iBorderY + 512, m_prcBottom );
		SPR_Set( m_hBottomRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 512, m_iBorderY + 512, m_prcBottomRight );
	}

	if ( ScreenWidth <= 800 && ScreenWidth >= 640 )
	{
		SPR_Set( m_hTopLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY, m_prcTopLeft );
		SPR_Set( m_hTop, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 192, m_iBorderY, m_prcTop );
		SPR_Set( m_hTopRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 448, m_iBorderY, m_prcTopRight );
		SPR_Set( m_hLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY + 112, m_prcLeft );
		SPR_Set( m_hRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 448, m_iBorderY + 112, m_prcRight );
		SPR_Set( m_hBottomLeft, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX, m_iBorderY + 368, m_prcBottomLeft );
		SPR_Set( m_hBottom, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 192, m_iBorderY + 368, m_prcBottom );
		SPR_Set( m_hBottomRight, 255, 255, 255 );
		SPR_DrawHoles( 0, m_iBorderX + 448, m_iBorderY + 368, m_prcBottomRight );
	}

	memset( &nullrc, 0, sizeof( nullrc ) );
	SetCrosshair( 0, nullrc, 0, 0, 0 );

	SPR_Set( m_hCenter, 255, 255, 255 );
	SPR_DrawHoles( 0, x, y, m_prcCenter );
	m_bDrawn = true;

	return 1;
}

// SBO 2.0: the scope picture as a centered square as high as the screen (min: or as wide, on a screen higher than
// wide), as 1.38's is at 1024x768 and 640x480: its top left quadrant (m_hQuad) drawn four times, mirrored. Black from
// it to the screen edges. Screen pixels: the same picture with and without hud_scale.
int CHudSniper::DrawScaled( void )
{
	// a sprite is missing: no overlay (the weapon's crosshair stays)
	if ( !m_iQuadTiles )
		return 1;

	const int width = ScreenWidth;
	const int height = ScreenHeight;
	const int side = min( width, height );
	const int x0 = ( width - side ) / 2;
	const int y0 = ( height - side ) / 2;

	// black around the picture: the black texels in the corner of the first tile, stretched
	SPR_Set( m_hQuad[0][0], 255, 255, 255 );
	if ( x0 > 0 )
	{
		SPR_DrawTexels( 0, 0, 0, x0, height, 2, 2, 6, 6, false );
		SPR_DrawTexels( 0, x0 + side, 0, width, height, 2, 2, 6, 6, false );
	}
	if ( y0 > 0 )
	{
		SPR_DrawTexels( 0, x0, 0, x0 + side, y0, 2, 2, 6, 6, false );
		SPR_DrawTexels( 0, x0, y0 + side, x0 + side, height, 2, 2, 6, 6, false );
	}

	scope_piece_t px[4], py[4];
	const int n = ScopePieces( x0, side, m_iQuadTiles, px );
	ScopePieces( y0, side, m_iQuadTiles, py );

	for ( int row = 0; row < n; row++ )
	{
		for ( int col = 0; col < n; col++ )
		{
			SPR_Set( m_hQuad[py[row].tile][px[col].tile], 255, 255, 255 );
			SPR_DrawTexels( 0, px[col].a, py[row].a, px[col].b, py[row].b, px[col].ta, py[row].ta, px[col].tb, py[row].tb, true );
		}
	}

	wrect_t nullrc;
	memset( &nullrc, 0, sizeof( nullrc ) );
	SetCrosshair( 0, nullrc, 0, 0, 0 );

	m_bDrawn = true;
	return 1;
}

int CHudSniper::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_bDrawn = false;

	return 1;
}
