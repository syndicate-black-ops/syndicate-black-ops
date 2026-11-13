//========= SBO VGUI2: the world screen editor =========//
//
// Purpose: see sbo_worldscreen_editor.h. The layout's text is a wslayout::Doc (worldscreen/worldscreen_layout_doc.h):
//          every change rewrites only what it touches, so comments and keys the editor does not show stay. The edit
//          view is the layout built by the SDK's VGUI2 from that text (EditablePanel::LoadControlSettings with the
//          text's KeyValues, as the game builds a screen from its file), at its real pixel size; selecting, moving and
//          resizing are a layer of our own over it (CWseCanvas). The screens in the world that preview it are built
//          from the same text (worldscreen::EditorPreview).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>

#include "../worldscreen/worldscreen.h"
#include "../worldscreen/worldscreen_layout_doc.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "controls/sbo_list_controls.h"
#include "controls/sbo_picture_picker.h"
#include "controls/sbo_tool_window.h"
#include "controls/sbo_value_controls.h"
#include "sbo_bridge.h"
#include "sbo_image_decode.h" // the picture previews
#include "sbo_server_browser.h" // CSboBrowserButton, CSboBrowserCheck, CSboBrowserSelect
#include "sbo_ui_style.h"
#include "sbo_worldscreen_editor.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
const char LAYOUT_DIR[] = VGUI2_ROOT_DIR "scripts/screens/";
const char SCHEME_FILE[] = VGUI2_ROOT_DIR "resource/worldscreenscheme.res";
constexpr int MAX_UNDO = 64;
constexpr int MAX_LIST_FILES = 512;
constexpr int MAX_CONTROL_NAME = 31;   // a vgui_control_proxy's Control Name (WSCREEN control names)
constexpr int HANDLE = 7;              // the resize handles, pixels
constexpr double PREVIEW_INTERVAL = 0.1; // the world screens are built again at most this often while you edit

class CWseMainWindow;
class CWsePropsWindow;
class CWseLoadWindow;
class CWseSaveWindow;
class CWseLayersWindow;

wslayout::Doc s_Doc;
std::string s_Name;       // the layout's name (its file ui/scripts/screens/<name>.res); "" = a new layout
bool s_bStarted = false;
bool s_bShowHidden = true; // the edit view shows controls hidden at the start
bool s_bModified = false;   // the text differs from the file as loaded or saved (UpdateTitle keeps it)
std::string s_SavedText;
std::vector<std::string> s_Undo; // the text before each change
std::string s_Selected;   // the selected section's name ("" = none)
std::function<void()> s_AfterSave;

// the editor's own screen in the world (Place screen): only in this game, placed on a wall the player clicks
bool s_bLocalPlaced = false;
float s_LocalPoint[3] = {}, s_LocalNormal[3] = {}; // the point clicked and the surface's normal
float s_flLocalWide = 128.0f, s_flLocalTall = 64.0f; // its size in map units
std::string LocalKeysText();
void UpdateLocalScreen();

// preview in the world
std::set<int> s_Picked;          // the screens picked (in view / clicked), and those that kept a preview over a Save as
std::set<int> s_Previewed;       // slots that show the editor's text (built from their files again when it stops)
bool s_bPreviewDirty = true;
double s_flNextPreview = 0.0;

CWseMainWindow *s_pMain = nullptr;
CWsePropsWindow *s_pProps = nullptr;
CWseLoadWindow *s_pLoad = nullptr;
CWseSaveWindow *s_pSave = nullptr;
CWseLayersWindow *s_pLayers = nullptr;
void RefreshLayers();
void OpenLayers();
CSboScreenPicker *s_pPicker = nullptr;

//-----------------------------------------------------------------------------
// The controls the editor offers, in plain names
//-----------------------------------------------------------------------------
struct KindDef
{
	const char *cls;   // ControlName (the SDK's VGUI2 class)
	const char *plain; // what the Add list calls it
	const char *noun;  // in a sentence: "Title is a text label"
	const char *tip;
	int wide, tall;    // a new one's size
};
const KindDef KINDS[] = {
	{ "Label", "Text label", "text label", "Words on the screen", 220, 40 },
	{ "Button", "Button", "button", "A button that players click by aiming at it and pressing fire", 200, 56 },
	{ "ImagePanel", "Picture", "picture", "A picture from a .tga file", 128, 128 },
	{ "ProgressBar", "Bar in blocks", "bar in blocks", "A bar drawn in blocks, which the map can fill", 240, 24 },
	{ "ContinuousProgressBar", "Smooth bar", "smooth bar", "A smooth bar, which the map can fill", 240, 24 },
	{ "AnalogBar", "Bar in blocks without a background", "bar in blocks without a background",
	    "A bar drawn in blocks without a background, which the map can fill", 240, 24 },
	{ "ContinuousAnalogBar", "Smooth bar without a background", "smooth bar without a background",
	    "A smooth bar without a background, which the map can fill", 240, 24 },
	{ "Slider", "Slider", "slider", "A slider whose handle the map can move", 240, 32 },
};
constexpr int KIND_COUNT = (int)(sizeof(KINDS) / sizeof(KINDS[0]));

const KindDef *KindOf(const std::string *cls)
{
	if (!cls)
		return nullptr;
	for (const KindDef &k : KINDS)
		if (!Q_stricmp(k.cls, cls->c_str()))
			return &k;
	return nullptr;
}

//! A section's control in a sentence ("a text label", "a button", "a Frame control" for a class the editor doesn't
//! offer)
std::string PlainKind(int sec)
{
	if (sec < 0 || sec >= s_Doc.Count())
		return "";
	if (!Q_stricmp(s_Doc.Sec(sec).name.c_str(), wslayout::SCREEN_SECTION))
		return "the screen";
	const std::string *cls = s_Doc.Get(sec, "ControlName");
	const KindDef *k = KindOf(cls);
	if (!k)
		return "a " + (cls ? *cls + " control" : std::string("text label"));
	return std::string("a ") + k->noun;
}

bool IsScreen(int sec)
{
	return sec >= 0 && sec < s_Doc.Count() && !Q_stricmp(s_Doc.Sec(sec).name.c_str(), wslayout::SCREEN_SECTION);
}

int SelectedIndex()
{
	return s_Selected.empty() ? -1 : s_Doc.Find(s_Selected);
}

int ScreenIndex()
{
	return s_Doc.Find(wslayout::SCREEN_SECTION);
}

//! "1 control", "33 controls": the layout's controls, without the screen's own section
std::string ControlCountText()
{
	const int n = s_Doc.Count() - (ScreenIndex() >= 0 ? 1 : 0);
	return std::to_string(n) + (n == 1 ? " control" : " controls");
}

int IntKey(int sec, const char *key, int def)
{
	const std::string *v = s_Doc.Get(sec, key);
	return v && !v->empty() ? atoi(v->c_str()) : def;
}

std::string ShortName()
{
	return s_Name.empty() ? std::string("new layout") : s_Name + ".res";
}

std::string LayoutPath(const std::string &name)
{
	return std::string(LAYOUT_DIR) + name + ".res";
}

//! The game's search paths a list looks in (SBO's, the downloads, Half-Life's)
const char *const LIST_PATH_IDS[] = { nullptr, "GAME", "GAMEDOWNLOAD", "DEFAULTGAME" };

//! The layouts in ui/scripts/screens/ of every game folder, by name (no .res), once each, sorted
std::vector<std::string> ListLayouts()
{
	std::vector<std::string> names;
	const std::string pattern = std::string(LAYOUT_DIR) + "*.res";
	for (const char *pathID : LIST_PATH_IDS)
	{
		FileFindHandle_t fh;
		const char *fn = g_pFullFileSystem->FindFirst(pattern.c_str(), &fh, pathID);
		while (fn && (int)names.size() < MAX_LIST_FILES)
		{
			char clean[40];
			if (fn[0] && fn[0] != '.' && !g_pFullFileSystem->FindIsDirectory(fh) &&
			    wslayout::CleanLayoutName(fn, clean, sizeof(clean)))
			{
				const std::string s = Lower(clean);
				if (std::find(names.begin(), names.end(), s) == names.end())
					names.push_back(s);
			}
			fn = g_pFullFileSystem->FindNext(fh);
		}
		g_pFullFileSystem->FindClose(fh);
	}
	std::sort(names.begin(), names.end());
	return names;
}

bool ReadGameFile(const char *path, std::string &out)
{
	out.clear();
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
	if (!fh)
		return false;
	const unsigned size = g_pFullFileSystem->Size(fh);
	bool ok = size <= wslayout::MAX_TEXT;
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

//-----------------------------------------------------------------------------
// The pictures a Picture control (ImagePanel) can show: its "image" is a .tga's path from the game folder without the
// extension (vgui2::scheme()->GetImage). The engine loads only truecolor TGAs (type 2 or 10, 24 or 32 bits) of at most
// 512 x 512 pixels in all ("LoadTGA: texture too large (WxH>256x256)" above that, seen on HL25 Linux: 1024 x 256 and
// 512 x 512 load, 513 x 512 and 640 x 480 don't; a gray TGA: "Only type 2 and 10 targa RGB images supported").
//-----------------------------------------------------------------------------
constexpr int MAX_PICTURE_PIXELS = 512 * 512;
constexpr int MAX_PICTURE_FILES = 4000;
constexpr int MAX_PICTURE_FOLDERS = 2000;
constexpr int MAX_PICTURE_DEPTH = 5;

//! Folders that hold no pictures for a screen (maps, sounds, models...): not searched
bool SkipFolder(const std::string &lowerName)
{
	static const char *const SKIP[] = { "maps", "models", "sound", "sprites", "save", "logs", "addons", "dlls",
		"cl_dlls", "media", "video", "aurora", "events", "bin", "platform", "downloads" };
	for (const char *n : SKIP)
		if (lowerName == n)
			return true;
	return false;
}

//! A game's own folder seen from the Half-Life folder (some search paths start there): its files are found under
//! their own search path already, and a path through it is not what the game reads
bool IsGameFolder(const std::string &lowerName)
{
	const size_t us = lowerName.rfind('_');
	if (us != std::string::npos)
	{
		const std::string tail = lowerName.substr(us);
		if (tail == "_downloads" || tail == "_addon" || tail == "_hd" || tail == "_lv")
			return true;
	}
	return g_pFullFileSystem->FileExists((lowerName + "/liblist.gam").c_str());
}

bool PictureLoads(const std::string &file)
{
	FileHandle_t fh = g_pFullFileSystem->Open(file.c_str(), "rb");
	if (!fh)
		return false;
	uint8_t head[18];
	const bool read = g_pFullFileSystem->Read(head, sizeof(head), fh) == (int)sizeof(head);
	g_pFullFileSystem->Close(fh);
	int w = 0, h = 0, bits = 0, type = 0;
	return read && sbo_image::ReadTgaHeader(head, sizeof(head), w, h, bits, type) && w * h <= MAX_PICTURE_PIXELS;
}

//! Every picture a Picture control can show, as its "image" value (path without .tga), once each, sorted. The game
//! folders are searched a few at a time, between frames (Step), so the game doesn't stop while the list is made.
struct PictureScan
{
	std::vector<std::string> folders = { "" };
	size_t next = 0;
	std::vector<std::string> found;
	bool done = false;

	//! Searches for up to `seconds`; true once the whole list is made (sorted)
	bool Step(double seconds)
	{
		const double until = vgui2::system()->GetCurrentTime() + seconds;
		while (!done && vgui2::system()->GetCurrentTime() < until)
		{
			if (next >= folders.size() || (int)next >= MAX_PICTURE_FOLDERS || (int)found.size() >= MAX_PICTURE_FILES)
			{
				std::sort(found.begin(), found.end());
				done = true;
				break;
			}
			ScanFolder(folders[next++]);
		}
		return done;
	}

private:
	void ScanFolder(const std::string dir) // a copy: folders grows while it runs
	{
		const int depth = (int)std::count(dir.begin(), dir.end(), '/');
		for (const char *pathID : LIST_PATH_IDS)
		{
			FileFindHandle_t fh;
			const std::string pattern = dir + "*";
			const char *fn = g_pFullFileSystem->FindFirst(pattern.c_str(), &fh, pathID);
			while (fn && (int)found.size() < MAX_PICTURE_FILES)
			{
				const std::string name = Lower(fn);
				if (!name.empty() && name[0] != '.')
				{
					if (g_pFullFileSystem->FindIsDirectory(fh))
					{
						const std::string sub = dir + name + "/";
						if (depth < MAX_PICTURE_DEPTH && !SkipFolder(name) && !(dir.empty() && IsGameFolder(name)) &&
						    std::find(folders.begin(), folders.end(), sub) == folders.end())
							folders.push_back(sub);
					}
					else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tga") == 0)
					{
						const std::string image = dir + name.substr(0, name.size() - 4);
						if (std::find(found.begin(), found.end(), image) == found.end() && PictureLoads(dir + name))
							found.push_back(image);
					}
				}
				fn = g_pFullFileSystem->FindNext(fh);
			}
			g_pFullFileSystem->FindClose(fh);
		}
	}
};
PictureScan *s_pPictureScan = nullptr; // the list being made (nullptr: not started, or made)

//! A picture's preview for the list, from the file (sbo_image, never an engine texture slot)
bool PictureThumb(const std::string &image, int size, std::vector<uint8_t> &rgba, int &w, int &h)
{
	FileHandle_t fh = g_pFullFileSystem->Open((image + ".tga").c_str(), "rb");
	if (!fh)
		return false;
	const unsigned bytes = g_pFullFileSystem->Size(fh);
	std::vector<uint8_t> data;
	bool ok = bytes > 0 && bytes <= sbo_image::MAX_FILE_BYTES;
	if (ok)
	{
		data.resize(bytes);
		ok = g_pFullFileSystem->Read(data.data(), (int)bytes, fh) == (int)bytes;
	}
	g_pFullFileSystem->Close(fh);
	return ok && sbo_image::DecodeTgaScaled(data.data(), data.size(), size, size, rgba, w, h);
}

CSboPicturePicker *s_pPictures = nullptr;

void DisableMouseInput(vgui2::VPANEL panel)
{
	vgui2::ipanel()->SetMouseInputEnabled(panel, false);
	for (int i = 0; i < vgui2::ipanel()->GetChildCount(panel); i++)
		DisableMouseInput(vgui2::ipanel()->GetChild(panel, i));
}

void UpdateTitle();
void OnDocChanged(bool rebuild = true);
void OpenSaveWindow(std::function<void()> after);
void RefreshProps();

//! Before a change: what Undo puts back
void PushUndo(const std::string &before)
{
	if (!s_Undo.empty() && s_Undo.back() == before)
		return;
	s_Undo.push_back(before);
	if ((int)s_Undo.size() > MAX_UNDO)
		s_Undo.erase(s_Undo.begin());
}

//! Unsaved changes: asks "Save changes to <name>?" (Save, Don't save, Cancel), then goes on unless cancelled
void ConfirmDiscard(std::function<void()> next)
{
	if (s_Doc.Text() == s_SavedText)
	{
		next();
		return;
	}
	const std::string q = "Save changes to " + ShortName() + "?";
	CSboChoiceDialog::Get()->Ask("Unsaved changes", q.c_str(), { "Save", "Don't save", "Cancel" }, [next](int choice) {
		if (choice == 0)
			OpenSaveWindow(next);
		else if (choice == 1)
			next();
	});
}

//-----------------------------------------------------------------------------
// Preview in the world: the screen picked, and every screen that shows the layout being edited
//-----------------------------------------------------------------------------
std::vector<int> PreviewSlots()
{
	std::vector<int> out;
	for (int i = 0; i < WORLDSCREEN_MAX; i++)
	{
		const char *panel = worldscreen::EditorSlotPanel(i);
		if (!panel[0])
			continue;
		if (i == worldscreen::EditorLocalSlot() || s_Picked.count(i) || (!s_Name.empty() && !Q_stricmp(panel, s_Name.c_str())))
			out.push_back(i);
	}
	return out;
}

//! Builds the previewing screens again from the text (at most every PREVIEW_INTERVAL while you edit)
void FlushPreview(bool now = false)
{
	const double t = vgui2::system()->GetCurrentTime();
	if (!s_bPreviewDirty || (!now && t < s_flNextPreview))
		return;
	s_bPreviewDirty = false;
	s_flNextPreview = t + PREVIEW_INTERVAL;
	const std::vector<int> slots = PreviewSlots();
	// a screen that previewed and no longer should shows its own file again
	for (auto it = s_Previewed.begin(); it != s_Previewed.end();)
	{
		if (std::find(slots.begin(), slots.end(), *it) == slots.end())
		{
			worldscreen::EditorPreview(*it, nullptr);
			it = s_Previewed.erase(it);
		}
		else
			++it;
	}
	for (int slot : slots)
		if (worldscreen::EditorPreview(slot, s_Doc.Text().c_str()))
			s_Previewed.insert(slot);
}

void StopPreview()
{
	for (int slot : s_Previewed)
		worldscreen::EditorPreview(slot, nullptr);
	s_Previewed.clear();
	s_Picked.clear();
	worldscreen::EditorRemoveLocal();
	s_bLocalPlaced = false;
	s_bPreviewDirty = true;
}

//-----------------------------------------------------------------------------
// The layout as the game builds it
//-----------------------------------------------------------------------------
class CWseScreen : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CWseScreen, vgui2::EditablePanel);

