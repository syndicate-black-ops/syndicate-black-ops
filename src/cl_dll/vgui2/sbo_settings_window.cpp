//========= SBO VGUI2: windows and setting rows in the server browser's look =========//
//
// Purpose: see sbo_settings_window.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include <FileSystem.h>
#include <IEngineVGui.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IVGui.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_settings_window.h"
#include "sbo_ui_style.h"
#include "sbo_widgets.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
// the windows' places, kept between games in the mod folder (as serverbrowser.vdf); no cvar
constexpr const char SETTINGS_FILE[] = "sbomenus.vdf";
constexpr const char SETTINGS_PATH_ID[] = "GAMECONFIG";

KeyValues *s_pSaved = nullptr;

KeyValues *Saved()
{
	if (!s_pSaved)
	{
		s_pSaved = new KeyValues("SboMenus");
		s_pSaved->LoadFromFile(g_pFullFileSystem, SETTINGS_FILE, SETTINGS_PATH_ID);
	}
	return s_pSaved;
}

float CvarFloat(const char *name)
{
	sbo::CvarHandle h = sbo::FindCvar(name);
	return h ? sbo::CvarGetFloat(h) : 0.0f;
}

std::string CvarString(const char *name)
{
	sbo::CvarHandle h = sbo::FindCvar(name);
	return h ? sbo::CvarGetString(h) : "";
}

bool FindMenuIn(vgui2::VPANEL panel, int depth, int &x, int &y, int &w, int &h)
{
	if (!panel || depth > 5 || !vgui2::ipanel()->IsVisible(panel))
		return false;
	const char *name = vgui2::ipanel()->GetName(panel);
	const char *cls = vgui2::ipanel()->GetClassName(panel);
	if ((name && Q_stristr(name, "GameMenu")) || (cls && Q_stristr(cls, "GameMenu")))
	{
		vgui2::ipanel()->GetAbsPos(panel, x, y);
		vgui2::ipanel()->GetSize(panel, w, h);
		return w > 0 && h > 0;
	}
	const int count = vgui2::ipanel()->GetChildCount(panel);
	for (int i = 0; i < count; i++)
	{
		if (FindMenuIn(vgui2::ipanel()->GetChild(panel, i), depth + 1, x, y, w, h))
			return true;
	}
	return false;
}
} // namespace

bool SboFindGameMenuBounds(int &x, int &y, int &w, int &h)
{
	return g_pEngineVGui && FindMenuIn(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), 0, x, y, w, h);
}

//-----------------------------------------------------------------------------
// CSboSlider
//-----------------------------------------------------------------------------
CSboSlider::CSboSlider(vgui2::Panel *pParent, const char *pName, float min, float max)
    : BaseClass(pParent, pName)
    , m_flMin(min)
    , m_flMax(max > min ? max : min + 1.0f)
    , m_flValue(min)
{
	SetMouseInputEnabled(true);
}

void CSboSlider::SetValue(float value)
{
	m_flValue = std::max(m_flMin, std::min(m_flMax, value));
	Repaint();
}

void CSboSlider::SetFromX(int x)
{
	const int knob = S().M(8);
	const int w = std::max(1, GetWide() - knob);
	float f = (float)(x - knob / 2) / (float)w;
	f = std::max(0.0f, std::min(1.0f, f));
	const float v = m_flMin + f * (m_flMax - m_flMin);
	if (v != m_flValue)
	{
		m_flValue = v;
		Repaint();
		PostActionSignal(new KeyValues("SliderMoved"));
	}
}

void CSboSlider::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT || !IsEnabled())
		return;
	m_bDragging = true;
	vgui2::input()->SetMouseCapture(GetVPanel());
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	SetFromX(x);
}

void CSboSlider::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_bDragging)
	{
		m_bDragging = false;
		vgui2::input()->SetMouseCapture(0);
	}
}

void CSboSlider::OnCursorMoved(int x, int y)
{
	if (m_bDragging)
		SetFromX(x);
}

