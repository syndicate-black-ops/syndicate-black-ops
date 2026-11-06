//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: What SBO's client code (VGUI1 / SDK 2.3 world) calls to drive the
//          VGUI2 layer. Plain C++ only: safe to include from both worlds.
//
// Implemented in:
//   sbo_bridge.cpp  (SBO world): VGui2_SetEngineFuncs, SboHud_InitChat, SboHud_ChatPrint
//   client_vgui.cpp (SDK world): VGui2_IsAvailable, VGui2_IsCursorVisible, VGui2_Frame
//   hud_chat.cpp    (SDK world): VGui2_Chat_*
//   sbo_console.cpp (SDK world): VGui2_Console_*
//   score_panel.cpp (SDK world): VGui2_Score_*
//   hud_deathnotice.cpp (SDK world): VGui2_KillFeed_*
//   hud_voice_status.cpp (SDK world): VGui2_VoiceStatus_*
//   death.cpp       (SBO world): VGui2_DeathNotice_AddEntry (VGUI2 kill feed or legacy fallback)
//   tutor_text_window.cpp (SDK world): VGui2_Tutor_*
//   sbo_message_box.cpp (SDK world): VGui2_MessageBox_*
//   sbo_menus.cpp, sbo_text_window.cpp, sbo_gm_menu.cpp, sbo_team_menu.cpp,
//   sbo_brief_menu.cpp, sbo_spectator_panel.cpp (SDK world): VGui2_Menus_*, VGui2_TextWindow_*,
//                   VGui2_GmMenu_*, VGui2_TeamMenu_*, VGui2_BriefMenu_*, VGui2_Spectator_*
//=================================================================//

#ifndef SBO_VGUI2_API_H
#define SBO_VGUI2_API_H

//-----------------------------------------------------------------------------
// Called from SBO code
//-----------------------------------------------------------------------------

// cdll_int.cpp Initialize(): remember the engine's full function table
// (cl_enginefunc_t *, longer than SDK 2.3's struct; needed for the
// messagemode/messagemode2 hooks).
void VGui2_SetEngineFuncs(const void *pEnginefuncs);

// hud.cpp CHud::Init(): replaces m_SayText.Init(). Uses the VGUI2 chat when the
// engine initialized the client's VGUI2 (IClientVGUI), else SBO's legacy CHudSayText.
void SboHud_InitChat();

// text_message.cpp HUD_PRINTTALK: replaces gHUD.m_SayText.SayTextPrint(psz, 128).
void SboHud_ChatPrint(const char *msg);

// true once the engine called IClientVGUI::Initialize (VGUI2 interfaces connected).
bool VGui2_IsAvailable();

// inputw32.cpp: mouse look must stop while a VGUI2 panel shows the cursor
// (chat input open), like BugfixedHL's IN_MouseEvent/IN_MouseMove/IN_Accumulate.
bool VGui2_IsCursorVisible();
// SBO 2.0: a full-screen SBO menu is up (loadout, briefing, MOTD, team, GM and server menus; not the spectator bar):
// text_message.cpp drops the engine's center text (print_center), which the engine draws over VGUI2
bool VGui2_MenuHidesCenterText();
// SBO 2.0: true if the game folder itself (gamedir, e.g. "syndicate") has the file, not a fallback folder (valve/);
// also true when it cannot be told (no file system), so callers only act on a sure "no"
bool VGui2_GameDirHasFile(const char *file);

// cdll_int.cpp HUD_Frame(): frame counter for the ESC guard (see client_viewport.cpp).
void VGui2_Frame();

// The updater (sbo_update_window.cpp, docs/UPDATER.md). cdll_int.cpp Initialize(): finishes or undoes an
// interrupted install before SBO's UI loads any file; HUD_Init(): cl_check_for_updates, update_check,
// update_changelog; HUD_Frame(): the start's check, downloads, the dialog over the main menu.
void VGui2_Updater_Startup();
void VGui2_Updater_Init();
void VGui2_Updater_Frame();
//! cl_experimental_menus changed (VGui2_MenusStyle_Frame): an open update window closes and opens again in the new
//! look on the next frame, with the same state and the notes at the same scroll position
void VGui2_Updater_MenusStyleChanged();