public:
	CWseScreen(vgui2::Panel *parent) : BaseClass(parent, "WorldScreen")
	{
		SetProportional(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		const vgui2::HScheme h = vgui2::scheme()->LoadSchemeFromFile(SCHEME_FILE, "WorldScreenScheme");
		if (h)
			SetScheme(h);
	}
	// a button's command does nothing here
	void OnCommand(const char *) override {}
};

//-----------------------------------------------------------------------------
// The edit view: the layout at its real size, and the layer that selects, moves and resizes its controls
//-----------------------------------------------------------------------------
class CWseCanvas : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CWseCanvas, vgui2::Panel);

public:
	enum Drag
	{
		DRAG_NONE,
		DRAG_MOVE,
		DRAG_RESIZE,
	};

	CWseCanvas(vgui2::Panel *parent) : BaseClass(parent, "WseCanvas")
	{
		SetPaintBackgroundEnabled(true);
		SetPostChildPaintEnabled(true);
		m_pScreen = new CWseScreen(this);
	}

	//! Builds the view from the text again
	void Rebuild()
	{
		KeyValues *kv = new KeyValues("layout");
		const std::string path = LayoutPath(s_Name.empty() ? "new" : s_Name);
		if (!kv->LoadFromBuffer(path.c_str(), s_Doc.Text().c_str()) || !kv->GetFirstSubKey())
		{
			kv->deleteThis();
			kv = new KeyValues("layout"); // nothing to show
		}
		m_pScreen->LoadControlSettings(path.c_str(), nullptr, kv);
		kv->deleteThis();
		// hidden at the start ("visible" 0): shown here, with a dashed outline
		m_Hidden.clear();
		for (int i = 0; i < m_pScreen->GetChildCount(); i++)
		{
			vgui2::Panel *c = m_pScreen->GetChild(i);
			if (c && !c->IsVisible())
			{
				m_Hidden.insert(c->GetName());
				c->SetVisible(s_bShowHidden);
			}
		}
		DisableMouseInput(m_pScreen->GetVPanel());
		int w = 0, t = 0;
		m_pScreen->GetSize(w, t);
		w = std::max(16, std::min(w, WORLDSCREEN_MAX_PIXELS));
		t = std::max(16, std::min(t, WORLDSCREEN_MAX_PIXELS));
		m_pScreen->SetBounds(0, 0, w, t);
		if (w != GetWide() || t != GetTall())
		{
			SetSize(w, t);
			if (GetParent())
				GetParent()->InvalidateLayout();
		}
		Repaint();
	}

	vgui2::Panel *ControlPanel(int sec)
	{
		if (sec < 0 || sec >= s_Doc.Count() || IsScreen(sec))
			return nullptr;
		return m_pScreen->FindChildByName(s_Doc.Sec(sec).name.c_str());
	}

	//! The topmost control under (x, y), else -1
	int ControlAt(int x, int y)
	{
		for (int i = s_Doc.Count() - 1; i >= 0; i--)
		{
			vgui2::Panel *p = ControlPanel(i);
			if (!p || !p->IsVisible())
				continue;
			int cx, cy, cw, ct;
			p->GetBounds(cx, cy, cw, ct);
			if (x >= cx && y >= cy && x < cx + cw && y < cy + ct)
				return i;
		}
		return -1;
	}

	bool IsDragging() const { return m_eDrag != DRAG_NONE; }
	vgui2::Panel *ScreenPanel() { return m_pScreen; }

	//! Esc while dragging: the control goes back to where it was
	void CancelDrag()
	{
		if (m_eDrag == DRAG_NONE)
			return;
		m_eDrag = DRAG_NONE;
		vgui2::input()->SetMouseCapture(0);
		s_Doc.SetText(m_DragStart);
		s_bModified = m_bDragStartModified;
		EndDragMode();
		OnDocChanged();
		UpdateTitle();
	}

	void PaintBackground() override
	{
		FillRect(0, 0, GetWide(), GetTall(), Color(0, 0, 0, 255));
	}

	void PostChildPaint() override
	{
		const int sel = SelectedIndex();
		// every control's outline, faint, so empty labels and bars can be found
		for (int i = 0; i < s_Doc.Count(); i++)
		{
			vgui2::Panel *p = ControlPanel(i);
			if (!p || !p->IsVisible())
				continue;
			int x, y, w, t;
			p->GetBounds(x, y, w, t);
			if (m_Hidden.count(p->GetName()))
				DashedRect(x, y, w, t, Color(240, 160, 122, 200));
			else if (i != sel)
				OutlineRect(x, y, w, t, Color(95, 174, 69, 70));
		}
		if (sel < 0)
			return;
		if (IsScreen(sel))
		{
			OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_PRIMARY);
			OutlineRect(1, 1, GetWide() - 2, GetTall() - 2, COL_BORDER_PRIMARY);
			return;
		}
		vgui2::Panel *p = ControlPanel(sel);
		if (!p)
			return;
		int x, y, w, t;
		p->GetBounds(x, y, w, t);
		OutlineRect(x - 1, y - 1, w + 2, t + 2, COL_BORDER_PRIMARY);
		OutlineRect(x, y, w, t, Color(0, 0, 0, 160));
		for (int h = 0; h < 8; h++)
		{
			int hx, hy;
			HandlePos(h, x, y, w, t, hx, hy);
			FillRect(hx, hy, HANDLE, HANDLE, COL_TEXT_BRIGHT);
			OutlineRect(hx, hy, HANDLE, HANDLE, Color(0, 0, 0, 255));
		}
	}

	void OnMousePressed(vgui2::MouseCode code) override
	{
		if (code != vgui2::MOUSE_LEFT || m_eDrag != DRAG_NONE)
			return;
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		int sel = SelectedIndex();
		int handle = -1;
		if (vgui2::Panel *p = ControlPanel(sel))
		{
			int cx, cy, cw, ct;
			p->GetBounds(cx, cy, cw, ct);
			handle = HandleAt(x, y, cx, cy, cw, ct);
		}
		if (handle < 0)
		{
			const int hit = ControlAt(x, y);
			sel = hit >= 0 ? hit : ScreenIndex();
			s_Selected = sel >= 0 ? s_Doc.Sec(sel).name : std::string();
			RefreshProps();
			Repaint();
			if (hit < 0)
				return; // the screen itself: selected, not moved
		}
		vgui2::Panel *p = ControlPanel(sel);
		if (!p)
			return;
		p->GetBounds(m_iStartX, m_iStartY, m_iStartW, m_iStartT);
		m_iPressX = x;
		m_iPressY = y;
		m_iHandle = handle;
		m_eDrag = handle >= 0 ? DRAG_RESIZE : DRAG_MOVE;
		m_DragStart = s_Doc.Text();
		m_bDragStartModified = s_bModified;
		m_bMoved = false;
		vgui2::input()->SetMouseCapture(GetVPanel());
		BeginDragMode();
	}

	void OnCursorMoved(int, int) override
	{
		if (m_eDrag == DRAG_NONE)
			return;
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		const int dx = x - m_iPressX, dy = y - m_iPressY;
		if (!m_bMoved && abs(dx) < 2 && abs(dy) < 2)
			return; // a click, not a drag yet
		m_bMoved = true;
		int nx = m_iStartX, ny = m_iStartY, nw = m_iStartW, nt = m_iStartT;
		const int W = GetWide(), T = GetTall();
		if (m_eDrag == DRAG_MOVE)
		{
			nx = std::max(-(nw - 4), std::min(W - 4, m_iStartX + dx));
			ny = std::max(-(nt - 4), std::min(T - 4, m_iStartY + dy));
		}
		else
		{
			// handles: 0 top-left, 1 top, 2 top-right, 3 right, 4 bottom-right, 5 bottom, 6 bottom-left, 7 left
			const bool left = m_iHandle == 0 || m_iHandle == 6 || m_iHandle == 7;
			const bool right = m_iHandle == 2 || m_iHandle == 3 || m_iHandle == 4;
			const bool top = m_iHandle == 0 || m_iHandle == 1 || m_iHandle == 2;
			const bool bottom = m_iHandle == 4 || m_iHandle == 5 || m_iHandle == 6;
			if (left)
			{
				nx = std::min(m_iStartX + dx, m_iStartX + m_iStartW - 4);
				nw = m_iStartX + m_iStartW - nx;
			}
			if (right)
				nw = std::max(4, m_iStartW + dx);
			if (top)
			{
				ny = std::min(m_iStartY + dy, m_iStartY + m_iStartT - 4);
				nt = m_iStartY + m_iStartT - ny;
			}
			if (bottom)
				nt = std::max(4, m_iStartT + dy);
		}
		const int sec = SelectedIndex();
		vgui2::Panel *p = ControlPanel(sec);
		if (!p)
			return;
		char buf[16];
		const int keys[4] = { nx, ny, nw, nt };
		const char *const names[4] = { "xpos", "ypos", "wide", "tall" };
		const int old[4] = { m_iStartX, m_iStartY, m_iStartW, m_iStartT };
		for (int k = 0; k < 4; k++)
		{
			// a key is written once it differs from where the drag started, or when the layout had it already
			if (keys[k] == old[k] && !s_Doc.Get(sec, names[k]))
				continue;
			snprintf(buf, sizeof(buf), "%d", keys[k]);
			s_Doc.Set(sec, names[k], buf);
		}
		p->SetBounds(nx, ny, nw, nt);
		s_bModified = s_Doc.Text() != m_DragStart || m_bDragStartModified;
		OnDocChanged(false);
		UpdateTitle();
		Repaint();
	}

	void OnMouseReleased(vgui2::MouseCode code) override
	{
		if (code != vgui2::MOUSE_LEFT || m_eDrag == DRAG_NONE)
			return;
		m_eDrag = DRAG_NONE;
		vgui2::input()->SetMouseCapture(0);
		EndDragMode();
		if (s_Doc.Text() != m_DragStart)
		{
			PushUndo(m_DragStart);
			s_bModified = true;
			OnDocChanged(); // labels wrap again at their new width
		}
		UpdateTitle();
	}

	//! The status line: the control under the cursor or the selected one
	std::string Describe(int sec)
	{
		if (sec < 0 || sec >= s_Doc.Count())
			return "";
		char buf[200];
		if (IsScreen(sec))
		{
			snprintf(buf, sizeof(buf), "The screen is %d pixels wide and %d pixels high", GetWide(), GetTall());
			return buf;
		}
		vgui2::Panel *p = ControlPanel(sec);
		int x = 0, y = 0, w = 0, t = 0;
		if (p)
			p->GetBounds(x, y, w, t);
		snprintf(buf, sizeof(buf), "%s is %s, %d pixels wide and %d high, and sits %d pixels from the left and %d from the top",
		    s_Doc.Sec(sec).name.c_str(), PlainKind(sec).c_str(), w, t, x, y);
		return buf;
	}

