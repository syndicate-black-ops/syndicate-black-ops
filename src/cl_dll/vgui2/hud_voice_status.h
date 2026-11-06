//========= SBO VGUI2 port =========//
//
// Purpose: VGUI2 voice labels: the players who are talking, each with his Steam avatar. Port of BugfixedHL-Rebased's
//          CHudVoiceStatus (src/game/client/hud/voice_status.{h,cpp}, Valve CS:S code); see
//          docs/dev/CHAT_PORT_INTEGRATION.md "VGUI2 voice labels". The local player's own row is the "you are
//          talking" feedback: BHL's separate CHudVoiceStatusSelf icon and the per-row speaker icon are left out.
//
// SDK world: hl1_source_sdk headers + sbo_bridge.h / vgui2_api.h only.
//=================================================================//

#ifndef SBO_VGUI2_HUD_VOICE_STATUS_H
#define SBO_VGUI2_HUD_VOICE_STATUS_H

#include <Color.h>
#include <tier1/utllinkedlist.h>
#include <vgui_controls/Panel.h>

class CAvatarImage;

//=============================================================================
// Icons for other players using voice
//=============================================================================
class CHudVoiceStatus : public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudVoiceStatus, vgui2::Panel);

	static CHudVoiceStatus *Get() { return m_sInstance; }

	CHudVoiceStatus(vgui2::Panel *pParent);
	~CHudVoiceStatus(void);

	virtual void Paint() override;
	virtual void OnThink() override;
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

	//! BHL CHudVoiceStatus::Init: forget all speakers.
	void ClearActiveList();

	//! BHL CHudVoiceStatus::RunFrame (BHL: from HUD_Frame; SBO: OnThink, the panel is always visible).
	void RunFrame(float fTime);

protected:
	int FindActiveSpeaker(int playerId);

private:
	static CHudVoiceStatus *m_sInstance;

	Color m_NoTeamColor;
	double m_flLastThinkTime = 0;

	struct ActiveSpeaker
	{
		int playerId;
		CAvatarImage *pAvatar;
		class CTGAImage *pFile; //!< SBO: the slot's userinfo sbo_avatar picture (CScorePanel::GetFileAvatar), else nullptr
		bool bSpeaking;
		float fAlpha;
		float fAlphaMultiplier;
	};

	CUtlLinkedList<ActiveSpeaker> m_SpeakingList;

	//CPanelAnimationVar(vgui2::HFont, m_NameFont, "Default", "Default");
	vgui2::HFont m_NameFont = 0;

	CPanelAnimationVarAliasType(float, item_tall, "item_tall", "16", "proportional_float");
	CPanelAnimationVarAliasType(float, item_wide, "item_wide", "160", "proportional_float");
	CPanelAnimationVarAliasType(float, item_spacing, "item_spacing", "2", "proportional_float");

	CPanelAnimationVarAliasType(bool, show_avatar, "show_avatar", "0", "bool");
	CPanelAnimationVarAliasType(bool, show_friend, "show_friend", "1", "bool");
	// SBO defaults = voicestatus.res: avatar 1 unit inside the left edge, name 7 units right of it (BHL defaults:
	// avatar_xpos 16, 16x16, text_xpos 40 behind the speaker icon)
	CPanelAnimationVarAliasType(float, avatar_ypos, "avatar_ypos", "1", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_xpos, "avatar_xpos", "1", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_tall, "avatar_tall", "14", "proportional_float");
	CPanelAnimationVarAliasType(float, avatar_wide, "avatar_wide", "14", "proportional_float");

	// BHL's speaker icon per row (show_voice_icon, icon_xpos, ..., ui/gfx/hud/speaker.tga) is left out: the row is
	// [avatar] name. BHL declares the dead icon (show_dead_icon, dead_xpos, ...) but never draws it: left out.

	CPanelAnimationVarAliasType(float, text_xpos, "text_xpos", "22", "proportional_float");

	CPanelAnimationVarAliasType(float, fade_in_time, "fade_in_time", "0.0", "float");
	CPanelAnimationVarAliasType(float, fade_out_time, "fade_out_time", "0.0", "float");
};

//! BHL HudLayout.res: ui/resource/voicestatus.res holds the HudVoiceStatus entry.
//! Applies the panel's entry (by panel name) if the file has one.
void VGui2_ApplyVoiceStatusLayout(vgui2::Panel *pPanel);

#endif // SBO_VGUI2_HUD_VOICE_STATUS_H
