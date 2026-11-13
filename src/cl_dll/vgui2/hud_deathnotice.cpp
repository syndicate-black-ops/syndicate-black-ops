//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: VGUI2 kill feed (see hud_deathnotice.h). Ported from BugfixedHL-Rebased
//          src/game/client/hud/death_notice_panel.cpp (CHudDeathNoticePanel).
//
// Changes vs. BHL:
//   * rows come from VGui2_KillFeed_AddEntry (names + encoded colours, see vgui2_api.h) instead of player
//     indices, so non-player rows (NPCs) work; death.cpp turns DeathMsg into such a call
//   * name colours: SBO's team table (sbo::GetClientColor / sbo::GetTeamColor), NPC / plain colours from
//     killfeed.res ("npc_name_color" / "plain_name_color", scheme colours KF_Name_NPC / KF_Name_Plain)
//   * colour codes ^0..^9 / ^xRGB / ^#RRGGBB in player names are kept for every player (BHL: team 0 only, SBO's categories are never
//     team 0) and follow the chat's hud_colortext like BHL: 1 = drawn in the code's colour (^0/^9 back to the name's
//     state colour), 2 = removed, 0 = shown as typed
//   * VGUI2_KF_FLAG_HEADSHOT adds the d_headshot icon (CS 1.6; "HS" text without one); the icon is the d_<weapon> sprite or the skull like BHL (never the
//     weapon as text; "*" only if hud.txt has no d_skull either); a headshot whose icon would be that fallback skull
//     (e.g. an NPC kill) shows only the d_headshot icon, no skull (with "HS" text the skull stays)
//   * layout from ui/resource/killfeed.res (BHL: HudLayout.res entry HudDeathNoticePanel), scheme ClientScheme
//   * the panel stays visible and simply paints nothing while the HUD is hidden (sbo::KillFeedShouldDraw: hud_draw,
//     HIDEHUD_ALL, intermission, not connected, VGUI1 team/class menus) -- VGUI2 doesn't think invisible panels
//   * HUD sprites are drawn through the bridge (sbo::DrawHudSpriteAdditive, BHL CHudRenderer::DrawSprite)
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "hud_chat.h"
#include "hud_deathnotice.h"
#include "sbo_bridge.h"
#include "sbo_cvar.h"
#include "vgui2_api.h"

namespace
{
CSboCvar hud_deathnotice_vgui("hud_deathnotice_vgui", "1");             // BHL: 0 = legacy (sprite) death notices
CSboCvar hud_deathnotice_time_self("hud_deathnotice_time_self", "12");   // BHL: rows of the local player
sbo::CvarHandle s_hDeathNoticeTime = nullptr;                           // legacy hud_deathnotice_time (death.cpp)

#define KILLFEED_RES VGUI2_ROOT_DIR "resource/killfeed.res"
const wchar_t HEADSHOT_TEXT[] = L"HS";

// hud_colortext (the chat's cvar, BHL CHud::GetColorCodeAction); no chat: codes removed
ColorCodeAction GetColorCodeAction()
{
	CHudChat *pChat = CHudChat::Get();
	return pChat ? pChat->GetColorCodeAction() : ColorCodeAction::Strip;
}

// bKeepColorCodes: keep ^0..^9 / ^xRGB / ^#RRGGBB (hud_colortext 0/1), else they are removed
int ToWide(const char *utf8, wchar_t *out, int outBytes, bool bKeepColorCodes = false)
{
	char buf[128];
	Q_strncpy(buf, utf8 ? utf8 : "", sizeof(buf));
	Q_UTF8ToWString(bKeepColorCodes ? buf : RemoveColorCodes(buf), out, outBytes, STRINGCONVERT_REPLACE);
	return (int)wcslen(out);
}

// SBO: a player's name with his name color (cvar namecolor) as color codes, which DrawKfText draws like the codes
// typed in a name: the name's uncoded text in the name color, ^0 / ^9 in the name back to it (hud_colortext 1 only;
// CHudChat::GetColoredPlayerName). colorCode: the name's VGUI2_KF_COLOR_*; only VGUI2_KF_COLOR_CLIENT names a player.
const char *GetColoredName(const char *name, int colorCode)
{
	if (colorCode < 0 || (colorCode & 0x40000000) || !(colorCode & 0x20000000))
		return name;
	return CHudChat::GetColoredPlayerName(colorCode & 0xFF, name);
}
} // namespace

