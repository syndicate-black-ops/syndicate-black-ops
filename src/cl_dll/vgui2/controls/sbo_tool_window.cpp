//========= SBO VGUI2 controls: in-game tool windows =========//
//
// Purpose: see sbo_tool_window.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

#include "../client_viewport.h"
#include "../sbo_bridge.h"
#include "../sbo_gm_ui.h" // gmui::HintsBarTop
#include "../sbo_server_browser.h" // CSboBrowserButton
#include "../sbo_ui_style.h"
#include "../sbo_widgets.h"
#include "../vgui2_api.h"
#include "sbo_tool_window.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
// the tool windows' places, kept between games in the mod folder (as sbomenus.vdf); no cvar
constexpr const char TOOLS_FILE[] = "sbotools.vdf";
constexpr const char TOOLS_PATH_ID[] = "GAMECONFIG";

KeyValues *s_pSaved = nullptr;
std::vector<CSboToolWindow *> s_Open;

// mouse look with the tools open
bool s_bMouseLook = false;       // the windows gave up the mouse
bool s_bMouseLookToggled = false; // tool_mouselook turned it on (mouse look while the tools stay open)
int s_iLookCursorX = 0, s_iLookCursorY = 0; // where the cursor was
bool s_bEscGuard = false;                   // mouse look holds the Esc guard
// The engine opens the game menu on Esc before the client sees the key (HL25), so a tool mode that Esc ends (mouse
// look, a pick on the screen, typing in a field or a list) holds the game menu's Esc guard: the menu is hidden again at
// once and the press is handed to the tools (EscapeTick). The guard is on while any mode holds it.
int s_iEscHolders = 0;
CSboScreenPicker *s_pPicking = nullptr;     // the screen picker waiting for a click

// A text field that Esc ended lets go of the guard a moment later: the field can get the key before the engine's
// game menu does, and the guard must still be up when the menu opens (seen on HL25 Linux)
constexpr double ESC_RELEASE_DELAY = 0.3;
int s_iEscReleases = 0;          // guards to let go of at s_flEscReleaseAt
double s_flEscReleaseAt = 0.0;

void HoldEscape(bool hold)
{
	const int before = s_iEscHolders;
	s_iEscHolders = std::max(0, s_iEscHolders + (hold ? 1 : -1));
	if (g_pGameUIViewport && (before > 0) != (s_iEscHolders > 0))
		g_pGameUIViewport->SetMenuEscapeGuard(s_iEscHolders > 0);
}

// "Mouse look: Esc to stop" at the bottom center while the tools' mouse look is on (paint only, no input): where
// the GM key hints are (gmui::HintsBarTop), over the spectator bottom bar and the watched player's box when they are up
class CSboToolHint : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboToolHint, vgui2::Panel);

public:
	CSboToolHint(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboToolHint")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);
	}
	void Show(bool show)
	{
		if (show)
		{
			Place();
			MoveToFront();
		}
		SetVisible(show);
	}
	void OnThink() override
	{
		Place(); // the spectator bars come and go
	}
	void Place()
	{
		const int tw = TextWide(S().body, TEXT), pad = S().M(10), h = gmui::HintsBarTall();
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int spec = VGui2_Spectator_BottomTop();
		SetBounds((sw - tw) / 2 - pad, gmui::HintsBarTop(spec > 0 ? spec : sh), tw + 2 * pad, h);
	}
	void Paint() override
	{
		FillRect(0, 0, GetWide(), GetTall(), COL_BG_WINDOW);
		OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
		DrawTextMid(S().body, COL_TEXT, S().M(10), 0, GetTall(), TEXT);
	}

private:
	static constexpr const wchar_t *TEXT = L"Mouse look: Esc to stop";
};
CSboToolHint *s_pHint = nullptr;

// the tip of the control the cursor rests on (paint only, no input), shared by every tool window
class CSboToolTipPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboToolTipPanel, vgui2::Panel);

public:
	CSboToolTipPanel(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboToolTip")
	{
		MakePopup(false, false); // above the tool windows (popups themselves)
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);
	}
	void ShowAt(const std::string &text, int cx, int cy)
	{
		m_Text = Widen(text.c_str());
		const int pad = S().M(6), w = TextWide(S().small, m_Text) + 2 * pad, h = FontTall(S().small) + 2 * pad;
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		int x = cx + S().M(14), y = cy + S().M(18);
		if (x + w > sw)
			x = std::max(0, sw - w);
		if (y + h > sh)
			y = std::max(0, cy - h - S().M(4));
		SetBounds(x, y, w, h);
		SetVisible(true);
		MoveToFront();
		Repaint();
	}
	void Paint() override
	{
		FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT);
		OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
		DrawTextMid(S().small, COL_TEXT_LIGHT, S().M(6), 0, GetTall(), m_Text);
	}

private:
	std::wstring m_Text;
};
CSboToolTipPanel *s_pTip = nullptr;

enum
{
	RESIZE_NS,   // up and down
	RESIZE_WE,   // left and right
	RESIZE_NWSE, // top left and bottom right
	RESIZE_NESW, // top right and bottom left
};

// The resize arrow of a tool window's edge, painted by the window itself over its controls (PostChildPaint), beside
// the mouse on the window's side of the edge. Not a panel of its own: the engine does not look through a popup for the
// panel under the mouse, even one that takes no mouse input, and an arrow popup under the cursor made the grip miss the
// press and froze VGUI2's cursor during a resize (the window followed only the first step).
void DrawResizeDot(int x, int y, int grow, const Color &c)
{
	vgui2::surface()->DrawSetColor(c);
	vgui2::surface()->DrawFilledRect(x - grow, y - grow, x + 1 + grow, y + 1 + grow);
}

