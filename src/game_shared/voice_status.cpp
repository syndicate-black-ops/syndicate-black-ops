//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// There are hud.h's coming out of the woodwork so this ensures that we get the right one.
#if defined( DMC_BUILD )
	#include "../dmc/cl_dll/hud.h"
	#include "../dmc/cl_dll/cl_util.h"
#elif defined( RICOCHET_BUILD )
	#include "../ricochet/cl_dll/hud.h"
	#include "../ricochet/cl_dll/cl_util.h"
#else
	#include "../cl_dll/hud.h"
	#include "../cl_dll/cl_util.h"
#endif

#include <assert.h>
#include <string.h>
#include <stdio.h>

#if defined( DMC_BUILD )
	#include "../dmc/cl_dll/parsemsg.h"
	#include "../dmc/cl_dll/demo.h"
#elif defined( RICOCHET_BUILD )
	#include "../ricochet/cl_dll/parsemsg.h"
	#include "../ricochet/cl_dll/demo.h"
#else
	#include "../cl_dll/parsemsg.h"
	#include "../cl_dll/demo.h"
#endif

#include "demo_api.h"
#include "voice_status.h"
#include "r_efx.h"
#include "entity_types.h"
#include "../cl_dll/vgui2/vgui2_api.h"	// VGui2_GetPlayerSteamID64


extern int cam_thirdperson;


#define VOICE_MODEL_INTERVAL		0.3
#define SCOREBOARD_BLINK_FREQUENCY	0.3	// How often to blink the scoreboard icons.
#define SQUELCHOSCILLATE_PER_SECOND	2.0f





// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
CVoiceStatus g_VoiceStatus;

CVoiceStatus* GetClientVoiceMgr()
{
	return &g_VoiceStatus;
}



// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

static CVoiceStatus *g_pInternalVoiceStatus = NULL;