CHudDeathNoticePanel *CHudDeathNoticePanel::m_sInstance = nullptr;

CHudDeathNoticePanel::CHudDeathNoticePanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "HudDeathNoticePanel")
{
	Assert(!m_sInstance);
	m_sInstance = this;

	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);

	// BHL: child of its viewport, ClientScheme. SBO's viewport uses ChatScheme -> load ClientScheme here (KF_* colours).
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/clientscheme.res", "ClientScheme"));

	// BHL's HudLayout.res defaults (xpos r640, ypos 24, 628x468 in 640x480 units): the right part of the screen.
	SetBounds(vgui2::scheme()->GetProportionalScaledValue(12), vgui2::scheme()->GetProportionalScaledValue(24),
	          vgui2::scheme()->GetProportionalScaledValue(628), vgui2::scheme()->GetProportionalScaledValue(468));

	if (g_pFullFileSystem && g_pFullFileSystem->FileExists(KILLFEED_RES))
		LoadControlSettings(KILLFEED_RES);
	else
		sbo::DevPrint("Kill feed: " KILLFEED_RES " not found, using defaults\n");

	SetVisible(true);
}

CHudDeathNoticePanel::~CHudDeathNoticePanel()
{
	if (m_sInstance == this)
		m_sInstance = nullptr;
}

void CHudDeathNoticePanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0, 0, 0, 0)); // rows draw their own backgrounds
	m_iHeadshotWide = 0;           // font may have changed
}

void CHudDeathNoticePanel::Reset()
{
	m_iEntryCount = 0;
	m_HUD_d_skull = sbo::GetHudSpriteIndex("d_skull");
	m_HUD_d_headshot = sbo::GetHudSpriteIndex("d_headshot");
	m_iHeadshotWide = 0;
	m_flLastTime = sbo::GetHudTime();
}

void CHudDeathNoticePanel::UpdateRowTall()
{
	// BHL VidInit: row >= 2 corner textures (rounded DrawBox) and >= the skull sprite
	int cornerWide = 0, cornerTall = 0;
	GetCornerTextureSize(cornerWide, cornerTall);

	int skullWide = 0, skullTall = 0;
	sbo::GetHudSpriteSize(m_HUD_d_skull, skullWide, skullTall);

	int fontTall = m_TextFont ? vgui2::surface()->GetFontTall(m_TextFont) : 0;

	m_iRowTall = m_iRowHeight;
	if (m_iRowTall < cornerTall * 2)
		m_iRowTall = cornerTall * 2;
	if (m_iRowTall < skullTall)
		m_iRowTall = skullTall;
	if (m_iRowTall < fontTall)
		m_iRowTall = fontTall;

	// SBO 2.0: headshot marker width, before any row is measured (GetEntryContentWide)
	int hsWide = 0, hsTall = 0;
	if (sbo::GetHudSpriteSize(m_HUD_d_headshot, hsWide, hsTall))
		m_iHeadshotWide = hsWide;
	else if (m_TextFont)
		m_iHeadshotWide = GetTextWide(HEADSHOT_TEXT, (int)wcslen(HEADSHOT_TEXT));
}

Color CHudDeathNoticePanel::ResolveColor(int code) const
{
	int r = 216, g = 216, b = 216;

	if (code == VGUI2_KF_COLOR_NPC || (code > 0 && (code & 0x10000000)))
		return m_ColorNameNPC; // a Game Master's monster too (its owner's part is coloured in the text)
	if (code == VGUI2_KF_COLOR_PLAIN)
		return m_ColorNamePlain;

	if (code & 0x40000000)
	{
		r = (code >> 16) & 0xFF;
		g = (code >> 8) & 0xFF;
		b = code & 0xFF;
	}
	else if (code & 0x20000000)
		sbo::GetClientColor(code & 0xFF, r, g, b);
	else if (code >= 0)
		sbo::GetTeamColor(code, r, g, b);
	else
		return m_ColorNamePlain;

	return Color(r, g, b, 255);
}