void DrawResizeArrowPass(int kind, int x0, int y0, int n, const Color &c, int grow)
{
	const int m = n / 2, head = n / 4;
	// the shaft and the two heads along (dx, dy)
	int dx = 0, dy = 0;
	switch (kind)
	{
	case RESIZE_NS: dx = 0; dy = 1; break;
	case RESIZE_WE: dx = 1; dy = 0; break;
	case RESIZE_NWSE: dx = 1; dy = 1; break;
	default: dx = 1; dy = -1; break;
	}
	const int len = (dx && dy) ? m * 7 / 10 : m - 2;
	for (int t = -len; t <= len; t++)
		DrawResizeDot(x0 + m + t * dx, y0 + m + t * dy, grow, c);
	for (int side = -1; side <= 1; side += 2)
	{
		const int tipX = m + side * len * dx, tipY = m + side * len * dy;
		const int bx = -side * dx, by = -side * dy; // back from the tip
		for (int k = 1; k <= head; k++)
		{
			if (dx && dy)
			{
				// a diagonal: the head's strokes run along x and along y
				DrawResizeDot(x0 + tipX + k * bx, y0 + tipY, grow, c);
				DrawResizeDot(x0 + tipX, y0 + tipY + k * by, grow, c);
			}
			else
			{
				// along an axis: the strokes go back and out to both sides
				DrawResizeDot(x0 + tipX + k * bx + k * by, y0 + tipY + k * by + k * bx, grow, c);
				DrawResizeDot(x0 + tipX + k * bx - k * by, y0 + tipY + k * by - k * bx, grow, c);
			}
		}
	}
}

//! A double arrow in the square (x, y, n, n): a dark outline under a white one
void DrawResizeArrow(int kind, int x, int y, int n)
{
	DrawResizeArrowPass(kind, x, y, n, Color(0, 0, 0, 255), 1);
	DrawResizeArrowPass(kind, x, y, n, Color(255, 255, 255, 255), 0);
}
vgui2::VPANEL s_TipControl = 0;     // the control whose tip is shown or waited for
double s_flTipSince = 0.0;          // when the cursor came to rest on it
int s_iTipX = -1, s_iTipY = -1;     // where the cursor rested
const void *s_pTipOwner = nullptr;  // the window that shows it
std::string s_TipText;              // its text

void HideTip()
{
	if (s_pTip)
		s_pTip->SetVisible(false);
	s_TipControl = 0;
	s_pTipOwner = nullptr;
}

// the arrow left of a tool window's title: folds the window to its header and opens it again
class CSboFoldArrow : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboFoldArrow, vgui2::Panel);

public:
	CSboFoldArrow(vgui2::Panel *pParent)
	    : BaseClass(pParent, "FoldArrow")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(false);
	}
	void SetFolded(bool folded)
	{
		m_bFolded = folded;
		Repaint();
	}
	void Paint() override
	{
		const int w = GetWide(), h = GetTall(), s = std::max(3, std::min(w, h) / 3);
		const Color c = m_bHover ? COL_TEXT_BRIGHT : COL_TEXT;
		vgui2::surface()->DrawSetColor(c);
		if (m_bFolded)
		{
			// pointing right: columns getting shorter
			const int x0 = (w - s) / 2, cy = h / 2;
			for (int i = 0; i < s; i++)
				vgui2::surface()->DrawFilledRect(x0 + i, cy - (s - i), x0 + i + 1, cy + (s - i));
		}
		else
			DrawTriangle(w / 2, h / 2, s, false, c);
	}
	void OnCursorEntered() override
	{
		m_bHover = true;
		Repaint();
	}
	void OnCursorExited() override
	{
		m_bHover = false;
		Repaint();
	}
	void OnMousePressed(vgui2::MouseCode code) override
	{
		if (code == vgui2::MOUSE_LEFT)
			PostActionSignal(new KeyValues("Command", "command", "ToolFold"));
	}
	void OnMouseDoublePressed(vgui2::MouseCode code) override { OnMousePressed(code); }

private:
	bool m_bFolded = false;
	bool m_bHover = false;
};

KeyValues *Saved()
{
	if (!s_pSaved)
	{
		s_pSaved = new KeyValues("SboTools");
		s_pSaved->LoadFromFile(g_pFullFileSystem, TOOLS_FILE, TOOLS_PATH_ID);
	}
	return s_pSaved;
}
} // namespace

bool SboToolsAllowed()
{
	if (!sbo::IsConnected())
		return false;
	if (sbo::GetMaxClients() <= 1)
		return true;
	unsigned ip = 0;
	unsigned short port = 0;
	bool loopback = false;
	if (sbo::GetServerAddress(ip, port, loopback) && loopback)
		return true;
	// another server: only with sv_cheats 2 or more, so that the ordinary cheats (sv_cheats 1) don't open the editors.
	// The client's own sv_cheats cvar can't tell 1 from 2 (the engine sends only whether it is 0): the server sends its
	// value in the "Cheats" message
	return sbo::GetServerCheats() >= 2;
}

bool VGui2_ToolWindows_HaveMouse()
{
	return !s_Open.empty();
}

static void Cmd_ToolMouseLook()
{
	if (s_Open.empty())
	{
		s_bMouseLookToggled = false;
		return;
	}
	s_bMouseLookToggled = !s_bMouseLookToggled;
	CSboToolWindow::SetMouseLook(s_bMouseLookToggled);
}

void VGui2_ToolWindows_RegisterCommands()
{
	sbo::AddCommand("tool_mouselook", &Cmd_ToolMouseLook);
}

