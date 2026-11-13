//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: SBO side of the VGUI2 bridge (see sbo_bridge.h).
//
// This file is compiled with SBO's normal client include paths (SDK 2.3, VGUI1),
// NOT with hl1_source_sdk's. It must not include any Source SDK header.
//=================================================================//

#include <vector>	// before cl_util.h (its min and max macros): ammohistory.h uses it
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "cvardef.h"
#include "custom.h"
#include "net_api.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "ammohistory.h"
#include "viewport.h"
extern vec3_t v_origin, v_angles; // view.cpp: the last view (sbo::GetLocalView)

#include <stddef.h>
#include <string.h>

#include "sbo_bridge.h"
#include "vgui2_api.h"
#include "sbo_server_menu.h"

extern float *GetClientColor(int clientIndex); // death.cpp
extern float *GetTeamColor(int teamNumber);    // death.cpp
extern int g_iVisibleMouse; // inputw32.cpp: SBO's viewport wants the cursor
void IN_ReleaseRelativeMouse( void ); // inputw32.cpp

//-----------------------------------------------------------------------------
// Engine function table: engine/cdll_int.h's cl_enginefunc_t has the full
// Steam-era/HL25 layout (halflife engine/APIProxy.h), so GetFirstCmdFunctionHandle
// is a regular member now. The engine returns the head of its cmd_function_t list
// as an opaque handle (unsigned int in Valve's header; a pointer on 32-bit).
//-----------------------------------------------------------------------------
namespace
{
typedef void (*CmdFunction)(void);

// engine cmd_function_t (BHL src/engine/cmd_function_t.h)
struct cmd_function_t
{
	cmd_function_t *next;
	const char *name;
	CmdFunction function;
	int flags;
};

// The engine's own table (persistent for the life of the client DLL).
const cl_enginefunc_t *s_pEngineFuncsFull = nullptr;
} // namespace

void VGui2_SetEngineFuncs(const void *pEnginefuncs)
{
	s_pEngineFuncsFull = static_cast<const cl_enginefunc_t *>(pEnginefuncs);
}

//-----------------------------------------------------------------------------
// HUD integration
//-----------------------------------------------------------------------------
namespace
{
// the server's sv_cheats value ("Cheats", MsgFunc_Cheats below), reset with the HUD (InitHUDData)
int s_iServerCheats = 0;

// "SpecHealth2" (byte player, short health, short armor, byte weapon id, byte flags, byte BR rounds won, short view model): the state of the player this
// client watches as a spectator, for the spectator panel's target info (sbo_spectator_info.cpp). Sent by this branch's
// server to an observer when its target or one of the values changed (Observer_SendTargetInfo, observer.cpp), so the
// last one received is the current state. Old servers and HLTV proxies never send it: no target info then.
const int SPECINFO_LEGS_BROKEN = 1; // server player.h

struct SpecTarget_t
{
	int player; // 0: nothing received
	int health;
	int armor;
	int weapon;
	int flags;
	int brWins;
	int viewModel; // model index of the player's view model, 0: none; -1: not in the message (a server before it)
};
SpecTarget_t s_SpecTarget = {0, 0, 0, 0, 0, 0, -1};

void ResetSpecTarget()
{
	s_SpecTarget = SpecTarget_t();
	s_SpecTarget.viewModel = -1;
}

int MsgFunc_SpecHealth2(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	s_SpecTarget.player = READ_BYTE();
	s_SpecTarget.health = READ_SHORT();
	s_SpecTarget.armor = READ_SHORT();
	s_SpecTarget.weapon = READ_BYTE();
	s_SpecTarget.flags = READ_BYTE();
	s_SpecTarget.brWins = READ_BYTE();
	s_SpecTarget.viewModel = (iSize >= 10) ? READ_SHORT() : -1;
	return 1;
}

// Keeps the VGUI2 chat in SBO's HUD element list so it gets InitHUDData()
// (new map / InitHUD message) like every other element. Drawing is done by VGUI2.
class CHudVGui2ChatProxy : public CHudBase
{
public:
	int Init(void)
	{
		gHUD.AddHudElem(this);
		return 1;
	}