// SBO 2.0 GM mode: "<GM>'s <monster>" (a VGUI2_KF_COLOR_OWNED name): the GM's name exactly as a player name of his
// is drawn (his team colour with his name colour, resolved now), the rest in the NPC colour (hud_colortext 1)
const char *CHudDeathNoticePanel::OwnedName(const char *name, int colorCode, bool bColorCodes, char *buf, int bufLen) const
{
	if (!name || colorCode <= 0 || !(colorCode & 0x10000000) || !bColorCodes)
		return name;
	const int owner = colorCode & 0xFF;
	const char *ownerName = sbo::GetPlayerName(owner);
	const size_t n = ownerName ? strlen(ownerName) : 0;
	if (!n || strncmp(name, ownerName, n))
		return name;
	int r, g, b;
	sbo::GetClientColor(owner, r, g, b);
	const Color npc = m_ColorNameNPC;
	// "'s" goes on in his colours (his name's last colour), only the monster's name in the NPC colour
	const char *rest = name + n;
	const char *possessive = !strncmp(rest, "'s", 2) ? "'s" : "";
	rest += strlen(possessive);
	Q_snprintf(buf, bufLen, "^#%02X%02X%02X%s%s^#%02X%02X%02X%s", r, g, b,
	    GetColoredName(ownerName, VGUI2_KF_COLOR_CLIENT(owner)), possessive, npc.r(), npc.g(), npc.b(), rest);
	return buf;
}

void CHudDeathNoticePanel::AddItem(const char *killerName, int killerColor, bool killerIsLocal,
                                   const char *victimName, int victimColor, bool victimIsLocal,
                                   const char *weaponName, int flags)
{
	if (!victimName || !victimName[0])
		return;

	Entry e;

	if (victimIsLocal)
		e.type = EntryType::Death;
	else if (killerIsLocal)
		e.type = EntryType::Kill;

	e.bIsTeamKill = (flags & VGUI2_KF_FLAG_TEAMKILL) != 0;
	e.bHeadshot = (flags & VGUI2_KF_FLAG_HEADSHOT) != 0;

	// Player colour codes (BHL): kept unless hud_colortext 2; parsed (drawn in colour) only with hud_colortext 1
	ColorCodeAction colorCodeAction = GetColorCodeAction();
	bool bKeepCodes = colorCodeAction != ColorCodeAction::Strip;
	e.bColorCodes = colorCodeAction == ColorCodeAction::Handle;

	// Killer (none for suicides / world)
	if (!(flags & VGUI2_KF_FLAG_SUICIDE) && killerName && killerName[0])
	{
		char owned[160];
		killerName = OwnedName(killerName, killerColor, e.bColorCodes, owned, sizeof(owned));
		e.iKillerLen = ToWide(GetColoredName(killerName, killerColor), e.wszKiller, sizeof(e.wszKiller), bKeepCodes);
		e.iKillerWide = GetTextWide(e.wszKiller, e.iKillerLen, e.bColorCodes);
		e.killerColor = ResolveColor(killerColor);
	}

	// Victim
	char ownedVictim[160];
	victimName = OwnedName(victimName, victimColor, e.bColorCodes, ownedVictim, sizeof(ownedVictim));
	e.iVictimLen = ToWide(GetColoredName(victimName, victimColor), e.wszVictim, sizeof(e.wszVictim), bKeepCodes);
	e.iVictimWide = GetTextWide(e.wszVictim, e.iVictimLen, e.bColorCodes);
	e.victimColor = ResolveColor(victimColor);

	// Icon (BHL): d_<weapon> sprite, else the skull -- the weapon is never drawn as text
	int spriteId = -1;

	if (weaponName && weaponName[0])
	{
		char spriteName[64];
		Q_snprintf(spriteName, sizeof(spriteName), "d_%s", weaponName);
		spriteId = sbo::GetHudSpriteIndex(spriteName);
	}

	if (spriteId < 0)
		spriteId = m_HUD_d_skull;

	// SBO 2.0: a headshot whose icon would be the fallback skull (no d_<weapon> sprite, e.g. an NPC's own attack;
	// death.cpp's "skull" for non-player killers is the same fallback) shows only the headshot icon, in the
	// weapon icon's place; without a d_headshot sprite ("HS" text) the skull stays so the row keeps an icon
	if (e.bHeadshot && spriteId >= 0 && spriteId == m_HUD_d_skull && m_HUD_d_headshot >= 0)
	{
		spriteId = m_HUD_d_headshot;
		e.bHeadshot = false;
	}

	if (spriteId < 0)
	{
		// no hud.txt skull either: a placeholder
		e.nSpriteIdx = -1;
		e.iWeaponLen = ToWide("*", e.wszWeapon, sizeof(e.wszWeapon));
		e.iIconWide = GetTextWide(e.wszWeapon, e.iWeaponLen);
		e.iIconTall = 0;
	}
	else
	{
		e.nSpriteIdx = spriteId;
		sbo::GetHudSpriteSize(spriteId, e.iIconWide, e.iIconTall);
	}

	// Expiration time
	float ttl = 6.0f;
	if (e.type != EntryType::Other)
		ttl = hud_deathnotice_time_self.GetFloat();
	else if (s_hDeathNoticeTime)
		ttl = sbo::CvarGetFloat(s_hDeathNoticeTime);

	e.flEndTime = sbo::GetHudTime() + ttl;

	// Insert into the list: full -> drop the row that expires first (BHL)
	if (m_iEntryCount >= KILLFEED_COUNT)
	{
		int minItem = 0;
		for (int i = 1; i < m_iEntryCount; i++)
		{
			if (m_Entries[i].flEndTime < m_Entries[minItem].flEndTime)
				minItem = i;
		}

		for (int i = minItem; i < m_iEntryCount - 1; i++)
			m_Entries[i] = m_Entries[i + 1];

		m_iEntryCount--;
	}

	m_Entries[m_iEntryCount++] = e;
}

