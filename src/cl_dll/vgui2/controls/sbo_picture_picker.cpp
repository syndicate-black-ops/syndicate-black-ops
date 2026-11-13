//========= SBO VGUI2 controls: a picture picker =========//
//
// Purpose: see sbo_picture_picker.h.
//=================================================================//

#include <stdio.h>
#include <algorithm>

#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "../client_viewport.h"
#include "../sbo_server_browser.h" // CSboBrowserButton
#include "../sbo_ui_style.h"
#include "sbo_list_controls.h"
#include "sbo_picture_picker.h"
#include "sbo_value_controls.h" // CSboTextField

using namespace sbolook;

CSboPicturePicker::CSboPicturePicker(vgui2::Panel *pParent, const char *pName, const char *title, const char *key,
    const char *noun)
    : BaseClass(pParent, pName, title, key, 340, 460, 0.1f, 0.15f), m_Noun(noun ? noun : "files")
{
	m_pFilter = new CSboTextField(this, "Filter", "Search");
	m_pFilter->SetMaximumCharCount(40);
	m_pFilter->AddActionSignalTarget(this);
	m_pList = new CSboOrderedList(this, "Pictures");
	m_pList->SetStandardButtons(false);
	// a grid that fills the window: a picture with the file name under it in each cell
	m_pList->SetGrid(100, 92, 64);
	m_pList->SetIconPainter([this](int index, bool, int x, int y, int size) { PaintThumb(index, x, y, size); });
	m_pUse = m_pList->AddButton("Use", "UsePicture");
	m_pList->AddActionSignalTarget(this);
}

void CSboPicturePicker::SetTips(const char *search, const char *list, const char *use)
{
	SetToolTip(m_pFilter, search);
	// a picture's tip is its whole path
	const std::string listTip = list ? list : "";
	SetToolTipFunc(m_pList, [this, listTip]() {
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		m_pList->ScreenToLocal(x, y);
		const int i = m_pList->RowAtPoint(x, y);
		if (i >= 0 && i < (int)m_Shown.size())
			return m_Shown[i];
		return listTip;
	});
	SetToolTip(m_pUse, use);
}

void CSboPicturePicker::SetFiles(const std::vector<std::string> &files, const std::vector<bool> &filtered)
{
	FreeThumbs();
	m_All = files;
	m_Filtered = filtered;
	m_Filtered.resize(m_All.size(), false);
	Fill();
}

void CSboPicturePicker::SetFilteredToggle(const char *label, const char *tip)
{
	if (!m_pShowFiltered)
	{
		m_pShowFiltered = new CSboBrowserCheck(this, "ShowFiltered", label);
		m_pShowFiltered->AddActionSignalTarget(this);
	}
	SetToolTip(m_pShowFiltered, tip);
	InvalidateLayout();
}

void CSboPicturePicker::OnButtonToggled(vgui2::Panel *)
{
	Fill();
}

void CSboPicturePicker::OpenFor(const std::string &status, PickFunc pick)
{
	m_Status = status;
	m_Pick = pick;
	Fill();
	Open();
	if (IsCollapsed())
		SetCollapsed(false);
}

void CSboPicturePicker::OnListActivated(int index)
{
	Use(index);
}

void CSboPicturePicker::OnListIconClicked(int index)
{
	m_pList->SetSelected(index);
}

void CSboPicturePicker::OnTextChanged(vgui2::Panel *)
{
	// the list follows the typing
	char text[64];
	m_pFilter->GetText(text, sizeof(text));
	const std::string f = Lower(text);
	if (f != m_Filter)
	{
		m_Filter = f;
		Fill();
	}
}

void CSboPicturePicker::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "UsePicture"))
	{
		Use(m_pList->GetSelected());
		return;
	}
	BaseClass::OnCommand(command);
}

void CSboPicturePicker::LayoutContent(int x, int y, int w, int h)
{
	m_pFilter->SetBounds(x, y, w, M(22));
	int top = M(26);
	if (m_pShowFiltered)
	{
		m_pShowFiltered->SetBounds(x, y + top, m_pShowFiltered->GetContentWide(), M(22));
		top += M(26);
	}
	m_iTextY = y + h - M(34);
	m_pList->SetBounds(x, y + top, w, std::max(M(60), h - top - M(40)));
}

void CSboPicturePicker::Paint()
{
	BaseClass::Paint();
	if (IsCollapsed())
		return;
	DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY, Widen(m_Status.c_str()), GetWide() - M(20));
	char count[128];
	if (m_iFilteredOut > 0)
		snprintf(count, sizeof(count), "%d of %d %s (%d filtered)", (int)m_Shown.size(), (int)m_All.size(),
		    m_Noun.c_str(), m_iFilteredOut);
	else
		snprintf(count, sizeof(count), "%d of %d %s", (int)m_Shown.size(), (int)m_All.size(), m_Noun.c_str());
	DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY + M(16), Widen(count), GetWide() - M(20));
}

void CSboPicturePicker::OnToolClosed()
{
	FreeThumbs(); // the pictures go with the window
}

void CSboPicturePicker::FreeThumbs()
{
	for (auto &it : m_Thumbs)
		if (it.second.texture >= 0)
			vgui2::surface()->DeleteTextureByID(it.second.texture);
	m_Thumbs.clear();
}

void CSboPicturePicker::PaintThumb(int index, int x, int y, int size)
{
	if (index < 0 || index >= (int)m_Shown.size())
		return;
	FillRect(x, y, size, size, m_CellColor);
	const std::string &name = m_Shown[index];
	auto it = m_Thumbs.find(name);
	if (it == m_Thumbs.end())
	{
		Thumb t;
		std::vector<uint8_t> rgba;
		int w = 0, h = 0;
		if (m_Maker && m_Maker(name, size, rgba, w, h) && w > 0 && h > 0 && rgba.size() == (size_t)w * h * 4)
		{
			t.texture = vgui2::surface()->CreateNewTextureID(true);
			vgui2::surface()->DrawSetTextureRGBA(t.texture, rgba.data(), w, h, true, false);
			t.wide = w;
			t.tall = h;
		}
		it = m_Thumbs.emplace(name, t).first;
	}
	const Thumb &t = it->second;
	if (t.texture < 0)
	{
		OutlineRect(x, y, size, size, COL_BORDER_LIST);
		return;
	}
	const int dx = x + (size - t.wide) / 2, dy = y + (size - t.tall) / 2;
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawSetTexture(t.texture);
	vgui2::surface()->DrawTexturedRect(dx, dy, dx + t.wide, dy + t.tall);
}

void CSboPicturePicker::Fill()
{
	m_pList->ClearRows();
	m_Shown.clear();
	m_iFilteredOut = 0;
	const bool all = !m_pShowFiltered || m_pShowFiltered->IsChecked();
	for (size_t i = 0; i < m_All.size(); i++)
	{
		const std::string &f = m_All[i];
		if (!all && m_Filtered[i])
		{
			m_iFilteredOut++;
			continue;
		}
		if (m_Filter.empty() || Lower(f).find(m_Filter) != std::string::npos)
		{
			m_Shown.push_back(f);
			m_pList->AddRow(f.c_str(), (int)m_Shown.size() - 1, true);
		}
	}
	Repaint();
}

void CSboPicturePicker::Use(int index)
{
	if (index < 0 || index >= (int)m_Shown.size() || !m_Pick)
		return;
	const std::string why = m_Pick(m_Shown[index]);
	if (!why.empty())
	{
		m_Status = why;
		Repaint();
		return;
	}
	CloseTool(); // chosen: the list goes
}