	int VidInit(void) { return 1; }
	int Draw(float flTime) { return 1; }
	void InitHUDData(void)
	{
		VGui2_Chat_InitHUDData();
		VGui2_Tutor_Reset(); // CS:CZ tutor window: a new map / connection (the server forgets its queue too)
		VGui2_GameTimer_Reset(); // game_timer: the new map's timers follow with the HUD initialisation
		VGui2_Objectives_Reset(); // trigger_objective: the server sends the list again
		VGui2_MessageBox_Reset(); // game_messagebox: the server sends the open boxes again after InitHUD
		VGui2_GmSpawn_Reset();   // GM mode: the server sends the Game Master's state again
		ResetSpecTarget();   // spectator target info: the player slots mean other players now
		ServerMenu_InitHUD(); // server menus: the server defines them again after its hello
		s_iServerCheats = 0; // the server sends its sv_cheats value again (Cheats)
	}
};

CHudVGui2ChatProxy s_VGui2ChatProxy;
bool s_bUseVGui2Chat = false;

// "SboXP" (byte client, long XP): coop XP for the scoreboard, sent by this branch's server (SboScoreboard_*).
// Old servers never send it: the XP column stays hidden (s_bSboXPReceived).
int s_iSboXP[MAX_PLAYERS + 1];
bool s_bSboXPReceived = false;

int MsgFunc_SboXP(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int cl = READ_BYTE();
	int xp = READ_LONG();

	if (cl > 0 && cl <= MAX_PLAYERS)
	{
		s_iSboXP[cl] = xp;
		s_bSboXPReceived = true;
		VGui2_Score_UpdateOnPlayerInfo(cl);
	}
	return 1;
}

// "SboModeInfo" (byte type, short value1, short value2): the scoreboard's round / frags left line, sent by this branch's
// server on join and on change (SboModeInfo_*); it stops sending once the map is over, so the last values stay.
int s_iModeInfo[3] = {sbo::MODEINFO_NONE, 0, 0};

int MsgFunc_SboModeInfo(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	s_iModeInfo[0] = READ_BYTE();
	s_iModeInfo[1] = READ_SHORT();
	s_iModeInfo[2] = READ_SHORT();
	VGui2_Score_UpdateModeInfo();
	return 1;
}

// "RoundStatus" (byte state, short seconds): the co-op / BR round for the new loadout menu's header (server
// RoundStatus_Update, coop.cpp), sent when it changes. The seconds are kept as the time they run out.
int s_iRoundStatus = 0;
float s_flRoundStatusEnd = -1.0f;

int MsgFunc_RoundStatus(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	s_iRoundStatus = READ_BYTE();
	const int seconds = READ_SHORT();
	s_flRoundStatusEnd = gEngfuncs.GetClientTime() + seconds;
	return 1;
}

// "HeavyLimit" (byte grenade launchers taken, byte max, byte nuclear launchers taken, byte max): the co-op team limits
// for the new loadout menu (server HeavyLimit_Update, coop.cpp), the counts without this player, sent when they change
bool s_bHeavyLimit = false;
int s_iHeavyLimit[4] = {};

int MsgFunc_HeavyLimit(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	for (int &v : s_iHeavyLimit)
		v = READ_BYTE();
	s_bHeavyLimit = true;
	return 1;
}

// "Cheats" (byte): the server's sv_cheats value (server CBasePlayer::UpdateClientData), sent when it changes and
// again after each connection / map load. The engine's own sv_cheats on the client only says 0 or 1 (svc_sendextrainfo
// sends sv_cheats != 0). 0 until the server sends it (s_iServerCheats): older servers never do.
int MsgFunc_Cheats(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const int cheats = READ_BYTE();
	s_iServerCheats = READ_OK() && cheats > 0 ? cheats : 0;
	return 1;
}

// "SboDeathMsg": kill feed rows with NPC names from this branch's server (SboKillfeed_*, multiplay_gamerules.cpp; native
// port of the metamod-syndicate-killfeed plugin). byte flags, byte killer + string killer NPC name, byte victim +
// string victim NPC name, string weapon code (DeathMsg style, for the icon), string weapon for the console line,
// byte headshot (SBO 2.0, last; read only if present).
// Player index 0 = the NPC named next to it (or no killer). Old servers never send it.
// SBO_KF_REPLACES_DEATHMSG: an NPC killed a player; the server sends the stock DeathMsg for that victim right after
// (killer 0, weapon "world", for the scoreboard/spectator code and old clients), whose row/console line is skipped.
const int SBO_KF_REPLACES_DEATHMSG = 1; // server player.h

// A name in a console line: its colour codes drawn as colours (hud_colortext 1, docs/CONSOLE_COLOR.md), else removed
// (kept as typed with hud_colortext 0)
static void ConsolePrintName(const char *name)
{
	if (!VGui2_Console_PrintColorCodes(name))
		sbo::ConsolePrint(VGui2_Chat_RemoveColorCodes(name));
}

int MsgFunc_SboDeathMsg(const char *pszName, int iSize, void *pbuf)
{
	char killerNpc[64], victimNpc[64], weapon[64], weaponName[64];

	BEGIN_READ(pbuf, iSize);
	int flags = READ_BYTE();
	int killer = READ_BYTE();
	strncpy(killerNpc, READ_STRING(), sizeof(killerNpc) - 1);
	killerNpc[sizeof(killerNpc) - 1] = 0;
	int victim = READ_BYTE();
	strncpy(victimNpc, READ_STRING(), sizeof(victimNpc) - 1);
	victimNpc[sizeof(victimNpc) - 1] = 0;
	strncpy(weapon, READ_STRING(), sizeof(weapon) - 1);
	weapon[sizeof(weapon) - 1] = 0;
	strncpy(weaponName, READ_STRING(), sizeof(weaponName) - 1);
	weaponName[sizeof(weaponName) - 1] = 0;
	const int headshotByte = READ_BYTE(); // -1 and READ_OK() false if the message ends before it
	const bool headshot = READ_OK() && headshotByte > 0;
	// SBO 2.0 GM mode: the Game Master whose monster killed ("<GM>'s <monster>"): his part in his colours
	const int ownerByte = READ_BYTE();
	const int owner = READ_OK() && ownerByte > 0 && ownerByte <= MAX_PLAYERS ? ownerByte : 0;
	const int victimOwnerByte = READ_BYTE(); // the victim NPC's Game Master
	const int victimOwner = READ_OK() && victimOwnerByte > 0 && victimOwnerByte <= MAX_PLAYERS ? victimOwnerByte : 0;

	if (killer < 0 || killer > MAX_PLAYERS)
		killer = 0;
	if (victim < 0 || victim > MAX_PLAYERS)
		victim = 0;

	const int local = sbo::GetLocalPlayerIndex();
	const char *killerName = killer ? sbo::GetPlayerName(killer) : (killerNpc[0] ? killerNpc : nullptr);
	const char *victimName = victim ? sbo::GetPlayerName(victim) : (victimNpc[0] ? victimNpc : nullptr);
	if (!victimName)
		return 1; // unknown player slot: nothing to show (a following DeathMsg shows it the stock way)

	if ((flags & SBO_KF_REPLACES_DEATHMSG) && victim)
		VGui2_DeathNotice_SkipNextDeathMsg(victim);

	// Feed icon: the code's HUD icon (d_grenade, d_satchel, ...) if there is one, else the skull (an NPC's own
	// attack: its classname has no d_ sprite); the weapon is never drawn as text
	VGui2_DeathNotice_AddEntry(killerName,
		killer ? VGUI2_KF_COLOR_CLIENT(killer) : (owner ? VGUI2_KF_COLOR_OWNED(owner) : VGUI2_KF_COLOR_NPC),
		killer != 0 && killer == local, victimName,
		victim ? VGUI2_KF_COLOR_CLIENT(victim) : (victimOwner ? VGUI2_KF_COLOR_OWNED(victimOwner) : VGUI2_KF_COLOR_NPC),
		victim != 0 && victim == local, weapon,
		(killerName ? 0 : VGUI2_KF_FLAG_SUICIDE) | (headshot ? VGUI2_KF_FLAG_HEADSHOT : 0));

	// console line like CHudDeathNotice::MsgFunc_DeathMsg's (player names with their colour codes drawn as colours,
	// else without them)
	if (killerName)
	{
		ConsolePrintName(killerName);
		sbo::ConsolePrint(" killed ");
		ConsolePrintName(victimName);
		if (weaponName[0])
		{
			// the weapon code's player-facing name ("wong" -> "Fists")
			char displayName[64];
			VGui2_WeaponDisplayName(weaponName, displayName, sizeof(displayName));
			sbo::ConsolePrint(" with ");
			sbo::ConsolePrint(displayName);
		}
	}
	else
	{
		ConsolePrintName(victimName);
		sbo::ConsolePrint(" died");
	}
	sbo::ConsolePrint("\n");
	return 1;
}
} // namespace

