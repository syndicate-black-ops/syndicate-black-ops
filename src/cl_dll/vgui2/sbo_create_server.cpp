//========= SBO VGUI2: Create server window (the new menus) =========//
//
// Purpose: see sbo_create_server.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <FileSystem.h>
#include <IBaseUI.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui/IInputInternal.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

#include "client_steam_context.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_create_server.h"
#include "sbo_loading_screen.h"
#include "sbo_map_picture.h"
#include "sbo_script_file.h"
#include "sbo_settings_window.h"
#include "sbo_ui_style.h"

#ifdef PostMessage
#undef PostMessage
#endif

using namespace sbolook;

namespace
{
enum ModeTab
{
	TAB_COOP = 0,
	TAB_BOUNTY,
	TAB_BR,
	TAB_ALL,
	TAB_COUNT
};
const char *const s_TabNames[TAB_COUNT] = { "Co-op", "Bounty Killer", "Battle Royale", "All maps" };

bool ReadFile(const char *path, std::string &out, unsigned maxBytes, const char *pathID)
{
	out.clear();
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb", pathID);
	if (!fh)
		return false;
	unsigned size = g_pFullFileSystem->Size(fh);
	if (size > maxBytes)
		size = maxBytes;
	out.resize(size);
	if (size > 0 && g_pFullFileSystem->Read(&out[0], (int)size, fh) != (int)size)
		out.clear();
	g_pFullFileSystem->Close(fh);
	return !out.empty();
}

//! GameUI's LoadMaps for a mod: maps/*.bsp of the GAME and GAMEDOWNLOAD path IDs, without ".bsp", sorted, once each
std::vector<std::string> ListMaps()
{
	std::vector<std::string> maps;
	for (const char *pathID : { "GAME", "GAMEDOWNLOAD" })
	{
		FileFindHandle_t fh;
		const char *fn = g_pFullFileSystem->FindFirst("maps/*.bsp", &fh, pathID);
		while (fn && maps.size() < 4096)
		{
			std::string s = fn;
			if (s.size() > 4 && !Q_stricmp(s.c_str() + s.size() - 4, ".bsp"))
			{
				s.erase(s.size() - 4);
				bool dup = false;
				for (const std::string &m : maps)
					dup = dup || !Q_stricmp(m.c_str(), s.c_str());
				if (!dup && !s.empty())
					maps.push_back(s);
			}
			fn = g_pFullFileSystem->FindNext(fh);
		}
		g_pFullFileSystem->FindClose(fh);
	}
	std::sort(maps.begin(), maps.end(), [](const std::string &a, const std::string &b) { return Q_stricmp(a.c_str(), b.c_str()) < 0; });
	return maps;
}

//! A LIST of exactly two items with the values 0 and 1 (a yes/no setting): the item of value 1, else -1
int YesNoItem(const sbo_script::Object &o)
{
	if (o.items.size() != 2)
		return -1;
	const double a = atof(o.items[0].value.c_str()), b = atof(o.items[1].value.c_str());
	if (a == 0.0 && b == 1.0)
		return 1;
	if (a == 1.0 && b == 0.0)
		return 0;
	return -1;
}

//! What each settings.scr setting does (the row's tooltip), from the game code that reads the cvar
//! (docs/MENUS.md, Tooltips); "" for a cvar not listed
const char *ScrTooltip(const char *cvar)
{
	static const struct
	{
		const char *cvar, *text;
	} s_Tips[] = {
		{ "hostname", "The name players see in the server list" },
		{ "sv_lan", "Only players on your local network can join" },
		{ "mp_fraglimit", "The map ends when a player reaches this many kills, 0 for no limit" },
		{ "mp_timelimit", "Minutes until the map ends, 0 for no limit" },
		{ "mp_hardcore", "Co-op only: a round is lost when this percent of the players who started it have died (0 to 100)" },
		{ "mp_juniorpowmonst", "How much damage monsters do" },
		{ "mp_junioracmar", "How much damage enemy marines do" },
		{ "mp_juniorpvmonst", "How much health monsters have" },
		{ "mp_makermon", "How many monsters the map's spawners make: half, normal, double or triple" },
		{ "mp_speedmon", "How fast the map's spawners make monsters (Off: they make none)" },
		{ "mp_friendlyfire", "Players can hurt each other" },
		{ "mp_ban", "After 5 team kills a player respawns with no weapons and 3 health" },
		{ "mp_junior", "Hits to a player's head, chest and stomach do less damage" },
		{ "mp_easymode", "Co-op only: a mission fails after 1.5 times as many rounds (10 instead of 7 on most maps)" },
		{ "mp_gmmode", "A game master can spawn and remove monsters during the game" },
		{ "mp_XP", "Players gain speed and health levels by damaging monsters" },
		{ "mp_footsteps", "Players' footsteps make sounds" },
		{ "mp_flashlight", "Players can use the flashlight" },
	};
	for (const auto &t : s_Tips)
	{
		if (!Q_stricmp(cvar, t.cvar))
			return t.text;
	}
	return "";
}

//! "<Steam name>'s Server" (GameUI's default host name, "Someone" without Steam), the name cut at a UTF-8 boundary
std::string DefaultHostName()
{
	std::string name = "Someone";
	if (ISteamFriends *pFriends = ClientSteamContext().SteamFriends())
	{
		const char *p = pFriends->GetPersonaName();
		if (p && p[0])
			name = p;
	}
	size_t cut = std::min<size_t>(name.size(), 48);
	while (cut > 0 && cut < name.size() && ((unsigned char)name[cut] & 0xC0) == 0x80)
		cut--;
	name.resize(cut);
	while (!name.empty() && name.back() == ' ')
		name.pop_back();
	return name + "'s Server";
}

//! A "#token" text through the game's localization (as VGUI2 labels do), any other text as it is
std::string LocalizedText(const std::string &text)
{
	if (text.size() > 1 && text[0] == '#' && g_pVGuiLocalize)
	{
		if (const wchar_t *w = g_pVGuiLocalize->Find(text.c_str()))
		{
			char buf[512];
			g_pVGuiLocalize->ConvertUnicodeToANSI(w, buf, sizeof(buf));
			return buf;
		}
	}
	return text;
}

//! A console argument inside quotes: no quote, no control character, no ';'
std::string QuoteSafe(const std::string &s, size_t maxLen)
{
	std::string out;
	for (char c : s)
	{
		if (c != '"' && c != ';' && (unsigned char)c >= 32 && out.size() < maxLen)
			out += c;
	}
	return out;
}
} // namespace