bool VGui2_ToolWindows_TextInput()
{
	for (const CSboToolWindow *p : s_Open)
		if (p->HasTextInput())
			return true;
	return false;
}

//-----------------------------------------------------------------------------
// CSboToolWindow
//-----------------------------------------------------------------------------
CSboToolWindow::CSboToolWindow(vgui2::Panel *pParent, const char *pName, const char *title, const char *key,
    int mockWide, int mockTall, float placeX, float placeY)
    : BaseClass(pParent, pName)
    , m_Title(title ? title : "")
    , m_Key(key ? key : pName)
    , m_iMockWide(mockWide)
    , m_iMockTall(mockTall)
    , m_flPlaceX(placeX)
    , m_flPlaceY(placeY)
{
	SetTitleBarVisible(false); // drawn here; the frame's caption grip moves it (made as tall as the header)
	SetMoveable(true);
	SetSizeable(false);
	SetDeleteSelfOnClose(false);
	DisableFadeEffect();
	SetProportional(true);
	SetScheme(BrowserScheme());
	SetKeyBoardInputEnabled(false); // the game keeps the keyboard (walking while editing)
	SetMouseInputEnabled(true);
	SetVisible(false);
	UpdateStyle(this);
	m_pClose = new CSboBrowserButton(this, "CloseButton", "", "Close", CSboBrowserButton::KIND_CLOSE);
	CSboFoldArrow *arrow = new CSboFoldArrow(this);
	arrow->AddActionSignalTarget(this);
	m_pArrow = arrow;
	sboui::ApplySymbolSubstitutes(this);
	SetToolSizeable(true);
	SetPostChildPaintEnabled(true); // the resize arrow over the controls
	SetToolTip(m_pClose, "Close this window");
	SetToolTip(m_pArrow, "Fold the window to its title bar, or open it again");
}

CSboToolWindow::~CSboToolWindow()
{
	s_Open.erase(std::remove(s_Open.begin(), s_Open.end(), this), s_Open.end());
	if (m_bEscCloseHeld)
		HoldEscape(false);
	if (s_pTipOwner == this)
		HideTip();
}

void CSboToolWindow::SetToolTip(vgui2::Panel *pControl, const char *tip)
{
	if (!pControl)
		return;
	if (tip && tip[0])
		m_Tips[pControl->GetVPanel()] = tip;
	else
		m_Tips.erase(pControl->GetVPanel());
}

void CSboToolWindow::SetToolTipFunc(vgui2::Panel *pControl, std::function<std::string()> tip)
{
	if (!pControl)
		return;
	if (tip)
		m_TipFuncs[pControl->GetVPanel()] = tip;
	else
		m_TipFuncs.erase(pControl->GetVPanel());
}

void CSboToolWindow::SetEscapeCloses(bool closes)
{
	m_bEscCloses = closes;
	const bool hold = closes && m_bOpen;
	if (hold != m_bEscCloseHeld)
	{
		HoldEscape(hold);
		m_bEscCloseHeld = hold;
	}
}

int CSboToolWindow::HeaderTall() const
{
	return M(26) + M(4);
}

bool CSboToolWindow::IsHeaderControl(vgui2::Panel *p) const
{
	if (!p || p == m_pArrow || p == m_pClose || p == m_pLook)
		return true;
	const char *name = p->GetName();
	return name && !Q_strncmp(name, "frame_", 6); // the frame's own grips
}

void CSboToolWindow::HideContent()
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		vgui2::Panel *p = GetChild(i);
		if (!p || IsHeaderControl(p) || !p->IsVisible())
			continue;
		if (std::find(m_HiddenContent.begin(), m_HiddenContent.end(), p->GetVPanel()) == m_HiddenContent.end())
			m_HiddenContent.push_back(p->GetVPanel());
		p->SetVisible(false);
	}
}

void CSboToolWindow::SetCollapsed(bool collapsed)
{
	if (collapsed == m_bCollapsed)
		return;
	EndTextInput();
	int x, y, w, h;
	GetBounds(x, y, w, h);
	m_bCollapsed = collapsed;
	if (collapsed)
	{
		m_iExpandedTall = h;
		SetSizeable(false);
		SetMinimumSize(0, 0);
		SetBounds(x, y, w, HeaderTall());
		m_HiddenContent.clear();
		HideContent();
	}
	else
	{
		for (vgui2::VPANEL v : m_HiddenContent)
			vgui2::ipanel()->SetVisible(v, true);
		m_HiddenContent.clear();
		SetBounds(x, y, w, m_iExpandedTall > HeaderTall() ? m_iExpandedTall : M(m_iMockTall));
		SetSizeable(m_bSizeable);
		if (m_bSizeable)
			SetMinimumSize(M(m_iMockWide) * 2 / 3, M(m_iMockTall) * 2 / 3);
		KeepOnScreen();
	}
	static_cast<CSboFoldArrow *>(m_pArrow)->SetFolded(collapsed);
	InvalidateLayout(true, true);
	Repaint();
	SaveBounds();
}

// moving by the header (the frame's own caption grip is not used: a double click on the header folds the window)
void CSboToolWindow::OnMouseDoublePressed(vgui2::MouseCode code)
{
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	if (code == vgui2::MOUSE_LEFT && y < M(26))
	{
		m_bDragging = false;
		vgui2::input()->SetMouseCapture(0);
		SetCollapsed(!m_bCollapsed);
		return;
	}
	BaseClass::OnMouseDoublePressed(code);
}

void CSboToolWindow::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_bDragging && code == vgui2::MOUSE_LEFT)
	{
		m_bDragging = false;
		vgui2::input()->SetMouseCapture(0);
		KeepOnScreen();
		return;
	}
	BaseClass::OnMouseReleased(code);
}

