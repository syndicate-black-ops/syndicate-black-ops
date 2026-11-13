//========= SBO VGUI2: the particle editor =========//
//
// Purpose: see sbo_particle_editor.h. The script is an auroradoc::Doc (aurora/aurora_doc.h): every change edits its
//          text, and the preview (aurora/aurora_editor.h) runs that text as the game would.
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

#include "../aurora/aurora_doc.h"
#include "../aurora/aurora_editor.h"
#include "../aurora/aurora_keys.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "controls/sbo_list_controls.h"
#include "controls/sbo_picture_picker.h"
#include "controls/sbo_tool_window.h"
#include "controls/sbo_value_controls.h"
#include "sbo_bridge.h"
#include "sbo_image_decode.h" // the sprite pictures
#include "sbo_particle_editor.h"
#include "sbo_server_browser.h" // CSboBrowserButton, CSboBrowserCheck, CSboBrowserSelect
#include "sbo_ui_style.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
constexpr const char SCRIPT_FOLDER[] = "aurora/";
constexpr unsigned MAX_SCRIPT_BYTES = 1u << 20;
constexpr int MAX_LIST_FILES = 4096;

class CPeMainWindow;
class CPeTypeWindow;
class CPeSaveWindow;
class CPeLoadWindow;

auroradoc::Doc s_Doc;
std::string s_File;     // "aurora/<name>.aur" loaded or saved last ("" for a new script)
bool s_bModified = false; // changed since the last load or save
bool s_bPreviewOn = true;
float s_flPitch = 0, s_flYaw = 0;
bool s_bStarted = false; // the editor was opened once (its script exists)

CPeMainWindow *s_pMain = nullptr;
CSboPicturePicker *s_pSprites = nullptr;
CPeSaveWindow *s_pSave = nullptr;
CPeLoadWindow *s_pLoad = nullptr;
std::map<int, CPeTypeWindow *> s_TypeWindows; // by type index
CSboScreenPicker *s_pPicker = nullptr;
std::function<void()> s_AfterSave; // what waits for a save (the unsaved-changes question's Save)

void UpdateTitle();
void OpenSaveWindow(std::function<void()> after);

//! The script's short name for questions and the title ("new script" before its first save)
std::string ShortName()
{
	if (s_File.empty())
		return "new script";
	const size_t slash = s_File.rfind('/');
	return slash == std::string::npos ? s_File : s_File.substr(slash + 1);
}

//! Unsaved changes: asks "Save changes to <name>?" (Save, Don't save, Cancel), then goes on unless cancelled;
//! without changes it goes on at once
void ConfirmDiscard(std::function<void()> next)
{
	if (!s_bModified)
	{
		next();
		return;
	}
	const std::string q = "Save changes to " + ShortName() + "?";
	CSboChoiceDialog::Get()->Ask("Unsaved changes", q.c_str(), { "Save", "Don't save", "Cancel" }, [next](int choice) {
		if (choice == 0)
			OpenSaveWindow(next); // goes on once the script is saved
		else if (choice == 1)
			next();
	});
}

//! The name the preview's problems show: the file, or "new script"
const char *PreviewName()
{
	return s_File.empty() ? "new script" : s_File.c_str();
}

void SendPreview()
{
	AuroraEditor_SetPreview(s_Doc.Text().c_str(), PreviewName());
	AuroraEditor_SetPreviewOn(s_bPreviewOn);
	AuroraEditor_SetTurn(s_flPitch, s_flYaw);
}

void StopPreview()
{
	AuroraEditor_SetPreview(nullptr, nullptr);
}

SboValueRange Range(float min, float max, float step, int decimals)
{
	SboValueRange r;
	r.min = min;
	r.max = max;
	r.step = step;
	r.decimals = decimals;
	return r;
}

std::vector<std::string> SplitChoices(const char *choices)
{
	std::vector<std::string> out;
	if (!choices)
		return out;
	std::string cur;
	for (const char *p = choices;; p++)
	{
		if (!*p || *p == '|')
		{
			out.push_back(cur);
			cur.clear();
			if (!*p)
				break;
		}
		else
			cur += *p;
	}
	return out;
}

//! The game's search paths a list looks in: all of them, and by name SBO's, the downloads and Half-Life's (an engine
//! that does not know a name finds nothing there)
const char *const LIST_PATH_IDS[] = { nullptr, "GAME", "GAMEDOWNLOAD", "DEFAULTGAME" };

//! Files matching the patterns in every game folder (SBO's, Half-Life's, downloads), once each, sorted
std::vector<std::string> ListFiles(const std::vector<std::string> &patterns)
{
	std::vector<std::string> files;
	for (const char *pathID : LIST_PATH_IDS)
	for (const std::string &pattern : patterns)
	{
		// the folder part of the pattern goes in front of each name found
		const size_t slash = pattern.rfind('/');
		const std::string dir = slash == std::string::npos ? std::string() : pattern.substr(0, slash + 1);
		FileFindHandle_t fh;
		const char *fn = g_pFullFileSystem->FindFirst(pattern.c_str(), &fh, pathID);
		while (fn && (int)files.size() < MAX_LIST_FILES)
		{
			if (fn[0] && fn[0] != '.' && !g_pFullFileSystem->FindIsDirectory(fh))
			{
				std::string s = Lower(dir + fn);
				if (std::find(files.begin(), files.end(), s) == files.end())
					files.push_back(s);
			}
			fn = g_pFullFileSystem->FindNext(fh);
		}
		g_pFullFileSystem->FindClose(fh);
	}
	std::sort(files.begin(), files.end());
	return files;
}

//! The folders one level under dir (for sprites/<folder>/*.spr)
std::vector<std::string> ListFolders(const char *dir)
{
	std::vector<std::string> out;
	const std::string pattern = std::string(dir) + "*";
	for (const char *pathID : LIST_PATH_IDS)
	{
		FileFindHandle_t fh;
		const char *fn = g_pFullFileSystem->FindFirst(pattern.c_str(), &fh, pathID);
		while (fn && out.size() < 256)
		{
			if (fn[0] && fn[0] != '.' && g_pFullFileSystem->FindIsDirectory(fh))
			{
				const std::string s = Lower(fn);
				if (std::find(out.begin(), out.end(), s) == out.end())
					out.push_back(s);
			}
			fn = g_pFullFileSystem->FindNext(fh);
		}
		g_pFullFileSystem->FindClose(fh);
	}
	return out;
}

//! A whole file from the game's folders, if it is at most maxBytes (scripts: MAX_SCRIPT_BYTES; sprites: the picture
//! decoder's own limit, sbo_image::MAX_FILE_BYTES)
bool ReadGameFile(const char *path, std::string &out, size_t maxBytes = MAX_SCRIPT_BYTES)
{
	out.clear();
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
	if (!fh)
		return false;
	const unsigned size = g_pFullFileSystem->Size(fh);
	bool ok = size <= maxBytes;
	if (ok && size)
	{
		out.resize(size);
		ok = g_pFullFileSystem->Read(&out[0], (int)size, fh) == (int)size;
	}
	g_pFullFileSystem->Close(fh);
	if (!ok)
		out.clear();
	return ok;
}

void OnDocChanged(CPeTypeWindow *source = nullptr);
void RefreshAll(CPeTypeWindow *except = nullptr);
void CloseTypeWindows();
void OpenSpritePicker(int typeIndex);