private:
	static void HandlePos(int h, int x, int y, int w, int t, int &hx, int &hy)
	{
		const int xs[3] = { x - HANDLE / 2 - 1, x + w / 2 - HANDLE / 2, x + w - HANDLE / 2 };
		const int ys[3] = { y - HANDLE / 2 - 1, y + t / 2 - HANDLE / 2, y + t - HANDLE / 2 };
		static const int col[8] = { 0, 1, 2, 2, 2, 1, 0, 0 };
		static const int row[8] = { 0, 0, 0, 1, 2, 2, 2, 1 };
		hx = xs[col[h]];
		hy = ys[row[h]];
	}
	static int HandleAt(int px, int py, int x, int y, int w, int t)
	{
		for (int h = 0; h < 8; h++)
		{
			int hx, hy;
			HandlePos(h, x, y, w, t, hx, hy);
			if (px >= hx - 2 && py >= hy - 2 && px < hx + HANDLE + 2 && py < hy + HANDLE + 2)
				return h;
		}
		return -1;
	}
	static void DashedRect(int x, int y, int w, int t, Color c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int i = 0; i < w; i += 8)
		{
			vgui2::surface()->DrawFilledRect(x + i, y, x + std::min(i + 4, w), y + 1);
			vgui2::surface()->DrawFilledRect(x + i, y + t - 1, x + std::min(i + 4, w), y + t);
		}
		for (int i = 0; i < t; i += 8)
		{
			vgui2::surface()->DrawFilledRect(x, y + i, x + 1, y + std::min(i + 4, t));
			vgui2::surface()->DrawFilledRect(x + w - 1, y + i, x + w, y + std::min(i + 4, t));
		}
	}
	void BeginDragMode();
	void EndDragMode();

	CWseScreen *m_pScreen;
	std::set<std::string> m_Hidden;
	Drag m_eDrag = DRAG_NONE;
	int m_iHandle = -1;
	int m_iPressX = 0, m_iPressY = 0;
	int m_iStartX = 0, m_iStartY = 0, m_iStartW = 0, m_iStartT = 0;
	bool m_bMoved = false;
	std::string m_DragStart;
	bool m_bDragStartModified = false;
};

//-----------------------------------------------------------------------------
// The main window: Load, Save, Undo, Add, Delete, Layers, the preview's screen, the placed screen, and the edit view
//-----------------------------------------------------------------------------
class CWseMainWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CWseMainWindow, CSboToolWindow);

public:
	CWseMainWindow()
	    : BaseClass(g_pViewport, "WseEditor", "World Screen Editor", "wse_main", 560, 400, 0.04f, 0.08f)
	{
		m_pLoad = Button("Load", "Load", "LoadLayout");
		m_pSave = Button("Save", "Save", "SaveLayout");
		m_pUndo = Button("Undo", "Undo", "Undo");
		m_pAdd = new CSboBrowserSelect(this, "Add");
		m_pAdd->AddItem("Add a control");
		for (const KindDef &k : KINDS)
			m_pAdd->AddItem(k.plain);
		m_pAdd->SetSelected(0);
		m_pAdd->AddActionSignalTarget(this);
		m_pDelete = Button("Delete", "Delete", "DeleteControl");
		m_pFront = Button("Layers", "Layers", "ShowLayers");
		m_pInView = Button("InView", "Screen in view", "PickInView");
		m_pPick = Button("Pick", "Pick a screen", "PickScreen");
		m_pHidden = new CSboBrowserCheck(this, "Hidden", "Show hidden controls");
		m_pHidden->SetChecked(s_bShowHidden);
		m_pHidden->AddActionSignalTarget(this);
		m_pPlace = Button("Place", "Place screen", "PlaceScreen");
		m_pLocalW = new CSboNumberEntry(this, "LocalWide");
		m_pLocalT = new CSboNumberEntry(this, "LocalTall");
		for (CSboNumberEntry *n : { m_pLocalW, m_pLocalT })
		{
			SboValueRange r;
			r.min = 4.0f;
			r.max = 4096.0f;
			n->SetRange(r);
			n->AddActionSignalTarget(this);
		}
		m_pLocalW->SetValue(s_flLocalWide);
		m_pLocalT->SetValue(s_flLocalTall);
		m_pKeys = new CSboTextField(this, "Keys", "Place a screen to see its vgui_screen keys here");
		m_pKeys->SetEditable(false);
		m_pCopy = Button("Copy", "Copy", "CopyKeys");
		m_pCanvas = new CWseCanvas(this);
		ShowMouseLookButton(true);
		SetToolTip(m_pPlace, "Then click a wall to put a screen with this layout there. Only you see it, and it disappears "
		                     "when you close the editor or the map changes");
		SetToolTip(m_pLocalW, "The placed screen's width in map units");
		SetToolTip(m_pLocalT, "The placed screen's height in map units");
		SetToolTip(m_pKeys, "The keys a vgui_screen needs to show this screen in the same place in your map");
		SetToolTip(m_pCopy, "Copy the keys, so you can paste them into your map editor");
		UpdateKeys();
		SetToolTip(m_pHidden, "Show the controls that start hidden, drawn with a dashed outline, so you can select them");
		SetToolTip(m_pLoad, "Open a world screen layout, or start a new one");
		SetToolTip(m_pSave, "Save the layout as a .res file that the game reads");
		SetToolTip(m_pUndo, "Take back the last change");
		SetToolTip(m_pAdd, "Put a new control in the middle of the screen");
		SetToolTip(m_pDelete, "Remove the selected control");
		SetToolTip(m_pFront, "Show the Layers window, which lists the controls from front to back");
		SetToolTip(m_pInView, "Show your changes on the world screen in the middle of your view");
		SetToolTip(m_pPick, "Then click a world screen to show your changes on it");
		SetToolTip(m_pCanvas, "Click a control to select it, drag it to move it, or drag one of the small squares "
		                      "on its edges to resize it");
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 50);
	}

	CWseCanvas *Canvas() { return m_pCanvas; }

	//! The edit view built from the text again
	void Refresh()
	{
		m_pCanvas->Rebuild();
		UpdateButtons();
		FitCanvas();
	}

	//! The selection or the undo list changed
	void UpdateButtons()
	{
		const int sel = SelectedIndex();
		m_pDelete->SetEnabled(sel >= 0 && !IsScreen(sel));

		m_pUndo->SetEnabled(!s_Undo.empty());
		m_pCanvas->Repaint();
		Repaint();
	}

	void SetStatus(const std::string &s)
	{
		m_Status = s;
		FitText();
		Repaint();
	}

	void CloseEditor()
	{
		if (m_bClosing)
			return;
		m_bClosing = true;
		if (m_pCanvas->IsDragging())
			m_pCanvas->CancelDrag();
		for (CSboToolWindow *w : { (CSboToolWindow *)s_pProps, (CSboToolWindow *)s_pLoad, (CSboToolWindow *)s_pSave,
		         (CSboToolWindow *)s_pPictures, (CSboToolWindow *)s_pLayers, (CSboToolWindow *)this })
			if (w && w->IsOpen())
				w->CloseTool();
		if (s_pPicker && s_pPicker->IsPicking())
			s_pPicker->Cancel();
		StopPreview();
		m_bClosing = false;
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
		// a map change removed the screens (and the slot numbers mean other screens): pick again
		for (auto it = s_Picked.begin(); it != s_Picked.end();)
		{
			if (!worldscreen::EditorSlotPanel(*it)[0])
			{
				it = s_Picked.erase(it);
				s_bPreviewDirty = true;
			}
			else
				++it;
		}
		// a map change removed the editor's own screen
		if (s_bLocalPlaced && worldscreen::EditorLocalSlot() < 0)
		{
			s_bLocalPlaced = false;
			UpdateKeys();
		}
		// screens that came in since (a map change, a screen turned on later) get the preview too
		for (int slot : PreviewSlots())
			if (!s_Previewed.count(slot))
				s_bPreviewDirty = true;
		FlushPreview();
		// the status line follows the cursor
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		m_pCanvas->ScreenToLocal(x, y);
		const bool over = x >= 0 && y >= 0 && x < m_pCanvas->GetWide() && y < m_pCanvas->GetTall() && IsCursorOver();
		const int hover = over ? m_pCanvas->ControlAt(x, y) : -1;
		std::string info = m_pCanvas->IsDragging() ? m_pCanvas->Describe(SelectedIndex())
		    : hover >= 0 ? m_pCanvas->Describe(hover) : m_pCanvas->Describe(SelectedIndex());
		if (info != m_Info)
		{
			m_Info = info;
			FitText();
			Repaint();
		}
		const std::string preview = PreviewText();
		if (preview != m_Preview)
		{
			m_Preview = preview;
			Repaint();
		}
	}

	void OnCommand(const char *command) override;

	MESSAGE_FUNC(OnSelectChanged, "SelectChanged")
	{
		const int k = m_pAdd->GetSelected() - 1;
		m_pAdd->SetSelected(0);
		if (k >= 0 && k < KIND_COUNT)
			AddControl(KINDS[k]);
	}

	MESSAGE_FUNC_INT(OnButtonToggled, "ButtonToggled", state)
	{
		s_bShowHidden = state != 0;
		const int sel = SelectedIndex();
		m_pCanvas->Rebuild();
		// a hidden control can't stay selected when it is not shown
		vgui2::Panel *p = m_pCanvas->ControlPanel(sel);
		if (p && !p->IsVisible())
		{
			s_Selected.clear();
			RefreshProps();
		}
		m_pCanvas->Repaint();
	}

	MESSAGE_FUNC_INT_INT(OnScreenPicked, "ScreenPicked", x, y)
	{
		if (m_bPlacing)
		{
			m_bPlacing = false;
			float point[3], normal[3];
			if (!worldscreen::EditorTraceWall(x, y, point, normal))
			{
				SetStatus("There is no wall there, so try again closer to one");
				return;
			}
			for (int i = 0; i < 3; i++)
			{
				s_LocalPoint[i] = point[i];
				s_LocalNormal[i] = normal[i];
			}
			if (!s_bLocalPlaced)
			{
				// the first time: a quarter of the layout's pixels in map units (512 x 256 pixels: 128 x 64 units)
				s_flLocalWide = std::max(4.0f, m_pCanvas->GetWide() / 4.0f);
				s_flLocalTall = std::max(4.0f, m_pCanvas->GetTall() / 4.0f);
				m_pLocalW->SetValue(s_flLocalWide);
				m_pLocalT->SetValue(s_flLocalTall);
			}
			s_bLocalPlaced = true;
			UpdateLocalScreen();
			if (worldscreen::EditorLocalSlot() < 0)
			{
				s_bLocalPlaced = false;
				SetStatus("The map already shows as many world screens as the game can, so there is no room for one more");
			}
			else
				SetStatus("Your screen is on the wall. Only you see it, and the keys above put the same screen in your map");
			UpdateKeys();
			return;
		}
		const int slot = worldscreen::EditorSlotAtScreenPoint(x, y);
		UsePickedSlot(slot, "There is no world screen there");
	}
	MESSAGE_FUNC(OnScreenPickCancelled, "ScreenPickCancelled")
	{
		m_bPlacing = false;
		SetStatus("");
	}
	//! the placed screen's size, typed: it follows the typing
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel)
	{
		if (panel != m_pLocalW && panel != m_pLocalT)
			return;
		char text[32];
		static_cast<CSboNumberEntry *>(panel)->GetText(text, sizeof(text));
		char *end = nullptr;
		const float v = strtof(text, &end);
		if (!text[0] || !end || *end || v < 4.0f || v > 4096.0f)
			return;
		(panel == m_pLocalW ? s_flLocalWide : s_flLocalTall) = v;
		UpdateLocalScreen();
		UpdateKeys();
	}
	MESSAGE_FUNC_PARAMS(OnNumberEntered, "NumberEntered", data)
	{
		vgui2::Panel *p = (vgui2::Panel *)data->GetPtr("panel");
		if (p != m_pLocalW && p != m_pLocalT)
			return;
		(p == m_pLocalW ? s_flLocalWide : s_flLocalTall) = data->GetFloat("value");
		UpdateLocalScreen();
		UpdateKeys();
	}

	void UpdateKeys()
	{
		m_pKeys->SetValue(s_bLocalPlaced ? LocalKeysText().c_str() : "");
		m_pCopy->SetEnabled(s_bLocalPlaced);
		m_pLocalW->SetEnabled(s_bLocalPlaced);
		m_pLocalT->SetEnabled(s_bLocalPlaced);
	}