void CSboToolWindow::OnCursorMoved(int x, int y)
{
	if (m_bDragging)
	{
		int cx, cy;
		vgui2::input()->GetCursorPos(cx, cy);
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int nx = std::max(-GetWide() + M(60), std::min(cx - m_iDragX, sw - M(60)));
		const int ny = std::max(0, std::min(cy - m_iDragY, sh - M(20)));
		SetPos(nx, ny);
		return;
	}
	BaseClass::OnCursorMoved(x, y);
}

// the tip of the control under the cursor, after the cursor rests on it for half a second
void CSboToolWindow::UpdateToolTip()
{
	vgui2::VPANEL over = vgui2::input()->GetMouseOver();
	std::string tip;
	vgui2::VPANEL control = 0;
	for (vgui2::VPANEL p = over; p && p != GetVPanel(); p = vgui2::ipanel()->GetParent(p))
	{
		auto fn = m_TipFuncs.find(p);
		if (fn != m_TipFuncs.end())
		{
			tip = fn->second();
			control = tip.empty() ? 0 : p;
			break;
		}
		auto it = m_Tips.find(p);
		if (it != m_Tips.end())
		{
			tip = it->second;
			control = p;
			break;
		}
	}
	const bool mine = over && (over == GetVPanel() || vgui2::ipanel()->HasParent(over, GetVPanel()));
	if (!mine)
	{
		if (s_pTipOwner == this)
			HideTip();
		return;
	}
	int cx, cy;
	vgui2::input()->GetCursorPos(cx, cy);
	const double now = vgui2::system()->GetCurrentTime();
	// no tip while a button is held, while a drop-down's list or a drag has the mouse
	if (!control || vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT) || m_bDragging || vgui2::input()->GetMouseCapture())
	{
		HideTip();
		return;
	}
	if (control != s_TipControl || tip != s_TipText || abs(cx - s_iTipX) > 3 || abs(cy - s_iTipY) > 3)
	{
		// moved: the tip waits until the cursor rests again (a shown tip stays while it moves on the same control and
		// its text stays the same)
		const bool shown = s_pTip && s_pTip->IsVisible() && control == s_TipControl && tip == s_TipText;
		if (!shown)
		{
			HideTip();
			s_TipControl = control;
			s_TipText = tip;
			s_flTipSince = now;
		}
		s_iTipX = cx;
		s_iTipY = cy;
		return;
	}
	if (now - s_flTipSince >= 0.5 && (!s_pTip || !s_pTip->IsVisible()) && g_pViewport)
	{
		if (!s_pTip)
			s_pTip = new CSboToolTipPanel(g_pViewport);
		s_pTip->ShowAt(tip, cx, cy);
		s_pTipOwner = this;
	}
}

void CSboToolWindow::SetToolTitle(const char *title)
{
	m_Title = title ? title : "";
	Repaint();
}

void CSboToolWindow::SetToolSizeable(bool sizeable)
{
	m_bSizeable = sizeable;
	SetSizeable(sizeable && !m_bCollapsed);
}

int CSboToolWindow::M(int px) const
{
	return S().M(px);
}

int CSboToolWindow::OpenCount()
{
	return (int)s_Open.size();
}

void CSboToolWindow::CloseAll()
{
	const std::vector<CSboToolWindow *> open = s_Open;
	for (CSboToolWindow *p : open)
		p->CloseTool();
}

void CSboToolWindow::SetOpen(bool open)
{
	if (open == m_bOpen)
		return;
	m_bOpen = open;
	if (open)
		s_Open.push_back(this);
	else
		s_Open.erase(std::remove(s_Open.begin(), s_Open.end(), this), s_Open.end());
	const bool holdEsc = open && m_bEscCloses;
	if (holdEsc != m_bEscCloseHeld)
	{
		HoldEscape(holdEsc); // Esc closes it (not the game menu)
		m_bEscCloseHeld = holdEsc;
	}
	SetMouseInputEnabled(!(open && s_bMouseLook)); // a window opened during mouse look waits for it to end
	if (s_Open.empty() && s_bMouseLook)
		SetMouseLook(false); // the hint and the Esc guard go too
	if (s_Open.empty())
	{
		// the last window closed: a guard a field is still letting go of goes now (no EscapeTick runs any more)
		for (; s_iEscReleases > 0; s_iEscReleases--)
			HoldEscape(false);
		// the last window closed: no mouse look of the tools is left over
		s_bMouseLook = s_bMouseLookToggled = false;
	}
	if (open && !s_bMouseLook)
		sbo::ReleaseMouseForCursor(); // the mouse leaves the view: the cursor moves freely
	vgui2::surface()->CalculateMouseVisible();
}

bool CSboToolWindow::IsMouseLook()
{
	return s_bMouseLook;
}

void CSboToolWindow::SetMouseLook(bool on)
{
	if (on == s_bMouseLook)
		return;
	if (on)
		vgui2::input()->GetCursorPos(s_iLookCursorX, s_iLookCursorY);
	s_bMouseLook = on;
	// no tool window takes the mouse: VGUI2 hides the cursor and the game turns the view (IN_MouseMove re-centres
	// and drops the movement the cursor made); taken back, the cursor shows again where it was
	for (CSboToolWindow *p : s_Open)
		p->SetMouseInputEnabled(!on);
	// Esc ends it: the engine opens the game menu on Esc before the client sees the key (HL25), so the game menu's
	// Esc guard hides it again at once and tells (OnThink, TakeMenuEscape), as for the server menus
	if (on != s_bEscGuard)
	{
		HoldEscape(on);
		s_bEscGuard = on;
	}
	if (!s_pHint && on && g_pViewport)
		s_pHint = new CSboToolHint(g_pViewport);
	if (s_pHint)
		s_pHint->Show(on);
	vgui2::surface()->CalculateMouseVisible();
	if (!on)
	{
		sbo::ReleaseMouseForCursor();
		vgui2::input()->SetCursorPos(s_iLookCursorX, s_iLookCursorY);
	}
}

