//========= SBO VGUI2: the color correction editor =========//
//
// Purpose: see sbo_cc_editor.h. The operations and the table are colorcorrection_ops.{h,cpp} (no VGUI2); the view
//          shows the table through ColorCorr_SetEditorLook (colorcorrection.cpp), mixed with the map's entities as
//          Source mixes its lookups (by weight).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

#include "../colorcorrection.h"
#include "../colorcorrection_ops.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "controls/sbo_graph_controls.h"
#include "controls/sbo_list_controls.h"
#include "controls/sbo_tool_window.h"
#include "controls/sbo_value_controls.h"
#include "sbo_bridge.h"
#include "sbo_cc_editor.h"
#include "sbo_server_browser.h" // CSboBrowserButton, CSboBrowserCheck, CSboBrowserSelect
#include "sbo_ui_style.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;
namespace ops = colorcorr::ops;

namespace
{
constexpr int LUT_SIZE = colorcorr::RAW_SIZE; // Source's .raw: 32 x 32 x 32
constexpr int PREVIEW_WIDE = 200;             // the Selected HSV picture
constexpr int LOOKUP_VIEW_COLUMNS = 4;        // Source's lookup view: 4 slices across, 8 down
constexpr unsigned MAX_TABLE_BYTES = 4u << 20;

class CCcMainWindow;
class CCcOpWindow;
class CCcLookupWindow;
class CCcSaveWindow;
class CCcNewWindow;
class CCcLoadWindow;

struct OpEntry
{
	int id;
	ops::Operation op;
};

std::vector<OpEntry> s_Ops;
int s_iNextId = 1;
bool s_bTableDirty = true; // the operations changed: the table is built again
bool s_bLookDirty = true;  // the table or Enable / Enable Entities / Blend changed: the view gets it again
bool s_bEnable = true;
bool s_bEntities = true;
int s_iBlend = 255;
colorcorr::Lut s_Lut;
int s_iLutSerial = 0; // bumped with every new table (the lookup view follows)
bool s_bLookSet = false;

CCcMainWindow *s_pMain = nullptr;
CCcLookupWindow *s_pLookup = nullptr;
CCcSaveWindow *s_pSave = nullptr;
CCcNewWindow *s_pNew = nullptr;
CCcLoadWindow *s_pLoad = nullptr;
std::string s_LastName; // the look loaded or saved last (the Save window starts with it)
std::map<int, CCcOpWindow *> s_OpWindows;
CSboScreenPicker *s_pPicker = nullptr;

OpEntry *FindOp(int id)
{
	for (OpEntry &e : s_Ops)
		if (e.id == id)
			return &e;
	return nullptr;
}

int IndexOf(int id)
{
	for (int i = 0; i < (int)s_Ops.size(); i++)
		if (s_Ops[i].id == id)
			return i;
	return -1;
}

bool s_bModified = false;           // the operations or Blend changed since the last load or save
std::function<void()> s_AfterSave;  // what waits for a save (the unsaved-changes question's Save)
void UpdateTitle();
void OpenSave(std::function<void()> after);

void MarkTableDirty()
{
	s_bTableDirty = true;
	s_bLookDirty = true;
	s_bModified = true;
	UpdateTitle();
}

//! Unsaved changes: asks "Save changes to <name>?" (Save, Don't save, Cancel), then goes on unless cancelled
void ConfirmDiscard(std::function<void()> next)
{
	if (!s_bModified)
	{
		next();
		return;
	}
	const std::string q = "Save changes to " + (s_LastName.empty() ? std::string("this look") : s_LastName + ".raw") + "?";
	CSboChoiceDialog::Get()->Ask("Unsaved changes", q.c_str(), { "Save", "Don't save", "Cancel" }, [next](int choice) {
		if (choice == 0)
			OpenSave(next);
		else if (choice == 1)
			next();
	});
}

//! The editor may run: the tool windows' rule (SboToolsAllowed)
bool Allowed()
{
	return SboToolsAllowed();
}

bool AnyOperationOn()
{
	for (const OpEntry &e : s_Ops)
		if (e.op.enabled && e.op.blend > 0)
			return true;
	return false;
}

//! The table and the view's editor look, when something changed
void UpdateLook()
{
	if (s_bTableDirty)
	{
		std::vector<ops::Operation> list;
		list.reserve(s_Ops.size());
		for (const OpEntry &e : s_Ops)
			list.push_back(e.op);
		ops::BuildLut(list, LUT_SIZE, s_Lut);
		s_bTableDirty = false;
		s_iLutSerial++;
	}
	if (!s_bLookDirty)
		return;
	s_bLookDirty = false;
	// no operation on: the editor's table adds nothing (an unchanged picture would only weaken the map's entities)
	const float weight = (s_bEnable && AnyOperationOn()) ? s_iBlend / 255.0f : 0.0f;
	ColorCorr_SetEditorLook(s_Lut.rgb.data(), s_Lut.size, weight, s_bEntities);
	s_bLookSet = true;
}

void ClearLook()
{
	if (s_bLookSet)
		ColorCorr_SetEditorLook(nullptr, 0, 0.0f, true);
	s_bLookSet = false;
	s_bLookDirty = true;
}

//! A table file in gfx/colorcorrection/ (path from the mod's search paths); false with a reason in err
bool LoadTable(const char *path, colorcorr::Lut &out, char *err, int errLen)
{
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
	if (!fh)
	{
		snprintf(err, errLen, "can't open it");
		return false;
	}
	const unsigned size = g_pFullFileSystem->Size(fh);
	std::vector<unsigned char> data;
	if (size > 0 && size <= MAX_TABLE_BYTES)
	{
		data.resize(size);
		if (g_pFullFileSystem->Read(data.data(), (int)size, fh) != (int)size)
			data.clear();
	}
	g_pFullFileSystem->Close(fh);
	if (data.empty())
	{
		snprintf(err, errLen, size > MAX_TABLE_BYTES ? "too big" : "can't read it");
		return false;
	}
	return colorcorr::ReadTable(path, data.data(), (int)data.size(), out, err, errLen);
}

//! The table files players can use: gfx/colorcorrection/*.raw and *.cube (names only), sorted, once each
std::vector<std::string> ListTables()
{
	std::vector<std::string> files;
	for (const char *pathID : { "GAME", "GAMEDOWNLOAD" })
		for (const char *pattern : { "gfx/colorcorrection/*.raw", "gfx/colorcorrection/*.cube" })
		{
			FileFindHandle_t fh;
			const char *fn = g_pFullFileSystem->FindFirst(pattern, &fh, pathID);
			while (fn && files.size() < 512)
			{
				std::string s = fn;
				bool dup = false;
				for (const std::string &f : files)
					dup = dup || !Q_stricmp(f.c_str(), s.c_str());
				if (!dup && !s.empty() && !g_pFullFileSystem->FindIsDirectory(fh))
					files.push_back(s);
				fn = g_pFullFileSystem->FindNext(fh);
			}
			g_pFullFileSystem->FindClose(fh);
		}
	std::sort(files.begin(), files.end(),
	    [](const std::string &a, const std::string &b) { return Q_stricmp(a.c_str(), b.c_str()) < 0; });
	return files;
}

//! An operation's icon in the list: a small picture of what it does, faint when it is off
void PaintOpIcon(int type, bool on, int x, int y, int size)
{
	const int a = on ? 255 : 70;
	const int c = std::max(1, size / 6);
	FillRect(x, y, size, size, Color(11, 14, 11, 255));
	OutlineRect(x, y, size, size, on ? COL_BORDER_PRIMARY : COL_BORDER);
	const int in = std::max(2, size / 5), w = size - 2 * in;
	switch (type)
	{
	case ops::OP_CURVES:
		// an S curve
		vgui2::surface()->DrawSetColor(Color(198, 242, 176, a));
		for (int i = 0; i < w; i++)
		{
			const float t = (float)i / std::max(1, w - 1);
			const float yv = t + 0.25f * sinf(t * 6.2832f);
			const int py = y + in + (int)((1.0f - std::max(0.0f, std::min(1.0f, yv))) * (w - 1));
			vgui2::surface()->DrawFilledRect(x + in + i, py, x + in + i + 1, py + 2);
		}
		break;
	case ops::OP_LEVELS:
		// three bars, dark to light
		for (int i = 0; i < 3; i++)
		{
			const int bh = w * (i + 1) / 3, g = 90 + 70 * i;
			FillRect(x + in + i * w / 3, y + in + w - bh, std::max(1, w / 3 - 1), bh, Color(g, g, g, a));
		}
		break;
	case ops::OP_SELECTED_HSV:
		// three hues and a selection frame
		FillRect(x + in, y + in, w / 3, w, Color(220, 60, 60, a));
		FillRect(x + in + w / 3, y + in, w / 3, w, Color(70, 200, 70, a));
		FillRect(x + in + 2 * w / 3, y + in, w - 2 * w / 3, w, Color(70, 110, 230, a));
		OutlineRect(x + in + w / 3 - 1, y + in - 1, w / 3 + 2, w + 2, Color(255, 255, 255, a));
		break;
	case ops::OP_BALANCE:
		// cyan against red
		FillRect(x + in, y + in, w / 2, w, Color(60, 200, 210, a));
		FillRect(x + in + w / 2, y + in, w - w / 2, w, Color(220, 70, 60, a));
		break;
	case ops::OP_LOOKUP:
		// a 2 x 2 table
		FillRect(x + in, y + in, w / 2, w / 2, Color(40, 40, 40, a));
		FillRect(x + in + w / 2, y + in, w - w / 2, w / 2, Color(220, 70, 60, a));
		FillRect(x + in, y + in + w / 2, w / 2, w - w / 2, Color(70, 200, 70, a));
		FillRect(x + in + w / 2, y + in + w / 2, w - w / 2, w - w / 2, Color(230, 220, 120, a));
		break;
	}
	(void)c;
}

SboValueRange Range(float min, float max, float step = 1.0f, int decimals = 0)
{
	SboValueRange r;
	r.min = min;
	r.max = max;
	r.step = step;
	r.decimals = decimals;
	return r;
}

//-----------------------------------------------------------------------------
// A window of one operation
//-----------------------------------------------------------------------------
class CCcOpWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcOpWindow, CSboToolWindow);

