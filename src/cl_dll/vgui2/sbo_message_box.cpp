//========= SBO VGUI2: game_messagebox message boxes =========//
//
// Purpose: see sbo_message_box.h.
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/ScrollBarSlider.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_menus.h"
#include "sbo_message_box.h"
#include "sbo_widgets.h"
#include "vgui2_api.h"

namespace
{
// featureful WindowGeometry (game_shared/window_geometry.h): fractions of the screen and of the box
constexpr float BOX_WIDTH = 7.0f / 9.0f;
constexpr float BOX_HEIGHT = 11.0f / 15.0f;
constexpr float PADDING_HORIZONTAL = 1.0f / 14.0f;
constexpr float PADDING_VERTICAL = 1.0f / 13.0f;

// featureful CHudMessageBox::HandleClientInput: a press this soon after the box arrived doesn't close it
constexpr double CLOSE_DELAY = 0.1;

// A wheel step that came through HUD_Key_Event: VGUI2's own wheel message for the same step is ignored this long
constexpr double WHEEL_DEDUP_TIME = 0.25;

// The longest text taken (bytes as received, characters after the lookup); the server sends up to what the map's
// keyvalue holds (1023 bytes)
constexpr int MAX_TEXT_BYTES = 4096;

// 640x480 units to pixels at the screen's height (the proportional fonts' scale)
int Scaled(float value)
{
	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	return (int)(value * tall / 480.0f + 0.5f);
}
} // namespace

// The Close button: SBO's window button (CSboCommandButton: its colors, green line, armed look), with the label in
// "MessageBox.ButtonFont", proportional like the box's text, so that the label grows with the button (the text
// scheme's "Primary Button Text" stops growing at 1024 pixels wide: 27 pixels from there on)
class CSboMessageBoxButton : public CSboCommandButton
{
	DECLARE_CLASS_SIMPLE(CSboMessageBoxButton, CSboCommandButton);

public:
	CSboMessageBoxButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget, const char *command)
	    : BaseClass(pParent, pName, pActionTarget, command)
	{
	}

	void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);

		vgui2::HFont font = pScheme->GetFont("MessageBox.ButtonFont", true);
		if (font != vgui2::INVALID_FONT)
			SetFont(font);
	}
};

CSboMessageBox *CSboMessageBox::s_pInstance = nullptr;

CSboMessageBox *CSboMessageBox::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboMessageBox(pParent);
	return s_pInstance;
}

CSboMessageBox::CSboMessageBox(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboMessageBox")
{
	s_pInstance = this;

	// A popup gets mouse input although CClientViewport has none (as the scoreboard and SBO's menus). It takes the
	// mouse only while a box is shown (UpdateActive) and never the keyboard: the game keeps the keys (movement,
	// chat, ...); Enter / PageUp / PageDown / the wheel come through HUD_Key_Event, the attack through +attack.
	MakePopup(false, false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetProportional(false); // the fonts are chosen in ApplySchemeSettings

	if (sboui::GetScheme())
		SetScheme(sboui::GetScheme());

	m_pScrollBar = new vgui2::ScrollBar(this, "ScrollBar", true);
	m_pScrollBar->AddActionSignalTarget(this);
	m_pScrollBar->SetButtonPressedScrollValue(1);
	m_pScrollBar->SetVisible(false);

	m_pCloseButton = new CSboMessageBoxButton(this, "CloseButton", this, "close");
	m_pCloseButton->SetScheme(GetScheme());
	wchar_t wbuf[64];
	sboui::Localize("#SBO_MessageBox_Close", wbuf, sizeof(wbuf));
	m_pCloseButton->SetCommandText(wbuf);
	m_pCloseButton->SetBoundKey((char)255);
	m_pCloseButton->SetCommandAlignment(vgui2::Label::a_center);
	m_pCloseButton->SetVisible(false);

	sboui::ApplySymbolSubstitutes(this); // Linux: the scroll bar's arrow buttons (Marlett is not drawn there)

	// Always visible (VGUI2 doesn't think invisible panels), but without a size while no box is shown: a popup that
	// covers the screen could stand in the way of the other popups' clicks (UpdateActive)
	SetBounds(0, 0, 0, 0);
	SetVisible(true);
}

void CSboMessageBox::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0, 0, 0, 0)); // the box is drawn in PaintBackground

	// proportional: the size in the scheme is for a 480-pixel-high screen, it grows with the screen height
	m_hFont = pScheme->GetFont("MessageBox.TextFont", true);
	if (m_hFont == vgui2::INVALID_FONT)
		m_hFont = pScheme->GetFont("Default", true);
	m_hTitleFont = pScheme->GetFont("MessageBox.TitleFont", true);
	if (m_hTitleFont == vgui2::INVALID_FONT)
		m_hTitleFont = m_hFont;

	// text and frame in the HUD color (featureful), background black at alpha 240 (featureful: 160) so the text stays
	// readable over a bright scene
	m_TextColor = pScheme->GetColor("MessageBox.TextColor", Color(47, 145, 60, 255));
	m_BorderColor = pScheme->GetColor("MessageBox.BorderColor", Color(47, 145, 60, 255));
	m_BgColor = pScheme->GetColor("MessageBox.BgColor", Color(0, 0, 0, 240));
	m_TitleColor = pScheme->GetColor("MessageBox.TitleColor", Color(47, 145, 60, 255));
	m_TitleBgColor = pScheme->GetColor("MessageBox.TitleBgColor", Color(47, 145, 60, 40));

	Layout(true); // the new fonts
}