void CSboToolWindow::Open()
{
	UpdateStyle(this);
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	const int wide = std::min(M(m_iMockWide), sw), tall = std::min(M(m_iMockTall), sh);
	KeyValues *kv = Saved()->FindKey(m_Key.c_str());
	if (kv && kv->GetInt("screenwide") > 0 && kv->GetInt("screentall") > 0)
	{
		// the place the player gave it; saved at another resolution: the same place relative to the screen
		int x = kv->GetInt("x"), y = kv->GetInt("y");
		int w = m_bSizeable && kv->GetInt("wide") > 0 ? kv->GetInt("wide") : wide;
		int h = m_bSizeable && kv->GetInt("tall") > 0 ? kv->GetInt("tall") : tall;
		const int ssw = kv->GetInt("screenwide"), ssh = kv->GetInt("screentall");
		if (ssw != sw || ssh != sh)
		{
			x = x * sw / ssw;
			y = y * sh / ssh;
			if (m_bSizeable)
			{
				w = w * sw / ssw;
				h = h * sh / ssh;
			}
		}
		if (m_bSizeable)
		{
			w = std::max(w, wide * 2 / 3);
			h = std::max(h, tall * 2 / 3);
		}
		SetBounds(x, y, w, h);
	}
	else
		SetBounds((int)(m_flPlaceX * sw), (int)(m_flPlaceY * sh), wide, tall);
	// a window always opens unfolded (a fold lasts only while the window is open; its size and place are kept)
	for (vgui2::VPANEL v : m_HiddenContent)
		vgui2::ipanel()->SetVisible(v, true);
	m_HiddenContent.clear();
	m_bCollapsed = false;
	SetSizeable(m_bSizeable);
	if (m_bSizeable)
		SetMinimumSize(wide * 2 / 3, tall * 2 / 3);
	KeepOnScreen();
	static_cast<CSboFoldArrow *>(m_pArrow)->SetFolded(false);
	GetBounds(m_iSavedX, m_iSavedY, m_iSavedW, m_iSavedH);
	SetOpen(true);
	InvalidateLayout(true, true);
	Activate();
}

void CSboToolWindow::CloseTool()
{
	if (!m_bOpen)
		return;
	EndTextInput();
	SaveBounds();
	SetVisible(false);
	SetOpen(false);
	if (s_pTipOwner == this)
		HideTip();
	OnToolClosed();
}

void CSboToolWindow::OnClose()
{
	// the close button ("Close"): hidden, not deleted (SetDeleteSelfOnClose(false))
	CloseTool();
}

void CSboToolWindow::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "Close"))
	{
		CloseTool();
		return;
	}
	if (!Q_stricmp(command, "ToolFold"))
	{
		SetCollapsed(!m_bCollapsed);
		return;
	}
	if (!Q_stricmp(command, "ToolMouseLook"))
	{
		s_bMouseLookToggled = true;
		SetMouseLook(true);
		return;
	}
	BaseClass::OnCommand(command);
}

// An Esc the guard caught (the game menu stayed shut) ends the mode waiting for it: a pick on the screen first, then
// typing in a field or a list, then mouse look, then the newest window that Esc closes (SetEscapeCloses). Without
// such a mode or window the guard is off and Esc opens the menu as usual.
void CSboToolWindow::EscapeTick()
{
	const bool releasing = s_iEscReleases > 0;
	if (releasing && vgui2::system()->GetCurrentTime() >= s_flEscReleaseAt)
	{
		for (; s_iEscReleases > 0; s_iEscReleases--)
			HoldEscape(false);
	}
	if (s_iEscHolders <= 0 || !g_pGameUIViewport || !g_pGameUIViewport->TakeMenuEscape())
		return;
	if (releasing)
		return; // the Esc that ended the typing: done
	if (s_pPicking)
	{
		s_pPicking->Cancel();
		return;
	}
	for (CSboToolWindow *p : s_Open)
		if (p->m_pTextField)
		{
			p->EndTextInput();
			return;
		}
	if (s_bMouseLook)
	{
		s_bMouseLookToggled = false;
		SetMouseLook(false);
		return;
	}
	for (auto it = s_Open.rbegin(); it != s_Open.rend(); ++it)
		if ((*it)->m_bEscCloses)
		{
			(*it)->OnEscape();
			return;
		}
}