//-----------------------------------------------------------------------------
// The map tiles
//-----------------------------------------------------------------------------
class CSboMapGrid : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboMapGrid, vgui2::Panel);

public:
	CSboMapGrid(vgui2::Panel *pParent)
	    : BaseClass(pParent, "MapGrid")
	{
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
	}

	std::vector<std::string> m_Maps; //!< shown (filtered)
	std::string m_Selected;
	int m_iScroll = 0;
	CSboMapPictures m_Pictures;
	double m_flLastClick = 0.0;
	std::string m_LastClickMap;

	int Columns() { return std::max(1, GetWide() / S().M(150)); }
	int TileW() { return (GetWide() - (Columns() - 1) * S().M(10)) / Columns(); }
	int TileH() { return TileW() * 288 / 512 + S().M(22); }
	int RowStep() { return TileH() + S().M(10); }
	int ContentTall() { return ((int)m_Maps.size() + Columns() - 1) / Columns() * RowStep(); }

	void SetMaps(const std::vector<std::string> &maps)
	{
		m_Maps = maps;
		m_iScroll = 0;
		if (!m_Maps.empty() && std::find(m_Maps.begin(), m_Maps.end(), m_Selected) == m_Maps.end())
			m_Selected = m_Maps[0];
		Repaint();
	}

	int TileAt(int x, int y)
	{
		const int cols = Columns(), tw = TileW(), step = RowStep();
		y += m_iScroll;
		const int col = x / (tw + S().M(10)), row = y / step;
		if (col < 0 || col >= cols || x - col * (tw + S().M(10)) > tw || y - row * step > TileH())
			return -1;
		const int i = row * cols + col;
		return i >= 0 && i < (int)m_Maps.size() ? i : -1;
	}

	void OnMousePressed(vgui2::MouseCode code) override
	{
		if (code != vgui2::MOUSE_LEFT)
			return;
		RequestFocus();
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		const int i = TileAt(x, y);
		if (i < 0)
			return;
		const double now = vgui2::system()->GetCurrentTime();
		const bool dbl = m_Maps[i] == m_LastClickMap && now - m_flLastClick < 0.5;
		m_Selected = m_Maps[i];
		m_LastClickMap = m_Maps[i];
		m_flLastClick = now;
		Repaint();
		PostActionSignal(new KeyValues(dbl ? "MapActivated" : "MapSelected"));
	}

	void OnMouseWheeled(int delta) override
	{
		const int maxScroll = std::max(0, ContentTall() - GetTall());
		m_iScroll = std::max(0, std::min(maxScroll, m_iScroll - delta * RowStep() / 2));
		Repaint();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		if (m_Pictures.Poll())
			Repaint();
		if (m_Pictures.IsDownloading())
			return;
		// the downloaded picture of a map without one, one download at a time: the selected map first, then the tiles in view
		// (the browser's order; a finished download refreshes its tile)
		if (!m_Selected.empty() && m_Pictures.Has(Lower(m_Selected)))
			m_Pictures.StartDownload(Lower(m_Selected));
		const int cols = Columns(), step = RowStep();
		const int first = std::max(0, m_iScroll / std::max(1, step)) * cols;
		const int last = std::min((int)m_Maps.size(), (m_iScroll + GetTall()) / std::max(1, step) * cols + cols);
		for (int i = first; i < last && !m_Pictures.IsDownloading(); i++)
		{
			const std::string key = Lower(m_Maps[i]);
			if (m_Pictures.Has(key))
				m_Pictures.StartDownload(key);
		}
	}

	//! The texture's part that covers [x, x + w) x [y, y + h) keeping its shape, then clipped to this panel (a textured
	//! polygon is not clipped by the surface on Windows: the tiles scrolled half out drew over the tabs)
	void DrawCoverClipped(int texture, int texW, int texH, int x, int y, int w, int h, bool cover)
	{
		const float f = cover ? std::max((float)w / texW, (float)h / texH) : std::min((float)w / texW, (float)h / texH);
		const float dw = texW * f, dh = texH * f;
		const float dx = x + (w - dw) / 2, dy = y + (h - dh) / 2;
		// visible rectangle: the box, the picture and the panel
		const float x0 = std::max({ (float)x, dx, 0.0f }), y0 = std::max({ (float)y, dy, 0.0f });
		const float x1 = std::min({ (float)(x + w), dx + dw, (float)GetWide() });
		const float y1 = std::min({ (float)(y + h), dy + dh, (float)GetTall() });
		if (x1 <= x0 || y1 <= y0)
			return;
		const float s0 = (x0 - dx) / dw, s1 = (x1 - dx) / dw, t0 = (y0 - dy) / dh, t1 = (y1 - dy) / dh;
		vgui2::VGuiVertex v[4] = { vgui2::VGuiVertex((int)x0, (int)y0, s0, t0), vgui2::VGuiVertex((int)x1, (int)y0, s1, t0),
			vgui2::VGuiVertex((int)x1, (int)y1, s1, t1), vgui2::VGuiVertex((int)x0, (int)y1, s0, t1) };
		vgui2::surface()->DrawSetTexture(texture);
		vgui2::surface()->DrawSetColor(255, 255, 255, 255);
		vgui2::surface()->DrawTexturedPolygon(v, 4);
	}

	void PaintBackground() override {}
	void PaintBorder() override {}

	void Paint() override
	{
		const Style &st = S();
		const int cols = Columns(), tw = TileW(), th = TileH(), step = RowStep(), picH = th - st.M(22);
		int newPictures = 0;
		for (int i = 0; i < (int)m_Maps.size(); i++)
		{
			const int col = i % cols, row = i / cols;
			const int x = col * (tw + st.M(10)), y = row * step - m_iScroll;
			if (y + th < 0 || y > GetTall())
				continue;
			const std::string key = Lower(m_Maps[i]);
			FillRect(x, y, tw, th, COL_BG_INPUT);
			// at most two new pictures per frame (a JPEG decode each): the grid fills in over a few frames
			const CSboMapPictures::Picture *pic = nullptr;
			if (m_Pictures.Has(key) || newPictures < 2)
			{
				if (!m_Pictures.Has(key))
					newPictures++;
				pic = m_Pictures.Get(key);
			}
			else
				Repaint();
			if (pic && pic->texture != -1 && pic->wide > 0 && pic->tall > 0)
				DrawCoverClipped(pic->texture, pic->wide, pic->tall, x, y, tw, picH, pic->isMapImage || pic->isPlaceholder);
			if (!pic || pic->texture == -1 || pic->isPlaceholder)
			{
				// SBO's placeholder (or nothing yet): the map name in the middle, as the browser shows it
				const std::wstring name = Widen(m_Maps[i].c_str());
				const int nw = std::min(TextWide(st.body, name), tw - st.M(12));
				const int ny = y + (picH - FontTall(st.body)) / 2 + st.M(8);
				if (ny >= 0 && ny + FontTall(st.body) <= GetTall())
					DrawText(st.body, COL_TEXT_DIM, x + (tw - nw) / 2, ny, name, tw - st.M(12));
			}
			const bool sel = !Q_stricmp(m_Maps[i].c_str(), m_Selected.c_str());
			FillRect(x, y + picH, tw, th - picH, sel ? COL_BG_SELECTED : COL_BG_WINDOW);
			DrawTextMid(st.small, sel ? COL_TEXT_SELECTED : COL_TEXT_LIGHT, x + st.M(6), y + picH, th - picH,
			    Widen(m_Maps[i].c_str()), tw - st.M(12));
			OutlineRect(x, y, tw, th, sel ? COL_BORDER_PRIMARY : COL_BORDER_LIST);
		}
		if (m_Maps.empty())
			DrawText(st.body, COL_TEXT_DIM, 0, 0, L"No map matches");
	}
};