int VGui2_SpecTargetViewModel(int player)
{
	if (player < 1 || s_SpecTarget.player != player)
		return -1;
	return s_SpecTarget.viewModel;
}

void SboHud_InitChat()
{
	s_bUseVGui2Chat = VGui2_Chat_Init();

	if (s_bUseVGui2Chat)
	{
		s_VGui2ChatProxy.Init();
		// The legacy CHudSayText isn't initialised in this mode, but other HUD code (CHudSpectator::Init) still reads
		// its cvar pointers: point them at the same-named cvars the VGUI2 chat registered.
		gHUD.m_SayText.m_HUD_saytext = gEngfuncs.pfnGetCvarPointer("hud_saytext");
		gHUD.m_SayText.m_HUD_saytext_time = gEngfuncs.pfnGetCvarPointer("hud_saytext_time");
	}
	else
		gHUD.m_SayText.Init(); // engine without client VGUI2: SBO's legacy chat

	// VGUI2 scoreboard (replaces the VGUI1 ScorePanel when VGUI2 is up; see TeamFortressViewport::ShowScoreBoard)
	VGui2_Score_Init();

	// Console colors (color codes in console lines; docs/CONSOLE_COLOR.md)
	if (s_bUseVGui2Chat)
		VGui2_Console_Init();

	// SBO's menus (docs/dev/VGUI2_MIGRATION.md)
	VGui2_Menus_Init();

	// CS:CZ tutor window (env_tutor: TutorText / TutorPart / TutorClose; docs/TUTOR_PORT.md)
	VGui2_Tutor_Init();

	// game_timer HUD timer (the GameTimer message; docs/GAME_TIMER.md)
	VGui2_GameTimer_Init();

	// trigger_objective list (the Objective message; docs/OBJECTIVES.md)
	VGui2_Objectives_Init();

	// game_messagebox message boxes (MessageBox / MsgBoxPart; docs/MESSAGE_BOX.md)
	VGui2_MessageBox_Init();

	// Game Master mode spawn window (GmMode; docs/GM_MODE.md)
	VGui2_GmSpawn_Init();

	// Color correction editor (colorcorrectionui; docs/COLOR_CORRECTION.md "Editor")
	VGui2_CcEditor_Init();

	// Particle editor (particleeditorui; docs/AURORA.md "Editor")
	VGui2_ParticleEditor_Init();

	// World screen editor (worldscreeneditorui; docs/WORLD_SCREENS.md "Editor")
	VGui2_WorldScreenEditor_Init();

	gEngfuncs.pfnHookUserMsg((char *)"SboXP", MsgFunc_SboXP);
	gEngfuncs.pfnHookUserMsg((char *)"SboModeInfo", MsgFunc_SboModeInfo); // scoreboard round / frags left line
	gEngfuncs.pfnHookUserMsg((char *)"RoundStatus", MsgFunc_RoundStatus); // the loadout menu's NEXT ROUND IN
	gEngfuncs.pfnHookUserMsg((char *)"Cheats", MsgFunc_Cheats); // the server's sv_cheats value (the in-game editors)
	gEngfuncs.pfnHookUserMsg((char *)"HeavyLimit", MsgFunc_HeavyLimit); // the loadout menu's FULL heavy weapons
	gEngfuncs.pfnHookUserMsg((char *)"SboDeathMsg", MsgFunc_SboDeathMsg); // NPC kill feed rows (server SboKillfeed_*)
	gEngfuncs.pfnHookUserMsg((char *)"SpecHealth2", MsgFunc_SpecHealth2); // spectator target info (server Observer_SendTargetInfo)
	ServerMenu_Init(); // plugins' server menus ("ServerMenu"; docs/SERVER_MENUS.md)
}