// The resize arrows. The frame's edge grips would set the engine's own sizing cursors, but the engine does not go back
// to the normal arrow once one was set (seen on HL25 Linux: the arrow stayed everywhere), so the grips keep the normal
// arrow and the window draws the resize arrow for that edge beside the mouse (DrawResizeArrow) while the mouse is on
// the edge or a resize is under way. The grips must not hide the cursor: while the panel under the mouse has no
// cursor, the engine stops moving VGUI2's cursor, so a press after resting on an edge started from a stale place.
void CSboToolWindow::UpdateResizeCursor()
{
	static vgui2::VPANEL s_LastOver = 0;
	const vgui2::VPANEL capture = vgui2::input()->GetMouseCapture();
	const vgui2::VPANEL over = capture ? capture : vgui2::input()->GetMouseOver();
	int kind = -1; // RESIZE_*
	int dx = 0, dy = 0; // the edge's outer side
	if (!s_bMouseLook && over)
	{
		const char *name = vgui2::ipanel()->GetName(over);
		if (name && !Q_strncmp(name, "frame_", 6) && Q_strstr(name, "Grip"))
		{
			vgui2::VPANEL parent = vgui2::ipanel()->GetParent(over);
			for (CSboToolWindow *w : s_Open)
				if (w->GetVPanel() == parent && w->m_bSizeable && !w->m_bCollapsed)
				{
					if (!Q_stricmp(name, "frame_topGrip") || !Q_stricmp(name, "frame_bottomGrip"))
						kind = RESIZE_NS;
					else if (!Q_stricmp(name, "frame_leftGrip") || !Q_stricmp(name, "frame_rightGrip"))
						kind = RESIZE_WE;
					else if (!Q_stricmp(name, "frame_tlGrip") || !Q_stricmp(name, "frame_brGrip"))
						kind = RESIZE_NWSE;
					else
						kind = RESIZE_NESW;
					// frame_<t|b><l|r>Grip, frame_leftGrip, ...: the side the window grows to
					const char *side = name + 6;
					if (!Q_strncmp(side, "left", 4))
						dx = -1;
					else if (!Q_strncmp(side, "right", 5))
						dx = 1;
					else if (!Q_strncmp(side, "top", 3))
						dy = -1;
					else if (!Q_strncmp(side, "bottom", 6))
						dy = 1;
					else
					{
						dy = side[0] == 't' ? -1 : 1;
						dx = side[1] == 'l' ? -1 : 1;
					}
				}
		}
	}
	// the arrow, in the window whose edge it is (inside it, next to the mouse); every other window draws none
	CSboToolWindow *edgeWindow = nullptr;
	if (kind >= 0)
		for (CSboToolWindow *w : s_Open)
			if (w->GetVPanel() == vgui2::ipanel()->GetParent(over))
				edgeWindow = w;
	for (CSboToolWindow *w : s_Open)
	{
		int gx = 0, gy = 0;
		const int gk = w == edgeWindow ? kind : -1;
		if (gk >= 0)
		{
			const int size = std::max(16, S().M(22)), gap = S().M(6);
			int cx, cy;
			vgui2::input()->GetCursorPos(cx, cy);
			w->ScreenToLocal(cx, cy);
			gx = dx < 0 ? cx + gap : (dx > 0 ? cx - gap - size : cx - size / 2);
			gy = dy < 0 ? cy + gap : (dy > 0 ? cy - gap - size : cy - size / 2);
		}
		if (gk != w->m_iArrowKind || gx != w->m_iArrowX || gy != w->m_iArrowY)
		{
			w->m_iArrowKind = gk;
			w->m_iArrowX = gx;
			w->m_iArrowY = gy;
			w->Repaint();
		}
	}
	if (over != s_LastOver)
	{
		s_LastOver = over;
		if (kind < 0 && !s_bMouseLook)
			vgui2::surface()->SetCursor(vgui2::dc_arrow); // off an edge: the normal cursor again
	}
}

//! The grips keep the normal arrow (the frame gives them the engine's sizing cursors; see UpdateResizeCursor)
void CSboToolWindow::SetGripCursors()
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		vgui2::Panel *p = GetChild(i);
		const char *name = p ? p->GetName() : nullptr;
		if (name && !Q_strncmp(name, "frame_", 6) && Q_strstr(name, "Grip"))
			p->SetCursor(vgui2::dc_arrow);
	}
}

void CSboToolWindow::PostChildPaint()
{
	BaseClass::PostChildPaint();
	if (m_iArrowKind >= 0)
		DrawResizeArrow(m_iArrowKind, m_iArrowX, m_iArrowY, std::max(16, S().M(22)));
}

void CSboToolWindow::BeginTextInput(vgui2::Panel *pField)
{
	if (!m_bEscHeld)
	{
		HoldEscape(true); // Esc ends the typing (not the game menu)
		m_bEscHeld = true;
	}
	m_pTextField = pField;
	m_bFieldHadFocus = false;
	m_flTextInputSince = vgui2::system()->GetCurrentTime();
	SetKeyBoardInputEnabled(true);
	if (pField)
		pField->RequestFocus();
}

void CSboToolWindow::EndTextInput()
{
	if (!m_pTextField && !IsKeyBoardInputEnabled())
		return;
	m_pTextField = nullptr;
	if (m_bEscHeld)
	{
		s_iEscReleases++; // EscapeTick lets go of it shortly
		s_flEscReleaseAt = vgui2::system()->GetCurrentTime() + ESC_RELEASE_DELAY;
		m_bEscHeld = false;
	}
	SetKeyBoardInputEnabled(false);
	// the keyboard goes back to the game
	vgui2::input()->SetMouseCapture(0);
	RequestFocus(); // takes the focus from the field (the window has no keyboard input: no key goes to it)
}

void CSboToolWindow::OnMousePressed(vgui2::MouseCode code)
{
	// a click on the window outside a text field ends its typing
	if (m_pTextField)
		EndTextInput();
	HideTip();
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	if (code == vgui2::MOUSE_LEFT && y < M(26))
	{
		// the header moves the window
		int cx, cy;
		vgui2::input()->GetCursorPos(cx, cy);
		int wx, wy;
		GetPos(wx, wy);
		m_iDragX = cx - wx;
		m_iDragY = cy - wy;
		m_bDragging = true;
		vgui2::input()->SetMouseCapture(GetVPanel());
	}
	BaseClass::OnMousePressed(code);
}

void CSboToolWindow::KeepOnScreen()
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

