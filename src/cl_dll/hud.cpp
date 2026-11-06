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
// hud.cpp
//
// implementation of CHud class
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "viewport.h"

#include "demo.h"
#include "demo_api.h"
#include "environment.h"
#include "aurora/particlemgr.h"
#include "worldtext.h"	// SBO 2.0: point_worldtext
#include "mirror.h"	// SBO 2.0: env_mirror
#include "render_fade.h"	// SBO 2.0: env_render fades
#include "head_scale.h"		// SBO 2.0: players' head scales
#include "killfeed_icons.h"	// SBO 2.0: killfeed icons plugins set
#include "video/video_player.h"	// SBO 2.0: game_playvideo
#include "objecthint.h"	// SBO 2.0: object hints
#include "break_gibs.h"	// SBO 2.0: scaled break gibs
#include "dlights.h"	// SBO 2.0: env_dlight
#include "static_props.h"	// SBO 2.0: env_static
#include "map_music.h"	// SBO 2.0: ambient_music
#include "vgui2/vgui2_api.h"
#include "sequence_speak.h"	// game_shared: Sequence_IsSpeakSafe
#include "worldscreen/worldscreen.h"
#include "weapon_hand.h"	// SBO 2.0: weapon hand (cl_righthand)
#include "version_gate_shared.h"	// SBO 2.0: the version gate's userinfo key
#include "version/sbo_version.h"



class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 255;

		if( entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo)/sizeof(g_PlayerExtraInfo[0]) )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	virtual void UpdateCursorState()
	{
		gViewPort->UpdateCursorState();
	}

	virtual int	GetAckIconHeight()
	{
		// VGUI1 position in pixels, above the HUD numbers (HUD units, see hud_scale.cpp)
		return HUD_ToScreenY( gHUD.GetHeight() - gHUD.m_iFontHeight*3 - 6 );
	}

	virtual bool			CanShowSpeakerLabels()
	{
		if( gViewPort && VGui2_Score_IsActive() )
			return !VGui2_Score_IsVisible();
		// SBO 2.0: no VGUI1 scoreboard any more; without the VGUI2 one no scoreboard can be up
		return gViewPort != NULL;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern float IN_GetMouseSensitivity( void );	// HL25: clamped sensitivity
cvar_t *cl_lw = NULL;
cvar_t *cl_rollangle = NULL;
cvar_t *cl_rollspeed = NULL;

void ShutdownInput (void);

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Logo(pszName, iSize, pbuf );
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf );
}

int __MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_InitHUD( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_Capability(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Capability( pszName, iSize, pbuf );
}

int __MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ViewMode( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetFOV( pszName, iSize, pbuf );
}

int __MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Concuss( pszName, iSize, pbuf );
}

int __MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GameMode( pszName, iSize, pbuf );
}

// cs16-client hud.cpp: env_fog (hud_msg.cpp)
int __MsgFunc_Fog(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Fog( pszName, iSize, pbuf );
}

// SoHL hud.cpp: env_sky (hud_msg.cpp)
int __MsgFunc_SetSky(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetSky( pszName, iSize, pbuf );
}

// PROTOTYPE: env_fogzone list (hud_msg.cpp)
int __MsgFunc_FogZone(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_FogZone( pszName, iSize, pbuf );
}

// cs16-client hud.cpp: CS 1.6 weather (environment.cpp)
// SBO 2.0: spectator timer
int __MsgFunc_RoundTime(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_RoundTime( pszName, iSize, pbuf );
}

int __MsgFunc_ReceiveW(const char *pszName, int iSize, void *pbuf)
{
	return g_Environment.MsgFunc_ReceiveW( pszName, iSize, pbuf );
}

// SBO 2.0: CS:CZ Deleted Scenes' grass (environment.cpp, docs/MATERIALS.md)
int __MsgFunc_WeatherVol(const char *pszName, int iSize, void *pbuf)
{
	return g_Environment.MsgFunc_WeatherVol( pszName, iSize, pbuf );
}

int __MsgFunc_GrassInfo(const char *pszName, int iSize, void *pbuf)
{
	return g_Environment.MsgFunc_GrassInfo( pszName, iSize, pbuf );
}

// TFFree Command Menu
void __CmdFunc_OpenCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->ShowCommandMenu( gViewPort->m_StandardMenu );
	}
}

// TFC "special" command
void __CmdFunc_InputPlayerSpecial(void)
{
	if ( gViewPort )
	{
		gViewPort->InputPlayerSpecial();
	}
}