void SboHud_ChatPrint(const char *msg)
{
	if (s_bUseVGui2Chat)
		VGui2_Chat_Print(msg);
	else
		gHUD.m_SayText.SayTextPrint(msg, 128);
}

namespace sbo
{
//-----------------------------------------------------------------------------
// Engine
//-----------------------------------------------------------------------------
int GetMaxClients()
{
	return gEngfuncs.GetMaxClients();
}

int Cmd_Argc()
{
	return gEngfuncs.Cmd_Argc();
}

const char *Cmd_Argv(int i)
{
	return gEngfuncs.Cmd_Argv(i);
}

void ExecClientCmd(const char *cmd)
{
	gEngfuncs.pfnClientCmd((char *)cmd);
}

const char *GetLevelName()
{
	return gEngfuncs.pfnGetLevelName();
}

const char *GetLocalUserInfo(const char *key)
{
	const char *v = gEngfuncs.LocalPlayerInfo_ValueForKey ? gEngfuncs.LocalPlayerInfo_ValueForKey(key) : nullptr;
	return v ? v : "";
}

void *GetVoiceTweak()
{
	return gEngfuncs.pVoiceTweak;
}

bool IsConnected()
{
	if (!gEngfuncs.pNetAPI)
		return false;
	net_status_t status;
	memset(&status, 0, sizeof(status));
	gEngfuncs.pNetAPI->Status(&status);
	return status.connected != 0;
}

bool GetServerAddress(unsigned &ip, unsigned short &port, bool &loopback)
{
	ip = 0;
	port = 0;
	loopback = false;
	if (!gEngfuncs.pNetAPI)
		return false;
	net_status_t status;
	memset(&status, 0, sizeof(status));
	gEngfuncs.pNetAPI->Status(&status);
	const netadr_t &a = status.remote_address;
	if (a.type == NA_LOOPBACK)
	{
		loopback = true;
		return true;
	}
	if (a.type != NA_IP)
		return false;
	ip = ((unsigned)a.ip[0] << 24) | ((unsigned)a.ip[1] << 16) | ((unsigned)a.ip[2] << 8) | a.ip[3];
	port = (unsigned short)(((a.port & 0xff) << 8) | ((a.port >> 8) & 0xff)); // network byte order in netadr_t
	return ip != 0 && port != 0;
}

const char *GetGameDirectory()
{
	return gEngfuncs.pfnGetGameDirectory();
}

const char *GetLaunchParm(const char *name)
{
	char *value = nullptr;
	if (!gEngfuncs.CheckParm || !gEngfuncs.CheckParm((char *)name, &value))
		return nullptr;
	return value ? value : "";
}

void ConsolePrint(const char *text)
{
	gEngfuncs.pfnConsolePrint(text);
}

void DevPrint(const char *text)
{
	gEngfuncs.Con_DPrintf("%s", text);
}

void HookUserMsg(const char *name, UserMsgHookFn fn)
{
	gEngfuncs.pfnHookUserMsg((char *)name, fn);
}

void PlaySoundByName(const char *sample, float volume)
{
	gEngfuncs.pfnPlaySoundByName((char *)sample, volume);
}

void AddCommand(const char *name, CmdFn fn)
{
	gEngfuncs.pfnAddCommand((char *)name, fn);
}

bool ReplaceEngineCommand(const char *name, CmdFn newFn, CmdFn *pOldFn)
{
	if (!s_pEngineFuncsFull)
		return false;

	if (!s_pEngineFuncsFull->GetFirstCmdFunctionHandle)
		return false;

	for (cmd_function_t *item = reinterpret_cast<cmd_function_t *>(s_pEngineFuncsFull->GetFirstCmdFunctionHandle());
	     item; item = item->next)
	{
		if (item->name && !strcmp(item->name, name))
		{
			if (pOldFn)
				*pOldFn = item->function;
			item->function = newFn;
			return true;
		}
	}

	return false;
}

CvarHandle RegisterCvar(const char *name, const char *defValue, bool archive, bool userinfo)
{
	const int flags = (archive ? FCVAR_ARCHIVE : 0) | (userinfo ? FCVAR_USERINFO : 0);
	return reinterpret_cast<CvarHandle>(gEngfuncs.pfnRegisterVariable((char *)name, (char *)defValue, flags));
}

float CvarGetFloat(CvarHandle cvar)
{
	return cvar ? reinterpret_cast<struct cvar_s *>(cvar)->value : 0.0f;
}

void CvarSetFloat(const char *name, float value)
{
	gEngfuncs.Cvar_SetValue((char *)name, value);
}

const char *CvarGetString(CvarHandle cvar)
{
	const struct cvar_s *pCvar = reinterpret_cast<struct cvar_s *>(cvar);
	return (pCvar && pCvar->string) ? pCvar->string : "";
}

void CvarSetString(const char *name, const char *value)
{
	// cl_enginefunc_t::Cvar_Set: Steam-era engines (like GetFirstCmdFunctionHandle, used by ReplaceEngineCommand)
	gEngfuncs.Cvar_Set((char *)name, (char *)value);
}

const char *LookupKeyBinding(const char *command)
{
	// cl_enginefunc_t::Key_LookupBinding (the engine's Key_NameForBinding, SDK 2.3; ricochet's and BHL's input.cpp
	// pass "binding + 1"): it skips a binding's leading '+' and compares strlen(command) characters case-insensitively,
	// so it takes "reload" (never "+reload", which matches nothing) and returns the first key whose binding starts so.
	if (command && (command[0] == '+' || command[0] == '-'))
		command++;
	if (!command || !command[0] || !gEngfuncs.Key_LookupBinding)
		return nullptr;

	const char *key = gEngfuncs.Key_LookupBinding(command);
	return (key && key[0]) ? key : nullptr;
}

//-----------------------------------------------------------------------------
// Players
//-----------------------------------------------------------------------------
int GetLocalPlayerIndex()
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	return local ? local->index : 0;
}

