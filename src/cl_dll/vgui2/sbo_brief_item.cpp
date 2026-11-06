//========= SBO VGUI2 port: briefing menu item button =========//
//
// Purpose: see sbo_brief_item.h.
//=================================================================//

#include "sbo_bridge.h"
#include "sbo_brief_item.h"
#include "sbo_menus.h"

CSboBriefItemButton::CSboBriefItemButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget,
    const char *command, ISboBriefInfoTarget *pInfoTarget, int iInfo)
    : BaseClass(pParent, pName, pActionTarget, command)
    , m_pInfoTarget(pInfoTarget)
    , m_iInfo(iInfo)
    , m_FgColor(sboui::SBO_GREEN)
    , m_ArmedFgColor(0, 0, 0, 255)
    , m_ArmedBgColor(sboui::SBO_GREEN)
{
}

void CSboBriefItemButton::SetChosen(bool bChosen)
{
	if (m_bChosen == bChosen)
		return;

	m_bChosen = bChosen;
	InvalidateLayout(false); // Button::PerformLayout applies GetButtonFgColor / GetButtonBgColor
	Repaint();
}

void CSboBriefItemButton::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	sbo::TextSchemeInfo info;
	if (sbo::GetTextSchemeInfo(sboui::DEFAULT_TEXT_SCHEME, info))
	{
		m_FgColor = Color(info.fg[0], info.fg[1], info.fg[2], info.fg[3]);
		m_ArmedFgColor = Color(info.armedFg[0], info.armedFg[1], info.armedFg[2], info.armedFg[3]);
		m_ArmedBgColor = Color(info.armedBg[0], info.armedBg[1], info.armedBg[2], info.armedBg[3]);
	}
}

void CSboBriefItemButton::OnCursorEntered()
{
	// bNoHighlight: no arming; CHandler_MenuButtonOver::cursorEntered -> SetActiveInfo
	if (m_pInfoTarget)
		m_pInfoTarget->SetActiveInfo(m_iInfo);
}

void CSboBriefItemButton::OnCursorExited()
{
	// bNoHighlight: nothing happens when the cursor leaves
}

Color CSboBriefItemButton::GetButtonFgColor()
{
	return m_bChosen ? m_ArmedFgColor : m_FgColor;
}

Color CSboBriefItemButton::GetButtonBgColor()
{
	return m_bChosen ? m_ArmedBgColor : Color(0, 0, 0, 0);
}
