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
// death notice
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

#include "viewport.h"
#include "vgui2/vgui2_api.h"

DECLARE_MESSAGE( m_DeathNotice, DeathMsg );

struct DeathNoticeItem {
	char szKiller[MAX_PLAYER_NAME_LENGTH*2];
	char szVictim[MAX_PLAYER_NAME_LENGTH*2];
	int iId;	// the index number of the associated sprite
	int iSuicide;
	int iTeamKill;
	int iNonPlayerKill;
	int iHeadshot;	// SBO 2.0: d_headshot after the weapon icon (CS 1.6)
	float flDisplayTime;
	float *KillerColor;
	float *VictimColor;
	float KillerRGB[3];	// chat-vgui2: storage for VGui2_DeathNotice_AddEntry colours (KillerColor/VictimColor point here)
	float VictimRGB[3];
};

#define MAX_DEATHNOTICES	4
static int s_iHudHeadshot = -1;	// SBO 2.0: d_headshot sprite index (-1: none in hud.txt)
static int DEATHNOTICE_DISPLAY_TIME = 6;

#define DEATHNOTICE_TOP		32

DeathNoticeItem rgDeathNoticeList[ MAX_DEATHNOTICES + 1 ];

float g_ColorBlue[3]	= { 0.6, 0.8, 1.0 };
float g_ColorRed[3]		= { 1.0, 0.25, 0.25 };
float g_ColorGreen[3]	= { 0.6, 1.0, 0.6 };
float g_ColorYellow[3]	= { 1.0, 0.7, 0.0 };
float g_ColorGrey[3]	= { 0.8, 0.8, 0.8 };

// chat-vgui2: the one team colour table (death notices, legacy + VGUI2 chat names, VGUI2 scoreboard sections and
// names via sbo::GetTeamColor / sbo::GetClientColor). SBO scoreboard categories (server TeamNames): 1 Game Master
// blue, 2 Dead Ops red, 3 No Loadout yellow, 4 Alive Ops green, 5 Spectators grey.
float *GetTeamColor( int teamNumber )
{
	switch ( teamNumber )
	{
	case 1:	return g_ColorBlue;
	case 2: return g_ColorRed;
	case 3: return g_ColorYellow;
	case 4: return g_ColorGreen;
	case 0: return g_ColorYellow;

		default	: return g_ColorGrey;
	}
}

float *GetClientColor( int clientIndex )
{
	return GetTeamColor( g_PlayerExtraInfo[clientIndex].teamnumber );
}

// Name in the console line: its ^0..^9 / ^xRGB / ^#RRGGBB colour codes drawn as colours (hud_colortext 1,
// docs/CONSOLE_COLOR.md), else removed like BHL's ConsolePrint (kept as typed with hud_colortext 0)
static void DeathNotice_ConsolePrintName( const char *name )
{
	if ( !VGui2_Console_PrintColorCodes( name ) )
		ConsolePrint( VGui2_Chat_RemoveColorCodes( name ) );
}

//-----------------------------------------------------------------------------
// chat-vgui2: kill feed (docs/dev/KILLFEED_PORT.md)
//-----------------------------------------------------------------------------
static bool SboKillFeed_IsLocal( int client )
{
	return client > 0 && client <= MAX_PLAYERS && g_PlayerInfoList[client].thisplayer;
}