//! An operation's icon in the list: a square in the type's start color, framed when it is the main type
void PaintTypeIcon(int index, bool on, int x, int y, int size)
{
	if (index < 0 || index >= s_Doc.TypeCount())
		return;
	float rgb[3] = { 1, 1, 1 };
	static const char *const keys[3] = { "startred", "startgreen", "startblue" };
	for (int i = 0; i < 3; i++)
		if (const std::string *v = s_Doc.Get(index, keys[i]))
		{
			float a, b;
			bool r;
			auroradoc::ParseRange(*v, a, b, r);
			rgb[i] = std::max(0.0f, std::min(1.0f, (a + b) * 0.5f));
		}
	const int pad = size / 5;
	FillRect(x + pad, y + pad, size - 2 * pad, size - 2 * pad,
	    Color((int)(rgb[0] * 255), (int)(rgb[1] * 255), (int)(rgb[2] * 255), 255));
	const std::string *main = s_Doc.Get(-1, "maintype");
	const bool isMain = main && !Q_stricmp(main->c_str(), s_Doc.TypeName(index).c_str());
	OutlineRect(x + pad - 1, y + pad - 1, size - 2 * pad + 2, size - 2 * pad + 2, isMain ? COL_TEXT_BRIGHT : COL_BORDER);
	(void)on;
}

//-----------------------------------------------------------------------------
// One key's row: a slider with its number (and a "to" box for a range), a choice, or a text with a Pick button
//-----------------------------------------------------------------------------
struct KeyRow
{
	const AuroraKeyDef *def = nullptr;
	CSboLabeledSlider *slider = nullptr;
	CSboTextField *to = nullptr;      // a range's maximum ("" = one value)
	CSboBrowserSelect *select = nullptr;
	CSboTextField *text = nullptr;
	CSboBrowserButton *pick = nullptr;
	std::vector<std::string> items; // the select's values ("" = not set)

	bool Owns(vgui2::Panel *p) const { return p && (p == slider || p == to || p == select || p == text || p == pick); }
	void SetVisible(bool v)
	{
		for (vgui2::Panel *p : { (vgui2::Panel *)slider, (vgui2::Panel *)to, (vgui2::Panel *)select,
		         (vgui2::Panel *)text, (vgui2::Panel *)pick })
			if (p)
				p->SetVisible(v);
	}
};

//! Creates the row's controls on parent for the key, with the key's tip
void MakeRow(CSboToolWindow *parent, KeyRow &row, const AuroraKeyDef *def)
{
	row.def = def;
	switch (def->kind)
	{
	case AURKEY_INT:
	case AURKEY_NUMBER:
	case AURKEY_RANGE:
		row.slider = new CSboLabeledSlider(parent, "Key", def->label,
		    Range(def->uiMin, def->uiMax, def->step > 0 ? def->step : 1.0f, def->decimals));
		row.slider->SetColumns(104, 48);
		row.slider->SetDefaultValue(def->hasDefault ? def->def : def->uiMin);
		row.slider->AddActionSignalTarget(parent);
		parent->SetToolTip(row.slider, def->tip);
		if (def->kind == AURKEY_RANGE)
		{
			row.to = new CSboTextField(parent, "To", "to");
			row.to->SetMaximumCharCount(12);
			row.to->AddActionSignalTarget(parent);
			parent->SetToolTip(row.to, "Type a second number for a range, and each particle picks its own value between the two");
		}
		break;
	case AURKEY_ENUM:
	case AURKEY_TYPE:
		row.select = new CSboBrowserSelect(parent, "Choice");
		row.select->AddActionSignalTarget(parent);
		parent->SetToolTip(row.select, def->tip);
		break;
	case AURKEY_SPRITE:
		row.text = new CSboTextField(parent, "Sprite", "sprites/name.spr");
		row.text->SetMaximumCharCount(63);
		row.text->AddActionSignalTarget(parent);
		row.pick = new CSboBrowserButton(parent, "Pick", "Pick", "PickSprite", CSboBrowserButton::KIND_NORMAL);
		row.pick->AddActionSignalTarget(parent);
		parent->SetToolTip(row.text, def->tip);
		parent->SetToolTip(row.pick, "Choose a sprite from a list with pictures");
		break;
	default:
		break;
	}
}

//! Shows the key's value of block in the row
void ShowRow(KeyRow &row, int block)
{
	const AuroraKeyDef *def = row.def;
	const std::string *v = s_Doc.Get(block, def->key);
	if (row.slider)
	{
		float a = def->hasDefault ? def->def : def->uiMin, b = a;
		bool isRange = false;
		if (v)
			auroradoc::ParseRange(*v, a, b, isRange);
		// a value past the slider's span widens it (a script's value is never cut by showing it)
		SboValueRange r = Range(def->uiMin, def->uiMax, def->step > 0 ? def->step : 1.0f, def->decimals);
		r.min = std::min(r.min, a);
		r.max = std::max(r.max, a);
		if (def->hasLimit)
		{
			r.min = std::max(r.min, def->limitMin);
			r.max = std::min(r.max, def->limitMax);
		}
		row.slider->SetRange(r);
		row.slider->SetValue(a);
		std::string label = def->label;
		if (!v && !def->hasDefault)
			label += " (not set)";
		row.slider->SetLabel(label.c_str());
		if (row.to)
			row.to->SetValue(isRange ? auroradoc::FormatNumber(b, std::max(def->decimals, 2)).c_str() : "");
	}
	if (row.select)
	{
		row.select->ClearItems();
		row.items.clear();
		if (def->kind == AURKEY_TYPE)
		{
			row.items.push_back("");
			row.select->AddItem("None");
			for (int i = 0; i < s_Doc.TypeCount(); i++)
			{
				const std::string n = s_Doc.TypeName(i);
				if (n.empty())
					continue;
				row.items.push_back(n);
				row.select->AddItem(n.c_str());
			}
		}
		else
		{
			const std::vector<std::string> values = SplitChoices(def->choices), names = SplitChoices(def->choiceLabels);
			for (size_t i = 0; i < values.size(); i++)
			{
				const std::string &c = values[i];
				row.items.push_back(c);
				// the plain name; a value without one shows itself
				std::string shown = i < names.size() && !names[i].empty() ? names[i] : (c.empty() ? "Not set" : c);
				if (def->defText && c == def->defText)
					shown += " (default)";
				row.select->AddItem(shown.c_str());
			}
		}
		std::string cur = v ? *v : (def->defText ? def->defText : "");
		int sel = 0;
		bool found = false;
		for (int i = 0; i < (int)row.items.size(); i++)
			if (!Q_stricmp(row.items[i].c_str(), cur.c_str()))
			{
				sel = i;
				found = true;
				break;
			}
		if (!found && !cur.empty())
		{
			// a value the list does not have (a type not defined, a misspelled choice): shown as it is
			row.items.push_back(cur);
			row.select->AddItem((cur + " (unknown)").c_str());
			sel = (int)row.items.size() - 1;
		}
		row.select->SetSelected(sel);
	}
	if (row.text)
		row.text->SetValue(v ? v->c_str() : "");
}

//-----------------------------------------------------------------------------
// A window of one particle type
//-----------------------------------------------------------------------------
class CPeTypeWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CPeTypeWindow, CSboToolWindow);

