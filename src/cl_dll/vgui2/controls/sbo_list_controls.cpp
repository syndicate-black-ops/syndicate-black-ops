//========= SBO VGUI2 controls: lists =========//
//
// Purpose: see sbo_list_controls.h.
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "../sbo_server_browser.h" // CSboBrowserButton
#include "../sbo_ui_style.h"
#include "sbo_list_controls.h"
#include "sbo_tool_window.h"

using namespace sbolook;

CSboOrderedList::CSboOrderedList(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true); // only gets keys while its tool window gives them (a click in the list)
	m_pNew = new CSboBrowserButton(this, "New", "New", "ListNew", CSboBrowserButton::KIND_NORMAL);
	m_pDelete = new CSboBrowserButton(this, "Delete", "Delete", "ListDelete", CSboBrowserButton::KIND_NORMAL);
	m_pUp = new CSboBrowserButton(this, "Up", "Up", "ListUp", CSboBrowserButton::KIND_NORMAL);
	m_pDown = new CSboBrowserButton(this, "Down", "Down", "ListDown", CSboBrowserButton::KIND_NORMAL);
	UpdateButtons();
}

CSboBrowserButton *CSboOrderedList::AddButton(const char *text, const char *command)
{
	CSboBrowserButton *b = new CSboBrowserButton(this, "Extra", text, command, CSboBrowserButton::KIND_NORMAL);
	m_Extra.push_back(b);
	InvalidateLayout();
	return b;
}

void CSboOrderedList::SetStandardButtons(bool show)
{
	m_bStandard = show;
	m_pNew->SetVisible(show);
	m_pDelete->SetVisible(show);
	m_pUp->SetVisible(show);
	m_pDown->SetVisible(show);
	InvalidateLayout();
}

int CSboOrderedList::AddRow(const char *text, int icon, bool on)
{
	InsertRow((int)m_Rows.size(), text, icon, on);
	return (int)m_Rows.size() - 1;
}

void CSboOrderedList::InsertRow(int index, const char *text, int icon, bool on)
{
	index = std::max(0, std::min(index, (int)m_Rows.size()));
	Row r;
	r.text = text ? text : "";
	r.icon = icon;
	r.on = on;
	m_Rows.insert(m_Rows.begin() + index, r);
	if (m_iSelected >= index)
		m_iSelected++;
	UpdateButtons();
	Repaint();
}

void CSboOrderedList::RemoveRow(int index)
{
	if (index < 0 || index >= (int)m_Rows.size())
		return;
	m_Rows.erase(m_Rows.begin() + index);
	if (m_iSelected == index)
		m_iSelected = std::min(index, (int)m_Rows.size() - 1);
	else if (m_iSelected > index)
		m_iSelected--;
	ClampFirst();
	UpdateButtons();
	Repaint();
}

void CSboOrderedList::ClearRows()
{
	m_Rows.clear();
	m_iSelected = -1;
	m_iFirst = 0;
	UpdateButtons();
	Repaint();
}

void CSboOrderedList::SetRowText(int index, const char *text)
{
	if (index >= 0 && index < (int)m_Rows.size())
	{
		m_Rows[index].text = text ? text : "";
		Repaint();
	}
}

void CSboOrderedList::SetRowOn(int index, bool on)
{
	if (index >= 0 && index < (int)m_Rows.size())
	{
		m_Rows[index].on = on;
		Repaint();
	}
}

void CSboOrderedList::SetSelected(int index)
{
	m_iSelected = (index >= 0 && index < (int)m_Rows.size()) ? index : -1;
	if (m_iSelected >= 0)
		ScrollTo(m_iSelected);
	UpdateButtons();
	Repaint();
}

void CSboOrderedList::SetGrid(int cellWide, int cellTall, int iconSize)
{
	m_iCellDesignWide = std::max(0, cellWide);
	m_iCellDesignTall = std::max(0, cellTall);
	m_iIconDesign = std::max(0, iconSize);
	InvalidateLayout();
	Repaint();
}

// a grid: as many lines as fit at the cell's design height, spread over the whole height
static int GridFit(int listTall, int cellTall)
{
	return std::max(1, (listTall - 2) / std::max(1, cellTall));
}

int CSboOrderedList::RowTall()
{
	if (!IsGrid())
		return S().M(m_iRowDesignTall);
	const int cell = S().M(std::max(8, m_iCellDesignTall));
	return std::max(cell, (ListTall() - 2) / GridFit(ListTall(), cell));
}