void CSboToolWindow::SaveBounds()
{
	int x, y, w, h;
	GetBounds(x, y, w, h);
	if (m_bCollapsed)
		h = m_iExpandedTall;
	KeyValues *kv = Saved()->FindKey(m_Key.c_str(), true);
	kv->SetInt("x", x);
	kv->SetInt("y", y);
	kv->SetInt("wide", w);
	kv->SetInt("tall", h);
	if (KeyValues *old = kv->FindKey("collapsed"))
	{
		// no longer kept (an earlier build saved it)
		kv->RemoveSubKey(old);
		old->deleteThis();
	}
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	kv->SetInt("screenwide", sw);
	kv->SetInt("screentall", sh);
	Saved()->SaveToFile(g_pFullFileSystem, TOOLS_FILE, TOOLS_PATH_ID);
}

void CSboToolWindow::ShowMouseLookButton(bool show)
{
	if (show && !m_pLook)
	{
		m_pLook = new CSboBrowserButton(this, "MouseLook", "Mouse look", "ToolMouseLook", CSboBrowserButton::KIND_NORMAL);
		SetToolTip(m_pLook, "Look around with the mouse while the windows stay open, Esc gives the cursor back");
	}
	if (m_pLook)
		m_pLook->SetVisible(show);
	InvalidateLayout();
}

void CSboToolWindow::OnThink()
{
	BaseClass::OnThink();
	if (!m_bOpen)
		return;
	UpdateToolTip();
	if (m_bCollapsed)
		HideContent(); // a control the tool showed while folded waits too
	EscapeTick();
	UpdateResizeCursor();
	int x, y, w, h;
	GetBounds(x, y, w, h);
	const double now = vgui2::system()->GetCurrentTime();
	if (x != m_iSavedX || y != m_iSavedY || w != m_iSavedW || h != m_iSavedH)
	{
		m_iSavedX = x;
		m_iSavedY = y;
		m_iSavedW = w;
		m_iSavedH = h;
		m_flSaveAt = now + 1.0; // written once the moving / sizing stops
	}
	if (m_flSaveAt > 0.0 && now >= m_flSaveAt)
	{
		m_flSaveAt = 0.0;
		SaveBounds();
	}
	// a text field that lost the focus (a click elsewhere, a window closed) gives the keyboard back; the focus moves
	// a frame after RequestFocus, so only once the field had it (or never got it within a second)
	if (m_pTextField)
	{
		const vgui2::VPANEL focus = vgui2::input()->GetFocus();
		const bool has = focus && (focus == m_pTextField->GetVPanel() ||
		                              vgui2::ipanel()->HasParent(focus, m_pTextField->GetVPanel()));
		if (has)
			m_bFieldHadFocus = true;
		else if (m_bFieldHadFocus || now - m_flTextInputSince > 1.0)
			EndTextInput();
	}
}

void CSboToolWindow::OnScreenSizeChanged(int oldWide, int oldTall)
{
	BaseClass::OnScreenSizeChanged(oldWide, oldTall);
	UpdateStyle(this);
	if (!m_bSizeable)
	{
		int x, y;
		GetPos(x, y);
		SetBounds(x, y, M(m_iMockWide), m_bCollapsed ? HeaderTall() : M(m_iMockTall));
	}
	KeepOnScreen();
	InvalidateLayout(true, true);
}

void CSboToolWindow::PerformLayout()
{
	BaseClass::PerformLayout();
	const int W = GetWide(), H = GetTall();
	const int pad = M(10), header = M(26);
	const int closeSize = M(20);
	m_pClose->SetBounds(W - pad - closeSize + M(4), (header - closeSize) / 2 + M(2), closeSize, closeSize);
	m_pArrow->SetBounds(M(4), (header - M(16)) / 2 + M(2), M(16), M(16));
	if (m_pLook && m_pLook->IsVisible())
	{
		const int lw = m_pLook->GetContentWide() - M(12), lh = M(20);
		m_pLook->SetBounds(m_pClose->GetXPos() - M(6) - lw, (header - lh) / 2 + M(2), lw, lh);
	}
	SetGripCursors(); // the frame sets its sizing cursors again whenever its sizeable state changes
	if (!m_bCollapsed)
		LayoutContent(pad, header + M(6), W - 2 * pad, H - header - M(6) - pad);
	// the header is this window's own (moving, a double click folds it): the frame's caption grip is not used
	if (vgui2::Panel *pCaption = FindChildByName("frame_caption"))
		pCaption->SetBounds(0, 0, 0, 0);
}

void CSboToolWindow::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_WINDOW);
}

void CSboToolWindow::PaintBorder()
{
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
}

void CSboToolWindow::Paint()
{
	const int pad = M(10), header = M(26);
	const int titleEnd = (m_pLook && m_pLook->IsVisible()) ? m_pLook->GetXPos() : m_pClose->GetXPos();
	const int titleX = M(22);
	DrawTextMid(S().name, COL_TEXT, titleX, M(2), header, Widen(m_Title.c_str()), titleEnd - titleX - pad);
	if (m_bCollapsed)
		return;
	FillRect(pad, header + M(2), GetWide() - 2 * pad, 1, COL_BORDER_LIST);
	if (m_bSizeable)
	{
		// resize grip
		const int W = GetWide(), H = GetTall(), gs = M(4);
		vgui2::surface()->DrawSetColor(COL_BORDER);
		for (int line = 1; line <= 3; line++)
		{
			const int len = line * gs;
			for (int i = 0; i < len; i++)
				vgui2::surface()->DrawFilledRect(W - 3 - len + i, H - 3 - i - 1, W - 3 - len + i + 1, H - 3 - i);
		}
	}
}