const char *GetPlayerName(int clientIndex)
{
	if (clientIndex <= 0 || clientIndex > MAX_PLAYERS)
		return nullptr;

	GetPlayerInfo(clientIndex, &g_PlayerInfoList[clientIndex]);
	const char *name = g_PlayerInfoList[clientIndex].name;

	if (!name || !name[0])
		return nullptr;

	return name;
}

const char *GetPlayerInfoValue(int clientIndex, const char *key)
{
	if (clientIndex <= 0 || clientIndex > MAX_PLAYERS || !gEngfuncs.PlayerInfo_ValueForKey)
		return "";

	const char *value = gEngfuncs.PlayerInfo_ValueForKey(clientIndex, key);
	return value ? value : "";
}

bool IsMenuCursorVisible()
{
	return ::g_iVisibleMouse != 0;
}

void ClearCenterText()
{
	::CenterPrint("");
}

void GetClientColor(int clientIndex, int &r, int &g, int &b)
{
	if (clientIndex < 0 || clientIndex > MAX_PLAYERS)
		clientIndex = 0;

	// Same table as SBO's legacy chat / death notices (death.cpp):
	// 1 blue, 2 red, 3 yellow, 4 green, 0 yellow, other grey.
	const float *c = ::GetClientColor(clientIndex);
	r = (int)(c[0] * 255.0f);
	g = (int)(c[1] * 255.0f);
	b = (int)(c[2] * 255.0f);
}

