//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: VGUI2 scoreboard (see score_panel.h). Ported from BugfixedHL-Rebased
//          src/game/client/vgui/score_panel.cpp.
//
// Changes vs. BHL:
//   * data from SBO through sbo_bridge.h (BHL: CPlayerInfo/CTeamInfo); SteamID from the engine's
//     player_info_t on SteamPipe engines only (BHL also parses `status` output on old engines)
//   * SBO category grouping/order (IsSboCategoryMode) on top of BHL's team grouping
//   * section and name colours: sbo::GetTeamColor / sbo::GetClientColor (the chat's table)
//   * player names: colour codes follow the chat's hud_colortext like BHL (1 = drawn in colour, ^0/^9 = the row's
//     category colour; 2 = removed; 0 = shown as typed); menu titles / mute messages always without; no real names.
//     The name column is an image column showing one TextImage per player (CPlayerNameImage): the SDK's
//     COLUMN_COLORED text (BHL) only knows the ten table colours of ^0..^9, SBO's ^xRGB / ^#RRGGBB codes need any colour
//   * cvars are archived engine cvars (CSboCvar); hud_scoreboard_showeff defaults to 0 (SBO coop sends levels)
//   * own language file ui/resource/language/scoreboard_<language>.txt (column titles included)
//   * a mute button at the right end of each row (CMuteButtonImage, column "mute"): left-click toggles the voice mute (BHL: the
//     right-click menu only; SBO's VGUI1 ScorePanel muted on a click on the row)
//   * badges after some players' names (BADGES, by SteamID64: the developer icon, animated ones from a frame sheet;
//     ui/gfx TGAs: client files, not precached), with a tooltip and a link the click opens (PostChildPaint; Aura-CL's
//     developer emblem does the same with its own column)
//=================================================================//

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <wchar.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/IImage.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/SectionedListPanel.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/Menu.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "client_steam_context.h"
#include "hud_chat.h"
#include "score_panel.h"
#include "avatar_image.h"
#include "tga_image.h"
#include "sbo_bridge.h"
#include "sbo_cvar.h"
#include "vgui2_api.h"
#include "hud_objectives.h"

#define STEAM_PROFILE_URL "http://steamcommunity.com/profiles/"

namespace
{
CSboCvar hud_scoreboard_mousebtn("hud_scoreboard_mousebtn", "1");     // 0 off, 1 MOUSE1, 2 MOUSE2 enables the cursor
CSboCvar hud_scoreboard_showavatars("hud_scoreboard_showavatars", "1");
CSboCvar hud_scoreboard_showloss("hud_scoreboard_showloss", "1");
CSboCvar hud_scoreboard_efftype("hud_scoreboard_efftype", "0");
CSboCvar hud_scoreboard_effpercent("hud_scoreboard_effpercent", "0");
CSboCvar hud_scoreboard_showsteamid("hud_scoreboard_showsteamid", "1");
CSboCvar hud_scoreboard_showeff("hud_scoreboard_showeff", "0");         // BHL: 1
CSboCvar hud_scoreboard_size("hud_scoreboard_size", "0");
CSboCvar hud_scoreboard_spacing_normal("hud_scoreboard_spacing_normal", "0");
CSboCvar hud_scoreboard_spacing_compact("hud_scoreboard_spacing_compact", "0");

// SBO category mode: section order (TeamNames team numbers): Game Master, Alive Ops, Dead Ops, No Loadout, Spectators
// (Bounty Killer maps: Alive with the Hunted in it, Dead Ops, Spectators: GetPlayerTeam keeps section 1 empty there,
// the server section 3). Fixed: never by score or player count.
constexpr int s_SboCategoryOrder[] = { 1, 4, 2, 3, 5 };

int GetScreenHeight()
{
	int w, h;
	vgui2::surface()->GetScreenSize(w, h);
	return h;
}

double GetAbsoluteTime()
{
	return vgui2::system()->GetCurrentTime();
}

Color TeamColor(int team)
{
	int r, g, b;
	sbo::GetTeamColor(team, r, g, b);
	return Color(r, g, b, 255);
}

Color ClientColor(int client)
{
	int r, g, b;
	sbo::GetClientColor(client, r, g, b);
	return Color(r, g, b, 255);
}

int s_iMutedIconTexture = -1;
int s_iUnmutedIconTexture = -1;

// Index of the name column in every section (CScorePanel::CreateSection: avatar, name, ...). The mute button column
// is the last one (CScorePanel::m_iMuteColumn).
constexpr int NAME_COLUMN = 1;

// Frames after the cursor was turned on in which a click on a mute button is ignored (CScorePanel::EnableMousePointer)
constexpr int MUTE_CLICK_IGNORE_THINKS = 3;

// SBO: scoreboard badges, one row per player: a picture after the name of the player with that SteamID64 (SteamPipe
// engines only, like the avatars), a tooltip when the cursor is on it, and a link the click opens (Steam overlay, else
// the system browser; nullptr: no link). Pictures: ui/gfx/<image><size>.tga for every BADGE_SIZES size; frames > 1:
// each TGA is a sheet of the frames in order, BADGE_SHEET_COLUMNS per row, played at frameMs per frame
// (scripts/gen_scoreboard_badges.py prints both numbers). Client files, not precached.
struct ScoreBadge
{
	uint64 steamID64;
	const char *image;
	int frames;
	int frameMs;
	const wchar_t *tooltip;
	const char *url;
};

const ScoreBadge BADGES[] = {
	// MrCalvin, the developer: STEAM_0:1:1039656 (76561197960265728 + 2 * 1039656 + 1); scripts/gen_dev_icon.py
	{ 76561197962345041ull, "ui/gfx/dev_icon", 1, 0, L"It's MrCalvin", nullptr },
	// Boxyt: STEAM_0:1:54040906 (76561197960265728 + 2 * 54040906 + 1); scripts/gen_scoreboard_badges.py
	{ 76561198068347541ull, "ui/gfx/badge_boxyt", 15, 80, L"Community Organizer", "https://linktr.ee/boxyt" },
};
constexpr int NUM_BADGES = sizeof(BADGES) / sizeof(BADGES[0]);
constexpr int BADGE_SIZES[] = { 16, 24, 32, 64 };
constexpr int NUM_BADGE_SIZES = sizeof(BADGE_SIZES) / sizeof(BADGE_SIZES[0]);
constexpr int BADGE_SHEET_COLUMNS = 4;

// A badge's texture for one of BADGE_SIZES, loaded the first time it is drawn; -1: none
int BadgeTexture(int badge, int sizeIndex)
{
	static int s_Textures[NUM_BADGES][NUM_BADGE_SIZES];
	static bool s_bLoaded[NUM_BADGES][NUM_BADGE_SIZES];
	if (!s_bLoaded[badge][sizeIndex])
	{
		s_bLoaded[badge][sizeIndex] = true;
		char path[128];
		V_snprintf(path, sizeof(path), "%s%d", BADGES[badge].image, BADGE_SIZES[sizeIndex]);
		s_Textures[badge][sizeIndex] = vgui2::surface()->CreateNewTextureID();
		// filtered: at its own size a picture is drawn pixel for pixel anyway; a capped badge (BadgeDrawSize) is a
		// larger picture drawn smaller
		vgui2::surface()->DrawSetTextureFile(s_Textures[badge][sizeIndex], path, true, false);
	}
	return s_Textures[badge][sizeIndex];
}

// The size a badge is drawn at for a slot (CScorePanel::GetBadgeSlot) and its texture (index into BADGE_SIZES): the
// largest that fits, at its own size; below 16 the 16 one scaled down, above 64 the 64 one scaled up. A slot larger
// than the cap (CScorePanel::GetBadgeCap; 0: none) draws the badge at the cap, from the smallest picture at least that
// large, scaled down.
int BadgeDrawSize(int slot, int cap, int &sizeIndex)
{
	if (cap > 0 && slot > cap)
	{
		sizeIndex = NUM_BADGE_SIZES - 1;
		for (int i = 0; i < NUM_BADGE_SIZES; i++)
		{
			if (BADGE_SIZES[i] >= cap)
			{
				sizeIndex = i;
				break;
			}
		}
		return cap;
	}

	for (int i = NUM_BADGE_SIZES - 1; i >= 0; i--)
	{
		if (BADGE_SIZES[i] <= slot)
		{
			sizeIndex = i;
			return slot > BADGE_SIZES[NUM_BADGE_SIZES - 1] ? slot : BADGE_SIZES[i];
		}
	}
	sizeIndex = 0;
	return std::max(slot, 1);
}

// Space between the name and the badge
int BadgeGap(int size)
{
	return std::max(2, size / 4);
}

#ifdef SBO_TEST_COMMANDS
// test_cl_devicon <client> [score] (test builds only): shows the developer's badge (BADGES[0]) on that client's row (the
// client under test is not the developer); "score": after the score instead of the name (to compare the placements). 0: off.
int s_iTestDevClient = 0;
bool s_bTestDevIconAfterScore = false;

void TestDevIcon_f()
{
	s_iTestDevClient = sbo::Cmd_Argc() > 1 ? atoi(sbo::Cmd_Argv(1)) : 0;
	s_bTestDevIconAfterScore = sbo::Cmd_Argc() > 2 && !Q_stricmp(sbo::Cmd_Argv(2), "score");
	if (CScorePanel::Get())
		CScorePanel::Get()->UpdateOnPlayerInfo(0);
}
#endif

//-----------------------------------------------------------------------------
// SBO: a player's name cell. BHL uses a COLUMN_COLORED text column, but the SDK's TextImage::SetColorCodedText only
// knows ^0..^9 with a table of ten colours. "^xRGB" / "^#RRGGBB" codes take any colour, so the name column is an image column
// (like the avatars) and the image is the same TextImage the list would have made, with the colour changes added
// here: same font, size, truncation ("...") and drawing code.
//-----------------------------------------------------------------------------
class CPlayerNameImage : public vgui2::TextImage
{
public:
	CPlayerNameImage()
	    : vgui2::TextImage("")
	{
	}

	//! text: UTF-8; bColorCodes: parse ^0..^9 / ^xRGB / ^#RRGGBB (hud_colortext 1), else the text is shown as given
	void SetName(const char *text, bool bColorCodes)
	{
		wchar_t in[160];
		wchar_t out[160];
		g_pVGuiLocalize->ConvertANSIToUnicode(text, in, sizeof(in));

		ClearColorChangeStream();
		CHudChat *pChat = bColorCodes ? CHudChat::Get() : nullptr;

		// TextImage::Paint takes one colour change per character: of several codes in a row the last one counts
		bool bPending = false, bPendingReset = false;
		Color pending;
		int n = 0;
		for (const wchar_t *p = in; *p;)
		{
			const int codeLen = pChat ? GetColorCodeLength(p) : 0;
			if (codeLen > 0)
			{
				bPending = true;
				bPendingReset = !pChat->GetColorCodeColor(p, pending); // ^0 / ^9: the row's colour (SetColor)
				p += codeLen;
				continue;
			}

			if (bPending)
			{
				if (bPendingReset)
					AddColorChangeReset(n);
				else
					AddColorChange(pending, n);
				bPending = false;
			}
			out[n++] = *p++;
		}
		out[n] = L'\0';

		SetText(out, false);
	}
};

//-----------------------------------------------------------------------------
// SBO: a player's mute button (column "mute", the last one): the unmuted icon, faint until the cursor is over it,
// or the muted icon (the same artwork: ui/gfx/unmuted_icon32 / muted_icon32, drawn the same way at the same size).
// Nothing for players who can't be muted (the local player, players without a ban list key of their own).
//-----------------------------------------------------------------------------
class CMuteButtonImage : public vgui2::IImage
{
public:
	enum class State
	{
		None,
		Unmuted,
		Muted,
	};