//-----------------------------------------------------------------------------
// VGUI2 chat (hud_chat.cpp)
//-----------------------------------------------------------------------------
// Creates the chat panel (once) and hooks SayText + messagemode/messagemode2.
// Returns false if VGUI2 isn't available.
bool VGui2_Chat_Init();
// New map / InitHUD: closes the input line, resets history fade.
void VGui2_Chat_InitHUDData();
// Prints a text line (HUD_PRINTTALK, client index 0 = "Console").
void VGui2_Chat_Print(const char *msg);
// true while the chat input line is open (messagemode / messagemode2).
bool VGui2_Chat_IsInMessageMode();
// BHL ConsolePrint: text for the console, ^0..^9 / ^xRGB / ^#RRGGBB colour codes removed unless hud_colortext is 0. Static buffer.
const char *VGui2_Chat_RemoveColorCodes(const char *text);
// SBO: hud_colortext (CHudChat::GetColorCodeAction): 0 codes shown as typed, 1 drawn as colors, 2 removed; 2
// without the VGUI2 chat.
int VGui2_Chat_GetColorCodeAction();
// SBO: player client's name color, the "namecolor" key of his userinfo (CHudChat::GetPlayerNameColor: the engine's
// player info, ParseChatColor), as the chat draws it (0..255 into rgb: ^1..^8 the scheme's ColorCodeN, a wheel color
// raised to the brightness floor). false: he has none (or there is no VGUI2 chat).
bool VGui2_Chat_GetPlayerNameColor(int client, int *rgb);
// SBO: length of the ^0..^9 / ^xRGB / ^#RRGGBB colour code at s, 0 if there is none. pbReset / rgb (either may be
// nullptr): ^0 / ^9 (and any code without the VGUI2 chat) set *pbReset, the reset to the text's own colour; any other
// code clears it and writes its colour (0..255) to rgb, the colour the chat and kill feed draw it in
// (CHudChat::GetColorCodeColor).
int VGui2_Chat_GetColorCode(const char *s, bool *pbReset, int *rgb);

//-----------------------------------------------------------------------------
// Console colors (sbo_console.cpp, docs/CONSOLE_COLOR.md)
//-----------------------------------------------------------------------------
// SboHud_InitChat: finds the GameUI console's print colors (BugfixedHL / Zombie Panic! method). If they cannot be
// found: one console warning, then no colors for the session (VGui2_Console_PrintColorCodes returns false).
void VGui2_Console_Init();
// Prints text to the console with its ^0..^9 / ^xRGB / ^#RRGGBB color codes drawn as colors, the chat's colors
// (VGui2_Chat_GetColorCode: ^1..^8 the scheme's ColorCodeN, ^xRGB / ^#RRGGBB raised to the brightness floor,
// ^0 / ^9 back to the console's own color). Returns false and prints nothing unless hud_colortext is 1 and the
// console colors were found: the caller then prints the text as before.
bool VGui2_Console_PrintColorCodes(const char *text);

// Implemented in saytext.cpp (SBO world), shared by both chats:
// true for the server's hit messages ("My fucking head", ...; exact text, trailing newlines ignored), which are shown
// but kept out of the chat history and the console.
bool SboChat_IsTransientMessage(const char *pszMsg);
// Length of the server's category tag ("*DEAD* ", "*SPEC* ", "*NOLOAD* ", "*GM* ") at psz, else 0.
int SboChat_TagLength(const char *psz);