void CSboSlider::Paint()
{
	const int w = GetWide(), h = GetTall(), knob = S().M(8), track = std::max(2, S().M(4));
	const float f = (m_flValue - m_flMin) / (m_flMax - m_flMin);
	const int kx = (int)(f * (w - knob));
	FillRect(knob / 2, (h - track) / 2, w - knob, track, COL_BG_INPUT);
	OutlineRect(knob / 2, (h - track) / 2, w - knob, track, COL_BORDER);
	FillRect(knob / 2, (h - track) / 2, kx, track, IsEnabled() ? COL_BG_PRIMARY : COL_BORDER_LIST);
	const int kh = std::min(h, S().M(16));
	FillRect(kx, (h - kh) / 2, knob, kh, IsEnabled() ? COL_BORDER_PRIMARY : COL_BORDER);
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------
CSboCvarToggle::CSboCvarToggle(vgui2::Panel *pPage, const char *text, const char *cvar, float onValue, float offValue)
    : m_Cvar(cvar)
    , m_flOn(onValue)
    , m_flOff(offValue)
{
	CSboBrowserCheck *pCheck = new CSboBrowserCheck(pPage, "Check", text);
	pCheck->AddActionSignalTarget(pPage);
	control = pCheck;
	rowTall = 26;
}

void CSboCvarToggle::Reset()
{
	const float v = CvarFloat(m_Cvar.c_str());
	// offValue 0: on = any value but 0 (GameUI's toggle); otherwise on = the on value
	m_bReset = m_flOff == 0.0f ? v != 0.0f : v == m_flOn;
	static_cast<CSboBrowserCheck *>(control)->SetChecked(m_bReset);
}

bool CSboCvarToggle::Changed()
{
	return static_cast<CSboBrowserCheck *>(control)->IsChecked() != m_bReset;
}

void CSboCvarToggle::Apply()
{
	const bool on = static_cast<CSboBrowserCheck *>(control)->IsChecked();
	sbo::CvarSetFloat(m_Cvar.c_str(), on ? m_flOn : m_flOff); // Cvar_SetValue, as GameUI
	m_bReset = on;
}

CSboCvarNegate::CSboCvarNegate(vgui2::Panel *pPage, const char *text, const char *cvar)
    : m_Cvar(cvar)
{
	CSboBrowserCheck *pCheck = new CSboBrowserCheck(pPage, "Check", text);
	pCheck->AddActionSignalTarget(pPage);
	control = pCheck;
	rowTall = 26;
}

void CSboCvarNegate::Reset()
{
	m_bReset = CvarFloat(m_Cvar.c_str()) < 0.0f;
	static_cast<CSboBrowserCheck *>(control)->SetChecked(m_bReset);
}

bool CSboCvarNegate::Changed()
{
	return static_cast<CSboBrowserCheck *>(control)->IsChecked() != m_bReset;
}

void CSboCvarNegate::Apply()
{
	const bool on = static_cast<CSboBrowserCheck *>(control)->IsChecked();
	float v = fabsf(CvarFloat(m_Cvar.c_str()));
	if (v < 1e-5f)
		v = 0.022f; // GameUI's CCvarNegateCheckButton
	sbo::CvarSetFloat(m_Cvar.c_str(), on ? -v : v);
	m_bReset = on;
}

namespace
{
//! A slider with its value as text right of it
class CSboValueSlider : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboValueSlider, vgui2::Panel);

public:
	CSboValueSlider(vgui2::Panel *pPage, float min, float max, const char *format)
	    : BaseClass(pPage, "ValueSlider")
	    , m_Format(format)
	{
		m_pSlider = new CSboSlider(this, "Slider", min, max);
		m_pSlider->AddActionSignalTarget(this);
		m_pPage = pPage;
	}
	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		const int textW = S().M(48);
		m_pSlider->SetBounds(0, 0, std::max(10, GetWide() - textW - S().M(8)), GetTall());
	}
	void PaintBackground() override {}
	void PaintBorder() override {}
	void Paint() override
	{
		char buf[32];
		if (m_Format == "percent") // 0..1 as a percentage
			snprintf(buf, sizeof(buf), "%.0f%%", m_pSlider->GetValue() * 100.0f);
		else
			snprintf(buf, sizeof(buf), m_Format.c_str(), m_pSlider->GetValue());
		const std::wstring w = Widen(buf);
		DrawTextMid(S().body, COL_TEXT, GetWide() - TextWide(S().body, w), 0, GetTall(), w);
	}
	MESSAGE_FUNC(OnSliderMoved, "SliderMoved")
	{
		Repaint();
		vgui2::ivgui()->PostMessage(m_pPage->GetVPanel(), new KeyValues("SliderMoved"), GetVPanel());
	}
	CSboSlider *m_pSlider;
	vgui2::Panel *m_pPage;
	std::string m_Format;
};
} // namespace

vgui2::Panel *SboMakeValueSlider(vgui2::Panel *pPage, float min, float max, const char *format)
{
	return new CSboValueSlider(pPage, min, max, format);
}

CSboSlider *SboValueSliderOf(vgui2::Panel *pValueSlider)
{
	return static_cast<CSboValueSlider *>(pValueSlider)->m_pSlider;
}

CSboCvarSlider::CSboCvarSlider(vgui2::Panel *pPage, const char *text, const char *cvar, float min, float max,
    const char *format)
    : m_Cvar(cvar)
{
	label = text;
	control = new CSboValueSlider(pPage, min, max, format);
	rowTall = 28;
}

CSboSlider *CSboCvarSlider::Slider()
{
	return static_cast<CSboValueSlider *>(control)->m_pSlider;
}

void CSboCvarSlider::Reset()
{
	m_flReset = CvarFloat(m_Cvar.c_str());
	Slider()->SetValue(m_flReset);
	m_flReset = Slider()->GetValue(); // clamped: an out-of-range cvar is not "changed" by showing it
	control->Repaint();
}

bool CSboCvarSlider::Changed()
{
	return Slider()->GetValue() != m_flReset;
}

void CSboCvarSlider::Apply()
{
	sbo::CvarSetFloat(m_Cvar.c_str(), Slider()->GetValue());
	m_flReset = Slider()->GetValue();
}