public:
	CPeTypeWindow(int index)
	    : BaseClass(g_pViewport, "PeType", "Particle Type", "pe_type", 380, 470, 0.4f, 0.1f)
	    , m_iIndex(index)
	{
		m_pName = new CSboTextField(this, "Name", "Type name");
		m_pName->SetMaximumCharCount(AURORA_KEYS_MAX_TYPE_NAME);
		m_pName->AddActionSignalTarget(this);
		m_pGroup = new CSboBrowserSelect(this, "Group");
		for (int g = 0; g < AURORA_KEY_GROUP_COUNT; g++)
			m_pGroup->AddItem(AURORA_KEY_GROUPS[g].label);
		m_pGroup->AddActionSignalTarget(this);
		SetToolTip(m_pName, "The type's name, which other types use to spray or draw it");
		SetToolTip(m_pGroup, "Choose which settings to show");
		for (int i = 0; i < AURORA_TYPE_KEY_COUNT; i++)
		{
			if (AURORA_TYPE_KEYS[i].kind == AURKEY_NAME)
				continue;
			KeyRow row;
			MakeRow(this, row, &AURORA_TYPE_KEYS[i]);
			m_Rows.push_back(row);
		}
		// the Color group: a swatch next to each color's red, green, blue and alpha rows opens the color wheel
		static const char *const titles[2] = { "Start color", "End color" };
		static const char *const tips[2] = { "Click to choose the color a particle has when it appears",
			"Click to choose the color a particle has when it disappears" };
		for (int i = 0; i < 2; i++)
		{
			m_pSwatch[i] = new CSboColorSwatch(this, "Color", titles[i]);
			m_pSwatch[i]->SetHasAlpha(true);
			m_pSwatch[i]->AddActionSignalTarget(this);
			SetToolTip(m_pSwatch[i], tips[i]);
		}
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 100);
	}

	int Index() const { return m_iIndex; }
	void SetIndex(int index) { m_iIndex = index; }

	void Refresh()
	{
		if (m_iIndex < 0 || m_iIndex >= s_Doc.TypeCount())
			return;
		const std::string name = s_Doc.TypeName(m_iIndex);
		m_pName->SetValue(name.c_str());
		SetToolTitle(name.empty() ? "Particle Type" : name.c_str());
		for (KeyRow &row : m_Rows)
			ShowRow(row, m_iIndex);
		// the swatches show the values the rows show (a range: its first number); not while the wheel itself sets them
		if (!m_bColorEdit)
			for (int i = 0; i < 2; i++)
			{
				float c[4];
				for (int k = 0; k < 4; k++)
					c[k] = ColorValue(COLOR_KEYS[i][k]);
				m_pSwatch[i]->SetColor((int)(c[0] * 255.0f + 0.5f), (int)(c[1] * 255.0f + 0.5f), (int)(c[2] * 255.0f + 0.5f));
				m_pSwatch[i]->SetAlpha(c[3]);
			}
		ShowGroup();
	}

	MESSAGE_FUNC_PTR(OnColorPickerOpened, "ColorPickerOpened", panel)
	{
		// what Esc puts back: the four keys as they are now
		const int i = SwatchIndex(panel);
		if (i < 0)
			return;
		m_ColorUndo.swatch = i;
		m_ColorUndo.text = s_Doc.Text();
		m_ColorUndo.modified = s_bModified;
		for (int k = 0; k < 4; k++)
		{
			const std::string *v = s_Doc.Get(m_iIndex, COLOR_KEYS[i][k]);
			m_ColorUndo.had[k] = v != nullptr;
			m_ColorUndo.value[k] = v ? *v : std::string();
		}
	}
	MESSAGE_FUNC_PARAMS(OnColorChanged, "ColorChanged", data)
	{
		const int i = SwatchIndex((vgui2::Panel *)data->GetPtr("panel"));
		if (i < 0 || m_iIndex < 0 || m_iIndex >= s_Doc.TypeCount())
			return;
		if (data->GetInt("cancel"))
		{
			if (m_ColorUndo.swatch != i)
				return;
			for (int k = 0; k < 4; k++)
				if (m_ColorUndo.had[k])
					s_Doc.Set(m_iIndex, COLOR_KEYS[i][k], m_ColorUndo.value[k]);
				else
					s_Doc.Remove(m_iIndex, COLOR_KEYS[i][k]);
			m_ColorUndo.swatch = -1;
			const bool same = s_Doc.Text() == m_ColorUndo.text;
			OnDocChanged(this);
			if (same)
			{
				s_bModified = m_ColorUndo.modified; // nothing changed after all
				UpdateTitle();
			}
			return;
		}
		const float c[4] = { data->GetInt("r") / 255.0f, data->GetInt("g") / 255.0f, data->GetInt("b") / 255.0f,
			data->GetFloat("a", 1.0f) };
		bool changed = false;
		for (int k = 0; k < 4; k++)
		{
			KeyRow *row = RowOfKey(COLOR_KEYS[i][k]);
			if (!row || !row->slider)
				continue;
			const int dec = row->def->decimals;
			if (auroradoc::FormatNumber(c[k], dec) == auroradoc::FormatNumber(ColorValue(COLOR_KEYS[i][k]), dec))
				continue; // a key the wheel did not move stays as it is (or not set)
			// as a drag of the row's slider: its range's second number stays
			std::string value = auroradoc::FormatNumber(c[k], dec);
			char to[32] = "";
			if (row->to)
				row->to->GetText(to, sizeof(to));
			float a, b;
			bool r;
			if (to[0] && auroradoc::ParseRange(to, a, b, r))
				value += ".." + auroradoc::FormatNumber(a, std::max(dec, 2));
			s_Doc.Set(m_iIndex, COLOR_KEYS[i][k], value);
			changed = true;
		}
		if (!changed)
			return;
		m_bColorEdit = true;
		OnDocChanged(this);
		m_bColorEdit = false;
	}

	MESSAGE_FUNC_PARAMS(OnValueChanged, "ValueChanged", data)
	{
		vgui2::Panel *p = (vgui2::Panel *)data->GetPtr("panel");
		KeyRow *row = RowOf(p);
		if (!row || !row->slider)
			return;
		const AuroraKeyDef *def = row->def;
		if (data->GetInt("reset"))
		{
			// a right click: the key goes, the game uses its default
			s_Doc.Remove(m_iIndex, def->key);
			OnDocChanged(this);
			return;
		}
		const float v = data->GetFloat("value");
		std::string value = auroradoc::FormatNumber(v, def->decimals);
		char to[32] = "";
		if (row->to)
			row->to->GetText(to, sizeof(to));
		if (to[0])
		{
			float a, b;
			bool r;
			if (auroradoc::ParseRange(to, a, b, r))
				value += ".." + auroradoc::FormatNumber(a, std::max(def->decimals, 2));
		}
		s_Doc.Set(m_iIndex, def->key, value);
		OnDocChanged(this);
	}
	MESSAGE_FUNC_PARAMS(OnTextEntered, "TextEntered", data)
	{
		vgui2::Panel *p = (vgui2::Panel *)data->GetPtr("panel");
		const std::string text = data->GetString("text");
		if (p == m_pName)
		{
			Rename(text);
			return;
		}
		KeyRow *row = RowOf(p);
		if (!row)
			return;
		const AuroraKeyDef *def = row->def;
		if (p == row->to)
		{
			// the range's maximum: empty is one value
			std::string value = auroradoc::FormatNumber(row->slider->GetValue(), def->decimals);
			float a, b;
			bool r;
			if (!text.empty() && auroradoc::ParseRange(text, a, b, r))
				value += ".." + auroradoc::FormatNumber(a, std::max(def->decimals, 2));
			s_Doc.Set(m_iIndex, def->key, value);
			OnDocChanged(this);
			return;
		}
		if (p == row->text)
		{
			if (text.empty())
				s_Doc.Remove(m_iIndex, def->key);
			else
			{
				SetSprite(text);
				return;
			}
			OnDocChanged(this);
		}
	}
	MESSAGE_FUNC_PTR(OnSelectChanged, "SelectChanged", panel)
	{
		if (panel == m_pGroup)
		{
			ShowGroup();
			return;
		}
		KeyRow *row = RowOf(panel);
		if (!row || !row->select)
			return;
		const int sel = row->select->GetSelected();
		if (sel < 0 || sel >= (int)row->items.size())
			return;
		const std::string &v = row->items[sel];
		const AuroraKeyDef *def = row->def;
		if (v.empty() || (def->defText && v == def->defText && def->kind == AURKEY_ENUM))
			s_Doc.Remove(m_iIndex, def->key);
		else
			s_Doc.Set(m_iIndex, def->key, v);
		OnDocChanged(this);
	}
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "PickSprite"))
		{
			OpenSpritePicker(m_iIndex);
			return;
		}
		BaseClass::OnCommand(command);
	}
	void OnTick() override
	{
		// the hovered row's help
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		int hover = -1;
		for (int i = 0; i < (int)m_Rows.size(); i++)
			if (m_RowY[i] >= 0 && my >= m_RowY[i] && my < m_RowY[i] + m_iRowTall && mx >= 0 && mx < GetWide())
				hover = i;
		if (hover >= 0 && hover != m_iHover)
		{
			m_iHover = hover;
			Repaint();
		}
	}
	void SetSprite(const std::string &name)
	{
		char err[200];
		if (!AuroraEditor_LoadSprite(name.c_str(), err, sizeof(err)))
		{
			m_Status = std::string("Couldn't load the sprite: ") + err;
			Refresh();
			Repaint();
			return;
		}
		s_Doc.Set(m_iIndex, "sprite", name);
		m_Status = name + " is loaded";
		OnDocChanged(this);
	}