	void SetState(State state)
	{
		m_State = state;
	}

	void SetHovered(bool state)
	{
		m_bHovered = state;
	}

	virtual void Paint() override
	{
		if (m_State == State::None)
			return;

		const bool bMuted = m_State == State::Muted;
		const int texture = bMuted ? s_iMutedIconTexture : s_iUnmutedIconTexture;
		if (texture == -1)
			return;

		const int alpha = m_bHovered ? 255 : (bMuted ? 200 : 64);
		vgui2::surface()->DrawSetTexture(texture);
		vgui2::surface()->DrawSetColor(Color(255, 255, 255, alpha));
		vgui2::surface()->DrawTexturedRect(m_iX, m_iY, m_iX + m_iWide, m_iY + m_iTall);
	}

	virtual void SetPos(int x, int y) override
	{
		m_iX = x;
		m_iY = y;
	}

	virtual void GetContentSize(int &wide, int &tall) override
	{
		wide = m_iWide;
		tall = m_iTall;
	}

	virtual void GetSize(int &wide, int &tall) override
	{
		GetContentSize(wide, tall);
	}

	virtual void SetSize(int wide, int tall) override
	{
		m_iWide = wide;
		m_iTall = tall;
	}

	virtual void SetColor(Color col) override
	{
	}

private:
	int m_iX = 0, m_iY = 0;
	int m_iWide = 0, m_iTall = 0;
	State m_State = State::None;
	bool m_bHovered = false;
};

class CPlayerImage : public vgui2::IImage
{
public:
	//! SBO: an image from a file (userinfo "sbo_avatar") drawn instead of the Steam avatar; nullptr: none
	void SetFileImage(CTGAImage *pImage)
	{
		m_pFileImage = pImage;
	}

	void SetAvatar(CAvatarImage *pAvatar)
	{
		m_pAvatar = pAvatar;

		if (m_pAvatar)
		{
			m_pAvatar->SetPos(m_iX, m_iY);
			m_pAvatar->SetOffset(m_iOffX, m_iOffY);
			m_pAvatar->SetSize(m_iWide, m_iTall);
			m_pAvatar->SetColor(m_DrawColor);
		}
	}

	void SetMuted(bool state)
	{
		m_bIsMuted = state;
	}

	// Call to Paint the image
	// Image will draw within the current panel context at the specified position
	virtual void Paint() override
	{
		if (m_pFileImage)
		{
			m_pFileImage->SetPos(m_iX + m_iOffX, m_iY + m_iOffY);
			m_pFileImage->SetSize(m_iWide, m_iTall);
			m_pFileImage->SetColor(m_DrawColor);
			m_pFileImage->Paint();
		}
		else if (m_pAvatar)
			m_pAvatar->Paint();

		if (m_bIsMuted && s_iMutedIconTexture != -1)
		{
			vgui2::surface()->DrawSetTexture(s_iMutedIconTexture);
			vgui2::surface()->DrawSetColor(m_DrawColor);
			vgui2::surface()->DrawTexturedRect(m_iX + m_iOffX, m_iY + m_iOffY,
			    m_iX + m_iOffX + m_iWide, m_iY + m_iOffY + m_iTall);
		}
	}

	// Set the position of the image
	virtual void SetPos(int x, int y) override
	{
		m_iX = x;
		m_iY = y;
		SetAvatar(m_pAvatar);
	}

	virtual void SetOffset(int x, int y)
	{
		m_iOffX = x;
		m_iOffY = y;
		SetAvatar(m_pAvatar);
	}

	// Gets the size of the content
	virtual void GetContentSize(int &wide, int &tall) override
	{
		wide = m_iWide;
		tall = m_iTall;
	}

	// Get the size the image will actually draw in (usually defaults to the content size)
	virtual void GetSize(int &wide, int &tall) override
	{
		GetContentSize(wide, tall);
	}

	// Sets the size of the image
	virtual void SetSize(int wide, int tall) override
	{
		m_iWide = wide;
		m_iTall = tall;
		SetAvatar(m_pAvatar);
	}

	// Set the draw color
	virtual void SetColor(Color col) override
	{
		m_DrawColor = col;
		SetAvatar(m_pAvatar);
	}

private:
	int m_iX = 0, m_iY = 0;
	int m_iOffX = 0, m_iOffY = 0;
	int m_iWide = 0, m_iTall = 0;
	Color m_DrawColor = Color(255, 255, 255, 255);
	CAvatarImage *m_pAvatar = nullptr;
	CTGAImage *m_pFileImage = nullptr;
	bool m_bIsMuted = false;
};

}

CScorePanel *CScorePanel::m_sInstance = nullptr;

CScorePanel::CScorePanel(vgui2::Panel *pParent)
    : BaseClass(nullptr, "ScorePanel")
{
	Assert(!m_sInstance);
	m_sInstance = this;

	SetParent(pParent);
	SetTitle("", true);
	SetCloseButtonVisible(false);
	SetMoveable(false);
	SetSizeable(false);
	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// BHL: SetScheme("ClientScheme"), loaded by its viewport. SBO's viewport uses ChatScheme, so load it here.
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/clientscheme.res", "ClientScheme"));

	// Header labels
	m_pServerNameLabel = new vgui2::Label(this, "ServerName", "A Half-Life Server");
	m_pMapNameLabel = new vgui2::Label(this, "MapName", "Map: crossfire");
	m_pPlayerCountLabel = new vgui2::Label(this, "PlayerCount", "Players: 2 / 32");
	m_pModeInfoLabel = new vgui2::Label(this, "ModeInfo", "");

	// Player list
	m_pPlayerList = new vgui2::SectionedListPanel(this, "PlayerList");
	m_pPlayerList->SetMouseInputEnabled(true);
	m_pPlayerList->SetVerticalScrollbar(false);

	wchar_t *specTag = g_pVGuiLocalize->Find("#BHL_Scores_PlayerSpec");
	if (specTag)
	{
		Q_WStringToUTF8(specTag, m_szSpectatorTag, sizeof(m_szSpectatorTag));
	}

	wchar_t *huntedTag = g_pVGuiLocalize->Find("#SBO_Scores_PlayerHunted");
	if (huntedTag)
	{
		Q_WStringToUTF8(huntedTag, m_szHuntedTag, sizeof(m_szHuntedTag));
	}

	// Image list
	m_pImageList = new vgui2::ImageList(true);
	m_pPlayerList->SetImageList(m_pImageList, true);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerImage *pImg = new CPlayerImage();
		pImg->SetSize(32, 32);
		m_pImageList->SetImageAtIndex(i, pImg);
	}

	// SBO: the name cells (CPlayerNameImage), after the avatars: index MAX_PLAYERS + client
	for (int i = 1; i <= MAX_PLAYERS; i++)
		m_pImageList->SetImageAtIndex(MAX_PLAYERS + i, new CPlayerNameImage());

	// SBO: the mute buttons (CMuteButtonImage): index 2 * MAX_PLAYERS + client
	for (int i = 1; i <= MAX_PLAYERS; i++)
		m_pImageList->SetImageAtIndex(2 * MAX_PLAYERS + i, new CMuteButtonImage());

	m_iMutedIconTexture = -1;
	m_iUnmutedIconTexture = -1;

	// SBO: the developer icon is drawn on top of the rows
	SetPostChildPaintEnabled(true);

#ifdef SBO_TEST_COMMANDS
	sbo::AddCommand("test_cl_devicon", TestDevIcon_f);
#endif

	CreatePlayerMenu();

	LoadControlSettings(VGUI2_ROOT_DIR "resource/scorepanel.res");
	SetVisible(false);
}

void CScorePanel::UpdateServerName()
{
	wchar_t wbuf[64];
	g_pVGuiLocalize->ConvertANSIToUnicode(sbo::GetServerName(), wbuf, sizeof(wbuf));
	m_pServerNameLabel->SetText(wbuf);
}

void CScorePanel::EnableMousePointer(bool bEnable)
{
	if (bEnable && !IsVisible())
		return;

	SetMouseInputEnabled(bEnable);
	SetKeyBoardInputEnabled(false);
	vgui2::surface()->CalculateMouseVisible();

	// SBO: the click that turns the cursor on must not press a mute button (the cursor is put on the board below)
	if (bEnable)
		m_iIgnoreMuteClickThinks = MUTE_CLICK_IGNORE_THINKS;

	if (!bEnable)
		return;

	// SBO: the cursor appears in the middle of the game window (the crosshair, and the board is centred on it).
	// IInput::SetCursorPos takes a position in the game window. BHL passes gEngfuncs.GetWindowCenterX/Y, the window
	// centre on the desktop (what the client's own Win32 SetCursorPos needs): right only for a window at the
	// desktop's origin, anywhere else (windowed, a second monitor) the cursor went off by the window's position.
	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	sbo::ReleaseMouseForCursor();
	vgui2::input()->SetCursorPos(wide / 2, tall / 2);
}

void CScorePanel::UpdateOnPlayerInfo(int client)
{
	if (!IsVisible())
		return;

	if (client <= 0)
	{
		// TeamNames: labels and (maybe) the category mode changed
		RefreshItems();
		return;
	}

	if (client > MAX_PLAYERS)
		return;

	if (ShouldShowXP() != m_bShowXP)
	{
		// first SboXP message: add the XP column
		RefreshItems();
		return;
	}

	RestoreSize();
	UpdatePlayer(client);
	UpdateClientInfo(client);
	UpdateScoresAndCounts();
	Resize();
}

void CScorePanel::DeathMsg(int killer, int victim)
{
	int localPlayer = sbo::GetLocalPlayerIndex();
	if (localPlayer <= 0)
	{
		// Not yet connected
		return;
	}

	if (victim == localPlayer)
	{
		// if we were the one killed, set the scoreboard to indicate killer
		m_flKillerHighlightStart = sbo::GetHudTime();
		m_iKillerIndex = (killer >= 1 && killer <= MAX_PLAYERS) ? killer : 0;
	}
}

void CScorePanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pPlayerList->SetBorder(pScheme->GetBorder("FrameBorder"));
	m_pPlayerList->SetPaintBackgroundType(0);

	m_ThisPlayerBgColor = pScheme->GetColor("ThisPlayerBgColor", Color(0, 0, 0, 0));
	m_KillerBgColor = pScheme->GetColor("KillerBgColor", Color(0, 0, 0, 0));

	s_iMutedIconTexture = m_iMutedIconTexture;
	s_iUnmutedIconTexture = m_iUnmutedIconTexture;
}

void CScorePanel::OnThink()
{
	if (m_iKillerIndex != 0 && m_PlayerData[m_iKillerIndex].nItemID != -1)
	{
		// Update bg color
		UpdateClientInfo(m_iKillerIndex);
	}

	if (m_flLastUpdateTime + UPDATE_PERIOD <= GetAbsoluteTime())
	{
		UpdateAllClients();
	}

	// SBO: mute buttons
	if (m_iIgnoreMuteClickThinks > 0)
		m_iIgnoreMuteClickThinks--;

	SetMuteHoverClient(IsMouseInputEnabled() && !m_pPlayerMenu->IsVisible() ? GetMuteButtonClientAtCursor() : 0);

	// SBO: the badge under the cursor: its tooltip (PostChildPaint) and its link (OnItemLeftClick)
	int badgeHover = 0;
	if (IsMouseInputEnabled() && !m_pPlayerMenu->IsVisible())
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		for (int i = 1; i <= MAX_PLAYERS && badgeHover == 0; i++)
		{
			int x, y, wide, tall;
			if (GetBadge(i) >= 0 && GetBadgeBounds(i, x, y, wide, tall) && mx >= x && mx < x + wide && my >= y && my < y + tall)
				badgeHover = i;
		}
	}
	m_iBadgeHoverClient = badgeHover;

	// SBO 2.0 trigger_objective: the list under the players changed: the board's height again
	if (m_iObjectivesRevision != Objectives_Revision())
		Resize();
}