CSboCvarChoice::CSboCvarChoice(vgui2::Panel *pPage, const char *text, const char *cvar, const std::vector<float> &values,
    const std::vector<std::string> &labels)
    : m_Cvar(cvar)
    , m_Values(values)
{
	label = text;
	CSboBrowserSelect *pSelect = new CSboBrowserSelect(pPage, "Choice");
	for (const std::string &l : labels)
		pSelect->AddItem(l.c_str());
	pSelect->AddActionSignalTarget(pPage);
	control = pSelect;
}

void CSboCvarChoice::Reset()
{
	const float v = CvarFloat(m_Cvar.c_str());
	int best = 0;
	for (int i = 0; i < (int)m_Values.size(); i++)
	{
		if (fabsf(m_Values[i] - v) < fabsf(m_Values[best] - v))
			best = i;
	}
	m_iReset = best;
	static_cast<CSboBrowserSelect *>(control)->SetSelected(best);
}

bool CSboCvarChoice::Changed()
{
	return static_cast<CSboBrowserSelect *>(control)->GetSelected() != m_iReset;
}

void CSboCvarChoice::Apply()
{
	const int i = static_cast<CSboBrowserSelect *>(control)->GetSelected();
	if (i >= 0 && i < (int)m_Values.size())
		sbo::CvarSetFloat(m_Cvar.c_str(), m_Values[i]);
	m_iReset = i;
}

CSboCommandChoice::CSboCommandChoice(vgui2::Panel *pPage, const char *text, const std::vector<std::string> &labels,
    const std::vector<std::string> &commands, std::function<int()> current)
    : m_Commands(commands)
    , m_Current(current)
{
	label = text;
	CSboBrowserSelect *pSelect = new CSboBrowserSelect(pPage, "Choice");
	for (const std::string &l : labels)
		pSelect->AddItem(l.c_str());
	pSelect->AddActionSignalTarget(pPage);
	control = pSelect;
}

int CSboCommandChoice::Selected()
{
	return static_cast<CSboBrowserSelect *>(control)->GetSelected();
}

void CSboCommandChoice::Reset()
{
	m_iReset = m_Current ? m_Current() : 0;
	if (m_iReset < 0 || m_iReset >= (int)m_Commands.size())
		m_iReset = 0;
	static_cast<CSboBrowserSelect *>(control)->SetSelected(m_iReset);
}

bool CSboCommandChoice::Changed()
{
	return Selected() != m_iReset;
}

void CSboCommandChoice::Apply()
{
	const int i = Selected();
	if (i >= 0 && i < (int)m_Commands.size() && !m_Commands[i].empty())
		sbo::ExecClientCmd(m_Commands[i].c_str());
	m_iReset = i;
}

CSboCvarText::CSboCvarText(vgui2::Panel *pPage, const char *text, const char *cvar, int maxChars)
    : m_Cvar(cvar)
{
	label = text;
	CSboBrowserEntry *pEntry = new CSboBrowserEntry(pPage, "Text", "");
	pEntry->SetMaximumCharCount(maxChars);
	pEntry->AddActionSignalTarget(pPage);
	control = pEntry;
}

void CSboCvarText::Reset()
{
	m_Reset = CvarString(m_Cvar.c_str());
	static_cast<CSboBrowserEntry *>(control)->SetText(m_Reset.c_str());
}

bool CSboCvarText::Changed()
{
	char buf[256];
	static_cast<CSboBrowserEntry *>(control)->GetText(buf, sizeof(buf));
	return m_Reset != buf;
}

void CSboCvarText::Apply()
{
	char buf[256];
	static_cast<CSboBrowserEntry *>(control)->GetText(buf, sizeof(buf));
	// GameUI's CCvarTextEntry: `cvar "text"` as a console command; a quote would end the argument, so none is sent
	std::string text;
	for (const char *c = buf; *c; c++)
	{
		if (*c != '"' && (unsigned char)*c >= 32)
			text += *c;
	}
	std::string cmd = m_Cvar + " \"" + text + "\"\n";
	sbo::ExecClientCmd(cmd.c_str());
	m_Reset = buf;
}

//-----------------------------------------------------------------------------
// CSboSettingsPage
//-----------------------------------------------------------------------------
CSboSettingsPage::CSboSettingsPage(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	SetPaintBackgroundEnabled(true);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);
}

//-----------------------------------------------------------------------------
// Tooltip: one shared popup in the windows' look (dark box, 1 px green frame, the body font of the window scale),
// placed directly under the hovered row's label, left-aligned with it (above the row at the bottom of the screen),
// wrapped at the window's right edge, on the screen. A settings row's label is painted by its page (no panel of its
// own), so VGUI2's per-panel Tooltip cannot cover it: the page finds the row under the mouse itself (OnThink) and
// asks for the popup every frame it should show; the popup hides itself when nobody asked for a few frames (the page
// hidden, the window closed).
//-----------------------------------------------------------------------------
namespace
{
constexpr double TOOLTIP_DELAY = 0.5;

class CSboTooltipPopup : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboTooltipPopup, vgui2::Panel);