protected:
	void OnToolClosed() override
	{
		for (CSboColorSwatch *p : m_pSwatch)
			p->ClosePicker();
	}
	void LayoutContent(int x, int y, int w, int h) override
	{
		const int row = M(22), gap = M(4);
		m_pName->SetBounds(x, y, w, row);
		y += row + gap;
		m_pGroup->SetBounds(x, y, w, row);
		y += row + M(8);
		m_iRowsTop = y;
		m_iContentX = x;
		m_iContentW = w;
		m_iBottom = y + h;
		LayoutRows();
		m_iHelpY = std::max(y, (m_iRowsTop + h) - M(70));
		(void)h;
	}
	void Paint() override
	{
		BaseClass::Paint();
		if (IsCollapsed())
			return;
		const int x = M(10), w = GetWide() - M(20);
		for (int i = 0; i < (int)m_Rows.size(); i++)
			if (m_RowY[i] >= 0 && (m_Rows[i].select || m_Rows[i].text))
				DrawTextMid(S().body, COL_TEXT_DIM, x, m_RowY[i], M(22), Widen(m_Rows[i].def->label), M(100));
		int y = GetTall() - M(66);
		OutlineRect(x, y - M(4), w, 1, COL_BORDER_LIST);
		if (m_iHover >= 0 && m_iHover < (int)m_Rows.size())
		{
			// the setting in plain words: its name in the script, its unit and default, and what it does
			const AuroraKeyDef *def = m_Rows[m_iHover].def;
			std::string head = std::string(def->label) + " (\"" + def->key + "\" in the script";
			if (def->unit)
				head += std::string(", in ") + def->unit;
			head += ")";
			if (def->defText && def->defText[0])
				head += std::string(", default ") + def->defText;
			DrawText(S().small, COL_TEXT, x, y, Widen(head.c_str()), w);
			y += M(15);
			for (const std::string &line : Wrap(def->tip, w, 2))
			{
				DrawText(S().small, COL_TEXT_DIM, x, y, Widen(line.c_str()), w);
				y += M(14);
			}
		}
		else
		{
			DrawText(S().small, COL_TEXT_DIM, x, y, L"Point at a setting to see what it does, or right-click a slider to reset it", w);
			y += M(15);
		}
		if (!m_Status.empty())
			DrawText(S().small, COL_TEXT_DIM, x, GetTall() - M(20), Widen(m_Status.c_str()), w);
	}

private:
	//! The keys of the start and the end color: red, green, blue, alpha
	static constexpr const char *COLOR_KEYS[2][4] = { { "startred", "startgreen", "startblue", "startalpha" },
		{ "endred", "endgreen", "endblue", "endalpha" } };

	KeyRow *RowOf(vgui2::Panel *p)
	{
		for (KeyRow &row : m_Rows)
			if (row.Owns(p))
				return &row;
		return nullptr;
	}
	KeyRow *RowOfKey(const char *key)
	{
		for (KeyRow &row : m_Rows)
			if (!strcmp(row.def->key, key))
				return &row;
		return nullptr;
	}
	int SwatchIndex(vgui2::Panel *p) const
	{
		for (int i = 0; i < 2; i++)
			if (p && p == m_pSwatch[i])
				return i;
		return -1;
	}
	//! A color key's value as its row shows it (a range: its first number; not set: the default), 0..1
	float ColorValue(const char *key)
	{
		KeyRow *row = RowOfKey(key);
		float a = row && row->def->hasDefault ? row->def->def : 1.0f, b = a;
		bool isRange = false;
		if (const std::string *v = s_Doc.Get(m_iIndex, key))
			auroradoc::ParseRange(*v, a, b, isRange);
		return std::max(0.0f, std::min(1.0f, a));
	}
	void ShowGroup()
	{
		m_iHover = -1;
		LayoutRows();
		Repaint();
	}
	void LayoutRows()
	{
		const int g = std::max(0, std::min(AURORA_KEY_GROUP_COUNT - 1, m_pGroup->GetSelected()));
		const char *group = AURORA_KEY_GROUPS[g].id;
		m_RowY.assign(m_Rows.size(), -1);
		m_iRowTall = M(CSboLabeledSlider::DesignTall());
		int y = m_iRowsTop;
		const int x = m_iContentX;
		int w = m_iContentW;
		// the Color group: the start color's red, green, blue and alpha, then the end color's, each block with its
		// swatch right of the number fields
		const bool colors = !strcmp(group, "color");
		std::vector<int> order;
		if (colors)
		{
			for (int c = 0; c < 2; c++)
				for (int k = 0; k < 4; k++)
					for (int i = 0; i < (int)m_Rows.size(); i++)
						if (!strcmp(m_Rows[i].def->key, COLOR_KEYS[c][k]))
							order.push_back(i);
			const int sw = M(30);
			w -= sw + M(6);
			for (int c = 0; c < 2; c++)
			{
				const int top = y + c * (4 * (m_iRowTall + M(2)) + M(8));
				m_pSwatch[c]->SetBounds(x + w + M(6), top, sw, 4 * (m_iRowTall + M(2)) - M(2) - M(14));
			}
		}
		for (int i = 0; i < (int)m_Rows.size(); i++)
			if (std::find(order.begin(), order.end(), i) == order.end())
				order.push_back(i);
		for (CSboColorSwatch *p : m_pSwatch)
			p->SetVisible(colors);
		for (int n = 0; n < (int)order.size(); n++)
		{
			const int i = order[n];
			KeyRow &row = m_Rows[i];
			const bool show = !strcmp(row.def->group, group);
			row.SetVisible(show);
			if (!show)
				continue;
			if (colors && n == 4)
				y += M(8); // between the start and the end color
			m_RowY[i] = y;
			if (row.slider)
			{
				const int toW = row.to ? M(54) : 0;
				row.slider->SetBounds(x, y, w - (toW ? toW + M(6) : 0), m_iRowTall);
				if (row.to)
					row.to->SetBounds(x + w - toW, y, toW, M(20));
				y += m_iRowTall + M(2);
			}
			else if (row.select)
			{
				row.select->SetBounds(x + M(104), y, w - M(104), M(22));
				y += M(28);
			}
			else if (row.text)
			{
				row.text->SetBounds(x + M(104), y, w - M(104) - M(56), M(22));
				row.pick->SetBounds(x + w - M(50), y, M(50), M(22));
				y += M(28);
			}
		}
	}
	std::vector<std::string> Wrap(const char *text, int wide, int maxLines)
	{
		std::vector<std::string> lines;
		std::string cur, word;
		auto flush = [&](bool force) {
			if (!cur.empty() || force)
				lines.push_back(cur);
			cur.clear();
		};
		const std::string s = text ? text : "";
		for (size_t i = 0; i <= s.size(); i++)
		{
			const char c = i < s.size() ? s[i] : ' ';
			if (c != ' ')
			{
				word += c;
				continue;
			}
			if (word.empty())
				continue;
			const std::string test = cur.empty() ? word : cur + " " + word;
			if (TextWide(S().small, Widen(test.c_str())) > wide && !cur.empty())
			{
				flush(false);
				cur = word;
			}
			else
				cur = test;
			word.clear();
		}
		flush(false);
		if ((int)lines.size() > maxLines)
		{
			lines.resize(maxLines);
			lines.back() += " ...";
		}
		return lines;
	}
	void Rename(const std::string &text);

	int m_iIndex;
	CSboTextField *m_pName;
	CSboBrowserSelect *m_pGroup;
	std::vector<KeyRow> m_Rows;
	std::vector<int> m_RowY;
	int m_iRowTall = 0, m_iRowsTop = 0, m_iContentX = 0, m_iContentW = 0, m_iBottom = 0, m_iHelpY = 0;
	int m_iHover = -1;
	std::string m_Status;
	CSboColorSwatch *m_pSwatch[2] = {}; //!< start color, end color
	bool m_bColorEdit = false;          //!< the wheel is setting the keys (the swatch already shows its color)
	struct ColorUndo
	{
		int swatch = -1; //!< the swatch whose window is open
		std::string text;
		bool modified = false;
		bool had[4] = {};
		std::string value[4];
	} m_ColorUndo; //!< the keys as they were when its color window opened (Esc)
};