public:
	CCcOpWindow(int id, int type);
	int Id() const { return m_iId; }
	//! Shows the operation's values (opened, or the list renamed it)
	void Refresh();
	void OnTick() override;

	MESSAGE_FUNC_PARAMS(OnValueChanged, "ValueChanged", data);
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel);
	MESSAGE_FUNC_PTR(OnSelectChanged, "SelectChanged", panel);
	MESSAGE_FUNC(OnCurveChanged, "CurveChanged");
	MESSAGE_FUNC_PARAMS(OnColorChanged, "ColorChanged", data);
	MESSAGE_FUNC_PARAMS(OnTextEntered, "TextEntered", data);
	MESSAGE_FUNC_INT_INT(OnScreenPicked, "ScreenPicked", x, y);
	MESSAGE_FUNC(OnScreenPickCancelled, "ScreenPickCancelled");
	MESSAGE_FUNC_INT_INT(OnImageClicked, "ImageClicked", x, y);
	void OnCommand(const char *command) override;
	void Paint() override;

protected:
	void LayoutContent(int x, int y, int w, int h) override;
	void OnToolClosed() override;

private:
	ops::Operation *Op();
	void Changed(); //!< the operation changed: the table, the list and the preview follow
	void ShowChannel();
	void ShowRange();
	void TakeSnapshot(bool pick);
	void UpdatePreview();
	void SetTarget(int r, int g, int b);
	CSboLabeledSlider *Slider(const char *label, const SboValueRange &range, float def);
	CSboBrowserCheck *Check(const char *text);

	int m_iId, m_iType;
	CSboTextField *m_pName = nullptr;
	CSboLabeledSlider *m_pBlend = nullptr;
	// Curves / Levels
	CSboBrowserSelect *m_pChannel = nullptr;
	CSboCurveEditor *m_pCurve = nullptr;
	CSboBrowserButton *m_pResetCurve = nullptr;
	CSboLabeledSlider *m_pLevels[5] = {}; //!< input black, input white, gamma, output black, output white
	// Selected HSV
	CSboBrowserSelect *m_pMode = nullptr;
	CSboBrowserButton *m_pSelect = nullptr;
	CSboRgbaImage *m_pPreview = nullptr;
	CSboColorSwatch *m_pTarget = nullptr;
	CSboBrowserCheck *m_pColorize = nullptr, *m_pInvert = nullptr;
	CSboLabeledSlider *m_pHsv[5] = {}; //!< hue, saturation, value, tolerance, fuzziness
	// Color Balance
	CSboLabeledSlider *m_pBalance[3] = {};
	CSboBrowserCheck *m_pRanges[3] = {};
	CSboBrowserCheck *m_pPreserve = nullptr;
	int m_iRange = ops::RANGE_MIDTONES;
	// Lookup
	CSboBrowserSelect *m_pFile = nullptr;
	CSboBrowserButton *m_pReload = nullptr;
	std::string m_LookupStatus;
	std::vector<std::string> m_Files;

	// the copy of the view (Selected HSV)
	std::vector<unsigned char> m_Shot; //!< PREVIEW_WIDE wide, RGB, top row first
	int m_iShotW = 0, m_iShotH = 0;
	int m_iShotSerial = 0;
	bool m_bShotWanted = false;
	bool m_bPickWanted = false;
	int m_iPickX = 0, m_iPickY = 0; //!< screen pixels
	bool m_bPreviewDirty = false;
	int m_iStatusY = 0;
};

//-----------------------------------------------------------------------------
// The lookup view
//-----------------------------------------------------------------------------
class CCcLookupWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcLookupWindow, CSboToolWindow);

public:
	CCcLookupWindow()
	    : BaseClass(g_pViewport, "CcLookupView", "Lookup View", "cc_lookup", 150, 300, 0.02f, 0.05f)
	{
		m_pImage = new CSboRgbaImage(this, "Table");
		m_pImage->SetFiltered(false);
		SetToolTip(m_pImage, "The lookup table your look makes: in each square red grows to the right and green downward, and blue grows from one square to the next");
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 100);
	}
	void OnTick() override
	{
		if (!IsOpen() || m_iSerial == s_iLutSerial)
			return;
		m_iSerial = s_iLutSerial;
		std::vector<unsigned char> rgba;
		int w = 0, h = 0;
		ops::LutImage(s_Lut, LOOKUP_VIEW_COLUMNS, rgba, w, h);
		m_pImage->SetImage(rgba.empty() ? nullptr : rgba.data(), w, h);
	}
	void Open() override
	{
		m_iSerial = -1;
		BaseClass::Open();
		OnTick();
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override { m_pImage->SetBounds(x, y, w, h); }
	void OnToolClosed() override;

private:
	CSboRgbaImage *m_pImage;
	int m_iSerial = -1;
};

//-----------------------------------------------------------------------------
// Save: the look's name, then gfx/colorcorrection/<name>.raw in the mod folder
//-----------------------------------------------------------------------------
class CCcSaveWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcSaveWindow, CSboToolWindow);