void CHudDeathNoticePanel::RemoveExpired(float now)
{
	int newCount = 0;
	for (int i = 0; i < m_iEntryCount; i++)
	{
		if (m_Entries[i].flEndTime > now)
		{
			if (newCount != i)
				m_Entries[newCount] = m_Entries[i];
			newCount++;
		}
	}
	m_iEntryCount = newCount;
}

void CHudDeathNoticePanel::OnThink()
{
	BaseClass::OnThink();

	float now = sbo::GetHudTime();

	// HUD time restarted (new map, demo): the end times are meaningless
	if (now + 1.0f < m_flLastTime)
		m_iEntryCount = 0;
	m_flLastTime = now;

	RemoveExpired(now);
}

// SBO 2.0 GM mode: the rows start under the Game Master's nearest players panel (top right) while it is up
static int GmTopOffset()
{
	const int top = VGui2_GmKillFeedTop();
	if (top <= 0 || !CHudDeathNoticePanel::Get())
		return 0;
	int x = 0, y = 0;
	CHudDeathNoticePanel::Get()->LocalToScreen(x, y);
	return top > y ? top - y : 0;
}

void CHudDeathNoticePanel::GetRowsBottom(int &screenY, int &screenRight)
{
	int x = 0, y = GmTopOffset();
	if (m_iEntryCount > 0 && hud_deathnotice_vgui.GetBool() && sbo::KillFeedShouldDraw())
	{
		UpdateRowTall();
		y += m_iEntryCount * (m_iRowTall + m_iVMargin);
	}
	int right = GetWide();
	LocalToScreen(x, y);
	int rightY = 0;
	LocalToScreen(right, rightY);
	screenY = y;
	screenRight = right;
}