void CPeTypeWindow::Rename(const std::string &text)
{
	const std::string old = s_Doc.TypeName(m_iIndex);
	std::string name = auroradoc::CleanValue(text);
	if (name.size() > AURORA_KEYS_MAX_TYPE_NAME)
		name.resize(AURORA_KEYS_MAX_TYPE_NAME);
	const int other = s_Doc.FindType(name);
	if (name.empty() || (other >= 0 && other != m_iIndex))
	{
		m_Status = name.empty() ? "A type needs a name" : "Another type has that name";
		m_pName->SetValue(old.c_str());
		Repaint();
		return;
	}
	if (name == old)
		return;
	s_Doc.RenameType(m_iIndex, name);
	m_Status = "Renamed, and the types that used the old name now use the new one";
	OnDocChanged(nullptr);
}

//-----------------------------------------------------------------------------
// The sprites the list hides unless "Show filtered sprites" is on: the HUD, kill feed and editor sprites, which
// particles don't use. One rule a line:
//   FOLDER  every sprite in that folder
//   PREFIX  every sprite whose file name starts so (in any folder)
//   LISTED  every sprite a HUD list file names (sprites/hud.txt, sprites/weapon_*.txt: the third word of a line)
//-----------------------------------------------------------------------------
enum SpriteRuleKind
{
	RULE_FOLDER,
	RULE_PREFIX,
	RULE_LISTED,
};
struct SpriteRule
{
	SpriteRuleKind kind;
	const char *what;
};
const SpriteRule SPRITE_FILTER[] = {
	{ RULE_FOLDER, "sprites/320/" },      // HUD sprites for each screen size
	{ RULE_FOLDER, "sprites/640/" },
	{ RULE_FOLDER, "sprites/1280/" },
	{ RULE_FOLDER, "sprites/2560/" },
	{ RULE_FOLDER, "sprites/editor/" },   // the map editors' entity icons
	{ RULE_PREFIX, "hud_" },              // HUD parts
	{ RULE_PREFIX, "320" },               // HUD sprites named for a screen size (320hud1, 640_pain, 1024top: scopes)
	{ RULE_PREFIX, "640" },
	{ RULE_PREFIX, "1024" },
	{ RULE_PREFIX, "1280" },
	{ RULE_PREFIX, "2560" },
	{ RULE_PREFIX, "d_" },                // kill feed weapon icons
	{ RULE_PREFIX, "killfeed" },          // kill feed icons
	{ RULE_LISTED, "sprites/hud.txt" },   // the HUD's sprites
	{ RULE_LISTED, "sprites/weapon_*.txt" }, // each weapon's HUD sprites
};

//! The sprites the LISTED rules name, read from the files in every game folder
std::vector<std::string> ListedSprites()
{
	std::vector<std::string> out;
	for (const SpriteRule &r : SPRITE_FILTER)
	{
		if (r.kind != RULE_LISTED)
			continue;
		for (const std::string &file : ListFiles({ r.what }))
		{
			std::string text;
			if (!ReadGameFile(file.c_str(), text))
				continue;
			size_t pos = 0;
			while (pos < text.size())
			{
				size_t end = text.find('\n', pos);
				if (end == std::string::npos)
					end = text.size();
				// "name resolution sprite x y w h": the third word
				std::vector<std::string> words;
				std::string word;
				for (size_t i = pos; i <= end; i++)
				{
					const char c = i < end ? text[i] : ' ';
					if (c == ' ' || c == '\t' || c == '\r')
					{
						if (!word.empty())
							words.push_back(word);
						word.clear();
					}
					else
						word += c;
				}
				if (words.size() >= 3 && words[1].find_first_not_of("0123456789") == std::string::npos)
				{
					const std::string spr = Lower("sprites/" + words[2] + ".spr");
					if (std::find(out.begin(), out.end(), spr) == out.end())
						out.push_back(spr);
				}
				pos = end + 1;
			}
		}
	}
	return out;
}

//! The sprite is one the list hides by default
bool IsFilteredSprite(const std::string &path, const std::vector<std::string> &listed)
{
	const size_t slash = path.rfind('/');
	const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
	for (const SpriteRule &r : SPRITE_FILTER)
	{
		const size_t n = strlen(r.what);
		if (r.kind == RULE_FOLDER && path.compare(0, n, r.what) == 0)
			return true;
		if (r.kind == RULE_PREFIX && base.compare(0, n, r.what) == 0)
			return true;
	}
	return std::find(listed.begin(), listed.end(), path) != listed.end();
}

//-----------------------------------------------------------------------------
// The sprite list: the shared picture picker (controls/sbo_picture_picker.h) with every sprite in the game's folders,
// pictures decoded from the .spr (no engine sprite slot; only the sprite picked is loaded), the HUD, kill feed and
// editor sprites filtered out unless "Show filtered sprites" is ticked
//-----------------------------------------------------------------------------
bool SpriteThumb(const std::string &name, int size, std::vector<uint8_t> &rgba, int &w, int &h)
{
	std::string data;
	if (!ReadGameFile(name.c_str(), data, sbo_image::MAX_FILE_BYTES) || data.empty())
		return false;
	return sbo_image::DecodeSprScaled((const uint8_t *)data.data(), data.size(), size, size, rgba, w, h);
}

//-----------------------------------------------------------------------------
// Save: aurora/<name>.aur in the mod folder
//-----------------------------------------------------------------------------
class CPeSaveWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CPeSaveWindow, CSboToolWindow);

public:
	CPeSaveWindow()
	    : BaseClass(g_pViewport, "PeSave", "Save Script", "pe_save", 340, 166, 0.35f, 0.35f)
	{
		m_pName = new CSboTextField(this, "Name", "Name of the script");
		m_pName->SetMaximumCharCount(39);
		m_pName->AddActionSignalTarget(this);
		m_pSave = new CSboBrowserButton(this, "Save", "Save", "DoSave", CSboBrowserButton::KIND_PRIMARY);
		m_pCancel = new CSboBrowserButton(this, "Cancel", "Cancel", "Close", CSboBrowserButton::KIND_NORMAL);
		SetToolTip(m_pName, "The file name, using letters, digits, _ and -");
		SetToolTip(m_pSave, "Write the script to aurora/<name>.aur in the SBO folder");
		SetToolTip(m_pCancel, "Close without saving");
		SetToolSizeable(false);
	}
	void Open() override
	{
		m_Status = "Letters, digits, _ and -";
		m_Hint.clear();
		m_Confirm.clear();
		std::string name = s_File;
		if (name.compare(0, sizeof(SCRIPT_FOLDER) - 1, SCRIPT_FOLDER) == 0)
			name = name.substr(sizeof(SCRIPT_FOLDER) - 1);
		if (name.size() > 4)
			name.resize(name.size() - 4);
		m_pName->SetValue(name.c_str());
		BaseClass::Open();
		m_pName->StartEditing();
	}
	MESSAGE_FUNC_PARAMS(OnTextEntered, "TextEntered", data)
	{
		if (data->GetInt("enter"))
			Save();
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
		if (IsCollapsed())
			return;
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY, Widen(m_Status.c_str()), GetWide() - M(20));
		if (!m_Hint.empty())
			DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY + M(16), Widen(m_Hint.c_str()), GetWide() - M(20));
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