void GetTeamColor(int teamNumber, int &r, int &g, int &b)
{
	const float *c = ::GetTeamColor(teamNumber);
	r = (int)(c[0] * 255.0f);
	g = (int)(c[1] * 255.0f);
	b = (int)(c[2] * 255.0f);
}

bool IsPlayerBlocked(int clientIndex)
{
	CVoiceStatus *mgr = GetClientVoiceMgr();
	return mgr && mgr->IsPlayerBlocked(clientIndex);
}

bool CanPlayerBeBlocked(int clientIndex)
{
	CVoiceStatus *mgr = GetClientVoiceMgr();
	return mgr && mgr->CanPlayerBeBlocked(clientIndex);
}

void SetPlayerBlocked(int clientIndex, bool blocked)
{
	CVoiceStatus *mgr = GetClientVoiceMgr();
	if (mgr)
		mgr->SetPlayerBlockedState(clientIndex, blocked);
}

bool IsPlayerSpeaking(int clientIndex)
{
	CVoiceStatus *mgr = GetClientVoiceMgr();
	if (!mgr || clientIndex <= 0 || clientIndex > VOICE_MAX_PLAYERS)
		return false;

	if (mgr->m_VoicePlayers[clientIndex - 1])
		return true;

	// BHL/HL25 CVoiceStatus::UpdateSpeakerStatus(-1) also marks the local player's slot as speaking; SBO's keeps
	// only m_bTalking for it
	// (GetLocalPlayer is garbage while not connected: cl_entities is null then -- BHL's GetEntityByIndex(0) check)
	return mgr->m_bTalking && gEngfuncs.GetEntityByIndex(0) && GetLocalPlayerIndex() == clientIndex;
}

bool CanShowSpeakerLabels()
{
	CVoiceStatus *mgr = GetClientVoiceMgr();
	return mgr && mgr->m_pHelper && mgr->m_pHelper->CanShowSpeakerLabels();
}

//-----------------------------------------------------------------------------
// Scoreboard
//-----------------------------------------------------------------------------
namespace
{
// The engine's player_info_t of the SteamPipe engines (HL25 included), as in BugfixedHL-Rebased
// src/common/com_model.h. hud_player_info_t::name points at player_info_t::name, so the full record (and the
// SteamID the engine got from Steam) is reachable from it (BHL CPlayerInfo::GetEnginePlayerInfo).
struct engine_player_info_t
{
	int userid;
	char userinfo[256];
	char name[32];
	int spectator;
	int ping;
	int packet_loss;
	char model[64];
	int topcolor;
	int bottomcolor;
	int renderframe;
	int gaitsequence;
	float gaitframe;
	float gaityaw;
	float prevgaitorigin[3];
	customization_t customdata;
	char hashedcdkey[16];
	unsigned long long m_nSteamID;
};
} // namespace