public:
	CSboTooltipPopup()
	    : BaseClass(nullptr, "SboTooltip")
	{
		SetParent(g_pGameUIViewport);
		MakePopup(false, false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);
	}

	//! x, y: the top-left wanted (under the hovered row's label); rowTop: the row's top, for placing
	//! the tip above the row when there is no room below it; right: the right edge it may reach (the window's): a
	//! longer text wraps there
	void Show(const std::string &text, int x, int y, int rowTop, int right)
	{
		m_flAskedAt = vgui2::system()->GetCurrentTime();
		const Style &st = S();
		if (text != m_Text || !IsVisible())
		{
			m_Text = text;
			const int padX = st.M(8), padY = st.M(5);
			int sw, sh;
			vgui2::surface()->GetScreenSize(sw, sh);
			right = std::min(right, sw);
			const int room = std::max(st.M(160), right - x);
			m_Lines = WrapLines(st.body, text, room - 2 * padX);
			int textW = 0;
			for (const std::wstring &l : m_Lines)
				textW = std::max(textW, TextWide(st.body, l));
			const int w = textW + 2 * padX, h = (int)m_Lines.size() * FontTall(st.body) + 2 * padY;
			if (x + w > sw)
				x = std::max(0, sw - w);
			if (y + h > sh)
				y = std::max(0, rowTop - h - st.M(2)); // above the row at the bottom edge
			SetBounds(x, y, w, h);
			SetVisible(true);
		}
		MoveToFront();
	}

	void OnThink() override
	{
		if (IsVisible() && vgui2::system()->GetCurrentTime() - m_flAskedAt > 0.1)
			SetVisible(false);
	}

	void PaintBackground() override {}
	void PaintBorder() override {}
	void Paint() override
	{
		const Style &st = S();
		FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT);
		OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_PRIMARY); // 1 px green frame
		const int lineTall = FontTall(st.body);
		for (size_t i = 0; i < m_Lines.size(); i++)
			DrawTextMid(st.body, COL_TEXT_LIGHT, st.M(8), st.M(5) + (int)i * lineTall, lineTall, m_Lines[i]);
	}

private:
	std::string m_Text;
	std::vector<std::wstring> m_Lines;
	double m_flAskedAt = 0.0;
};

vgui2::DHANDLE<CSboTooltipPopup> s_hTooltip;

void ShowTooltip(const std::string &text, int x, int y, int rowTop, int right)
{
	if (!g_pGameUIViewport)
		return;
	if (!s_hTooltip.Get())
		s_hTooltip = new CSboTooltipPopup();
	s_hTooltip->Show(text, x, y, rowTop, right);
}
} // namespace

void CSboSettingsPage::OnThink()
{
	BaseClass::OnThink();
	const double now = vgui2::system()->GetCurrentTime();
	const CSboSetting *hover = nullptr;
	int tipX = 0, tipY = 0, rowTop = 0; // page coordinates of the hovered row: the control column, its bottom, its top
	int cx = 0, cy = 0;
	vgui2::input()->GetCursorPos(cx, cy);
	// only while the mouse is over this page or one of its controls (not over a drop-down list or another window)
	const vgui2::VPANEL over = vgui2::input()->GetMouseOver();
	if (over && (over == GetVPanel() || vgui2::ipanel()->HasParent(over, GetVPanel())))
	{
		int lx = cx, ly = cy;
		ScreenToLocal(lx, ly);
		const int pad = S().M(12);
		for (const Card &c : m_Cards)
		{
			if (lx < c.x || lx >= c.x + c.w)
				continue;
			for (const Row &r : c.rows)
			{
				const int top = r.y - m_iScroll;
				if (r.setting && !r.setting->tooltip.empty() && ly >= top && ly < top + r.tall && lx >= c.x + pad &&
				    lx < c.x + c.w - pad)
				{
					hover = r.setting;
					// directly under the row's label, left-aligned with it
					tipX = c.x + pad;
					tipY = top + r.tall + S().M(2);
					rowTop = top;
				}
			}
		}
	}
	if (hover != m_pHover)
	{
		m_pHover = hover;
		m_flHoverSince = now;
	}
	if (hover && now - m_flHoverSince >= TOOLTIP_DELAY)
	{
		int sx = tipX, sy = tipY, topX = 0, topY = rowTop;
		LocalToScreen(sx, sy);
		LocalToScreen(topX, topY);
		// the window's right edge (the page's parent window), else the screen's
		int right = 1 << 30;
		if (vgui2::Panel *pWindow = GetParent())
		{
			int wx = 0, wy = 0;
			pWindow->LocalToScreen(wx, wy);
			right = wx + pWindow->GetWide();
		}
		ShowTooltip(hover->tooltip, sx, sy, topY, right);
	}
}

CSboSettingsPage::~CSboSettingsPage()
{
	for (CSboSetting *p : m_Settings)
		delete p;
}

void CSboSettingsPage::AddCard(const char *heading, int column, bool wide)
{
	Card c;
	c.heading = heading ? heading : "";
	c.column = column;
	c.wide = wide;
	m_Cards.push_back(c);
}

CSboSetting *CSboSettingsPage::Add(CSboSetting *pSetting)
{
	if (m_Cards.empty())
		AddCard("");
	m_Settings.push_back(pSetting);
	Row r;
	r.setting = pSetting;
	m_Cards.back().rows.push_back(r);
	InvalidateLayout();
	return pSetting;
}

