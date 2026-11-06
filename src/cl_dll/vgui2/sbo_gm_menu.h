//========= SBO VGUI2: Game Master mode guide =========//
//
// Purpose: the GM mode intro and help (ShowVGUIMenu 19; the plugin showed its intro as an MOTD): a guide in
//          the GM screens' look (the window and palette of the spawn and players
//          windows): one page of short lines after the plugin's intro text, the keys in it the player's own primary
//          keys ({command} in the text), the lines about points and tiers ([pts]) only when the budget plugin sends
//          points. Bottom row: Leave GM Mode (with Yes / No) and Start. The intro is shown over a black screen
//          (sbo_gm_spawn_menu.cpp CSboGmFade); closing it sends "gm" (the server's Game Master starts). The texts are
//          the #SBO_GM_Guide_* tokens of ui/resource/language/sbo_ui_<language>.txt.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_GM_MENU_H
#define SBO_VGUI2_SBO_GM_MENU_H

#include <string>
#include <vector>
#include "sbo_widgets.h"

class CSboGmMenu : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboGmMenu, CSboMenuPanel);

public:
	//! Creates the panel once (VGui2_Menus_Init).
	static CSboGmMenu *Create(vgui2::Panel *pParent);
	static CSboGmMenu *Get() { return s_pInstance; }

	void Open();

	virtual void Paint() override;
	virtual void PaintBackground() override;
	virtual void OnMousePressed(vgui2::MouseCode code) override;
	virtual void OnMouseWheeled(int delta) override;
	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override;

protected:
	virtual void LayoutMenu() override;

private:
	CSboGmMenu(vgui2::Panel *pParent);

	struct Rect
	{
		int x = 0, y = 0, w = 0, h = 0;
		bool Hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
	};

	void Finish();
	std::wstring GuideText() const; //!< the guide's text with the player's keys, without [pts] lines when no points
	void Button(const Rect &r, const wchar_t *label, bool primary, float px);

	static CSboGmMenu *s_pInstance;

	int m_iScroll = 0;
	bool m_bConfirmLeave = false;
	Rect m_rWin, m_rClose, m_rText, m_rStart, m_rLeave, m_rYes, m_rNo;
};

#endif // SBO_VGUI2_SBO_GM_MENU_H