public:
	CCcSaveWindow()
	    : BaseClass(g_pViewport, "CcSave", "Save Look", "cc_save", 320, 166, 0.35f, 0.35f)
	{
		m_pName = new CSboTextField(this, "Name", "Name of the look");
		m_pName->SetMaximumCharCount(39);
		m_pName->AddActionSignalTarget(this);
		m_pSave = new CSboBrowserButton(this, "Save", "Save", "DoSave", CSboBrowserButton::KIND_PRIMARY);
		m_pCancel = new CSboBrowserButton(this, "Cancel", "Cancel", "Close", CSboBrowserButton::KIND_NORMAL);
		SetToolTip(m_pName, "The look's name, using letters, digits, _ and -");
		SetToolTip(m_pSave, "Write the look to gfx/colorcorrection/<name>.raw in the SBO folder, with its list of operations");
		SetToolTip(m_pCancel, "Close without saving");
	}
	void Open() override
	{
		m_Status = "Letters, digits, _ and -";
		m_Hint.clear();
		m_Confirm.clear();
		m_pName->SetValue(s_LastName.c_str());
		BaseClass::Open();
		m_pName->StartEditing();
	}
	MESSAGE_FUNC_PARAMS(OnTextEntered, "TextEntered", data)
	{
		if (data->GetInt("enter"))
			Save(); // Enter; a click on Save saves through its command
	}
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "DoSave"))
		{
			char text[64];
			m_pName->GetText(text, sizeof(text));
			m_pName->SetValue(text);
			Save();
			return;
		}
		BaseClass::OnCommand(command);
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		m_pName->SetBounds(x, y, w, M(22));
		m_iTextY = y + M(28);
		const int bw = M(72), bh = M(24);
		m_pSave->SetBounds(x + w - bw, y + h - bh, bw, bh);
		m_pCancel->SetBounds(x + w - 2 * bw - M(6), y + h - bh, bw, bh);
	}
	void Paint() override
	{
		BaseClass::Paint();
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY, Widen(m_Status.c_str()), GetWide() - M(20));
		if (!m_Hint.empty())
		{
			DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY + M(16), Widen(m_Hint.c_str()), GetWide() - M(20));
			DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY + M(32), L"A map needs only the .raw", GetWide() - M(20));
		}
	}

	//! closed without a save: what waited for it does not happen
	void OnToolClosed() override
	{
		if (!m_bSaved)
			s_AfterSave = nullptr;
		m_bSaved = false;
	}

private:
	void Save();
	bool m_bSaved = false;
	CSboTextField *m_pName;
	CSboBrowserButton *m_pSave, *m_pCancel;
	std::string m_Status, m_Hint, m_Confirm;
	int m_iTextY = 0;
};

//-----------------------------------------------------------------------------
// New: the operation types
//-----------------------------------------------------------------------------
class CCcNewWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcNewWindow, CSboToolWindow);

public:
	CCcNewWindow()
	    : BaseClass(g_pViewport, "CcNew", "New Operation", "cc_new", 170, 196, 0.6f, 0.3f)
	{
		for (int t = 0; t < ops::OP_COUNT; t++)
		{
			char cmd[16];
			snprintf(cmd, sizeof(cmd), "new %d", t);
			m_pTypes[t] = new CSboBrowserButton(this, "Type", ops::TypeName(t), cmd, CSboBrowserButton::KIND_NORMAL);
			static const char *const tips[ops::OP_COUNT] = {
				"Bend the brightness of all three colors, or of red, green or blue, with a curve",
				"Set the black and white points and the mid tones, like the Levels tool of an image editor",
				"Change the hue, saturation and brightness of the colors near one color",
				"Move the colors toward cyan or red, magenta or green, and yellow or blue",
				"Use a look or table file from gfx/colorcorrection/ as a step",
			};
			SetToolTip(m_pTypes[t], tips[t]);
		}
	}
	void OnCommand(const char *command) override;

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		const int bh = M(24), gap = M(4);
		for (int t = 0; t < ops::OP_COUNT; t++)
			m_pTypes[t]->SetBounds(x, y + t * (bh + gap), w, bh);
	}

private:
	CSboBrowserButton *m_pTypes[ops::OP_COUNT];
};

//-----------------------------------------------------------------------------
// The main window (Source's "Color Correction Tools")
//-----------------------------------------------------------------------------
class CCcMainWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcMainWindow, CSboToolWindow);

public:
	CCcMainWindow()
	    : BaseClass(g_pViewport, "CcEditor", "Color Correction Tools", "cc_main", 310, 340, 0.72f, 0.12f)
	{
		m_pEnable = new CSboBrowserCheck(this, "Enable", "Enable");
		m_pEnable->SetChecked(s_bEnable);
		m_pEnable->AddActionSignalTarget(this);
		m_pEntities = new CSboBrowserCheck(this, "Entities", "Enable Entities");
		m_pEntities->SetChecked(s_bEntities);
		m_pEntities->AddActionSignalTarget(this);
		m_pLookupView = new CSboBrowserCheck(this, "LookupView", "Lookup View");
		m_pLookupView->AddActionSignalTarget(this);
		m_pBlend = new CSboLabeledSlider(this, "Blend", "Blend", Range(0, 255));
		m_pBlend->SetColumns(44, 40);
		m_pBlend->SetDefaultValue(255.0f);
		m_pBlend->SetValue((float)s_iBlend);
		m_pBlend->AddActionSignalTarget(this);
		m_pList = new CSboOrderedList(this, "Operations");
		m_pList->SetIconPainter(PaintOpIcon);
		CSboBrowserButton *load = m_pList->AddButton("Load", "LoadLook");
		CSboBrowserButton *save = m_pList->AddButton("Save", "SaveLook");
		m_pList->AddActionSignalTarget(this);
		ShowMouseLookButton(true);
		SetToolTip(m_pEnable, "Show your look on the view");
		SetToolTip(m_pEntities, "Also show the map's own color correction entities, mixed with your look");
		SetToolTip(m_pLookupView, "Show the lookup table your look makes");
		SetToolTip(m_pBlend, "How strongly the view shows your look, from 0 for not at all to 255 for fully");
		SetToolTip(m_pList, "Your look's steps, applied from the top down, double-click one to edit it or click its icon to turn it off or on");
		SetToolTip(m_pList->NewButton(), "Add an operation after the selected one and open it");
		SetToolTip(m_pList->DeleteButton(), "Remove the selected operation");
		SetToolTip(m_pList->UpButton(), "Move the selected operation up, so it comes earlier");
		SetToolTip(m_pList->DownButton(), "Move the selected operation down, so it comes later");
		SetToolTip(load, "Load a look from gfx/colorcorrection/");
		SetToolTip(save, "Save your look as a .raw file");
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 0);
	}

	void SetLookupViewChecked(bool on) { m_pLookupView->SetChecked(on); }
	void ShowBlend() { m_pBlend->SetValue((float)s_iBlend); }

	//! A loaded look: these operations instead of the list (their windows close), or one more at the end (append)
	void SetOps(const std::vector<ops::Operation> &list, bool append)
	{
		if (!append)
		{
			for (auto &it : s_OpWindows)
			{
				it.second->CloseTool();
				it.second->MarkForDeletion();
			}
			s_OpWindows.clear();
			s_Ops.clear();
		}
		for (const ops::Operation &op : list)
		{
			OpEntry e;
			e.id = s_iNextId++;
			e.op = op;
			s_Ops.push_back(e);
		}
		RefreshList();
		m_pList->SetSelected(s_Ops.empty() ? -1 : (int)s_Ops.size() - 1);
		ShowBlend();
		MarkTableDirty();
	}
	void RefreshList()
	{
		const int sel = m_pList->GetSelected();
		m_pList->ClearRows();
		for (const OpEntry &e : s_Ops)
			m_pList->AddRow(e.op.name.c_str(), e.op.type, e.op.enabled);
		m_pList->SetSelected(std::min(sel, (int)s_Ops.size() - 1));
	}
	void SelectOp(int id) { m_pList->SetSelected(IndexOf(id)); }

	void AddOp(int type)
	{
		OpEntry e;
		e.id = s_iNextId++;
		e.op = ops::Make(type);
		// the selected operation's place + 1, as Source adds after the selection
		int at = m_pList->GetSelected();
		at = at < 0 ? (int)s_Ops.size() : at + 1;
		s_Ops.insert(s_Ops.begin() + at, e);
		RefreshList();
		m_pList->SetSelected(at);
		MarkTableDirty();
		OpenOp(e.id);
	}

	void OpenOp(int id)
	{
		OpEntry *e = FindOp(id);
		if (!e)
			return;
		CCcOpWindow *&w = s_OpWindows[id];
		if (!w)
			w = new CCcOpWindow(id, e->op.type);
		w->Refresh();
		w->Open();
		if (w->IsCollapsed())
			w->SetCollapsed(false); // opening an operation always shows it
	}

	void OnTick() override
	{
		// no longer allowed (sv_cheats below 2, another server): the editor closes and its look goes
		if (s_bLookSet && !Allowed())
		{
			CSboToolWindow::CloseAll();
			ClearLook();
			return;
		}
		// a new map forgets the editor's look (ColorCorr_VidInit): given again while the editor is open
		if (s_bLookSet && !ColorCorr_HasEditorLook())
		{
			s_bLookSet = false;
			if (IsOpen())
				s_bLookDirty = true;
			else
				return;
		}
		if (IsOpen() || s_bLookSet)
			UpdateLook();
	}

	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel)
	{
		if (panel == m_pEnable)
			s_bEnable = m_pEnable->IsChecked();
		else if (panel == m_pEntities)
			s_bEntities = m_pEntities->IsChecked();
		else if (panel == m_pLookupView)
		{
			if (!s_pLookup)
				s_pLookup = new CCcLookupWindow();
			if (m_pLookupView->IsChecked())
				s_pLookup->Open();
			else
				s_pLookup->CloseTool();
		}
		s_bLookDirty = true;
	}
	MESSAGE_FUNC_PARAMS(OnValueChanged, "ValueChanged", data)
	{
		s_iBlend = (int)data->GetFloat("value");
		s_bLookDirty = true;
		s_bModified = true;
		UpdateTitle();
	}
	MESSAGE_FUNC_INT(OnListActivated, "ListActivated", index)
	{
		if (index >= 0 && index < (int)s_Ops.size())
			OpenOp(s_Ops[index].id);
	}
	MESSAGE_FUNC_INT(OnListIconClicked, "ListIconClicked", index)
	{
		if (index < 0 || index >= (int)s_Ops.size())
			return;
		s_Ops[index].op.enabled = !s_Ops[index].op.enabled;
		m_pList->SetRowOn(index, s_Ops[index].op.enabled);
		MarkTableDirty();
	}
	MESSAGE_FUNC_INT_INT(OnListMoved, "ListMoved", from, to)
	{
		if (from >= 0 && to >= 0 && from < (int)s_Ops.size() && to < (int)s_Ops.size())
		{
			std::swap(s_Ops[from], s_Ops[to]);
			MarkTableDirty();
		}
	}
	MESSAGE_FUNC(OnListNew, "ListNew")
	{
		if (!s_pNew)
			s_pNew = new CCcNewWindow();
		s_pNew->Open();
	}
	MESSAGE_FUNC_INT(OnListDelete, "ListDelete", index)
	{
		if (index < 0 || index >= (int)s_Ops.size())
			return;
		const int id = s_Ops[index].id;
		auto it = s_OpWindows.find(id);
		if (it != s_OpWindows.end())
		{
			it->second->CloseTool();
			it->second->MarkForDeletion();
			s_OpWindows.erase(it);
		}
		s_Ops.erase(s_Ops.begin() + index);
		m_pList->RemoveRow(index);
		MarkTableDirty();
	}
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "SaveLook"))
		{
			OpenSave(nullptr);
			return;
		}
		if (!Q_stricmp(command, "LoadLook"))
		{
			OpenLoad();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OpenLoad();

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		const int y0 = y, row = M(22);
		m_pEnable->SetBounds(x, y, m_pEnable->GetContentWide(), row);
		m_pEntities->SetBounds(x + m_pEnable->GetContentWide() + M(16), y, m_pEntities->GetContentWide(), row);
		y += row;
		m_pLookupView->SetBounds(x, y, m_pLookupView->GetContentWide(), row);
		y += row + M(6);
		const int sh = M(CSboLabeledSlider::DesignTall());
		m_pBlend->SetBounds(x, y, w, sh);
		y += sh + M(4);
		m_pList->SetBounds(x, y, w, std::max(M(80), h - (y - y0)));
	}