//-----------------------------------------------------------------------------
// The window
//-----------------------------------------------------------------------------
class CSboCreateServer : public CSboWindow
{
	DECLARE_CLASS_SIMPLE(CSboCreateServer, CSboWindow);

public:
	CSboCreateServer(vgui2::Panel *pParent);
	void OnCommand(const char *command) override;
	void Paint() override;

	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel) { Filter(); }
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel) { Repaint(); }
	MESSAGE_FUNC(OnMapActivated, "MapActivated") { Start(); }
	MESSAGE_FUNC(OnMapSelected, "MapSelected") { Repaint(); }
	MESSAGE_FUNC(OnSettingsChanged, "SettingsChanged") {}

protected:
	void OnTabChanged(int tab) override { Filter(); }
	void LayoutContent(int x, int y, int w, int h) override;

private:
	void Filter();
	void Start();
	std::string Value(const char *cvar, const char *fallback);

	std::vector<std::string> m_AllMaps;
	CSboBrowserEntry *m_pFind = nullptr;
	CSboBrowserCheck *m_pRandom = nullptr;
	CSboMapGrid *m_pGrid = nullptr;
	CSboSettingsPage *m_pSettings = nullptr;
	CSboBrowserCheck *m_pSteam = nullptr;
	int m_iFindY = 0, m_iFindTall = 0, m_iFindX = 0;

	sbo_script::Description m_Scr;
	bool m_bHaveScr = false;
	struct ScrRow
	{
		sbo_script::Object *obj;
		vgui2::Panel *control;
		int yesItem; //!< a yes/no LIST shown as a check box: the item checked means (-1: not one)
	};
	void AddSteamRow();
	std::vector<ScrRow> m_Rows;
	CSboBrowserEntry *m_pMaxPlayers = nullptr, *m_pPassword = nullptr; //!< when settings.scr has no entry for them
};