// a grid keeps the scroll bar's room on the right at all times, so the columns don't change when it comes and goes
static int GridBarWide()
{
	return std::max(6, S().M(9));
}

int CSboOrderedList::Columns()
{
	if (!IsGrid())
		return 1;
	return std::max(1, (GetWide() - 2 - GridBarWide() - 2) / std::max(1, S().M(m_iCellDesignWide)));
}

int CSboOrderedList::CellWide()
{
	if (!IsGrid())
		return GetWide();
	return std::max(1, (GetWide() - 2 - GridBarWide() - 2) / Columns());
}

int CSboOrderedList::Lines()
{
	const int cols = Columns();
	return ((int)m_Rows.size() + cols - 1) / cols;
}

int CSboOrderedList::ListTall()
{
	return GetTall() - S().M(24) - S().M(6);
}

int CSboOrderedList::RowsFit()
{
	if (IsGrid())
		return GridFit(ListTall(), S().M(std::max(8, m_iCellDesignTall)));
	return std::max(1, (ListTall() - 2) / RowTall());
}

int CSboOrderedList::RowAt(int y)
{
	if (y < 1 || y >= ListTall() - 1)
		return -1;
	const int row = m_iFirst + (y - 1) / RowTall();
	return row < (int)m_Rows.size() ? row : -1;
}

int CSboOrderedList::RowAtPoint(int x, int y)
{
	if (!IsGrid())
		return RowAt(y);
	if (y < 1 || y >= ListTall() - 1 || x < 1)
		return -1;
	const int col = (x - 1) / CellWide(), cols = Columns();
	if (col >= cols)
		return -1;
	const int row = (m_iFirst + (y - 1) / RowTall()) * cols + col;
	return row < (int)m_Rows.size() ? row : -1;
}

bool CSboOrderedList::OnIcon(int x, int y, int row)
{
	// a grid cell is one target: its picture selects it like its name
	if (!m_Painter || IsGrid() || row < 0)
		return false;
	const int pad = S().M(4), icon = RowTall() - S().M(4);
	return x >= pad && x < pad + icon;
}

void CSboOrderedList::ScrollTo(int index)
{
	const int fit = RowsFit(), line = index / Columns();
	if (line < m_iFirst)
		m_iFirst = line;
	else if (line >= m_iFirst + fit)
		m_iFirst = line - fit + 1;
	ClampFirst();
}

void CSboOrderedList::ClampFirst()
{
	m_iFirst = std::max(0, std::min(m_iFirst, std::max(0, Lines() - RowsFit())));
}

int CSboOrderedList::BarWide()
{
	return Lines() > RowsFit() ? std::max(6, S().M(9)) : 0;
}

void CSboOrderedList::ThumbSpan(int &y, int &h)
{
	const int n = Lines(), fit = RowsFit();
	const int trackH = ListTall() - 4;
	h = std::max(S().M(16), trackH * fit / std::max(1, n));
	y = 2 + (trackH - h) * m_iFirst / std::max(1, n - fit);
}

void CSboOrderedList::Select(int index, bool post)
{
	if (m_Rows.empty())
		return;
	index = std::max(0, std::min(index, (int)m_Rows.size() - 1));
	const bool changed = index != m_iSelected;
	m_iSelected = index;
	ScrollTo(index);
	UpdateButtons();
	Repaint();
	if (changed && post)
		Post("ListSelected", "index", index);
}

void CSboOrderedList::UpdateButtons()
{
	const bool sel = m_iSelected >= 0 && m_iSelected < (int)m_Rows.size();
	m_pDelete->SetEnabled(sel);
	m_pUp->SetEnabled(sel && m_iSelected > 0);
	m_pDown->SetEnabled(sel && m_iSelected < (int)m_Rows.size() - 1);
}

void CSboOrderedList::Post(const char *name, const char *key, int value, const char *key2, int value2)
{
	KeyValues *kv = new KeyValues(name);
	if (key)
		kv->SetInt(key, value);
	if (key2)
		kv->SetInt(key2, value2);
	PostActionSignal(kv);
}

void CSboOrderedList::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "ListNew"))
		Post("ListNew");
	else if (!Q_stricmp(command, "ListDelete"))
	{
		if (m_iSelected >= 0)
			Post("ListDelete", "index", m_iSelected);
	}
	else if (!Q_stricmp(command, "ListUp") || !Q_stricmp(command, "ListDown"))
	{
		const int from = m_iSelected;
		const int to = from + (!Q_stricmp(command, "ListUp") ? -1 : 1);
		if (from < 0 || to < 0 || to >= (int)m_Rows.size())
			return;
		std::swap(m_Rows[from], m_Rows[to]);
		m_iSelected = to;
		ScrollTo(to);
		UpdateButtons();
		Repaint();
		Post("ListMoved", "from", from, "to", to);
	}
	else
	{
		// a tool's own button
		PostActionSignal(new KeyValues("Command", "command", command));
	}
}