//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CSboMessageBox::Layout(bool force)
{
	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	const bool hasTitle = !m_Boxes.empty() && m_Boxes.back().title.size() > 1;
	if (!force && wide == m_iLayoutWide && tall == m_iLayoutTall && hasTitle == m_bLayoutTitle)
		return;

	m_iLayoutWide = wide;
	m_iLayoutTall = tall;
	m_bLayoutTitle = hasTitle;
	if (m_bActive)
		SetBounds(0, 0, wide, tall);

	m_iBoxWide = (int)(BOX_WIDTH * wide);
	m_iBoxTall = (int)(BOX_HEIGHT * tall);
	m_iBoxX = (wide - m_iBoxWide) / 2;
	m_iBoxY = (tall - m_iBoxTall) / 2;
	const int padX = (int)(PADDING_HORIZONTAL * m_iBoxWide);
	const int padY = (int)(PADDING_VERTICAL * m_iBoxTall);
	const int margin = Scaled(8);

	m_iLineTall = m_hFont != vgui2::INVALID_FONT ? vgui2::surface()->GetFontTall(m_hFont) : 0;
	if (m_iLineTall < 1)
		m_iLineTall = 1;

	// title bar: the title font and a margin above and below
	m_iTitleTall = 0;
	if (hasTitle && m_hTitleFont != vgui2::INVALID_FONT)
		m_iTitleTall = vgui2::surface()->GetFontTall(m_hTitleFont) + 2 * Scaled(5);

	// Close button: bottom right, the GM menu's Return size (80 x 20 at 640x480), scaled with the screen height like
	// its label's font (MessageBox.ButtonFont)
	const int buttonWide = Scaled(80);
	const int buttonTall = Scaled(20);
	const int buttonX = m_iBoxX + m_iBoxWide - padX - buttonWide;
	const int buttonY = m_iBoxY + m_iBoxTall - margin - buttonTall;
	m_iButtonX = buttonX;
	m_iButtonY = buttonY;
	m_iButtonWide = buttonWide;
	m_iButtonTall = buttonTall;

	// text area: featureful's paddings at the sides and at the top (below the title bar: a margin), down to a margin
	// above the button
	m_iTextX = m_iBoxX + padX;
	m_iTextY = m_iTitleTall ? m_iBoxY + m_iTitleTall + margin : m_iBoxY + padY;
	m_iTextWide = m_iBoxWide - 2 * padX;
	m_iTextTall = buttonY - margin - m_iTextY;
	if (m_iTextTall < m_iLineTall)
		m_iTextTall = m_iLineTall;

	m_iRows = m_iTextTall / m_iLineTall;
	if (m_iRows < 1)
		m_iRows = 1;

	// the scroll bar on the right of the text area (sboscheme.res styles it as the MOTD's)
	m_iScrollWide = Scaled(12);
	if (m_iScrollWide < 16)
		m_iScrollWide = 16;

	PlaceControls();

	if (!m_Boxes.empty())
		Wrap(m_Boxes.back());
	UpdateScrollBar();
}