CSboCreateServer::CSboCreateServer(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboCreateServer", "Create server", "createserver", 960, 640)
{
	for (int i = 0; i < TAB_COUNT; i++)
		AddTab(s_TabNames[i]);
	AddBottomButton("Start", "Start", true);
	AddBottomButton("Cancel", "Close", false);

	m_pFind = new CSboBrowserEntry(this, "Find", "co_");
	m_pFind->SetMaximumCharCount(31);
	m_pFind->AddActionSignalTarget(this);
	m_pRandom = new CSboBrowserCheck(this, "Random", "Random map");
	m_pRandom->AddActionSignalTarget(this);
	m_pGrid = new CSboMapGrid(this);
	m_pGrid->AddActionSignalTarget(this);
	m_pSettings = new CSboSettingsPage(this, "ServerSettings");

	m_AllMaps = ListMaps();

	// settings.scr (GameUI reads it from the mod folder, GAMECONFIG)
	std::string text;
	m_bHaveScr = ReadFile("settings.scr", text, 256 * 1024, "GAMECONFIG") &&
	             m_Scr.Parse(text.data(), text.size(), sbo_script::Description::KIND_SERVER);
	m_pSettings->AddCard("Server settings");
	if (!m_bHaveScr)
		m_pSettings->AddText("settings.scr not found: the server uses its defaults");
	for (sbo_script::Object &o : m_Scr.objects)
	{
		// GameUI shows a "#token" prompt localized, any other as written (the file keeps the token)
		const std::string prompt = LocalizedText(o.prompt);
		CSboCustomSetting *s = new CSboCustomSetting();
		vgui2::Panel *control = nullptr;
		if (o.type == sbo_script::TYPE_BOOL)
		{
			CSboBrowserCheck *chk = new CSboBrowserCheck(m_pSettings, "ScrBool", ""); // the prompt wraps in the label column
			s->label = prompt;
			chk->SetChecked(atof(o.defValue.c_str()) != 0.0);
			control = chk;
			s->rowTall = 26;
		}
		else if (o.type == sbo_script::TYPE_LIST && YesNoItem(o) >= 0)
		{
			// a list of only "0" and "1" (settings.scr's "LAN server") is a yes/no setting: a check box; the file
			// keeps its LIST and the value of the item
			CSboBrowserCheck *chk = new CSboBrowserCheck(m_pSettings, "ScrYesNo", "");
			s->label = prompt;
			chk->SetChecked(o.ListRow() == YesNoItem(o));
			control = chk;
			s->rowTall = 26;
		}
		else if (o.type == sbo_script::TYPE_LIST)
		{
			CSboBrowserSelect *sel = new CSboBrowserSelect(m_pSettings, "ScrList");
			for (const sbo_script::ListItem &it : o.items)
				sel->AddItem(LocalizedText(it.label).c_str());
			sel->SetSelected(std::max(0, o.ListRow()));
			control = sel;
			s->label = prompt;
		}
		else
		{
			CSboBrowserEntry *e = new CSboBrowserEntry(m_pSettings, "ScrText", "");
			e->SetMaximumCharCount(63);
			// GameUI: the host name is always "<Steam name>'s Server" when the dialog opens
			e->SetText(!Q_stricmp(o.cvar.c_str(), "hostname") ? DefaultHostName().c_str()
			           : (o.type == sbo_script::TYPE_NUMBER ? sbo_script::NumberText(o.defValue).c_str() : o.defValue.c_str()));
			control = e;
			s->label = prompt;
		}
		s->control = control;
		s->tooltip = ScrTooltip(o.cvar.c_str());
		m_pSettings->Add(s);
		m_Rows.push_back({ &o, control, o.type == sbo_script::TYPE_LIST ? YesNoItem(o) : -1 });
		// Steam networking right under LAN server, the setting it goes with
		if (!m_pSteam && !Q_stricmp(o.cvar.c_str(), "sv_lan"))
			AddSteamRow();
	}
	// GameUI takes max players and the password from settings.scr entries too, else "32" and ""; SBO's settings.scr
	// has none, so they get rows of their own with those values
	if (!m_Scr.Find("maxplayers"))
	{
		CSboCustomSetting *s = new CSboCustomSetting();
		s->label = "Max players";
		s->tooltip = "How many players can join";
		m_pMaxPlayers = new CSboBrowserEntry(m_pSettings, "MaxPlayers", "");
		m_pMaxPlayers->SetMaximumCharCount(2);
		m_pMaxPlayers->SetText("32");
		s->control = m_pMaxPlayers;
		s->controlWide = 80;
		m_pSettings->Add(s);
	}
	if (!m_Scr.Find("sv_password"))
	{
		CSboCustomSetting *s = new CSboCustomSetting();
		s->label = "Password";
		s->tooltip = "Players need this password to join; leave empty for none";
		m_pPassword = new CSboBrowserEntry(m_pSettings, "Password", "none");
		m_pPassword->SetMaximumCharCount(31);
		m_pPassword->SetTextHidden(true);
		s->control = m_pPassword;
		m_pSettings->Add(s);
	}
	if (!m_pSteam)
		AddSteamRow();

	// the tab of the map chosen last time (settings kept by the window: sbomenus.vdf)
	SetTab(TAB_COOP);
}

void CSboCreateServer::AddSteamRow()
{
	// GameUI's "Enable Steam Networking": its state from sv_use_steam_networking when the engine has it
	// laid out as the LAN server row above it: the wording in the label column, the box in the control column
	CSboCustomSetting *s = new CSboCustomSetting();
	s->label = "Enable Steam Networking";
	s->tooltip = "Players join through Steam";
	m_pSteam = new CSboBrowserCheck(m_pSettings, "Steam", "");
	sbo::CvarHandle h = sbo::FindCvar("sv_use_steam_networking");
	m_pSteam->SetChecked(h && sbo::CvarGetFloat(h) != 0.0f);
	s->control = m_pSteam;
	s->rowTall = 26;
	m_pSettings->Add(s);
}

void CSboCreateServer::Filter()
{
	char find[64];
	m_pFind->GetText(find, sizeof(find));
	const std::string f = Lower(find);
	std::vector<std::string> shown;
	for (const std::string &m : m_AllMaps)
	{
		const int mode = ModeOfMap(m.c_str());
		switch (GetTab())
		{
		case TAB_COOP:
			if (mode != SboServerRow::MODE_COOP)
				continue;
			break;
		case TAB_BOUNTY:
			if (mode != SboServerRow::MODE_BOUNTY)
				continue;
			break;
		case TAB_BR:
			if (mode != SboServerRow::MODE_BR)
				continue;
			break;
		default:
			break;
		}
		if (!f.empty() && Lower(m).find(f) == std::string::npos)
			continue;
		shown.push_back(m);
	}
	m_pGrid->SetMaps(shown);
}

void CSboCreateServer::LayoutContent(int x, int y, int w, int h)
{
	const Style &st = S();
	const int rightW = std::max(st.M(300), w * 36 / 100);
	const int leftW = w - rightW - st.M(16);
	const int rowTall = st.M(28);
	m_iFindX = x;
	m_iFindY = y;
	m_iFindTall = rowTall;
	const int labelW = st.M(40);
	m_pFind->SetBounds(x + labelW, y, std::min(st.M(200), leftW - labelW - st.M(140)), rowTall);
	m_pRandom->SetBounds(m_pFind->GetXPos() + m_pFind->GetWide() + st.M(12), y, m_pRandom->GetContentWide(), rowTall);
	m_pGrid->SetBounds(x, y + rowTall + st.M(12), leftW, h - rowTall - st.M(12));
	m_pSettings->SetBounds(x + leftW + st.M(16), y, rightW, h);
	m_pSettings->InvalidateLayout();
}

void CSboCreateServer::Paint()
{
	BaseClass::Paint();
	DrawTextMid(S().body, COL_TEXT_LIGHT, m_iFindX, m_iFindY, m_iFindTall, L"Find");
}

std::string CSboCreateServer::Value(const char *cvar, const char *fallback)
{
	for (const ScrRow &r : m_Rows)
	{
		if (!Q_stricmp(r.obj->cvar.c_str(), cvar))
			return r.obj->curValue;
	}
	return fallback;
}

// The page's values for the game DLL's cvars that did not exist yet at Start (the DLL loads with the first map).
// Host_Map_f loads the DLL, then paints the loading screen before it spawns the server, so set from the loading
// screen's paint they are in place before the map's entities spawn and before listenserver.cfg runs (which still
// overrides them, as for a server started later in the session).
static std::vector<std::pair<std::string, std::string>> s_PendingCvars;

void VGui2_CreateServer_ApplyPending()
{
	if (s_PendingCvars.empty() || !sbo::FindCvar(s_PendingCvars.front().first.c_str()))
		return;
	for (const auto &cv : s_PendingCvars)
	{
		if (sbo::FindCvar(cv.first.c_str()))
			sbo::CvarSetString(cv.first.c_str(), cv.second.c_str());
	}
	s_PendingCvars.clear();
}

void CSboCreateServer::Start()
{
	// 1. the page's values (GameUI's GatherCurrentValues), to the engine and into settings.scr. A LIST's value is
	// its row (the file's format: "LIST is index"; ConfigValue sends that row's item value), so an item whose value
	// is not its row (mp_speedmon "Off" "-1") reaches the server as written; GameUI stores the item's value there
	for (ScrRow &r : m_Rows)
	{
		sbo_script::Object &o = *r.obj;
		if (o.type == sbo_script::TYPE_BOOL)
			o.SetCurValue(static_cast<CSboBrowserCheck *>(r.control)->IsChecked() ? "1" : "0");
		else if (o.type == sbo_script::TYPE_LIST && r.yesItem >= 0)
		{
			const bool checked = static_cast<CSboBrowserCheck *>(r.control)->IsChecked();
			o.SetCurValue(std::to_string(checked ? r.yesItem : 1 - r.yesItem));
		}
		else if (o.type == sbo_script::TYPE_LIST)
		{
			const int row = static_cast<CSboBrowserSelect *>(r.control)->GetSelected();
			o.SetCurValue(row >= 0 && row < (int)o.items.size() ? std::to_string(row) : o.defValue);
		}
		else
		{
			char buf[256];
			static_cast<CSboBrowserEntry *>(r.control)->GetText(buf, sizeof(buf));
			o.SetCurValue(buf);
		}
	}
	// The engine loads the game DLL with the first "map" of the session (Host_Map_f: Host_InitializeGameDLL), so
	// until then the game's own cvars (mp_gmmode, mp_XP, ...) do not exist and a value sent now is lost. Those are
	// kept and set by VGui2_CreateServer_ApplyPending as soon as the DLL has registered them, during the map load.
	s_PendingCvars.clear();
	if (m_bHaveScr)
	{
		for (const sbo_script::Object &o : m_Scr.objects)
		{
			sbo::ExecClientCmd(o.ConfigCommand().c_str());
			if (!o.setInfo && !sbo::FindCvar(o.cvar.c_str()))
				s_PendingCvars.emplace_back(o.cvar, o.ConfigValue());
		}
		const std::string out = m_Scr.Write(sbo_script::FileTime((long long)time(nullptr), sbo_script::Description::KIND_SERVER));
		FileHandle_t fh = g_pFullFileSystem->Open("settings.scr", "wb"); // GameUI: path ID NULL (the write path)
		if (fh)
		{
			g_pFullFileSystem->Write(out.data(), (int)out.size(), fh);
			g_pFullFileSystem->Close(fh);
		}
	}

	// 2. the map (a random one of the list shown for "Random map"), the server's start
	std::string map = m_pGrid->m_Selected;
	if (m_pRandom->IsChecked() && !m_pGrid->m_Maps.empty())
	{
		srand((unsigned)time(nullptr));
		map = m_pGrid->m_Maps[rand() % m_pGrid->m_Maps.size()];
	}
	if (map.empty() || !SafeMapName(map))
		return;
	std::string maxPlayers = Value("maxplayers", "32"), password = Value("sv_password", ""), host = Value("hostname", "Half-Life");
	if (m_pMaxPlayers)
	{
		char buf[16];
		m_pMaxPlayers->GetText(buf, sizeof(buf));
		maxPlayers = buf;
	}
	if (m_pPassword)
	{
		char buf[64];
		m_pPassword->GetText(buf, sizeof(buf));
		password = buf;
	}
	const int steam = m_pSteam->IsChecked() ? 1 : 0;
	// GameUI's command sets sv_lan from the Steam box alone (sv_lan = not Steam networking), which left settings.scr's
	// "LAN server" without effect; a ticked LAN server box makes it a LAN server
	bool lan = !steam;
	for (const ScrRow &r : m_Rows)
	{
		if (!Q_stricmp(r.obj->cvar.c_str(), "sv_lan") && atof(r.obj->ConfigValue().c_str()) != 0.0)
			lan = true;
	}
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
	    "disconnect\nsv_lan %d\nsv_use_steam_networking %d\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname "
	    "\"%s\"\ncd fadeout\nmap %s\n",
	    lan ? 1 : 0, steam, (int)strtol(maxPlayers.c_str(), nullptr, 10), QuoteSafe(password, 63).c_str(),
	    QuoteSafe(host, 63).c_str(), map.c_str());
	VGui2_LoadingScreen_StartingLocal(map.c_str()); // the joining screen shows this game, not the server left
	sbo::ExecClientCmd(cmd);
	sbo::ExecClientCmd("wait\nwait\n");
	Close();
}

