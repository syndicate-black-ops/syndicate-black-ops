//========= CS:CZ tutor: VGUI2 tutor window =========//
//
// Purpose: The client side of Counter-Strike: Condition Zero's tutor window, shown by the server's env_tutor
//          entities (src/dlls/tutor.cpp, env_tutor.cpp): the TutorText / TutorPart / TutorClose messages.
//          docs/TUTOR_PORT.md.
//
// Reference: Valve's CS/CZ VGUI2 game controls, built into the Half-Life SDK's lib/public/game_controls.lib
// (halflife repository): TutorWindowManager.cpp, TutorTextWindow.cpp (CTutorTextWindow / CTutorMessageBox),
// vgui_TeamFortressViewport_msgs.cpp (MsgFunc_TutorText / TutorClose). No source exists; this is a reimplementation
// of their behaviour, read from the disassembly:
//   * The window: a 200x200 frame laid out by resource/ui/tutortextwindow.res -- icon, the text label, optional
//     rounded corners (gfx/career/round_corner_*) -- that then grows or shrinks by the text's size. Colours from
//     resource/tutorscheme.res: text and icon in "TutorTextClass<type>" (fallback "TutorTextClassDefault"),
//     background "WindowBG", outline "BorderBright" (square window only). Top right of the screen, 10 pixels from
//     the edges (1/8 of the screen down for a dead player, below the spectator bar). No mouse/keyboard input.
//   * game_controls.lib never enables the rounded corners (TutorWindowManager passes 0) and has no close button:
//     the server closes the window (TutorClose).
//
// SBO changes: the message carries everything env_tutor sets (icon, sound, colours, alignment and position, see
// TutorWindow_MsgFunc_TutorText); "%n" is the local player's name and "%+command%" the key bound to the command;
// files under ui/ (VGUI2_ROOT_DIR); the layout is scaled with the scheme font (Tahoma 14 = CZ's pixel sizes,
// bigger fonts at higher resolutions, tutorscheme.res); lines longer than twice the label's width are wrapped (CZ
// only breaks at the "\n" of its texts); the sound is played here, for this player only.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef TUTOR_TEXT_WINDOW_H
#define TUTOR_TEXT_WINDOW_H

#include <wchar.h>

// TutorMessageType of the server (src/dlls/tutor.h), the TutorText message's type short
enum
{
	TUTOR_TYPE_DEFAULT = (1 << 0),
	TUTOR_TYPE_FRIEND_DEATH = (1 << 1),
	TUTOR_TYPE_ENEMY_DEATH = (1 << 2),
	TUTOR_TYPE_SCENARIO = (1 << 3),
	TUTOR_TYPE_BUY = (1 << 4),
	TUTOR_TYPE_CAREER = (1 << 5),
	TUTOR_TYPE_HINT = (1 << 6),
	TUTOR_TYPE_INGAME_HINT = (1 << 7),
	TUTOR_TYPE_END_GAME = (1 << 8),
};

// TutorIcon of the server: TutorText's icon byte
enum
{
	TUTOR_ICON_NONE = 0,
	TUTOR_ICON_I = 1,
	TUTOR_ICON_EXCLAMATION = 2,
	TUTOR_ICON_SKULLS = 3,
	TUTOR_ICON_I_BIGGER = 4,
	TUTOR_ICON_EXCLAMATION_BIGGER = 5,
	TUTOR_ICON_CUSTOM = 255,
};

// TUTOR_FLAG_* of the server: TutorText's flags byte
enum
{
	TUTOR_FLAG_DEAD = (1 << 0),
	TUTOR_FLAG_TEXT_COLOR = (1 << 1),
	TUTOR_FLAG_ICON_COLOR = (1 << 2),
	TUTOR_FLAG_BG_COLOR = (1 << 3),
	TUTOR_FLAG_BORDER_COLOR = (1 << 4),
};

// TutorAlign of the server: TutorText's alignment byte
enum
{
	TUTOR_ALIGN_TOP_RIGHT = 0,
	TUTOR_ALIGN_TOP_LEFT,
	TUTOR_ALIGN_TOP_CENTER,
	TUTOR_ALIGN_CENTER_LEFT,
	TUTOR_ALIGN_CENTER,
	TUTOR_ALIGN_CENTER_RIGHT,
	TUTOR_ALIGN_BOTTOM_LEFT,
	TUTOR_ALIGN_BOTTOM_CENTER,
	TUTOR_ALIGN_BOTTOM_RIGHT,
	TUTOR_ALIGN_COUNT
};

//! Everything a tutor window shows (TutorWindow_MsgFunc_TutorText fills it from the message).
struct TutorWindowParams
{
	int id = -1;
	int type = TUTOR_TYPE_DEFAULT; //!< text colour "TutorTextClass<type>" unless textColor is set
	bool isDead = false;           //!< dead / spectating: the window keeps clear of the spectator bars
	char icon[128] = "";           //!< TGA without extension, e.g. VGUI2_ROOT_DIR "gfx/career/icon_i"; "" = no icon
	bool tintIcon = true;          //!< without iconColor: icon in the text colour (CZ), else in its own colours
	bool roundedCorners = false;   //!< CZ's round_corner_* frame (black corners, no outline) instead of the outline
	int align = TUTOR_ALIGN_TOP_RIGHT;
	float x = -1.0f; //!< 0..1: horizontal position on the screen (of the edge/centre the alignment names); -1: margin
	float y = -1.0f; //!< 0..1: vertical position, the same way
	bool hasTextColor = false, hasIconColor = false, hasBgColor = false, hasBorderColor = false;
	unsigned char textColor[4] = { 255, 255, 255, 255 };
	unsigned char iconColor[4] = { 255, 255, 255, 255 };
	unsigned char bgColor[4] = { 0, 0, 0, 200 };
	unsigned char borderColor[4] = { 255, 255, 255, 255 };
};

//! The preset icon's file (TUTOR_ICON_*) and whether it is tinted by default; false for none / unknown.
bool TutorWindow_PresetIcon(int icon, TutorWindowParams &params);

//! Replaces the open window (CZ: TutorWindowManager::NewTutorWindow). text: final text ("\n" = new line).
void TutorWindow_Show(const wchar_t *text, const TutorWindowParams &params);
//! TutorClose (CZ: TutorWindowManager::CloseTutorTextWindow).
void TutorWindow_Close();
bool TutorWindow_IsOpen();

//! The TutorText / TutorPart messages (also used to inject one).
int TutorWindow_MsgFunc_TutorText(const char *pszName, int iSize, void *pbuf);
int TutorWindow_MsgFunc_TutorPart(const char *pszName, int iSize, void *pbuf);

#endif // TUTOR_TEXT_WINDOW_H