protected:
	//! its close button: the whole editor closes, as with the command
	void OnToolClosed() override { CloseEditor(); }
	//! Esc during a drag puts the control back (SetEscapeCloses is on only while dragging)
	void OnEscape() override { m_pCanvas->CancelDrag(); }

	void LayoutContent(int x, int y, int w, int h) override
	{
		const int bh = M(24), gap = M(6);
		int bx = x;
		for (vgui2::Panel *b : { (vgui2::Panel *)m_pLoad, (vgui2::Panel *)m_pSave, (vgui2::Panel *)m_pUndo })
		{
			const int bw = static_cast<CSboBrowserButton *>(b)->GetContentWide();
			b->SetBounds(bx, y, bw, bh);
			bx += bw + gap;
		}
		const int aw = std::max(M(150), m_pAdd->GetContentWide());
		m_pAdd->SetBounds(bx, y, aw, bh);
		bx += aw + gap;
		for (CSboBrowserButton *b : { m_pDelete, m_pFront })
		{
			const int bw = b->GetContentWide();
			b->SetBounds(bx, y, bw, bh);
			bx += bw + gap;
		}
		y += bh + gap;
		m_iPreviewY = y;
		const int pw = m_pPick->GetContentWide(), vw = m_pInView->GetContentWide();
		m_pPick->SetBounds(x + w - pw, y, pw, bh);
		m_pInView->SetBounds(x + w - pw - gap - vw, y, vw, bh);
		m_iPreviewW = w - pw - vw - 2 * gap;
		y += bh + gap;
		m_pHidden->SetBounds(x, y, m_pHidden->GetContentWide(), bh);
		{
			// right side: Place screen, Width, Height (the labels are painted)
			const int nw = M(56), lw = TextWide(S().body, L"Height") + M(6);
			int rx = x + w - nw;
			m_pLocalT->SetBounds(rx, y, nw, bh);
			m_iLocalTLabelX = rx - lw;
			rx -= lw + gap + nw;
			m_pLocalW->SetBounds(rx, y, nw, bh);
			m_iLocalWLabelX = rx - (TextWide(S().body, L"Width") + M(6));
			rx = m_iLocalWLabelX - gap - m_pPlace->GetContentWide();
			m_pPlace->SetBounds(rx, y, m_pPlace->GetContentWide(), bh);
			m_iLocalRowY = y;
		}
		y += bh + gap;
		{
			const int cw = m_pCopy->GetContentWide();
			m_pKeys->SetBounds(x, y, w - cw - gap, bh);
			m_pCopy->SetBounds(x + w - cw, y, cw, bh);
		}
		y += bh + gap;
		m_pCanvas->SetPos(x, y);
		m_iInfoY = y + m_pCanvas->GetTall() + gap;
	}

	void Paint() override
	{
		BaseClass::Paint();
		if (IsCollapsed())
			return;
		const int x = M(10);
		DrawTextMid(S().small, COL_TEXT_DIM, x, m_iPreviewY, M(24), Widen(m_Preview.c_str()), m_iPreviewW);
		DrawTextMid(S().body, COL_TEXT, m_iLocalWLabelX, m_iLocalRowY, M(24), L"Width");
		DrawTextMid(S().body, COL_TEXT, m_iLocalTLabelX, m_iLocalRowY, M(24), L"Height");
		// the line about the control and the status under it, each wrapped at the window's width
		const int textW = GetWide() - M(20);
		int ty = m_iInfoY;
		const std::vector<std::wstring> info = WrapLines(S().small, m_Info, textW);
		for (const std::wstring &line : info)
		{
			DrawText(S().small, COL_TEXT_DIM, x, ty, line);
			ty += M(16);
		}
		if (info.empty())
			ty += M(16);
		for (const std::wstring &line : WrapLines(S().small, m_Status, textW))
		{
			DrawText(S().small, COL_TEXT, x, ty, line);
			ty += M(16);
		}
	}

private:
	CSboBrowserButton *Button(const char *name, const char *text, const char *command)
	{
		CSboBrowserButton *b = new CSboBrowserButton(this, name, text, command, CSboBrowserButton::KIND_NORMAL);
		b->AddActionSignalTarget(this);
		return b;
	}

	std::string PreviewText()
	{
		const std::vector<int> slots = PreviewSlots();
		if (slots.empty())
			return s_Name.empty() ? "This layout isn't on any world screen yet, so pick one to see your changes on it"
			                      : "No world screen in this map uses " + s_Name + ", so pick one to see your changes on it";
		char buf[160];
		if (slots.size() == 1)
			snprintf(buf, sizeof(buf), "Your changes show on 1 world screen");
		else
			snprintf(buf, sizeof(buf), "Your changes show on %d world screens", (int)slots.size());
		return buf;
	}

	void UsePickedSlot(int slot, const char *none)
	{
		if (slot < 0)
		{
			SetStatus(none);
			return;
		}
		s_Picked.insert(slot);
		s_bPreviewDirty = true;
		FlushPreview(true);
		char buf[200];
		snprintf(buf, sizeof(buf), "The screen you picked shows your changes until you close the editor");
		SetStatus(buf);
		char msg[240];
		snprintf(msg, sizeof(msg), "The world screen editor is showing your layout on screen slot %d (entity %d), "
		                           "which normally shows %s\n", slot, worldscreen::EditorSlotEntity(slot),
		    worldscreen::EditorSlotPanel(slot));
		sbo::DevPrint(msg);
	}

	void AddControl(const KindDef &k)
	{
		if (!s_Doc.HasRoot())
			return;
		const std::string before = s_Doc.Text();
		const std::string base = !strcmp(k.cls, "Label") ? "Text" : !strcmp(k.cls, "ImagePanel") ? "Picture" : k.cls;
		std::string name;
		for (int n = 1; n < 10000 && name.empty(); n++)
			if (s_Doc.Find(base + std::to_string(n)) < 0)
				name = base + std::to_string(n);
		const int W = m_pCanvas->GetWide(), T = m_pCanvas->GetTall();
		const int w = std::min(k.wide, std::max(8, W - 16)), t = std::min(k.tall, std::max(8, T - 16));
		char xs[16], ys[16], ws[16], ts[16];
		snprintf(xs, sizeof(xs), "%d", (W - w) / 2);
		snprintf(ys, sizeof(ys), "%d", (T - t) / 2);
		snprintf(ws, sizeof(ws), "%d", w);
		snprintf(ts, sizeof(ts), "%d", t);
		std::vector<std::pair<std::string, std::string>> pairs = { { "ControlName", k.cls }, { "fieldName", name },
			{ "xpos", xs }, { "ypos", ys }, { "wide", ws }, { "tall", ts } };
		if (!strcmp(k.cls, "Label"))
		{
			pairs.push_back({ "labelText", "New text" });
			pairs.push_back({ "font", "WorldScreenText" });
			pairs.push_back({ "textAlignment", "west" });
		}
		else if (!strcmp(k.cls, "Button"))
		{
			pairs.push_back({ "labelText", "BUTTON" });
			pairs.push_back({ "font", "WorldScreenButton" });
			pairs.push_back({ "textAlignment", "center" });
			pairs.push_back({ "command", Lower(name) });
		}
		else if (!strcmp(k.cls, "ImagePanel"))
		{
			pairs.push_back({ "image", "ui/gfx/vgui/worldscreen_cursor" });
			pairs.push_back({ "scaleImage", "1" });
		}
		else if (strstr(k.cls, "ProgressBar"))
			pairs.push_back({ "progress", "0.5" });
		else if (strstr(k.cls, "AnalogBar"))
			pairs.push_back({ "analogValue", "0.5" });
		else if (!strcmp(k.cls, "Slider"))
		{
			pairs.push_back({ "rangeMin", "0" });
			pairs.push_back({ "rangeMax", "100" });
		}
		if (s_Doc.Add(name, pairs) < 0)
			return;
		PushUndo(before);
		s_bModified = true;
		s_Selected = name;
		OnDocChanged();
		SetStatus(std::string("Added ") + name + ", now drag it where you want it");
	}

	//! The height the wrapped line about the control and the status take at this text width (room for two lines about
	//! the control and one status line at least, so the window rarely grows under the Layers window placed below it)
	int TextTall(int textW)
	{
		const int infoLines = std::max(2, (int)WrapLines(S().small, m_Info, textW).size());
		const int statusLines = std::max(1, (int)WrapLines(S().small, m_Status, textW).size());
		return (infoLines + statusLines) * M(16);
	}

	//! The text under the canvas changed: the window grows when the wrapped lines need more room
	void FitText()
	{
		if (IsOpen() && !IsCollapsed())
			FitCanvas();
	}

	void FitCanvas()
	{
		// the window holds the screen at its real size (wider or taller than its design only when the screen is)
		const int cw = std::max(m_pCanvas->GetWide(), ToolbarWide()), ct = m_pCanvas->GetTall();
		int x, y, w, t;
		GetBounds(x, y, w, t);
		if (IsCollapsed())
			return;
		const int needW = cw + M(20), nw = std::max(w, needW);
		const int needT = M(26) + M(4) + M(8) + 4 * (M(24) + M(6)) + ct + M(6) + TextTall(nw - M(20)) + M(8);
		const int nt = std::max(t, needT);
		SetMinimumSize(std::max(M(380), needW), needT);
		if (nw != w || nt != t)
			SetSize(nw, nt);
		InvalidateLayout();
	}

	//! The width the buttons on the top row need
	int ToolbarWide()
	{
		// the Place row: Show hidden controls, Place screen, Width and Height
		const int placeRow = m_pHidden->GetContentWide() + m_pPlace->GetContentWide() + 2 * M(56) +
		                     TextWide(S().body, L"Width") + TextWide(S().body, L"Height") + M(60);
		int w = std::max(M(150), m_pAdd->GetContentWide());
		for (CSboBrowserButton *b : { m_pLoad, m_pSave, m_pUndo, m_pDelete, m_pFront })
			w += b->GetContentWide() + M(6);
		return std::max(w, placeRow);
	}

	CSboBrowserButton *m_pLoad, *m_pSave, *m_pUndo, *m_pDelete, *m_pFront, *m_pInView, *m_pPick;
	CSboBrowserSelect *m_pAdd;
	CSboBrowserCheck *m_pHidden;
	CSboBrowserButton *m_pPlace, *m_pCopy;
	CSboNumberEntry *m_pLocalW, *m_pLocalT;
	CSboTextField *m_pKeys;
	int m_iLocalRowY = 0, m_iLocalWLabelX = 0, m_iLocalTLabelX = 0;
	bool m_bPlacing = false; // the screen picker waits for Place screen's click (not Pick a screen's)
	CWseCanvas *m_pCanvas;
	std::string m_Status, m_Info, m_Preview;
	int m_iPreviewY = 0, m_iPreviewW = 0, m_iInfoY = 0;
	bool m_bClosing = false;
};

void CWseCanvas::BeginDragMode()
{
	if (s_pMain)
		s_pMain->SetEscapeCloses(true); // Esc ends the drag (OnEscape), not the game menu
}

void CWseCanvas::EndDragMode()
{
	if (s_pMain)
		s_pMain->SetEscapeCloses(false);
	RefreshProps();
}

//-----------------------------------------------------------------------------
// The selected control's settings, in plain names
//-----------------------------------------------------------------------------
enum PropKind
{
	PROP_NAME,   // the section's name
	PROP_TEXT,
	PROP_INT,
	PROP_PERCENT, // a 0..1 key shown in percent
	PROP_CHOICE,
	PROP_CHECK,
	PROP_COLOR,
};

enum
{
	FOR_SCREEN = 1 << 0,
	FOR_LABEL = 1 << 1,
	FOR_BUTTON = 1 << 2,
	FOR_IMAGE = 1 << 3,
	FOR_PROGRESS = 1 << 4,
	FOR_ANALOG = 1 << 5,
	FOR_SLIDER = 1 << 6,
	FOR_CONTROL = FOR_LABEL | FOR_BUTTON | FOR_IMAGE | FOR_PROGRESS | FOR_ANALOG | FOR_SLIDER,
	FOR_TEXT = FOR_LABEL | FOR_BUTTON,
	FOR_BAR = FOR_PROGRESS | FOR_ANALOG,
	FOR_ALL = FOR_SCREEN | FOR_CONTROL,
};

struct PropDef
{
	PropKind kind;
	const char *key;
	const char *label;
	int forMask;
	const char *tip;
	const char *choices;      // PROP_CHOICE: value=plain name|... ("" = not set)
	int min, max, def;        // PROP_INT / PROP_PERCENT / PROP_CHECK (def: when the key is not there)
};