//-----------------------------------------------------------------------------
// VGUI2 scoreboard (score_panel.cpp, BHL CScorePanel). Replaces SBO's VGUI1 ScorePanel: the
// TeamFortressViewport scoreboard entry points (Show/Hide/IsVisible, ScoreInfo/TeamInfo updates,
// DeathMsg, Initialize) forward here while VGui2_Score_IsActive().
//-----------------------------------------------------------------------------
// Creates the panel (once), registers the hud_scoreboard_* cvars, starts the Steam API context
// (avatars). Returns false if VGUI2 isn't available (then there is no scoreboard).
bool VGui2_Score_Init();
bool VGui2_Score_IsActive();
void VGui2_Score_Show();
//! Hides unless in intermission (caller checks, like TeamFortressViewport::HideScoreBoard).
void VGui2_Score_Hide();
bool VGui2_Score_IsVisible();
//! New map / viewport Initialize: hide, forget the killer highlight.
void VGui2_Score_Reset();
void VGui2_Score_UpdateOnPlayerInfo(int client);
//! "SboModeInfo" message received: refresh the round / frags left line (sbo::GetModeInfo).
void VGui2_Score_UpdateModeInfo();
void VGui2_Score_DeathMsg(int killer, int victim);
//! HUD_Key_Event: mouseButton 1 = MOUSE1, 2 = MOUSE2, 3 = wheel, 0 = any other key. Returns true when the
//! scoreboard consumed the key (activating the cursor with hud_scoreboard_mousebtn, or clicks for VGUI2).
bool VGui2_Score_MouseKey(int down, int mouseButton);
//! The player's SteamID64 as the engine has it (player_info_t::m_nSteamID), 0 if he has no individual Steam account
//! ID (bots, empty slots). Works without the scoreboard panel. Used as the voice ban list's key
//! (game_shared/voice_status.cpp CVoiceStatus::GetPlayerBanID).
unsigned long long VGui2_GetPlayerSteamID64(int client);

//-----------------------------------------------------------------------------
// Kill feed (death notices). docs/dev/KILLFEED_PORT.md.
//
// PUBLIC entry point for client code that wants a kill-feed row (e.g. NPC-kill messages parsed in sbo_bridge.cpp):
// VGui2_DeathNotice_AddEntry. Always safe to call (implemented in death.cpp, SBO world): the row goes to the VGUI2
// kill feed (hud_deathnotice.cpp, BHL CHudDeathNoticePanel) while that is active (VGUI2 up and
// hud_deathnotice_vgui 1), else to SBO's legacy CHudDeathNotice (sprite + console font). It does NOT print to the
// console and does NOT notify the scoreboard / spectator code (the caller does that if wanted). The standard
// DeathMsg is handled by death.cpp itself; don't call this for it.
//
// killerColor / victimColor (int):
//   VGUI2_KF_COLOR_CLIENT(i)  player i (1..32): SBO's team colour of that player (sbo::GetClientColor, the table
//                             chat and scoreboard use), resolved when the row is added.
//   0..31                     a team / category number -> the same table (sbo::GetTeamColor: 1 blue, 2 red,
//                             3 yellow, 4 green, 0 yellow, else grey).
//   VGUI2_KF_COLOR_NPC  (-1)  default NPC name colour (ClientScheme "KF_Name_NPC", legacy: 255 140 90).
//   VGUI2_KF_COLOR_PLAIN (-2) neutral name colour (ClientScheme "KF_Name_Plain", legacy: 216 216 216).
//   VGUI2_KF_COLOR_RGB(r,g,b) explicit colour (0..255 each).
//   VGUI2_KF_COLOR_OWNED(i)   an NPC of player i (a Game Master's monster, "<GM>'s <monster>"): the NPC colour, the
//                             player's name at the start in his colours as a VGUI2_KF_COLOR_CLIENT name (resolved
//                             when the row is added: his current name colour).
//
// killerName: nullptr or "" = no killer (world / NPC-less death / suicide): only "<icon> victim" is drawn.
// victimName: required (nullptr/"" -> the call is ignored). Names are UTF-8, shown as given except that ^0..^9 /
//   ^xRGB / ^#RRGGBB colour codes follow hud_colortext (1 drawn in colour, 2 removed, 0 shown as typed). Max 63 bytes each are kept.
//   A VGUI2_KF_COLOR_CLIENT name also gets that player's name colour (cvar namecolor; hud_colortext 1 only).
// killerIsLocal / victimIsLocal: the local player is the killer / victim -> highlighted row (BHL: KF_Background_Kill /
//   KF_Background_Death, longer display time hud_deathnotice_time_self); the victim highlight wins.
// weaponName: the weapon/icon name WITHOUT the "d_" prefix, as in DeathMsg ("9mmhandgun", "crowbar", "skull", ...).
//   Icon = HUD sprite "d_<weaponName>" from sprites/hud.txt; nullptr / "" / a name without such a sprite -> "d_skull"
//   (BHL). The weapon is never drawn as text.
// flags: VGUI2_KF_FLAG_* (0 for a normal kill).
//-----------------------------------------------------------------------------
#define VGUI2_KF_COLOR_NPC     (-1)
#define VGUI2_KF_COLOR_PLAIN   (-2)
#define VGUI2_KF_COLOR_RGB(r, g, b) ((int)(0x40000000 | (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF)))
#define VGUI2_KF_COLOR_CLIENT(i) ((int)(0x20000000 | ((i) & 0xFF)))
#define VGUI2_KF_COLOR_OWNED(i) ((int)(0x10000000 | ((i) & 0xFF)))