bool CScorePanel::GetObjectivesLineBounds(int &x, int &y, int &wide, int &tall)
{
	tall = Objectives_SummaryTall();
	if (tall <= 0)
		return false;
	int lx, ly, lw, lt;
	m_pPlayerList->GetPos(lx, ly);
	m_pPlayerList->GetSize(lw, lt);
	x = lx;
	y = ly + lt + m_iBottomPadding;
	wide = lw;
	return true;
}

void CScorePanel::SetObjectivesOpen(bool bOpen)
{
	m_bObjectivesOpen = bOpen;
	// the open list covers the players: their rows take no clicks meanwhile (a click there closes the list). Only
	// the open list turns the rows' mouse off: the board's own mouse decides whether the cursor is on at all (this
	// also runs when the board closes, with its mouse already off, and must not leave the rows dead for the next open)
	m_pPlayerList->SetMouseInputEnabled(!bOpen);
	if (bOpen)
		m_pPlayerMenu->SetVisible(false);
	Repaint();
}

void CScorePanel::OnMousePressed(vgui2::MouseCode code)
{
	int x, y, wide, tall;
	if (code == vgui2::MOUSE_LEFT && GetObjectivesLineBounds(x, y, wide, tall))
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		const bool onLine = mx >= x && mx < x + wide && my >= y && my < y + tall;
		const bool onList = m_bObjectivesOpen && mx >= x && mx < x + wide && my >= m_iObjectivesTop && my < y;
		if (onLine || onList)
		{
			SetObjectivesOpen(!m_bObjectivesOpen);
			return;
		}
	}
	BaseClass::OnMousePressed(code);
}

// SBO 2.0 trigger_objective: the height the objective line takes under the player list, its gap included (0: none)
int CScorePanel::ObjectivesTall()
{
	const int lineTall = Objectives_SummaryTall();
	return lineTall > 0 ? lineTall + m_iBottomPadding : 0;
}

void CScorePanel::PostChildPaint()
{
	BaseClass::PostChildPaint();

	// SBO 2.0 trigger_objective: one line under the player list: done / total, the newest active objective, how many
	// more (docs/OBJECTIVES.md). With the mouse on, a click on it opens the full list upwards over the players.
	{
		int x, y, wide, tall;
		if (GetObjectivesLineBounds(x, y, wide, tall))
		{
			Objectives_PaintSummary(x, y, wide, IsMouseInputEnabled() ? (m_bObjectivesOpen ? 2 : 1) : 0);
			if (m_bObjectivesOpen)
			{
				int lx, ly;
				m_pPlayerList->GetPos(lx, ly);
				m_iObjectivesTop = Objectives_PaintFullListUp(x, y - 2, wide, y - 2 - ly);
			}
		}
	}

	const double now = vgui2::system()->GetCurrentTime();
	int sizeIndex;
	BadgeDrawSize(GetBadgeSlot(), GetBadgeCap(), sizeIndex);
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const int badge = GetBadge(i);
		int x, y, wide, tall;
		if (badge < 0 || !GetBadgeBounds(i, x, y, wide, tall))
			continue;
		const int texture = BadgeTexture(badge, sizeIndex);
		if (texture == -1)
			continue;

		vgui2::surface()->DrawSetTexture(texture);
		vgui2::surface()->DrawSetColor(Color(255, 255, 255, 255));
		const ScoreBadge &b = BADGES[badge];
		if (b.frames <= 1)
		{
			vgui2::surface()->DrawTexturedRect(x, y, x + wide, y + tall);
			continue;
		}

		// the frame for this moment, its cell in the sheet
		const int frame = (int)((long long)(now * 1000.0 / std::max(b.frameMs, 1)) % b.frames);
		const int rows = (b.frames + BADGE_SHEET_COLUMNS - 1) / BADGE_SHEET_COLUMNS;
		const float s0 = (float)(frame % BADGE_SHEET_COLUMNS) / BADGE_SHEET_COLUMNS, s1 = s0 + 1.0f / BADGE_SHEET_COLUMNS;
		const float t0 = (float)(frame / BADGE_SHEET_COLUMNS) / rows, t1 = t0 + 1.0f / rows;
		vgui2::VGuiVertex v[4] = { vgui2::VGuiVertex(x, y, s0, t0), vgui2::VGuiVertex(x + wide, y, s1, t0),
			vgui2::VGuiVertex(x + wide, y + tall, s1, t1), vgui2::VGuiVertex(x, y + tall, s0, t1) };
		vgui2::surface()->DrawTexturedPolygon(v, 4);
	}

	const int hover = m_iBadgeHoverClient;
	const int badge = GetBadge(hover);
	int x, y, wide, tall;
	vgui2::HFont font = GetNameFont();
	if (badge < 0 || font == vgui2::INVALID_FONT || !GetBadgeBounds(hover, x, y, wide, tall))
		return;

	// Tooltip: centred under the badge (above it when the board ends there), inside the board; the chat color
	// picker's tooltip look (CHudChatColorTooltip)
	const wchar_t *tip = BADGES[badge].tooltip;
	int textWide = 0, textTall = 0;
	vgui2::surface()->GetTextSize(font, tip, textWide, textTall);
	const int tipWide = textWide + 8, tipTall = textTall + 4;
	int tipX = x + wide / 2 - tipWide / 2;
	int tipY = y + tall + 2;
	if (tipY + tipTall > GetTall())
		tipY = y - tipTall - 2;
	tipX = std::max(0, std::min(tipX, GetWide() - tipWide));
	tipY = std::max(0, tipY);

	vgui2::surface()->DrawSetColor(Color(0, 0, 0, 220));
	vgui2::surface()->DrawFilledRect(tipX, tipY, tipX + tipWide, tipY + tipTall);
	vgui2::surface()->DrawSetColor(Color(255, 255, 255, 64));
	vgui2::surface()->DrawOutlinedRect(tipX, tipY, tipX + tipWide, tipY + tipTall);
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(Color(255, 255, 255, 255));
	vgui2::surface()->DrawSetTextPos(tipX + 4, tipY + 2);
	vgui2::surface()->DrawPrintText(tip, (int)wcslen(tip));
}

int CScorePanel::GetBadge(int client)
{
	if (client <= 0 || client > MAX_PLAYERS || !m_Players[client].connected)
		return -1;

#ifdef SBO_TEST_COMMANDS
	if (client == s_iTestDevClient)
		return 0;
#endif

	const uint64 steamID64 = GetValidSteamID64(m_Players[client]);
	if (steamID64 == 0)
		return -1;
	for (int i = 0; i < NUM_BADGES; i++)
	{
		if (BADGES[i].steamID64 == steamID64)
			return i;
	}
	return -1;
}

int CScorePanel::GetBadgeSlot()
{
	return std::max(8, m_pPlayerList->GetLineSpacing() - 6);
}

int CScorePanel::GetBadgeCap()
{
	// about twice the name's letters: 5/3 of the name font's height, 40 px with the 24 px font of 1200 lines and up;
	// at 1080 lines and below (20 px font and smaller) the row slot is smaller than this, so it changes nothing there
	vgui2::HFont font = GetNameFont();
	if (font == vgui2::INVALID_FONT)
		return 0;
	return vgui2::surface()->GetFontTall(font) * 5 / 3;
}

int CScorePanel::GetBadgeReserve(int client)
{
	if (GetBadge(client) < 0)
		return 0;

#ifdef SBO_TEST_COMMANDS
	if (s_bTestDevIconAfterScore && client == s_iTestDevClient)
		return 0;
#endif

	int sizeIndex;
	const int size = BadgeDrawSize(GetBadgeSlot(), GetBadgeCap(), sizeIndex);
	return size + 2 * BadgeGap(size);
}

bool CScorePanel::GetBadgeBounds(int client, int &x, int &y, int &wide, int &tall)
{
	const PlayerData &pd = m_PlayerData[client];
	if (!pd.bIsConnected || pd.nItemID == -1 || !m_pPlayerList->IsItemIDValid(pd.nItemID))
		return false;

	int itemX, itemY, itemWide, itemTall;
	if (!m_pPlayerList->GetItemBounds(pd.nItemID, itemX, itemY, itemWide, itemTall))
		return false;

	int sizeIndex;
	const int size = BadgeDrawSize(GetBadgeSlot(), GetBadgeCap(), sizeIndex);

	// the end of the cell's content: the name image (as wide as the name, "..." included)
	int cellX, cellY, cellWide, cellTall;
	int contentEnd;
#ifdef SBO_TEST_COMMANDS
	if (s_bTestDevIconAfterScore && client == s_iTestDevClient)
	{
		char fragsName[] = "frags";
		int column = m_pPlayerList->GetColumnIndexByName(pd.nTeamID, fragsName);
		if (column < 0 || !m_pPlayerList->GetMaxCellBounds(pd.nItemID, column, cellX, cellY, cellWide, cellTall))
			return false;
		wchar_t frags[16];
		V_snwprintf(frags, sizeof(frags) / sizeof(frags[0]), L"%d", m_Players[client].frags);
		int textWide = 0, textTall = 0;
		vgui2::surface()->GetTextSize(GetNameFont(), frags, textWide, textTall);
		contentEnd = cellX + textWide;
	}
	else
#endif
	{
		if (!m_pPlayerList->GetMaxCellBounds(pd.nItemID, NAME_COLUMN, cellX, cellY, cellWide, cellTall))
			return false;
		int nameWide, nameTall;
		m_pImageList->GetImage(MAX_PLAYERS + client)->GetSize(nameWide, nameTall);
		contentEnd = cellX + nameWide;
	}

	int listX, listY;
	m_pPlayerList->GetPos(listX, listY);
	x = listX + itemX + contentEnd + BadgeGap(size);
	y = listY + itemY + (itemTall - size) / 2;
	wide = tall = size;
	return true;
}