int CSboSettingsPage::AddText(const char *text)
{
	if (m_Cards.empty())
		AddCard("");
	Row r;
	r.text = text ? text : "";
	m_Cards.back().rows.push_back(r);
	InvalidateLayout();
	return (int)(m_Cards.size() - 1) * 1000 + (int)m_Cards.back().rows.size() - 1;
}

void CSboSettingsPage::SetText(int textId, const char *text)
{
	const int card = textId / 1000, row = textId % 1000;
	if (textId < 0 || card >= (int)m_Cards.size() || row >= (int)m_Cards[card].rows.size())
		return;
	m_Cards[card].rows[row].text = text ? text : "";
	InvalidateLayout();
	Repaint();
}

void CSboSettingsPage::SetButtonLabel(vgui2::Panel *pButton, const char *label)
{
	for (Card &c : m_Cards)
	{
		for (Row &r : c.rows)
		{
			if (r.panel == pButton)
				r.buttonLabel = label ? label : "";
		}
	}
	InvalidateLayout();
	Repaint();
}

CSboBrowserButton *CSboSettingsPage::AddButton(const char *text, const char *command, const char *label)
{
	if (m_Cards.empty())
		AddCard("");
	CSboBrowserButton *pButton = new CSboBrowserButton(this, "Button", text, command, CSboBrowserButton::KIND_NORMAL);
	Row r;
	r.panel = pButton;
	r.buttonLabel = label ? label : "";
	m_Cards.back().rows.push_back(r);
	InvalidateLayout();
	return pButton;
}

CSboLinkLabel *CSboSettingsPage::AddLink(const char *text, const char *linkText, const char *url)
{
	if (m_Cards.empty())
		AddCard("");
	CSboLinkLabel *pLink = new CSboLinkLabel(this, "Link", linkText, url);
	Row r;
	r.text = text ? text : "";
	r.panel = pLink;
	r.link = true;
	m_Cards.back().rows.push_back(r);
	InvalidateLayout();
	return pLink;
}

void CSboSettingsPage::ResetAll()
{
	for (CSboSetting *p : m_Settings)
		p->Reset();
}

bool CSboSettingsPage::AnyChanged()
{
	for (CSboSetting *p : m_Settings)
	{
		if (p->Changed())
			return true;
	}
	return false;
}

void CSboSettingsPage::ApplyAll()
{
	for (CSboSetting *p : m_Settings)
	{
		if (p->Changed())
			p->Apply();
	}
}

void CSboSettingsPage::NotifyChanged()
{
	if (GetParent())
		vgui2::ivgui()->PostMessage(GetParent()->GetVPanel(), new KeyValues("SettingsChanged"), GetVPanel());
}