void CSboOrderedList::PerformLayout()
{
	BaseClass::PerformLayout();
	const int w = GetWide(), bh = S().M(24), by = GetTall() - bh, gap = S().M(4);
	std::vector<CSboBrowserButton *> buttons;
	if (m_bStandard)
		buttons = { m_pNew, m_pDelete, m_pUp, m_pDown };
	for (CSboBrowserButton *b : m_Extra)
		buttons.push_back(b);
	const int n = (int)buttons.size();
	if (n > 0)
	{
		const int bw = (w - gap * (n - 1)) / n;
		for (int i = 0; i < n; i++)
			buttons[i]->SetBounds(i * (bw + gap), by, i == n - 1 ? w - i * (bw + gap) : bw, bh);
	}
	if (m_iSelected >= 0)
		ScrollTo(m_iSelected);
	else
		ClampFirst();
}

void CSboOrderedList::PaintBackground()
{
	FillRect(0, 0, GetWide(), ListTall(), COL_BG_INPUT);
	OutlineRect(0, 0, GetWide(), ListTall(), COL_BORDER);
}

void CSboOrderedList::Paint()
{
	const int rowTall = RowTall(), fit = RowsFit(), n = (int)m_Rows.size();
	const bool scroll = Lines() > fit;
	const int barW = BarWide();
	const int pad = S().M(4), icon = rowTall - S().M(4);
	if (IsGrid())
	{
		// cells: the picture, and the name under it
		const int cols = Columns(), cw = CellWide(), textH = FontTall(S().small) + S().M(2);
		const int pic = std::max(4, std::min({ S().M(m_iIconDesign), cw - 2 * pad, rowTall - textH - 2 * pad }));
		for (int line = 0; line < fit; line++)
			for (int col = 0; col < cols; col++)
			{
				const int row = (m_iFirst + line) * cols + col;
				if (row >= n)
					break;
				const Row &r = m_Rows[row];
				const bool sel = row == m_iSelected;
				const int cx = 1 + col * cw, cy = 1 + line * rowTall;
				if (sel)
					FillRect(cx, cy, cw, rowTall, COL_BG_PRIMARY);
				// the picture and the name, in the middle of the cell
				const int top = cy + std::max(0, (rowTall - (pic + S().M(2) + textH)) / 2) - pad;
				if (m_Painter)
					m_Painter(r.icon, r.on, cx + (cw - pic) / 2, top + pad, pic);
				const size_t slash = r.text.rfind('/');
				const std::wstring name = Widen((slash == std::string::npos ? r.text : r.text.substr(slash + 1)).c_str());
				const int maxW = cw - 2 * pad, tw = std::min(TextWide(S().small, name), maxW);
				const Color c = !r.on ? COL_TEXT_FAINT : (sel ? COL_TEXT_PRIMARY : COL_TEXT);
				DrawText(S().small, c, cx + (cw - tw) / 2, top + pad + pic + S().M(2), name, maxW);
			}
	}
	for (int i = 0; !IsGrid() && i < fit && m_iFirst + i < n; i++)
	{
		const int row = m_iFirst + i, y = 1 + i * rowTall;
		const Row &r = m_Rows[row];
		const bool sel = row == m_iSelected;
		if (sel)
			FillRect(1, y, GetWide() - 2 - barW, rowTall, COL_BG_PRIMARY);
		int x = pad;
		if (m_Painter)
		{
			m_Painter(r.icon, r.on, x, y + (rowTall - icon) / 2, icon);
			x += icon + pad;
		}
		const Color c = !r.on ? COL_TEXT_FAINT : (sel ? COL_TEXT_PRIMARY : COL_TEXT);
		DrawTextMid(S().body, c, x, y, rowTall, Widen(r.text.c_str()), GetWide() - x - pad - barW);
	}
	if (scroll)
	{
		int thumbY, thumbH;
		ThumbSpan(thumbY, thumbH);
		FillRect(GetWide() - 2 - barW, 2, barW, ListTall() - 4, COL_BG_WINDOW);
		FillRect(GetWide() - 2 - barW, thumbY, barW, thumbH, m_bThumbDrag ? COL_TEXT_BRIGHT : COL_BORDER_PRIMARY);
	}
}

