//========= SBO VGUI2: spectator target info =========//
//
// Purpose: see sbo_spectator_info.h.
//
// The drawing follows the kill feed (hud_deathnotice.cpp, BugfixedHL-Rebased CHudDeathNoticePanel): backgrounds in
// PaintBackground, text in Paint (text rendering leaves alpha blending off).
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h"
#include "sbo_bridge.h"
#include "sbo_menus.h"
#include "sbo_spectator_info.h"

#define SPECTATORINFO_RES VGUI2_ROOT_DIR "resource/spectatorinfo.res"

namespace
{
// "weapon_pm66" -> "pm66" (the part the #SBO_Weapon_* tokens are named after)
const char *WeaponShortName(const char *classname)
{
	return !strncmp(classname, "weapon_", 7) ? classname + 7 : classname;
}
} // namespace

CSboSpectatorInfoPanel::CSboSpectatorInfoPanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "SpectatorInfoPanel")
{
	memset(&m_State, 0, sizeof(m_State));

	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);

	// as the kill feed and the voice labels: ClientScheme (SpecInfo_* colours, "Default" font), which players can
	// change in clientscheme.res. The spectator panel itself has SBO's menu scheme.
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/clientscheme.res", "ClientScheme"));

	if (g_pFullFileSystem && g_pFullFileSystem->FileExists(SPECTATORINFO_RES))
	{
		// the panel's entry, by name (as voicestatus.res)
		KeyValues *pFile = new KeyValues("spectatorinfo.res");
		if (pFile->LoadFromFile(g_pFullFileSystem, SPECTATORINFO_RES))
		{
			KeyValues *pSettings = pFile->FindKey(GetName());
			if (pSettings)
				ApplySettings(pSettings);
		}
		pFile->deleteThis();
	}
	else
		sbo::ConsolePrint("Spectator info: " SPECTATORINFO_RES " not found, using defaults\n");

	SetVisible(false);
}

void CSboSpectatorInfoPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0, 0, 0, 0)); // the box is drawn in PaintBackground

	// the font may have changed: measure again
	m_bHasState = false;
}

void CSboSpectatorInfoPanel::SetAnchor(int centerX, int bottomY)
{
	if (centerX == m_iCenterX && bottomY == m_iBottomY)
		return;

	m_iCenterX = centerX;
	m_iBottomY = bottomY;
	if (m_bHasState)
		UpdateBounds();
}

void CSboSpectatorInfoPanel::Update()
{
	sbo::SpectatorTargetInfo info;
	sbo::ScorePlayer sp;

	if (sbo::GetSpectatorTargetInfo(info))
		sbo::GetScorePlayer(info.player, sp, false);

	if (!sp.connected)
	{
		if (IsVisible())
			SetVisible(false);
		return;
	}

	State state;
	memset(&state, 0, sizeof(state)); // compared as bytes
	state.player = info.player;
	state.health = info.health;
	state.armor = info.armor;
	state.legsBroken = info.legsBroken;
	Q_strncpy(state.weapon, info.weapon, sizeof(state.weapon));

	// The scoreboard's score of the player: on co-op maps the level and the XP column's value (from servers that
	// send it), else the frags. A co-op Game Master (scoreboard category 1) has frags there, not a level. BR: the
	// rounds won on this map, from the server's message (the scoreboard has no such column).
	if (sbo::IsCoopMap())
	{
		if (sp.teamnumber != 1)
			state.scoreMode = sbo::HasSboXP() ? SCORE_LEVEL_XP : SCORE_LEVEL;
	}
	else if (sbo::IsBrMap())
		state.scoreMode = SCORE_WON;
	else
		state.scoreMode = SCORE_POINTS;

	if (state.scoreMode == SCORE_WON)
		state.score = info.brWins;
	else if (state.scoreMode != SCORE_NONE)
		state.score = sp.frags;
	if (state.scoreMode == SCORE_LEVEL_XP)
		state.xp = sp.xp;

	if (!m_bHasState || memcmp(&state, &m_State, sizeof(state)))
	{
		m_State = state;
		m_bHasState = true;
		Rebuild();
		UpdateBounds();
	}

	if (!IsVisible())
		SetVisible(true);
}

void CSboSpectatorInfoPanel::AddItem(int row, const char *labelToken, const wchar_t *value, bool bWarning)
{
	if (row < 0 || row >= ROW_COUNT || m_iItemCount[row] >= ROW_ITEMS)
		return;

	Item &item = m_Items[row][m_iItemCount[row]++];
	sboui::Localize(labelToken, item.wszLabel, sizeof(item.wszLabel));
	wcsncpy(item.wszValue, value, TEXT_LEN - 1);
	item.wszValue[TEXT_LEN - 1] = 0;
	item.iLabelWide = GetTextWide(item.wszLabel);
	item.iValueWide = GetTextWide(item.wszValue);
	item.bWarning = bWarning;
}

void CSboSpectatorInfoPanel::AddNumberItem(int row, const char *labelToken, int value)
{
	char buf[16];
	wchar_t wbuf[16];
	Q_snprintf(buf, sizeof(buf), "%d", value);
	Q_UTF8ToWString(buf, wbuf, sizeof(wbuf), STRINGCONVERT_REPLACE);
	AddItem(row, labelToken, wbuf);
}

