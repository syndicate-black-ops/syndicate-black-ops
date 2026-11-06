//========= SBO VGUI2 port: spectator UI =========//
//
// Purpose: see sbo_spectator_panel.h.
//=================================================================//

#include <algorithm>
#include <string.h>
#include <wchar.h>

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "sbo_bridge.h"
#include "sbo_command_menu.h"
#include "sbo_menus.h"
#include "sbo_spectator_info.h"
#include "sbo_spectator_panel.h"
#include "vgui2_api.h"

namespace
{
// vgui_SpectatorPanel.cpp
constexpr int PANEL_HEIGHT = 32;
constexpr int OPTIONS_BUTTON_X = 96;
constexpr int CAMOPTIONS_BUTTON_X = 200;

// ColorButton colours of every spectator button (VGUI1 alpha)
const Color BUTTON_TEXT = sboui::InvAlphaColor(143, 143, 54, 0);
const Color BUTTON_TEXT_ARMED = sboui::InvAlphaColor(194, 202, 54, 0);
const Color BUTTON_BORDER = sboui::InvAlphaColor(59, 58, 34, 48);
const Color BUTTON_BORDER_ARMED = sboui::InvAlphaColor(194, 202, 54, 0);

// m_ExtraInfo / m_CurrentTime: "Team Info Text", setFgColor( 143, 143, 54, 0 )
const char *const STATUS_FONT = "Team Info Text";
const Color STATUS_TEXT = sboui::InvAlphaColor(143, 143, 54, 0);

// Copies src into dst (size n); true if the text changed.
bool UpdateString(char *dst, size_t n, const char *src)
{
	if (!src)
		src = "";
	if (!strncmp(dst, src, n - 1))
		return false;
	strncpy(dst, src, n - 1);
	dst[n - 1] = 0;
	return true;
}

// Single-line texts built from titles.txt strings ("<#Spec_Map>: <map>", "%d:%02d\n"): titles.txt texts end with the
// line break before the closing brace, which VGUI1 labels ignored. Kept, it made the status two lines tall and
// pushed the timer row out of the top bar.
bool UpdateLine(char *dst, size_t n, const char *src)
{
	char line[128];
	size_t j = 0;
	for (const char *s = src ? src : ""; *s && j < sizeof(line) - 1; s++)
	{
		if (*s != '\n' && *s != '\r')
			line[j++] = *s;
	}
	while (j > 0 && line[j - 1] == ' ')
		j--;
	line[j] = 0;
	return UpdateString(dst, n, line);
}

void TextSize(const char *pszScheme, const char *text, int &wide, int &tall)
{
	wide = tall = 0;
	vgui2::HFont font = sboui::GetTextSchemeFont(pszScheme);
	if (font == vgui2::INVALID_FONT)
		return;

	wchar_t wbuf[256];
	sboui::TextToWide(text, wbuf, sizeof(wbuf));
	vgui2::surface()->GetTextSize(font, wbuf, wide, tall);
	if (!wbuf[0])
		tall = vgui2::surface()->GetFontTall(font);
}
} // namespace

//-----------------------------------------------------------------------------
// VGUI1 Panel with a background colour (m_Separator: setBgColor( 59, 58, 34, 48 )).
//-----------------------------------------------------------------------------
class CSboFilledPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboFilledPanel, vgui2::Panel);

public:
	CSboFilledPanel(vgui2::Panel *pParent, const char *pName, Color color)
	    : BaseClass(pParent, pName)
	    , m_Color(color)
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(false);
	}

	virtual void PaintBackground() override
	{
		int wide, tall;
		GetSize(wide, tall);
		vgui2::surface()->DrawSetColor(m_Color);
		vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	}

private:
	Color m_Color;
};

//-----------------------------------------------------------------------------
// CSboSpectatorPanel
//-----------------------------------------------------------------------------
CSboSpectatorPanel *CSboSpectatorPanel::s_pInstance = nullptr;

CSboSpectatorPanel *CSboSpectatorPanel::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboSpectatorPanel(pParent);
	return s_pInstance;
}