void CPeSaveWindow::Save()
{
	char name[64];
	if (!auroradoc::CleanScriptName(m_pName->GetValue(), name, sizeof(name)))
	{
		m_Status = "Use 1 to 39 letters, digits, _ and -";
		m_Hint.clear();
		Repaint();
		return;
	}
	char path[96];
	snprintf(path, sizeof(path), "%s%s.aur", SCRIPT_FOLDER, name);
	// an existing file is replaced only on a second Save with the same name
	if (m_Confirm != name && g_pFullFileSystem->FileExists(path))
	{
		m_Confirm = name;
		m_Status = std::string(name) + ".aur already exists. Press Save again to replace it";
		m_Hint.clear();
		Repaint();
		return;
	}
	g_pFullFileSystem->CreateDirHierarchy("aurora", "GAMECONFIG");
	FileHandle_t fh = g_pFullFileSystem->Open(path, "wb", "GAMECONFIG");
	bool ok = false;
	const std::string &text = s_Doc.Text();
	if (fh)
	{
		ok = text.empty() || g_pFullFileSystem->Write(text.data(), (int)text.size(), fh) == (int)text.size();
		g_pFullFileSystem->Close(fh);
	}
	m_Confirm.clear();
	char line[200];
	if (!ok)
	{
		snprintf(line, sizeof(line), "Couldn't save %s", path);
		m_Status = line;
		m_Hint.clear();
	}
	else
	{
		s_File = path;
		s_bModified = false;
		snprintf(line, sizeof(line), "Saved %s/%s", sbo::GetGameDirectory(), path);
		m_Status = line;
		snprintf(line, sizeof(line), "To use it, set an env_particle's Particle script to %s", path);
		m_Hint = line;
		char msg[256];
		snprintf(msg, sizeof(msg), "particleeditorui: saved %s/%s (%d types)\n", sbo::GetGameDirectory(), path,
		    s_Doc.TypeCount());
		sbo::DevPrint(msg);
		SendPreview(); // the problems now name the file
		UpdateTitle();
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

void OpenSaveWindow(std::function<void()> after)
{
	if (!s_pSave)
		s_pSave = new CPeSaveWindow();
	s_AfterSave = after;
	s_pSave->Open();
}

//-----------------------------------------------------------------------------
// Load: a new script, or the scripts in aurora/ (the game's, the mod folder's, downloads)
//-----------------------------------------------------------------------------
class CPeLoadWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CPeLoadWindow, CSboToolWindow);

public:
	CPeLoadWindow()
	    : BaseClass(g_pViewport, "PeLoad", "Load Script", "pe_load", 300, 340, 0.45f, 0.2f)
	{
		m_pList = new CSboOrderedList(this, "Scripts");
		m_pList->SetStandardButtons(false);
		CSboBrowserButton *load = m_pList->AddButton("Load", "DoLoad");
		m_pList->AddActionSignalTarget(this);
		SetToolTip(m_pList, "Double-click a script or press Enter to open it");
		SetToolTip(load, "Open the selected script");
	}
	void Open() override
	{
		m_Files = ListFiles({ "aurora/*.aur" });
		m_pList->ClearRows();
		m_pList->AddRow("(new script)", -1, true);
		for (const std::string &f : m_Files)
			m_pList->AddRow(f.c_str() + sizeof(SCRIPT_FOLDER) - 1, -1, true);
		m_Status = "Double-click a script to open it";
		m_Status2.clear();
		BaseClass::Open();
	}
	MESSAGE_FUNC_INT(OnListActivated, "ListActivated", index) { Load(index); }
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
		if (IsCollapsed())
			return;
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

//-----------------------------------------------------------------------------
// The main window
//-----------------------------------------------------------------------------
class CPeMainWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CPeMainWindow, CSboToolWindow);