// SBO 2.0: "showbriefing" (CS's command name; key B in default.cfg): only the map briefing, the window the spectator
// Options menu's "Map Briefing" opens (MENU_MAPBRIEFING: OK and the close button only close it), alive, dead,
// spectating or waiting. Not while another window shows (MOTD, team or loadout menus): queued behind the MOTD it
// would be closed again by that window's OK. A server menu gives way, as to every other window.
void __CmdFunc_ShowBriefing(void)
{
	if ( !gViewPort )
		return;
	if ( gViewPort->m_pCurrentMenu && gViewPort->m_pCurrentMenu->GetMenuID() != MENU_SERVERMENU )
		return;
	gViewPort->ShowVGUIMenu( MENU_MAPBRIEFING );
}

void __CmdFunc_CloseCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

void __CmdFunc_ForceCloseCommandMenu( void )
{
	if ( gViewPort )
	{
		gViewPort->HideCommandMenu();
	}
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ValClass( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamNames( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Feign( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Detpack( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_MOTD( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_BuildSt( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_RandomPC( pszName, iSize, pbuf );
	return 0;
}
 
int __MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ServerName( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Spectator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_AllowSpec( pszName, iSize, pbuf );
	return 0;
}
 
// SBO 2.0: trigger_sequence (dlls/trigger_sequence.cpp, docs/SEQUENCES.md "Sounds"): a sentence of several words
// from a map's sequence file, to be played by the engine's sentence player. The engine plays a sentence that is
// not in sentences.txt only through its console command "spk" (Valve's Ricochet: CBasePlayer::ClientHearVox), so
// that is what is run, locally: pfnClientCmd puts a command in this client's own command buffer and is not
// subject to cl_filterstuffcmd (for a server's command strings the HL25 SDK has pfnFilteredClientCmd). The text
// comes from the server, so it is checked here whatever the server checked: only the characters of a sentence, 120
// at most (game_shared/sequence_speak.h); anything else is dropped without a word.
int __MsgFunc_SeqSpeak(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	const char *sentence = READ_STRING();

	if ( Sequence_IsSpeakSafe( sentence ) )
	{
		char command[SEQUENCE_SPEAK_MAX + 16];

		snprintf( command, sizeof( command ), "spk \"%s\"\n", sentence );
		command[ sizeof( command ) - 1 ] = '\0';

		gEngfuncs.pfnClientCmd( command );
	}

	return 1;
}

// This is called every time the DLL is loaded
void CHud :: Init( void )
{
	// SBO 2.0 version gate (docs/VERSION_GATE.md): the game version ("2.0.0") in the userinfo, so a server refuses an
	// older client at connect. Registering a cvar does not put it in the userinfo (only setting it does: the engine's
	// Cvar_DirectSet), so it is set right away. First: the userinfo holds 256 bytes, the keys set later lose when it is full.
	CVAR_CREATE( VERSION_GATE_KEY, SboVersion_Number(), FCVAR_USERINFO );
	gEngfuncs.Cvar_Set( (char *)VERSION_GATE_KEY, (char *)SboVersion_Number() );
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( SeqSpeak );	// SBO 2.0: trigger_sequence
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( Capability );	// SBO 2.0 (Featureful player_capabilities)
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );

	// TFFree CommandMenu
	HOOK_COMMAND( "+commandmenu", OpenCommandMenu );
	HOOK_COMMAND( "-commandmenu", CloseCommandMenu );
	HOOK_COMMAND( "ForceCloseCommandMenu", ForceCloseCommandMenu );
	HOOK_COMMAND( "special", InputPlayerSpecial );
	HOOK_COMMAND( "showbriefing", ShowBriefing );	// SBO 2.0: the map briefing window
	gEngfuncs.pfnAddCommand( HUD_SPRITELIST_COMMAND, HudSpriteList_f );	// SBO 2.0: HUD sprite slots in use (util.cpp)

	HOOK_MESSAGE( ValClass );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );

	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );

	// cs16-client: CS 1.6 weather (environment.cpp) and env_fog (hud_msg.cpp)
	gEngfuncs.pfnHookUserMsg( "ReceiveW", __MsgFunc_ReceiveW );
	gEngfuncs.pfnHookUserMsg( "GrassInfo", __MsgFunc_GrassInfo );
	gEngfuncs.pfnHookUserMsg( "WeatherVol", __MsgFunc_WeatherVol );
	HOOK_MESSAGE( Fog );
	HOOK_MESSAGE( SetSky );		// SoHL: env_sky
	HOOK_MESSAGE( FogZone );	// PROTOTYPE: env_fogzone list
	HOOK_MESSAGE( RoundTime );	// SBO 2.0: spectator timer
	PlayerAfk_Init();	// SBO 2.0: AFK sign ("PlayerAfk", entity.cpp)
	Materials_Init();	// SBO 2.0: material system ("MatFile", materials.cpp)
	Mirror_Init();		// SBO 2.0: env_mirror ("Mirror", mirror.cpp)
	RenderFade_Init();	// SBO 2.0: env_render fades ("RenderFade", render_fade.cpp)
	HeadScale_Init();	// SBO 2.0: players' head scales ("HeadScale", head_scale.cpp)
	KillFeedIcons_Init();	// SBO 2.0: killfeed icons plugins set ("KfIcon", killfeed_icons.cpp)
	ObjectHint_Init();	// SBO 2.0: object hints ("ObjectHint", cl_objecthint; objecthint.cpp)
	BreakGibs_Init();	// SBO 2.0: "BreakModel", func_breakable / func_pushable "gibscale" (break_gibs.cpp)
	DLights_Init();		// SBO 2.0: env_dlight ("KeyedDLight", dlights.cpp)
	StaticProps_Init();	// SBO 2.0: env_static ("StaticProp", static_props.cpp)
	MapMusic_Init();	// SBO 2.0: ambient_music ("MapMusic", map_music.cpp)
	Video_Init();		// SBO 2.0: game_playvideo ("PlayVideo", video/video_player.cpp)

	// SBO 2.0: no hud_classautokill (TFC's class switch suicide; unused in SBO, and as a userinfo key it took 20 of
	// the 256 userinfo bytes: a long name pushed hud_chat_color out of the server's copy, playtest 2026-10-17)
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );		// controls whether or not to automatically take screenshots at the end of a round


	m_iLogo = 0;
	m_iFOV = 0;

	// HL25: archived; cl_autowepswitch is sent in userinfo (the server decides whether picking up a weapon switches to it)
	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE );
	CVAR_CREATE( "cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	WeaponHand_Init();	// SBO 2.0: cl_righthand (weapon_hand.cpp)
	// SBO: the player's name color, separate from the name (userinfo, like topcolor / bottomcolor: the engine sends it
	// to every client, which draw the name in it: chat, scoreboard, kill feed, voice labels, status bar). Values as hud_chat_color:
	// 0 none, 1..8 the ^1..^8 colors, xRRGGBB (e.g. x40A0FF; xRGB = x4AF too) any color. Set by the chat color picker ("Name").
	// (hud_chat_color, the color of the player's chat text, is a userinfo cvar too: vgui2/hud_chat.cpp.)
	CVAR_CREATE( "namecolor", "0", FCVAR_ARCHIVE | FCVAR_USERINFO );
	default_fov = CVAR_CREATE( "default_fov", "90", FCVAR_ARCHIVE );
	// halflife-updated (halflife #1544): client-side view roll. SBO default 0 (no roll); halflife-updated uses 2.0
	cl_rollangle = CVAR_CREATE( "cl_rollangle", "0", FCVAR_ARCHIVE );
	cl_rollspeed = CVAR_CREATE( "cl_rollspeed", "200", FCVAR_ARCHIVE );
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	// halflife-updated PR #245: stretch the 640x480 HUD to the screen (hardware renderer); applied at the next VidInit.
	// SBO default 0 (the classic unscaled HUD); upstream uses 1
	m_pCvarScale = CVAR_CREATE( "hud_scale", "0", FCVAR_ARCHIVE );
	// cs16-client: CS 1.6 weather intensity, 0 (off) to 3 (environment.cpp)
	cl_weather = CVAR_CREATE( "cl_weather", "1", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );

	m_pSpriteList = NULL;

	// Clear any old HUD list
	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	SboHud_InitChat();	// VGUI2 chat, or m_SayText as fallback
	WorldScreen_Init();	// SBO 2.0: world screens (vgui_screen)
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_Sniper.Init();
	m_Flamme.Init();
	m_Fumee.Init();
	Aurora_Init();		// SBO 2.0: env_particle (aurora/particlemgr.cpp)
	WorldText_Init();	// SBO 2.0: point_worldtext ("WorldText", worldtext.cpp)
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (void**)&gViewPort);
	VGui2_VoiceStatus_Init();	// the speaker list (hud_voice_status.cpp)

	m_Menu.Init();

	MsgFunc_ResetHUD(0, 0, NULL );
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ) == 0 )
			return i;
	}

	return -1; // invalid sprite
}