const PropDef PROPS[] = {
	{ PROP_NAME, "", "Name", FOR_CONTROL, "The control's name. To change this control from the map, put this name in a vgui_control_proxy's Control Name",
	    nullptr, 0, 0, 0 },
	{ PROP_INT, "wide", "Width", FOR_SCREEN, "The screen's width in pixels, from 16 to 1024. More pixels look sharper "
	                                         "but take longer to draw",
	    nullptr, 16, 1024, 512 },
	{ PROP_INT, "tall", "Height", FOR_SCREEN, "The screen's height in pixels, from 16 to 1024. Give it the same shape "
	                                          "as the vgui_screen's Width and Height in your map, for example twice as wide as it is high, or it looks "
	                                          "stretched",
	    nullptr, 16, 1024, 256 },
	{ PROP_INT, "xpos", "Left", FOR_CONTROL, "How far the control's left edge is from the screen's left edge, in pixels",
	    nullptr, -1024, 2048, 0 },
	{ PROP_INT, "ypos", "Top", FOR_CONTROL, "How far the control's top edge is from the screen's top edge, in pixels",
	    nullptr, -1024, 2048, 0 },
	{ PROP_INT, "wide", "Width", FOR_CONTROL, "The control's width in pixels", nullptr, 0, 2048, 64 },
	{ PROP_INT, "tall", "Height", FOR_CONTROL, "The control's height in pixels", nullptr, 0, 2048, 24 },
	{ PROP_TEXT, "labelText", "Text", FOR_TEXT, "The words it shows. You can also type # and the name of a text from SBO's language files in resource/ "
	    "or ui/resource/language/, so every player reads it in their own language", nullptr, 0, 0, 0 },
	{ PROP_CHOICE, "font", "Font", FOR_TEXT, "The size and look of the text",
	    "=Default|WorldScreenHeader=Header, 42 pixels|WorldScreenTitle=Title, 34 pixels|WorldScreenLarge=Large, 30 "
	    "pixels|WorldScreenButton=Button, 24 pixels|WorldScreenText=Text, 22 pixels|WorldScreenSmall=Small, 18 pixels",
	    0, 0, 0 },
	{ PROP_CHOICE, "textAlignment", "Text position", FOR_TEXT, "Where the text sits in the control",
	    "=Default|north-west=Top left|north=Top middle|north-east=Top right|west=Left|center=Middle|east=Right|"
	    "south-west=Bottom left|south=Bottom middle|south-east=Bottom right",
	    0, 0, 0 },
	{ PROP_CHECK, "wrap", "Wrap long text", FOR_LABEL, "Break text that is too wide onto more lines", nullptr, 0, 1, 0 },
	{ PROP_COLOR, "fgcolor_override", "Text color", FOR_LABEL, "Click to choose the color of the text", nullptr, 0, 0, 0 },
	{ PROP_COLOR, "defaultFgColor_override", "Text color", FOR_BUTTON, "Click to choose the color of the button's text",
	    nullptr, 0, 0, 0 },
	{ PROP_COLOR, "defaultBgColor_override", "Button color", FOR_BUTTON, "Click to choose the color of the button",
	    nullptr, 0, 0, 0 },
	{ PROP_COLOR, "armedFgColor_override", "Text color when aimed at", FOR_BUTTON,
	    "Click to choose the color of the text while a player aims at the button", nullptr, 0, 0, 0 },
	{ PROP_COLOR, "armedBgColor_override", "Button color when aimed at", FOR_BUTTON,
	    "Click to choose the color of the button while a player aims at it", nullptr, 0, 0, 0 },
	{ PROP_COLOR, "depressedFgColor_override", "Text color when pressed", FOR_BUTTON,
	    "Click to choose the color of the text while the button is held down", nullptr, 0, 0, 0 },
	{ PROP_COLOR, "depressedBgColor_override", "Button color when pressed", FOR_BUTTON,
	    "Click to choose the color of the button while it is held down", nullptr, 0, 0, 0 },
	{ PROP_COLOR, "fgcolor_override", "Bar color", FOR_BAR | FOR_SLIDER, "Click to choose the color of the bar",
	    nullptr, 0, 0, 0 },
	{ PROP_COLOR, "bgcolor_override", "Background color", FOR_ALL & ~(FOR_IMAGE | FOR_BUTTON),
	    "Click to choose the color behind it", nullptr, 0, 0, 0 },
	{ PROP_TEXT, "command", "Command", FOR_BUTTON,
	    "When a player clicks the button, the vgui_screen fires the target in its key named ~ plus this "
	    "text, such as ~open. Type anim: and a sequence name instead to play an animation that only that player sees",
	    nullptr, 0, 0, 0 },
	{ PROP_CHECK, "enabled", "Can be clicked", FOR_BUTTON, "Untick it to keep the button switched off until the map turns "
	                                                      "it on",
	    nullptr, 0, 1, 1 },
	{ PROP_TEXT, "image", "Picture file", FOR_IMAGE,
	    "Where the .tga picture is, starting from the SBO folder and leaving off .tga, for example "
	    "ui/gfx/vgui/my_logo", nullptr, 0, 0, 0 },
	{ PROP_CHECK, "scaleImage", "Stretch to fit", FOR_IMAGE, "Stretch the picture to the control's size", nullptr, 0, 1,
	    0 },
	{ PROP_COLOR, "fillcolor", "Fill color", FOR_IMAGE, "Click to choose the color drawn behind the picture", nullptr, 0,
	    0, 0 },
	{ PROP_PERCENT, "progress", "Filled", FOR_PROGRESS, "How full the bar is at the start, in percent", nullptr, 0, 100,
	    0 },
	{ PROP_PERCENT, "analogValue", "Filled", FOR_ANALOG, "How full the bar is at the start, in percent", nullptr, 0, 100,
	    0 },
	{ PROP_INT, "rangeMin", "Lowest value", FOR_SLIDER, "The slider's value at its left end", nullptr, -100000, 100000,
	    0 },
	{ PROP_INT, "rangeMax", "Highest value", FOR_SLIDER, "The slider's value at its right end", nullptr, -100000, 100000,
	    100 },
	{ PROP_INT, "numTicks", "Tick marks", FOR_SLIDER, "How many tick marks are drawn under the slider", nullptr, 0, 100,
	    0 },
	{ PROP_TEXT, "leftText", "Left end text", FOR_SLIDER, "Words under the slider's left end", nullptr, 0, 0, 0 },
	{ PROP_TEXT, "rightText", "Right end text", FOR_SLIDER, "Words under the slider's right end", nullptr, 0, 0, 0 },
	{ PROP_CHECK, "visible", "Shown at the start", FOR_CONTROL,
	    "Untick it to keep the control hidden until the map shows it. The editor still shows it, drawn "
	    "with a dashed outline",
	    nullptr, 0, 1, 1 },
};
constexpr int PROP_COUNT = (int)(sizeof(PROPS) / sizeof(PROPS[0]));

int MaskOf(int sec)
{
	if (IsScreen(sec))
		return FOR_SCREEN;
	const std::string *cls = s_Doc.Get(sec, "ControlName");
	if (!cls)
		return FOR_LABEL;
	const char *c = cls->c_str();
	if (!Q_stricmp(c, "Button"))
		return FOR_BUTTON;
	if (!Q_stricmp(c, "ImagePanel"))
		return FOR_IMAGE;
	if (!Q_stricmp(c, "ProgressBar") || !Q_stricmp(c, "ContinuousProgressBar"))
		return FOR_PROGRESS;
	if (!Q_stricmp(c, "AnalogBar") || !Q_stricmp(c, "ContinuousAnalogBar"))
		return FOR_ANALOG;
	if (!Q_stricmp(c, "Slider"))
		return FOR_SLIDER;
	return FOR_LABEL;
}

std::vector<std::pair<std::string, std::string>> SplitPairs(const char *choices)
{
	std::vector<std::pair<std::string, std::string>> out;
	std::string cur;
	for (const char *p = choices;; p++)
	{
		if (!*p || *p == '|')
		{
			const size_t eq = cur.find('=');
			out.push_back({ cur.substr(0, eq), eq == std::string::npos ? cur : cur.substr(eq + 1) });
			cur.clear();
			if (!*p)
				break;
		}
		else
			cur += *p;
	}
	return out;
}

bool ParseColor(const std::string *v, int c[4])
{
	c[0] = c[1] = c[2] = 255;
	c[3] = 255;
	if (!v || v->empty())
		return false;
	int n = sscanf(v->c_str(), "%d %d %d %d", &c[0], &c[1], &c[2], &c[3]);
	for (int k = 0; k < 4; k++)
		c[k] = std::max(0, std::min(255, c[k]));
	return n >= 3;
}

class CWsePropsWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CWsePropsWindow, CSboToolWindow);