// DeathMsg -> VGUI2 kill feed row. weapon: DeathMsg string (no "d_"). Player names must be current
// (GetAllPlayersInfo was called). headshot: the DeathMsg's headshot byte.
static void SboKillFeed_FromDeathMsg( int killer, int victim, const char *weapon, bool headshot )
{
	if ( killer < 0 || killer > MAX_PLAYERS )
		killer = 0;

	const char *killerName = ( killer > 0 ) ? g_PlayerInfoList[killer].name : NULL;

	// Non-player object kill (victim -1, e.g. a sentry gun): "<killer> <skull> <object>"
	if ( ((char)victim) == -1 )
	{
		VGui2_KillFeed_AddEntry( killerName, VGUI2_KF_COLOR_CLIENT( killer ), SboKillFeed_IsLocal( killer ),
			weapon, VGUI2_KF_COLOR_PLAIN, false, "", 0 );
		return;
	}

	if ( victim <= 0 || victim > MAX_PLAYERS )
		return;

	const char *victimName = g_PlayerInfoList[victim].name;
	if ( !victimName || !victimName[0] )
		return;

	int flags = headshot ? VGUI2_KF_FLAG_HEADSHOT : 0;
	int killerColor = VGUI2_KF_COLOR_CLIENT( killer );

	if ( !strcmp( weapon, "teammate" ) )
		flags |= VGUI2_KF_FLAG_TEAMKILL;

	if ( killer == victim )
	{
		flags |= VGUI2_KF_FLAG_SUICIDE;
	}
	else if ( killer == 0 )
	{
		// Killed by a non-player entity (world, func_tank, trigger_hurt, falls, an NPC): like stock, no killer name,
		// "<icon> victim" with the inflictor's d_ icon (d_tracktrain) or the skull -- never its classname as text
		// ("world", "tank"). NPC killer names come from SboDeathMsg (sbo_bridge.cpp), which skips this DeathMsg.
		char spr[64];
		_snprintf( spr, sizeof(spr), "d_%s", weapon );
		spr[sizeof(spr) - 1] = 0;

		if ( gHUD.GetSpriteIndex( spr ) < 0 )
			weapon = "skull";

		flags |= VGUI2_KF_FLAG_SUICIDE;
	}

	VGui2_KillFeed_AddEntry( killerName, killerColor, SboKillFeed_IsLocal( killer ),
		victimName, VGUI2_KF_COLOR_CLIENT( victim ), SboKillFeed_IsLocal( victim ), weapon, flags );
}

// Encoded kill feed colour (vgui2_api.h VGUI2_KF_COLOR_*) -> legacy float colour
static void SboKillFeed_DecodeColor( int code, float *out )
{
	const float *c = NULL;
	static const float npc[3] = { 1.0f, 140 / 255.0f, 90 / 255.0f };
	static const float plain[3] = { 216 / 255.0f, 216 / 255.0f, 216 / 255.0f };

	if ( code == VGUI2_KF_COLOR_NPC || ( code > 0 && ( code & 0x10000000 ) ) )	// an NPC, also a Game Master's
		c = npc;
	else if ( code & 0x40000000 )
	{
		out[0] = ( ( code >> 16 ) & 0xFF ) / 255.0f;
		out[1] = ( ( code >> 8 ) & 0xFF ) / 255.0f;
		out[2] = ( code & 0xFF ) / 255.0f;
		return;
	}
	else if ( code & 0x20000000 )
	{
		int cl = code & 0xFF;
		c = ( cl <= MAX_PLAYERS ) ? GetClientColor( cl ) : plain;
	}
	else if ( code >= 0 )
		c = GetTeamColor( code );
	else
		c = plain;

	out[0] = c[0];
	out[1] = c[1];
	out[2] = c[2];
}

// VGui2_DeathNotice_SkipNextDeathMsg (vgui2_api.h): victim index of a DeathMsg whose notice is replaced, -1 none
static int s_iSboSkipDeathMsgVictim = -1;

void VGui2_DeathNotice_SkipNextDeathMsg( int victimIndex )
{
	s_iSboSkipDeathMsgVictim = victimIndex;
}