int __MsgFunc_VoiceMask(const char *pszName, int iSize, void *pbuf)
{
	if(g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleVoiceMaskMsg(iSize, pbuf);

	return 1;
}

int __MsgFunc_ReqState(const char *pszName, int iSize, void *pbuf)
{
	if(g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleReqStateMsg(iSize, pbuf);

	return 1;
}


int g_BannedPlayerPrintCount;
void ForEachBannedPlayer(char id[16])
{
	char str[256];
	sprintf(str, "Ban %d: %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		g_BannedPlayerPrintCount++,
		id[0], id[1], id[2], id[3], 
		id[4], id[5], id[6], id[7], 
		id[8], id[9], id[10], id[11], 
		id[12], id[13], id[14], id[15]
		);
#ifdef _WIN32
	strupr(str);
#endif
	gEngfuncs.pfnConsolePrint(str);
}


void ShowBannedCallback()
{
	if(g_pInternalVoiceStatus)
	{
		g_BannedPlayerPrintCount = 0;
		gEngfuncs.pfnConsolePrint("------- BANNED PLAYERS -------\n");
		g_pInternalVoiceStatus->m_BanMgr.ForEachBannedPlayer(ForEachBannedPlayer);
		gEngfuncs.pfnConsolePrint("------------------------------\n");
	}
}


// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

CVoiceStatus::CVoiceStatus()
{
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;

	m_bTalking = m_bServerAcked = false;

	m_bServerModEnable = -1;

	m_pchGameDir = NULL;
}


CVoiceStatus::~CVoiceStatus()
{
	g_pInternalVoiceStatus = NULL;
	
	if(m_pchGameDir)
	{
		if(m_bBanMgrInitialized)
		{
			m_BanMgr.SaveState(m_pchGameDir);
		}

		free(m_pchGameDir);
	}
}


int CVoiceStatus::Init(
	IVoiceStatusHelper *pHelper,
	void **pParentPanel)
{
	// Setup the voice_modenable cvar.
	gEngfuncs.pfnRegisterVariable("voice_modenable", "1", FCVAR_ARCHIVE);

	gEngfuncs.pfnRegisterVariable("voice_clientdebug", "0", 0);

	gEngfuncs.pfnAddCommand("voice_showbanned", ShowBannedCallback);

	if(gEngfuncs.pfnGetGameDirectory())
	{
		m_BanMgr.Init(gEngfuncs.pfnGetGameDirectory());
		m_bBanMgrInitialized = true;
	}

	assert(!g_pInternalVoiceStatus);
	g_pInternalVoiceStatus = this;

	m_VoiceHeadModel = NULL;
	m_bInSquelchMode = false;

	m_pHelper = pHelper;
	m_pParentPanel = pParentPanel;
	gHUD.AddHudElem(this);
	m_iFlags = HUD_ACTIVE;
	HOOK_MESSAGE(VoiceMask);
	HOOK_MESSAGE(ReqState);

	// Cache the game directory for use when we shut down
	const char *pchGameDirT = gEngfuncs.pfnGetGameDirectory();
	m_pchGameDir = (char *)malloc(strlen(pchGameDirT) + 1);
	strcpy(m_pchGameDir, pchGameDirT);

	return 1;
}


int CVoiceStatus::VidInit()
{
	// Figure out the voice head model height.
	m_VoiceHeadModelHeight = 45;
	char *pFile = (char *)gEngfuncs.COM_LoadFile("scripts/voicemodel.txt", 5, NULL);
	if(pFile)
	{
		char token[4096];
		gEngfuncs.COM_ParseFile(pFile, token);
		if(token[0] >= '0' && token[0] <= '9')
		{
			m_VoiceHeadModelHeight = (float)atof(token);
		}

		gEngfuncs.COM_FreeFile(pFile);
	}

	m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/voiceicon.spr");
	return TRUE;
}


void CVoiceStatus::Frame(double frametime)
{
	// check server banned players once per second
	if(gEngfuncs.GetClientTime() - m_LastUpdateServerState > 1)
	{
		UpdateServerState(false);
	}

}


void CVoiceStatus::CreateEntities()
{
	if(!m_VoiceHeadModel) return;

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	int iOutModel = 0;
	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if(!m_VoicePlayers[i]) continue;
		
		cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(i+1);
		
		// Don't show an icon if the player is not in our PVS.
		if(!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum) continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if(pClient->curstate.effects & EF_NODRAW) continue;

		// Don't show an icon for the local player unless we're in thirdperson mode.
		if(pClient == localPlayer && !cam_thirdperson) continue;

		cl_entity_s *pEnt = &m_VoiceHeadModels[iOutModel];
		++iOutModel;

		memset(pEnt, 0, sizeof(*pEnt));

		pEnt->curstate.rendermode = kRenderTransAdd;
		pEnt->curstate.renderamt = 255;
		pEnt->baseline.renderamt = 255;
		pEnt->curstate.renderfx = kRenderFxNoDissipation;
		pEnt->curstate.framerate = 1;
		pEnt->curstate.frame = 0;
		pEnt->model = (struct model_s*)gEngfuncs.GetSpritePointer(m_VoiceHeadModel);
		pEnt->angles[0] = pEnt->angles[1] = pEnt->angles[2] = 0;
		pEnt->curstate.scale = 0.5f;
		
		pEnt->origin[0] = pEnt->origin[1] = 0;
		pEnt->origin[2] = 45;

		VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

		// Tell the engine.
		gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
	}
}


void CVoiceStatus::UpdateSpeakerStatus(int entindex, qboolean bTalking)
{
	if(!*m_pParentPanel)
		return;

	if( gEngfuncs.pfnGetCvarFloat("voice_clientdebug") )
	{
		char msg[256];
		_snprintf( msg, sizeof(msg), "CVoiceStatus::UpdateSpeakerStatus: ent %d talking = %d\n", entindex, bTalking );
		gEngfuncs.pfnConsolePrint( msg );
	}

	// Is it the local player talking?
	if( entindex == -1 )
	{
		m_bTalking = !!bTalking;
		if( bTalking )
		{
			// Enable voice for them automatically if they try to talk.
			gEngfuncs.pfnClientCmd( "voice_modenable 1" );
		}
	}
	else if( entindex == -2 )
	{
		m_bServerAcked = !!bTalking;
	}
	else if(entindex >= 0 && entindex <= VOICE_MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		if(bTalking)
		{
			m_VoicePlayers[iClient] = true;
			m_VoiceEnabledPlayers[iClient] = true;
		}
		else
		{
			m_VoicePlayers[iClient] = false;
		}
	}
}


void CVoiceStatus::UpdateServerState(bool bForce)
{
	// Can't do anything when we're not in a level.
	char const *pLevelName = gEngfuncs.pfnGetLevelName();
	if( pLevelName[0] == 0 )
	{
		if( gEngfuncs.pfnGetCvarFloat("voice_clientdebug") )
		{
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: pLevelName[0]==0\n" );
		}

		return;
	}
	
	int bCVarModEnable = !!gEngfuncs.pfnGetCvarFloat("voice_modenable");
	if(bForce || m_bServerModEnable != bCVarModEnable)
	{
		m_bServerModEnable = bCVarModEnable;

		char str[256];
		_snprintf(str, sizeof(str), "VModEnable %d", m_bServerModEnable);
		ServerCmd(str);

		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			sprintf(msg, "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}
	}

	char str[2048];
	sprintf(str, "vban");
	bool bChange = false;

	for(unsigned long dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{	
		unsigned long serverBanMask = 0;
		unsigned long banMask = 0;
		for(unsigned long i=0; i < 32; i++)
		{
			char playerID[16];
			if(!GetPlayerBanID(dw*32 + i+1, playerID))
				continue;

			if(m_BanMgr.GetPlayerBan(playerID))
				banMask |= 1 << i;

			if(m_ServerBannedPlayers[dw*32 + i])
				serverBanMask |= 1 << i;
		}

		if(serverBanMask != banMask)
			bChange = true;

		// Ok, the server needs to be updated.
		char numStr[512];
		sprintf(numStr, " %x", banMask);
		strcat(str, numStr);
	}

	if(bChange || bForce)
	{
		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			sprintf(msg, "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}

		gEngfuncs.pfnServerCmdUnreliable(str);	// Tell the server..
	}
	else
	{
		if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: no change\n" );
		}
	}
	
	m_LastUpdateServerState = gEngfuncs.GetClientTime();
}

void CVoiceStatus::HandleVoiceMaskMsg(int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	unsigned long dw;
	for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		m_AudiblePlayers.SetDWord(dw, (unsigned long)READ_LONG());
		m_ServerBannedPlayers.SetDWord(dw, (unsigned long)READ_LONG());

		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char str[256];
			gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleVoiceMaskMsg\n");
			
			sprintf(str, "    - m_AudiblePlayers[%d] = %lu\n", dw, m_AudiblePlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);
			
			sprintf(str, "    - m_ServerBannedPlayers[%d] = %lu\n", dw, m_ServerBannedPlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);
		}
	}

	m_bServerModEnable = READ_BYTE();
}

void CVoiceStatus::HandleReqStateMsg(int iSize, void *pbuf)
{
	if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleReqStateMsg\n");
	}

	UpdateServerState(true);	
}