public:
	CPeMainWindow()
	    : BaseClass(g_pViewport, "PeEditor", "Particle Editor", "pe_main", 330, 600, 0.72f, 0.08f)
	{
		for (int i = 0; i < AURORA_TOP_KEY_COUNT; i++)
		{
			KeyRow row;
			MakeRow(this, row, &AURORA_TOP_KEYS[i]);
			m_Rows.push_back(row);
		}
		m_pPreview = new CSboBrowserCheck(this, "Preview", "Preview");
		m_pPreview->SetChecked(s_bPreviewOn);
		m_pPreview->AddActionSignalTarget(this);
		m_pFront = new CSboBrowserButton(this, "Front", "In front", "PlaceFront", CSboBrowserButton::KIND_NORMAL);
		m_pPlace = new CSboBrowserButton(this, "Place", "Place", "PlaceScreen", CSboBrowserButton::KIND_NORMAL);
		m_pRestart = new CSboBrowserButton(this, "Restart", "Restart", "Restart", CSboBrowserButton::KIND_NORMAL);
		m_pPitch = new CSboLabeledSlider(this, "Pitch", "Pitch", Range(-90, 90, 1, 0));
		m_pPitch->SetColumns(56, 44);
		m_pPitch->SetDefaultValue(0);
		m_pPitch->AddActionSignalTarget(this);
		m_pYaw = new CSboLabeledSlider(this, "Yaw", "Yaw", Range(0, 360, 1, 0));
		m_pYaw->SetColumns(56, 44);
		m_pYaw->SetDefaultValue(0);
		m_pYaw->AddActionSignalTarget(this);
		m_pList = new CSboOrderedList(this, "Types");
		m_pList->SetIconPainter(PaintTypeIcon);
		CSboBrowserButton *load = m_pList->AddButton("Load", "LoadScript");
		CSboBrowserButton *save = m_pList->AddButton("Save", "SaveScript");
		m_pList->AddActionSignalTarget(this);
		ShowMouseLookButton(true);
		SetToolTip(m_pPreview, "Show or hide the effect, like switching an env_particle on and off");
		SetToolTip(m_pFront, "Put the effect in front of you");
		SetToolTip(m_pPlace, "Then click a wall or floor to put the effect there");
		SetToolTip(m_pRestart, "Start the effect over");
		SetToolTip(m_pPitch, "Turn the spray up or down, like an env_particle's pitch");
		SetToolTip(m_pYaw, "Turn the spray around, like an env_particle's yaw");
		SetToolTip(m_pList, "Your script's particle types, double-click one to edit it");
		SetToolTip(m_pList->NewButton(), "Add a particle type and open it");
		SetToolTip(m_pList->DeleteButton(), "Remove the selected type");
		SetToolTip(m_pList->UpButton(), "Move the selected type up in the file");
		SetToolTip(m_pList->DownButton(), "Move the selected type down in the file");
		SetToolTip(load, "Open a script from aurora/, or start a new one");
		SetToolTip(save, "Save the script as a .aur file");
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 100);
	}

	void Refresh()
	{
		for (KeyRow &row : m_Rows)
			ShowRow(row, -1);
		const int sel = m_pList->GetSelected();
		m_pList->ClearRows();
		for (int i = 0; i < s_Doc.TypeCount(); i++)
		{
			const std::string n = s_Doc.TypeName(i);
			m_pList->AddRow(n.empty() ? "(unnamed)" : n.c_str(), i, true);
		}
		m_pList->SetSelected(std::min(sel, s_Doc.TypeCount() - 1));
		m_pPitch->SetValue(s_flPitch);
		m_pYaw->SetValue(s_flYaw);
		m_pPreview->SetChecked(s_bPreviewOn);
		Repaint();
	}

	void OnTick() override
	{
		if (!IsOpen())
			return;
		// no longer allowed (sv_cheats below 2, another server): the editor closes and its preview goes
		if (!SboToolsAllowed())
		{
			CloseEditor();
			return;
		}
		SendPreview(); // again after a map change cleared it (nothing is rebuilt while the text is the same)
		char first[200];
		const int problems = AuroraEditor_Problems(first, sizeof(first));
		const int alive = AuroraEditor_PreviewParticles();
		if (problems != m_iProblems || alive != m_iAlive || m_First != first)
		{
			m_iProblems = problems;
			m_iAlive = alive;
			m_First = first;
			Repaint();
		}
	}

	void CloseEditor()
	{
		if (m_bClosing)
			return;
		m_bClosing = true;
		CloseTypeWindows();
		for (CSboToolWindow *w : { (CSboToolWindow *)s_pSprites, (CSboToolWindow *)s_pSave, (CSboToolWindow *)s_pLoad,
		         (CSboToolWindow *)this })
			if (w && w->IsOpen())
				w->CloseTool();
		if (s_pPicker && s_pPicker->IsPicking())
			s_pPicker->Cancel();
		StopPreview();
		m_bClosing = false;
	}

	MESSAGE_FUNC_PARAMS(OnValueChanged, "ValueChanged", data)
	{
		vgui2::Panel *p = (vgui2::Panel *)data->GetPtr("panel");
		if (p == m_pPitch || p == m_pYaw)
		{
			s_flPitch = m_pPitch->GetValue();
			s_flYaw = m_pYaw->GetValue();
			AuroraEditor_SetTurn(s_flPitch, s_flYaw);
			return;
		}
		for (KeyRow &row : m_Rows)
			if (row.Owns(p) && row.slider)
			{
				if (data->GetInt("reset"))
					s_Doc.Remove(-1, row.def->key);
				else
					s_Doc.Set(-1, row.def->key, auroradoc::FormatNumber(data->GetFloat("value"), row.def->decimals));
				OnDocChanged();
				return;
			}
	}
	MESSAGE_FUNC_PTR(OnSelectChanged, "SelectChanged", panel)
	{
		for (KeyRow &row : m_Rows)
			if (row.Owns(panel) && row.select)
			{
				const int sel = row.select->GetSelected();
				if (sel < 0 || sel >= (int)row.items.size())
					return;
				const std::string &v = row.items[sel];
				if (v.empty() || (row.def->defText && v == row.def->defText && row.def->kind == AURKEY_ENUM))
					s_Doc.Remove(-1, row.def->key);
				else
					s_Doc.Set(-1, row.def->key, v);
				OnDocChanged();
				return;
			}
	}
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel)
	{
		if (panel == m_pPreview)
		{
			s_bPreviewOn = m_pPreview->IsChecked();
			AuroraEditor_SetPreviewOn(s_bPreviewOn);
		}
	}
	MESSAGE_FUNC_INT(OnListActivated, "ListActivated", index) { OpenType(index); }
	MESSAGE_FUNC_INT_INT(OnListMoved, "ListMoved", from, to)
	{
		if (from < 0 || to < 0 || from >= s_Doc.TypeCount() || to >= s_Doc.TypeCount() || from == to)
			return;
		s_Doc.SwapTypes(std::min(from, to));
		// the windows follow their types
		CPeTypeWindow *a = s_TypeWindows.count(from) ? s_TypeWindows[from] : nullptr;
		CPeTypeWindow *b = s_TypeWindows.count(to) ? s_TypeWindows[to] : nullptr;
		s_TypeWindows.erase(from);
		s_TypeWindows.erase(to);
		if (a)
		{
			a->SetIndex(to);
			s_TypeWindows[to] = a;
		}
		if (b)
		{
			b->SetIndex(from);
			s_TypeWindows[from] = b;
		}
		OnDocChanged();
		m_pList->SetSelected(to);
	}
	MESSAGE_FUNC(OnListNew, "ListNew")
	{
		if (s_Doc.TypeCount() >= AURORA_KEYS_MAX_TYPES)
		{
			m_Status = "A script can have up to 64 types";
			Repaint();
			return;
		}
		const int i = s_Doc.AddType(s_Doc.UniqueTypeName("type"));
		// a type that shows something at once: a sprite, a life and a size
		s_Doc.Set(i, "sprite", "sprites/steam1.spr");
		s_Doc.Set(i, "lifetime", "1");
		s_Doc.Set(i, "startsize", "8");
		OnDocChanged();
		m_pList->SetSelected(i);
		OpenType(i);
	}
	MESSAGE_FUNC_INT(OnListDelete, "ListDelete", index)
	{
		if (index < 0 || index >= s_Doc.TypeCount())
			return;
		CloseTypeWindows(); // the indexes after it move
		s_Doc.DeleteType(index);
		OnDocChanged();
	}
	MESSAGE_FUNC_INT_INT(OnScreenPicked, "ScreenPicked", x, y)
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		if (sw > 0 && sh > 0)
			AuroraEditor_PlaceAtScreen((float)x / sw, (float)y / sh);
		m_Status = "The effect is where you clicked";
		Repaint();
	}
	MESSAGE_FUNC(OnScreenPickCancelled, "ScreenPickCancelled")
	{
		m_Status.clear();
		Repaint();
	}
	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "Close"))
		{
			// the close button: unsaved changes are asked about first
			ConfirmDiscard([this]() { CloseEditor(); });
			return;
		}
		if (!Q_stricmp(command, "SaveScript"))
		{
			OpenSaveWindow(nullptr);
			return;
		}
		if (!Q_stricmp(command, "LoadScript"))
		{
			if (!s_pLoad)
				s_pLoad = new CPeLoadWindow();
			s_pLoad->Open();
			return;
		}
		if (!Q_stricmp(command, "PlaceFront"))
		{
			AuroraEditor_PlaceInFront();
			m_Status = "The effect is in front of you";
			Repaint();
			return;
		}
		if (!Q_stricmp(command, "PlaceScreen"))
		{
			if (!s_pPicker)
				s_pPicker = new CSboScreenPicker(g_pViewport);
			s_pPicker->Start(this);
			m_Status = "Click where the effect should go, or press Esc to cancel";
			Repaint();
			return;
		}
		if (!Q_stricmp(command, "Restart"))
		{
			AuroraEditor_Restart();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OpenType(int index)
	{
		if (index < 0 || index >= s_Doc.TypeCount())
			return;
		CPeTypeWindow *&w = s_TypeWindows[index];
		if (!w)
			w = new CPeTypeWindow(index);
		w->Refresh();
		w->Open();
		if (w->IsCollapsed())
			w->SetCollapsed(false); // opening a type always shows it
	}
	void SetStatus(const std::string &s)
	{
		m_Status = s;
		Repaint();
	}

protected:
	//! its close button: the whole editor closes, as with the command
	void OnToolClosed() override { CloseEditor(); }
	void LayoutContent(int x, int y, int w, int h) override
	{
		const int y0 = y, row = M(22), sh = M(CSboLabeledSlider::DesignTall());
		m_iFileY = y;
		y += M(20);
		for (KeyRow &r : m_Rows)
		{
			if (r.slider)
			{
				r.slider->SetBounds(x, y, w, sh);
				y += sh + M(2);
			}
			else if (r.select)
			{
				r.select->SetBounds(x + M(104), y, w - M(104), row);
				m_LabelY[r.def->key] = y;
				y += row + M(6);
			}
		}
		y += M(4);
		m_pPreview->SetBounds(x, y, m_pPreview->GetContentWide(), row);
		const int bw = M(64);
		m_pRestart->SetBounds(x + w - bw, y, bw, row);
		m_pPlace->SetBounds(x + w - 2 * bw - M(4), y, bw, row);
		m_pFront->SetBounds(x + w - 3 * bw - M(8), y, bw, row);
		y += row + M(6);
		m_pPitch->SetBounds(x, y, w, sh);
		y += sh + M(2);
		m_pYaw->SetBounds(x, y, w, sh);
		y += sh + M(6);
		m_iStatusY = y0 + h - M(48);
		m_pList->SetBounds(x, y, w, std::max(M(100), m_iStatusY - M(6) - y));
	}
	void Paint() override
	{
		BaseClass::Paint();
		if (IsCollapsed())
			return;
		const int x = M(10), w = GetWide() - M(20);
		std::string file = s_File.empty() ? "New script" : s_File;
		if (s_bModified)
			file += " *";
		DrawText(S().body, COL_TEXT, x, m_iFileY, Widen(file.c_str()), w);
		for (const KeyRow &r : m_Rows)
			if (r.select)
				DrawTextMid(S().body, COL_TEXT_DIM, x, m_LabelY[r.def->key], M(22), Widen(r.def->label), M(100));
		char line[300];
		if (m_iProblems == 0)
			snprintf(line, sizeof(line), "No problems, %d particles on screen", m_iAlive);
		else
			snprintf(line, sizeof(line), "%d problem%s: %s", m_iProblems, m_iProblems == 1 ? "" : "s", m_First.c_str());
		DrawText(S().small, m_iProblems ? COL_PING_MID : COL_TEXT_DIM, x, m_iStatusY, Widen(line), w);
		if (!m_Status.empty())
			DrawText(S().small, COL_TEXT_DIM, x, m_iStatusY + M(16), Widen(m_Status.c_str()), w);
	}

private:
	std::vector<KeyRow> m_Rows;
	std::map<std::string, int> m_LabelY;
	bool m_bClosing = false;
	CSboBrowserCheck *m_pPreview;
	CSboBrowserButton *m_pFront, *m_pPlace, *m_pRestart;
	CSboLabeledSlider *m_pPitch, *m_pYaw;
	CSboOrderedList *m_pList;
	int m_iFileY = 0, m_iStatusY = 0;
	int m_iProblems = -1, m_iAlive = -1;
	std::string m_First, m_Status;
};