// Public kill feed entry point (vgui2_api.h): VGUI2 kill feed, else a row in the legacy list below.
void VGui2_DeathNotice_AddEntry( const char *killerName, int killerColor, bool killerIsLocal,
	const char *victimName, int victimColor, bool victimIsLocal, const char *weaponName, int flags )
{
	if ( !victimName || !victimName[0] )
		return;

	if ( VGui2_KillFeed_IsActive() )
	{
		VGui2_KillFeed_AddEntry( killerName, killerColor, killerIsLocal, victimName, victimColor, victimIsLocal,
			weaponName, flags );
		return;
	}

	// Legacy fallback: same list and drawing as DeathMsg
	gHUD.m_DeathNotice.m_iFlags |= HUD_ACTIVE;

	int i; for ( i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;
	}
	if ( i == MAX_DEATHNOTICES )
	{ // move the rest of the list forward to make room for this item
		memmove( rgDeathNoticeList, rgDeathNoticeList+1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES );
		i = MAX_DEATHNOTICES - 1;
	}

	DeathNoticeItem &item = rgDeathNoticeList[i];
	memset( &item, 0, sizeof(item) );

	if ( ( flags & VGUI2_KF_FLAG_SUICIDE ) || !killerName || !killerName[0] )
	{
		item.iSuicide = TRUE;	// victim only
	}
	else
	{
		// SBO 2.0: this list draws plain text: names without their color codes (the chat's parser)
		strncpy( item.szKiller, VGui2_Chat_RemoveColorCodes( killerName ), sizeof(item.szKiller) - 1 );
		SboKillFeed_DecodeColor( killerColor, item.KillerRGB );
		item.KillerColor = item.KillerRGB;
	}

	strncpy( item.szVictim, VGui2_Chat_RemoveColorCodes( victimName ), sizeof(item.szVictim) - 1 );
	SboKillFeed_DecodeColor( victimColor, item.VictimRGB );
	item.VictimColor = item.VictimRGB;

	if ( flags & VGUI2_KF_FLAG_TEAMKILL )
		item.iTeamKill = TRUE;
	if ( flags & VGUI2_KF_FLAG_HEADSHOT )
		item.iHeadshot = TRUE;

	int spr = -1;
	if ( weaponName && weaponName[0] )
	{
		char name[64];
		_snprintf( name, sizeof(name), "d_%s", weaponName );
		name[sizeof(name) - 1] = 0;
		spr = gHUD.GetSpriteIndex( name );
	}
	item.iId = ( spr == 0 ) ? -1 : spr;	// 0 marks the end of the list; -1 draws the skull

	DEATHNOTICE_DISPLAY_TIME = CVAR_GET_FLOAT( "hud_deathnotice_time" );
	item.flDisplayTime = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;
}

int CHudDeathNotice :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( DeathMsg );

	CVAR_CREATE( "hud_deathnotice_time", "6", FCVAR_ARCHIVE );	// HL25: archived

	// chat-vgui2: VGUI2 kill feed (BHL CHudDeathNoticePanel); this element stays the DeathMsg hook and the fallback
	VGui2_KillFeed_Init();

	return 1;
}


void CHudDeathNotice :: InitHUDData( void )
{
	memset( rgDeathNoticeList, 0, sizeof(rgDeathNoticeList) );
	VGui2_KillFeed_Reset();
}


int CHudDeathNotice :: VidInit( void )
{
	m_HUD_d_skull = gHUD.GetSpriteIndex( "d_skull" );
	s_iHudHeadshot = gHUD.GetSpriteIndex( "d_headshot" );
	VGui2_KillFeed_Reset();	// sprite indices of the rows may have changed

	return 1;
}