private:
	CSboBrowserCheck *m_pEnable, *m_pEntities, *m_pLookupView;
	CSboLabeledSlider *m_pBlend;
	CSboOrderedList *m_pList;
};

//-----------------------------------------------------------------------------
// Load: the looks in gfx/colorcorrection/. A .raw with its operation file (<name>.ccops) brings back the whole list;
// any other table is added as one Lookup operation.
//-----------------------------------------------------------------------------
std::string OpsPathOf(const std::string &file)
{
	const size_t dot = file.rfind('.');
	return std::string(COLORCORR_FOLDER) + file.substr(0, dot) + ops::OPS_FILE_EXT;
}

bool IsRaw(const std::string &file)
{
	return file.size() > 4 && !Q_stricmp(file.c_str() + file.size() - 4, ".raw");
}

class CCcLoadWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CCcLoadWindow, CSboToolWindow);

public:
	CCcLoadWindow()
	    : BaseClass(g_pViewport, "CcLoad", "Load Look", "cc_load", 300, 300, 0.45f, 0.25f)
	{
		m_pList = new CSboOrderedList(this, "Looks");
		m_pList->SetStandardButtons(false);
		CSboBrowserButton *load = m_pList->AddButton("Load", "DoLoad");
		m_pList->AddActionSignalTarget(this);
		SetToolTip(m_pList, "Double-click a look or press Enter to load it");
		SetToolTip(load, "Load the selected look");
	}
	void Open() override
	{
		m_Files = ListTables();
		m_pList->ClearRows();
		for (const std::string &f : m_Files)
		{
			// a look with its operations says so
			const bool withOps = IsRaw(f) && g_pFullFileSystem->FileExists(OpsPathOf(f).c_str());
			m_pList->AddRow((f + (withOps ? "  (operations)" : "")).c_str(), 0, true);
		}
		m_Status = m_Files.empty() ? "There are no looks in gfx/colorcorrection/ yet" : "Double-click a look to load it";
		BaseClass::Open();
	}
	MESSAGE_FUNC_INT(OnListActivated, "ListActivated", index) { Load(index); } // double click or Enter
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "DoLoad"))
		{
			Load(m_pList->GetSelected());
			return;
		}
		BaseClass::OnCommand(command);
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		m_iTextY = y + h - M(34);
		m_pList->SetBounds(x, y, w, h - M(40));
	}
	void Paint() override
	{
		BaseClass::Paint();
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY, Widen(m_Status.c_str()), GetWide() - M(20));
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY + M(16), Widen(m_Status2.c_str()), GetWide() - M(20));
	}

private:
	void Load(int index);
	void DoLoad(int index);
	CSboOrderedList *m_pList;
	std::vector<std::string> m_Files;
	std::string m_Status, m_Status2;
	int m_iTextY = 0;
};

void CCcLoadWindow::Load(int index)
{
	if (index < 0 || index >= (int)m_Files.size() || !s_pMain)
		return;
	// a look with its operations replaces the list: unsaved changes are asked about first
	const std::string file = m_Files[index];
	if (IsRaw(file) && g_pFullFileSystem->FileExists(OpsPathOf(file).c_str()))
		ConfirmDiscard([this, index]() { DoLoad(index); });
	else
		DoLoad(index);
}

