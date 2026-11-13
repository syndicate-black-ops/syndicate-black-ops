//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: VGUI2 kill feed. Port of BugfixedHL-Rebased's CHudDeathNoticePanel
//          (src/game/client/hud/death_notice_panel.{h,cpp}); see docs/dev/KILLFEED_PORT.md.
//
// SDK world: hl1_source_sdk headers + sbo_bridge.h / vgui2_api.h only.
//=================================================================//

#ifndef SBO_VGUI2_HUD_DEATHNOTICE_H
#define SBO_VGUI2_HUD_DEATHNOTICE_H

#include <Color.h>
#include <vgui_controls/EditablePanel.h>

class CHudDeathNoticePanel : public vgui2::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE(CHudDeathNoticePanel, vgui2::EditablePanel);

	static CHudDeathNoticePanel *Get() { return m_sInstance; }

	CHudDeathNoticePanel(vgui2::Panel *pParent);
	~CHudDeathNoticePanel();

	//! New map / VidInit: drop all rows, refresh the skull sprite and the row height.
	void Reset();

	//! See VGui2_DeathNotice_AddEntry (vgui2_api.h) for the arguments.
	void AddItem(const char *killerName, int killerColor, bool killerIsLocal,
	             const char *victimName, int victimColor, bool victimIsLocal,
	             const char *weaponName, int flags);

	//! SBO 2.0 objectives (hud_objectives.cpp): the screen y just below the rows painted now (the rows' top when there
	//! are none or the kill feed is hidden), and the right edge of the rows
	void GetRowsBottom(int &screenY, int &screenRight);

	void OnThink() override;
	void PaintBackground() override;
	void Paint() override;
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
	static constexpr int NAME_LEN = 128;  //!< wide chars per name (engine names are <= 31 bytes, plus the name color's codes; NPC names longer)
	static constexpr int WEAPON_LEN = 32; //!< wide chars of a weapon text label

	enum class EntryType : unsigned char
	{
		Other, //!< Other players
		Kill,  //!< Kill of this player
		Death, //!< Death of this player
	};

	struct Entry
	{
		wchar_t wszKiller[NAME_LEN] = {0};
		wchar_t wszVictim[NAME_LEN] = {0};
		wchar_t wszWeapon[WEAPON_LEN] = {0}; //!< "*" placeholder when hud.txt has no d_skull
		float flEndTime = 0;
		int iKillerLen = 0;
		int iKillerWide = 0;
		int iVictimLen = 0;
		int iVictimWide = 0;
		int iWeaponLen = 0;
		Color killerColor = Color(0, 0, 0, 0);
		Color victimColor = Color(0, 0, 0, 0);
		EntryType type = EntryType::Other;
		bool bIsTeamKill = false;
		bool bHeadshot = false;
		bool bColorCodes = false; //!< names contain ^0..^9 / ^xRGB / ^#RRGGBB to draw in colour (hud_colortext 1)
		int nSpriteIdx = -1;  //!< gHUD sprite index, -1 = text label (wszWeapon)
		int iIconWide = 0;    //!< sprite or weapon text width
		int iIconTall = 0;    //!< sprite height (0 for text)
	};

	static CHudDeathNoticePanel *m_sInstance;

	static constexpr int KILLFEED_COUNT = 6; // BHL: 6 rows (SBO legacy: 4)

	int m_HUD_d_skull = -1; // sprite index of skull icon
	int m_HUD_d_headshot = -1; // SBO 2.0: d_headshot (CS 1.6), -1: none in hud.txt -> "HS" text
	Entry m_Entries[KILLFEED_COUNT];
	int m_iEntryCount = 0;
	int m_iRowTall = 0;
	int m_iHeadshotWide = 0; //!< d_headshot sprite or "HS" text width (UpdateRowTall)
	float m_flLastTime = 0;

	CPanelAnimationVarAliasType(int, m_iHPadding, "hor_padding", "12", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iIconPadding, "icon_padding", "2", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iVMargin, "vert_margin", "4", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iRowHeight, "row_height", "13", "proportional_int");

	CPanelAnimationVar(vgui2::HFont, m_TextFont, "text_font", "Default");

	CPanelAnimationVar(Color, m_ColorBg, "color_bg", "0 0 0 64");
	CPanelAnimationVar(Color, m_ColorBgKill, "color_bg_kill", "255 119 0 96");
	CPanelAnimationVar(Color, m_ColorBgDeath, "color_bg_death", "255 0 0 96");

	CPanelAnimationVar(Color, m_ColorIcon, "color_icon", "255 80 0 255");
	CPanelAnimationVar(Color, m_ColorIconKill, "color_icon_kill", "255 80 0 255");
	CPanelAnimationVar(Color, m_ColorIconDeath, "color_icon_death", "241 237 212 255");
	CPanelAnimationVar(Color, m_ColorIconTK, "color_icon_teamkill", "10 240 10 255");

	// SBO: names without a team colour (VGUI2_KF_COLOR_NPC / _PLAIN)
	CPanelAnimationVar(Color, m_ColorNameNPC, "npc_name_color", "255 140 90 255");
	CPanelAnimationVar(Color, m_ColorNamePlain, "plain_name_color", "216 216 216 255");

	Color ResolveColor(int code) const;
	//! SBO 2.0 GM mode: "<GM>'s <monster>" with the GM's part in his colours (VGUI2_KF_COLOR_OWNED), else the name
	const char *OwnedName(const char *name, int colorCode, bool bColorCodes, char *buf, int bufLen) const;
	int GetEntryContentWide(const Entry &e) const;
	int GetTextWide(const wchar_t *str, int len, bool bColorCodes = false) const;
	int DrawKfText(int x, int y, const wchar_t *str, int len, Color c, bool bColorCodes = false);
	void UpdateRowTall();
	void RemoveExpired(float now);
};

#endif // SBO_VGUI2_HUD_DEATHNOTICE_H