void GetScorePlayer(int clientIndex, ScorePlayer &out, bool bReadEngineSteamID)
{
	out = ScorePlayer();

	if (clientIndex <= 0 || clientIndex > MAX_PLAYERS || clientIndex > SCORE_MAX_PLAYERS)
		return;

	hud_player_info_t &info = g_PlayerInfoList[clientIndex];
	GetPlayerInfo(clientIndex, &info);

	if (!info.name || !info.name[0])
		return;

	const extra_player_info_t &extra = g_PlayerExtraInfo[clientIndex];
	out.connected = true;
	out.name = info.name;
	out.ping = info.ping;
	out.packetloss = info.packetloss;
	out.thisplayer = info.thisplayer != 0;
	out.spectator = info.spectator != 0;
	out.frags = extra.frags;
	out.deaths = extra.deaths;
	out.teamnumber = extra.teamnumber < 0 ? 0 : extra.teamnumber;
	out.playerclass = extra.playerclass;
	out.teamname = extra.teamname;
	out.xp = s_iSboXP[clientIndex];

	if (bReadEngineSteamID)
	{
		const engine_player_info_t *pEngine = reinterpret_cast<const engine_player_info_t *>(
		    info.name - offsetof(engine_player_info_t, name));
		out.steamID64 = pEngine->m_nSteamID;
	}
}

int GetServerCheats()
{
	return s_iServerCheats;
}

bool HasSboXP()
{
	return s_bSboXPReceived;
}

void ResetSboXP()
{
	memset(s_iSboXP, 0, sizeof(s_iSboXP));
	s_bSboXPReceived = false;
}

void GetModeInfo(int &type, int &value1, int &value2)
{
	type = s_iModeInfo[0];
	value1 = s_iModeInfo[1];
	value2 = s_iModeInfo[2];
}

void ResetModeInfo()
{
	s_iModeInfo[0] = MODEINFO_NONE;
	s_iModeInfo[1] = s_iModeInfo[2] = 0;
	s_iRoundStatus = ROUNDSTATUS_NONE;
	s_flRoundStatusEnd = -1.0f;
	s_bHeavyLimit = false;
}

bool GetHeavyLimit(int heavy, int &taken, int &max)
{
	taken = max = 0;
	const int i = heavy == 7 ? 0 : (heavy == 9 ? 2 : -1); // the "heavy" command's grenade / nuclear launcher
	if (!s_bHeavyLimit || i < 0 || s_iHeavyLimit[i + 1] <= 0)
		return false;
	taken = s_iHeavyLimit[i];
	max = s_iHeavyLimit[i + 1];
	return true;
}

int GetRoundStatus(int &seconds)
{
	seconds = 0;
	if (s_iRoundStatus == ROUNDSTATUS_NEXT)
	{
		const float left = s_flRoundStatusEnd - gEngfuncs.GetClientTime();
		if (left <= 0.0f)
			return ROUNDSTATUS_NONE; // run out: until the server's next word
		seconds = (int)ceilf(left);
	}
	return s_iRoundStatus;
}

namespace
{
// Level name is "maps/co_xxx.bsp"
bool MapHasPrefix(const char *prefix)
{
	const char *level = gEngfuncs.pfnGetLevelName();
	if (!level)
		return false;
	const char *base = strrchr(level, '/');
	base = base ? base + 1 : level;
	return !strncmp(base, prefix, strlen(prefix));
}
} // namespace

bool IsCoopMap()
{
	// gamerules.cpp InstallGameRules: "co_" maps run CCoop.
	return MapHasPrefix("co_");
}

bool IsTeamplayMap()
{
	// gamerules.cpp InstallGameRules: "sp_" maps run CHalfLifeTeamplay (its frag limit counts team scores).
	// Not gHUD.m_Teamplay: the server's GameMode message also says "teams" for SBO's scoreboard categories.
	return MapHasPrefix("sp_");
}

bool IsBrMap()
{
	// gamerules.cpp InstallGameRules: "br_" maps run CBr.
	return MapHasPrefix("br_");
}

bool IsBountyMap()
{
	// gamerules.cpp InstallGameRules: every other map runs CDm (Bounty Killer; the "bk_" maps).
	return !IsTeamplayMap() && !IsCoopMap() && !IsBrMap();
}