int CHudDeathNotice :: Draw( float flTime )
{
	int x, y, r, g, b;

	// chat-vgui2: the VGUI2 kill feed draws the rows (hud_deathnotice_vgui 0 -> this legacy display)
	if ( VGui2_KillFeed_IsActive() )
		return 1;

	for ( int i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;  // we've gone through them all

		if ( rgDeathNoticeList[i].flDisplayTime < flTime )
		{ // display time has expired
			// remove the current item from the list
			memmove( &rgDeathNoticeList[i], &rgDeathNoticeList[i+1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i) );
			i--;  // continue on the next item;  stop the counter getting incremented
			continue;
		}

		rgDeathNoticeList[i].flDisplayTime = min( rgDeathNoticeList[i].flDisplayTime, gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME );

		// Only draw if the viewport will let me
		if ( gViewPort && gViewPort->AllowedToPrintText() )
		{
			// Draw the death notice
			y = HUD_YRES(DEATHNOTICE_TOP) + 2 + (20 * i);  //!!!	HUD scaling: HUD units

			int id = (rgDeathNoticeList[i].iId == -1) ? m_HUD_d_skull : rgDeathNoticeList[i].iId;

			// SBO 2.0: headshot icon after the weapon (CS 1.6); a headshot whose icon is the fallback skull (no
			// d_<weapon> sprite, e.g. an NPC's own attack, or the "skull" weapon of SboKillFeed_FromDeathMsg) draws
			// only d_headshot in the icon's place
			bool bHeadshot = rgDeathNoticeList[i].iHeadshot && s_iHudHeadshot >= 0;
			if ( bHeadshot && id == m_HUD_d_skull )
			{
				id = s_iHudHeadshot;
				bHeadshot = false;
			}

			// HUD scaling: the names keep the font's pixel size, centre them on the (scaled) weapon icon
			int texty = y;
			if ( gHUD.ShouldScale() )
			{
				int iTextWidth, iTextHeight;
				GetConsoleStringSize( "M", &iTextWidth, &iTextHeight );
				texty = y + ( ( gHUD.GetSpriteRect(id).bottom - gHUD.GetSpriteRect(id).top ) - iTextHeight ) / 2;
			}

			x = gHUD.GetWidth() - ConsoleStringLen(rgDeathNoticeList[i].szVictim) - (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left);

			if ( bHeadshot )
				x -= gHUD.GetSpriteRect(s_iHudHeadshot).right - gHUD.GetSpriteRect(s_iHudHeadshot).left;

			if ( !rgDeathNoticeList[i].iSuicide )
			{
				x -= (5 + ConsoleStringLen( rgDeathNoticeList[i].szKiller ) );

				// Draw killers name
				if ( rgDeathNoticeList[i].KillerColor )
					gEngfuncs.pfnDrawSetTextColor( rgDeathNoticeList[i].KillerColor[0], rgDeathNoticeList[i].KillerColor[1], rgDeathNoticeList[i].KillerColor[2] );
				x = 5 + DrawConsoleString( x, texty, rgDeathNoticeList[i].szKiller );
			}

			r = 255;  g = 80;	b = 0;
			if ( rgDeathNoticeList[i].iTeamKill )
			{
				r = 10;	g = 240; b = 10;  // display it in sickly green
			}

			// Draw death weapon
			SPR_Set( gHUD.GetSprite(id), r, g, b );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(id) );

			x += (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left);

			if ( bHeadshot )
			{
				SPR_Set( gHUD.GetSprite(s_iHudHeadshot), r, g, b );
				SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(s_iHudHeadshot) );
				x += gHUD.GetSpriteRect(s_iHudHeadshot).right - gHUD.GetSpriteRect(s_iHudHeadshot).left;
			}

			// Draw victims name (if it was a player that was killed)
			if (rgDeathNoticeList[i].iNonPlayerKill == FALSE)
			{
				if ( rgDeathNoticeList[i].VictimColor )
					gEngfuncs.pfnDrawSetTextColor( rgDeathNoticeList[i].VictimColor[0], rgDeathNoticeList[i].VictimColor[1], rgDeathNoticeList[i].VictimColor[2] );
				x = DrawConsoleString( x, texty, rgDeathNoticeList[i].szVictim );
			}
		}
	}

	return 1;
}