void CPeLoadWindow::Load(int index)
{
	if (index < 0 || index > (int)m_Files.size() || !s_pMain)
		return;
	// changes that are not saved: the question first (Save, Don't save, Cancel)
	ConfirmDiscard([this, index]() { DoLoad(index); });
}

void CPeLoadWindow::DoLoad(int index)
{
	if (index < 0 || index > (int)m_Files.size() || !s_pMain)
		return;
	if (index == 0)
	{
		s_Doc.SetText(auroradoc::NewScriptText());
		s_File.clear();
		m_Status = "A new script";
		m_Status2.clear();
	}
	else
	{
		const std::string path = m_Files[index - 1];
		std::string text;
		if (!ReadGameFile(path.c_str(), text))
		{
			m_Status = "Couldn't read " + path;
			m_Status2.clear();
			Repaint();
			return;
		}
		s_Doc.SetText(text);
		s_File = path;
		char line[200];
		snprintf(line, sizeof(line), "Loaded %s (%d types)", path.c_str(), s_Doc.TypeCount());
		m_Status = line;
		m_Status2 = "The main window shows any problems it has";
		char msg[256];
		snprintf(msg, sizeof(msg), "particleeditorui: loaded %s (%d types)\n", path.c_str(), s_Doc.TypeCount());
		sbo::DevPrint(msg);
	}
	CloseTypeWindows();
	OnDocChanged();
	s_bModified = false;
	UpdateTitle();
	s_pMain->Refresh();
	AuroraEditor_Restart();
	CloseTool(); // loaded: the list goes
}

void CloseTypeWindows()
{
	for (auto &it : s_TypeWindows)
	{
		it.second->CloseTool();
		it.second->MarkForDeletion();
	}
	s_TypeWindows.clear();
}

void RefreshAll(CPeTypeWindow *except)
{
	if (s_pMain)
		s_pMain->Refresh();
	for (auto &it : s_TypeWindows)
		if (it.second != except && it.second->IsOpen())
			it.second->Refresh();
	if (except)
		except->Refresh();
}

void OnDocChanged(CPeTypeWindow *source)
{
	s_bModified = true;
	SendPreview();
	RefreshAll(source);
	UpdateTitle();
}

void UpdateTitle()
{
	if (s_pMain)
		s_pMain->SetToolTitle(("Particle Editor - " + ShortName() + (s_bModified ? " *" : "")).c_str());
}

void OpenSpritePicker(int typeIndex)
{
	if (!s_pSprites)
	{
		s_pSprites = new CSboPicturePicker(g_pViewport, "PeSprites", "Sprites", "pe_sprites", "sprites");
		s_pSprites->SetThumbMaker(SpriteThumb); // black cells, as additive sprites show in the game
		s_pSprites->SetTips("Type part of a name to find a sprite", "Double-click a sprite or press Enter to use it",
		    "Use the selected sprite");
		s_pSprites->SetFilteredToggle("Show filtered sprites",
		    "Shows the HUD, kill feed and editor sprites, which particles don't use");
	}
	if (!s_pSprites->HasFiles())
	{
		std::vector<std::string> patterns = { "sprites/*.spr" };
		for (const std::string &f : ListFolders("sprites/"))
			patterns.push_back("sprites/" + f + "/*.spr");
		const std::vector<std::string> all = ListFiles(patterns);
		const std::vector<std::string> listed = ListedSprites();
		std::vector<bool> filtered;
		for (const std::string &f : all)
			filtered.push_back(IsFilteredSprite(f, listed));
		s_pSprites->SetFiles(all, filtered);
		char msg[96];
		snprintf(msg, sizeof(msg), "particleeditorui: %d sprites in the game's folders\n", (int)all.size());
		sbo::DevPrint(msg);
	}
	const std::string name = s_Doc.TypeName(typeIndex);
	s_pSprites->OpenFor("Choosing a sprite for " + (name.empty() ? std::string("a type without a name") : name),
	    [typeIndex](const std::string &sprite) -> std::string {
		    if (typeIndex < 0 || typeIndex >= s_Doc.TypeCount())
			    return "That type was deleted";
		    char err[200];
		    if (!AuroraEditor_LoadSprite(sprite.c_str(), err, sizeof(err)))
			    return std::string("Couldn't load it: ") + err;
		    s_Doc.Set(typeIndex, "sprite", sprite);
		    OnDocChanged();
		    return "";
	    });
}

//-----------------------------------------------------------------------------
// particleeditorui
//-----------------------------------------------------------------------------
void Cmd_ParticleEditorUI()
{
	if (!SboToolsAllowed())
	{
		sbo::ConsolePrint("The particle editor only works in single player, in a game you host, or on a server with sv_cheats 2\n");
		return;
	}
	if (!s_pMain)
		s_pMain = new CPeMainWindow();
	if (s_pMain->IsOpen())
	{
		// the editor's windows close together, and its preview goes (unsaved changes are asked about first)
		ConfirmDiscard([]() {
			if (s_pMain)
				s_pMain->CloseEditor();
		});
		return;
	}
	if (!s_bStarted)
	{
		s_Doc.SetText(auroradoc::NewScriptText());
		s_bStarted = true;
	}
	AuroraEditor_PlaceInFront();
	SendPreview();
	UpdateTitle();
	s_pMain->Refresh();
	s_pMain->Open();
}
} // namespace

bool VGui2_ParticleEditor_Init()
{
	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;
	sbo::AddCommand("particleeditorui", &Cmd_ParticleEditorUI);
	return true;
}