void CHudDeathNoticePanel::PaintBackground()
{
	// HACK ALERT (BHL): sprites are drawn here, text in Paint(), because text rendering disables alpha-blending at
	// the end, which would draw the transparent backgrounds opaque.
	if (m_iEntryCount == 0 || !hud_deathnotice_vgui.GetBool() || !sbo::KillFeedShouldDraw())
		return;

	UpdateRowTall();

	int panelWide = GetWide();
	int y = GmTopOffset();

	// Draw backgrounds
	for (int i = 0; i < m_iEntryCount; i++)
	{
		const Entry &entry = m_Entries[i];
		int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
		int x = panelWide - wide;
		Color bgColor;

		if (entry.type == EntryType::Kill)
			bgColor = m_ColorBgKill;
		else if (entry.type == EntryType::Death)
			bgColor = m_ColorBgDeath;
		else
			bgColor = m_ColorBg;

		DrawBox(x, y, wide, m_iRowTall, bgColor, 1.0f);

		y += m_iRowTall + m_iVMargin;
	}

	// Draw sprites
	y = GmTopOffset();

	for (int i = 0; i < m_iEntryCount; i++)
	{
		const Entry &entry = m_Entries[i];

		if (entry.nSpriteIdx >= 0)
		{
			int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
			int x = panelWide - wide;

			int iconX = m_iHPadding + entry.iKillerWide;
			if (entry.iKillerWide != 0)
				iconX += m_iIconPadding; // padding on the left only if there is a text

			int iconY = (m_iRowTall - entry.iIconTall) / 2;

			Color color;
			if (entry.bIsTeamKill)
				color = m_ColorIconTK;
			else if (entry.type == EntryType::Kill)
				color = m_ColorIconKill;
			else if (entry.type == EntryType::Death)
				color = m_ColorIconDeath;
			else
				color = m_ColorIcon;

			sbo::DrawHudSpriteAdditive(entry.nSpriteIdx, x + iconX, y + iconY, color.r(), color.g(), color.b());

			// SBO 2.0: headshot icon between the weapon and the victim (CS 1.6)
			if (entry.bHeadshot && m_HUD_d_headshot >= 0)
			{
				int hsWide = 0, hsTall = 0;
				sbo::GetHudSpriteSize(m_HUD_d_headshot, hsWide, hsTall);
				sbo::DrawHudSpriteAdditive(m_HUD_d_headshot, x + iconX + entry.iIconWide + m_iIconPadding,
				                           y + (m_iRowTall - hsTall) / 2, color.r(), color.g(), color.b());
			}
		}

		y += m_iRowTall + m_iVMargin;
	}
}

void CHudDeathNoticePanel::Paint()
{
	if (m_iEntryCount == 0 || !hud_deathnotice_vgui.GetBool() || !sbo::KillFeedShouldDraw())
		return;

	int panelWide = GetWide();
	int y = GmTopOffset();
	int fontTall = vgui2::surface()->GetFontTall(m_TextFont);
	int textY = (m_iRowTall - fontTall) / 2;

	vgui2::surface()->DrawSetTextFont(m_TextFont);

	for (int i = 0; i < m_iEntryCount; i++)
	{
		const Entry &entry = m_Entries[i];
		int wide = 2 * m_iHPadding + GetEntryContentWide(entry);
		int x = panelWide - wide;
		x += m_iHPadding;

		Color iconColor;
		if (entry.bIsTeamKill)
			iconColor = m_ColorIconTK;
		else if (entry.type == EntryType::Kill)
			iconColor = m_ColorIconKill;
		else if (entry.type == EntryType::Death)
			iconColor = m_ColorIconDeath;
		else
			iconColor = m_ColorIcon;

		// Draw killer name
		if (entry.iKillerWide != 0)
		{
			DrawKfText(x, y + textY, entry.wszKiller, entry.iKillerLen, entry.killerColor, entry.bColorCodes);
			x += entry.iKillerWide + m_iIconPadding;
		}

		// Icon (drawn in PaintBackground) or weapon text
		if (entry.nSpriteIdx < 0)
			DrawKfText(x, y + textY, entry.wszWeapon, entry.iWeaponLen, iconColor);
		x += entry.iIconWide + m_iIconPadding;

		if (entry.bHeadshot)
		{
			if (m_HUD_d_headshot < 0)
				DrawKfText(x, y + textY, HEADSHOT_TEXT, (int)wcslen(HEADSHOT_TEXT), iconColor);
			// else: the d_headshot sprite (PaintBackground)
			x += m_iHeadshotWide + m_iIconPadding;
		}

		// Draw victim name
		DrawKfText(x, y + textY, entry.wszVictim, entry.iVictimLen, entry.victimColor, entry.bColorCodes);
		x += entry.iVictimWide;

		y += m_iRowTall + m_iVMargin;
	}
}