void CCcLoadWindow::DoLoad(int index)
{
	if (index < 0 || index >= (int)m_Files.size() || !s_pMain)
		return;
	const std::string file = m_Files[index];
	const std::string tablePath = std::string(COLORCORR_FOLDER) + file;
	const std::string opsPath = OpsPathOf(file);
	char err[160] = "", line[200];
	m_Status2.clear();
	if (IsRaw(file) && g_pFullFileSystem->FileExists(opsPath.c_str()))
	{
		// the whole list as it was saved
		std::vector<unsigned char> text;
		if (FileHandle_t fh = g_pFullFileSystem->Open(opsPath.c_str(), "rb"))
		{
			const unsigned size = g_pFullFileSystem->Size(fh);
			if (size > 0 && size <= (256u << 10))
			{
				text.resize(size);
				if (g_pFullFileSystem->Read(text.data(), (int)size, fh) != (int)size)
					text.clear();
			}
			g_pFullFileSystem->Close(fh);
		}
		std::vector<ops::Operation> list;
		int blend = 255;
		if (text.empty() || !ops::ReadOpsText((const char *)text.data(), (int)text.size(), list, blend, err, sizeof(err)))
		{
			snprintf(line, sizeof(line), "%s: %s", opsPath.c_str() + sizeof(COLORCORR_FOLDER) - 1,
			    text.empty() ? "can't read it" : err);
			m_Status = line;
			Repaint();
			return;
		}
		// Lookup operations read their tables again
		int missing = 0;
		for (ops::Operation &op : list)
			if (op.type == ops::OP_LOOKUP && !op.file.empty())
			{
				char lerr[160];
				if (!LoadTable((std::string(COLORCORR_FOLDER) + op.file).c_str(), op.lut, lerr, sizeof(lerr)))
				{
					op.lut = colorcorr::Lut();
					missing++;
				}
			}
		s_iBlend = blend;
		s_pMain->SetOps(list, false);
		UpdateLook();
		// the operations must give the saved table byte for byte
		colorcorr::Lut saved;
		const bool read = LoadTable(tablePath.c_str(), saved, err, sizeof(err));
		const bool same = read && saved.size == s_Lut.size && saved.rgb == s_Lut.rgb;
		snprintf(line, sizeof(line), "Loaded %d operations from %s", (int)list.size(), file.c_str());
		m_Status = line;
		m_Status2 = !read ? std::string("Couldn't read its table: ") + err
		            : (same ? "The table matches the .raw" : "The table doesn't match the .raw, which may have been changed outside the editor");
		if (missing)
			m_Status2 += ", a Lookup file is missing";
		char msg[300];
		snprintf(msg, sizeof(msg), "colorcorrectionui: loaded %s (%d operations), table %s\n", file.c_str(),
		    (int)list.size(), same ? "matches the .raw" : "DIFFERS from the .raw");
		sbo::DevPrint(msg);
		s_LastName = file.substr(0, file.size() - 4);
		s_bModified = false;
		UpdateTitle();
	}
	else
	{
		// a table without operations: one Lookup operation at the end of the list
		ops::Operation op = ops::Make(ops::OP_LOOKUP);
		op.file = file;
		op.name = file;
		if (!LoadTable(tablePath.c_str(), op.lut, err, sizeof(err)))
		{
			snprintf(line, sizeof(line), "%s: %s", file.c_str(), err);
			m_Status = line;
			Repaint();
			return;
		}
		s_pMain->SetOps({ op }, true);
		snprintf(line, sizeof(line), "Added %s as a Lookup operation", file.c_str());
		m_Status = line;
	}
	CloseTool(); // loaded: the list goes
}

void CCcMainWindow::OpenLoad()
{
	if (!s_pLoad)
		s_pLoad = new CCcLoadWindow();
	s_pLoad->Open();
}

void CCcLookupWindow::OnToolClosed()
{
	if (s_pMain)
		s_pMain->SetLookupViewChecked(false);
}

void CCcNewWindow::OnCommand(const char *command)
{
	if (!Q_strncmp(command, "new ", 4))
	{
		const int type = atoi(command + 4);
		CloseTool();
		if (s_pMain && type >= 0 && type < ops::OP_COUNT)
			s_pMain->AddOp(type);
		return;
	}
	BaseClass::OnCommand(command);
}

void CCcSaveWindow::Save()
{
	char name[64];
	if (!ops::CleanName(m_pName->GetValue(), name, sizeof(name)))
	{
		m_Status = "Use 1 to 39 letters, digits, _ and -";
		m_Hint.clear();
		Repaint();
		return;
	}
	char path[COLORCORR_MAX_PATH];
	snprintf(path, sizeof(path), "%s%s.raw", COLORCORR_FOLDER, name);
	// an existing file is replaced only on a second Save with the same name
	if (m_Confirm != name && g_pFullFileSystem->FileExists(path))
	{
		m_Confirm = name;
		m_Status = std::string(name) + ".raw already exists. Press Save again to replace it";
		m_Hint.clear();
		Repaint();
		return;
	}
	UpdateLook(); // the table of the operations as they are now
	g_pFullFileSystem->CreateDirHierarchy("gfx/colorcorrection", "GAMECONFIG");
	FileHandle_t fh = g_pFullFileSystem->Open(path, "wb", "GAMECONFIG");
	bool ok = false;
	if (fh)
	{
		ok = g_pFullFileSystem->Write(s_Lut.rgb.data(), (int)s_Lut.rgb.size(), fh) == (int)s_Lut.rgb.size();
		g_pFullFileSystem->Close(fh);
	}
	// the operations next to it, for the editor's Load (the game never reads them)
	if (ok)
	{
		std::vector<ops::Operation> list;
		for (const OpEntry &e : s_Ops)
			list.push_back(e.op);
		std::string text;
		ops::WriteOpsText(list, s_iBlend, text);
		char opsPath[COLORCORR_MAX_PATH * 2];
		snprintf(opsPath, sizeof(opsPath), "%s%s%s", COLORCORR_FOLDER, name, ops::OPS_FILE_EXT);
		FileHandle_t oh = g_pFullFileSystem->Open(opsPath, "wb", "GAMECONFIG");
		ok = oh != nullptr;
		if (oh)
		{
			ok = g_pFullFileSystem->Write(text.data(), (int)text.size(), oh) == (int)text.size();
			g_pFullFileSystem->Close(oh);
		}
	}
	if (ok)
	{
		s_LastName = name;
		s_bModified = false;
		UpdateTitle();
	}
	m_Confirm.clear();
	char line[160];
	if (!ok)
	{
		snprintf(line, sizeof(line), "Couldn't save %s", path);
		m_Status = line;
		m_Hint.clear();
	}
	else
	{
		snprintf(line, sizeof(line), "Saved %s/%s", sbo::GetGameDirectory(), path);
		m_Status = line;
		snprintf(line, sizeof(line), "To use it, set Lookup Table File to %s.raw", name);
		m_Hint = line;
		char msg[256];
		snprintf(msg, sizeof(msg), "colorcorrectionui: saved %s/%s (%d operations)\n", sbo::GetGameDirectory(), path,
		    (int)s_Ops.size());
		sbo::DevPrint(msg);
		if (s_AfterSave)
		{
			// saved for the unsaved-changes question: the window closes and what waited goes on
			std::function<void()> after = s_AfterSave;
			s_AfterSave = nullptr;
			m_bSaved = true;
			CloseTool();
			after();
			return;
		}
	}
	Repaint();
}

void OpenSave(std::function<void()> after)
{
	if (!s_pSave)
		s_pSave = new CCcSaveWindow();
	s_AfterSave = after;
	s_pSave->Open();
}

void UpdateTitle()
{
	if (s_pMain)
		s_pMain->SetToolTitle(("Color Correction Tools" + (s_LastName.empty() ? std::string() : " - " + s_LastName) +
		                          (s_bModified ? " *" : ""))
		                          .c_str());
}

//-----------------------------------------------------------------------------
// CCcOpWindow
//-----------------------------------------------------------------------------
//! Each operation type keeps its own window place
const char *OpWindowKey(int type)
{
	static const char *const keys[ops::OP_COUNT] = { "cc_op_curves", "cc_op_levels", "cc_op_selected_hsv",
		"cc_op_balance", "cc_op_lookup" };
	return type >= 0 && type < ops::OP_COUNT ? keys[type] : "cc_op";
}

//! The window's height for its type, design pixels
int OpWindowTall(int type)
{
	switch (type)
	{
	case ops::OP_CURVES:		return 390;
	case ops::OP_SELECTED_HSV:	return 480;
	case ops::OP_LOOKUP:		return 165;
	}
	return 310;
}