public:
	CWsePropsWindow()
	    : BaseClass(g_pViewport, "WseProps", "Control Settings", "wse_props", 340, 600, 0.70f, 0.08f)
	{
		for (int i = 0; i < PROP_COUNT; i++)
		{
			const PropDef &d = PROPS[i];
			Row r;
			r.def = &d;
			switch (d.kind)
			{
			case PROP_NAME:
			case PROP_TEXT:
				r.text = new CSboTextField(this, "Text", d.kind == PROP_NAME ? "Name" : "");
				r.text->SetMaximumCharCount(d.kind == PROP_NAME ? MAX_CONTROL_NAME : 127);
				r.text->AddActionSignalTarget(this);
				SetToolTip(r.text, d.tip);
				if (!Q_stricmp(d.key, "image"))
				{
					r.pick = new CSboBrowserButton(this, "Choose", "Choose", "PickPicture", CSboBrowserButton::KIND_NORMAL);
					r.pick->AddActionSignalTarget(this);
					SetToolTip(r.pick, "Choose a picture from a list with previews");
				}
				break;
			case PROP_INT:
			case PROP_PERCENT:
			{
				r.number = new CSboNumberEntry(this, "Number");
				SboValueRange range;
				range.min = (float)d.min;
				range.max = (float)d.max;
				range.step = 1.0f;
				r.number->SetRange(range);
				r.number->AddActionSignalTarget(this);
				SetToolTip(r.number, d.tip);
				break;
			}
			case PROP_CHOICE:
				r.select = new CSboBrowserSelect(this, "Choice");
				for (const auto &c : SplitPairs(d.choices))
					r.select->AddItem(c.second.c_str());
				r.select->AddActionSignalTarget(this);
				SetToolTip(r.select, d.tip);
				break;
			case PROP_CHECK:
				r.check = new CSboBrowserCheck(this, "Check", d.label);
				r.check->AddActionSignalTarget(this);
				SetToolTip(r.check, d.tip);
				break;
			case PROP_COLOR:
				r.swatch = new CSboColorSwatch(this, "Color", d.label);
				r.swatch->SetHasAlpha(true);
				r.swatch->AddActionSignalTarget(this);
				r.clear = new CSboBrowserButton(this, "Clear", "Default", "", CSboBrowserButton::KIND_NORMAL);
				r.clear->SetCommand(new KeyValues("ClearColor", "row", i));
				r.clear->AddActionSignalTarget(this);
				SetToolTip(r.swatch, d.tip);
				SetToolTip(r.clear, "Go back to the control's usual color");
				break;
			}
			m_Rows.push_back(r);
		}
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 50);
	}

	// Typing: TextEntry posts "TextChanged" on every key (the particle editor's sprite filter follows its typing the
	// same way), so the layout, the edit view and the previewed screens follow what is typed. One typing session is
	// one undo step: it ends with Enter, Esc or a click elsewhere (kept), or with a field that puts its old text back
	// (the layout goes back too).
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel)
	{
		Row *r = RowOf(panel);
		if (!r || (!r->text && !r->number) || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		if (m_Live.row && m_Live.row != r)
			FinishLive(true);
		const std::string text = FieldText(r);
		if (!m_Live.row)
		{
			m_Live.row = r;
			m_Live.start = s_Doc.Text();
			m_Live.startSelected = s_Selected;
		}
		m_Live.applied = text;
		const std::string before = s_Doc.Text();
		ApplyTyped(r, text);
		if (s_Doc.Text() != before)
		{
			OnDocChanged();
			UpdateTitle();
		}
	}

	void OnTick() override
	{
		BaseClass::OnTick();
		// the picture list is made a little each frame while its window is open
		if (s_pPictureScan && s_pPictures && s_pPictures->IsOpen() && s_pPictureScan->Step(0.008))
		{
			s_pPictures->SetFiles(s_pPictureScan->found);
			s_pPictures->SetStatus("Choosing a picture for " + m_PictureFor);
			char msg[120];
			snprintf(msg, sizeof(msg), "The world screen editor found %d pictures a screen can show\n",
			    (int)s_pPictureScan->found.size());
			sbo::DevPrint(msg);
			delete s_pPictureScan;
			s_pPictureScan = nullptr;
		}
		// typing ended without a message (Esc), or with one still on its way (Enter, a click elsewhere): the field's
		// text tells which (Esc puts back the text it had before)
		if (m_Live.row && !HasTextInput())
			FinishLive(FieldText(m_Live.row) == m_Live.applied);
	}

	void Refresh()
	{
		const int sec = SelectedIndex();
		m_iSec = sec;
		const int mask = sec >= 0 ? MaskOf(sec) : 0;
		if (sec < 0)
			m_Header = "Click a control on the screen to see its settings";
		else if (IsScreen(sec))
			m_Header = "Settings for the whole screen";
		else
			m_Header = s_Doc.Sec(sec).name + " is " + PlainKind(sec);
		for (Row &r : m_Rows)
		{
			r.shown = (r.def->forMask & mask) != 0;
			r.SetVisible(r.shown && !IsCollapsed());
			if (!r.shown || m_bColorEdit)
				continue;
			const PropDef &d = *r.def;
			const std::string *v = d.kind == PROP_NAME ? &s_Doc.Sec(sec).name : s_Doc.Get(sec, d.key);
			switch (d.kind)
			{
			case PROP_NAME:
			case PROP_TEXT:
				r.text->SetValue(v ? v->c_str() : "");
				break;
			case PROP_INT:
				r.number->SetValue((float)(v && !v->empty() ? atoi(v->c_str()) : DefaultInt(d, sec)));
				break;
			case PROP_PERCENT:
				r.number->SetValue(v && !v->empty() ? floorf((float)atof(v->c_str()) * 100.0f + 0.5f) : (float)d.def);
				break;
			case PROP_CHOICE:
			{
				const auto pairs = SplitPairs(d.choices);
				int pick = 0;
				for (int k = 0; k < (int)pairs.size(); k++)
					if (v && !Q_stricmp(pairs[k].first.c_str(), v->c_str()))
						pick = k;
				r.select->SetSelected(pick);
				break;
			}
			case PROP_CHECK:
				r.check->SetChecked(v && !v->empty() ? atoi(v->c_str()) != 0 : d.def != 0);
				break;
			case PROP_COLOR:
			{
				int c[4];
				const bool set = ParseColor(v, c);
				if (!set)
					DefaultColor(d.key, sec, c); // the color the scheme gives it, as the view shows it
				r.swatch->SetColor(c[0], c[1], c[2]);
				r.swatch->SetAlpha(c[3] / 255.0f);
				r.clear->SetEnabled(set);
				break;
			}
			}
		}
		InvalidateLayout();
		Repaint();
	}

	MESSAGE_FUNC_PARAMS(OnTextEntered, "TextEntered", data)
	{
		Row *r = RowOf((vgui2::Panel *)data->GetPtr("panel"));
		if (r && m_Live.row == r)
			FinishLive(true);
		if (!r || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		const std::string text = data->GetString("text");
		if (r->def->kind == PROP_NAME)
		{
			Rename(text);
			return;
		}
		const std::string *old = s_Doc.Get(m_iSec, r->def->key);
		if (old && *old == text)
			return;
		Change([&]() {
			if (text.empty() && Q_stricmp(r->def->key, "labelText"))
				s_Doc.Remove(m_iSec, r->def->key);
			else
				s_Doc.Set(m_iSec, r->def->key, text);
		});
	}

	MESSAGE_FUNC_PARAMS(OnNumberEntered, "NumberEntered", data)
	{
		Row *r = RowOf((vgui2::Panel *)data->GetPtr("panel"));
		if (r && m_Live.row == r)
			FinishLive(true);
		if (!r || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		const float value = data->GetFloat("value");
		char buf[32];
		if (r->def->kind == PROP_PERCENT)
			snprintf(buf, sizeof(buf), "%g", std::max(0.0f, std::min(100.0f, value)) / 100.0f);
		else
			snprintf(buf, sizeof(buf), "%d", (int)floorf(value + 0.5f));
		const std::string *old = s_Doc.Get(m_iSec, r->def->key);
		if (old && *old == buf)
			return;
		Change([&]() { s_Doc.Set(m_iSec, r->def->key, buf); });
	}

	MESSAGE_FUNC_PTR(OnSelectChanged, "SelectChanged", panel)
	{
		Row *r = RowOf(panel);
		if (!r || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		const auto pairs = SplitPairs(r->def->choices);
		const int k = r->select->GetSelected();
		if (k < 0 || k >= (int)pairs.size())
			return;
		Change([&]() {
			if (pairs[k].first.empty())
				s_Doc.Remove(m_iSec, r->def->key);
			else
				s_Doc.Set(m_iSec, r->def->key, pairs[k].first);
		});
	}

	MESSAGE_FUNC_PARAMS(OnButtonToggled, "ButtonToggled", data)
	{
		Row *r = RowOf((vgui2::Panel *)data->GetPtr("panel"));
		if (!r || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		const bool on = data->GetInt("state") != 0;
		Change([&]() {
			if (on == (r->def->def != 0))
				s_Doc.Remove(m_iSec, r->def->key); // the default: the key is not needed
			else
				s_Doc.Set(m_iSec, r->def->key, on ? "1" : "0");
		});
	}

	MESSAGE_FUNC_PTR(OnColorPickerOpened, "ColorPickerOpened", panel)
	{
		Row *r = RowOf(panel);
		if (!r || m_iSec < 0)
			return;
		m_ColorRow = r;
		m_ColorStart = s_Doc.Text();
		m_bColorStartModified = s_bModified;
	}

	MESSAGE_FUNC_PARAMS(OnColorChanged, "ColorChanged", data)
	{
		Row *r = RowOf((vgui2::Panel *)data->GetPtr("panel"));
		if (!r || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		if (data->GetInt("cancel"))
		{
			// Esc: the color as it was when the window opened
			if (m_ColorRow == r && !m_ColorStart.empty())
			{
				s_Doc.SetText(m_ColorStart);
				s_bModified = m_bColorStartModified;
				m_ColorRow = nullptr;
				OnDocChanged();
				UpdateTitle();
			}
			return;
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%d %d %d %d", data->GetInt("r"), data->GetInt("g"), data->GetInt("b"),
		    (int)floorf(data->GetFloat("a", 1.0f) * 255.0f + 0.5f));
		const std::string *old = s_Doc.Get(m_iSec, r->def->key);
		if (old && *old == buf)
			return;
		// one undo step for the whole pick (the text from when the window opened)
		if (m_ColorRow == r && !m_ColorStart.empty())
			PushUndo(m_ColorStart);
		else
			PushUndo(s_Doc.Text());
		s_Doc.Set(m_iSec, r->def->key, buf);
		s_bModified = true;
		m_bColorEdit = true; // the swatch shows the color already
		OnDocChanged();
		m_bColorEdit = false;
		UpdateTitle();
	}

	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "PickPicture"))
		{
			OpenPictures();
			return;
		}
		BaseClass::OnCommand(command);
	}

	//! The picture list for the selected Picture control; the one chosen goes in its Picture file (one undo step)
	void OpenPictures()
	{
		if (m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		if (!s_pPictures)
		{
			s_pPictures = new CSboPicturePicker(g_pViewport, "WsePictures", "Pictures", "wse_pictures", "pictures");
			s_pPictures->SetCellColor(Color(40, 40, 40, 255));
			s_pPictures->SetThumbMaker(PictureThumb);
			s_pPictures->SetTips("Type part of a name to find a picture", "Double-click a picture or press Enter to use it",
			    "Use the selected picture");
		}
		if (!s_pPictures->HasFiles() && !s_pPictureScan)
			s_pPictureScan = new PictureScan(); // OnTick makes the list, a little each frame
		const std::string name = s_Doc.Sec(m_iSec).name;
		m_PictureFor = name;
		s_pPictures->OpenFor(s_pPictureScan ? "Looking for pictures in the game's folders" : "Choosing a picture for " + name,
		    [name](const std::string &image) -> std::string {
			const int sec = s_Doc.Find(name);
			if (sec < 0)
				return "That control was deleted";
			if (s_pProps)
				s_pProps->SetPicture(sec, image);
			return "";
		});
	}

	void SetPicture(int sec, const std::string &image)
	{
		const std::string *old = s_Doc.Get(sec, "image");
		if (old && *old == image)
			return;
		const std::string before = s_Doc.Text();
		s_Doc.Set(sec, "image", image);
		PushUndo(before);
		OnDocChanged();
		UpdateTitle();
	}

	MESSAGE_FUNC_INT(OnClearColor, "ClearColor", row)
	{
		if (row < 0 || row >= (int)m_Rows.size() || m_iSec < 0 || m_iSec >= s_Doc.Count())
			return;
		Row &r = m_Rows[row];
		if (!s_Doc.Get(m_iSec, r.def->key))
			return;
		Change([&]() { s_Doc.Remove(m_iSec, r.def->key); });
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		const int labelW = M(130), rowH = M(22), gap = M(4);
		m_iHeaderY = y;
		y += M(22);
		for (Row &r : m_Rows)
		{
			if (!r.shown)
				continue;
			r.y = y;
			const int fx = x + labelW, fw = w - labelW;
			if (r.text)
			{
				const int pw = r.pick ? r.pick->GetContentWide() : 0;
				r.text->SetBounds(fx, y, r.pick ? fw - pw - gap : fw, rowH);
				if (r.pick)
					r.pick->SetBounds(fx + fw - pw, y, pw, rowH);
			}
			if (r.number)
				r.number->SetBounds(fx, y, M(80), rowH);
			if (r.select)
				r.select->SetBounds(fx, y, fw, rowH);
			if (r.check)
				r.check->SetBounds(x, y, w, rowH);
			if (r.swatch)
			{
				r.swatch->SetBounds(fx, y, M(48), rowH);
				const int cw = r.clear->GetContentWide();
				r.clear->SetBounds(fx + M(48) + gap, y, cw, rowH);
			}
			y += rowH + gap;
		}
	}

	void Paint() override
	{
		BaseClass::Paint();
		if (IsCollapsed())
			return;
		const int x = M(10);
		DrawText(S().small, COL_TEXT_DIM, x, m_iHeaderY, Widen(m_Header.c_str()), GetWide() - M(20));
		for (const Row &r : m_Rows)
			if (r.shown && !r.check)
				DrawTextMid(S().body, COL_TEXT, x, r.y, M(22), Widen(r.def->label), M(124));
	}

private:
	struct Row
	{
		const PropDef *def = nullptr;
		CSboTextField *text = nullptr;
		CSboNumberEntry *number = nullptr;
		CSboBrowserSelect *select = nullptr;
		CSboBrowserCheck *check = nullptr;
		CSboColorSwatch *swatch = nullptr;
		CSboBrowserButton *clear = nullptr;
		CSboBrowserButton *pick = nullptr; // the picture list (Picture file)
		bool shown = false;
		int y = 0;
		bool Owns(vgui2::Panel *p) const
		{
			return p && (p == text || p == number || p == select || p == check || p == swatch || p == clear || p == pick);
		}
		void SetVisible(bool v)
		{
			for (vgui2::Panel *p : { (vgui2::Panel *)text, (vgui2::Panel *)number, (vgui2::Panel *)select,
			         (vgui2::Panel *)check, (vgui2::Panel *)swatch, (vgui2::Panel *)clear, (vgui2::Panel *)pick })
				if (p)
					p->SetVisible(v);
		}
	};

	Row *RowOf(vgui2::Panel *p)
	{
		for (Row &r : m_Rows)
			if (r.Owns(p))
				return &r;
		return nullptr;
	}

	int DefaultInt(const PropDef &d, int sec)
	{
		// a control without a size key: the size the game gives it (the editor's view has it)
		if (s_pMain && (!Q_stricmp(d.key, "wide") || !Q_stricmp(d.key, "tall") || !Q_stricmp(d.key, "xpos") ||
		                   !Q_stricmp(d.key, "ypos")))
		{
			if (IsScreen(sec))
				return !Q_stricmp(d.key, "wide") ? s_pMain->Canvas()->GetWide() : s_pMain->Canvas()->GetTall();
			if (vgui2::Panel *p = s_pMain->Canvas()->ControlPanel(sec))
			{
				int x, y, w, t;
				p->GetBounds(x, y, w, t);
				return !Q_stricmp(d.key, "xpos") ? x : !Q_stricmp(d.key, "ypos") ? y : !Q_stricmp(d.key, "wide") ? w : t;
			}
		}
		return d.def;
	}

	void DefaultColor(const char *key, int sec, int c[4])
	{
		if (!s_pMain)
			return;
		vgui2::Panel *p = IsScreen(sec) ? s_pMain->Canvas()->ScreenPanel() : s_pMain->Canvas()->ControlPanel(sec);
		if (!p)
			return;
		// a button's colors come from the scheme's Button colors
		static const char *const BUTTON_COLORS[][2] = { { "defaultFgColor_override", "Button.TextColor" },
			{ "defaultBgColor_override", "Button.BgColor" }, { "armedFgColor_override", "Button.ArmedTextColor" },
			{ "armedBgColor_override", "Button.ArmedBgColor" }, { "depressedFgColor_override", "Button.DepressedTextColor" },
			{ "depressedBgColor_override", "Button.DepressedBgColor" } };
		Color col;
		if (!Q_stricmp(key, "fgcolor_override"))
			col = p->GetFgColor();
		else if (!Q_stricmp(key, "bgcolor_override"))
			col = p->GetBgColor();
		else
		{
			const char *schemeName = nullptr;
			for (const auto &bc : BUTTON_COLORS)
				if (!Q_stricmp(key, bc[0]))
					schemeName = bc[1];
			vgui2::IScheme *scheme = vgui2::scheme()->GetIScheme(p->GetScheme());
			if (!schemeName || !scheme)
				return;
			col = scheme->GetColor(schemeName, Color(255, 255, 255, 255));
		}
		c[0] = col.r();
		c[1] = col.g();
		c[2] = col.b();
		c[3] = col.a();
	}

	void Change(const std::function<void()> &edit)
	{
		const std::string before = s_Doc.Text();
		edit();
		if (s_Doc.Text() == before)
			return;
		PushUndo(before);
		s_bModified = true;
		OnDocChanged();
		UpdateTitle();
	}

	void Rename(const std::string &text)
	{
		std::string name;
		for (char c : text)
			if (isalnum((unsigned char)c) || c == '_' || c == '-')
				name += c;
		name = name.substr(0, MAX_CONTROL_NAME);
		const std::string old = s_Doc.Sec(m_iSec).name;
		if (name == old)
			return;
		if (name.empty() || !Q_stricmp(name.c_str(), wslayout::SCREEN_SECTION) ||
		    (s_Doc.Find(name) >= 0 && s_Doc.Find(name) != m_iSec))
		{
			if (s_pMain)
				s_pMain->SetStatus(name.empty() ? "A name can only use letters, digits, _ and -"
				                                : "Another control already has the name " + name);
			Refresh();
			return;
		}
		Change([&]() { s_Doc.Rename(m_iSec, name); });
		s_Selected = name;
		Refresh();
	}

	std::vector<Row> m_Rows;
	int m_iSec = -1;
	std::string m_Header;
	int m_iHeaderY = 0;
	Row *m_ColorRow = nullptr;
	std::string m_ColorStart;
	bool m_bColorStartModified = false;
	bool m_bColorEdit = false;
	std::string m_PictureFor; // the control the picture list chooses for

	struct LiveTyping
	{
		Row *row = nullptr;
		std::string start;         // the text before the typing (its undo step)
		std::string startSelected; // the selection then (a name typed renames it)
		std::string applied;       // the field's text the layout has now
	} m_Live;

	std::string FieldText(Row *r)
	{
		char buf[256] = "";
		if (r->text)
			r->text->GetText(buf, sizeof(buf));
		else if (r->number)
			r->number->GetText(buf, sizeof(buf));
		return buf;
	}

	//! What is typed so far goes into the layout (a number only once it is one, a name only once it is a free one)
	void ApplyTyped(Row *r, const std::string &text)
	{
		const PropDef &d = *r->def;
		switch (d.kind)
		{
		case PROP_NAME:
		{
			std::string name;
			for (char c : text)
				if (isalnum((unsigned char)c) || c == '_' || c == '-')
					name += c;
			name = name.substr(0, MAX_CONTROL_NAME);
			const int other = s_Doc.Find(name);
			if (name.empty() || name != text || !Q_stricmp(name.c_str(), wslayout::SCREEN_SECTION) ||
			    (other >= 0 && other != m_iSec) || name == s_Doc.Sec(m_iSec).name)
				return;
			s_Doc.Rename(m_iSec, name);
			s_Selected = name;
			return;
		}
		case PROP_TEXT:
			if (text.empty() && Q_stricmp(d.key, "labelText"))
				s_Doc.Remove(m_iSec, d.key);
			else
				s_Doc.Set(m_iSec, d.key, text);
			return;
		case PROP_INT:
		case PROP_PERCENT:
		{
			char *end = nullptr;
			float v = strtof(text.c_str(), &end);
			if (text.empty() || !end || *end)
				return; // "-" or "" while typing: not a number yet
			v = std::max((float)d.min, std::min((float)d.max, v));
			char buf[32];
			if (d.kind == PROP_PERCENT)
				snprintf(buf, sizeof(buf), "%g", v / 100.0f);
			else
				snprintf(buf, sizeof(buf), "%d", (int)floorf(v + 0.5f));
			s_Doc.Set(m_iSec, d.key, buf);
			return;
		}
		default:
			return;
		}
	}

	//! The typing session ends: kept (one undo step) or put back
	void FinishLive(bool keep)
	{
		if (!m_Live.row)
			return;
		LiveTyping live = m_Live;
		m_Live = LiveTyping();
		if (keep)
		{
			if (s_Doc.Text() != live.start)
				PushUndo(live.start);
			UpdateTitle();
			if (s_pMain)
				s_pMain->UpdateButtons();
			return;
		}
		if (s_Doc.Text() != live.start)
		{
			s_Doc.SetText(live.start);
			s_Selected = live.startSelected;
			OnDocChanged();
			UpdateTitle();
		}
	}
};

//-----------------------------------------------------------------------------
// Save: ui/scripts/screens/<name>.res in the SBO folder
//-----------------------------------------------------------------------------
class CWseSaveWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CWseSaveWindow, CSboToolWindow);

public:
	CWseSaveWindow()
	    : BaseClass(g_pViewport, "WseSave", "Save Layout", "wse_save", 340, 166, 0.35f, 0.35f)
	{
		m_pName = new CSboTextField(this, "Name", "Name of the layout");
		m_pName->SetMaximumCharCount(31);
		m_pName->AddActionSignalTarget(this);
		m_pSave = new CSboBrowserButton(this, "Save", "Save", "DoSave", CSboBrowserButton::KIND_PRIMARY);
		m_pCancel = new CSboBrowserButton(this, "Cancel", "Cancel", "Close", CSboBrowserButton::KIND_NORMAL);
		SetToolTip(m_pName, "The file name, using letters, digits, _ and -");
		SetToolTip(m_pSave, "Save the layout as ui/scripts/screens/<name>.res in the SBO folder");
		SetToolTip(m_pCancel, "Close without saving");
		SetToolSizeable(false);
	}
	void Open() override
	{
		m_Status = "Use letters, digits, _ and -";
		m_Hint.clear();
		m_Confirm.clear();
		m_pName->SetValue(s_Name.c_str());
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

void CWseSaveWindow::Save()
{
	char name[40];
	if (!wslayout::CleanLayoutName(m_pName->GetValue(), name, sizeof(name)))
	{
		m_Status = "Use up to 31 letters, digits, _ and -";
		m_Hint.clear();
		Repaint();
		return;
	}
	const std::string path = LayoutPath(name);
	// an existing file is replaced only on a second Save with the same name
	if (m_Confirm != name && g_pFullFileSystem->FileExists(path.c_str()))
	{
		m_Confirm = name;
		m_Status = std::string(name) + ".res already exists. Press Save again to replace it";
		m_Hint.clear();
		Repaint();
		return;
	}
	g_pFullFileSystem->CreateDirHierarchy(VGUI2_ROOT_DIR "scripts/screens", "GAMECONFIG");
	FileHandle_t fh = g_pFullFileSystem->Open(path.c_str(), "wb", "GAMECONFIG");
	bool ok = false;
	const std::string &text = s_Doc.Text();
	if (fh)
	{
		ok = text.empty() || g_pFullFileSystem->Write(text.data(), (int)text.size(), fh) == (int)text.size();
		g_pFullFileSystem->Close(fh);
	}
	m_Confirm.clear();
	char line[240];
	if (!ok)
	{
		snprintf(line, sizeof(line), "Couldn't save %s", path.c_str());
		m_Status = line;
		m_Hint.clear();
		Repaint();
		return;
	}
	const bool renamed = Q_stricmp(name, s_Name.c_str()) != 0;
	if (renamed)
		for (int slot : PreviewSlots())
			s_Picked.insert(slot); // the screens that showed it keep showing it under its new name
	s_Name = name;
	s_SavedText = s_Doc.Text();
	s_bModified = false;
	if (renamed)
		s_bPreviewDirty = true; // screens with this name preview it now
	snprintf(line, sizeof(line), "Saved %s/%s", sbo::GetGameDirectory(), path.c_str());
	m_Status = line;
	snprintf(line, sizeof(line), "To use it, set a vgui_screen's Panel Name to %s", name);
	m_Hint = line;
	char msg[300];
	snprintf(msg, sizeof(msg), "The world screen editor saved %s/%s with %s\n", sbo::GetGameDirectory(),
	    path.c_str(), ControlCountText().c_str());
	sbo::DevPrint(msg);
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
	Repaint();
}

void OpenSaveWindow(std::function<void()> after)
{
	if (!s_pSave)
		s_pSave = new CWseSaveWindow();
	s_AfterSave = after;
	s_pSave->Open();
}

//-----------------------------------------------------------------------------
// Load: a new layout, or the layouts in ui/scripts/screens/ (the game's, the mod folder's, downloads)
//-----------------------------------------------------------------------------
class CWseLoadWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CWseLoadWindow, CSboToolWindow);

public:
	CWseLoadWindow()
	    : BaseClass(g_pViewport, "WseLoad", "Load Layout", "wse_load", 300, 340, 0.45f, 0.2f)
	{
		m_pList = new CSboOrderedList(this, "Layouts");
		m_pList->SetStandardButtons(false);
		CSboBrowserButton *load = m_pList->AddButton("Load", "DoLoad");
		m_pList->AddActionSignalTarget(this);
		SetToolTip(m_pList, "Double-click a layout or press Enter to open it");
		SetToolTip(load, "Open the selected layout");
	}
	void Open() override
	{
		m_Names = ListLayouts();
		m_pList->ClearRows();
		m_pList->AddRow("(new layout)", -1, true);
		for (const std::string &n : m_Names)
			m_pList->AddRow((n + ".res").c_str(), -1, true);
		m_Status = "Double-click a layout to open it";
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
		m_iTextY = y + h - M(18);
		m_pList->SetBounds(x, y, w, h - M(24));
	}
	void Paint() override
	{
		BaseClass::Paint();
		if (IsCollapsed())
			return;
		DrawText(S().small, COL_TEXT_DIM, M(10), m_iTextY, Widen(m_Status.c_str()), GetWide() - M(20));
	}

private:
	void Load(int index)
	{
		if (index < 0 || index > (int)m_Names.size() || !s_pMain)
			return;
		ConfirmDiscard([this, index]() { DoLoad(index); });
	}
	void DoLoad(int index);
	CSboOrderedList *m_pList;
	std::vector<std::string> m_Names;
	std::string m_Status;
	int m_iTextY = 0;
};

void StartLayout(const std::string &name, const std::string &text)
{
	// the screens that showed the old layout because of its name show their own files again; the screens you picked
	// keep showing the editor's layout, now this one
	for (auto it = s_Previewed.begin(); it != s_Previewed.end();)
	{
		if (!s_Picked.count(*it) && *it != worldscreen::EditorLocalSlot())
		{
			worldscreen::EditorPreview(*it, nullptr);
			it = s_Previewed.erase(it);
		}
		else
			++it;
	}
	s_Doc.SetText(text);
	s_Name = name;
	s_SavedText = s_Doc.Text();
	s_bModified = false;
	s_Undo.clear();
	s_Selected.clear();
	s_bPreviewDirty = true;
	OnDocChanged();
	UpdateTitle();
}

void CWseLoadWindow::DoLoad(int index)
{
	if (index < 0 || index > (int)m_Names.size() || !s_pMain)
		return;
	if (index == 0)
	{
		StartLayout("", wslayout::NewLayoutText("new_screen"));
		s_pMain->SetStatus("This is a new layout. You'll name it when you save");
	}
	else
	{
		const std::string &name = m_Names[index - 1];
		const std::string path = LayoutPath(name);
		std::string text;
		wslayout::Doc check;
		if (!ReadGameFile(path.c_str(), text) || !check.SetText(text))
		{
			m_Status = "Couldn't read " + path;
			Repaint();
			return;
		}
		StartLayout(name, text);
		char line[200];
		snprintf(line, sizeof(line), "Loaded %s with %s", path.c_str(), ControlCountText().c_str());
		s_pMain->SetStatus(line);
		char msg[256];
		snprintf(msg, sizeof(msg), "The world screen editor loaded %s with %s\n", path.c_str(),
		    ControlCountText().c_str());
		sbo::DevPrint(msg);
	}
	CloseTool(); // loaded: the list goes
}

//-----------------------------------------------------------------------------

void CWseMainWindow::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "Close"))
	{
		// the close button: unsaved changes are asked about first
		ConfirmDiscard([this]() { CloseEditor(); });
		return;
	}
	if (!Q_stricmp(command, "LoadLayout"))
	{
		if (!s_pLoad)
			s_pLoad = new CWseLoadWindow();
		s_pLoad->Open();
		return;
	}
	if (!Q_stricmp(command, "SaveLayout"))
	{
		OpenSaveWindow(nullptr);
		return;
	}
	if (!Q_stricmp(command, "Undo"))
	{
		if (s_Undo.empty())
			return;
		s_Doc.SetText(s_Undo.back());
		s_Undo.pop_back();
		s_bModified = true;
		if (s_Doc.Find(s_Selected) < 0)
			s_Selected.clear();
		OnDocChanged();
		UpdateTitle();
		SetStatus(s_Undo.empty() ? "Nothing more to undo" : "");
		return;
	}
	if (!Q_stricmp(command, "DeleteControl"))
	{
		const int sel = SelectedIndex();
		if (sel < 0 || IsScreen(sel))
			return;
		PushUndo(s_Doc.Text());
		const std::string name = s_Doc.Sec(sel).name;
		s_Doc.Delete(sel);
		s_Selected.clear();
		s_bModified = true;
		OnDocChanged();
		UpdateTitle();
		SetStatus("Removed " + name + ". Press Undo to bring it back");
		return;
	}
	if (!Q_stricmp(command, "ShowLayers"))
	{
		OpenLayers();
		return;
	}
	if (!Q_stricmp(command, "PickInView"))
	{
		UsePickedSlot(worldscreen::EditorSlotInView(), "There is no world screen in the middle of your view");
		return;
	}
	if (!Q_stricmp(command, "PlaceScreen"))
	{
		if (!s_pPicker)
			s_pPicker = new CSboScreenPicker(g_pViewport);
		m_bPlacing = true;
		s_pPicker->Start(this);
		SetStatus("Click a wall where the screen should go, or press Esc to cancel");
		return;
	}
	if (!Q_stricmp(command, "CopyKeys"))
	{
		if (s_bLocalPlaced)
		{
			const std::string keys = LocalKeysText();
			vgui2::system()->SetClipboardText(keys.c_str(), (int)keys.size());
			SetStatus("The keys are copied, so you can paste them into your map editor");
		}
		return;
	}
	if (!Q_stricmp(command, "PickScreen"))
	{
		if (!s_pPicker)
			s_pPicker = new CSboScreenPicker(g_pViewport);
		m_bPlacing = false;
		s_pPicker->Start(this);
		SetStatus("Click a world screen, or press Esc to cancel");
		return;
	}
	BaseClass::OnCommand(command);
}

