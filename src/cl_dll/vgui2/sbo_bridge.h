//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: The narrow interface the VGUI2 code (compiled with hl1_source_sdk
//          include paths) uses to reach SBO's client (compiled with SBO's own
//          SDK 2.3 / VGUI1 include paths).
//
// Why a bridge: SBO's headers (hud.h -> cl_dll.h -> engine/cdll_int.h,
// game_shared/voice_status.h -> VGUI1 headers, common/interface.h, common/mathlib.h)
// collide with hl1_source_sdk's headers (tier1/interface.h, mathlib/, ::Color,
// basetypes). Instead of untangling both header worlds, the VGUI2 translation
// units include only this header (plain C++ types, no SDK or SBO headers), and
// sbo_bridge.cpp implements it on the SBO side.
//
// The opposite direction (SBO -> VGUI2) is vgui2_api.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_BRIDGE_H
#define SBO_VGUI2_SBO_BRIDGE_H

namespace sbo
{
// Engine user message hook (same signature as SBO's pfnUserMsgHook).
typedef int (*UserMsgHookFn)(const char *pszName, int iSize, void *pbuf);
// Engine console command callback.
typedef void (*CmdFn)(void);

// Opaque engine cvar handle (cvar_t *).
typedef struct CvarHandle_s *CvarHandle;

//-----------------------------------------------------------------------------
// Engine
//-----------------------------------------------------------------------------
int GetMaxClients();
int Cmd_Argc();
const char *Cmd_Argv(int i);
void ExecClientCmd(const char *cmd);
const char *GetLevelName();
//! The server the engine is connected or connecting to (net_api Status remote_address): an IPv4 address in host
//! byte order and its port; loopback: true for a game this client hosts (no address then). False when the engine has
//! no remote address.
bool GetServerAddress(unsigned &ip, unsigned short &port, bool &loopback);
//! The engine is connected or connecting to a server (net_api Status connected)
bool IsConnected();
//! The engine's voice tweak interface (cl_enginefunc_t::pVoiceTweak, an IVoiceTweak_s: ivoicetweak.h), or nullptr
void *GetVoiceTweak();
//! A key of this client's own userinfo ("" if none; cl_enginefunc_t::LocalPlayerInfo_ValueForKey)
const char *GetLocalUserInfo(const char *key);
// the game folder's name ("syndicate"), gEngfuncs.pfnGetGameDirectory
const char *GetGameDirectory();
//! A launch option's value ("-name value" on the command line, gEngfuncs.CheckParm): nullptr if the option is not
//! given, "" if it has no value
const char *GetLaunchParm(const char *name);
void ConsolePrint(const char *text);
void HookUserMsg(const char *name, UserMsgHookFn fn);
void PlaySoundByName(const char *sample, float volume);
void AddCommand(const char *name, CmdFn fn);

// Replaces the callback of an engine console command (e.g. "messagemode")
// through cl_enginefunc_t::GetFirstCmdFunctionHandle (engine table past SDK 2.3's
// end, see sbo_bridge.cpp). Returns false if the engine doesn't expose it or the
// command doesn't exist. *pOldFn receives the engine's callback.
bool ReplaceEngineCommand(const char *name, CmdFn newFn, CmdFn *pOldFn);

//! userinfo: FCVAR_USERINFO, the cvar's value is the key <name> of the player's userinfo (sent to every client)
CvarHandle RegisterCvar(const char *name, const char *defValue, bool archive, bool userinfo = false);
float CvarGetFloat(CvarHandle cvar);
void CvarSetFloat(const char *name, float value);
//! The cvar's string value ("" for a null handle).
const char *CvarGetString(CvarHandle cvar);
void CvarSetString(const char *name, const char *value);

//! Name of the first key bound to a command ("+reload" or "reload" -> "r", "MOUSE1", "SHIFT"), the engine's own key
//! names; nullptr if no key is bound to it (cl_enginefunc_t::Key_LookupBinding, a prefix match: "attack" also finds a
//! "+attack2" key; the tutor uses IGameUIFuncs for exact matches).
const char *LookupKeyBinding(const char *command);

//-----------------------------------------------------------------------------
// Players
//-----------------------------------------------------------------------------
// Index of the local player (1..maxclients), 0 if unknown.
int GetLocalPlayerIndex();
// Refreshes and returns the player's name from the engine, nullptr if the slot is empty.
const char *GetPlayerName(int clientIndex);
// A key of the player's userinfo as the engine has it (cl_enginefunc_t::PlayerInfo_ValueForKey: the server sends every
// client's userinfo to all clients, without the keys that start with '_'), "" if the key or the player isn't there.
// The engine's static buffer: use the value at once.
const char *GetPlayerInfoValue(int clientIndex, const char *key);
// The sender's team color, exactly what SBO's legacy chat used (death.cpp GetClientColor,
// keyed on g_PlayerExtraInfo[].teamnumber from the ScoreInfo/TeamInfo messages).
void GetClientColor(int clientIndex, int &r, int &g, int &b);
// SBO's voice mute state (game_shared/voice_status.cpp, CVoiceStatus::IsPlayerBlocked).
bool IsPlayerBlocked(int clientIndex);
// SBO's voice mute state is false for the local player whatever the ban list holds.
// The player can be muted (CVoiceStatus::CanPlayerBeBlocked): not the local player, and he has a ban list key of his
// own (his SteamID64, else the engine's "unique ID" unless he shares it with the local player). False for empty slots.
// SetPlayerBlocked does nothing for the others.
bool CanPlayerBeBlocked(int clientIndex);

//-----------------------------------------------------------------------------
// Voice status (hud_voice_status.cpp). SBO's CVoiceStatus (game_shared/voice_status.cpp) stays the model.
//-----------------------------------------------------------------------------
//! The player is talking (CVoiceStatus::m_VoicePlayers; the local player also while m_bTalking, as BHL's
//! CVoiceStatus::UpdateSpeakerStatus(-1) marks him).
bool IsPlayerSpeaking(int clientIndex);
//! IVoiceStatusHelper::CanShowSpeakerLabels (hud.cpp): false while the scoreboard is shown.
bool CanShowSpeakerLabels();

//! True while SBO's viewport wants the mouse cursor (g_iVisibleMouse: MOTD, briefing, team menu, duck menu, ...).
bool IsMenuCursorVisible();
//! Clears the engine's center text (print_center), which the engine draws over VGUI2
void ClearCenterText();

//-----------------------------------------------------------------------------
// Team colours: ONE table for chat names, scoreboard section headers and scoreboard names
// (death.cpp GetTeamColor: 1 blue, 2 red, 3 yellow, 4 green, 0 yellow, else grey).
//-----------------------------------------------------------------------------
void GetTeamColor(int teamNumber, int &r, int &g, int &b);

//-----------------------------------------------------------------------------
// Scoreboard (score_panel.cpp)
//-----------------------------------------------------------------------------
// Highest player slot the scoreboard looks at (GoldSrc: 32).
constexpr int SCORE_MAX_PLAYERS = 32;

struct ScorePlayer
{
	bool connected = false;
	const char *name = "";     //!< engine name (may contain ^ colour codes)
	int ping = 0;
	int packetloss = 0;
	bool thisplayer = false;
	bool spectator = false;    //!< engine spectator flag (HLTV / spectate-only)
	int frags = 0;             //!< ScoreInfo frags (SBO coop: level)
	int deaths = 0;
	int teamnumber = 0;        //!< ScoreInfo team (>= 0)
	int playerclass = 0;       //!< ScoreInfo playerclass (SBO Bounty Killer: 1 = the Hunted, alive or dead; else 0)
	const char *teamname = ""; //!< TeamInfo string
	int xp = 0;                //!< SboXP message (coop XP towards the next level; 0 if none received)
	unsigned long long steamID64 = 0; //!< raw engine SteamID (only if requested, 0 otherwise)
};

//! Refreshes the player's engine info and fills `out`. bReadEngineSteamID: read player_info_t::m_nSteamID
//! (SteamPipe/HL25 engine layout only -- the caller checks the engine).
void GetScorePlayer(int clientIndex, ScorePlayer &out, bool bReadEngineSteamID);
//! True once the server sent an "SboXP" message this map (this branch's server, coop maps only).
bool HasSboXP();
//! New map: forget the XP values (VGui2_Score_Reset).
void ResetSboXP();
//! Coop map ("co_" prefix, as the server's InstallGameRules): score column = level.
bool IsCoopMap();
//! Teamplay map ("sp_" prefix, as the server's InstallGameRules): mp_fraglimit ends the map on a team's frags
//! (CHalfLifeTeamplay::Think), on every other map on a player's (CDm / CBr).
bool IsTeamplayMap();
//! Bounty Killer map (every map without a "co_", "br_" or "sp_" prefix, e.g. "bk_": InstallGameRules runs CDm): the
//! Hunted is ScoreInfo playerclass 1 (and team 1 while in play).
bool IsBountyMap();
//! BR map ("br_" prefix, as the server's InstallGameRules): last-survivor rounds (CBr).
bool IsBrMap();
//! "SboModeInfo" message (server SboModeInfo_*, player.h SBO_MODEINFO_*): the scoreboard's round / frags left line.
constexpr int MODEINFO_NONE = 0;      //!< no line (old server, or none received yet this map)
constexpr int MODEINFO_ROUNDS = 1;    //!< co-op: value1 current round (0 = first round not started), value2 round limit
constexpr int MODEINFO_FRAGLIMIT = 2; //!< value1 mp_fraglimit (0 = no frag limit: no line); shown as "Frags left"
void GetModeInfo(int &type, int &value1, int &value2);
//! New map: no line until the server sends one (VGui2_Score_Reset).
void ResetModeInfo();
//! "RoundStatus" (server coop.cpp RoundStatus_Update): what the co-op / BR round does, for the new loadout menu
constexpr int ROUNDSTATUS_NONE = 0;    //!< nothing to show (no message: an older server, Bounty Killer)
constexpr int ROUNDSTATUS_NEXT = 1;    //!< the next round starts in `seconds`
constexpr int ROUNDSTATUS_WAITING = 2; //!< waiting for players, no time limit
constexpr int ROUNDSTATUS_PLAYING = 3; //!< a round in progress
int GetRoundStatus(int &seconds);
//! "HeavyLimit" (server coop.cpp HeavyLimit_Update): the co-op team limit of the heavy weapon `heavy` (the "heavy"
//! command's 1-9; 7 grenade launcher, 9 nuclear launcher). taken = how many the other players hold; full when
//! taken >= max. False when that weapon has no limit here (other weapons, Bounty Killer, BR, an older server).
bool GetHeavyLimit(int heavy, int &taken, int &max);
//! Number of team labels from the last TeamNames message (0 = none received).
int GetTeamNameCount();
//! Localised TeamNames label for team 1..GetTeamNameCount(), "" otherwise.
const char *GetTeamNameLabel(int teamNumber);
//! TeamScore override for a TeamInfo team name; false if the server didn't send one.
bool GetTeamScoreOverride(const char *teamName, int &frags, int &deaths);
//! ServerName message.
const char *GetServerName();
//! titles.txt localisation (CHudTextMessage::BufferedLocaliseTextString), e.g. "#Muted".
const char *LocaliseText(const char *msg);
//! The text of a titles.txt entry (name without '#', any case), nullptr if there is none (TextMessageGet, as
//! CHudTextMessage::LookupString). The engine's copy: valid until the next map.
const char *LookupTitle(const char *name);
void SetPlayerBlocked(int clientIndex, bool blocked);
bool IsIntermission();
float GetHudTime();
//! Call before vgui2::input()->SetCursorPos when a panel turns the VGUI2 cursor on: gives the mouse back to the
//! cursor now (inputw32.cpp IN_ReleaseRelativeMouse; HL25 raw input / SDL relative mouse mode), so the position is
//! applied to the real cursor. Positions for SetCursorPos are in the game window (0,0 = its top left corner), NOT
//! the desktop coordinates of gEngfuncs.GetWindowCenterX/Y.
void ReleaseMouseForCursor();

//-----------------------------------------------------------------------------
// Kill feed (hud_deathnotice.cpp). Implemented in sbo_killfeed_bridge.cpp (SBO world).
//-----------------------------------------------------------------------------
//! gHUD sprite index of a hud.txt name (e.g. "d_crowbar"), -1 if the current resolution has none.
int GetHudSpriteIndex(const char *name);
//! Size of a gHUD sprite's rectangle; false (0x0) for an invalid index.
bool GetHudSpriteSize(int spriteIndex, int &wide, int &tall);
//! Draws a gHUD sprite additively with the TriangleAPI at (x, y) in the current VGUI2 paint context (BHL
//! CHudRenderer::DrawSprite: the engine's VGUI2 surface translates GL to the panel). Leaves kRenderNormal set.
void DrawHudSpriteAdditive(int spriteIndex, int x, int y, int r, int g, int b);
//! The legacy HUD would draw now: hud_draw on, HUD not hidden (HIDEHUD_ALL), no intermission, connected, and SBO's
//! viewport allows text (no full-screen team menu / MOTD).
bool KillFeedShouldDraw();
//! Engine cvar by name (pfnGetCvarPointer), nullptr if it doesn't exist.
CvarHandle FindCvar(const char *name);

//-----------------------------------------------------------------------------
// HUD text menu (sbo_hud_menu.cpp). Implemented in sbo_menu_bridge.cpp (SBO world).
//-----------------------------------------------------------------------------
//! The text menu would be drawn now: KillFeedShouldDraw() and the scoreboard isn't shown (CHudMenu::Draw's check).
bool HudMenuShouldDraw();
//! HUD scaling (hud_scale) is on: the HUD is laid out in HUD units, GetHudScale() pixels each.
bool IsHudScaled();
float GetHudScale();
//! HUD units -> screen pixels (HUD_ToScreenX, includes the centring offset).
int HudToScreenX(int x);

//-----------------------------------------------------------------------------
// VGUI2 menus (sbo_menus.cpp, sbo_widgets.cpp, ...). Implemented in sbo_menu_bridge.cpp (SBO world).
//-----------------------------------------------------------------------------
//! One scheme of SBO's text scheme, the SDK's VGUI1 one (gamedir/syndicate/<res>_textscheme.txt, read by the viewport's
//! CSchemeManager for the resolution it was created at). Colours in the file's convention: alpha 255 = opaque.
struct TextSchemeInfo
{
	char fontName[48] = "";
	int fontSize = 0;   //!< VGUI1 Font tall in pixels
	int fontWeight = 0; //!< 0 = normal, 700 = bold
	unsigned char fg[4] = {};
	unsigned char bg[4] = {};
	unsigned char armedFg[4] = {};
	unsigned char armedBg[4] = {};
};
//! Scheme by name ("Primary Button Text", "Title Font", "Briefing Text", ...). False while SBO's viewport
//! doesn't exist. An unknown name gives the file's first scheme (CSchemeManager::getSchemeHandle).
bool GetTextSchemeInfo(const char *schemeName, TextSchemeInfo &out);
//! SBO's ScreenWidth / ScreenHeight (the basis of the VGUI1 XRES()/YRES() layout).
void GetScreenSize(int &wide, int &tall);
//! TeamFortressViewport::HideTopMenu(): closes the top VGUI1 menu (incl. a VGUI2 menu's proxy) and opens the next.
void HideTopMenu();
//! gEngfuncs.COM_LoadFile( path, 5, NULL ), as SBO's VGUI1 menus load their texts (e.g. maps/<map>.txt): the file
//! (0-terminated) from the engine's search paths, nullptr if not found. Free it with FreeFile.
char *LoadFile(const char *path);
void FreeFile(char *pFile);
//! sbo::CommandMenu_Hide() through the viewport (VGUI1 action handlers that don't close their menu ended with it).
void HideCommandMenu();

//! The briefing menu's choices (vgui_SyndBrief.cpp): TeamFortressViewport::m_iBlade ... m_iMaleFemale, 1-based
//! button numbers, kept for the whole session and sent to the server by the briefing's "Go !" button.
enum BriefChoice
{
	BRIEF_BLADE = 0, //!< m_iBlade      "blade %d"
	BRIEF_LIGHT,     //!< m_iLight      "light %d"
	BRIEF_HEAVY,     //!< m_iHeavy      "heavy %d"
	BRIEF_ITEM,      //!< m_iItem       "item %d"
	BRIEF_CYBERWARE, //!< m_iCyberware  "cyberware %d"
	BRIEF_ARMOR,     //!< m_iArmor      "armor %d"
	BRIEF_MALEFEMALE, //!< m_iMaleFemale "malefemale %d" (1 = female, 2 = male)
	BRIEF_CHOICE_COUNT
};
//! The viewport's value (CSyndBrief's defaults, all 1, if the VGUI1 briefing never set them); 1 without a viewport.
int GetBriefChoice(int choice);
void SetBriefChoice(int choice, int value);

//-----------------------------------------------------------------------------
// Spectator UI (sbo_spectator_panel.cpp). Implemented in sbo_menu_bridge.cpp (SBO world).
// (The command menus the spectator bar's buttons open: sbo_command_menu.h.)
//-----------------------------------------------------------------------------
//! SpectatorPanel::ActionSignal commands (spectator_panel.h SPECTATOR_PANEL_CMD_*).
enum SpectatorAction
{
	SPECTATOR_ACTION_OPTIONS = 1,      //!< Options button: ShowCommandMenu( m_SpectatorOptionsMenu ) (toggles)
	SPECTATOR_ACTION_PREVPLAYER = 2,   //!< "<": FindNextPlayer( false )
	SPECTATOR_ACTION_NEXTPLAYER = 3,   //!< ">": FindNextPlayer( true )
	SPECTATOR_ACTION_TOGGLE_INSET = 5, //!< picture-in-picture frame: next inset mode
	SPECTATOR_ACTION_CAMERA = 6,       //!< camera mode button: ShowCommandMenu( m_SpectatorCameraMenu ) (toggles)
};
void SpectatorPanelAction(int action);


//-----------------------------------------------------------------------------
// Spectator target info (sbo_spectator_info.cpp). Implemented in sbo_bridge.cpp (SBO world).
// The state of the watched player that a client can't know by itself, from the server's "SpecHealth2" message
// (server Observer_SendTargetInfo: sent to an observer when its target or one of the values changed). Level / points
// and XP of the target are the scoreboard's (GetScorePlayer).
//-----------------------------------------------------------------------------
struct SpectatorTargetInfo
{
	int player = 0;          //!< the watched player (1..maxclients)
	int health = 0;          //!< > 0
	int armor = 0;
	const char *weapon = ""; //!< classname of the weapon in hand as in the WeaponList message ("weapon_pm66"), "" = none
	bool legsBroken = false; //!< SPECINFO_LEGS_BROKEN
	int brWins = 0;          //!< BR rounds the player won on this map (0 outside BR)
};
//! True (out filled) while the local player watches a living player in a mode that follows one (chase locked, chase
//! free, first person, chase overview) and the server has sent that player's state. False in the roaming and free
//! overview modes, without a target, for a dead target, on HLTV, on servers that don't send the message, and with
//! spec_drawinfo 0 (the spectator menu's Settings > Target Info).
bool GetSpectatorTargetInfo(SpectatorTargetInfo &out);

//-----------------------------------------------------------------------------
// GM mode (sbo_gm_hud.cpp). Implemented in sbo_bridge.cpp (SBO world).
//-----------------------------------------------------------------------------
//! The local player's view (the eye's origin and the view angles, refdef) and field of view (gHUD.m_iFOV, 90 = none)
void GetLocalView(float origin[3], float angles[3]);
int GetLocalFov();
//! The spectator overview of this map (overviews/<map>.txt, CHudSpectator::m_OverviewData); false if none
struct GmOverview
{
	float origin[3];
	float zoom;
	bool rotated;
	int layers;
	float heights[8];
	char images[8][128];
};
bool GetGmOverview(GmOverview &out);

} // namespace sbo

#endif // SBO_VGUI2_SBO_BRIDGE_H