void CScorePanel::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "MenuMute"))
	{
		OnPlayerMenuCommand(MenuAction::Mute);
	}
	else if (!Q_stricmp(command, "MenuSteamProfile"))
	{
		OnPlayerMenuCommand(MenuAction::SteamProfile);
	}
	else if (!Q_stricmp(command, "MenuSteamURL"))
	{
		OnPlayerMenuCommand(MenuAction::SteamURL);
	}
	else if (!Q_stricmp(command, "MenuCopyName"))
	{
		OnPlayerMenuCommand(MenuAction::CopyName);
	}
	else if (!Q_stricmp(command, "MenuCopyNameRaw"))
	{
		OnPlayerMenuCommand(MenuAction::CopyNameRaw);
	}
	else if (!Q_stricmp(command, "MenuCopySteamID"))
	{
		OnPlayerMenuCommand(MenuAction::CopySteamID);
	}
	else if (!Q_stricmp(command, "MenuCopySteamID64"))
	{
		OnPlayerMenuCommand(MenuAction::CopySteamID64);
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CScorePanel::Reset()
{
	if (IsVisible())
	{
		ShowPanel(false);
	}

	m_iKillerIndex = 0;
	m_flKillerHighlightStart = 0;
	m_iIntermissionFragsLeft = -1;
}

void CScorePanel::ShowPanel(bool state)
{
	if (state == IsVisible())
		return;

	if (state)
	{
		FullUpdate();

		// Make active unless chat is visible
		// (possible if scoreboard is activated by intermission when chat is open)
		if (!VGui2_Chat_IsInMessageMode())
			Activate();
	}
	else
	{
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		m_pPlayerMenu->SetVisible(false);
		SetObjectivesOpen(false); // SBO 2.0: the objective list opens closed every time
		SetMuteHoverClient(0);
		m_iIgnoreMuteClickThinks = 0;
	}

	SetVisible(state);

	// cursor follows at once (inputw32.cpp re-centres the mouse when it hides: no view snap)
	vgui2::surface()->CalculateMouseVisible();
}

void CScorePanel::OnItemContextMenu(int itemID)
{
	OpenPlayerMenu(itemID);
}

void CScorePanel::OnItemLeftClick(int itemID)
{
	// SBO: a left-click on the row's mute button or on a badge with a link
	if (m_iIgnoreMuteClickThinks > 0 || !IsMouseInputEnabled())
		return;

	const int badge = GetBadge(m_iBadgeHoverClient);
	if (badge >= 0 && BADGES[badge].url)
	{
		HudChat_OpenURL(BADGES[badge].url);
		m_pPlayerList->ClearSelection(); // a button press, not a row selection
		return;
	}

	int client = GetMuteButtonClientAtCursor(itemID);
	if (client != 0)
	{
		TogglePlayerMute(client);
		m_pPlayerList->ClearSelection(); // a button press, not a row selection
	}
}

bool CScorePanel::CanMutePlayer(int client)
{
	if (client <= 0 || client > MAX_PLAYERS)
		return false;

	const sbo::ScorePlayer &sp = m_Players[client];
	return sp.connected && !sp.thisplayer && sbo::CanPlayerBeBlocked(client);
}

int CScorePanel::GetMuteButtonClientAtCursor(int itemID)
{
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	m_pPlayerList->ScreenToLocal(mx, my);

	if (mx < 0 || my < 0 || mx >= m_pPlayerList->GetWide() || my >= m_pPlayerList->GetTall())
		return 0;

	auto fnTestItem = [&](int id) -> int {
		// the row in the list, the cell in the row
		int x, y, wide, tall;
		if (!m_pPlayerList->GetItemBounds(id, x, y, wide, tall) || my < y || my >= y + tall)
			return 0;

		int cellX, cellY, cellWide, cellTall;
		if (m_iMuteColumn < 0 || !m_pPlayerList->GetMaxCellBounds(id, m_iMuteColumn, cellX, cellY, cellWide, cellTall))
			return 0;

		KeyValues *kv = m_pPlayerList->GetItemData(id);
		int client = kv ? kv->GetInt("client", 0) : 0;
		if (!CanMutePlayer(client))
			return 0;

		// the icon in the cell, not the padding around it: centred in the column like the list lays out a
		// COLUMN_CENTER image, and in the row's height like the row's label draws it
		int iconWide, iconTall;
		m_pImageList->GetImage(2 * MAX_PLAYERS + client)->GetSize(iconWide, iconTall);
		int iconX = x + cellX + (cellWide / 2) - (iconWide / 2);
		int iconY = y + std::max(0, (tall - iconTall) / 2);
		if (mx < iconX || mx >= iconX + iconWide || my < iconY || my >= iconY + iconTall)
			return 0;

		return client;
	};

	if (itemID != -1)
		return fnTestItem(itemID);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const PlayerData &pd = m_PlayerData[i];
		if (!pd.bIsConnected || pd.nItemID == -1)
			continue;

		int client = fnTestItem(pd.nItemID);
		if (client != 0)
			return client;
	}

	return 0;
}

void CScorePanel::SetMuteHoverClient(int client)
{
	if (client == m_iMuteHoverClient)
		return;

	if (m_iMuteHoverClient != 0)
		static_cast<CMuteButtonImage *>(m_pImageList->GetImage(2 * MAX_PLAYERS + m_iMuteHoverClient))->SetHovered(false);
	if (client != 0)
		static_cast<CMuteButtonImage *>(m_pImageList->GetImage(2 * MAX_PLAYERS + client))->SetHovered(true);

	m_iMuteHoverClient = client;
}

void CScorePanel::TogglePlayerMute(int client)
{
	if (client <= 0 || client > MAX_PLAYERS)
		return;

	const sbo::ScorePlayer &sp = UpdatePlayer(client);
	if (!sp.connected)
		return;

	// Never the local player, whatever asked for it (mute button, right-click menu)
	if (sp.thisplayer || client == sbo::GetLocalPlayerIndex())
		return;

	CHudChat *pChat = CHudChat::Get();
	const char *name = GetDisplayName(sp);

	if (sbo::IsPlayerBlocked(client))
	{
		// Unmute
		sbo::SetPlayerBlocked(client, false);

		char string1[1024];
		snprintf(string1, sizeof(string1), sbo::LocaliseText("#Unmuted"), name);
		if (pChat)
			pChat->ChatPrintf(0, "** %s", string1);
	}
	else if (sbo::CanPlayerBeBlocked(client))
	{
		// Mute
		sbo::SetPlayerBlocked(client, true);

		char string1[1024];
		char string2[1024];
		snprintf(string1, sizeof(string1), sbo::LocaliseText("#Muted"), name);
		snprintf(string2, sizeof(string2), "%s", sbo::LocaliseText("#No_longer_hear_that_player"));
		if (pChat)
		{
			// One chat line ("** Name Muted... no speak brother"); titles.txt's "Muted" starts with a space
			const char *muted = string1;
			while (*muted == ' ')
				muted++;
			pChat->ChatPrintf(0, "** %s%s", muted, string2);
		}
	}

	// Muting one player may mute others: players without a SteamID who share the engine's unique ID
	// (CVoiceStatus::GetPlayerBanID); never the local player
	UpdateAllClients();
}

sbo::ScorePlayer &CScorePanel::UpdatePlayer(int client)
{
	sbo::ScorePlayer &sp = m_Players[client];
	sbo::GetScorePlayer(client, sp, CClientSteamContext::IsSteamPipeEngine());
	return sp;
}

bool CScorePanel::ShouldShowXP()
{
	return sbo::IsCoopMap() && sbo::HasSboXP();
}

bool CScorePanel::IsHunted(int client)
{
	// The server's ScoreInfo playerclass (SboScoreboard_GetClass): bit 1 while the player is the Hunted, in play (category
	// 1) or dead (category 2: the role stays after a suicide / a death by the world). Not a spectator (category 5):
	// the role ends with spectating.
	const sbo::ScorePlayer &sp = m_Players[client];
	return (sp.playerclass & SBO_CLASS_HUNTED) && (sp.teamnumber == SBO_TEAM_HUNTED || sp.teamnumber == SBO_TEAM_DEAD)
	    && sbo::IsBountyMap() && IsSboCategoryMode();
}

bool CScorePanel::IsSboCategoryMode()
{
	// The server's SBO scoreboard (src/dlls SboScoreboard_*, the user's syndicate_scoreboard plugin) always sends
	// five labels with "Dead Ops"/"No Loadout" as teams 2/3 (team 1/4 vary: Game Master or ""/Hunted,
	// Alive Ops). Anything else (old server, teamplay model teams) uses BHL's grouping.
	return sbo::GetTeamNameCount() >= 5
	    && !Q_stricmp(sbo::GetTeamNameLabel(2), "Dead Ops")
	    && !Q_stricmp(sbo::GetTeamNameLabel(3), "No Loadout");
}

void CScorePanel::GetTeamLabel(int nTeamID, wchar_t *buf, int bufSizeInBytes)
{
	const char *pszName = nullptr;
	char tmp[64];

	if (nTeamID == TEAM_SPECTATOR)
	{
		pszName = "#SBO_Scores_Spectators";
	}
	else if (nTeamID == GHOST_SECTION_ID)
	{
		pszName = "#SBO_Scores_Ghosts";
	}
	else
	{
		pszName = sbo::GetTeamNameLabel(nTeamID);

		if (!pszName[0])
		{
			// no TeamNames label: a member's TeamInfo team name
			for (int i = 1; i <= MAX_PLAYERS; i++)
			{
				const sbo::ScorePlayer &sp = m_Players[i];
				if (sp.connected && m_PlayerData[i].nTeamID == nTeamID && sp.teamname[0])
				{
					pszName = sp.teamname;
					break;
				}
			}
		}

		if (!pszName[0])
		{
			snprintf(tmp, sizeof(tmp), "Team %d", nTeamID);
			pszName = tmp;
		}
	}

	wchar_t *localizedName = nullptr;
	if (pszName[0] == '#' && (localizedName = g_pVGuiLocalize->Find(pszName)))
	{
		V_wcsncpy(buf, localizedName, bufSizeInBytes);
	}
	else if (pszName[0] == '#' && nTeamID == TEAM_SPECTATOR)
	{
		V_wcsncpy(buf, L"Spectators", bufSizeInBytes);
	}
	else if (pszName[0] == '#' && nTeamID == GHOST_SECTION_ID)
	{
		V_wcsncpy(buf, L"Ghosts", bufSizeInBytes);
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode(pszName, buf, bufSizeInBytes);
	}
}

//! SBO: userinfo "sbo_avatar": gfx/avatars/..., ".tga", lower case [a-z0-9_./-], no "..", no "//", at most 63 bytes
static bool IsAvatarFilePath(const char *path)
{
	static const char prefix[] = "gfx/avatars/";
	const size_t len = path ? strlen(path) : 0;
	if (len <= sizeof(prefix) - 1 + 4 || len > 63 || strncmp(path, prefix, sizeof(prefix) - 1) != 0)
		return false;
	if (strcmp(path + len - 4, ".tga") != 0 || strstr(path, "..") || strstr(path, "//"))
		return false;
	for (size_t i = 0; i < len; i++)
	{
		const char c = path[i];
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' || c == '-'))
			return false;
	}
	return path[len - 5] != '/';
}

CTGAImage *CScorePanel::GetFileAvatar(const sbo::ScorePlayer &sp, int client)
{
	// gfx/avatars/<name>.tga, lower case [a-z0-9_./-], no "..": only for slots without a SteamID (real Steam players
	// keep their Steam avatar: anyone can set a userinfo key); missing (not downloaded): nullptr, looked for once
	static std::map<std::string, CTGAImage *> s_Images;
	if (GetValidSteamID64(sp) != 0)
		return nullptr;
	const char *path = sbo::GetPlayerInfoValue(client, "sbo_avatar");
	if (!IsAvatarFilePath(path))
		return nullptr;
	auto it = s_Images.find(path);
	if (it == s_Images.end())
	{
		CTGAImage *pImage = nullptr;
		if (g_pFullFileSystem && g_pFullFileSystem->FileExists(path))
		{
			std::string noExt(path, strlen(path) - 4);
			pImage = new CTGAImage(noExt.c_str());
		}
		it = s_Images.insert({ path, pImage }).first;
	}
	return it->second;
}

uint64 CScorePanel::GetValidSteamID64(const sbo::ScorePlayer &sp)
{
	uint64 sid = sp.steamID64;

	// Valid SteamID must have type of account = 1 (Individual)
	if (((sid & 0x00F0000000000000ull) >> 52) != 1)
		return 0;

	// ... and a non-zero account ID (bots, listen-server quirks)
	if ((sid & 0xFFFFFFFFull) == 0)
		return 0;

	return sid;
}

void CScorePanel::GetSteamIDString(const sbo::ScorePlayer &sp, char *buf, int bufSize)
{
	uint64 sid = GetValidSteamID64(sp);
	if (sid == 0)
	{
		buf[0] = '\0';
		return;
	}

	// STEAM_X:Y:Z without "STEAM_", universe 0 as GoldSrc prints it
	uint32 accountID = (uint32)(sid & 0xFFFFFFFFull);
	snprintf(buf, bufSize, "0:%u:%u", accountID & 1, accountID >> 1);
}

const char *CScorePanel::GetDisplayName(const sbo::ScorePlayer &sp, bool bNoColorCodes)
{
	if (bNoColorCodes || GetColorCodeAction() == ColorCodeAction::Strip)
		return RemoveColorCodes(sp.name);
	return sp.name;
}