// SBO 2.0: the extra named sprites after hud.txt's (hud.h HUD_EXTRA_SPRITES)
void CHud :: ClearExtraSprites( void )
{
	if ( m_pSpriteList )
		m_iSpriteCount = m_iSpriteCountHud;
}

int CHud :: AddExtraSprite( const char *name, HSPRITE hSprite, const wrect_t &rc )
{
	if ( !m_pSpriteList || !name || !name[0] || m_iSpriteCount >= m_iSpriteCountHud + HUD_EXTRA_SPRITES )
		return -1;

	const int index = m_iSpriteCount++;
	m_rghSprites[index] = hSprite;
	m_rgrcRects[index] = rc;
	strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], name, MAX_SPRITE_NAME_LENGTH - 1 );
	m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH + MAX_SPRITE_NAME_LENGTH - 1] = 0;
	return index;
}

void CHud :: VidInit( void )
{
	m_iSuppressedCaps = 0;	// SBO 2.0 (Featureful player_capabilities): a new map starts with none
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// halflife-updated PR #245: HUD units for this screen size and hud_scale
	UpdateScalingInfo();

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");
	
	m_hsprLogo = 0;	
	m_hsprCursor = 0;

	// SBO 2.0: a new map has no 3D sky until the server's SetSky (it comes after the InitHUD, which clears it too)
	m_iSkyMode = SKY_OFF;

	// HUD scaling: still chosen from the real screen (only scales above 640x480, so always the 640 set then)
	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j; for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
			// SBO 2.0: and HUD_EXTRA_SPRITES more after them (AddExtraSprite)
			m_iSpriteCountHud = m_iSpriteCount;
 			m_rghSprites = new HSPRITE[m_iSpriteCount + HUD_EXTRA_SPRITES];
			m_rgrcRects = new wrect_t[m_iSpriteCount + HUD_EXTRA_SPRITES];
			m_rgszSpriteNames = new char[( m_iSpriteCount + HUD_EXTRA_SPRITES ) * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	PlayerAfk_VidInit();	// SBO 2.0: AFK sign
	Materials_VidInit();	// SBO 2.0: material system, the global files until the map's file is named
	V_ResetViewModelRender();	// SBO 2.0: "ViewModel" message
	Mirror_Reset();		// SBO 2.0: env_mirror: a new map has none until the server sends them
	RenderFade_Reset();	// SBO 2.0: env_render fades
	HeadScale_Reset();	// SBO 2.0: players' head scales: normal until the server sends them
	KillFeedIcons_Reset();	// SBO 2.0: plugins' killfeed icons: none until the server sends them
	ObjectHint_Reset();	// SBO 2.0: object hints: the engine freed their temporary entities
	DLights_Reset();	// SBO 2.0: env_dlight: a new map has none until the server sends them
	StaticProps_Reset();	// SBO 2.0: env_static: a new map has none until the server sends them
	MapMusic_Reset();	// SBO 2.0: ambient_music: the engine stops the MP3 at a level load; the next state runs
	Video_Reset();		// SBO 2.0: game_playvideo: a new map or server stops a video

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_Sniper.VidInit();
	m_Flamme.VidInit();
	m_Fumee.VidInit();
	Aurora_VidInit();	// SBO 2.0: env_particle
	WorldText_VidInit();	// SBO 2.0: point_worldtext: none until the server sends them; fonts loaded again
	FogZone_VidInit();	// SBO 2.0: env_fogzone haze sprite (tri.cpp)
	GetClientVoiceMgr()->VidInit();
}

int CHud::MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else 
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else 
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV( void )
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		int i = 0;
		unsigned char buf[ 100 ];

		// Active
		*( float * )&buf[ i ] = g_lastFOV;
		i += sizeof( float );

		Demo_WriteBuffer( TYPE_ZOOM, i, buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

bool CHud::IsZoomed( void )
{
	if ( m_iFOV >= 90 )
		return false;

	return !default_fov || m_iFOV < default_fov->value;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT( "default_fov" );

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	// halflife-updated (halflife #3044): "But it doesn't restore correctly so this still needs to be used". With cl_lw 1
	// a SetFOV from the server (or a plugin) that prediction doesn't know about was ignored.
	/*
	if ( cl_lw && cl_lw->value )
		return 1;
	*/

	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == def_fov )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = IN_GetMouseSensitivity() * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}


void CHud::AddHudElem(CHudBase *phudelem)
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST *)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity( void )
{
	return m_flMouseSensitivity;
}