#define VGUI2_KF_FLAG_SUICIDE  0x01 //!< victim killed himself: killer not drawn (killerName ignored)
#define VGUI2_KF_FLAG_TEAMKILL 0x02 //!< icon in the team-kill colour (HL d_teammate green)
#define VGUI2_KF_FLAG_HEADSHOT 0x04 //!< adds the d_headshot icon after the weapon icon (CS 1.6; "HS" text without one)

void VGui2_DeathNotice_AddEntry(const char *killerName, int killerColor, bool killerIsLocal,
                                const char *victimName, int victimColor, bool victimIsLocal,
                                const char *weaponName, int flags);

//! For a message that replaces the stock DeathMsg sent right after it (same frame), e.g. SboDeathMsg with
//! SBO_KF_REPLACES_DEATHMSG: the next DeathMsg whose victim is victimIndex adds no kill-feed row and prints no
//! console line (scoreboard / spectator updates still happen). One-shot: any other DeathMsg clears it.
//! Implemented in death.cpp.
void VGui2_DeathNotice_SkipNextDeathMsg(int victimIndex);

// VGUI2 kill feed internals (hud_deathnotice.cpp, SDK world), driven by death.cpp. Other code calls
// VGui2_DeathNotice_AddEntry above.
//! HUD_Init (CHudDeathNotice::Init): creates the panel (once), registers its cvars. False without VGUI2.
bool VGui2_KillFeed_Init();
//! True while rows go to the VGUI2 kill feed (panel exists and hud_deathnotice_vgui != 0).
bool VGui2_KillFeed_IsActive();
//! InitHUDData (new map / InitHUD) and VidInit (HUD sprite list rebuilt): drop all rows, re-read the skull sprite.
void VGui2_KillFeed_Reset();
//! Same arguments as VGui2_DeathNotice_AddEntry (colours still encoded).
void VGui2_KillFeed_AddEntry(const char *killerName, int killerColor, bool killerIsLocal,
                             const char *victimName, int victimColor, bool victimIsLocal,
                             const char *weaponName, int flags);

//-----------------------------------------------------------------------------
// Voice labels (hud_voice_status.cpp; BHL CHudVoiceStatus).
// SBO's CVoiceStatus (game_shared/voice_status.cpp) stays the voice manager (speaking state, mute, server state,
// head icons); its VGUI1 speaker labels and local "talking" icon (icntlk_pl) are gone (SBO 2.0): the VGUI2 list
// shows the speakers, the local player's own row included.
//-----------------------------------------------------------------------------
//! HUD_Init (CHud::Init, after CVoiceStatus::Init): creates the speaker list (talking players, the local one
//! included, with their Steam avatars), once. Returns false without VGUI2 (then there is no speaker list).
bool VGui2_VoiceStatus_Init();
//! True once the VGUI2 voice labels exist.
bool VGui2_VoiceStatus_IsActive();