CCcOpWindow::CCcOpWindow(int id, int type)
    : BaseClass(g_pViewport, "CcOperation", ops::TypeName(type), OpWindowKey(type), 270, OpWindowTall(type), 0.42f,
          0.12f)
    , m_iId(id)
    , m_iType(type)
{
	m_pName = new CSboTextField(this, "Name", "Name");
	m_pName->SetMaximumCharCount(40);
	m_pName->AddActionSignalTarget(this);

	switch (type)
	{
	case ops::OP_CURVES:
	case ops::OP_LEVELS:
		m_pChannel = new CSboBrowserSelect(this, "Channel");
		for (const char *c : { "RGB", "Red", "Green", "Blue" })
			m_pChannel->AddItem(c);
		m_pChannel->AddActionSignalTarget(this);
		if (type == ops::OP_CURVES)
		{
			m_pCurve = new CSboCurveEditor(this, "Curve");
			m_pCurve->AddActionSignalTarget(this);
			m_pResetCurve = new CSboBrowserButton(this, "Reset", "Reset", "ResetCurve", CSboBrowserButton::KIND_NORMAL);
		}
		else
		{
			const ops::Levels def;
			m_pLevels[0] = Slider("Input Black", Range(0, 254), (float)def.inBlack);
			m_pLevels[1] = Slider("Input White", Range(1, 255), (float)def.inWhite);
			m_pLevels[2] = Slider("Gamma", Range(0.1f, 9.99f, 0.01f, 2), def.gamma);
			m_pLevels[3] = Slider("Output Black", Range(0, 255), (float)def.outBlack);
			m_pLevels[4] = Slider("Output White", Range(0, 255), (float)def.outWhite);
		}
		break;
	case ops::OP_SELECTED_HSV:
		m_pMode = new CSboBrowserSelect(this, "Mode");
		m_pMode->AddItem("Select Nearby RGB");
		m_pMode->AddItem("Select Nearby Hue");
		m_pMode->AddActionSignalTarget(this);
		m_pSelect = new CSboBrowserButton(this, "Select", "Select", "PickColor", CSboBrowserButton::KIND_NORMAL);
		m_pPreview = new CSboRgbaImage(this, "Preview");
		m_pPreview->SetEmptyText(L"No picture of the view");
		m_pPreview->AddActionSignalTarget(this);
		m_pTarget = new CSboColorSwatch(this, "Target", "Selected Color");
		m_pTarget->AddActionSignalTarget(this);
		SetToolTip(m_pTarget, "The color this operation changes, click it to choose one on a color wheel");
		m_pColorize = Check("Colorize");
		m_pInvert = Check("Invert Selection");
	{
		const ops::Operation def = ops::Make(ops::OP_SELECTED_HSV);
		m_pHsv[0] = Slider("Hue", Range(-360, 360), (float)def.hue);
		m_pHsv[1] = Slider("Saturation", Range(-255, 255), (float)def.saturation);
		m_pHsv[2] = Slider("Value", Range(-255, 255), (float)def.value);
		m_pHsv[3] = Slider("Tolerance", Range(0, 255), (float)def.tolerance);
		m_pHsv[4] = Slider("Fuzziness", Range(0, 255), (float)def.fuzziness);
	}
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 50);
		break;
	case ops::OP_BALANCE:
		m_pBalance[0] = Slider("Cyan / Red", Range(-100, 100), 0.0f);
		m_pBalance[1] = Slider("Magenta / Green", Range(-100, 100), 0.0f);
		m_pBalance[2] = Slider("Yellow / Blue", Range(-100, 100), 0.0f);
		m_pRanges[0] = Check("Shadows");
		m_pRanges[1] = Check("Midtones");
		m_pRanges[2] = Check("Highlights");
		m_pPreserve = Check("Preserve Luminosity");
		break;
	case ops::OP_LOOKUP:
		m_pFile = new CSboBrowserSelect(this, "File");
		m_pFile->AddActionSignalTarget(this);
		m_pReload = new CSboBrowserButton(this, "Reload", "Reload", "ReloadFiles", CSboBrowserButton::KIND_NORMAL);
		break;
	}
	m_pBlend = Slider("Blend Factor", Range(0, 255), 255.0f);

	SetToolTip(m_pName, "The operation's name in the list");
	SetToolTip(m_pBlend, "How much of this step's result replaces what came into it, from 0 for none to 255 for all");
	SetToolTip(m_pChannel, "Change all three colors (RGB) or only red, green or blue");
	SetToolTip(m_pCurve, "Click the line to add a point, drag a point to move it, or right-click a point to remove it");
	SetToolTip(m_pResetCurve, "Make the line straight again");
	static const char *const levelTips[5] = {
		"The brightness that becomes black, so everything darker is black too",
		"The brightness that becomes white, so everything lighter is white too",
		"Above 1 makes the mid tones lighter and below 1 darker",
		"The darkest the result can be",
		"The lightest the result can be",
	};
	for (int i = 0; i < 5; i++)
		SetToolTip(m_pLevels[i], levelTips[i]);
	SetToolTip(m_pMode, "Choose how a color counts as near: by its red, green and blue, or by its hue");
	SetToolTip(m_pSelect, "Then click the screen to pick the color to change");
	SetToolTip(m_pPreview, "A picture of your view, click it to pick the color to change");
	SetToolTip(m_pColorize, "Give the selected colors one hue, the one Hue sets");
	SetToolTip(m_pInvert, "Change every color except the selected ones");
	static const char *const hsvTips[5] = {
		"How far to turn the hue of the selected colors, or with Colorize the hue they get",
		"Makes the selected colors more colorful or more gray",
		"Makes the selected colors lighter or darker",
		"How far from the chosen color a color can be and still change fully",
		"How far past Tolerance the change fades out",
	};
	for (int i = 0; i < 5; i++)
		SetToolTip(m_pHsv[i], hsvTips[i]);
	static const char *const balanceTips[3] = {
		"Moves the colors toward cyan or toward red, in the tones chosen below",
		"Moves the colors toward magenta or toward green, in the tones chosen below",
		"Moves the colors toward yellow or toward blue, in the tones chosen below",
	};
	static const char *const rangeTips[3] = { "Set the balance of the dark tones", "Set the balance of the middle tones",
		"Set the balance of the light tones" };
	for (int i = 0; i < 3; i++)
	{
		SetToolTip(m_pBalance[i], balanceTips[i]);
		SetToolTip(m_pRanges[i], rangeTips[i]);
	}
	SetToolTip(m_pPreserve, "Keep how light each color is");
	SetToolTip(m_pFile, "The look or table file from gfx/colorcorrection/ to use");
	SetToolTip(m_pReload, "Read the list of files again");
}

CSboLabeledSlider *CCcOpWindow::Slider(const char *label, const SboValueRange &range, float def)
{
	CSboLabeledSlider *s = new CSboLabeledSlider(this, "Slider", label, range);
	s->SetDefaultValue(def); // a right click on the slider: the operation's default
	s->SetColumns(96, 44);
	s->AddActionSignalTarget(this);
	return s;
}

CSboBrowserCheck *CCcOpWindow::Check(const char *text)
{
	CSboBrowserCheck *c = new CSboBrowserCheck(this, "Check", text);
	c->AddActionSignalTarget(this);
	return c;
}

ops::Operation *CCcOpWindow::Op()
{
	OpEntry *e = FindOp(m_iId);
	return e ? &e->op : nullptr;
}

void CCcOpWindow::Changed()
{
	MarkTableDirty();
	m_bPreviewDirty = true;
}

void CCcOpWindow::ShowChannel()
{
	ops::Operation *op = Op();
	if (!op || !m_pChannel)
		return;
	const int ch = std::max(0, std::min(ops::CH_COUNT - 1, m_pChannel->GetSelected()));
	if (m_pCurve)
	{
		static const Color lines[ops::CH_COUNT] = { Color(198, 242, 176, 255), Color(240, 90, 80, 255),
			Color(110, 220, 100, 255), Color(100, 150, 250, 255) };
		m_pCurve->SetPoints(op->curves[ch].points, op->curves[ch].count);
		m_pCurve->SetLineColor(lines[ch]);
	}
	if (m_pLevels[0])
	{
		const ops::Levels &l = op->levels[ch];
		m_pLevels[0]->SetValue((float)l.inBlack);
		m_pLevels[1]->SetValue((float)l.inWhite);
		m_pLevels[2]->SetValue(l.gamma);
		m_pLevels[3]->SetValue((float)l.outBlack);
		m_pLevels[4]->SetValue((float)l.outWhite);
	}
}