CSboSpectatorPanel::CSboSpectatorPanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboSpectatorPanel")
{
	SetBackdropTrans(0); // SpectatorPanel: setPaintBackgroundEnabled( false )
	SetHidesCenterText(false); // the spectator bar is up while spectating: center text stays

	// SpectatorPanel::Initialize, in its creation order (= drawing order)
	m_pTopBar = new CSboTransparentPanel(this, "TopBar", 64);
	m_pBottomBar = new CSboTransparentPanel(this, "BottomBar", 64);

	m_pExtraInfo = new CSboLabel(m_pTopBar, "ExtraInfo", STATUS_FONT, STATUS_TEXT);
	m_pTimerImage = new CSboImagePanel(m_pTopBar, "Timer"); // "timer", loaded when shown (resolution known)
	m_pTimerImage->SetMouseInputEnabled(false);
	m_pBanner = new CSboImagePanel(this, "Banner");
	m_pBanner->SetMouseInputEnabled(false);
	m_pCurrentTime = new CSboLabel(m_pTopBar, "CurrentTime", STATUS_FONT, STATUS_TEXT);
	m_pSeparator = new CSboFilledPanel(m_pTopBar, "Separator", sboui::InvAlphaColor(59, 58, 34, 48));
	// m_TeamScores: "   " labels, nothing to draw

	m_pOptionButton = new CSboColorButton(m_pBottomBar, "OptionButton", this, "options");
	m_pCamButton = new CSboColorButton(m_pBottomBar, "CamButton", this, "camera");
	m_pPrevButton = new CSboColorButton(m_pBottomBar, "PrevPlayerButton", this, "prevplayer");
	m_pPrevButton->SetText(L"<");
	m_pNextButton = new CSboColorButton(m_pBottomBar, "NextPlayerButton", this, "nextplayer");
	m_pNextButton->SetText(L">");

	// default font, centred, LineBorder( Color( 59, 58, 34, 48 ) ); the colour comes with the state
	m_pBottomLabel = new CSboLabel(m_pBottomBar, "BottomLabel", sboui::DEFAULT_TEXT_SCHEME, BUTTON_TEXT);
	m_pBottomLabel->SetLabelAlignment(vgui2::Label::a_center);
	m_pBottomLabel->SetLineBorder(sboui::InvAlphaColor(59, 58, 34, 48));

	m_pInsetButton = new CSboColorButton(this, "InsetViewButton", this, "inset");

	// SBO 2.0: the watched player's state. Before the command menus (sbo_command_menu_panel.cpp: children of this
	// panel, created when first opened), which so draw over it; it shows and hides itself (OnThink).
	m_pTargetInfo = new CSboSpectatorInfoPanel(this);

	CSboColorButton *buttons[] = { m_pOptionButton, m_pCamButton, m_pPrevButton, m_pNextButton, m_pInsetButton };
	for (CSboColorButton *pButton : buttons)
		pButton->SetColors(BUTTON_TEXT, BUTTON_TEXT_ARMED, BUTTON_BORDER, BUTTON_BORDER_ARMED);

	m_pOptionButton->SetVisible(false);
	m_pCamButton->SetVisible(false);
	m_pPrevButton->SetVisible(false);
	m_pNextButton->SetVisible(false);
	m_pInsetButton->SetVisible(false);
	m_pBanner->SetVisible(false);
}

void CSboSpectatorPanel::SetState(const SboSpectatorState &state)
{
	if (!state.visible)
	{
		if (m_bShown)
		{
			m_bShown = false;
			HideMenu(); // invisible, no mouse input
			VGui2_CommandMenu_Changed(); // an open spectator command menu goes with the bars
		}
		return;
	}

	if (UpdateString(m_szOptions, sizeof(m_szOptions), state.optionsText))
		m_pOptionButton->SetPlainText(m_szOptions);
	if (UpdateString(m_szCam, sizeof(m_szCam), state.camText))
		m_pCamButton->SetPlainText(m_szCam);
	if (UpdateString(m_szBottom, sizeof(m_szBottom), state.bottomText))
		m_pBottomLabel->SetPlainText(m_szBottom);
	if (UpdateLine(m_szExtraInfo, sizeof(m_szExtraInfo), state.extraInfo))
		m_pExtraInfo->SetPlainText(m_szExtraInfo);
	if (UpdateLine(m_szCurrentTime, sizeof(m_szCurrentTime), state.currentTime))
		m_pCurrentTime->SetPlainText(m_szCurrentTime);
	const bool bNewBanner = UpdateString(m_szBanner, sizeof(m_szBanner), state.bannerImage);
	if (bNewBanner && m_bShown)
		m_pBanner->SetImage(m_szBanner); // DRC_CMD_BANNER: CImageLabel::LoadImage

	m_pBottomLabel->SetColor(
	    Color(state.bottomColor[0], state.bottomColor[1], state.bottomColor[2], state.bottomColor[3]));

	// SpectatorPanel::ShowMenu: setArmed( false ) on the buttons it shows / hides
	if (state.menuVisible != m_bMenuVisible)
	{
		m_pOptionButton->SetArmed(false);
		m_pCamButton->SetArmed(false);
		m_pPrevButton->SetArmed(false);
		m_pNextButton->SetArmed(false);
	}

	m_bMenuVisible = state.menuVisible;
	m_bInsetVisible = state.insetVisible;
	m_iInset[0] = state.insetX;
	m_iInset[1] = state.insetY;
	m_iInset[2] = state.insetWide;
	m_iInset[3] = state.insetTall;
	m_bStatusVisible = state.statusVisible;
	m_bBannerVisible = state.bannerVisible;

	if (!m_bShown)
	{
		// not ShowMenu: no MoveToFront (menus, scoreboard and chat stay above the bars, as with VGUI1) and no mouse
		// input unless the duck menu is up
		m_bShown = true;
		UpdateScreenBounds();
		// TGAs for the current resolution (gfx/vgui/<320|640>_*.tga)
		m_pTimerImage->SetImage("timer");
		m_pBanner->SetImage(m_szBanner);
		InvalidateLayout(false, true); // the VGUI1 fonts (re)applied
		SetVisible(!m_bSuppressed);
	}

	LayoutMenu();
	UpdateMouseInput();
	VGui2_CommandMenu_Changed(); // an open spectator command menu: shown with the bars, laid out again
}