//-----------------------------------------------------------------------------
// SBO's menus (docs/dev/VGUI2_MIGRATION.md). sbo_menus.cpp (SDK world).
//
// SBO's TeamFortressViewport has the menu stack (m_pCurrentMenu, ShowVGUIMenu, HideTopMenu, KeyInput, cursor): a
// menu is put on that stack as a CViewportMenu (sbo_menu_proxy.h), whose Open()/Close() show/hide the VGUI2 panel
// through the functions below.
//-----------------------------------------------------------------------------
//! HUD_Init (SboHud_InitChat): adds ui/resource/language/sbo_ui_%language%.txt, loads the scheme, creates the
//! menu panels. Returns false without VGUI2 (an engine without client VGUI2: SBO 2.0 has no menus there).
bool VGui2_Menus_Init();

//! Text window (MOTD, map briefing, help): title and text as CreateTextWindow builds them (ANSI/UTF-8, \r\n or \n).
//! shadeFullscreen: CMessageWindowPanel's iShadeFullscreen (darken the whole screen, else only the window).
//! closeOnly: the OK button only closes the window (MENU_MAPBRIEFING), no "ShowVGUI 2" after it.
//! htmlFile: a game file shown as a web page instead of the text when HTML is on (the briefing's maps/<map>.htm),
//! or null. isMOTD: text is the MOTD (an http(s) URL or HTML in it is shown as a web page when HTML is on).
//! docs/HTML_MOTD.md.
void VGui2_TextWindow_Show(const char *title, const char *text, bool shadeFullscreen, bool closeOnly,
    const char *htmlFile, bool isMOTD);
void VGui2_TextWindow_Hide();
//! True if HTML is on (cl_disablehtmlmotd 0, Steam API available) and path is in the game's file system.
bool VGui2_HtmlFileAvailable(const char *path);

//! "GM Mode" window (text: #SBO_GM_* in sbo_ui_<language>.txt).
void VGui2_GmMenu_Show();
void VGui2_GmMenu_Hide();

//! Team menu (SBO: the map readme, maps/<map>.txt, with the Go! button that opens the briefing).
void VGui2_TeamMenu_Show();
void VGui2_TeamMenu_Hide();
//! CTeamMenuPanel::SlotInput (digit keys, "+jump" = 5 through TeamFortressViewport::KeyInput): true if handled.
bool VGui2_TeamMenu_SlotInput(int iSlot);
//! CTeamMenuPanel::Initialize (TeamFortressViewport::Initialize, every new level): reload the map's text next time.
void VGui2_TeamMenu_Initialize();

//! Briefing / equipment menu: blades, light/assault weapons, cyberware, equipment, armor, agent sex; "Go !" sends
//! the choices (blade/light/heavy/item/cyberware/armor/malefemale <n>). Texts: #SBO_Brief_*, #SBO_Item_*.
void VGui2_BriefMenu_Show();
void VGui2_BriefMenu_Hide();
//! Keys 1-7 in the new loadout menu (cl_new_ingame_menus 1); false when not used (CSboBriefMenu takes no keys)
bool VGui2_BriefMenu_SlotInput(int iSlot);

//! Spectator UI: SBO's SpectatorPanel (spectator_panel.cpp) holds the state (its callers, m_menuVisible and the
//! command menus' keys are unchanged) and the VGUI2 panel draws it. Pushed by SpectatorPanel::SyncVGui2 on changes
//! (show/hide, duck menu, inset, texts; about once a second while spectating). The strings are only read during
//! the call.
struct SboSpectatorState
{
	bool visible = false;       //!< bars shown (spectating, hud_draw, no intermission)
	bool menuVisible = false;   //!< m_menuVisible: Options, <, >, camera mode buttons shown; the bars take the mouse
	                            //!< (the cursor itself follows SBO's VGUI1 g_iVisibleMouse, set for this too)
	bool insetVisible = false;  //!< m_insetVisible: picture-in-picture frame (a button), top bar starts right of it
	int insetX = 0, insetY = 0, insetWide = 0, insetTall = 0; //!< overview inset window, 640x480 units
	bool statusVisible = false; //!< spec_drawstatus: map name / spectator count, timer, separator
	bool bannerVisible = false; //!< m_TopBanner (HLTV only)
	const char *bannerImage = ""; //!< its image ("banner" or the DRC_CMD_BANNER file)
	const char *optionsText = ""; //!< Options button (#SPECT_OPTIONS, titles.txt)
	const char *camText = "";     //!< camera mode button (#OBS_*, titles.txt)
	const char *bottomText = "";  //!< bottom label: target player's name or #Spec_ModeN (titles.txt)
	int bottomColor[4] = { 143, 143, 54, 255 }; //!< its colour (team colour in first person), alpha 255 = opaque
	const char *extraInfo = "";   //!< "<#Spec_Map>: <map>" or "<#Spectators>: <n>" (HLTV)
	const char *currentTime = ""; //!< below it, next to the timer icon
};
void VGui2_Spectator_SetState(const SboSpectatorState &state);
// SBO 2.0: a server menu is up: the spectator bars and their texts are not shown (they drew over its window)
void VGui2_Spectator_SetSuppressed(bool bSuppressed);