ColorCodeAction CScorePanel::GetColorCodeAction()
{
	CHudChat *pChat = CHudChat::Get();
	return pChat ? pChat->GetColorCodeAction() : ColorCodeAction::Strip;
}

void CScorePanel::FullUpdate()
{
	UpdateServerName();
	UpdateMapName();
	UpdateModeInfo();
	RefreshItems();
}

void CScorePanel::UpdateMapName()
{
	char buf[64];
	wchar_t wbuf[64];
	const char *levelName = sbo::GetLevelName();
	V_FileBase(levelName ? levelName : "", buf, sizeof(buf));
	g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
	m_pMapNameLabel->SetText(wbuf);
}

void CScorePanel::UpdateModeInfo()
{
	int type, value1, value2;
	sbo::GetModeInfo(type, value1, value2);

	wchar_t wbuf[128] = L"";
	wchar_t wnum1[16], wnum2[16];
	V_snwprintf(wnum1, sizeof(wnum1) / sizeof(wchar_t), L"%d", value1);
	V_snwprintf(wnum2, sizeof(wnum2) / sizeof(wchar_t), L"%d", value2);

	if (type == sbo::MODEINFO_ROUNDS)
	{
		// "Round: 0 / 7" until the first round starts
		static wchar_t s_szDefault[] = L"Round: %s1 / %s2";
		wchar_t *fmt = g_pVGuiLocalize->Find("#SBO_Scores_Round");
		g_pVGuiLocalize->ConstructString(wbuf, sizeof(wbuf), fmt ? fmt : s_szDefault, 2, wnum1, wnum2);
	}
	else if (type == sbo::MODEINFO_FRAGLIMIT && value1 > 0)
	{
		// no frag limit (mp_fraglimit 0): no line. "Frags left: 12": what the leader still needs to end the map
		static wchar_t s_szDefault[] = L"Frags left: %s1";
		V_snwprintf(wnum1, sizeof(wnum1) / sizeof(wchar_t), L"%d", GetFragsLeft(value1));
		wchar_t *fmt = g_pVGuiLocalize->Find("#SBO_Scores_FragsLeft");
		g_pVGuiLocalize->ConstructString(wbuf, sizeof(wbuf), fmt ? fmt : s_szDefault, 1, wnum1);
	}

	m_pModeInfoLabel->SetText(wbuf);
}

int CScorePanel::GetFragsLeft(int fragLimit)
{
	// Intermission: the value it began with (players leaving take their frags with them; the server sends no
	// SboModeInfo any more either)
	const bool bIntermission = sbo::IsIntermission();
	if (bIntermission && m_iIntermissionFragsLeft >= 0)
		return m_iIntermissionFragsLeft;

	// The engine's / SBO's current scores, not m_Players: this also runs while the board is hidden (SboModeInfo)
	// and before the rows are refreshed.
	// The server's checks: CDm / CBr::Think end the map when a player's frags reach mp_fraglimit, CHalfLifeTeamplay
	// when a team's do (the sum of its players' frags, by team name).
	const bool bTeams = sbo::IsTeamplayMap();
	std::array<sbo::ScorePlayer, MAX_PLAYERS + 1> players;
	bool bAny = false;
	int best = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
		sbo::GetScorePlayer(i, players[i], false);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const sbo::ScorePlayer &sp = players[i];
		if (!sp.connected)
			continue;

		int frags = sp.frags;

		if (bTeams)
		{
			if (!sp.teamname[0])
				continue; // no team (spectator)

			// each team once, at its first member
			bool bCounted = false;
			for (int j = 1; j < i && !bCounted; j++)
				bCounted = players[j].connected && !Q_stricmp(players[j].teamname, sp.teamname);
			if (bCounted)
				continue;

			int deaths;
			if (!sbo::GetTeamScoreOverride(sp.teamname, frags, deaths))
			{
				for (int j = i + 1; j <= MAX_PLAYERS; j++)
				{
					if (players[j].connected && !Q_stricmp(players[j].teamname, sp.teamname))
						frags += players[j].frags;
				}
			}
		}

		if (!bAny || frags > best)
			best = frags;
		bAny = true;
	}

	const int left = std::max(0, fragLimit - best);
	m_iIntermissionFragsLeft = bIntermission ? left : -1;
	return left;
}

void CScorePanel::RefreshItems()
{
	std::fill(m_TeamData.begin(), m_TeamData.end(), TeamData());
	std::fill(m_PlayerData.begin(), m_PlayerData.end(), PlayerData());
	std::fill(m_IsTeamSectionCreated.begin(), m_IsTeamSectionCreated.end(), false);
	m_pPlayerList->RemoveAll();
	m_pPlayerList->RemoveAllSections();
	m_bShowXP = ShouldShowXP();

	RestoreSize();

	// Refresh player info first: GetPlayerTeam looks at all players
	for (int i = 1; i <= MAX_PLAYERS; i++)
		UpdatePlayer(i);

	// Assign player teams, calculate team scores
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const sbo::ScorePlayer &sp = m_Players[i];
		PlayerData &pd = m_PlayerData[i];
		pd.bIsConnected = sp.connected;

		if (!pd.bIsConnected)
			continue;

		pd.nItemID = -1;
		pd.nTeamID = GetPlayerTeam(i);

		TeamData &td = m_TeamData[pd.nTeamID];
		td.iFrags += sp.frags;
		td.iDeaths += sp.deaths;
		td.iPlayerCount++;
	}

	// Create header before any other sections
	CreateSection(HEADER_SECTION_ID);

	if (IsSboCategoryMode())
	{
		// SBO categories in their fixed order, all of them: a player changing category while the board is open
		// lands in the right place (SectionedListPanel hides empty sections).
		for (int team : s_SboCategoryOrder)
			CreateSection(team);
		CreateSection(GHOST_SECTION_ID); // last, below the Spectators: the slots not in the game (IsGhost)

		// anything else the server numbered (none expected)
		for (int i = 1; i <= MAX_TEAMS; i++)
		{
			if (m_TeamData[i].iPlayerCount > 0)
				CreateSection(i);
		}
	}
	else
	{
		// BHL: override team scores and add them to sorting list
		int iTeamCount = 0;

		for (int i = 0; i <= MAX_TEAMS; i++)
		{
			TeamData &td = m_TeamData[i];

			if (td.iPlayerCount == 0)
				continue;

			if (i != 0)
			{
				for (int j = 1; j <= MAX_PLAYERS; j++)
				{
					if (m_PlayerData[j].bIsConnected && m_PlayerData[j].nTeamID == i)
					{
						int frags, deaths;
						if (sbo::GetTeamScoreOverride(m_Players[j].teamname, frags, deaths))
						{
							td.iFrags = frags;
							td.iDeaths = deaths;
						}
						break;
					}
				}
			}

			m_SortedTeamIDs[iTeamCount] = i;
			iTeamCount++;
		}

		// Sort teams based on the score
		std::sort(m_SortedTeamIDs.begin(), m_SortedTeamIDs.begin() + iTeamCount, [&](int ilhs, int irhs) {
			const TeamData &lhs = m_TeamData[ilhs];
			const TeamData &rhs = m_TeamData[irhs];

			// Compare kills
			if (lhs.iFrags > rhs.iFrags)
				return true;
			else if (lhs.iFrags < rhs.iFrags)
				return false;

			// Compare deaths if kills are equal
			if (lhs.iDeaths < rhs.iDeaths)
				return true;
			else if (lhs.iDeaths > rhs.iDeaths)
				return false;

			// Compare idx if everything is equal
			return ilhs > irhs;
		});

		// Create sections for teams
		if (iTeamCount != 1 || m_SortedTeamIDs[0] != 0)
		{
			for (int i = 0; i < iTeamCount; i++)
			{
				CreateSection(m_SortedTeamIDs[i]);
			}
		}
	}

	// Create last section for spectators
	if (m_TeamData[TEAM_SPECTATOR].iPlayerCount > 0)
	{
		CreateSection(TEAM_SPECTATOR);
	}

	UpdateAllClients();
}

void CScorePanel::CreateSection(int nTeamID)
{
	if (m_IsTeamSectionCreated[nTeamID])
		return;

	m_IsTeamSectionCreated[nTeamID] = true;

	m_pPlayerList->AddSection(nTeamID, "", StaticPlayerSortFuncByFrags);

	if (nTeamID == HEADER_SECTION_ID)
	{
		m_pPlayerList->SetSectionAlwaysVisible(nTeamID);
		m_pPlayerList->SetSectionDividerColor(nTeamID, Color(0, 0, 0, 0));
	}
	else
	{
		// same colour as the team's names in chat / death notices (TEAM_SPECTATOR: the table's grey)
		// SBO: Ghosts: the same grey, faded like their rows
		Color color = TeamColor(nTeamID == TEAM_SPECTATOR || nTeamID == GHOST_SECTION_ID ? -1 : nTeamID);
		if (nTeamID == GHOST_SECTION_ID)
			color[3] = GHOST_ALPHA;
		m_pPlayerList->SetSectionFgColor(nTeamID, color);
	}

	// Avatar
	m_pPlayerList->AddColumnToSection(nTeamID, "avatar", "",
	    vgui2::SectionedListPanel::COLUMN_IMAGE | vgui2::SectionedListPanel::COLUMN_CENTER,
	    m_iColumnWidthAvatar);

	// Name
	const char *nameCol;

	if (nTeamID == HEADER_SECTION_ID)
		nameCol = "#SBO_Scores_ColName";
	else
		nameCol = "??? (?/?)";

	// SBO: an image column, the cell is the player's CPlayerNameImage (BHL: COLUMN_BRIGHT | COLUMN_COLORED text)
	m_pPlayerList->AddColumnToSection(nTeamID, "name", nameCol,
	    vgui2::SectionedListPanel::COLUMN_IMAGE,
	    GetNameColumnWidth());

	// SteamID
	if (false && hud_scoreboard_showsteamid.GetBool()) // SBO: no SteamID column (still in the right-click menu)
	{
		m_pPlayerList->AddColumnToSection(nTeamID, "steamid", nTeamID == HEADER_SECTION_ID ? "#BHL_Scores_ColSteamID" : "",
		    vgui2::SectionedListPanel::COLUMN_BRIGHT,
		    m_iColumnWidthSteamID);
	}

	// Efficiency
	if (hud_scoreboard_showeff.GetBool())
	{
		m_pPlayerList->AddColumnToSection(nTeamID, "eff", nTeamID == HEADER_SECTION_ID ? "#BHL_Scores_ColEff" : "???",
		    vgui2::SectionedListPanel::COLUMN_BRIGHT,
		    m_iColumnWidthEff);
	}

	// Frags (coop: the level, as SBO's server / the scoreboard plugin send it)
	const char *fragsCol = sbo::IsCoopMap() ? "#SBO_Scores_ColLevel" : "#SBO_Scores_ColScore";
	m_pPlayerList->AddColumnToSection(nTeamID, "frags", nTeamID == HEADER_SECTION_ID ? fragsCol : "???",
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthFrags);

	// Coop XP (no team total)
	if (m_bShowXP)
	{
		m_pPlayerList->AddColumnToSection(nTeamID, "xp", nTeamID == HEADER_SECTION_ID ? "#SBO_Scores_ColXP" : "",
		    vgui2::SectionedListPanel::COLUMN_BRIGHT,
		    m_iColumnWidthXP);
	}

	// Deaths
	m_pPlayerList->AddColumnToSection(nTeamID, "deaths", nTeamID == HEADER_SECTION_ID ? "#SBO_Scores_ColDeaths" : "???",
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthDeaths);

	// Ping
	const char *pingLabel;

	if (nTeamID == HEADER_SECTION_ID)
		pingLabel = hud_scoreboard_showloss.GetBool() ? "#BHL_Scores_ColPingLoss" : "#BHL_Scores_ColPing";
	else
		pingLabel = "";

	m_pPlayerList->AddColumnToSection(nTeamID, "ping", pingLabel,
	    vgui2::SectionedListPanel::COLUMN_BRIGHT,
	    m_iColumnWidthPing);

	// SBO: mute button (CMuteButtonImage), no title: the last column, at the right end of the row. As wide as the
	// avatar column and centred like it, so the icon is as far from the row's right edge as the avatar from the left
	m_iMuteColumn = m_pPlayerList->GetColumnCountBySection(nTeamID);
	m_pPlayerList->AddColumnToSection(nTeamID, "mute", "",
	    vgui2::SectionedListPanel::COLUMN_IMAGE | vgui2::SectionedListPanel::COLUMN_CENTER,
	    m_iColumnWidthMute);
}