void CSboSpectatorPanel::UpdateMouseInput()
{
	const bool bWant = m_bShown && m_bMenuVisible;
	if (bWant == IsMouseInputEnabled())
		return;

	SetMouseInputEnabled(bWant);
	if (bWant)
		EnableControlsMouseInput(this);
	vgui2::surface()->CalculateMouseVisible();
}

void CSboSpectatorPanel::LayoutMenu()
{
	using sboui::XRes;
	using sboui::YRes;

	int sw, sh;
	sbo::GetScreenSize(sw, sh);

	// SpectatorPanel::EnableInsetView: a shorter top bar right of the inset window, its frame is a button
	const int offset = m_bInsetVisible ? m_iInset[0] + m_iInset[2] + 2 : 0;
	if (m_bInsetVisible)
	{
		m_pTopBar->SetBounds(XRes(offset), 0, XRes(640 - offset), YRes(PANEL_HEIGHT));
		m_pInsetButton->SetBounds(XRes(m_iInset[0]), YRes(m_iInset[1]), XRes(m_iInset[2]), YRes(m_iInset[3]));
	}
	else
		m_pTopBar->SetBounds(0, 0, sw, YRes(PANEL_HEIGHT));
	m_pInsetButton->SetVisible(m_bInsetVisible);

	// banner (HLTV): CImageLabel::LoadImage sizes it XRES( image wide ) x YRES( image tall ), drawn unscaled
	m_pBanner->SetVisible(m_bBannerVisible);
	if (m_bBannerVisible)
	{
		int imageWide, imageTall;
		m_pBanner->GetImageSize(imageWide, imageTall);
		m_pBanner->SetBounds(m_bInsetVisible ? XRes(offset) : 0, 0, XRes(imageWide), YRes(imageTall));
	}

	// bottom bar (SpectatorPanel::Initialize)
	m_pBottomBar->SetBounds(0, sh - YRes(32), sw, YRes(PANEL_HEIGHT));
	m_pOptionButton->SetBounds(XRes(15), YRes(6), XRes(OPTIONS_BUTTON_X), YRes(20));
	m_pCamButton->SetBounds(sw - (XRes(CAMOPTIONS_BUTTON_X) + 15), YRes(6), XRes(CAMOPTIONS_BUTTON_X), YRes(20));
	m_pPrevButton->SetBounds(XRes(15 + OPTIONS_BUTTON_X + 15), YRes(6), XRes(24), YRes(20));
	m_pNextButton->SetBounds((sw - (XRes(CAMOPTIONS_BUTTON_X) + 15)) - XRes(24 + 15), YRes(6), XRes(24), YRes(20));

	const float flLabelSize = ((sw - (XRes(CAMOPTIONS_BUTTON_X) + 15)) - XRes(24 + 15))
	    - XRes((15 + OPTIONS_BUTTON_X + 15) + 38);
	const int labelWide = (int)flLabelSize;
	m_pBottomLabel->SetSize(labelWide, YRes(20));

	// SpectatorPanel::ShowMenu: the buttons only with the menu; the label moves to the middle without it
	m_pOptionButton->SetVisible(m_bMenuVisible);
	m_pCamButton->SetVisible(m_bMenuVisible);
	m_pPrevButton->SetVisible(m_bMenuVisible);
	m_pNextButton->SetVisible(m_bMenuVisible);
	if (m_bMenuVisible)
		m_pBottomLabel->SetPos(XRes((15 + OPTIONS_BUTTON_X + 15) + 31), YRes(6));
	else
		m_pBottomLabel->SetPos((sw / 2) - (labelWide / 2), YRes(6));

	// the target info box sits on the bottom bar, centred like the bar's label without the menu
	m_pTargetInfo->SetAnchor(sw / 2, sh - YRes(32));

	LayoutStatus(sw, offset);
}