int GetTeamNameCount()
{
	return gViewPort ? gViewPort->GetNumberOfTeams() : 0;
}

const char *GetTeamNameLabel(int teamNumber)
{
	if (!gViewPort || teamNumber < 1 || teamNumber > gViewPort->GetNumberOfTeams() || teamNumber > MAX_TEAMNAMES)
		return "";
	return gViewPort->GetTeamName(teamNumber);
}

bool GetTeamScoreOverride(const char *teamName, int &frags, int &deaths)
{
	if (!teamName || !teamName[0])
		return false;

	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		if (g_TeamInfo[i].name[0] && !stricmp(g_TeamInfo[i].name, teamName) && g_TeamInfo[i].scores_overriden)
		{
			frags = g_TeamInfo[i].frags;
			deaths = g_TeamInfo[i].deaths;
			return true;
		}
	}

	return false;
}

const char *GetServerName()
{
	return gViewPort ? gViewPort->m_szServerName : "";
}

const char *LocaliseText(const char *msg)
{
	return CHudTextMessage::BufferedLocaliseTextString(msg);
}

const char *LookupTitle(const char *name)
{
	client_textmessage_t *clmsg = TextMessageGet(name);
	return (clmsg && clmsg->pMessage) ? clmsg->pMessage : nullptr;
}

bool IsIntermission()
{
	return gHUD.m_iIntermission != 0;
}

float GetHudTime()
{
	return gHUD.m_flTime;
}

void ReleaseMouseForCursor()
{
	IN_ReleaseRelativeMouse();
}

//-----------------------------------------------------------------------------
// Spectator target info
//-----------------------------------------------------------------------------
bool GetSpectatorTargetInfo(SpectatorTargetInfo &out)
{
	out = SpectatorTargetInfo();

	// spec_drawinfo 0 (the spectator menu's Settings > Target Info): the player turned the box off
	if (gHUD.m_Spectator.m_drawinfo && gHUD.m_Spectator.m_drawinfo->value == 0)
		return false;

	// the modes that follow one player; roaming has no target, the free overview shows every player
	switch (g_iUser1)
	{
	case OBS_CHASE_LOCKED:
	case OBS_CHASE_FREE:
	case OBS_IN_EYE:
	case OBS_MAP_CHASE:
		break;
	default:
		return false;
	}

	if (g_iUser2 < 1 || g_iUser2 > gEngfuncs.GetMaxClients() || g_iUser2 > MAX_PLAYERS)
		return false;

	// only what the server sent for this very player; a dying / dead target (still watched for a moment) has none
	if (s_SpecTarget.player != g_iUser2 || s_SpecTarget.health <= 0)
		return false;

	out.player = s_SpecTarget.player;
	out.health = s_SpecTarget.health;
	out.armor = s_SpecTarget.armor;
	out.legsBroken = (s_SpecTarget.flags & SPECINFO_LEGS_BROKEN) != 0;
	out.brWins = s_SpecTarget.brWins;

	// the id is the WeaponList one (every client gets the whole list on joining): its classname
	WEAPON *pWeapon = s_SpecTarget.weapon > 0 ? gWR.GetWeapon(s_SpecTarget.weapon) : NULL;
	if (pWeapon && pWeapon->iId == s_SpecTarget.weapon)
		out.weapon = pWeapon->szName;

	return true;
}

//-----------------------------------------------------------------------------
// GM mode (sbo_gm_hud.cpp)
//-----------------------------------------------------------------------------
void GetLocalView(float origin[3], float angles[3])
{
	for (int i = 0; i < 3; i++)
	{
		origin[i] = v_origin[i];
		angles[i] = v_angles[i];
	}
}

int GetLocalFov()
{
	return gHUD.m_iFOV > 0 ? gHUD.m_iFOV : 90;
}

bool GetGmOverview(GmOverview &out)
{
	const overviewInfo_t &o = gHUD.m_Spectator.m_OverviewData;
	if (o.layers <= 0 || o.zoom <= 0)
		return false;
	for (int i = 0; i < 3; i++)
		out.origin[i] = o.origin[i];
	out.zoom = o.zoom;
	out.rotated = o.rotated != 0;
	out.layers = o.layers < 8 ? o.layers : 8;
	for (int i = 0; i < out.layers; i++)
	{
		out.heights[i] = o.layersHeights[i];
		strncpy(out.images[i], o.layersImages[i], sizeof(out.images[i]) - 1);
		out.images[i][sizeof(out.images[i]) - 1] = 0;
	}
	return true;
}

} // namespace sbo