void CScorePanel::UpdateAllClients()
{
	// GetPlayerTeam (BHL spectator rule) looks at every player: refresh all first
	for (int i = 1; i <= MAX_PLAYERS; i++)
		UpdatePlayer(i);

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		UpdateClientInfo(i);
	}

	UpdateScoresAndCounts();
	Resize();

	m_flLastUpdateTime = GetAbsoluteTime();
}

void CScorePanel::UpdateClientInfo(int client)
{
	const sbo::ScorePlayer &sp = m_Players[client];
	PlayerData &pd = m_PlayerData[client];

	if (sp.connected && !pd.bIsConnected)
	{
		// Player just connected
		pd.bIsConnected = true;

		if (pd.nItemID != -1 && m_pPlayerList->IsItemIDValid(pd.nItemID))
			m_pPlayerList->RemoveItem(pd.nItemID);

		pd.nItemID = -1;
		pd.nTeamID = GetPlayerTeam(client);
	}
	else if (!sp.connected && pd.bIsConnected)
	{
		// Player disconnected
		pd.bIsConnected = false;

		if (pd.nItemID != -1)
			m_pPlayerList->RemoveItem(pd.nItemID);
		pd.nItemID = -1;
		pd.nTeamID = 0;
	}

	// Skip unconnected players
	if (!sp.connected)
		return;

	int nTeamID = GetPlayerTeam(client);
	if (nTeamID != pd.nTeamID)
	{
		// Player changed team
		if (pd.nItemID != -1)
			m_pPlayerList->RemoveItem(pd.nItemID);
		pd.nItemID = -1;
		pd.nTeamID = nTeamID;
	}

	// Create section for player's team if need to
	CreateSection(pd.nTeamID);

	KeyValues *playerKv = new KeyValues("data");
	char szName[128];
	// SBO: a ghost's row is plain grey: no colour codes
	const bool bGhost = pd.nTeamID == GHOST_SECTION_ID;
	const bool bColorCodes = !bGhost && GetColorCodeAction() == ColorCodeAction::Handle;

	{
		char buf[128];
		playerKv->SetInt("client", client);

		if (sp.thisplayer)
			playerKv->SetBool("thisplayer", true);

		// Avatar
		UpdateClientIcon(client);
		playerKv->SetInt("avatar", client); // Client index == index into m_pImageList

		// Name
		// SBO: with the player's name colour (cvar namecolor) as colour codes: the name's uncoded text in it instead
		// of the row colour, ^0 / ^9 in the name back to it (hud_colortext 1 only; CHudChat::GetColoredPlayerName)
		const char *pszName = bGhost ? GetDisplayName(sp) : CHudChat::GetColoredPlayerName(client, GetDisplayName(sp, false));

		// BHL: "^0" ends the name's colour codes, the tag keeps the row colour
		if (sp.spectator)
			snprintf(buf, sizeof(buf), "%s %s%s", pszName, bColorCodes ? "^0" : "", m_szSpectatorTag);
		else if (IsHunted(client))
			snprintf(buf, sizeof(buf), "%s %s%s", pszName, bColorCodes ? "^0" : "", m_szHuntedTag); // SBO: Bounty Killer
		else
			snprintf(buf, sizeof(buf), "%s", pszName);

		// SBO: drawn by the player's name image (set up below, once the row exists)
		V_strncpy(szName, buf, sizeof(szName));
		playerKv->SetInt("name", MAX_PLAYERS + client);
		if (IsHunted(client))
			playerKv->SetBool("hunted", true); // SBO: Bounty Killer: the section's first row
		playerKv->SetString("sortname", GetDisplayName(sp)); // SBO: the rows' last sort key (StaticPlayerSortFuncByFrags)
		playerKv->SetInt("mute", 2 * MAX_PLAYERS + client); // SBO: mute button (UpdateClientIcon)

		// SteamID
		GetSteamIDString(sp, buf, sizeof(buf));
		playerKv->SetString("steamid", buf);

		// Efficiency
		float eff = CalculateEfficiency(sp.frags, sp.deaths);
		if (hud_scoreboard_effpercent.GetBool())
			snprintf(buf, sizeof(buf), "%.0f%%", eff * 100.0);
		else
			snprintf(buf, sizeof(buf), "%.2f", eff);
		playerKv->SetString("eff", buf);

		// Frags & deaths
		playerKv->SetInt("frags", sp.frags);
		playerKv->SetInt("deaths", sp.deaths);
		playerKv->SetInt("xp", sp.xp);

		// Ping
		if (hud_scoreboard_showloss.GetBool())
		{
			snprintf(buf, sizeof(buf), "%d/%d", sp.ping, sp.packetloss);
			playerKv->SetString("ping", buf);
		}
		else
		{
			playerKv->SetInt("ping", sp.ping);
		}

		// SBO: a ghost is not in the game: no score, deaths, XP or ping (a reused slot's last ScoreInfo, a lost
		// client's last ping); the sort keys stay 0, so its rows go by name
		if (bGhost)
		{
			playerKv->SetString("eff", "");
			playerKv->SetString("frags", "");
			playerKv->SetString("deaths", "");
			playerKv->SetString("xp", "");
			playerKv->SetString("ping", "");
		}
	}

	if (pd.nItemID == -1)
	{
		// Create player's row
		pd.nItemID = m_pPlayerList->AddItem(pd.nTeamID, playerKv);
		m_pPlayerList->InvalidateLayout();
	}
	else
	{
		// Update player's row
		m_pPlayerList->ModifyItem(pd.nItemID, pd.nTeamID, playerKv);
	}

	// Name colour = chat name colour (the team number can change without a section change in BHL grouping);
	// SBO: a ghost's row in the section's faded grey, its avatar faded too
	Color rowColor = ClientColor(client);
	if (bGhost)
	{
		rowColor = TeamColor(-1);
		rowColor[3] = GHOST_ALPHA;
	}
	m_pPlayerList->SetItemFgColor(pd.nItemID, rowColor);
	static_cast<CPlayerImage *>(m_pImageList->GetImage(client))
	    ->SetColor(bGhost ? Color(255, 255, 255, GHOST_ALPHA) : Color(255, 255, 255, 255));

	// SBO: the name cell: the row's font and colour, the column's width
	CPlayerNameImage *pName = static_cast<CPlayerNameImage *>(m_pImageList->GetImage(MAX_PLAYERS + client));
	pName->SetFont(GetNameFont());
	pName->SetColor(rowColor);
	pName->SetName(szName, bColorCodes);
	// SBO: a name with a badge leaves room for the badge after it (PostChildPaint)
	pName->ResizeImageToContentMaxWidth(m_pPlayerList->GetColumnWidthBySection(pd.nTeamID, NAME_COLUMN)
	    - GetBadgeReserve(client));
	m_pPlayerList->SetItemBgColor(pd.nItemID, GetPlayerBgColor(client));

	playerKv->deleteThis();
}

void CScorePanel::UpdateClientIcon(int client)
{
	const sbo::ScorePlayer &sp = m_Players[client];
	CPlayerImage *pImg = static_cast<CPlayerImage *>(m_pImageList->GetImage(client));

	// Update size
	int size = GetClientIconSize();
	pImg->SetSize(size, size);

	// Update muted state (SBO: never the local player)
	const bool bMuted = !sp.thisplayer && client != sbo::GetLocalPlayerIndex() && sbo::IsPlayerBlocked(client);
	pImg->SetMuted(bMuted);

	// SBO: the mute button at the end of the row. A muted player keeps it (to unmute) whatever he is now.
	CMuteButtonImage *pMute = static_cast<CMuteButtonImage *>(m_pImageList->GetImage(2 * MAX_PLAYERS + client));
	// The avatar's size, centred in a column as wide as the avatar's (4: never up to the edges of a narrower column)
	int muteSize = std::max(0, std::min(size, (int)m_iColumnWidthMute - 4));
	pMute->SetSize(muteSize, muteSize);
	if (bMuted)
		pMute->SetState(CMuteButtonImage::State::Muted);
	else
		pMute->SetState(CanMutePlayer(client) ? CMuteButtonImage::State::Unmuted : CMuteButtonImage::State::None);

	// SBO: a slot without a SteamID (a plugin's virtual slot, a bot) may name its picture in userinfo "sbo_avatar"
	CTGAImage *pFile = hud_scoreboard_showavatars.GetBool() ? CScorePanel::GetFileAvatar(sp, client) : nullptr;
	pImg->SetFileImage(pFile);

	// Update avatar
	uint64 steamID64 = GetValidSteamID64(sp);
	if (hud_scoreboard_showavatars.GetBool() && ClientSteamContext().SteamFriends() && ClientSteamContext().SteamUtils() && steamID64 != 0)
	{
		CSteamID steamIDForPlayer(steamID64);
		auto it = m_PlayerAvatars.find(steamIDForPlayer);

		if (it == m_PlayerAvatars.end())
		{
			CAvatarImage *pAvatar = new CAvatarImage();
			pAvatar->SetDrawFriend(false);
			pAvatar->SetAvatarSteamID(steamIDForPlayer);

			pImg->SetAvatar(pAvatar);
			m_PlayerAvatars.insert({ steamIDForPlayer, pAvatar });
		}
		else
		{
			pImg->SetAvatar(it->second);
		}
	}
	else
	{
		pImg->SetAvatar(nullptr);
	}
}