void CCcOpWindow::ShowRange()
{
	ops::Operation *op = Op();
	if (!op || !m_pBalance[0])
		return;
	for (int i = 0; i < 3; i++)
	{
		m_pRanges[i]->SetChecked(i == m_iRange);
		m_pBalance[i]->SetValue((float)op->balance[m_iRange][i]);
	}
}

void CCcOpWindow::Refresh()
{
	ops::Operation *op = Op();
	if (!op)
		return;
	SetToolTitle(op->name.c_str());
	m_pName->SetValue(op->name.c_str());
	m_pBlend->SetValue((float)op->blend);
	ShowChannel();
	if (m_pMode)
	{
		m_pMode->SetSelected(op->selectMode);
		m_pTarget->SetColor(op->target[0], op->target[1], op->target[2]);
		m_pColorize->SetChecked(op->colorize);
		m_pInvert->SetChecked(op->invert);
		m_pHsv[0]->SetValue((float)op->hue);
		m_pHsv[1]->SetValue((float)op->saturation);
		m_pHsv[2]->SetValue((float)op->value);
		m_pHsv[3]->SetValue((float)op->tolerance);
		m_pHsv[4]->SetValue((float)op->fuzziness);
	}
	ShowRange();
	if (m_pPreserve)
		m_pPreserve->SetChecked(op->preserveLuminosity);
	if (m_pFile)
	{
		m_Files = ListTables();
		m_pFile->ClearItems();
		m_pFile->AddItem("(none)");
		int sel = 0;
		for (int i = 0; i < (int)m_Files.size(); i++)
		{
			m_pFile->AddItem(m_Files[i].c_str());
			if (!Q_stricmp(m_Files[i].c_str(), op->file.c_str()))
				sel = i + 1;
		}
		m_pFile->SetSelected(sel);
		if (op->file.empty())
			m_LookupStatus = m_Files.empty() ? "There are no table files in gfx/colorcorrection/ yet" : "Choose a table file";
	}
	if (m_pPreview && m_Shot.empty())
		TakeSnapshot(false); // a first picture of the view
}

void CCcOpWindow::OnToolClosed()
{
	if (s_pPicker && s_pPicker->IsPicking())
		s_pPicker->Cancel();
	if (m_pTarget)
		m_pTarget->ClosePicker();
}

void CCcOpWindow::TakeSnapshot(bool pick)
{
	m_bShotWanted = true;
	m_bPickWanted = pick;
	ColorCorr_GetSnapshot(nullptr, nullptr, nullptr, &m_iShotSerial);
	ColorCorr_RequestSnapshot();
}

void CCcOpWindow::SetTarget(int r, int g, int b)
{
	ops::Operation *op = Op();
	if (!op)
		return;
	op->target[0] = r;
	op->target[1] = g;
	op->target[2] = b;
	m_pTarget->SetColor(r, g, b);
	Changed();
}

void CCcOpWindow::OnTick()
{
	BaseClass::OnTick();
	if (!m_pPreview)
		return;
	if (m_bShotWanted)
	{
		const unsigned char *rgb = nullptr;
		int w = 0, h = 0, serial = 0;
		if (ColorCorr_GetSnapshot(&rgb, &w, &h, &serial) && serial != m_iShotSerial)
		{
			m_bShotWanted = false;
			m_iShotSerial = serial;
			if (m_bPickWanted)
			{
				// the picked point's color: the average of 3 x 3 pixels there, in the view's own pixels
				m_bPickWanted = false;
				int sw, sh;
				vgui2::surface()->GetScreenSize(sw, sh);
				const int px = std::max(1, std::min(w - 2, (int)((long long)m_iPickX * w / std::max(1, sw))));
				const int py = std::max(1, std::min(h - 2, (int)((long long)m_iPickY * h / std::max(1, sh))));
				int sum[3] = { 0, 0, 0 };
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++)
						for (int c = 0; c < 3; c++)
							sum[c] += rgb[((size_t)(py + dy) * w + px + dx) * 3 + c];
				SetTarget(sum[0] / 9, sum[1] / 9, sum[2] / 9);
			}
			// the preview: the view scaled down to PREVIEW_WIDE (nearest pixel)
			m_iShotW = PREVIEW_WIDE;
			m_iShotH = std::max(1, (int)((long long)h * PREVIEW_WIDE / std::max(1, w)));
			m_Shot.resize((size_t)m_iShotW * m_iShotH * 3);
			for (int y = 0; y < m_iShotH; y++)
				for (int x = 0; x < m_iShotW; x++)
				{
					const int sx = (int)((long long)x * w / m_iShotW), sy = (int)((long long)y * h / m_iShotH);
					memcpy(&m_Shot[((size_t)y * m_iShotW + x) * 3], &rgb[((size_t)sy * w + sx) * 3], 3);
				}
			m_bPreviewDirty = true;
		}
	}
	if (m_bPreviewDirty)
		UpdatePreview();
}

void CCcOpWindow::UpdatePreview()
{
	m_bPreviewDirty = false;
	ops::Operation *op = Op();
	if (!op || m_Shot.empty())
		return;
	// the selection on the copy of the view: selected parts with the operation applied, the rest darkened
	std::vector<unsigned char> rgba((size_t)m_iShotW * m_iShotH * 4);
	ops::Operation full = *op;
	full.enabled = true;
	for (size_t i = 0, n = (size_t)m_iShotW * m_iShotH; i < n; i++)
	{
		float c[3] = { m_Shot[i * 3] / 255.0f, m_Shot[i * 3 + 1] / 255.0f, m_Shot[i * 3 + 2] / 255.0f };
		const float w = ops::SelectionWeight(full, c);
		float out[3] = { c[0], c[1], c[2] };
		ops::ApplyOne(full, out);
		const float dim = 0.25f + 0.75f * w;
		for (int k = 0; k < 3; k++)
			rgba[i * 4 + k] = (unsigned char)(std::max(0.0f, std::min(1.0f, out[k] * dim)) * 255.0f + 0.5f);
		rgba[i * 4 + 3] = 255;
	}
	m_pPreview->SetImage(rgba.data(), m_iShotW, m_iShotH);
}

void CCcOpWindow::OnValueChanged(KeyValues *data)
{
	ops::Operation *op = Op();
	if (!op)
		return;
	vgui2::Panel *p = (vgui2::Panel *)data->GetPtr("panel");
	const float v = data->GetFloat("value");
	const int iv = (int)floorf(v + 0.5f);
	if (p == m_pBlend)
		op->blend = iv;
	const int ch = m_pChannel ? std::max(0, std::min(ops::CH_COUNT - 1, m_pChannel->GetSelected())) : 0;
	ops::Levels &l = op->levels[ch];
	if (p == m_pLevels[0])
		l.inBlack = std::min(iv, l.inWhite - 1);
	else if (p == m_pLevels[1])
		l.inWhite = std::max(iv, l.inBlack + 1);
	else if (p == m_pLevels[2])
		l.gamma = v;
	else if (p == m_pLevels[3])
		l.outBlack = iv;
	else if (p == m_pLevels[4])
		l.outWhite = iv;
	if (m_pLevels[0] && (p == m_pLevels[0] || p == m_pLevels[1]))
	{
		// input black stays below input white
		m_pLevels[0]->SetValue((float)l.inBlack);
		m_pLevels[1]->SetValue((float)l.inWhite);
	}
	if (p == m_pHsv[0])
		op->hue = iv;
	else if (p == m_pHsv[1])
		op->saturation = iv;
	else if (p == m_pHsv[2])
		op->value = iv;
	else if (p == m_pHsv[3])
		op->tolerance = iv;
	else if (p == m_pHsv[4])
		op->fuzziness = iv;
	for (int i = 0; i < 3; i++)
		if (p && p == m_pBalance[i])
			op->balance[m_iRange][i] = iv;
	Changed();
}