void CSboSettingsPage::PerformLayout()
{
	BaseClass::PerformLayout();
	const Style &st = S();
	const int W = GetWide();
	const bool two = W >= st.M(m_iTwoColumnWide);
	const int gap = st.M(16), pad = st.M(12), headTall = st.M(22), rowGap = st.M(6);
	const int bodyLine = FontTall(st.body) + st.M(2), smallLine = FontTall(st.small) + st.M(2);
	const int colW = two ? (W - gap) / 2 : W;
	int colY[2] = { 0, 0 };
	for (Card &c : m_Cards)
	{
		int col = 0;
		int x = 0, w = colW;
		int top;
		if (c.wide || !two)
		{
			top = std::max(colY[0], two ? colY[1] : 0);
			w = W;
		}
		else
		{
			col = c.column >= 0 ? c.column : (colY[1] < colY[0] ? 1 : 0);
			x = col * (colW + gap);
			top = colY[col];
		}
		c.x = x;
		c.y = top;
		c.w = w;
		// the label column fits the card's longest label, up to 55% of the card; longer labels wrap
		int labelW = st.M(80);
		for (const Row &r : c.rows)
		{
			const std::string &l = r.setting ? r.setting->label : r.buttonLabel;
			if (!l.empty())
				labelW = std::max(labelW, TextWide(st.body, Widen(l.c_str())) + st.M(14));
		}
		labelW = std::min(labelW, (w - 2 * pad) * 55 / 100);
		c.labelW = labelW;
		// every drop-down of the card gets one width: its widest item and the arrow (at least 120, at most 320 design
		// pixels), so their edges line up
		int selectW = 0;
		for (const Row &r : c.rows)
		{
			if (r.setting && r.setting->controlWide <= 0 && dynamic_cast<CSboBrowserSelect *>(r.setting->control))
			{
				const int want = static_cast<CSboBrowserSelect *>(r.setting->control)->GetContentWide();
				selectW = std::max(selectW, std::max(st.M(120), std::min(st.M(320), want)));
			}
		}
		int y = top + pad + (c.heading.empty() ? 0 : headTall);
		for (Row &r : c.rows)
		{
			r.y = y;
			if (r.setting && r.setting->hidden)
			{
				r.tall = 0;
				r.labelLines.clear();
				r.noteLines.clear();
				continue;
			}
			if (r.setting)
			{
				CSboSetting *s = r.setting;
				const bool full = s->label.empty();
				r.labelLines = full ? std::vector<std::wstring>() : WrapLines(st.body, s->label, labelW - st.M(10));
				r.tall = std::max(st.M(s->rowTall), (int)r.labelLines.size() * bodyLine + st.M(6));
				const int cx = full ? x + pad : x + pad + labelW;
				int cw = full ? w - 2 * pad : w - 2 * pad - labelW;
				if (s->controlWide > 0)
					cw = std::min(cw, st.M(s->controlWide));
				if (dynamic_cast<CSboBrowserCheck *>(s->control))
					cw = std::min(cw, static_cast<CSboBrowserCheck *>(s->control)->GetContentWide());
				// a drop-down as wide as its longest item and the arrow (at least 120, at most 320 design pixels),
				// left-aligned after the label column (stretched over the card it looked empty)
				if (dynamic_cast<CSboBrowserSelect *>(s->control) && s->controlWide <= 0)
					cw = std::min(cw, selectW);
				const int ch = s->rowTall > 40 ? st.M(s->rowTall) : std::min(st.M(s->rowTall), st.M(28)); // tall: a picture
				s->control->SetBounds(cx, y + (r.tall - ch) / 2 - m_iScroll, cw, ch);
				const int noteX = full ? st.M(19) : labelW;
				r.noteLines = s->note.empty() ? std::vector<std::wstring>() : WrapLines(st.small, s->note, w - 2 * pad - noteX);
				r.tall += (int)r.noteLines.size() * smallLine;
			}
			else if (r.link)
			{
				// the text, then the link on the same line, or on the next one where the card is too narrow for both
				CSboLinkLabel *pLink = static_cast<CSboLinkLabel *>(r.panel);
				pLink->SetLinkFont(st.small);
				pLink->SetFgColor(COL_TEXT);
				int linkW, linkH;
				pLink->GetContentSize(linkW, linkH);
				const int textW = r.text.empty() ? 0 : TextWide(st.small, Widen(r.text.c_str())) + st.M(4);
				const bool sameLine = textW + linkW <= w - 2 * pad;
				const int lx = sameLine ? x + pad + textW : x + pad;
				const int ly = sameLine ? y : y + smallLine;
				pLink->SetBounds(lx, ly - m_iScroll, std::min(linkW, x + w - pad - lx), smallLine);
				r.tall = (sameLine ? 1 : 2) * smallLine + st.M(2);
			}
			else if (r.panel)
			{
				r.tall = st.M(30);
				CSboBrowserButton *pButton = static_cast<CSboBrowserButton *>(r.panel);
				const int bx = r.buttonLabel.empty() ? x + pad : x + pad + labelW;
				pButton->SetBounds(bx, y + st.M(2) - m_iScroll, pButton->GetContentWide(), st.M(26));
			}
			else
			{
				r.noteLines = WrapLines(st.small, r.text, w - 2 * pad);
				r.tall = (int)r.noteLines.size() * smallLine + st.M(2);
			}
			y += r.tall + rowGap;
		}
		c.h = y - top + pad - rowGap;
		if (c.wide || !two)
			colY[0] = colY[1] = c.y + c.h + gap;
		else
			colY[col] = c.y + c.h + gap;
	}
	m_iContentTall = std::max(colY[0], colY[1]);
	const int maxScroll = std::max(0, m_iContentTall - GetTall());
	if (m_iScroll > maxScroll)
	{
		m_iScroll = maxScroll;
		InvalidateLayout();
	}
}

void CSboSettingsPage::OnCommand(const char *command)
{
	if (GetParent())
		GetParent()->OnCommand(command);
}

void CSboSettingsPage::OnMouseWheeled(int delta)
{
	const int maxScroll = std::max(0, m_iContentTall - GetTall());
	const int s = std::max(0, std::min(maxScroll, m_iScroll - delta * S().M(40)));
	if (s != m_iScroll)
	{
		m_iScroll = s;
		InvalidateLayout();
		Repaint();
	}
}

void CSboSettingsPage::PaintBackground()
{
	for (const Card &c : m_Cards)
	{
		const int y = c.y - m_iScroll;
		OutlineRect(c.x, y, c.w, c.h, COL_BORDER_LIST);
	}
}