void CVoiceStatus::StartSquelchMode()
{
	if(m_bInSquelchMode)
		return;

	m_bInSquelchMode = true;
	m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
	m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

//-----------------------------------------------------------------------------
// SBO: the player's key in the ban list (m_BanMgr), false if he can't be muted.
//
// The engine's "unique ID" (cl_enginefunc_t::GetPlayerUniqueID, player_info_t::hashedcdkey) is not unique: the
// client sends the MD5 of its own LAN address ("192.168.1.2") as its CD key and the server hands the hash of that to
// everyone, so players with the same private address share it; servers that hash the address they see instead
// (ReHLDS #1152) give one ID to everybody behind the same router -- the local player included. Muting one of them
// muted them all. So:
//   * the key is the player's SteamID64 whenever the engine has one (one per account);
//   * without a SteamID (bots, sv_lan) it is the engine's ID, unless that is the local player's own ID as well
//     (he can't be told apart from us: not mutable);
//   * the local player has no key: he is never muted, whatever the ban list holds.
//-----------------------------------------------------------------------------
bool CVoiceStatus::GetPlayerBanID(int iPlayer, char playerID[16])
{
	if (iPlayer < 1 || iPlayer > VOICE_MAX_PLAYERS)
		return false;

	cl_entity_t *pLocal = gEngfuncs.GetLocalPlayer();
	int iLocal = pLocal ? pLocal->index : 0;
	if (iPlayer == iLocal)
		return false;

	unsigned long long steamID64 = VGui2_GetPlayerSteamID64(iPlayer);
	if (steamID64 != 0)
	{
		// can't be an engine ID (an MD5 digest) by accident: 8 fixed bytes
		memcpy(playerID, "STEAMID", 8);
		memcpy(playerID + 8, &steamID64, 8);
		return true;
	}

	if (!gEngfuncs.GetPlayerUniqueID(iPlayer, playerID))
		return false;

	char localID[16];
	if (iLocal >= 1 && gEngfuncs.GetPlayerUniqueID(iLocal, localID) && memcmp(playerID, localID, 16) == 0)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// SBO: the player can be muted / unmuted (see GetPlayerBanID)
//-----------------------------------------------------------------------------
bool CVoiceStatus::CanPlayerBeBlocked(int iPlayer)
{
	char playerID[16];
	return GetPlayerBanID(iPlayer, playerID);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked(int iPlayer)
{
	char playerID[16];
	if (!GetPlayerBanID(iPlayer, playerID))
		return false;

	return m_BanMgr.GetPlayerBan(playerID);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the player can't hear the other client due to game rules (eg. the other team)
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerAudible(int iPlayer)
{
	return !!m_AudiblePlayers[iPlayer-1];
}

//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState(int iPlayer, bool blocked)
{
	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 1\n" );
	}

	char playerID[16];
	if (!GetPlayerBanID(iPlayer, playerID))
		return;

	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 2\n" );
	}

	// Squelch or (try to) unsquelch this player.
	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		char str[256];
		sprintf(str, "CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", iPlayer, !m_BanMgr.GetPlayerBan(playerID));
		gEngfuncs.pfnConsolePrint(str);
	}

	m_BanMgr.SetPlayerBan( playerID, blocked );
	UpdateServerState(false);
}
