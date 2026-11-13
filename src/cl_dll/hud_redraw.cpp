/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "pm_shared.h"	// SBO 2.0: OBS_MAP_FREE, OBS_MAP_CHASE

#include "viewport.h"
#include "map_music.h"	// SBO 2.0: ambient_music
#include "worldscreen/worldscreen.h"
#include "video/video_player.h"	// SBO 2.0: game_playvideo
#include "overlay.h"	// SBO 2.0: env_overlay
#include "inventory.h"	// SBO 2.0: inventory HUD row
#include "colorcorrection.h"	// SBO 2.0: color_correction

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};


extern int g_iVisibleMouse;
bool ServerMenu_HidesHud();	// SBO 2.0: vgui2/sbo_server_menu_bridge.cpp

float HUD_GetFOV( void );

#include "client_os_shared.h"	// SBO 2.0: cl_sbo_os

extern float IN_GetMouseSensitivity( void );	// HL25: clamped sensitivity

// Think
void CHud::Think(void)
{
	int newfov;
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	// SBO 2.0 client info: cl_sbo_os always names the build's OS (the server reads it with a cvar query)
	if ( m_pCvarClientOS && strcmp( m_pCvarClientOS->string, CLIENT_OS_VALUE ) )
		gEngfuncs.Cvar_Set( (char *)CLIENT_OS_CVAR, (char *)CLIENT_OS_VALUE );

	newfov = HUD_GetFOV();
	if ( newfov == 0 )
	{
		m_iFOV = default_fov->value;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		// HL25: clamped sensitivity, and no division by a default_fov of 0
		m_flMouseSensitivity = IN_GetMouseSensitivity() * ((float)newfov / (float)max( default_fov->value, 90.0f )) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = max( default_fov->value, 90 );  
	}

	// Steam SDK (halflife-updated): an HLTV spectator uses the director camera's FOV (DRC_CMD_CAMERA / WAYPOINTS;
	// 90 otherwise). SBO: ignore a 0 FOV from the proxy.
	if ( gEngfuncs.IsSpectateOnly() && gHUD.m_Spectator.GetFOV() > 0 )
	{
		m_iFOV = gHUD.m_Spectator.GetFOV();	// default_fov->value;
	}

	// SBO 2.0: over a map overview the server sends the watched player's FOV (observer.cpp
	// Observer_CheckProperties) for the first-person inset. The main view is the map, so the HUD keeps the
	// spectator's own FOV (no zoomed crosshair, scope overlay or mouse sensitivity); the frame is drawn with the
	// watched player's FOV only while the first-person inset shows that player, and the overview pass makes up
	// for it (OverviewFovScale). Before: the zoom went into the whole frame, so a zoomed player zoomed the map.
	m_iRenderFOV = m_iFOV;
	if ( !gEngfuncs.IsSpectateOnly() && ( g_iUser1 == OBS_MAP_FREE || g_iUser1 == OBS_MAP_CHASE ) )
	{
		if ( m_Spectator.InsetMode() != INSET_IN_EYE )
			m_iRenderFOV = OwnFOV();
		m_iFOV = OwnFOV();
		m_flMouseSensitivity = 0;
	}
}