int CHudDeathNoticePanel::GetEntryContentWide(const Entry &e) const
{
	int w = e.iKillerWide + m_iIconPadding + e.iIconWide + e.iVictimWide;

	if (e.iKillerWide != 0)
		w += m_iIconPadding;

	if (e.bHeadshot)
		w += m_iHeadshotWide + m_iIconPadding;

	return w;
}

// BHL GetColoredTextWide (bColorCodes) / GetTextWide
int CHudDeathNoticePanel::GetTextWide(const wchar_t *str, int len, bool bColorCodes) const
{
	int x = 0;
	for (int i = 0; i < len; i++)
	{
		// SBO: ^0..^9 (2 chars), ^xRGB (5 chars) or ^#RRGGBB (8 chars)
		const int codeLen = bColorCodes ? GetColorCodeLength(str + i) : 0;
		if (codeLen > 0 && i + codeLen <= len)
		{
			// Skip color code
			i += codeLen - 1;
			continue;
		}

		x += vgui2::surface()->GetCharacterWidth(m_TextFont, str[i]);
	}
	return x;
}

// BHL DrawColoredText (bColorCodes): ^1..^8 switch to the chat's ColorCode colours, SBO's ^xRGB / ^#RRGGBB to that colour (no
// darker than COLOR_CODE_MIN_LUMA), ^0/^9 back to c (the state colour)
int CHudDeathNoticePanel::DrawKfText(int x0, int y0, const wchar_t *str, int len, Color c, bool bColorCodes)
{
	int x = 0;
	vgui2::surface()->DrawSetTextColor(c);

	CHudChat *pChat = CHudChat::Get();

	for (int i = 0; i < len; i++)
	{
		const int codeLen = bColorCodes ? GetColorCodeLength(str + i) : 0;
		if (codeLen > 0 && i + codeLen <= len)
		{
			// Set color
			Color code = c;
			if (pChat)
				pChat->GetColorCodeColor(str + i, code);
			vgui2::surface()->DrawSetTextColor(code);
			i += codeLen - 1;
			continue;
		}

		vgui2::surface()->DrawSetTextPos(x0 + x, y0);
		vgui2::surface()->DrawUnicodeChar(str[i]);
		x += vgui2::surface()->GetCharacterWidth(m_TextFont, str[i]);
	}

	return x;
}

//-----------------------------------------------------------------------------
// vgui2_api.h
//-----------------------------------------------------------------------------
bool VGui2_KillFeed_Init()
{
	if (CHudDeathNoticePanel::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	hud_deathnotice_vgui.Register();
	hud_deathnotice_time_self.Register();
	s_hDeathNoticeTime = sbo::FindCvar("hud_deathnotice_time");

	CHudDeathNoticePanel *pPanel = new CHudDeathNoticePanel(g_pViewport);
	pPanel->MakeReadyForUse();
	pPanel->Reset();
	return true;
}

bool VGui2_KillFeed_IsActive()
{
	return CHudDeathNoticePanel::Get() && hud_deathnotice_vgui.GetBool();
}

void VGui2_KillFeed_Reset()
{
	if (CHudDeathNoticePanel::Get())
		CHudDeathNoticePanel::Get()->Reset();
}

void VGui2_KillFeed_AddEntry(const char *killerName, int killerColor, bool killerIsLocal,
                             const char *victimName, int victimColor, bool victimIsLocal,
                             const char *weaponName, int flags)
{
	if (CHudDeathNoticePanel::Get())
	{
		CHudDeathNoticePanel::Get()->AddItem(killerName, killerColor, killerIsLocal, victimName, victimColor,
		                                     victimIsLocal, weaponName, flags);
	}
}