//-----------------------------------------------------------------------------
// CS:CZ tutor window (tutor_text_window.cpp; server: src/dlls/tutor.cpp, env_tutor.cpp; docs/TUTOR_PORT.md)
//-----------------------------------------------------------------------------
//! HUD_Init (SboHud_InitChat): loads ui/resource/tutorscheme.res, ui/resource/ui/tutortextwindow.res and
//! ui/resource/language/tutor_%language%.txt, hooks TutorText / TutorPart / TutorClose. False without VGUI2.
bool VGui2_Tutor_Init();
//! InitHUD (new map / connect): closes the window (the server's tutor starts over).
void VGui2_Tutor_Reset();

//-----------------------------------------------------------------------------
// game_timer HUD timer (hud_game_timer.cpp; server: src/dlls/game_timer.cpp; docs/GAME_TIMER.md)
//-----------------------------------------------------------------------------
//! HUD_Init (SboHud_InitChat, after VGui2_Menus_Init: SBO's scheme): creates the timer panel (layout
//! ui/resource/gametimer.res, colours and fonts ui/resource/sboscheme.res) and hooks GameTimer. False without VGUI2.
bool VGui2_GameTimer_Init();
//! InitHUD (new map / connect): forgets the timers and hides the panel.
void VGui2_GameTimer_Reset();

//-----------------------------------------------------------------------------
// game_messagebox message boxes (sbo_message_box.cpp; server: src/dlls/game_messagebox.cpp; docs/MESSAGE_BOX.md)
//-----------------------------------------------------------------------------
//! HUD_Init (SboHud_InitChat, after VGui2_Menus_Init: SBO's scheme): creates the panel (colors and font
//! ui/resource/sboscheme.res "MessageBox.*") and hooks MessageBox / MsgBoxPart. False without VGUI2.
bool VGui2_MessageBox_Init();
//! game_playvideo (video/video_player.cpp): while on, Esc does not open GameUI (HL25 opens it before HUD_Key_Event);
//! VGui2_Video_TakeEscape is true once after such a press.
void VGui2_Video_SetEscapeGuard(bool state);
bool VGui2_Video_TakeEscape();

//! InitHUD (new map / connect): no boxes (the server sends the open ones again).
void VGui2_MessageBox_Reset();
//! input.cpp IN_AttackDown (+attack): true while a box is shown (the press is used up: no attack); it closes the
//! newest box (not within 0.1 s of its arrival).
bool VGui2_MessageBox_Attack();
//! A box is shown and has the mouse (cursor on, mouse look off): +attack2 does nothing either.
bool VGui2_MessageBox_IsActive();
//! At least one box is open, shown or hidden: no weapon change (ammo.cpp slot keys, invnext / invprev, the HUD
//! weapon menu).
bool VGui2_MessageBox_HasBoxes();
//! HUD_Key_Event, key pressed while a box is shown. True: the key is used up.
enum
{
	VGUI2_MSGBOX_LINE_UP = 1,  //!< mouse wheel up
	VGUI2_MSGBOX_LINE_DOWN,    //!< mouse wheel down
	VGUI2_MSGBOX_PAGE_UP,      //!< PageUp
	VGUI2_MSGBOX_PAGE_DOWN,    //!< PageDown
	VGUI2_MSGBOX_CLOSE,        //!< Enter / keypad Enter: closes the newest box
	VGUI2_MSGBOX_MOUSE_BUTTON, //!< MOUSE1..MOUSE5: the box has the mouse (VGUI2 gets the clicks), no binding runs
};
bool VGui2_MessageBox_Key(int action);