void CSboSpectatorInfoPanel::Rebuild()
{
	wchar_t wbuf[TEXT_LEN];

	for (int i = 0; i < ROW_COUNT; i++)
		m_iItemCount[i] = 0;

	// row 1: health, armor, weapon
	AddNumberItem(0, "#SBO_SpecInfo_Health", m_State.health);
	AddNumberItem(0, "#SBO_SpecInfo_Armor", m_State.armor);

	if (m_State.weapon[0])
	{
		// "#SBO_Weapon_pm66"; a weapon without a name there shows its short classname (as the plugin did)
		char token[64];
		Q_snprintf(token, sizeof(token), "#SBO_Weapon_%s", WeaponShortName(m_State.weapon));
		if (g_pVGuiLocalize && g_pVGuiLocalize->Find(token))
			sboui::Localize(token, wbuf, sizeof(wbuf));
		else
			sboui::TextToWide(WeaponShortName(m_State.weapon), wbuf, sizeof(wbuf));
	}
	else
		sboui::Localize("#SBO_SpecInfo_NoWeapon", wbuf, sizeof(wbuf));
	AddItem(0, "#SBO_SpecInfo_Weapon", wbuf);

	// row 2: legs, and the scoreboard's score
	sboui::Localize(m_State.legsBroken ? "#SBO_SpecInfo_LegsBroken" : "#SBO_SpecInfo_LegsOK", wbuf, sizeof(wbuf));
	AddItem(1, "#SBO_SpecInfo_Legs", wbuf, m_State.legsBroken != 0);

	switch (m_State.scoreMode)
	{
	case SCORE_LEVEL_XP:
		AddNumberItem(1, "#SBO_SpecInfo_Level", m_State.score);
		AddNumberItem(1, "#SBO_SpecInfo_XP", m_State.xp);
		break;
	case SCORE_LEVEL:
		AddNumberItem(1, "#SBO_SpecInfo_Level", m_State.score);
		break;
	case SCORE_POINTS:
		AddNumberItem(1, "#SBO_SpecInfo_Points", m_State.score);
		break;
	case SCORE_WON:
		AddNumberItem(1, "#SBO_SpecInfo_Won", m_State.score);
		break;
	}
}

int CSboSpectatorInfoPanel::GetRowWide(int row) const
{
	int wide = 0;
	for (int i = 0; i < m_iItemCount[row]; i++)
	{
		const Item &item = m_Items[row][i];
		if (i > 0)
			wide += m_iItemSpacing;
		wide += item.iLabelWide + m_iLabelSpacing + item.iValueWide;
	}
	return wide;
}

int CSboSpectatorInfoPanel::GetRowTall() const
{
	const int fontTall = m_TextFont ? vgui2::surface()->GetFontTall(m_TextFont) : 0;
	return m_iRowHeight > fontTall ? m_iRowHeight : fontTall;
}

void CSboSpectatorInfoPanel::UpdateBounds()
{
	// as wide as the wider row, the other one centred in it
	int contentWide = 0;
	for (int i = 0; i < ROW_COUNT; i++)
	{
		const int rowWide = GetRowWide(i);
		if (rowWide > contentWide)
			contentWide = rowWide;
	}

	const int wide = contentWide + 2 * m_iHPadding;
	const int tall = ROW_COUNT * GetRowTall() + 2 * m_iVPadding;
	SetBounds(m_iCenterX - wide / 2, m_iBottomY - m_iBottomMargin - tall, wide, tall);
}

void CSboSpectatorInfoPanel::PaintBackground()
{
	if (!m_bHasState)
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(m_ColorBg);
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	// 1-pixel outline, as the spectator bars' LineBorder
	vgui2::surface()->DrawSetColor(m_ColorBorder);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
}

void CSboSpectatorInfoPanel::Paint()
{
	if (!m_bHasState || !m_TextFont)
		return;

	const int wide = GetWide();
	const int rowTall = GetRowTall();
	const int textY = (rowTall - vgui2::surface()->GetFontTall(m_TextFont)) / 2;
	int y = m_iVPadding;

	vgui2::surface()->DrawSetTextFont(m_TextFont);

	for (int row = 0; row < ROW_COUNT; row++)
	{
		int x = (wide - GetRowWide(row)) / 2;
		for (int i = 0; i < m_iItemCount[row]; i++)
		{
			const Item &item = m_Items[row][i];
			if (i > 0)
				x += m_iItemSpacing;
			DrawInfoText(x, y + textY, item.wszLabel, m_ColorLabel);
			x += item.iLabelWide + m_iLabelSpacing;
			DrawInfoText(x, y + textY, item.wszValue, item.bWarning ? m_ColorWarning : m_ColorValue);
			x += item.iValueWide;
		}
		y += rowTall;
	}
}

int CSboSpectatorInfoPanel::GetTextWide(const wchar_t *str) const
{
	if (!m_TextFont)
		return 0;

	int x = 0;
	for (; *str; str++)
		x += vgui2::surface()->GetCharacterWidth(m_TextFont, *str);
	return x;
}

// character by character, as measured (hud_deathnotice.cpp DrawKfText)
int CSboSpectatorInfoPanel::DrawInfoText(int x0, int y0, const wchar_t *str, Color color)
{
	int x = 0;
	vgui2::surface()->DrawSetTextColor(color);

	for (; *str; str++)
	{
		vgui2::surface()->DrawSetTextPos(x0 + x, y0);
		vgui2::surface()->DrawUnicodeChar(*str);
		x += vgui2::surface()->GetCharacterWidth(m_TextFont, *str);
	}

	return x;
}