void CScorePanel::UpdateScoresAndCounts()
{
	// Reset team data
	for (int i = 1; i < (int)m_TeamData.size(); i++)
	{
		TeamData &td = m_TeamData[i];
		td.iPlayerCount = 0;
		td.iFrags = 0;
		td.iDeaths = 0;
	}

	// Refresh scores
	int iPlayerCount = 0;

	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		const sbo::ScorePlayer &sp = m_Players[i];

		if (!sp.connected)
			continue;

		TeamData &td = m_TeamData[GetPlayerTeam(i)];
		td.iFrags += sp.frags;
		td.iDeaths += sp.deaths;

		// SBO: a virtual slot is listed, never counted (its section's "(n/total)" and "Players: n / max")
		if (IsSboCategoryMode() && IsVirtualSlot(i))
			continue;
		td.iPlayerCount++;
		iPlayerCount++;
	}

	char buf[128];
	wchar_t wbuf[128];

	bool bCategories = IsSboCategoryMode();

	auto fnUpdateTeamHeader = [&](int nTeamID) {
		TeamData &td = m_TeamData[nTeamID];

		// Team name and player count
		wchar_t wname[128];
		GetTeamLabel(nTeamID, wname, sizeof(wname));
		// SBO categories: Alive Ops and Dead Ops count out of the players in the round (alive plus dead; not the
		// Game Master, No Loadout or the spectators); the other sections show their own count only, and Spectators
		// with none but a virtual slot (the radio's, listed, never counted) its name alone; "Players: n / max" stays
		// the slots used
		const bool bRound = bCategories && (nTeamID == SBO_TEAM_DEAD || nTeamID == SBO_TEAM_ALIVE);
		const int iRound = m_TeamData[SBO_TEAM_DEAD].iPlayerCount + m_TeamData[SBO_TEAM_ALIVE].iPlayerCount;
		if (!bCategories)
			V_snwprintf(wbuf, 128, L"%ls (%d/%d)", wname, td.iPlayerCount, iPlayerCount);
		else if (bRound)
			V_snwprintf(wbuf, 128, L"%ls (%d/%d)", wname, td.iPlayerCount, iRound);
		else if (td.iPlayerCount > 0 || nTeamID != SBO_TEAM_SPECTATORS)
			V_snwprintf(wbuf, 128, L"%ls (%d)", wname, td.iPlayerCount);
		else
			V_snwprintf(wbuf, 128, L"%ls", wname);
		m_pPlayerList->ModifyColumn(nTeamID, "name", wbuf);

		// SBO categories (Dead Ops, Alive Ops, ...) aren't teams: level/score, deaths and efficiency are individual
		// stats, so their sections have no totals (BHL's team mode keeps them: real teamplay teams)
		if (bCategories)
		{
			m_pPlayerList->ModifyColumn(nTeamID, "eff", L"");
			m_pPlayerList->ModifyColumn(nTeamID, "frags", L"");
			m_pPlayerList->ModifyColumn(nTeamID, "deaths", L"");
			return;
		}

		// Team efficiency
		float eff = CalculateEfficiency(td.iFrags, td.iDeaths);
		if (hud_scoreboard_effpercent.GetBool())
			snprintf(buf, sizeof(buf), "%.0f%%", eff * 100.0);
		else
			snprintf(buf, sizeof(buf), "%.2f", eff);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "eff", wbuf);

		// Team frags
		snprintf(buf, sizeof(buf), "%d", td.iFrags);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "frags", wbuf);

		// Team deaths
		snprintf(buf, sizeof(buf), "%d", td.iDeaths);
		g_pVGuiLocalize->ConvertANSIToUnicode(buf, wbuf, sizeof(wbuf));
		m_pPlayerList->ModifyColumn(nTeamID, "deaths", wbuf);
	};

	// Update team score and player count
	for (int i = 1; i <= MAX_TEAMS; i++)
	{
		TeamData &td = m_TeamData[i];

		if (!m_IsTeamSectionCreated[i])
			continue;

		if (!bCategories && td.iPlayerCount > 0)
		{
			for (int j = 1; j <= MAX_PLAYERS; j++)
			{
				if (m_Players[j].connected && GetPlayerTeam(j) == i)
				{
					int frags, deaths;
					if (sbo::GetTeamScoreOverride(m_Players[j].teamname, frags, deaths))
					{
						td.iFrags = frags;
						td.iDeaths = deaths;
					}
					break;
				}
			}
		}

		fnUpdateTeamHeader(i);
	}

	// Update spectator team
	if (m_IsTeamSectionCreated[TEAM_SPECTATOR])
		fnUpdateTeamHeader(TEAM_SPECTATOR);

	// SBO: Ghosts: "Ghosts (n)"
	if (m_IsTeamSectionCreated[GHOST_SECTION_ID])
		fnUpdateTeamHeader(GHOST_SECTION_ID);

	// Update total player count: "Players: 5 / 32"
	{
		static wchar_t s_szDefault[] = L"Players: %s1 / %s2";
		wchar_t wnum1[16], wnum2[16];
		V_snwprintf(wnum1, sizeof(wnum1) / sizeof(wchar_t), L"%d", iPlayerCount);
		V_snwprintf(wnum2, sizeof(wnum2) / sizeof(wchar_t), L"%d", sbo::GetMaxClients());
		wchar_t *fmt = g_pVGuiLocalize->Find("#SBO_Scores_Players");
		g_pVGuiLocalize->ConstructString(wbuf, sizeof(wbuf), fmt ? fmt : s_szDefault, 2, wnum1, wnum2);
		m_pPlayerCountLabel->SetText(wbuf);
	}

	// SBO: "Frags left" follows the scores
	UpdateModeInfo();
}

vgui2::HFont CScorePanel::GetNameFont()
{
	// the font of a row (SectionedListPanel's item buttons are Labels)
	for (int i = 0; i < m_pPlayerList->GetChildCount(); i++)
	{
		vgui2::Panel *pChild = m_pPlayerList->GetChild(i);
		if (!pChild || V_strcmp(pChild->GetClassName(), "CItemButton"))
			continue;
		vgui2::Label *pRow = dynamic_cast<vgui2::Label *>(pChild);
		if (pRow && pRow->GetFont() != vgui2::INVALID_FONT)
			return pRow->GetFont();
	}

	// no row yet: what CItemButton::ApplySchemeSettings will take
	vgui2::HFont font = m_pPlayerList->GetRowFont();
	if (font != vgui2::INVALID_FONT)
		return font;

	vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(m_pPlayerList->GetScheme());
	if (!pScheme)
		return vgui2::INVALID_FONT;
	return pScheme->GetFont(pScheme->GetResourceString("SectionedListPanel.Font"), m_pPlayerList->IsProportional());
}

int CScorePanel::GetNameColumnWidth()
{
	int w = m_iColumnWidthName;

	if (true || !hud_scoreboard_showsteamid.GetBool()) // SBO: SteamID column removed; name column gets its width
		w += m_iColumnWidthSteamID;

	if (!hud_scoreboard_showeff.GetBool())
		w += m_iColumnWidthEff;

	if (m_bShowXP)
		w -= m_iColumnWidthXP;

	w -= m_iColumnWidthMute; // SBO: the mute button column (the last one) takes its width from the name column

	return w;
}

int CScorePanel::GetPlayerTeam(int client)
{
	const sbo::ScorePlayer &sp = m_Players[client];
	int team = sp.teamnumber;
	if (team < 0 || team > MAX_TEAMS)
		team = 0;

	if (IsSboCategoryMode())
	{
		// the server's category (5 = Spectators gets its own TeamNames-labelled section);
		// 0 = not categorised yet (connecting, HLTV): unassigned rows under the column header
		// Bounty Killer: the Hunted in play (category 1) is one of the alive players, in their section (marked by the
		// row, UpdateClientInfo)
		if (team == SBO_TEAM_HUNTED && sbo::IsBountyMap())
			return SBO_TEAM_ALIVE;
		// a virtual slot (a plugin's, e.g. the radio's): with the spectators, as BHL's grouping puts a name without
		// a team
		if (IsVirtualSlot(client))
			return SBO_TEAM_SPECTATORS;
		// a slot not in the game (connecting, reconnecting, connection lost): Ghosts, below every section; before
		// this section existed a connecting player's team 0 put the row in the column header's section, above all
		if (IsGhost(client))
			return GHOST_SECTION_ID;
		if (team == HEADER_SECTION_ID)
			return SBO_TEAM_SPECTATORS; // HLTV (the engine's spectator flag): no category, never a ghost
		return team;
	}

	// BHL: no team name while others have one -> spectator
	if (sp.teamname[0] == '\0')
	{
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			if (m_Players[i].connected && m_Players[i].teamname[0])
				return TEAM_SPECTATOR;
		}
	}

	return team;
}

bool CScorePanel::IsVirtualSlot(int client)
{
	const sbo::ScorePlayer &sp = m_Players[client];
	// only where the engine gives SteamIDs (HL25): elsewhere nobody has one and every new player would look virtual
	return sp.connected && !sp.thisplayer && CClientSteamContext::IsSteamPipeEngine() && sp.teamnumber == 0 &&
	       sp.teamname[0] == '\0' && GetValidSteamID64(sp) == 0;
}

bool CScorePanel::IsGhost(int client)
{
	const sbo::ScorePlayer &sp = m_Players[client];
	if (!sp.connected || sp.spectator)
		return false;
	// every player in the game has a category name (SboScoreboard_TeamName: never "" but for HLTV): none yet = the
	// server has not run his InitHUD (still connecting or downloading); a reused slot may still hold the last
	// player's team number, so the name decides
	return sp.teamname[0] == '\0' || (sp.playerclass & SBO_CLASS_LOST);
}

Color CScorePanel::GetPlayerBgColor(int client)
{
	if (m_Players[client].thisplayer)
	{
		return m_ThisPlayerBgColor;
	}
	else if (m_iKillerIndex == client)
	{
		Color color = m_KillerBgColor;
		float t = m_flKillerHighlightStart;
		float dt = HIGHLIGHT_KILLER_TIME;
		float k = -color.a() / dt;
		float b = -k * (t + dt);
		float a = k * sbo::GetHudTime() + b;
		if (a > color.a() || a <= 0)
		{
			m_iKillerIndex = 0;
			m_flKillerHighlightStart = 0;
		}
		else
		{
			color[3] = (int)a;
			return color;
		}
	}

	// SBO: Bounty Killer: the Hunted's row in the Hunted's colour (the name's, sbo::GetTeamColor), faint
	if (IsHunted(client))
	{
		Color color = TeamColor(SBO_TEAM_HUNTED);
		color[3] = HUNTED_BG_ALPHA;
		return color;
	}

	return Color(0, 0, 0, 0);
}

float CScorePanel::CalculateEfficiency(int kills, int deaths)
{
	int type = clamp(hud_scoreboard_efftype.GetInt(), 0, 2);

	switch (type)
	{
	case 0:
	{
		// K / D
		if (deaths == 0)
			deaths = 1;
		return (float)kills / deaths;
	}
	case 1:
	{
		// K / (D + 1)
		if (deaths == -1)
			deaths = 0;
		return (float)kills / (deaths + 1);
	}
	case 2:
	{
		// K / (K + D)
		if (kills + deaths == 0)
			return 1.0f;
		return (float)kills / (kills + deaths);
	}
	}

	return 0;
}

int CScorePanel::GetClientIconSize()
{
	return clamp(m_pPlayerList->GetLineSpacing() - 2, 0, 32);
}

void CScorePanel::CreatePlayerMenu()
{
	m_pPlayerMenu = new vgui2::Menu(this, "PlayerMenu");
	m_pPlayerMenu->SetVisible(false);
	m_pPlayerMenu->AddActionSignalTarget(this);

	m_MenuData.nMuteItemID = m_pPlayerMenu->AddMenuItem("Mute", "#BHL_Scores_MenuMute", "MenuMute", this);
	m_pPlayerMenu->AddSeparator();

	m_MenuData.nProfilePageItemID = m_pPlayerMenu->AddMenuItem("SteamProfile", "#BHL_Scores_MenuSteamProfile", "MenuSteamProfile", this);
	m_MenuData.nProfileUrlItemID = m_pPlayerMenu->AddMenuItem("SteamURL", "#BHL_Scores_MenuSteamURL", "MenuSteamURL", this);
	m_pPlayerMenu->AddSeparator();

	m_pPlayerMenu->AddMenuItem("CopyName", "#BHL_Scores_MenuCopyName", "MenuCopyName", this);
	m_pPlayerMenu->AddMenuItem("CopyNameRaw", "#BHL_Scores_MenuCopyNameRaw", "MenuCopyNameRaw", this);
	m_pPlayerMenu->AddMenuItem("CopySteamID", "#BHL_Scores_MenuCopySteamID", "MenuCopySteamID", this);
	m_pPlayerMenu->AddMenuItem("CopySteamID64", "#BHL_Scores_MenuCopySteamID64", "MenuCopySteamID64", this);
}