//-----------------------------------------------------------------------------
// Game Master mode spawn window (sbo_gm_spawn_menu.cpp; server: src/dlls/gmmode.cpp; docs/GM_MODE.md)
//-----------------------------------------------------------------------------
//! HUD_Init (SboHud_InitChat, after VGui2_Menus_Init: SBO's scheme): creates the window, the intro's black screen and
//! the placement line, hooks GmMode and adds the client commands gmon, gmmenu, gm_menu, gm_spawnmenu. False without
//! VGUI2.
bool VGui2_GmSpawn_Init();
//! InitHUD (new map / connect): no Game Master state (the server sends it again).
void VGui2_GmSpawn_Reset();
//! The window is open (cursor on, keyboard in the search box): no +attack / +attack2, the mouse buttons are VGUI2's.
bool VGui2_GmSpawn_IsOpen();
//! IN_AttackDown: a flying Game Master without a placement or window starts the selection box (the mouse draws it;
//! the engine doesn't get the button). True if taken.
bool VGui2_GmAttack();
//! The selection box is being drawn
bool VGui2_GmBoxSelecting();
//! The flying Game Master's HUD is up: no spectator bar ("Ghost Mode 3") or spectator mode texts under it
bool VGui2_GmHudOwnsScreen();
//! SBO 2.0 GM mode: the players window's View (view.cpp): the player its picture follows in chase locked and the
//! picture's rectangle in screen pixels; false: no picture
bool VGui2_GmPip(int &target, int &x, int &y, int &w, int &h);
//! The top of the kill feed while the Game Master's nearest players panel is up (pixels), 0 otherwise
int VGui2_GmKillFeedTop();
//! The first free line under the Game Master's status strip while it is shown (pixels), 0 otherwise: top-center HUD
//! elements (the radio bar, the game timer) go below it
int VGui2_GmStripBottom();

//! HUD text menu (sbo_hud_menu.cpp): CHudMenu keeps the ShowMenu logic and pushes its state here; the VGUI2 panel
//! draws it.
//! Open: the complete menu string (localised, key names swapped in, \w \d \y \r \R colour codes).
void VGui2_HudMenu_Open(const char *menuString);
//! The menu went away without a choice (timeout, server, new map): hidden at once.
void VGui2_HudMenu_Close();
//! Item 1..10 was chosen (menuselect sent): flashed briefly, then the menu fades out.
void VGui2_HudMenu_Select(int item);

// SBO 2.0 server menus (sbo_server_menu.cpp): the keys the window takes while it is up (TeamFortressViewport::KeyInput): the rest stay the player's
enum ServerMenuKey
{
	SMKEY_ESCAPE = 1, //!< a dropdown, the confirm dialog or the item popup first (else not taken: the game menu)
	SMKEY_ENTER,      //!< picks the highlighted item, presses the focused control, Yes in the dialog
	SMKEY_SPACE,      //!< flips the focused switch, opens the focused dropdown (as Enter elsewhere)
	SMKEY_TAB,        //!< the next item or control
	SMKEY_BACKTAB,    //!< Shift+Tab: the one before
	SMKEY_UP,
	SMKEY_DOWN,
	SMKEY_LEFT,
	SMKEY_RIGHT,
};
//! A key pressed while the window is up; true when the window used it
bool VGui2_ServerMenu_KeyInput(int key);
//! The server menu window is up (with its popup or dialog): the mouse wheel and the keys are the menu's, no weapon
//! change (ammo.cpp)
bool VGui2_ServerMenu_IsOpen();

#endif // SBO_VGUI2_API_H