void CCcOpWindow::OnButtonToggled(vgui2::Panel *panel)
{
	ops::Operation *op = Op();
	if (!op)
		return;
	if (panel == m_pColorize)
		op->colorize = m_pColorize->IsChecked();
	else if (panel == m_pInvert)
		op->invert = m_pInvert->IsChecked();
	else if (panel == m_pPreserve)
		op->preserveLuminosity = m_pPreserve->IsChecked();
	for (int i = 0; i < 3; i++)
		if (panel && panel == m_pRanges[i])
		{
			// radio buttons: the clicked range is the one shown
			m_iRange = i;
			ShowRange();
			return;
		}
	Changed();
}

void CCcOpWindow::OnSelectChanged(vgui2::Panel *panel)
{
	ops::Operation *op = Op();
	if (!op)
		return;
	if (panel == m_pChannel)
	{
		ShowChannel();
		return;
	}
	if (panel == m_pMode)
		op->selectMode = m_pMode->GetSelected();
	else if (panel == m_pFile)
	{
		const int sel = m_pFile->GetSelected();
		op->lut = colorcorr::Lut();
		op->file.clear();
		m_LookupStatus = "Choose a table file";
		if (sel >= 1 && sel <= (int)m_Files.size())
		{
			op->file = m_Files[sel - 1];
			char path[COLORCORR_MAX_PATH * 2], err[160] = "";
			snprintf(path, sizeof(path), "%s%s", COLORCORR_FOLDER, op->file.c_str());
			if (LoadTable(path, op->lut, err, sizeof(err)))
			{
				char line[96];
				snprintf(line, sizeof(line), "%d x %d x %d table", op->lut.size, op->lut.size, op->lut.size);
				m_LookupStatus = line;
			}
			else
			{
				op->lut = colorcorr::Lut();
				m_LookupStatus = std::string("Can't use it: ") + err;
			}
		}
		Repaint();
	}
	Changed();
}

void CCcOpWindow::OnCurveChanged()
{
	ops::Operation *op = Op();
	if (!op || !m_pCurve || !m_pChannel)
		return;
	const int ch = std::max(0, std::min(ops::CH_COUNT - 1, m_pChannel->GetSelected()));
	op->curves[ch].count = m_pCurve->GetPoints(op->curves[ch].points, sboctl::CURVE_MAX_POINTS);
	Changed();
}

void CCcOpWindow::OnColorChanged(KeyValues *data)
{
	SetTarget(data->GetInt("r"), data->GetInt("g"), data->GetInt("b"));
}

void CCcOpWindow::OnTextEntered(KeyValues *data)
{
	ops::Operation *op = Op();
	const char *text = data->GetString("text");
	if (!op || !text[0])
	{
		if (op)
			m_pName->SetValue(op->name.c_str()); // never empty
		return;
	}
	op->name = text;
	SetToolTitle(text);
	if (s_pMain)
		s_pMain->RefreshList();
}

void CCcOpWindow::OnScreenPicked(int x, int y)
{
	m_iPickX = x;
	m_iPickY = y;
	TakeSnapshot(true);
}

void CCcOpWindow::OnScreenPickCancelled()
{
}

void CCcOpWindow::OnImageClicked(int x, int y)
{
	// a click on the picture picks its color there
	if (x < 0 || y < 0 || x >= m_iShotW || y >= m_iShotH || m_Shot.empty())
		return;
	const unsigned char *p = &m_Shot[((size_t)y * m_iShotW + x) * 3];
	SetTarget(p[0], p[1], p[2]);
}

void CCcOpWindow::OnCommand(const char *command)
{
	ops::Operation *op = Op();
	if (!Q_stricmp(command, "PickColor"))
	{
		if (!s_pPicker)
			s_pPicker = new CSboScreenPicker(g_pViewport);
		s_pPicker->Start(this);
		return;
	}
	if (!Q_stricmp(command, "ResetCurve") && op && m_pChannel)
	{
		const int ch = std::max(0, std::min(ops::CH_COUNT - 1, m_pChannel->GetSelected()));
		op->curves[ch] = ops::Curve();
		ShowChannel();
		Changed();
		return;
	}
	if (!Q_stricmp(command, "ReloadFiles"))
	{
		Refresh();
		if (op && !op->file.empty())
			OnSelectChanged(m_pFile); // the file read again
		return;
	}
	BaseClass::OnCommand(command);
}

void CCcOpWindow::Paint()
{
	BaseClass::Paint();
	if (m_iType == ops::OP_LOOKUP)
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iStatusY, Widen(m_LookupStatus.c_str()), GetWide() - M(20));
}

void CCcOpWindow::LayoutContent(int x, int y, int w, int h)
{
	const int y0 = y, row = M(22), gap = M(6), sh = M(CSboLabeledSlider::DesignTall());
	m_pName->SetBounds(x, y, w, row);
	y += row + gap;
	auto slider = [&](CSboLabeledSlider *s) {
		if (s)
		{
			s->SetBounds(x, y, w, sh);
			y += sh;
		}
	};
	switch (m_iType)
	{
	case ops::OP_CURVES:
	{
		m_pChannel->SetBounds(x, y, w - M(80), row);
		m_pResetCurve->SetBounds(x + w - M(72), y, M(72), row);
		y += row + gap;
		const int side = std::max(M(60), std::min(w, h - (y - y0) - sh - gap));
		m_pCurve->SetBounds(x + (w - side) / 2, y, side, side);
		y += side + gap;
		break;
	}
	case ops::OP_LEVELS:
		m_pChannel->SetBounds(x, y, w, row);
		y += row + gap;
		for (CSboLabeledSlider *s : m_pLevels)
			slider(s);
		break;
	case ops::OP_SELECTED_HSV:
	{
		m_pMode->SetBounds(x, y, w - M(80), row);
		m_pSelect->SetBounds(x + w - M(72), y, M(72), row);
		y += row + gap;
		const int ph = w * 9 / 16;
		m_pPreview->SetBounds(x, y, w, ph);
		y += ph + gap;
		m_pTarget->SetBounds(x, y + M(2), M(36), row - M(4));
		m_pColorize->SetBounds(x + M(46), y, m_pColorize->GetContentWide(), row);
		m_pInvert->SetBounds(x + M(46) + m_pColorize->GetContentWide() + M(10), y, m_pInvert->GetContentWide(), row);
		y += row + gap;
		for (CSboLabeledSlider *s : m_pHsv)
			slider(s);
		break;
	}
	case ops::OP_BALANCE:
		for (CSboLabeledSlider *s : m_pBalance)
			slider(s);
		y += M(2);
		for (int i = 0; i < 3; i++)
		{
			m_pRanges[i]->SetBounds(x, y, m_pRanges[i]->GetContentWide(), row);
			y += row;
		}
		y += M(2);
		m_pPreserve->SetBounds(x, y, m_pPreserve->GetContentWide(), row);
		y += row + gap;
		break;
	case ops::OP_LOOKUP:
		m_pFile->SetBounds(x, y, w - M(80), row);
		m_pReload->SetBounds(x + w - M(72), y, M(72), row);
		y += row + M(4);
		m_iStatusY = y;
		y += M(18) + gap;
		break;
	}
	slider(m_pBlend);
}

//-----------------------------------------------------------------------------
// colorcorrectionui
//-----------------------------------------------------------------------------
void Cmd_ColorCorrectionUI()
{
	if (!Allowed())
	{
		sbo::ConsolePrint("The color correction editor only works in single player, in a game you host, or on a server with sv_cheats 2\n");
		return;
	}
	if (!s_pMain)
		s_pMain = new CCcMainWindow();
	if (s_pMain->IsOpen())
	{
		// all of the editor's windows close together; its look stays (Enable turns it off)
		CSboToolWindow::CloseAll();
		return;
	}
	s_bLookDirty = true;
	UpdateLook();
	s_pMain->RefreshList();
	s_pMain->SetLookupViewChecked(false);
	s_pMain->Open();
}
} // namespace

bool VGui2_CcEditor_Init()
{
	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;
	sbo::AddCommand("colorcorrectionui", &Cmd_ColorCorrectionUI);
	VGui2_ToolWindows_RegisterCommands();
	return true;
}