void CScorePanel::OpenPlayerMenu(int itemID)
{
	// Set menu info
	m_MenuData.nItemID = itemID;
	m_MenuData.nClient = 0;
	KeyValues *kv = m_pPlayerList->GetItemData(itemID);
	if (!kv)
		return;
	m_MenuData.nClient = kv->GetInt("client", 0);
	if (m_MenuData.nClient <= 0 || m_MenuData.nClient > MAX_PLAYERS)
		return;

	// SteamID64
	m_MenuData.nSteamID64 = GetValidSteamID64(UpdatePlayer(m_MenuData.nClient));
	if (m_MenuData.nSteamID64 != 0)
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfilePageItemID, true);
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfileUrlItemID, true);
	}
	else
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfilePageItemID, false);
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nProfileUrlItemID, false);
	}

	// Player muting
	// SBO: the local player's menu always has a greyed out Mute (BHL offered Unmute there whenever the ban list
	// matched the local player's ID); the same for a player who can't be muted (CanMutePlayer)
	bool thisPlayer = kv->GetBool("thisplayer", 0) || m_MenuData.nClient == sbo::GetLocalPlayerIndex();
	bool isMuted = !thisPlayer && sbo::IsPlayerBlocked(m_MenuData.nClient);
	if (thisPlayer || (!isMuted && !CanMutePlayer(m_MenuData.nClient)))
	{
		// Can't mute yourself
		m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuMute", new KeyValues("Command", "command", "MenuMute"));
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nMuteItemID, false);
	}
	else
	{
		m_pPlayerMenu->SetItemEnabled(m_MenuData.nMuteItemID, true);
		if (isMuted)
		{
			m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuUnmute", new KeyValues("Command", "command", "MenuMute"));
		}
		else
		{
			m_pPlayerMenu->UpdateMenuItem(m_MenuData.nMuteItemID, "#BHL_Scores_MenuMute", new KeyValues("Command", "command", "MenuMute"));
		}
	}

	m_pPlayerMenu->PositionRelativeToPanel(this, vgui2::Menu::CURSOR, 0, true);
}

void CScorePanel::OnPlayerMenuCommand(MenuAction command)
{
	if (m_MenuData.nClient <= 0 || m_MenuData.nClient > MAX_PLAYERS)
		return;

	const sbo::ScorePlayer &sp = UpdatePlayer(m_MenuData.nClient);

	if (!sp.connected)
		return;

	switch (command)
	{
	case MenuAction::Mute:
	{
		TogglePlayerMute(m_MenuData.nClient);
		break;
	}
	case MenuAction::SteamProfile:
	{
		if (ClientSteamContext().SteamFriends())
		{
			// Open in overlay
			CSteamID steamId = CSteamID((uint64)m_MenuData.nSteamID64);
			ClientSteamContext().SteamFriends()->ActivateGameOverlayToUser("steamid", steamId);
		}
		else
		{
			// Open in browser
			std::string url = STEAM_PROFILE_URL + std::to_string(m_MenuData.nSteamID64);
			vgui2::system()->ShellExecute("open", url.c_str());
		}

		break;
	}
	case MenuAction::SteamURL:
	{
		std::string url = STEAM_PROFILE_URL + std::to_string(m_MenuData.nSteamID64);
		vgui2::system()->SetClipboardText(url.c_str(), url.size());
		break;
	}
	case MenuAction::CopyName:
	{
		wchar_t name[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(GetDisplayName(sp), name, sizeof(name));
		vgui2::system()->SetClipboardText(name, wcslen(name));
		break;
	}
	case MenuAction::CopyNameRaw:
	{
		wchar_t name[64];
		g_pVGuiLocalize->ConvertANSIToUnicode(sp.name, name, sizeof(name));
		vgui2::system()->SetClipboardText(name, wcslen(name));
		break;
	}
	case MenuAction::CopySteamID:
	{
		char buf[64];
		GetSteamIDString(sp, buf, sizeof(buf));
		if (!buf[0])
			break;
		std::string steamid = "STEAM_" + std::string(buf);
		vgui2::system()->SetClipboardText(steamid.c_str(), steamid.size());
		break;
	}
	case MenuAction::CopySteamID64:
	{
		std::string steamid = std::to_string(m_MenuData.nSteamID64);
		vgui2::system()->SetClipboardText(steamid.c_str(), steamid.size());
		break;
	}
	}
}

CScorePanel::SizeMode CScorePanel::GetSizeMode()
{
	return (SizeMode)clamp(hud_scoreboard_size.GetInt(), 0, 2);
}

int CScorePanel::GetLineSpacingForHeight(int h)
{
	if (h < 600)
		return 18; // (0; 600)
	if (h < 720)
		return 22; // [600; 720]
	if (h < 800)
		return 24; // [720; 800)
	if (h < 1024)
		return 26; // [800; 1024)
	if (h < 1080)
		return 28; // [1024; 1080)

	return 28; // >= 1080
}

int CScorePanel::GetLineSpacingForNormal()
{
	if (hud_scoreboard_spacing_normal.GetInt() > 0)
		return hud_scoreboard_spacing_normal.GetInt();
	return 0;
}

int CScorePanel::GetLineSpacingForCompact()
{
	if (hud_scoreboard_spacing_compact.GetInt() > 0)
		return hud_scoreboard_spacing_compact.GetInt();
	return GetLineSpacingForHeight(GetScreenHeight());
}

void CScorePanel::RestoreSize()
{
	if (GetSizeMode() == SizeMode::Compact)
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForCompact());
	else
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForNormal());
}

void CScorePanel::Resize()
{
	SizeMode mode = GetSizeMode();
	const int screenHeight = GetScreenHeight();
	m_iObjectivesRevision = Objectives_Revision();

	// Returns true if scrollbar was enabled
	auto fnUpdateSize = [&](int &height) {
		int wide, tall, x, y;
		int listHeight = 0, addHeight = 0;
		bool bIsOverflowed = false;
		height = 0;

		m_pPlayerList->GetPos(x, y);
		addHeight = y + m_iBottomPadding; // Distance on the top and on the bottom of the player list
		addHeight += ObjectivesTall(); // SBO 2.0: the objective list under the players
		height += addHeight;
		m_pPlayerList->GetContentSize(wide, tall);
		listHeight = std::max((int)m_iMinHeight, tall);
		height += listHeight;

		if (screenHeight - height < m_iVerticalMargin * 2)
		{
			// It didn't fit
			height = screenHeight - m_iVerticalMargin * 2;
			listHeight = height - addHeight;
			m_pPlayerList->SetVerticalScrollbar(true);
			bIsOverflowed = true;
		}
		else
		{
			m_pPlayerList->SetVerticalScrollbar(false);
		}

		m_pPlayerList->GetSize(wide, tall);
		m_pPlayerList->SetSize(wide, listHeight);

		return bIsOverflowed;
	};

	int wide, tall, x, y;
	int height;
	if (fnUpdateSize(height) && mode == SizeMode::Auto)
	{
		// Content overflowed, scrollbar is now visible. Set comapct line spacing
		m_pPlayerList->SetLineSpacingOverride(GetLineSpacingForCompact());

		// Refresh player info to update avatar sizes
		for (int i = 1; i <= MAX_PLAYERS; i++)
		{
			UpdateClientInfo(i);
		}

		// Resize again
		fnUpdateSize(height);
	}

	GetSize(wide, tall);
	SetSize(wide, height);

	// Move to center
	GetPos(x, y);
	y = (screenHeight - height) / 2;
	SetPos(x, y);
}

bool CScorePanel::StaticPlayerSortFuncByFrags(vgui2::SectionedListPanel *list, int itemID1, int itemID2)
{
	KeyValues *it1 = list->GetItemData(itemID1);
	KeyValues *it2 = list->GetItemData(itemID2);
	Assert(it1 && it2);

	// SBO: Bounty Killer: the Hunted is the first row of its section, whatever the scores
	int v1 = it1->GetInt("hunted");
	int v2 = it2->GetInt("hunted");
	if (v1 != v2)
		return v1 > v2;

	// first compare frags
	v1 = it1->GetInt("frags");
	v2 = it2->GetInt("frags");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// coop: same level, more XP first (0 for both without the XP column)
	v1 = it1->GetInt("xp");
	v2 = it2->GetInt("xp");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// next compare deaths
	v1 = it1->GetInt("deaths");
	v2 = it2->GetInt("deaths");
	if (v1 > v2)
		return false;
	else if (v1 < v2)
		return true;

	// SBO: all the same: by name (no colour codes, no case). BHL went straight to the itemID's, which follow the order
	// the rows were created in: by client number when the board opens, then by who changed section last
	int iNameDiff = Q_stricmp(it1->GetString("sortname"), it2->GetString("sortname"));
	if (iNameDiff != 0)
		return iNameDiff < 0;

	// the same, so compare itemID's (as a sentinel value to get deterministic sorts)
	return itemID1 < itemID2;
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
unsigned long long VGui2_GetPlayerSteamID64(int client)
{
	if (client <= 0 || client > sbo::SCORE_MAX_PLAYERS)
		return 0;

	sbo::ScorePlayer sp;
	sbo::GetScorePlayer(client, sp, CClientSteamContext::IsSteamPipeEngine());
	return sp.connected ? CScorePanel::GetValidSteamID64(sp) : 0;
}

bool VGui2_Score_Init()
{
	if (CScorePanel::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	hud_scoreboard_mousebtn.Register();
	hud_scoreboard_showavatars.Register();
	hud_scoreboard_showloss.Register();
	hud_scoreboard_efftype.Register();
	hud_scoreboard_effpercent.Register();
	hud_scoreboard_showsteamid.Register();
	hud_scoreboard_showeff.Register();
	hud_scoreboard_size.Register();
	hud_scoreboard_spacing_normal.Register();
	hud_scoreboard_spacing_compact.Register();

	// BHL: HUD_Init -> ClientSteamContext().Activate()
	ClientSteamContext().Activate();

	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/scoreboard_%language%.txt");

	CScorePanel *pPanel = new CScorePanel(g_pViewport);
	pPanel->MakeReadyForUse();
	return true;
}

bool VGui2_Score_IsActive()
{
	return CScorePanel::Get() != nullptr;
}

void VGui2_Score_Show()
{
	if (CScorePanel::Get())
		CScorePanel::Get()->ShowPanel(true);
}

void VGui2_Score_Hide()
{
	if (CScorePanel::Get())
		CScorePanel::Get()->ShowPanel(false);
}

bool VGui2_Score_IsVisible()
{
	return CScorePanel::Get() && CScorePanel::Get()->IsVisible();
}

void VGui2_Score_Reset()
{
	sbo::ResetSboXP(); // new map / server: no XP until the server sends some
	sbo::ResetModeInfo(); // ... and no round / frags left line until its InitHUD (SboModeInfo)
	if (CScorePanel::Get())
		CScorePanel::Get()->Reset();
}

void VGui2_Score_UpdateModeInfo()
{
	if (CScorePanel::Get())
		CScorePanel::Get()->UpdateModeInfo();
}

void VGui2_Score_UpdateOnPlayerInfo(int client)
{
	if (CScorePanel::Get())
		CScorePanel::Get()->UpdateOnPlayerInfo(client);
}

void VGui2_Score_DeathMsg(int killer, int victim)
{
	if (CScorePanel::Get())
		CScorePanel::Get()->DeathMsg(killer, victim);
}

bool VGui2_Score_MouseKey(int down, int mouseButton)
{
	CScorePanel *pPanel = CScorePanel::Get();

	// BHL CClientViewport::KeyInput
	if (!pPanel || !down || !pPanel->IsVisible())
		return false;

	if (!pPanel->IsMouseInputEnabled())
	{
		int btn = hud_scoreboard_mousebtn.GetInt();
		if ((mouseButton == 1 && btn == 1) || (mouseButton == 2 && btn == 2))
		{
			pPanel->EnableMousePointer(true);
			return true;
		}
	}
	else if (mouseButton != 0)
	{
		// Ignore mouse input so it can be handled by VGUI
		return true;
	}

	return false;
}
