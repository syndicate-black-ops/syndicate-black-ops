//========= SBO VGUI2 port: briefing menu item button =========//
//
// Purpose: One choice of the briefing (equipment) menu (sbo_brief_menu.h), the VGUI2 version of what
//          vgui_SyndBrief.cpp builds for every item:
//
//            CommandButton( "", ..., bNoHighlight = true )      the cursor doesn't arm it
//            + CHandler_MenuButtonOver( this, iInfo )            entering it shows the item (CSyndBrief::SetActiveInfo)
//            + CMenuHandler_Blade/Light/... ( n, buttons... )    clicking it chooses it: armed = chosen
//
//          So the armed look (armed fg on armed bg of "Primary Button Text") marks the chosen item of a column and
//          follows only SetChosen(), never the cursor or a mouse press.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_BRIEF_ITEM_H
#define SBO_VGUI2_SBO_BRIEF_ITEM_H

#include "sbo_widgets.h"

//! What CHandler_MenuButtonOver calls: the menu that shows an item's picture and description.
class ISboBriefInfoTarget
{
public:
	//! CSyndBrief::SetActiveInfo: iInput = the item's number (1..32, see sbo_brief_menu.cpp).
	virtual void SetActiveInfo(int iInput) = 0;
};

class CSboBriefItemButton : public CSboCommandButton
{
	DECLARE_CLASS_SIMPLE(CSboBriefItemButton, CSboCommandButton);

public:
	//! command: sent to pActionTarget when clicked. iInfo: passed to pInfoTarget->SetActiveInfo on cursor enter.
	CSboBriefItemButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget, const char *command,
	    ISboBriefInfoTarget *pInfoTarget, int iInfo);

	//! VGUI1 setArmed( true / false ) by the CMenuHandler_<category> of the column.
	void SetChosen(bool bChosen);
	bool IsChosen() const { return m_bChosen; }

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnCursorEntered() override;
	virtual void OnCursorExited() override;
	virtual Color GetButtonFgColor() override;
	virtual Color GetButtonBgColor() override;

private:
	ISboBriefInfoTarget *m_pInfoTarget;
	int m_iInfo;
	bool m_bChosen = false;

	// CommandButton::paint / paintBackground: sc_primary1, armed sc_secondary2 on sc_primary2
	Color m_FgColor;
	Color m_ArmedFgColor;
	Color m_ArmedBgColor;
};

#endif // SBO_VGUI2_SBO_BRIEF_ITEM_H
