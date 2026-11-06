//
// sniper.cpp
//
// SBO: implementation of CHudSniper class (sniper scope overlay)
//

#include "hud.h"
#include "cl_util.h"

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

	if ( ScreenWidth > 800 )
	{
		m_iCenter = gHUD.GetSpriteIndex( "center" );
		m_iTopLeft = gHUD.GetSpriteIndex( "top_left" );
		m_iTop = gHUD.GetSpriteIndex( "top" );
		m_iTopRight = gHUD.GetSpriteIndex( "top_right" );
		m_iLeft = gHUD.GetSpriteIndex( "left" );
		m_iRight = gHUD.GetSpriteIndex( "right" );
		m_iBottomLeft = gHUD.GetSpriteIndex( "bottom_left" );
		m_iBottom = gHUD.GetSpriteIndex( "bottom" );
		m_iBottomRight = gHUD.GetSpriteIndex( "bottom_right" );
	}

	if ( ScreenWidth <= 800 && ScreenWidth >= 640 )
	{
		m_iCenter = gHUD.GetSpriteIndex( "center" );
		m_iTopLeft = gHUD.GetSpriteIndex( "1top_left" );
		m_iTop = gHUD.GetSpriteIndex( "1top" );
		m_iTopRight = gHUD.GetSpriteIndex( "1top_right" );
		m_iLeft = gHUD.GetSpriteIndex( "1left" );
		m_iRight = gHUD.GetSpriteIndex( "1right" );
		m_iBottomLeft = gHUD.GetSpriteIndex( "1bottom_left" );
		m_iBottom = gHUD.GetSpriteIndex( "1bottom" );
		m_iBottomRight = gHUD.GetSpriteIndex( "1bottom_right" );
	}

	m_hCenter = gHUD.GetSprite( m_iCenter );
	m_hTopLeft = gHUD.GetSprite( m_iTopLeft );
	m_hTop = gHUD.GetSprite( m_iTop );
	m_hTopRight = gHUD.GetSprite( m_iTopRight );
	m_hLeft = gHUD.GetSprite( m_iLeft );
	m_hRight = gHUD.GetSprite( m_iRight );
	m_hBottomLeft = gHUD.GetSprite( m_iBottomLeft );
	m_hBottom = gHUD.GetSprite( m_iBottom );
	m_hBottomRight = gHUD.GetSprite( m_iBottomRight );

	m_prcCenter = &gHUD.GetSpriteRect( m_iCenter );
	m_prcTopLeft = &gHUD.GetSpriteRect( m_iTopLeft );
	m_prcTop = &gHUD.GetSpriteRect( m_iTop );
	m_prcTopRight = &gHUD.GetSpriteRect( m_iTopRight );
	m_prcLeft = &gHUD.GetSpriteRect( m_iLeft );
	m_prcRight = &gHUD.GetSpriteRect( m_iRight );
	m_prcBottomLeft = &gHUD.GetSpriteRect( m_iBottomLeft );
	m_prcBottom = &gHUD.GetSpriteRect( m_iBottom );
	m_prcBottomRight = &gHUD.GetSpriteRect( m_iBottomRight );

	m_iVBlack = gHUD.GetSpriteIndex( "vblack" );
	m_iHBlack = gHUD.GetSpriteIndex( "hblack" );
	m_hVBlack = gHUD.GetSprite( m_iVBlack );
	m_hHBlack = gHUD.GetSprite( m_iHBlack );

	if ( ScreenWidth > 1024 )
	{
		m_prcHBlack = &gHUD.GetSpriteRect( m_iHBlack );
		m_iBorderY = ( ScreenHeight - 768 ) / 2;
		m_prcHBlack->bottom = m_iBorderY;
		m_prcVBlack = &gHUD.GetSpriteRect( m_iVBlack );
		m_iBorderX = ( ScreenWidth - 768 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		return 1;
	}
	else if ( ScreenWidth > 800 )
	{
		m_prcVBlack = &gHUD.GetSpriteRect( m_iVBlack );
		m_iBorderX = ( ScreenWidth - 768 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		return 1;
	}
	else if ( ScreenWidth == 800 )
	{
		m_prcVBlack = &gHUD.GetSpriteRect( m_iVBlack );
		m_iBorderX = ( ScreenWidth - 640 ) / 2;
		m_prcVBlack->right = m_iBorderX;
		m_prcHBlack = &gHUD.GetSpriteRect( m_iHBlack );
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

	if ( ScreenWidth >= 1600 )
		return 1;

	if ( gHUD.m_iFOV >= 90 )
	{
		if ( m_bDrawn )
		{
			pw = gHUD.m_Ammo.GetCurWeapon();
			if ( !pw )
				return 1;

			SetCrosshair( pw->hCrosshair, pw->rcCrosshair, 255, 255, 255 );
			m_bDrawn = false;
		}
		return 1;
	}

	x = ( ScreenWidth - ( gHUD.GetSpriteRect( m_iCenter ).right - gHUD.GetSpriteRect( m_iCenter ).left ) ) / 2;
	y = ( ScreenHeight - ( gHUD.GetSpriteRect( m_iCenter ).bottom - gHUD.GetSpriteRect( m_iCenter ).top ) ) / 2;

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
			for ( j = 0; j <= 1024; j += 256 )
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

int CHudSniper::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_bDrawn = false;

	return 1;
}