//-----------------------------------------------------------------------------
// CSboChoiceDialog
//-----------------------------------------------------------------------------
CSboChoiceDialog::CSboChoiceDialog()
    : BaseClass(g_pViewport, "SboChoice", "", "tool_choice", 360, 130, 0.36f, 0.4f)
{
	SetToolSizeable(false);
}

CSboChoiceDialog *CSboChoiceDialog::Get()
{
	static CSboChoiceDialog *s_pDialog = nullptr;
	if (!s_pDialog)
		s_pDialog = new CSboChoiceDialog();
	return s_pDialog;
}

void CSboChoiceDialog::Ask(const char *title, const char *message, const std::vector<std::string> &buttons,
    std::function<void(int)> done)
{
	if (IsOpen())
		Finish((int)m_Buttons.size() - 1); // a question still open: its last choice (cancel)
	SetToolTitle(title);
	m_Message = message ? message : "";
	for (CSboBrowserButton *b : m_Buttons)
		b->MarkForDeletion();
	m_Buttons.clear();
	for (size_t i = 0; i < buttons.size(); i++)
	{
		char cmd[32];
		snprintf(cmd, sizeof(cmd), "choice %d", (int)i);
		m_Buttons.push_back(new CSboBrowserButton(this, "Choice", buttons[i].c_str(), cmd,
		    i == 0 ? CSboBrowserButton::KIND_PRIMARY : CSboBrowserButton::KIND_NORMAL));
	}
	m_Done = done;
	Open();
	if (IsCollapsed())
		SetCollapsed(false);
	MoveToFront();
}

void CSboChoiceDialog::Finish(int choice)
{
	if (m_bFinishing)
		return;
	m_bFinishing = true;
	std::function<void(int)> done = m_Done;
	m_Done = nullptr;
	CloseTool();
	m_bFinishing = false;
	if (done)
		done(choice);
}

void CSboChoiceDialog::OnCommand(const char *command)
{
	if (!Q_strncmp(command, "choice ", 7))
	{
		Finish(atoi(command + 7));
		return;
	}
	BaseClass::OnCommand(command);
}

void CSboChoiceDialog::OnToolClosed()
{
	// the close button: the last choice (cancel)
	if (m_Done && !m_bFinishing)
	{
		std::function<void(int)> done = m_Done;
		m_Done = nullptr;
		done((int)m_Buttons.size() - 1);
	}
}

void CSboChoiceDialog::LayoutContent(int x, int y, int w, int h)
{
	m_iMessageY = y + M(4);
	const int n = (int)m_Buttons.size(), gap = M(6), bh = M(24);
	if (!n)
		return;
	const int bw = std::min(M(100), (w - gap * (n - 1)) / n);
	int bx = x + w - n * bw - (n - 1) * gap;
	for (CSboBrowserButton *b : m_Buttons)
	{
		b->SetBounds(bx, y + h - bh, bw, bh);
		bx += bw + gap;
	}
}

void CSboChoiceDialog::Paint()
{
	BaseClass::Paint();
	if (!IsCollapsed())
		DrawText(S().body, COL_TEXT_LIGHT, M(10), m_iMessageY, Widen(m_Message.c_str()), GetWide() - M(20));
}

//-----------------------------------------------------------------------------
// CSboScreenPicker
//-----------------------------------------------------------------------------
CSboScreenPicker::CSboScreenPicker(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboScreenPicker")
{
	MakePopup(false, false);
	SetPaintBackgroundEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(true);
	SetVisible(false);
}

void CSboScreenPicker::Start(vgui2::Panel *pTarget)
{
	m_Target = pTarget ? pTarget->GetVPanel() : 0;
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	SetBounds(0, 0, sw, sh);
	SetVisible(true);
	vgui2::ipanel()->MoveToBack(GetVPanel()); // behind the tool windows: they still take their own clicks
	vgui2::surface()->CalculateMouseVisible();
	if (s_pPicking != this)
	{
		if (!s_pPicking)
			HoldEscape(true); // Esc cancels the pick (not the game menu)
		s_pPicking = this;
	}
}

void CSboScreenPicker::Done()
{
	if (s_pPicking == this)
	{
		s_pPicking = nullptr;
		HoldEscape(false);
	}
}

void CSboScreenPicker::Cancel()
{
	if (!IsVisible())
		return;
	SetVisible(false);
	Done();
	if (m_Target)
		vgui2::ivgui()->PostMessage(m_Target, new KeyValues("ScreenPickCancelled"), GetVPanel());
	m_Target = 0;
	vgui2::surface()->CalculateMouseVisible();
}

void CSboScreenPicker::OnMousePressed(vgui2::MouseCode code)
{
	if (code == vgui2::MOUSE_RIGHT)
	{
		Cancel();
		return;
	}
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	SetVisible(false);
	Done();
	if (m_Target)
		vgui2::ivgui()->PostMessage(m_Target, new KeyValues("ScreenPicked", "x", x, "y", y), GetVPanel());
	m_Target = 0;
	vgui2::surface()->CalculateMouseVisible();
}

void CSboScreenPicker::OnKeyCodePressed(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
		Cancel();
}

void CSboScreenPicker::Paint()
{
	// the hint, at the top of the screen
	const wchar_t *hint = L"Click on the screen to pick, or press Esc or right-click to cancel";
	const int tw = TextWide(S().body, hint), pad = S().M(8), h = S().M(26);
	const int x = (GetWide() - tw) / 2 - pad, y = S().M(40);
	FillRect(x, y, tw + 2 * pad, h, COL_BG_WINDOW);
	OutlineRect(x, y, tw + 2 * pad, h, COL_BORDER);
	DrawTextMid(S().body, COL_TEXT, x + pad, y, h, hint);
}