// The Close button and the scroll bar at the bounds Layout computed (in the popup, which covers the screen from 0,0
// while a box is shown). Also from PerformLayout: VGUI2 lays a panel out again after its size changed.
void CSboMessageBox::PlaceControls()
{
	m_pCloseButton->SetBounds(m_iButtonX, m_iButtonY, m_iButtonWide, m_iButtonTall);
	m_pScrollBar->SetBounds(m_iTextX + m_iTextWide - m_iScrollWide, m_iTextY, m_iScrollWide, m_iRows * m_iLineTall);
}

void CSboMessageBox::PerformLayout()
{
	BaseClass::PerformLayout();
	PlaceControls();
}

// EditablePanel::OnSizeChanged moves every child to its pin offset (top left, 0 0: nothing here sets one) whenever the
// panel's size changes. The popup changes size each time a box is shown (0 x 0 -> the screen, UpdateActive), so a
// box shown from nothing had its Close button and scroll bar moved to the screen's top left after Layout had placed
// them; a second box arriving while one was shown (no size change) got them placed again. The children are placed
// here, not pinned: skip EditablePanel's pinning, lay out again.
void CSboMessageBox::OnSizeChanged(int wide, int tall)
{
	vgui2::Panel::OnSizeChanged(wide, tall);
	InvalidateLayout();
}

// Lines no wider than `wide`: broken at "\n", else at the last space that fits (dropped), else inside a word that
// fills the line alone (featureful CHud::UtfText::CalcLineOffsets)
void CSboMessageBox::WrapLines(Box &box, int wide) const
{
	box.lines.clear();

	if (m_hFont == vgui2::INVALID_FONT || box.text.size() <= 1)
		return;

	const wchar_t *text = box.text.data();
	const int maxWide = wide > 1 ? wide : 1;
	int pos = 0;

	for (;;)
	{
		const int start = pos;
		int lineWide = 0;
		int lastSpace = -1;

		while (text[pos] && text[pos] != L'\n')
		{
			const int charWide = vgui2::surface()->GetCharacterWidth(m_hFont, text[pos]);
			if (lineWide + charWide > maxWide && pos > start)
				break;

			if (text[pos] == L' ')
				lastSpace = pos;

			lineWide += charWide;
			pos++;
		}

		Line line;
		line.start = start;
		int next;
		if (text[pos] && text[pos] != L'\n')
		{
			// too wide
			if (lastSpace > start)
			{
				line.len = lastSpace - start;
				next = lastSpace + 1;
			}
			else
			{
				line.len = pos - start;
				next = pos;
			}
		}
		else
		{
			line.len = pos - start;
			next = text[pos] ? pos + 1 : pos;
		}

		box.lines.push_back(line);

		if (!text[next])
			break;

		pos = next;
	}
}

// The whole width; a text that doesn't fit is wrapped again beside the scroll bar
void CSboMessageBox::Wrap(Box &box) const
{
	WrapLines(box, m_iTextWide);
	if ((int)box.lines.size() > m_iRows)
		WrapLines(box, m_iTextWide - m_iScrollWide - Scaled(6));

	if (box.scroll > MaxScroll(box))
		box.scroll = MaxScroll(box);
	if (box.scroll < 0)
		box.scroll = 0;
}

int CSboMessageBox::MaxScroll(const Box &box) const
{
	const int maxScroll = (int)box.lines.size() - m_iRows;
	return maxScroll > 0 ? maxScroll : 0;
}

void CSboMessageBox::UpdateScrollBar()
{
	const bool needed = m_bActive && !m_Boxes.empty() && (int)m_Boxes.back().lines.size() > m_iRows;
	m_pScrollBar->SetVisible(needed);
	if (!needed)
		return;

	const Box &box = m_Boxes.back();
	m_pScrollBar->SetRange(0, (int)box.lines.size());
	m_pScrollBar->SetRangeWindow(m_iRows);
	if (m_pScrollBar->GetValue() != box.scroll)
		m_pScrollBar->SetValue(box.scroll);
}

void CSboMessageBox::SetScroll(int scroll)
{
	if (m_Boxes.empty())
		return;

	Box &box = m_Boxes.back();
	if (scroll > MaxScroll(box))
		scroll = MaxScroll(box);
	if (scroll < 0)
		scroll = 0;
	box.scroll = scroll;
	UpdateScrollBar();
}

void CSboMessageBox::OnSliderMoved(int position)
{
	if (m_Boxes.empty() || position == m_Boxes.back().scroll)
		return;

	SetScroll(position);
}