void CSboOrderedList::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	// the list takes the keyboard (arrows, pages, Enter) until Esc or a click elsewhere
	for (vgui2::Panel *p = GetParent(); p; p = p->GetParent())
		if (CSboToolWindow *w = dynamic_cast<CSboToolWindow *>(p))
		{
			w->BeginTextInput(this);
			break;
		}
	// the scroll bar: drag the thumb, or a page above / below it
	const int barW = BarWide();
	if (barW && x >= GetWide() - 2 - barW && y < ListTall())
	{
		int thumbY, thumbH;
		ThumbSpan(thumbY, thumbH);
		if (y >= thumbY && y < thumbY + thumbH)
		{
			m_bThumbDrag = true;
			m_iThumbGrab = y - thumbY;
			vgui2::input()->SetMouseCapture(GetVPanel());
		}
		else
		{
			m_iFirst += (y < thumbY ? -1 : 1) * std::max(1, RowsFit() - 1);
			ClampFirst();
		}
		Repaint();
		return;
	}
	const int row = RowAtPoint(x, y);
	if (row < 0)
		return;
	if (OnIcon(x, y, row))
	{
		Post("ListIconClicked", "index", row);
		return;
	}
	if (row != m_iSelected)
	{
		m_iSelected = row;
		UpdateButtons();
		Repaint();
		Post("ListSelected", "index", row);
	}
}

void CSboOrderedList::OnMouseDoublePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	const int row = RowAtPoint(x, y);
	if (row < 0 || OnIcon(x, y, row))
	{
		OnMousePressed(code); // a double click on an icon is two clicks on it
		return;
	}
	Post("ListActivated", "index", row);
}

void CSboOrderedList::OnMouseWheeled(int delta)
{
	m_iFirst -= delta * (IsGrid() ? 1 : 3); // a line of cells is tall
	ClampFirst();
	Repaint();
}

void CSboOrderedList::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_bThumbDrag)
	{
		m_bThumbDrag = false;
		vgui2::input()->SetMouseCapture(0);
		Repaint();
	}
}

void CSboOrderedList::OnCursorMoved(int x, int y)
{
	if (!m_bThumbDrag)
		return;
	const int n = Lines(), fit = RowsFit();
	int thumbY, thumbH;
	ThumbSpan(thumbY, thumbH);
	const int trackH = ListTall() - 4, travel = std::max(1, trackH - thumbH);
	const int top = y - m_iThumbGrab - 2;
	m_iFirst = (int)((long long)top * std::max(0, n - fit) / travel + 0); // the thumb's top follows the cursor
	ClampFirst();
	Repaint();
}

void CSboOrderedList::OnKeyCodeTyped(vgui2::KeyCode code)
{
	const int cols = Columns(), page = std::max(1, RowsFit() - 1) * cols, sel = m_iSelected;
	switch (code)
	{
	case vgui2::KEY_UP: Select(sel < 0 ? 0 : (sel - cols >= 0 ? sel - cols : sel), true); break;
	case vgui2::KEY_DOWN: Select(sel < 0 ? 0 : (sel + cols < (int)m_Rows.size() ? sel + cols : sel), true); break;
	case vgui2::KEY_LEFT:
		if (!IsGrid())
			BaseClass::OnKeyCodeTyped(code);
		else
			Select(sel < 0 ? 0 : sel - 1, true);
		break;
	case vgui2::KEY_RIGHT:
		if (!IsGrid())
			BaseClass::OnKeyCodeTyped(code);
		else
			Select(sel < 0 ? 0 : sel + 1, true);
		break;
	case vgui2::KEY_PAGEUP: Select(sel < 0 ? 0 : sel - page, true); break;
	case vgui2::KEY_PAGEDOWN: Select(sel < 0 ? 0 : sel + page, true); break;
	case vgui2::KEY_HOME: Select(0, true); break;
	case vgui2::KEY_END: Select((int)m_Rows.size() - 1, true); break;
	case vgui2::KEY_ENTER:
	case vgui2::KEY_PAD_ENTER:
		if (sel >= 0)
			Post("ListActivated", "index", sel);
		break;
	case vgui2::KEY_ESCAPE:
		for (vgui2::Panel *p = GetParent(); p; p = p->GetParent())
			if (CSboToolWindow *w = dynamic_cast<CSboToolWindow *>(p))
			{
				w->EndTextInput();
				break;
			}
		break;
	default: BaseClass::OnKeyCodeTyped(code); break;
	}
}