void CSboCreateServer::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "Start"))
	{
		Start();
		return;
	}
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
static vgui2::DHANDLE<CSboCreateServer> s_hWindow;
static bool s_bOpenNextFrame = false;

// gameui_createserver: the game menu's Create server entry ("engine gameui_createserver", which hides GameUI: shown
// again and the window opened on the next frame). cl_experimental_menus 0: GameUI's Create Server dialog.
static void CreateServer_f()
{
	if (!g_pGameUIViewport)
		return;
	if (g_pBaseUI)
		g_pBaseUI->ActivateGameUI();
	if (!SboExperimentalMenus())
	{
		SboOpenGameUIDialog("OpenCreateMultiplayerGameDialog");
		return;
	}
	s_bOpenNextFrame = true;
}

void VGui2_CreateServer_Frame()
{
	if (!s_bOpenNextFrame || !g_pGameUIViewport)
		return;
	s_bOpenNextFrame = false;
	if (!s_hWindow.Get())
		s_hWindow = new CSboCreateServer(g_pGameUIViewport);
	s_hWindow->Open();
}

bool VGui2_CreateServer_CloseIfOpen()
{
	CSboCreateServer *p = s_hWindow.Get();
	if (!p || !p->IsVisible())
		return false;
	s_hWindow = nullptr; // it deletes itself on close; the next open makes a new one
	p->Close();
	return true;
}

void VGui2_RegisterCreateServerCommand()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	sbo::AddCommand("gameui_createserver", CreateServer_f);
}