std::string LocalKeysText()
{
	float origin[3], angles[3];
	worldscreen::EditorWallPlace(s_LocalPoint, s_LocalNormal, s_flLocalWide, s_flLocalTall, origin, angles);
	char buf[256];
	// whole numbers ("-0" reads as a minus sign)
	auto whole = [](float v) { return (int)lroundf(v); };
	snprintf(buf, sizeof(buf), "origin %d %d %d, angles %d %d 0, width %d, height %d, panelname %s", whole(origin[0]),
	    whole(origin[1]), whole(origin[2]), whole(angles[0]), whole(angles[1]), whole(s_flLocalWide), whole(s_flLocalTall),
	    s_Name.empty() ? "(the name you save it as)" : s_Name.c_str());
	return buf;
}

//! Puts the editor's own screen where it was placed, at its size, with the layout as it is now
void UpdateLocalScreen()
{
	if (!s_bLocalPlaced)
		return;
	float origin[3], angles[3];
	worldscreen::EditorWallPlace(s_LocalPoint, s_LocalNormal, s_flLocalWide, s_flLocalTall, origin, angles);
	const int slot = worldscreen::EditorPlaceLocal(origin, angles, s_flLocalWide, s_flLocalTall,
	    s_Name.empty() ? "new_screen" : s_Name.c_str(), s_Doc.Text().c_str());
	if (slot >= 0)
		s_Previewed.insert(slot);
	char msg[200];
	snprintf(msg, sizeof(msg), "The world screen editor placed its own screen at %.0f %.0f %.0f, slot %d\n", origin[0],
	    origin[1], origin[2], slot);
	sbo::DevPrint(msg);
}

