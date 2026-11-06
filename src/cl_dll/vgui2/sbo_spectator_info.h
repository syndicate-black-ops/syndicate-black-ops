//========= SBO VGUI2: spectator target info =========//
//
// Purpose: The box above the spectator panel's bottom bar that tells a spectator about the player being watched
//          (docs/SPECTATOR.md, "Target info"): health, armor, weapon in hand, legs, and the scoreboard's level / XP
//          (co-op), points, or the BR rounds won. The name is in the bottom bar already. Native replacement of the
//          metamod-spectator-info plugin, which printed the same as HUD text that it sent again on every change.
//
// Data:    health, armor, weapon, legs and BR rounds won from the server's "SpecHealth2" message
//          (sbo::GetSpectatorTargetInfo; sent only when the target or a value changes); level / points and XP are
//          what the client already has for the scoreboard (sbo::GetScorePlayer). Nothing is sent as text.
//
// A child of CSboSpectatorPanel, which asks it every frame (Update) and tells it where the bottom bar is
// (SetAnchor): shown only with the spectator bars, and only while sbo::GetSpectatorTargetInfo has a target (cvar
// spec_drawinfo, the spectator menu's Settings > Target Info, turns it off). Layout from
// ui/resource/spectatorinfo.res (640x480 proportional units), colours from ClientScheme (SpecInfo_*), texts from
// sbo_ui_<language>.txt (#SBO_SpecInfo_*, #SBO_Weapon_*).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SPECTATOR_INFO_H
#define SBO_VGUI2_SBO_SPECTATOR_INFO_H

#include <Color.h>
#include <vgui_controls/Panel.h>

class CSboSpectatorInfoPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboSpectatorInfoPanel, vgui2::Panel);

public:
	CSboSpectatorInfoPanel(vgui2::Panel *pParent);

	//! Reads the target's state; shows the box with it or hides it. Every frame while the spectator bars are up.
	void Update();
	//! Where the box goes: centred on centerX, its bottom edge "bottom_margin" above bottomY (the bottom bar's top),
	//! in the parent's coordinates.
	void SetAnchor(int centerX, int bottomY);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;
	virtual void Paint() override;

private:
	static constexpr int TEXT_LEN = 64;  //!< wide chars of a label or a value
	static constexpr int ROW_COUNT = 2;  //!< rows
	static constexpr int ROW_ITEMS = 3;  //!< "label value" pairs in a row, at most

	//! What the box shows; rebuilt only when this changes.
	struct State
	{
		int player;
		int health;
		int armor;
		int legsBroken;
		int scoreMode;   //!< SCORE_*
		int score;       //!< level (co-op), points, or BR rounds won
		int xp;
		char weapon[32];
	};

	struct Item
	{
		wchar_t wszLabel[TEXT_LEN] = {0};
		wchar_t wszValue[TEXT_LEN] = {0};
		int iLabelWide = 0;
		int iValueWide = 0;
		bool bWarning = false; //!< value in the warning colour (broken legs)
	};

	enum
	{
		SCORE_NONE = 0, //!< nothing (a co-op Game Master, whose score isn't a level)
		SCORE_LEVEL,    //!< co-op: level
		SCORE_LEVEL_XP, //!< co-op with the server's SboXP: level and XP
		SCORE_POINTS,   //!< Bounty Killer, teamplay: points (frags)
		SCORE_WON,      //!< BR: rounds won on this map
	};

	void Rebuild();
	void AddItem(int row, const char *labelToken, const wchar_t *value, bool bWarning = false);
	void AddNumberItem(int row, const char *labelToken, int value);
	void UpdateBounds();
	int GetRowWide(int row) const;
	int GetRowTall() const;
	int GetTextWide(const wchar_t *str) const;
	int DrawInfoText(int x, int y, const wchar_t *str, Color color);

	State m_State;
	bool m_bHasState = false;

	Item m_Items[ROW_COUNT][ROW_ITEMS];
	int m_iItemCount[ROW_COUNT] = {};

	int m_iCenterX = 0;
	int m_iBottomY = 0;

	// ui/resource/spectatorinfo.res
	CPanelAnimationVarAliasType(int, m_iBottomMargin, "bottom_margin", "4", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iHPadding, "hor_padding", "8", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iVPadding, "vert_padding", "3", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iRowHeight, "row_height", "12", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iItemSpacing, "item_spacing", "12", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iLabelSpacing, "label_spacing", "4", "proportional_int");

	CPanelAnimationVar(vgui2::HFont, m_TextFont, "text_font", "Default");

	CPanelAnimationVar(Color, m_ColorBg, "color_bg", "0 0 0 160");
	CPanelAnimationVar(Color, m_ColorBorder, "color_border", "59 58 34 207");
	CPanelAnimationVar(Color, m_ColorLabel, "color_label", "143 143 54 255");
	CPanelAnimationVar(Color, m_ColorValue, "color_value", "216 216 216 255");
	CPanelAnimationVar(Color, m_ColorWarning, "color_warning", "255 80 0 255");
};

#endif // SBO_VGUI2_SBO_SPECTATOR_INFO_H