//-----------------------------------------------------------------------------
// Boxes
//-----------------------------------------------------------------------------
void CSboMessageBox::Open(int id, const wchar_t *title, const wchar_t *text)
{
	for (size_t i = 0; i < m_Boxes.size(); i++)
	{
		if (m_Boxes[i].id == id)
		{
			m_Boxes.erase(m_Boxes.begin() + i);
			break;
		}
	}

	Box box;
	box.id = id;
	box.title.assign(title, title + wcslen(title) + 1);
	box.text.assign(text, text + wcslen(text) + 1);
	box.showTime = vgui2::system()->GetCurrentTime();
	m_Boxes.push_back(box);

	Layout(true); // the newest box: its title bar and lines
	UpdateActive();
}

void CSboMessageBox::Close(int id)
{
	for (size_t i = 0; i < m_Boxes.size(); i++)
	{
		if (m_Boxes[i].id == id)
		{
			m_Boxes.erase(m_Boxes.begin() + i);
			Layout(true);
			UpdateActive();
			return;
		}
	}
}

void CSboMessageBox::Reset()
{
	m_Boxes.clear();
	UpdateActive();
}

// featureful: only while the box is drawn (no "top level window": the scoreboard). SBO: the HUD is drawn (hud_draw,
// intermission, the HUD hidden, the viewport's full-screen menus: sbo::HudMenuShouldDraw, as the HUD text menu) and
// no menu has the mouse cursor (MOTD, briefing, team menu, the spectator duck menu): then the box gives the mouse
// back, and takes it again when they are gone.
bool CSboMessageBox::ShouldShow() const
{
	return !m_Boxes.empty() && sbo::HudMenuShouldDraw() && !sbo::IsMenuCursorVisible();
}

void CSboMessageBox::EnableChildInput(vgui2::Panel *pPanel)
{
	// the engine's vgui2 clears the children's mouse input together with the popup's (CSboMenuPanel::ShowMenu); and
	// no control may take the keyboard (a clicked button with keyboard input takes the focus from the game)
	for (int i = 0; i < pPanel->GetChildCount(); i++)
	{
		vgui2::Panel *pChild = pPanel->GetChild(i);
		if (!pChild)
			continue;
		pChild->SetMouseInputEnabled(true);
		pChild->SetKeyBoardInputEnabled(false);
		EnableChildInput(pChild);
	}
}

// The box takes the mouse while it is shown: a popup with mouse input makes the VGUI2 cursor visible, and the
// client's mouse look stops while it is (inputw32.cpp VGui2_IsCursorVisible, Windows and SDL alike), as for the
// scoreboard's mouse mode. When it gives the mouse back the cursor goes at once; inputw32.cpp re-centres the mouse
// (no view jump).
void CSboMessageBox::UpdateActive()
{
	const bool active = ShouldShow();
	if (active == m_bActive)
		return;

	m_bActive = active;
	if (active)
	{
		int wide, tall;
		vgui2::surface()->GetScreenSize(wide, tall);
		SetBounds(0, 0, wide, tall); // the whole screen: a click anywhere belongs to the box
		PlaceControls();
	}
	else
		SetBounds(0, 0, 0, 0);
	SetMouseInputEnabled(active);
	SetKeyBoardInputEnabled(false);
	m_pCloseButton->SetVisible(active);

	if (active)
	{
		EnableChildInput(this);
		// The button's colors come from SBO's text scheme (CSboCommandButton::ApplySchemeSettings), which only exists
		// once TeamFortressViewport does (the first HUD_VidInit). The box applied its scheme earlier, at HUD_Init
		// (MakeReadyForUse), so the button kept clientschemebase.res's Button colors (white, no armed background):
		// no highlight under the cursor. Re-applied at each show, as CSboMenuPanel::ShowMenu does for the menus.
		m_pCloseButton->InvalidateLayout(false, true);
		m_pCloseButton->SetArmed(false);
		MoveToFront();

		// the cursor appears in the middle of the screen (CScorePanel::EnableMousePointer)
		int wide, tall;
		vgui2::surface()->GetScreenSize(wide, tall);
		sbo::ReleaseMouseForCursor();
		vgui2::input()->SetCursorPos(wide / 2, tall / 2);
	}

	UpdateScrollBar();
	vgui2::surface()->CalculateMouseVisible();
}