//-----------------------------------------------------------------------------
// Layers: the controls in the order the screen draws them, the top one first. A screen draws its controls in the
// order of the layout's sections, the last on top (the SDK's BuildGroup makes them in that order); a "zpos" key would
// sort them first, so a change of order takes the layout's zpos keys out after putting the sections in the order they
// gave. Each row's square shows and hides the control at the start (its "visible" key).
//-----------------------------------------------------------------------------
//! The controls' sections in drawing order, the first drawn first (zpos first, then the order in the file)
std::vector<int> DrawOrder()
{
	std::vector<int> order;
	for (int i = 0; i < s_Doc.Count(); i++)
		if (!IsScreen(i))
			order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [](int a, int b) { return IntKey(a, "zpos", 0) < IntKey(b, "zpos", 0); });
	return order;
}

bool StartsVisible(int sec)
{
	return IntKey(sec, "visible", 1) != 0;
}

class CWseLayersWindow : public CSboToolWindow
{
	DECLARE_CLASS_SIMPLE(CWseLayersWindow, CSboToolWindow);

public:
	CWseLayersWindow()
	    : BaseClass(g_pViewport, "WseLayers", "Layers", "wse_layers", 300, 380, 0.70f, 0.62f)
	{
		m_pList = new CSboOrderedList(this, "Layers");
		m_pList->SetStandardButtons(false);
		m_pList->SetIconPainter([](int, bool on, int x, int y, int size) { PaintEye(on, x, y, size); });
		m_pForward = m_pList->AddButton("Forward", "LayerForward");
		m_pBackward = m_pList->AddButton("Backward", "LayerBackward");
		m_pTop = m_pList->AddButton("To front", "LayerFront");
		m_pBottom = m_pList->AddButton("To back", "LayerBack");
		m_pList->AddActionSignalTarget(this);
		SetToolTip(m_pForward, "Bring the selected control forward, over the one above it in the list");
		SetToolTip(m_pBackward, "Send the selected control backward, under the one below it in the list");
		SetToolTip(m_pTop, "Draw the selected control on top of all the others");
		SetToolTip(m_pBottom, "Draw the selected control under all the others");
		SetToolTipFunc(m_pList, [this]() {
			int x, y;
			vgui2::input()->GetCursorPos(x, y);
			m_pList->ScreenToLocal(x, y);
			const int row = m_pList->RowAtPoint(x, y);
			if (row < 0 || row >= (int)m_Rows.size())
				return std::string("The controls from front to back, so the first one is drawn over all the others. Click a square to hide "
				                   "or show a control at the start");
			return std::string(StartsVisible(m_Rows[row]) ? "Click the square to hide this control at the start"
			                                               : "Click the square to show this control at the start");
		});
	}

	void Refresh()
	{
		const std::vector<int> order = DrawOrder();
		m_Rows.assign(order.rbegin(), order.rend()); // the top first
		m_pList->ClearRows();
		int sel = -1;
		for (int r = 0; r < (int)m_Rows.size(); r++)
		{
			const int sec = m_Rows[r];
			std::string text = s_Doc.Sec(sec).name + ", " + PlainKind(sec).substr(2); // "Title, text label"
			if (!StartsVisible(sec))
				text += ", hidden at the start";
			m_pList->AddRow(text.c_str(), r, StartsVisible(sec));
			if (!s_Selected.empty() && !Q_stricmp(s_Doc.Sec(sec).name.c_str(), s_Selected.c_str()))
				sel = r;
		}
		m_pList->SetSelected(sel);
		const bool any = sel >= 0;
		m_pForward->SetEnabled(any && sel > 0);
		m_pTop->SetEnabled(any && sel > 0);
		m_pBackward->SetEnabled(any && sel < (int)m_Rows.size() - 1);
		m_pBottom->SetEnabled(any && sel < (int)m_Rows.size() - 1);
		Repaint();
	}

	void Open() override
	{
		Refresh();
		BaseClass::Open();
		PlaceClear();
	}

	MESSAGE_FUNC_INT(OnListSelected, "ListSelected", index)
	{
		s_Selected = index >= 0 && index < (int)m_Rows.size() ? s_Doc.Sec(m_Rows[index]).name : std::string();
		RefreshProps();
	}
	MESSAGE_FUNC_INT(OnListIconClicked, "ListIconClicked", index)
	{
		if (index < 0 || index >= (int)m_Rows.size())
			return;
		const int sec = m_Rows[index];
		const std::string before = s_Doc.Text();
		if (StartsVisible(sec))
			s_Doc.Set(sec, "visible", "0");
		else
			s_Doc.Remove(sec, "visible"); // shown: the default
		Changed(before);
	}
	void OnCommand(const char *command) override
	{
		const int sel = m_pList->GetSelected();
		const int n = (int)m_Rows.size();
		int to = -1; // the row it goes to (0 = the top)
		if (!Q_stricmp(command, "LayerForward"))
			to = sel - 1;
		else if (!Q_stricmp(command, "LayerBackward"))
			to = sel + 1;
		else if (!Q_stricmp(command, "LayerFront"))
			to = 0;
		else if (!Q_stricmp(command, "LayerBack"))
			to = n - 1;
		else
		{
			BaseClass::OnCommand(command);
			return;
		}
		if (sel < 0 || sel >= n || to < 0 || to >= n || to == sel)
			return;
		MoveRow(sel, to);
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		m_pList->SetBounds(x, y, w, h);
	}

private:
	//! Where the window covers another editor window, it moves beside one (as the color window opens beside its
	//! swatch): right of the main window, below it, or right of or below Control Settings, made smaller down to its
	//! minimum size (half its design height, so it fits under the main window on a 1280 x 720 screen) if that is what
	//! fits; if no place fits, it stays where it opened
	void PlaceClear()
	{
		const int minW = M(300) * 2 / 3, minH = M(380) / 2;
		SetMinimumSize(minW, minH);
		struct Rect
		{
			int x, y, w, h;
		};
		std::vector<Rect> others;
		for (CSboToolWindow *w : { (CSboToolWindow *)s_pMain, (CSboToolWindow *)s_pProps, (CSboToolWindow *)s_pLoad,
		         (CSboToolWindow *)s_pSave, (CSboToolWindow *)s_pPictures })
		{
			if (!w || !w->IsOpen())
				continue;
			Rect r;
			w->GetBounds(r.x, r.y, r.w, r.h);
			others.push_back(r);
		}
		const auto overlaps = [](const Rect &a, const Rect &b) {
			return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
		};
		Rect cur;
		GetBounds(cur.x, cur.y, cur.w, cur.h);
		bool covers = false;
		for (const Rect &o : others)
			covers = covers || overlaps(cur, o);
		if (!covers)
			return;
		int scrW, scrH;
		vgui2::surface()->GetScreenSize(scrW, scrH);
		const int gap = M(8);
		std::vector<Rect> anchors; // top-left corners to try, in order
		for (CSboToolWindow *w : { (CSboToolWindow *)s_pMain, (CSboToolWindow *)s_pProps })
		{
			if (!w || !w->IsOpen())
				continue;
			int x, y, ww, hh;
			w->GetBounds(x, y, ww, hh);
			anchors.push_back({ x + ww + gap, y, 0, 0 });
			anchors.push_back({ x, y + hh + gap, 0, 0 });
		}
		for (const Rect &a : anchors)
		{
			// the window grows right and down from the corner; what is in the way makes it narrower or shorter
			Rect r = { a.x, a.y, std::min(cur.w, scrW - a.x), std::min(cur.h, scrH - a.y) };
			bool ok = r.w >= minW && r.h >= minH;
			for (const Rect &o : others)
			{
				if (!ok || !overlaps(r, o))
					continue;
				const int fitW = o.x - gap - r.x, fitH = o.y - gap - r.y;
				if (fitW >= minW && (fitH < minH || fitW * r.h >= r.w * fitH))
					r.w = fitW;
				else if (fitH >= minH)
					r.h = fitH;
				else
					ok = false;
			}
			for (const Rect &o : others)
				ok = ok && !overlaps(r, o);
			if (ok)
			{
				SetBounds(r.x, r.y, r.w, r.h);
				return;
			}
		}
	}

	static void PaintEye(bool on, int x, int y, int size)
	{
		// a filled square: shown at the start; an empty one: hidden
		const int pad = size / 5;
		if (on)
			FillRect(x + pad, y + pad, size - 2 * pad, size - 2 * pad, COL_BORDER_PRIMARY);
		OutlineRect(x + pad, y + pad, size - 2 * pad, size - 2 * pad, COL_TEXT);
	}

	//! Row from goes to row to (rows: the top first); the file gets the new order (one undo step)
	void MoveRow(int from, int to)
	{
		const std::string before = s_Doc.Text();
		const std::string name = s_Doc.Sec(m_Rows[from]).name;
		std::vector<std::string> names; // the new drawing order, the first drawn first
		for (auto it = m_Rows.rbegin(); it != m_Rows.rend(); ++it)
			names.push_back(s_Doc.Sec(*it).name);
		const int n = (int)names.size();
		const int fromDraw = n - 1 - from, toDraw = n - 1 - to;
		names.erase(names.begin() + fromDraw);
		names.insert(names.begin() + toDraw, name);
		// the sections in that order, in the places the controls' sections have (the screen's section is skipped)
		for (int k = 0; k < (int)names.size(); k++)
		{
			std::vector<int> slots;
			for (int i = 0; i < s_Doc.Count(); i++)
				if (!IsScreen(i))
					slots.push_back(i);
			const int cur = s_Doc.Find(names[k]);
			if (k < (int)slots.size() && cur >= 0 && cur != slots[k])
				s_Doc.Move(cur, slots[k]);
		}
		// zpos would sort them again: the file order is the drawing order now
		for (const std::string &nm : names)
		{
			const int sec = s_Doc.Find(nm);
			if (sec >= 0 && s_Doc.Get(sec, "zpos"))
				s_Doc.Remove(sec, "zpos");
		}
		s_Selected = name;
		Changed(before);
	}

	void Changed(const std::string &before)
	{
		if (s_Doc.Text() == before)
			return;
		PushUndo(before);
		OnDocChanged();
		UpdateTitle();
	}

	CSboOrderedList *m_pList;
	CSboBrowserButton *m_pForward, *m_pBackward, *m_pTop, *m_pBottom;
	std::vector<int> m_Rows; // section index per row, the top first
};

void RefreshLayers()
{
	if (s_pLayers && s_pLayers->IsOpen())
		s_pLayers->Refresh();
}

void OpenLayers()
{
	if (!s_pLayers)
		s_pLayers = new CWseLayersWindow();
	s_pLayers->Open();
	if (s_pLayers->IsCollapsed())
		s_pLayers->SetCollapsed(false);
}

void RefreshProps()
{
	if (s_pProps && s_pProps->IsOpen())
		s_pProps->Refresh();
	if (s_pMain)
		s_pMain->UpdateButtons();
	RefreshLayers();
}

void OnDocChanged(bool rebuild)
{
	s_bPreviewDirty = true;
	if (rebuild && s_pMain)
		s_pMain->Refresh();
	if (s_pProps && s_pProps->IsOpen())
		s_pProps->Refresh();
	RefreshLayers();
}

void UpdateTitle()
{
	s_bModified = s_Doc.Text() != s_SavedText; // an undo back to the saved text is no change
	if (s_pMain)
		s_pMain->SetToolTitle(("World Screen Editor - " + ShortName() + (s_bModified ? " *" : "")).c_str());
}

//-----------------------------------------------------------------------------
// worldscreeneditorui
//-----------------------------------------------------------------------------
void Cmd_WorldScreenEditorUI()
{
	if (!SboToolsAllowed())
	{
		sbo::ConsolePrint("The world screen editor only works in single player, in a game you host, or on a server with sv_cheats 2\n");
		return;
	}
	if (!s_pMain)
	{
		s_pMain = new CWseMainWindow();
		s_pProps = new CWsePropsWindow();
	}
	if (s_pMain->IsOpen())
	{
		// the editor's windows close together, and the preview goes (unsaved changes are asked about first)
		ConfirmDiscard([]() {
			if (s_pMain)
				s_pMain->CloseEditor();
		});
		return;
	}
	if (!s_bStarted)
	{
		// the first time: the layout of the screen in view, else a new one
		s_bStarted = true;
		const int slot = worldscreen::EditorSlotInView();
		std::string text;
		const char *panel = worldscreen::EditorSlotPanel(slot);
		wslayout::Doc check;
		if (slot >= 0 && panel[0] && ReadGameFile(LayoutPath(panel).c_str(), text) && check.SetText(text))
			StartLayout(panel, text);
		else
			StartLayout("", wslayout::NewLayoutText("new_screen"));
	}
	s_pMain->Open();
	s_pProps->Open();
	OnDocChanged(); // the main window takes the layout's size before Layers finds a free place beside it
	OpenLayers();
	UpdateTitle();
}
} // namespace

bool VGui2_WorldScreenEditor_Init()
{
	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;
	sbo::AddCommand("worldscreeneditorui", &Cmd_WorldScreenEditorUI);
	return true;
}