void CSboSpectatorPanel::OnThink()
{
	BaseClass::OnThink();

	// Every frame while the bars are up (an invisible panel doesn't think): the target and its state change with the
	// client data and the server's SpecHealth2 message, not with the VGUI1 panel's updates (SetState).
	if (m_bShown)
		m_pTargetInfo->Update();
}

void CSboSpectatorPanel::LayoutStatus(int sw, int offset)
{
	using sboui::XRes;
	using sboui::YRes;

	// SpectatorPanel::Update (positions relative to the top bar, like the VGUI1 children of m_TopBorder)
	m_pExtraInfo->SetVisible(m_bStatusVisible);
	m_pTimerImage->SetVisible(m_bStatusVisible);
	m_pCurrentTime->SetVisible(m_bStatusVisible);
	m_pSeparator->SetVisible(m_bStatusVisible);
	if (!m_bStatusVisible)
		return;

	int iTextWidth, iTextHeight;
	int iTimeWidth, iTimeHeight;
	TextSize(STATUS_FONT, m_szExtraInfo, iTextWidth, iTextHeight);
	TextSize(STATUS_FONT, m_szCurrentTime, iTimeWidth, iTimeHeight);

	// The timer row: the icon's left edge under the title's, the time a fixed gap right of the icon, the icon centred
	// on the time's line. From the image's own size: the TGA is drawn unscaled (16 pixels at every resolution from
	// 1024 up), so the VGUI1 numbers (a XRES( 14 ) x YRES( 14 ) box at the top of the line, the time XRES( 15 )
	// right of it) left the icon above the digits and a gap that grew with the resolution.
	int iIconWidth, iIconHeight;
	m_pTimerImage->GetImageSize(iIconWidth, iIconHeight);
	if (iIconWidth <= 1) // no image for this resolution: the time starts under the title
		iIconWidth = iIconHeight = 0;
	const int iIconGap = iIconWidth ? std::max(2, iIconWidth / 4) : 0;
	iTimeWidth += iIconWidth + iIconGap; // +timer icon
	iTimeWidth += (4 - (iTimeWidth % 4));

	if (iTimeWidth > iTextWidth)
		iTextWidth = iTimeWidth;

	int xPos = sw - (iTextWidth + XRes(4 + offset));
	const int yTime = YRes(2) + iTextHeight;

	m_pExtraInfo->SetBounds(xPos, YRes(1), iTextWidth, iTextHeight);
	m_pTimerImage->SetBounds(xPos, yTime + (iTimeHeight - iIconHeight) / 2, iIconWidth, iIconHeight);
	m_pCurrentTime->SetBounds(xPos + iIconWidth + iIconGap, yTime, iTextWidth - (iIconWidth + iIconGap), iTimeHeight);
	m_pSeparator->SetBounds(sw - (iTextWidth + XRes(4 + 2 + 4 + offset)), YRes(1), XRes(4), YRes(PANEL_HEIGHT - 2));
}

void CSboSpectatorPanel::OnCommand(const char *command)
{
	// SpectatorPanel::ActionSignal (CSpectatorHandler_Command)
	if (!stricmp(command, "options"))
		sbo::SpectatorPanelAction(sbo::SPECTATOR_ACTION_OPTIONS);
	else if (!stricmp(command, "camera"))
		sbo::SpectatorPanelAction(sbo::SPECTATOR_ACTION_CAMERA);
	else if (!stricmp(command, "prevplayer"))
		sbo::SpectatorPanelAction(sbo::SPECTATOR_ACTION_PREVPLAYER);
	else if (!stricmp(command, "nextplayer"))
		sbo::SpectatorPanelAction(sbo::SPECTATOR_ACTION_NEXTPLAYER);
	else if (!stricmp(command, "inset"))
		sbo::SpectatorPanelAction(sbo::SPECTATOR_ACTION_TOGGLE_INSET);
	else
		BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_Spectator_SetState(const SboSpectatorState &state)
{
	if (CSboSpectatorPanel::Get())
		CSboSpectatorPanel::Get()->SetState(state);
}

void CSboSpectatorPanel::SetSuppressed(bool bSuppressed)
{
	if (m_bSuppressed == bSuppressed)
		return;
	m_bSuppressed = bSuppressed;
	if (m_bShown)
		SetVisible(!bSuppressed);
}

void VGui2_Spectator_SetSuppressed(bool bSuppressed)
{
	if (CSboSpectatorPanel::Get())
		CSboSpectatorPanel::Get()->SetSuppressed(bSuppressed);
}