bool CSboMessageBox::CloseNewest()
{
	if (!m_bActive || m_Boxes.empty())
		return false;

	const Box &box = m_Boxes.back();
	if (box.showTime + CLOSE_DELAY > vgui2::system()->GetCurrentTime())
		return false;

	char cmd[64];
	snprintf(cmd, sizeof(cmd), "close_messagebox %d\n", box.id);
	sbo::ExecClientCmd(cmd);

	m_Boxes.pop_back();
	Layout(true);
	UpdateActive();
	return true;
}

void CSboMessageBox::OnCommand(const char *command)
{
	if (!stricmp(command, "close"))
	{
		CloseNewest();
		return;
	}

	BaseClass::OnCommand(command);
}

// featureful ScrollUp / ScrollDown (one line), PageUp / PageDown (one box); SBO: Enter closes, the mouse buttons
// belong to the box (VGUI2 gets the clicks). Every key of these is taken while a box is shown.
bool CSboMessageBox::HandleKey(int action)
{
	if (!m_bActive || m_Boxes.empty())
		return false;

	const int scroll = m_Boxes.back().scroll;
	switch (action)
	{
	case VGUI2_MSGBOX_LINE_UP:
		m_flKeyWheelTime = vgui2::system()->GetCurrentTime();
		SetScroll(scroll - 1);
		break;
	case VGUI2_MSGBOX_LINE_DOWN:
		m_flKeyWheelTime = vgui2::system()->GetCurrentTime();
		SetScroll(scroll + 1);
		break;
	case VGUI2_MSGBOX_PAGE_UP: SetScroll(scroll - m_iRows); break;
	case VGUI2_MSGBOX_PAGE_DOWN: SetScroll(scroll + m_iRows); break;
	case VGUI2_MSGBOX_CLOSE: CloseNewest(); break;
	case VGUI2_MSGBOX_MOUSE_BUTTON: break;
	default: return false;
	}
	return true;
}