void CSboSettingsPage::Paint()
{
	const Style &st = S();
	const int pad = st.M(12);
	const int bodyLine = FontTall(st.body) + st.M(2), smallLine = FontTall(st.small) + st.M(2);
	for (const Card &c : m_Cards)
	{
		if (!c.heading.empty())
			DrawSpacedUpper(st.small, COL_TEXT_DIM, c.x + pad, c.y + pad - m_iScroll, c.heading.c_str());
		for (const Row &r : c.rows)
		{
			const int y = r.y - m_iScroll;
			if (r.setting && r.setting->hidden)
				continue;
			if (r.setting)
			{
				const CSboSetting *s = r.setting;
				const int labelTall = (int)r.labelLines.size() * bodyLine;
				const int rowTall = std::max(st.M(s->rowTall), labelTall + st.M(6));
				int ly = y + (rowTall - labelTall) / 2;
				for (const std::wstring &line : r.labelLines)
				{
					DrawText(st.body, COL_TEXT_LIGHT, c.x + pad, ly, line);
					ly += bodyLine;
				}
				int ny = y + rowTall;
				const int noteX = c.x + pad + (s->label.empty() ? st.M(19) : c.labelW);
				for (const std::wstring &line : r.noteLines)
				{
					DrawText(st.small, COL_TEXT_FAINT, noteX, ny, line);
					ny += smallLine;
				}
			}
			else if (r.link)
				DrawText(st.small, COL_TEXT_DIM, c.x + pad, y, Widen(r.text.c_str()));
			else if (r.panel)
			{
				if (!r.buttonLabel.empty())
					DrawTextMid(st.body, COL_TEXT_LIGHT, c.x + pad, y, st.M(30), Widen(r.buttonLabel.c_str()), c.labelW - st.M(8));
			}
			else
			{
				int ly = y;
				for (const std::wstring &line : r.noteLines)
				{
					DrawText(st.small, COL_TEXT_DIM, c.x + pad, ly, line);
					ly += smallLine;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// CSboWindow
//-----------------------------------------------------------------------------
CSboWindow::CSboWindow(vgui2::Panel *pParent, const char *pName, const char *title, const char *key, int mockWide,
    int mockTall)
    : BaseClass(pParent, pName)
    , m_Title(title)
    , m_Key(key)
    , m_iMockWide(mockWide)
    , m_iMockTall(mockTall)
{
	SetTitleBarVisible(false); // drawn here; the frame's caption grip moves it (made as tall as the header)
	SetMoveable(true);
	SetSizeable(true);
	SetDeleteSelfOnClose(true);
	SetProportional(true);
	SetScheme(BrowserScheme());
	UpdateStyle(this);
	m_pClose = new CSboBrowserButton(this, "CloseButton", "", "Close", CSboBrowserButton::KIND_CLOSE);
	sboui::ApplySymbolSubstitutes(this);
}

void CSboWindow::AddTab(const char *text)
{
	char cmd[16];
	snprintf(cmd, sizeof(cmd), "tab %d", (int)m_Tabs.size());
	CSboBrowserButton *pTab = new CSboBrowserButton(this, "Tab", text, cmd, CSboBrowserButton::KIND_TAB);
	m_Tabs.push_back(pTab);
	pTab->SetActiveTab(m_Tabs.size() == 1);
}

void CSboWindow::SetTab(int tab)
{
	if (tab < 0 || tab >= (int)m_Tabs.size())
		return;
	m_iTab = tab;
	for (int i = 0; i < (int)m_Tabs.size(); i++)
		m_Tabs[i]->SetActiveTab(i == tab);
	OnTabChanged(tab);
	InvalidateLayout();
}

CSboBrowserButton *CSboWindow::AddBottomButton(const char *text, const char *command, bool primary)
{
	CSboBrowserButton *pButton = new CSboBrowserButton(this, "Bottom", text, command,
	    primary ? CSboBrowserButton::KIND_PRIMARY : CSboBrowserButton::KIND_NORMAL);
	m_Bottom.push_back(pButton);
	return pButton;
}

void CSboWindow::Open()
{
	ResetStyle(this); // the normal look for this screen (an earlier window may have shrunk it)
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	// the game menu keeps its place on the left: GameUI's menu panel where it can be found (and plausible), else a
	// quarter of the screen
	int menuRight = sw / 4;
	int mx, my, mw, mh;
	if (SboFindGameMenuBounds(mx, my, mw, mh) && mx + mw < sw / 2)
		menuRight = std::max(menuRight, mx + mw);
	const int gap = S().M(24), edge = S().M(16);
	const int avail = std::max(S().M(300), sw - menuRight - gap - edge);
	if (S().M(m_iMockWide) > avail)
	{
		// a narrow screen (1280x720, 1024x768, ...): the whole look smaller, so the window fits beside the menu (as
		// the server browser does)
		SetStyleScale(S().scale * (float)avail / (float)S().M(m_iMockWide));
		InvalidateLayout(true, true);
	}
	SetMinimumSize(std::min(S().M(m_iMockWide * 3 / 4), avail), std::min(S().M(m_iMockTall * 3 / 4), sh));
	const int wide = std::min(S().M(m_iMockWide), avail);
	const int tall = std::min(S().M(m_iMockTall), sh - edge);
	KeyValues *w = Saved()->FindKey(m_Key.c_str());
	if (w && w->GetInt("wide") > 0 && w->GetInt("tall") > 0)
	{
		// the place the player gave it, but never over the menu
		int x = w->GetInt("x"), y = w->GetInt("y"), ww = w->GetInt("wide"), hh = w->GetInt("tall");
		// saved at another resolution: the same place and size relative to the screen
		const int ssw = w->GetInt("screenwide"), ssh = w->GetInt("screentall");
		if (ssw > 0 && ssh > 0 && (ssw != sw || ssh != sh))
		{
			x = x * sw / ssw;
			ww = ww * sw / ssw;
			y = y * sh / ssh;
			hh = hh * sh / ssh;
		}
		// never over the menu: moved right of it, its size kept (a window wider than the room ends at the right edge)
		if (x < menuRight + gap / 2)
			x = std::max(0, std::min(menuRight + gap, sw - ww));
		SetBounds(x, y, ww, hh);
	}
	else
		SetBounds(sw - wide - edge, (sh - tall) / 2, wide, tall);
	KeepOnScreen();
	GetBounds(m_iSavedX, m_iSavedY, m_iSavedW, m_iSavedH);
	InvalidateLayout(true, true);
	Activate();
}

void CSboWindow::KeepOnScreen()
{
	int sw, sh, x, y, w, h;
	vgui2::surface()->GetScreenSize(sw, sh);
	GetBounds(x, y, w, h);
	w = std::min(w, sw);
	h = std::min(h, sh);
	x = std::max(0, std::min(x, sw - w));
	y = std::max(0, std::min(y, sh - h));
	SetBounds(x, y, w, h);
}

void CSboWindow::SaveBounds()
{
	int x, y, w, h;
	GetBounds(x, y, w, h);
	KeyValues *kv = Saved()->FindKey(m_Key.c_str(), true);
	kv->SetInt("x", x);
	kv->SetInt("y", y);
	kv->SetInt("wide", w);
	kv->SetInt("tall", h);
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	kv->SetInt("screenwide", sw);
	kv->SetInt("screentall", sh);
	Saved()->SaveToFile(g_pFullFileSystem, SETTINGS_FILE, SETTINGS_PATH_ID);
}

void CSboWindow::OnThink()
{
	BaseClass::OnThink();
	int x, y, w, h;
	GetBounds(x, y, w, h);
	const double now = vgui2::system()->GetCurrentTime();
	if (x != m_iSavedX || y != m_iSavedY || w != m_iSavedW || h != m_iSavedH)
	{
		m_iSavedX = x;
		m_iSavedY = y;
		m_iSavedW = w;
		m_iSavedH = h;
		m_flSaveAt = now + 1.0; // written once the moving / resizing stops
	}
	if (m_flSaveAt > 0.0 && now >= m_flSaveAt)
	{
		m_flSaveAt = 0.0;
		SaveBounds();
	}
}

void CSboWindow::OnClose()
{
	SaveBounds();
	BaseClass::OnClose();
}

void CSboWindow::OnScreenSizeChanged(int oldWide, int oldTall)
{
	BaseClass::OnScreenSizeChanged(oldWide, oldTall);
	UpdateStyle(this);
	KeepOnScreen();
	InvalidateLayout(true, true);
}

void CSboWindow::OnCommand(const char *command)
{
	if (!Q_strncmp(command, "tab ", 4))
	{
		SetTab(atoi(command + 4));
		return;
	}
	BaseClass::OnCommand(command); // "Close"
}

void CSboWindow::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
		OnCommand("Close");
	else
		BaseClass::OnKeyCodeTyped(code);
}

void CSboWindow::PerformLayout()
{
	BaseClass::PerformLayout();
	const Style &st = S();
	const int W = GetWide(), H = GetTall();
	const int padX = st.M(24), padY = st.M(20);
	const int closeSize = st.M(28);
	m_pClose->SetBounds(W - padX - closeSize, padY, closeSize, closeSize);
	const int headerTall = st.M(40);
	int y = padY + headerTall;
	if (!m_Tabs.empty())
	{
		int x = padX;
		const int tabTall = st.M(32);
		for (CSboBrowserButton *pTab : m_Tabs)
		{
			const int tw = pTab->GetContentWide();
			pTab->SetBounds(x, y, tw, tabTall);
			x += tw + st.M(2);
		}
		y += tabTall;
		m_iTabLineY = y;
		y += st.M(14);
	}
	else
		m_iTabLineY = -1;
	// bottom buttons, right to left
	const int bh = st.M(32), by = H - padY - bh;
	int x = W - padX;
	for (CSboBrowserButton *pButton : m_Bottom)
	{
		const int bw = std::max(pButton->GetContentWide(), st.M(72));
		x -= bw;
		pButton->SetBounds(x, by, bw, bh);
		x -= st.M(10);
	}
	LayoutContent(padX, y, W - 2 * padX, (m_Bottom.empty() ? H - padY : by - st.M(16)) - y);
	if (vgui2::Panel *pCaption = FindChildByName("frame_caption"))
		pCaption->SetBounds(0, 0, m_pClose->GetXPos() - st.M(8), padY + headerTall);
}

void CSboWindow::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_WINDOW);
}

void CSboWindow::PaintBorder()
{
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
}

void CSboWindow::Paint()
{
	const Style &st = S();
	const int padX = st.M(24), padY = st.M(20);
	DrawTextMid(st.title, COL_TEXT, padX, padY, st.M(32), Widen(m_Title.c_str()));
	if (m_iTabLineY >= 0)
		FillRect(padX, m_iTabLineY - 1, GetWide() - 2 * padX, 1, COL_BORDER);
	// resize grip
	const int W = GetWide(), H = GetTall(), gs = st.M(4);
	vgui2::surface()->DrawSetColor(COL_BORDER);
	for (int line = 1; line <= 3; line++)
	{
		const int len = line * gs;
		for (int i = 0; i < len; i++)
			vgui2::surface()->DrawFilledRect(W - 3 - len + i, H - 3 - i - 1, W - 3 - len + i + 1, H - 3 - i);
	}
}