// This message handler may be better off elsewhere
int CHudDeathNotice :: MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ( pbuf, iSize );

	int killer = READ_BYTE();
	int victim = READ_BYTE();

	// Robustness: player indices index g_PlayerInfoList[MAX_PLAYERS+1]; 255 (-1) is the non-player victim marker
	// (-1: the message was too short)
	if ( killer < 0 || killer > MAX_PLAYERS )
		killer = 0;
	if ( victim < 0 || ( victim > MAX_PLAYERS && victim != 255 ) )
		victim = 0;

	char killedwith[32];
	strcpy( killedwith, "d_" );
	// fix: strncat's count is the room left, not the buffer size (a 31+ char weapon name overflowed killedwith)
	strncat( killedwith, READ_STRING(), sizeof( killedwith ) - strlen( killedwith ) - 1 );

	// SBO 2.0: headshot byte after HL's fields (CS 1.6 flag). Only read if the message has it (READ_BYTE fails at the
	// end of the message and READ_OK then says so): the teamplay "teammate" message and plugin-sent DeathMsgs end here.
	int iHeadshotByte = READ_BYTE();
	bool bHeadshot = READ_OK() && iHeadshotByte > 0;

	if (gViewPort)
		gViewPort->DeathMsg( killer, victim );

	gHUD.m_Spectator.DeathMessage(victim);

	// chat-vgui2: a preceding message (SboDeathMsg) already showed this death
	if ( s_iSboSkipDeathMsgVictim != -1 )
	{
		bool bSkip = ( s_iSboSkipDeathMsgVictim == victim );
		s_iSboSkipDeathMsgVictim = -1;
		if ( bSkip )
			return 1;
	}

	int i; for ( i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;
	}
	if ( i == MAX_DEATHNOTICES )
	{ // move the rest of the list forward to make room for this item
		memmove( rgDeathNoticeList, rgDeathNoticeList+1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES );
		i = MAX_DEATHNOTICES - 1;
	}

	if (gViewPort)
		gViewPort->GetAllPlayersInfo();

	// Get the Killer's name
	char *killer_name = g_PlayerInfoList[ killer ].name;
	if ( !killer_name )
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].KillerColor = GetClientColor( killer );
		strncpy( rgDeathNoticeList[i].szKiller, VGui2_Chat_RemoveColorCodes( killer_name ), MAX_PLAYER_NAME_LENGTH );	// SBO 2.0: no color codes
		rgDeathNoticeList[i].szKiller[MAX_PLAYER_NAME_LENGTH-1] = 0;
	}

	// Get the Victim's name
	char *victim_name = NULL;
	// If victim is -1, the killer killed a specific, non-player object (like a sentrygun)
	if ( ((char)victim) != -1 )
		victim_name = g_PlayerInfoList[ victim ].name;
	if ( !victim_name )
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].VictimColor = GetClientColor( victim );
		strncpy( rgDeathNoticeList[i].szVictim, VGui2_Chat_RemoveColorCodes( victim_name ), MAX_PLAYER_NAME_LENGTH );	// SBO 2.0: no color codes
		rgDeathNoticeList[i].szVictim[MAX_PLAYER_NAME_LENGTH-1] = 0;
	}

	// Is it a non-player object kill?
	if ( ((char)victim) == -1 )
	{
		rgDeathNoticeList[i].iNonPlayerKill = TRUE;

		// Store the object's name in the Victim slot (skip the d_ bit)
		strcpy( rgDeathNoticeList[i].szVictim, killedwith+2 );
	}
	else
	{
		if ( killer == victim || killer == 0 )
			rgDeathNoticeList[i].iSuicide = TRUE;

		if ( !strcmp( killedwith, "d_teammate" ) )
			rgDeathNoticeList[i].iTeamKill = TRUE;

		rgDeathNoticeList[i].iHeadshot = bHeadshot;
	}

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex( killedwith );

	rgDeathNoticeList[i].iId = spr;

	DEATHNOTICE_DISPLAY_TIME = CVAR_GET_FLOAT( "hud_deathnotice_time" );
	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;

	if ( VGui2_KillFeed_IsActive() )
		SboKillFeed_FromDeathMsg( killer, victim, killedwith + 2, bHeadshot );

	if (rgDeathNoticeList[i].iNonPlayerKill)
	{
		DeathNotice_ConsolePrintName( killer_name );
		ConsolePrint( " killed a " );
		ConsolePrint( rgDeathNoticeList[i].szVictim );
		ConsolePrint( "\n" );
	}
	else
	{
		// record the death notice in the console
		if ( rgDeathNoticeList[i].iSuicide )
		{
			DeathNotice_ConsolePrintName( victim_name );

			if ( !strcmp( killedwith, "d_world" ) )
			{
				ConsolePrint( " died" );
			}
			else
			{
				ConsolePrint( " killed self" );
			}
		}
		else if ( rgDeathNoticeList[i].iTeamKill )
		{
			DeathNotice_ConsolePrintName( killer_name );
			ConsolePrint( " killed his teammate " );
			DeathNotice_ConsolePrintName( victim_name );
		}
		else
		{
			DeathNotice_ConsolePrintName( killer_name );
			ConsolePrint( " killed " );
			DeathNotice_ConsolePrintName( victim_name );
		}

		if ( killedwith && *killedwith && (*killedwith > 13 ) && strcmp( killedwith, "d_world" ) && !rgDeathNoticeList[i].iTeamKill )
		{
			ConsolePrint( " with " );

			// replace the code names with the 'real' names
			if ( !strcmp( killedwith+2, "egon" ) )
				strcpy( killedwith, "d_gluon gun" );
			if ( !strcmp( killedwith+2, "gauss" ) )
				strcpy( killedwith, "d_tau cannon" );

			ConsolePrint( killedwith+2 ); // skip over the "d_" part
		}

		ConsolePrint( "\n" );
	}

	return 1;
}