// The wheel over the box (the scroll bar and the button pass it up here). HUD_Key_Event normally gets the same
// step and has scrolled already: only a step that didn't come that way scrolls here.
void CSboMessageBox::OnMouseWheeled(int delta)
{
	if (!m_bActive || m_Boxes.empty())
		return;

	if (vgui2::system()->GetCurrentTime() - m_flKeyWheelTime < WHEEL_DEDUP_TIME)
		return;

	SetScroll(m_Boxes.back().scroll - delta);
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------
void CSboMessageBox::OnThink()
{
	BaseClass::OnThink();

	Layout(false);
	UpdateActive();

	// the cursor follows the visible popups only when something asks the surface to recalculate (the game menu
	// closing, the loading screen): re-check every frame while the box has the mouse (CSboMenuPanel::OnThink)
	if (m_bActive)
		vgui2::surface()->CalculateMouseVisible();
}

void CSboMessageBox::PaintBackground()
{
	// box here, text in Paint(): text rendering disables alpha blending afterwards (BHL kill feed)
	if (!m_bActive)
		return;

	vgui2::surface()->DrawSetColor(m_BgColor);
	vgui2::surface()->DrawFilledRect(m_iBoxX, m_iBoxY, m_iBoxX + m_iBoxWide, m_iBoxY + m_iBoxTall);

	if (m_iTitleTall)
	{
		vgui2::surface()->DrawSetColor(m_TitleBgColor);
		vgui2::surface()->DrawFilledRect(m_iBoxX, m_iBoxY, m_iBoxX + m_iBoxWide, m_iBoxY + m_iTitleTall);
		vgui2::surface()->DrawSetColor(m_BorderColor);
		vgui2::surface()->DrawFilledRect(m_iBoxX, m_iBoxY + m_iTitleTall - 1, m_iBoxX + m_iBoxWide, m_iBoxY + m_iTitleTall);
	}

	vgui2::surface()->DrawSetColor(m_BorderColor);
	vgui2::surface()->DrawOutlinedRect(m_iBoxX, m_iBoxY, m_iBoxX + m_iBoxWide, m_iBoxY + m_iBoxTall);
}

void CSboMessageBox::Paint()
{
	if (!m_bActive || m_Boxes.empty() || m_hFont == vgui2::INVALID_FONT)
		return;

	const Box &box = m_Boxes.back();

	// the title: one line, centered vertically in its bar, cut with "..." where it is too wide
	if (m_iTitleTall)
	{
		const wchar_t *title = box.title.data();
		const int x = m_iTextX;
		const int maxWide = m_iBoxX + m_iBoxWide - Scaled(8) - x;
		const int dotsWide = 3 * vgui2::surface()->GetCharacterWidth(m_hTitleFont, L'.');

		int len = 0, wide = 0, fit = 0;
		const int total = (int)wcslen(title);
		for (; len < total && title[len] != L'\n'; len++)
		{
			const int charWide = vgui2::surface()->GetCharacterWidth(m_hTitleFont, title[len]);
			if (wide + charWide + dotsWide <= maxWide)
				fit = len + 1;
			wide += charWide;
		}
		const bool cut = wide > maxWide || len < total;

		vgui2::surface()->DrawSetTextFont(m_hTitleFont);
		vgui2::surface()->DrawSetTextColor(m_TitleColor);
		vgui2::surface()->DrawSetTextPos(x, m_iBoxY + (m_iTitleTall - vgui2::surface()->GetFontTall(m_hTitleFont)) / 2);
		vgui2::surface()->DrawPrintText(title, cut ? fit : len);
		if (cut)
			vgui2::surface()->DrawPrintText(L"...", 3);
	}

	const int lineCount = (int)box.lines.size();
	int endRow = box.scroll + m_iRows;
	if (endRow > lineCount)
		endRow = lineCount;

	vgui2::surface()->DrawSetTextFont(m_hFont);
	vgui2::surface()->DrawSetTextColor(m_TextColor);

	const wchar_t *text = box.text.data();
	int y = m_iTextY;
	for (int i = box.scroll; i < endRow; i++)
	{
		if (box.lines[i].len > 0)
		{
			vgui2::surface()->DrawSetTextPos(m_iTextX, y);
			vgui2::surface()->DrawPrintText(text + box.lines[i].start, box.lines[i].len);
		}
		y += m_iLineTall;
	}
}

//-----------------------------------------------------------------------------
// Messages
//-----------------------------------------------------------------------------
namespace
{
// BEGIN_READ / READ_BYTE / READ_SHORT / READ_STRING
struct CMsgBoxReader
{
	const unsigned char *data;
	int size;
	int pos;

	int ReadByte()
	{
		if (pos + 1 > size)
		{
			pos = size + 1;
			return -1;
		}
		return data[pos++];
	}

	int ReadShort()
	{
		if (pos + 2 > size)
		{
			pos = size + 1;
			return -1;
		}
		short value = (short)(data[pos] | (data[pos + 1] << 8));
		pos += 2;
		return value;
	}

	const char *ReadString(char *buf, int bufSize)
	{
		int n = 0;
		while (pos < size)
		{
			char c = (char)data[pos++];
			if (!c)
				break;
			if (n < bufSize - 1)
				buf[n++] = c;
		}
		buf[n] = '\0';
		return buf;
	}
};

// The title and text of a MessageBox: MsgBoxPart pieces first (raw bytes, decoded once complete)
char s_PendingText[MAX_TEXT_BYTES];
char s_PendingTitle[MAX_TEXT_BYTES];

void AppendPending(char *pending, size_t size, const char *piece)
{
	const size_t len = strlen(pending);
	strncpy(pending + len, piece, size - len - 1);
	pending[size - 1] = '\0';
}

// A literal backslash-n (a language file's token, a "\\n" in the map) is a new line too; tabs are spaces
void ConvertNewLines(wchar_t *text)
{
	wchar_t *out = text;
	for (const wchar_t *in = text; *in; in++)
	{
		if (in[0] == L'\\' && in[1] == L'n')
		{
			*out++ = L'\n';
			in++;
		}
		else if (in[0] == L'\t')
			*out++ = L' ';
		else
			*out++ = *in;
	}
	*out = 0;
}

// titles.txt texts end with the line break before their closing "}"
void TrimTrailingSpace(wchar_t *text)
{
	size_t n = wcslen(text);
	while (n > 0 && (text[n - 1] == L'\n' || text[n - 1] == L' '))
		text[--n] = 0;
}

// "#token": titles.txt (as the HUD's text messages, CHudTextMessage::LookupString), else a VGUI2 language file,
// else the text as written
void ResolveText(const char *raw, wchar_t *out, int outChars)
{
	out[0] = 0;

	bool found = false;
	if (raw[0] == '#' && raw[1])
	{
		const char *title = sbo::LookupTitle(raw + 1);
		if (title)
		{
			sboui::TextToWide(title, out, outChars * (int)sizeof(wchar_t));
			found = true;
		}
		else
		{
			const wchar_t *localized = g_pVGuiLocalize->Find(raw);
			if (!localized)
				localized = g_pVGuiLocalize->Find(raw + 1);
			if (localized)
			{
				wcsncpy(out, localized, outChars - 1);
				out[outChars - 1] = 0;
				found = true;
			}
		}
	}

	if (!found)
		sboui::TextToWide(raw, out, outChars * (int)sizeof(wchar_t));

	ConvertNewLines(out);
	TrimTrailingSpace(out);
}

int MsgFunc_MsgBoxPart(const char *pszName, int iSize, void *pbuf)
{
	CMsgBoxReader msg = { static_cast<const unsigned char *>(pbuf), iSize, 0 };

	const int which = msg.ReadByte();
	char piece[256];
	msg.ReadString(piece, sizeof(piece));
	if (which == 1)
		AppendPending(s_PendingTitle, sizeof(s_PendingTitle), piece);
	else
		AppendPending(s_PendingText, sizeof(s_PendingText), piece);
	return 1;
}

// featureful CHudMessageBox::MsgFunc_MessageBox. byte 1 (open), short id, string title, string text (their ends;
// MsgBoxPart pieces before it carry the starts) / byte 0 (close), short id
int MsgFunc_MessageBox(const char *pszName, int iSize, void *pbuf)
{
	CMsgBoxReader msg = { static_cast<const unsigned char *>(pbuf), iSize, 0 };

	const int operation = msg.ReadByte();
	const int id = msg.ReadShort();

	if (operation != 1)
	{
		s_PendingText[0] = '\0';
		s_PendingTitle[0] = '\0';
		if (CSboMessageBox::Get())
			CSboMessageBox::Get()->Close(id);
		return 1;
	}

	char last[256];
	msg.ReadString(last, sizeof(last));
	AppendPending(s_PendingTitle, sizeof(s_PendingTitle), last);
	msg.ReadString(last, sizeof(last));
	AppendPending(s_PendingText, sizeof(s_PendingText), last);

	static char rawTitle[MAX_TEXT_BYTES];
	static char rawText[MAX_TEXT_BYTES];
	strncpy(rawTitle, s_PendingTitle, sizeof(rawTitle) - 1);
	rawTitle[sizeof(rawTitle) - 1] = '\0';
	strncpy(rawText, s_PendingText, sizeof(rawText) - 1);
	rawText[sizeof(rawText) - 1] = '\0';
	s_PendingTitle[0] = '\0';
	s_PendingText[0] = '\0';

	if (!rawText[0])
		return 1;

	static wchar_t title[MAX_TEXT_BYTES];
	static wchar_t text[MAX_TEXT_BYTES];
	ResolveText(rawTitle, title, MAX_TEXT_BYTES);
	ResolveText(rawText, text, MAX_TEXT_BYTES);

	if (CSboMessageBox::Get())
		CSboMessageBox::Get()->Open(id, title, text);
	return 1;
}
} // namespace

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_MessageBox_Init()
{
	if (CSboMessageBox::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	CSboMessageBox *pPanel = CSboMessageBox::Create(g_pViewport);
	pPanel->MakeReadyForUse();

	sbo::HookUserMsg("MessageBox", &MsgFunc_MessageBox);
	sbo::HookUserMsg("MsgBoxPart", &MsgFunc_MsgBoxPart);
	return true;
}

void VGui2_MessageBox_Reset()
{
	s_PendingText[0] = '\0';
	s_PendingTitle[0] = '\0';
	if (CSboMessageBox::Get())
		CSboMessageBox::Get()->Reset();
}

bool VGui2_MessageBox_Attack()
{
	// while the box is shown the press is always used up; it closes the box unless the box has just appeared
	CSboMessageBox *pBox = CSboMessageBox::Get();
	if (!pBox || !pBox->IsActive())
		return false;

	pBox->CloseNewest();
	return true;
}

bool VGui2_MessageBox_IsActive()
{
	return CSboMessageBox::Get() && CSboMessageBox::Get()->IsActive();
}

bool VGui2_MessageBox_HasBoxes()
{
	return CSboMessageBox::Get() && CSboMessageBox::Get()->HasBoxes();
}

bool VGui2_MessageBox_Key(int action)
{
	return CSboMessageBox::Get() && CSboMessageBox::Get()->HandleKey(action);
}