int CHud::OwnFOV( void )
{
	int fov = default_fov ? (int)default_fov->value : 0;
	return fov > 0 ? fov : 90;
}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static int m_flShotTime = 0;
	
	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	MapMusic_Redraw();	// SBO 2.0: ambient_music: the server's last MapMusic, now that the client is in the game

	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if ( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
			gViewPort->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideVGUIMenu();
			gViewPort->ShowScoreBoard();
			gViewPort->UpdateSpectatorPanel();

			// Take a screenshot if the client's got the cvar set
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	WorldScreen_Frame();	// SBO 2.0 world screens: cursor, crosshair, repaint budget

	m_iIntermission = intermission;

	// SBO 2.0: color_correction: the frame's last view, if it had no view model to grade it after (before env_overlay
	// and the HUD, which keep their colors)
	ColorCorr_FrameEnd();

	// SBO 2.0: env_overlay: under the HUD (the HUD and the VGUI2 panels are drawn over it); a map's picture, not a
	// HUD element: hud_draw and the HUD hiding don't hide it
	Overlay_Redraw( flTime );

	// if no redrawing is necessary
	// return 0;
	
	if ( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while (pList)
		{
			if ( !intermission )
			{
				// SBO 2.0: no HUD while a server menu is up (vgui2/sbo_server_menu.h). The map's game_text / env_message
				// text (m_Message) stays while the HUD is hidden (HIDEHUD_ALL: a trigger_camera's "Hide HUD and
				// crosshair"): a camera scene's captions
				if ( (pList->p->m_iFlags & HUD_ACTIVE) && ( !(m_iHideHUDDisplay & HIDEHUD_ALL) || pList->p == &m_Message )
					&& !ServerMenu_HidesHud() )
					pList->p->Draw(flTime);
			}
			else
			{  // it's an intermission,  so only draw hud elements that are set to draw during intermissions
				if ( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}

			pList = pList->pNext;
		}

		// SBO 2.0: the inventory row, hidden as the HUD elements are
		if ( !intermission && !(m_iHideHUDDisplay & HIDEHUD_ALL) && !ServerMenu_HidesHud() )
			Inventory_Redraw( flTime );
	}


	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = GetWidth() - x;	// HUD scaling: HUD units
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

	Video_Redraw( flTime );	// SBO 2.0: game_playvideo: the video over the whole screen (Aura: end of HUD_Redraw)

	/*
	if ( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );
		
		if (m_hsprCursor == 0)
		{
			char sz[256];
			sprintf( sz, "sprites/cursor.spr" );
			m_hsprCursor = SPR_Load( sz );
		}

		SPR_Set(m_hsprCursor, 250, 250, 250 );
		
		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

int CHud :: DrawHudString(int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	// HUD scaling: positions are HUD units, but the characters keep the font's size in pixels, so the string is
	// walked in pixels (HUD_ToScreen*/HUD_ToHud* are the identity without hud_scale)
	int x = HUD_ToScreenX( xpos );
	const int y = HUD_ToScreenY( ypos );
	const int iMaxScreenX = HUD_ToScreenX( iMaxX );

	// draw the string until we hit the null character or a newline character
	for ( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = x + gHUD.m_scrinfo.charWidths[ (unsigned char)*szIt ]; // variable-width fonts look cool (unsigned: chars >127 indexed negatively)
		if ( next > iMaxScreenX )
			break;

		TextMessageDrawChar( x, y, *szIt, r, g, b );
		x = next;		
	}

	return HUD_ToHudX( x );
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

// draws a string from right to left (right-aligned)
int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	// find the end of the string
	char *szIt; for ( szIt = szString; *szIt != 0; szIt++ )
	{ // we should count the length?		
	}

	// HUD scaling: walked in pixels, see DrawHudString
	int x = HUD_ToScreenX( xpos );
	const int y = HUD_ToScreenY( ypos );
	const int iMinScreenX = HUD_ToScreenX( iMinX );

	// iterate throug the string in reverse
	for ( szIt--;  szIt != (szString-1);  szIt-- )	
	{
		int next = x - gHUD.m_scrinfo.charWidths[ (unsigned char)*szIt ]; // variable-width fonts look cool (unsigned: chars >127 indexed negatively)
		if ( next < iMinScreenX )
			break;
		x = next;

		TextMessageDrawChar( x, y, *szIt, r, g, b );
	}

	return HUD_ToHudX( x );
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b)
{
	int iWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;
	int k;
	
	if (iNumber > 0)
	{
		// SBO: SPR_Draw 10000's
		if (iNumber >= 10000)
		{
			k = iNumber/10000;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_5DIGITS))
		{
			x += iWidth;
		}

		// SBO: SPR_Draw 1000's
		if (iNumber >= 1000)
		{
			k = (iNumber % 10000)/1000;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_5DIGITS | DHN_4DIGITS))
		{
			x += iWidth;
		}

		// SPR_Draw 100's
		if (iNumber >= 100)
		{
			k = (iNumber % 1000)/100;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_5DIGITS | DHN_4DIGITS | DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if (iNumber >= 10)
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_5DIGITS | DHN_4DIGITS | DHN_3DIGITS | DHN_2DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
		x += iWidth;
	} 
	else if (iFlags & DHN_DRAWZERO) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		if (iFlags & (DHN_5DIGITS | DHN_4DIGITS | DHN_3DIGITS | DHN_2DIGITS))
		{
			x += iWidth;
		}

		if (iFlags & (DHN_5DIGITS | DHN_4DIGITS | DHN_3DIGITS))
		{
			x += iWidth;
		}

		if (iFlags & (DHN_5DIGITS | DHN_4DIGITS))
		{
			x += iWidth;
		}

		if (iFlags & (DHN_5DIGITS))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}

	return x;
}


int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if (iFlags & (DHN_3DIGITS))
		return 3;

	if (iFlags & (DHN_2DIGITS))
		return 2;

	if (iNumber <= 0)
	{
		if (iFlags & (DHN_DRAWZERO))
			return 1;
		else
			return 0;
	}

	if (iNumber < 10)
		return 1;

	if (iNumber < 100)
		return 2;

	return 3;

}	

// halflife-updated (halflife#2149): health can now exceed 999, so the health HUD reserves
// 3 digits by default but grows to fit the number, drawing digits right to left.
int CHud::GetHudNumberWidth(int number, int width, int flags)
{
	const int digitWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;

	int totalDigits = 0;

	if (number > 0)
	{
		totalDigits = static_cast<int>(log10(number)) + 1;
	}
	else if ((flags & DHN_DRAWZERO) != 0)
	{
		totalDigits = 1;
	}

	if (totalDigits < width)
		totalDigits = width;

	return totalDigits * digitWidth;
}

int CHud::DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b)
{
	if (number > 0 || (flags & DHN_DRAWZERO) != 0)
	{
		const int digitWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;

		int remainder = number;

		do
		{
			const int digit = remainder % 10;
			const int digitSpriteIndex = m_HUD_number_0 + digit;

			//This has to happen *before* drawing because we're drawing in reverse
			x -= digitWidth;

			SPR_Set(GetSprite(digitSpriteIndex), r, g, b);
			SPR_DrawAdditive(0, x, y, &GetSpriteRect(digitSpriteIndex));

			remainder /= 10;
		} while (remainder > 0);
	}

	return x;
}


