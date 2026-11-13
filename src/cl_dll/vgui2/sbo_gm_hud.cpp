//========= SBO VGUI2: Game Master mode HUD and players window =========//
//
// Purpose: the GM screens 1, 3 and 4 (docs/GM_MODE.md "Screen and
//          menus"), drawn by the client from the GmStatus / GmPlayers messages (server gmmode.cpp GmUpdateHud):
//   * CSboGmHud: the status strip (top left), nearest players (top right), the placement line under the crosshair
//     and the key hints (bottom center), while the Game Master flies and no GM window is open;
//   * CSboGmBoxSelect: Fire held and dragged draws the selection box on the screen (the mouse draws it, the view
//     stays); on release "gm_select" sends it to the server, which takes the monsters he can select inside it;
//   * CSboGmPlayersWindow: the players window: the level's overview map with the players' dots and the Game Master's
//     triangle, Level buttons per overview layer, Player / Health / Distance / Weapon rows (the living first, the dead
//     below with "Killed by your <monster>"), Go to / Follow / View through eyes / Spawn near.
// Fields without data (no budget plugin: points, tier, next spawn) are not drawn.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <algorithm>
#include <map>

#include <FileSystem.h>
#include <IGameUIFuncs.h>
#include <wctype.h>
#include <KeyValues.h>
#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "avatar_image.h"
#include "client_steam_context.h"
#include "sbo_bridge.h"
#include "sbo_gm_menu.h"
#include "sbo_gm_spawn_menu.h"
#include "sbo_gm_ui.h"
#include "sbo_image_decode.h"
#include "sbo_menus.h"
#include "vgui2_api.h"

#include "../../game_shared/gm_catalog.h"

//=============================================================================
// Drawing helpers
//=============================================================================
namespace gmui
{
namespace
{
constexpr const char FONT_FACE[] = "Tahoma"; // the design's face (Linux: fontconfig's match when it is missing)

Status s_Status;
std::wstring s_GuideTitle;
std::vector<PlayerRow> s_Players, s_PlayersIn;
std::vector<MonsterRow> s_Monsters, s_MonstersIn;
// the players window's View (VGui2_GmPip): the player and the picture's screen rectangle, 0: none
int s_iPipTarget = 0, s_PipRect[4] = {};
} // namespace

int S(float px)
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	return (int)floorf(px * sh / 720.0f + 0.5f);
}

int HintsBarTall()
{
	return S(22); // the design's bar
}

int HintsBarTop(int bottom)
{
	return bottom - S(16) - HintsBarTall(); // 16 px over the bottom
}

vgui2::HFont Font(float px, bool bold)
{
	struct Cached
	{
		int tall;
		int weight;
		vgui2::HFont font;
	};
	static Cached s_Fonts[32];
	static int s_nFonts = 0;
	// the design's CSS px are the em size, as a VGUI2 font's tall (the glyphs' pixel size)
	int tall = S(px);
	if (tall < 6)
		tall = 6;
	int weight = bold ? 700 : 400;
	for (int i = 0; i < s_nFonts; i++)
	{
		if (s_Fonts[i].tall == tall && s_Fonts[i].weight == weight)
			return s_Fonts[i].font;
	}
	vgui2::HFont font = vgui2::surface()->CreateFont();
	vgui2::surface()->AddGlyphSetToFont(font, FONT_FACE, tall, weight, 0, 0, vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0);
	if (s_nFonts < 32)
		s_Fonts[s_nFonts++] = { tall, weight, font };
	return font;
}

int FontTall(vgui2::HFont font)
{
	return vgui2::surface()->GetFontTall(font);
}

int TextWide(vgui2::HFont font, const wchar_t *text)
{
	int w = 0;
	for (const wchar_t *p = text; *p; p++)
		w += vgui2::surface()->GetCharacterWidth(font, *p);
	return w;
}

int Text(vgui2::HFont font, int x, int y, const Color &color, const wchar_t *text)
{
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(color);
	vgui2::surface()->DrawSetTextPos(x, y);
	vgui2::surface()->DrawPrintText(text, (int)wcslen(text));
	return TextWide(font, text);
}

int EnergyBar(int x, int y, int w, int h, int cost)
{
	const Status &st = GetStatus();
	Fill(x, y, w, h, INPUT_BG);
	Outline(x, y, w, h, BORDER_DIM);
	const int ix = x + 1, iy = y + 1, iw = w - 2, ih = h - 2;
	const int maxPts = std::max(1, st.maxPoints);
	auto px = [&](int pts) { return (int)((float)iw * std::min(std::max(pts, 0), maxPts) / maxPts + 0.5f); };
	const int have = std::max(0, st.points);
	const float frac = (float)have / maxPts;
	const Color fill = frac < 0.10f ? BAR_RED : (frac < 0.25f ? ACCENT : BAR_FILL);
	const bool over = cost > have;
	const int keep = cost > 0 && !over ? have - cost : have;
	const int fillEnd = px(keep);
	Fill(ix, iy, fillEnd, ih, fill);
	if (cost > 0 && !over)
		Fill(ix + fillEnd, iy, px(have) - fillEnd, ih, ACCENT); // what it costs, taken off the end
	else if (cost > 0)
		Fill(ix + px(have), iy, std::max(1, px(cost) - px(have)), ih, BAR_RED); // more than there is
	else if (st.refill > 0 && have < maxPts && st.refillInterval > 0)
	{
		// the next refill coming in: grows until it lands
		const float left = std::max(0.0f, st.refillEnd - sbo::GetHudTime());
		const float grown = 1.0f - std::min(1.0f, left / st.refillInterval);
		const int sw = (int)(px(have + st.refill) - px(have)) * grown;
		Fill(ix + px(have), iy, sw, ih, Color(0x7f, 0xd6, 0x5f, 56));
	}
	return ix + px(have);
}

void CloseButton(int x, int y, int w, int h)
{
	// the design's close button: a 32 px box, a centered 16 px "X" (two strokes about 1.5 px wide, 10 px across)
	Outline(x, y, w, h, BORDER);
	const int half = std::max(3, S(5)), cx = x + w / 2, cy = y + h / 2;
	const int t = std::max(1, S(1.5f)); // the 16 px glyph's stroke: 2 px at 1080 lines
	vgui2::surface()->DrawSetColor(GREEN);
	for (int i = 0; i < t; i++)
	{
		const int o = i - (t - 1) / 2;
		vgui2::surface()->DrawLine(cx - half + o, cy - half, cx + half + o, cy + half);
		vgui2::surface()->DrawLine(cx + half + o, cy - half, cx - half + o, cy + half);
	}
}

void Fill(int x, int y, int w, int h, const Color &color)
{
	vgui2::surface()->DrawSetColor(color);
	vgui2::surface()->DrawFilledRect(x, y, x + w, y + h);
}

void Outline(int x, int y, int w, int h, const Color &color, int thickness)
{
	vgui2::surface()->DrawSetColor(color);
	for (int i = 0; i < thickness; i++)
		vgui2::surface()->DrawOutlinedRect(x + i, y + i, x + w - i, y + h - i);
}

void Dashed(int x, int y, int w, int h, const Color &color, int dash, int gap)
{
	vgui2::surface()->DrawSetColor(color);
	if (dash < 1)
		dash = 1;
	for (int px = x; px < x + w; px += dash + gap)
	{
		int e = std::min(px + dash, x + w);
		vgui2::surface()->DrawFilledRect(px, y, e, y + 1);
		vgui2::surface()->DrawFilledRect(px, y + h - 1, e, y + h);
	}
	for (int py = y; py < y + h; py += dash + gap)
	{
		int e = std::min(py + dash, y + h);
		vgui2::surface()->DrawFilledRect(x, py, x + 1, e);
		vgui2::surface()->DrawFilledRect(x + w - 1, py, x + w, e);
	}
}

std::wstring Widen(const char *utf8)
{
	wchar_t buf[512];
	sboui::TextToWide(utf8 ? utf8 : "", buf, sizeof(buf));
	return buf;
}

const Status &GetStatus()
{
	return s_Status;
}

const std::wstring &GuideTitle()
{
	return s_GuideTitle;
}

void SetGuideTitle(const std::wstring &title)
{
	s_GuideTitle = title;
}

bool HasEconomy()
{
	return s_Status.received && (s_Status.flags & GM_STATUS_ECONOMY);
}

int SecondsLeft(float end)
{
	if (end <= 0)
		return 0;
	float left = end - sbo::GetHudTime();
	return left > 0 ? (int)ceilf(left) : 0;
}

const std::vector<MonsterRow> &GetMonsters()
{
	return s_Monsters;
}

const std::vector<PlayerRow> &GetPlayers()
{
	return s_Players;
}

std::wstring PlayerName(int index)
{
	sbo::ScorePlayer sp;
	sbo::GetScorePlayer(index, sp, false);
	return Widen(VGui2_Chat_RemoveColorCodes(sp.name ? sp.name : ""));
}

std::wstring KeyFor(const char *command, const wchar_t *fallback)
{
	// the command's primary key, the one Options > Keyboard shows in its Key column: GameUI lists the keys in key
	// number order and each later key takes the Key column (UP ARROW over W, MOUSE1 over ENTER, MOUSE3 over SPACE),
	// so the highest numbered key bound to exactly this command; as the engine names it, in capitals; unbound: the
	// fallback, else the command's name
	const char *key = nullptr;
	if (g_pGameUIFuncs)
	{
		for (int k = 255; k >= 0 && !key; k--)
		{
			const char *binding = g_pGameUIFuncs->Key_BindingForKey(k);
			if (binding && !_stricmp(binding, command))
				key = g_pGameUIFuncs->Key_NameForKey(k);
		}
	}
	else
		key = sbo::LookupKeyBinding(command);
	std::wstring w = (key && key[0]) ? Widen(key) : (fallback ? std::wstring(fallback) : Widen(command));
	for (auto &c : w)
		c = (wchar_t)towupper(c);
	return w;
}

std::wstring PlayersPath()
{
	// the players window is the quick menu's 3, in plain words with the player's own primary key for the quick menu
	return L"Press " + KeyFor("gmon", L"K") + L", then choose option 3";
}

int NearestPitch()
{
	return std::max(S(15), FontTall(Font(12)) + S(3));
}

int NearestPanelTall(int rows)
{
	// PaintNearest: 10 px padding, the header line, 6 px, the rows, 6 px, the hint line, 10 px
	const int tall = FontTall(Font(12));
	return S(10) + tall + S(6) + NearestPitch() * rows + S(6) + tall + S(10);
}

bool AnyWindowOpen()
{
	return (CSboGmSpawnMenu::Get() && CSboGmSpawnMenu::Get()->IsOpen()) || GmUi_PlayersOpen()
	    || (CSboGmMenu::Get() && CSboGmMenu::Get()->IsVisible());
}

// network
namespace
{
struct Reader
{
	const unsigned char *data;
	int size;
	int pos;
	bool bad = false;
	int Byte()
	{
		if (pos + 1 > size)
		{
			bad = true;
			return 0;
		}
		return data[pos++];
	}
	int Short()
	{
		int lo = Byte(), hi = Byte();
		return (short)(lo | (hi << 8));
	}
	int Long()
	{
		int a = Byte(), b = Byte(), c = Byte(), d = Byte();
		return (int)((unsigned)a | ((unsigned)b << 8) | ((unsigned)c << 16) | ((unsigned)d << 24));
	}
};

int MsgFunc_GmStatus(const char *pszName, int iSize, void *pbuf)
{
	Reader r = { (const unsigned char *)pbuf, iSize, 0 };
	Status st;
	st.received = true;
	st.flags = r.Byte();
	st.points = r.Long();
	st.tier = r.Byte();
	int tierLeft = r.Short();
	int spawnLeft = r.Short();
	st.alive = r.Byte();
	st.selected = r.Byte();
	st.roamSpeed = r.Short();
	st.maxPoints = r.Short();
	st.refill = r.Byte();
	const int interval10 = r.Short();
	const int refill10 = r.Short();
	const int pop = r.Short();
	if (r.bad)
		return 1;
	st.refillInterval = interval10 / 10.0f;
	st.killPop = s_Status.killPop;
	st.killPopTime = s_Status.killPopTime;
	if (pop > 0)
	{
		st.killPop = pop;
		st.killPopTime = sbo::GetHudTime();
	}
	const float now = sbo::GetHudTime();
	st.tierEnd = tierLeft > 0 ? now + tierLeft : 0;
	st.spawnEnd = spawnLeft > 0 ? now + spawnLeft : 0;
	st.refillEnd = refill10 > 0 ? now + refill10 / 10.0f : 0;
	if (st.tier < 0 || st.tier > 3)
		st.tier = 0;
	s_Status = st;
	return 1;
}

// "GmNotice": a short line under the crosshair for GM_NOTICE_TIME seconds (refusals, an action's feedback)
std::wstring s_Notice;
float s_flNoticeEnd = 0;

int MsgFunc_GmNotice(const char *pszName, int iSize, void *pbuf)
{
	char text[GM_NOTICE_LEN];
	const int n = std::min(iSize, (int)sizeof(text) - 1);
	memcpy(text, pbuf, n > 0 ? n : 0);
	text[n > 0 ? n : 0] = 0;
	s_Notice = Widen(text);
	s_flNoticeEnd = s_Notice.empty() ? 0 : sbo::GetHudTime() + GM_NOTICE_TIME;
	GmUi_ShowNotice();
	return 1;
}

int MsgFunc_GmPlayers(const char *pszName, int iSize, void *pbuf)
{
	Reader r = { (const unsigned char *)pbuf, iSize, 0 };
	const int part = r.Byte();
	int rows = r.Byte();
	if (r.bad)
		return 1;
	if (part & GM_PLAYERS_FIRST)
		s_PlayersIn.clear();
	if (rows > GM_PLAYERS_PER_MSG)
		rows = GM_PLAYERS_PER_MSG;
	const int maxClients = sbo::GetMaxClients();
	for (int i = 0; i < rows; i++)
	{
		PlayerRow row;
		row.index = r.Byte();
		row.alive = (r.Byte() & GM_PLAYER_ALIVE) != 0;
		row.health = r.Byte();
		row.meters = r.Short();
		row.pos[0] = (float)r.Short();
		row.pos[1] = (float)r.Short();
		row.pos[2] = (float)r.Short();
		row.weapon = r.Byte();
		row.killedBy = r.Byte();
		if (r.bad)
			return 1;
		if (row.index < 1 || row.index > maxClients || s_PlayersIn.size() >= 32)
			continue;
		if (row.weapon >= GM_CATALOG_COUNT)
			row.weapon = 255;
		if (row.killedBy >= GM_CATALOG_COUNT)
			row.killedBy = 255;
		s_PlayersIn.push_back(row);
	}
	if (part & GM_PLAYERS_LAST)
		s_Players = s_PlayersIn;
	return 1;
}

int MsgFunc_GmMonsters(const char *pszName, int iSize, void *pbuf)
{
	Reader r = { (const unsigned char *)pbuf, iSize, 0 };
	const int part = r.Byte();
	int rows = r.Byte();
	if (r.bad)
		return 1;
	if (part & GM_PLAYERS_FIRST)
		s_MonstersIn.clear();
	rows = std::min(rows, GM_MONSTERS_PER_MSG);
	for (int i = 0; i < rows; i++)
	{
		MonsterRow m;
		m.entity = r.Short() & 0xffff;
		m.pos[0] = (float)r.Short();
		m.pos[1] = (float)r.Short();
		m.pos[2] = (float)r.Short();
		m.entry = r.Byte();
		m.health = r.Byte();
		m.flags = r.Byte();
		if (r.bad)
			return 1;
		if (s_MonstersIn.size() < GM_MONSTERS_MAX)
			s_MonstersIn.push_back(m);
	}
	if (part & GM_PLAYERS_LAST)
		s_Monsters = s_MonstersIn;
	return 1;
}
} // namespace

void ResetData()
{
	s_Status = Status();
	s_Players.clear();
	s_PlayersIn.clear();
}
} // namespace gmui

using namespace gmui;

//=============================================================================
// CSboGmBoxSelect: Fire held, the mouse draws the box
//=============================================================================
class CSboGmBoxSelect : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboGmBoxSelect, vgui2::Panel);

public:
	CSboGmBoxSelect(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboGmBoxSelect")
	{
		MakePopup(false, false);
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetVisible(false);
		vgui2::ivgui()->AddTickSignal(GetVPanel());
	}

	bool IsActive() const { return m_bActive; }

	void Start()
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
		m_iStartX = sw / 2; // the crosshair
		m_iStartY = sh / 2;
		m_bActive = true;
		SetVisible(true);
		SetMouseInputEnabled(true);
		MoveToFront();
		sbo::ReleaseMouseForCursor();
		vgui2::input()->SetCursorPos(m_iStartX, m_iStartY);
		vgui2::surface()->CalculateMouseVisible();
	}

	void Finish(bool send)
	{
		if (!m_bActive)
			return;
		m_bActive = false;
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		SetMouseInputEnabled(false);
		SetVisible(false);
		vgui2::surface()->CalculateMouseVisible();
		if (!send)
			return;

		// -1..1 from the screen's center (y up); the view's half extents at distance 1: tan(fov/2) for a 4:3 screen,
		// widened for a wider one (HL25 keeps the vertical view)
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const float tanY = tanf(sbo::GetLocalFov() * 0.5f * 3.14159265f / 180.0f) * 0.75f;
		const float tanX = tanY * sw / (float)sh;
		auto nx = [&](int px) { return (px - sw * 0.5f) / (sw * 0.5f); };
		auto ny = [&](int py) { return -(py - sh * 0.5f) / (sh * 0.5f); };
		char cmd[160];
		snprintf(cmd, sizeof(cmd), "cmd gm_select %.4f %.4f %.4f %.4f %.4f %.4f", nx(m_iStartX), ny(m_iStartY), nx(x),
		    ny(y), tanX, tanY);
		sbo::ExecClientCmd(cmd);
	}

	virtual void OnTick() override
	{
		if (!m_bActive)
			return;
		// the button that started it is up (its release went to VGUI2, not the engine)
		if (!vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT))
			Finish(true);
	}

	virtual void OnMouseReleased(vgui2::MouseCode code) override
	{
		if (code == vgui2::MOUSE_LEFT)
			Finish(true);
	}

	virtual void Paint() override
	{
		if (!m_bActive)
			return;
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		int x0 = std::min(x, m_iStartX), y0 = std::min(y, m_iStartY);
		int w = abs(x - m_iStartX) + 1, h = abs(y - m_iStartY) + 1;
		// the design: 1px dashed #a6ec8a, rgba(166,236,138,0.06)
		Fill(x0, y0, w, h, Color(0xa6, 0xec, 0x8a, 15));
		Dashed(x0, y0, w, h, GREEN_LIGHT, S(4), S(3));
	}

private:
	bool m_bActive = false;
	int m_iStartX = 0, m_iStartY = 0;
};

static CSboGmBoxSelect *s_pBoxSelect = nullptr;

//=============================================================================
// CSboGmHud: the flying Game Master's strip, nearest players, placement line and key hints
//=============================================================================
class CSboGmHud : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboGmHud, vgui2::Panel);

public:
	CSboGmHud(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboGmHud")
	{
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetVisible(true);
	}

	virtual void OnThink() override
	{
		int wide, tall, parentWide, parentTall;
		GetSize(wide, tall);
		vgui2::ipanel()->GetSize(GetVParent(), parentWide, parentTall);
		if (wide != parentWide || tall != parentTall)
			SetBounds(0, 0, parentWide, parentTall);

		// Esc while placing cancels the placement, not opening the game menu: HL25 opens GameUI on Esc before the
		// client sees the key, so the viewport's menu Esc guard hides it again (as the server menus do)
		const bool guard = GetState() == GM_STATE_ACTIVE && IsPlacing() && !AnyWindowOpen();
		if (g_pGameUIViewport)
		{
			if (guard != m_bEscGuard)
				g_pGameUIViewport->SetMenuEscapeGuard(guard);
			m_bEscGuard = guard;
			if (guard && g_pGameUIViewport->TakeMenuEscape())
				sbo::ExecClientCmd("cmd gm_cancel quick"); // as Reload: back to the quick menu
		}
	}

	static bool Flying()
	{
		return GetState() == GM_STATE_ACTIVE && !(GetStatus().flags & GM_STATUS_FOLLOWING);
	}

	virtual void Paint() override
	{
		// in a spectator mode (chase, first person, overview) the spectator's own HUD is up
		if (GetState() != GM_STATE_ACTIVE || AnyWindowOpen() || !sbo::KillFeedShouldDraw() || !Flying())
			return;
		PaintCrosshair();
		PaintStrip();
		PaintNearest();
		if (IsPlacing())
			PaintPlacement(); // its second line has the keys: no hints bar under it
		else
			PaintHints();
	}

private:
	bool m_bEscGuard = false;

	// a flying observer has no weapon crosshair: the Game Master aims with this one (placing, the box's first corner,
	// move orders), at the screen's center, the standard crosshair's green
	void PaintCrosshair()
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int cx = sw / 2, cy = sh / 2, gap = S(3), len = S(6), t = std::max(1, S(1.4f));
		const Color c(50, 250, 50, 230);
		Fill(cx - gap - len, cy - t / 2, len, t, c);
		Fill(cx + gap + 1, cy - t / 2, len, t, c);
		Fill(cx - t / 2, cy - gap - len, t, len, c);
		Fill(cx - t / 2, cy + gap + 1, t, len, c);
	}

	// "GAME MASTER | [points] pts  Tier [n] | Next spawn in [s]s | Alive: [n] of your monsters"; with a selection
	// "GAME MASTER | [points] pts  Tier [n] | [n] selected"
	void PaintStrip()
	{
		struct Item
		{
			std::wstring text;
			vgui2::HFont font;
			Color color;
			bool sepBefore;
			int barW = 0; // the energy bar before the text
		};
		std::vector<Item> items;
		const Status &st = GetStatus();
		items.push_back({ L"GAME MASTER", Font(12), ACCENT, false });
		wchar_t buf[96];
		if (HasEconomy())
		{
			if (st.maxPoints > 0)
			{
				swprintf(buf, 96, L"%d / %d", st.points, st.maxPoints);
				Item bar = { buf, Font(12), VALUE, true };
				bar.barW = S(160);
				items.push_back(bar);
			}
			else
			{
				swprintf(buf, 96, L"%d pts", st.points);
				items.push_back({ buf, Font(13), BRIGHT, true });
			}
			if (st.tier >= 1 && st.tier <= 3)
			{
				static const wchar_t *const s_Tiers[3] = { L"Tier I", L"Tier II", L"Tier III" }; // as the tabs
				items.push_back({ s_Tiers[st.tier - 1], Font(12), DIM, false });
			}
		}
		if (st.selected > 0)
		{
			swprintf(buf, 96, L"%d selected", st.selected);
			items.push_back({ buf, Font(12), GREEN_LIGHT, true });
		}
		else
		{
			const int spawn = SecondsLeft(st.spawnEnd);
			if (HasEconomy() && spawn > 0)
			{
				swprintf(buf, 96, L"Next spawn in %ds", spawn);
				items.push_back({ buf, Font(12), VALUE, true });
			}
			swprintf(buf, 96, L"Alive: %d of your monsters", st.alive);
			items.push_back({ buf, Font(12), VALUE, true });
		}

		// the design's strip: 36 px tall (padding 8, the 18 px separators), 14 px padding and gaps
		const int gap = S(14), padX = S(14), sepTall = S(18);
		int w = padX * 2;
		for (size_t i = 0; i < items.size(); i++)
		{
			if (i)
				w += gap;
			if (items[i].sepBefore)
				w += 1 + gap;
			w += TextWide(items[i].font, items[i].text.c_str()) + (items[i].barW ? items[i].barW + S(8) : 0);
		}
		const int x0 = S(16), y0 = S(16), h = S(36);
		Fill(x0, y0, w, h, BG_STRIP);
		Outline(x0, y0, w, h, BORDER);
		int x = x0 + padX;
		for (size_t i = 0; i < items.size(); i++)
		{
			if (i)
				x += gap;
			if (items[i].sepBefore)
			{
				Fill(x, y0 + (h - sepTall) / 2, 1, sepTall, BORDER_DIM);
				x += 1 + gap;
			}
			if (items[i].barW)
			{
				// the design: 160 x 10, "96 / 140" 8 px after it, a kill reward's "+N" over the fill's end for 1.5 s
				const int bh = S(10), by = y0 + (h - bh) / 2;
				const int end = EnergyBar(x, by, items[i].barW, bh, 0);
				const float age = sbo::GetHudTime() - st.killPopTime;
				if (st.killPop > 0 && age >= 0 && age < 1.5f)
				{
					Color c = GREEN_LIGHT;
					c[3] = (unsigned char)(255 * std::min(1.0f, (1.5f - age) / 0.5f));
					wchar_t pop[16];
					swprintf(pop, 16, L"+%d", st.killPop);
					vgui2::HFont f11 = Font(11);
					Text(f11, end - TextWide(f11, pop) / 2, by - S(5) - FontTall(f11) - (int)(age * S(4)), c, pop);
				}
				x += items[i].barW + S(8);
			}
			const int ty = y0 + (h - FontTall(items[i].font)) / 2;
			x += Text(items[i].font, x, ty, items[i].color, items[i].text.c_str());
		}
	}

	// NEAREST PLAYERS: the three nearest living players, then the dead
	void PaintNearest()
	{
		std::vector<PlayerRow> rows = GetPlayers();
		std::stable_sort(rows.begin(), rows.end(), [](const PlayerRow &a, const PlayerRow &b) {
			if (a.alive != b.alive)
				return a.alive;
			return a.alive && a.meters < b.meters;
		});
		if (rows.size() > 3)
			rows.resize(3);

		// the design's panel: 280 wide + 12 px padding, 10 px top; the header line, a gap, the rows (never closer than
		// the font's height and 3 px), the hint under the last
		vgui2::HFont font = Font(12);
		const int padX = S(12), padTop = S(10), tall = FontTall(font);
		const int pitch = NearestPitch(), headerGap = S(6), hintGap = S(6);
		const int w = S(306);
		const int h = NearestPanelTall((int)rows.size());
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int x0 = sw - S(16) - w, y0 = S(16);
		Fill(x0, y0, w, h, BG_PANEL);
		Outline(x0, y0, w, h, BORDER_DIM);

		int y = y0 + padTop;
		const int right = x0 + w - padX;
		Text(font, x0 + padX, y, DIM, L"NEAREST PLAYERS");
		Text(font, right - TextWide(font, L"dist"), y, DIM, L"dist");
		y += tall + headerGap;
		for (const PlayerRow &row : rows)
		{
			wchar_t left[96], dist[32];
			std::wstring name = PlayerName(row.index);
			if (row.alive)
			{
				swprintf(left, 96, L"%ls \u00b7 %d HP", name.c_str(), row.health);
				swprintf(dist, 32, L"%d m", row.meters);
			}
			else
			{
				swprintf(left, 96, L"%ls \u00b7 dead", name.c_str());
				wcscpy(dist, L"-");
			}
			const Color c = row.alive ? BRIGHT : DIM;
			const int ry = y + (pitch - tall) / 2;
			Text(font, x0 + padX, ry, c, left);
			Text(font, right - TextWide(font, dist), ry, c, dist);
			y += pitch;
		}
		y += hintGap;
		// "All players: Press K, then choose option 3"
		const int lw = Text(font, x0 + padX, y, FAINT, L"All players: ");
		Text(font, x0 + padX + lw, y, VALUE, PlayersPath().c_str());
	}

	// under the crosshair, two lines: "[monster] · [cost] pts", then the controls with the player's own keys
	// ("MOUSE1 place · MOUSE2 rotate · R cancel")
	void PaintPlacement()
	{
		std::wstring line1 = PlaceLabel();
		const std::wstring cost = CardText(PlaceEntry(), PlaceVariant(), GM_TEXT_PRICE);
		if (!cost.empty())
			line1 += L" \u00b7 " + cost + L" pts";
		// line 2: the primary keys bright, the words dim
		struct Part
		{
			std::wstring text;
			bool key;
		};
		const Part parts[] = { { KeyFor("+attack", L"MOUSE1"), true }, { L" place \u00b7 ", false },
			{ KeyFor("+attack2", L"MOUSE2"), true }, { L" rotate \u00b7 ", false }, { KeyFor("+reload", L"R"), true },
			{ L" cancel", false } };
		vgui2::HFont font = Font(12);
		int line2W = 0;
		for (const Part &p : parts)
			line2W += TextWide(font, p.text.c_str());
		// the design's 70 px bar after line 1 (the points, the cost as an amber chunk at the end), with the plugin
		const bool bar = HasEconomy() && GetStatus().maxPoints > 0 && !cost.empty();
		const int barW = bar ? S(70) : 0, barGap = bar ? S(8) : 0;
		const int line1W = TextWide(font, line1.c_str()) + barGap + barW;

		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int padX = S(8), lineH = S(16), h = lineH * 2 + S(6);
		const int w = std::max(line1W, line2W) + padX * 2;
		// the design: the preview box (110 tall) at 58 % of the height, the line 6 px under it (at 535 of 720)
		const int y = (int)(sh * 535.0f / 720.0f);
		const int x = (sw - w) / 2;
		Fill(x, y, w, h, BG_HINT);
		const int t = (lineH - FontTall(font)) / 2;
		int lx = x + (w - line1W) / 2;
		lx += Text(font, lx, y + S(3) + t, HINT_TEXT, line1.c_str());
		if (bar)
			EnergyBar(lx + barGap, y + S(3) + (lineH - S(8)) / 2, barW, S(8), (int)wcstol(cost.c_str(), nullptr, 10));
		int kx = x + (w - line2W) / 2;
		for (const Part &p : parts)
			kx += Text(font, kx, y + S(3) + lineH + t, p.key ? BRIGHT : DIM, p.text.c_str());
	}

	// the key hints, with the player's own keys and dot separators
	void PaintHints()
	{
		const std::wstring k = KeyFor("gmon", nullptr), fire = KeyFor("+attack", nullptr), alt = KeyFor("+attack2", nullptr);
		struct Hint
		{
			std::wstring key, text;
		};
		std::vector<Hint> hints;
		if (GetStatus().selected > 0)
		{
			// something selected: what applies to it
			hints.push_back({ alt, L" move order" });
			hints.push_back({ k, L" orders menu" });
		}
		else
		{
			// idle: the quick menu, the spawn window, selecting
			hints.push_back({ k, L" quick menu" });
			hints.push_back({ k + L" twice", L": spawn window" });
			hints.push_back({ fire, L" select (drag for a box)" });
		}
		vgui2::HFont bold = Font(12, true), font = Font(12);
		const std::wstring dot = L" \u00b7 ";
		const int padX = S(12);
		int w = padX * 2 + TextWide(font, dot.c_str()) * ((int)hints.size() - 1);
		for (const Hint &h : hints)
			w += TextWide(bold, h.key.c_str()) + TextWide(font, h.text.c_str());
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int h = HintsBarTall();
		const int x0 = (sw - w) / 2, y0 = HintsBarTop(sh);
		Fill(x0, y0, w, h, BG_HINTS);
		int x = x0 + padX;
		for (size_t i = 0; i < hints.size(); i++)
		{
			if (i)
				x += Text(font, x, y0 + (h - FontTall(font)) / 2, FAINT, dot.c_str());
			x += Text(bold, x, y0 + (h - FontTall(bold)) / 2, BRIGHT, hints[i].key.c_str());
			x += Text(font, x, y0 + (h - FontTall(font)) / 2, DIM, hints[i].text.c_str());
		}
	}
};

static CSboGmHud *s_pHud = nullptr;

//=============================================================================
// CSboGmNotice: the GmNotice line, a popup over every window (a refusal can come while a menu or the briefing is up)
//=============================================================================
class CSboGmNotice : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboGmNotice, vgui2::Panel);

public:
	CSboGmNotice(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboGmNotice")
	{
		MakePopup(false, false);
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetVisible(false);
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 50);
	}

	//! a new line: up and in front
	void Show()
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
		SetVisible(true);
		MoveToFront();
	}

	virtual void OnTick() override
	{
		if (IsVisible() && sbo::GetHudTime() > s_flNoticeEnd)
			SetVisible(false);
		else if (IsVisible())
			Repaint();
	}

	virtual void Paint() override
	{
		PaintNotice();
	}

private:
	// the notice: one line just above the placement line's spot, fading out in its last half second; also for a
	// player who is not (yet) a Game Master (a refusal)
	void PaintNotice()
	{
		const float left = s_flNoticeEnd - sbo::GetHudTime();
		if (left <= 0 || s_Notice.empty() || left > GM_NOTICE_TIME + 1)
			return;
		const float a = std::min(1.0f, left / 0.5f);
		vgui2::HFont font = Font(12);
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int padX = S(8), h = S(20);
		const int w = TextWide(font, s_Notice.c_str()) + padX * 2;
		const int y = (int)(sh * 535.0f / 720.0f) - S(6) - h;
		const int x = (sw - w) / 2;
		Color bg = BG_HINT, fg = HINT_TEXT;
		bg[3] = (unsigned char)(bg[3] * a);
		fg[3] = (unsigned char)(fg[3] * a);
		Fill(x, y, w, h, bg);
		Text(font, x + padX, y + (h - FontTall(font)) / 2, fg, s_Notice.c_str());
	}

};

static CSboGmNotice *s_pNotice = nullptr;


//=============================================================================
// CSboGmPlayersWindow
//=============================================================================
class CSboGmPlayersWindow : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboGmPlayersWindow, vgui2::Panel);

public:
	CSboGmPlayersWindow(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboGmPlayers")
	{
		MakePopup(false, false);
		SetPaintBackgroundEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetVisible(false);
		vgui2::ivgui()->AddTickSignal(GetVPanel());
	}

	bool IsOpen() const { return m_bOpen; }

	void Open(int highlight)
	{
		if (m_bOpen)
			return;
		m_bOpen = true;
		m_iSelected = (highlight >= 1 && highlight <= 32) ? highlight : 0;
		if (m_iSelected <= 0)
		{
			// the nearest living player (the first row)
			const std::vector<PlayerRow> rows = SortedRows();
			if (!rows.empty() && rows[0].alive)
				m_iSelected = rows[0].index;
		}
		m_iScroll = 0;
		m_iLayer = -1; // the selected player's level
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
		SetVisible(true);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		MoveToFront();
		RequestFocus();
		if (g_pGameUIViewport)
			g_pGameUIViewport->PreventEscapeToShow(true);
		sbo::ReleaseMouseForCursor();
		vgui2::input()->SetCursorPos(sw / 2, sh / 2);
		vgui2::surface()->CalculateMouseVisible();
		LoadOverview();
		m_bView = false;
		m_bBoxing = false;
		m_iPipSent = 0;
		sbo::ExecClientCmd("cmd gm_mapwatch 1"); // the map gets this Game Master's monsters while it is open
	}

	void Close()
	{
		if (!m_bOpen)
			return;
		m_bOpen = false;
		m_bBoxing = false;
		s_iPipTarget = 0;
		sbo::ExecClientCmd("cmd gm_mapwatch 0");
		if (m_iPipSent)
			sbo::ExecClientCmd("cmd gm_pip 0");
		m_iPipSent = 0;
		if (g_pGameUIViewport)
			g_pGameUIViewport->PreventEscapeToShow(false);
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetVisible(false);
		vgui2::surface()->CalculateMouseVisible();
	}

	virtual void OnTick() override
	{
		if (m_bOpen && GetState() != GM_STATE_ACTIVE)
			Close();
		if (m_bOpen)
			Repaint();
	}

	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override
	{
		if (code == vgui2::KEY_ESCAPE)
		{
			Close();
			return;
		}
		const char *key = sbo::LookupKeyBinding("gmon");
		if (key && code >= vgui2::KEY_A && code <= vgui2::KEY_Z && key[1] == 0 && key[0] == 'a' + (code - vgui2::KEY_A))
		{
			Close();
			return;
		}
		if (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN)
		{
			std::vector<PlayerRow> rows = SortedRows();
			int at = -1;
			for (size_t i = 0; i < rows.size(); i++)
				if (rows[i].index == m_iSelected)
					at = (int)i;
			at += code == vgui2::KEY_UP ? -1 : 1;
			if (at >= 0 && at < (int)rows.size())
				Select(rows[at].index);
			return;
		}
		if (code == vgui2::KEY_ENTER)
		{
			Action(0);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	virtual void OnMouseWheeled(int delta) override
	{
		m_iScroll -= delta;
		if (m_iScroll < 0)
			m_iScroll = 0;
	}

	virtual void OnMousePressed(vgui2::MouseCode code) override
	{
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		Layout();
		// the map: a right click sends the selection there (the floor under it on the level shown)
		if (code == vgui2::MOUSE_RIGHT)
		{
			if (!m_bView && Hit(m_rImg, x, y))
				MapMoveOrder(x, y);
			return;
		}
		if (code != vgui2::MOUSE_LEFT)
			return;
		if (Hit(m_rClose, x, y))
		{
			Close();
			return;
		}
		if (Hit(m_rTabMap, x, y) || Hit(m_rTabView, x, y))
		{
			m_bView = Hit(m_rTabView, x, y);
			return;
		}
		if (!m_bView)
		{
			// a monster marker: it alone is selected; else a box on the map starts (a bubble: the player)
			for (const auto &mk : m_Markers)
			{
				if (abs(x - mk.x) <= MarkerR() + S(2) && abs(y - mk.y) <= MarkerR() + S(2))
				{
					char cmd[64];
					snprintf(cmd, sizeof(cmd), "cmd gm_mapselect %d", mk.entity);
					sbo::ExecClientCmd(cmd);
					return;
				}
			}
			bool onBubble = false;
			for (const auto &dot : m_Dots)
				onBubble = onBubble || (abs(x - dot.x) <= BubbleR() && abs(y - dot.y) <= BubbleR());
			if (!onBubble && Hit(m_rImg, x, y))
			{
				m_bBoxing = true;
				m_iBoxX = x;
				m_iBoxY = y;
				vgui2::input()->SetMouseCapture(GetVPanel());
				return;
			}
		}
		for (int i = 0; i < 4; i++)
		{
			if (Hit(m_rButtons[i], x, y))
			{
				Action(i);
				return;
			}
		}
		for (size_t i = 0; i < m_LevelRects.size(); i++)
		{
			if (Hit(m_LevelRects[i], x, y))
			{
				const std::vector<int> order = LevelOrder();
				m_iLayer = i < order.size() ? order[i] : 0;
				return;
			}
		}
		for (const auto &dot : m_Dots)
		{
			if (abs(x - dot.x) <= BubbleR() && abs(y - dot.y) <= BubbleR())
			{
				Select(dot.index);
				return;
			}
		}
		for (const auto &row : m_RowRects)
		{
			if (Hit(row.r, x, y))
			{
				Select(row.index);
				return;
			}
		}
	}

	virtual void OnMouseReleased(vgui2::MouseCode code) override
	{
		if (code != vgui2::MOUSE_LEFT || !m_bBoxing)
			return;
		m_bBoxing = false;
		vgui2::input()->SetMouseCapture(0);
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		if (abs(x - m_iBoxX) < S(4) && abs(y - m_iBoxY) < S(4))
		{
			// a click on the map that is no marker: a bubble under it selects the player
			for (const auto &dot : m_Dots)
				if (abs(x - dot.x) <= BubbleR() && abs(y - dot.y) <= BubbleR())
					Select(dot.index);
			return;
		}
		const int x0 = std::min(x, m_iBoxX), x1 = std::max(x, m_iBoxX), y0 = std::min(y, m_iBoxY), y1 = std::max(y, m_iBoxY);
		std::string cmd = "cmd gm_mapselect";
		int n = 0;
		for (const auto &mk : m_Markers)
		{
			if (mk.x >= x0 && mk.x <= x1 && mk.y >= y0 && mk.y <= y1 && n < 16)
			{
				cmd += " " + std::to_string(mk.entity);
				n++;
			}
		}
		sbo::ExecClientCmd(cmd.c_str()); // none: the selection is cleared
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code) override
	{
		OnMousePressed(code);
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		for (const auto &row : m_RowRects)
		{
			if (Hit(row.r, x, y))
			{
				Action(0);
				return;
			}
		}
	}

	virtual void Paint() override
	{
		if (!m_bOpen)
			return;
		Layout();
		PaintWindow();
	}

private:
	struct Rect
	{
		int x = 0, y = 0, w = 0, h = 0;
	};
	struct RowRect
	{
		Rect r;
		int index;
	};
	struct Dot
	{
		int x, y, index;
	};

	static bool Hit(const Rect &r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

	static int BubbleR() { return S(6); }  // the map's player bubbles: 12 px (18 at 1080 lines)
	static int MarkerR() { return S(4); }  // the map's monster markers: 8 px diamonds

	// a point of the shown map image -> world x, y (MapUV's inverse)
	void MapWorld(int x, int y, float &wx, float &wy) const
	{
		const float u = (float)(x - m_rImg.x) / std::max(1, m_rImg.w), v = (float)(y - m_rImg.y) / std::max(1, m_rImg.h);
		const float z = m_Overview.zoom, a = 4.0f / 3.0f;
		const float xs = m_Overview.origin[0], ys = m_Overview.origin[1];
		if (m_Overview.rotated)
		{
			wx = u * 2 * 4096.0f / z + xs - 4096.0f / z;
			wy = ys + 4096.0f / (z * a) - v * 2 * 4096.0f / (z * a);
		}
		else
		{
			wx = xs + 4096.0f / (z * a) - v * 2 * 4096.0f / (z * a);
			wy = ys + 4096.0f / z - u * 2 * 4096.0f / z;
		}
	}

	// the top of the level shown (the next layer's height, else high up): the floor under the click is found from it
	float LevelTop(int layer) const
	{
		float top = 4096.0f;
		if (m_bHaveOverview)
			for (int i = 0; i < m_Overview.layers; i++)
				if (m_Overview.heights[i] > m_Overview.heights[layer] && m_Overview.heights[i] < top)
					top = m_Overview.heights[i];
		return top - 1;
	}

	void MapMoveOrder(int x, int y)
	{
		if (!m_bHaveOverview)
			return;
		float wx, wy;
		MapWorld(x, y, wx, wy);
		char cmd[96];
		snprintf(cmd, sizeof(cmd), "cmd gm_mapmove %.0f %.0f %.0f", wx, wy, LevelTop(CurrentLayer()));
		sbo::ExecClientCmd(cmd);
		m_flMoveMarkTime = sbo::GetHudTime();
		m_iMoveMarkX = x;
		m_iMoveMarkY = y;
	}

	static void Diamond(int cx, int cy, int r, const Color &c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int dy = -r; dy <= r; dy++)
		{
			const int half = r - abs(dy);
			vgui2::surface()->DrawFilledRect(cx - half, cy + dy, cx + half + 1, cy + dy + 1);
		}
	}

	// a new selection: the map shows his level (a Level button picks one by hand until the next selection)
	void Select(int index)
	{
		m_iSelected = index;
		m_iLayer = -1;
	}

	// the overview layer a height is on, as the spectator overview picks it: the highest layer at or under it
	int LayerOfZ(float z) const
	{
		if (!m_bHaveOverview || m_Overview.layers <= 1)
			return 0;
		int best = -1;
		for (int i = 0; i < m_Overview.layers; i++)
			if (z >= m_Overview.heights[i] && (best < 0 || m_Overview.heights[i] >= m_Overview.heights[best]))
				best = i;
		if (best < 0)
		{
			best = 0; // under every layer: the lowest
			for (int i = 1; i < m_Overview.layers; i++)
				if (m_Overview.heights[i] < m_Overview.heights[best])
					best = i;
		}
		return best;
	}

	// "Level n": a layer's place from the lowest floor up (1 = the lowest)
	int LevelNumber(int layer) const
	{
		const std::vector<int> order = LevelOrder();
		for (size_t i = 0; i < order.size(); i++)
			if (order[i] == layer)
				return (int)i + 1;
		return 1;
	}

	// a player's Steam picture as a texture (the scoreboard's CAvatarImage), -1 none (a bot, no Steam)
	int AvatarTexture(int index)
	{
		const unsigned long long sid = VGui2_GetPlayerSteamID64(index);
		if (!sid || !ClientSteamContext().SteamFriends() || !ClientSteamContext().SteamUtils())
			return -1;
		auto it = m_Avatars.find(sid);
		if (it == m_Avatars.end())
		{
			CAvatarImage *pAvatar = new CAvatarImage();
			pAvatar->SetDrawFriend(false);
			pAvatar->SetAvatarSteamID(CSteamID((uint64)sid), k_EAvatarSize64x64);
			it = m_Avatars.insert({ sid, pAvatar }).first;
		}
		return it->second->GetTextureID();
	}

	// a round picture: the avatar in a circle (no avatar: a disc), a thin border
	void Bubble(int cx, int cy, int r, int texture, const Color &tint, const Color &border)
	{
		constexpr int N = 24;
		if (texture >= 0)
		{
			vgui2::VGuiVertex v[N];
			for (int i = 0; i < N; i++)
			{
				const float a = 6.2831853f * i / N, c = cosf(a), s = sinf(a);
				v[i] = vgui2::VGuiVertex(cx + c * r, cy + s * r, 0.5f + 0.5f * c, 0.5f + 0.5f * s);
			}
			vgui2::surface()->DrawSetColor(tint);
			vgui2::surface()->DrawSetTexture(texture);
			vgui2::surface()->DrawTexturedPolygon(v, N);
		}
		else
			Disc(cx, cy, r, tint);
		vgui2::surface()->DrawSetColor(border);
		for (int i = 0; i < N; i++)
		{
			const float a0 = 6.2831853f * i / N, a1 = 6.2831853f * (i + 1) / N;
			vgui2::surface()->DrawLine(cx + (int)(cosf(a0) * r), cy + (int)(sinf(a0) * r), cx + (int)(cosf(a1) * r),
			    cy + (int)(sinf(a1) * r));
		}
	}

	std::vector<PlayerRow> SortedRows() const
	{
		std::vector<PlayerRow> rows = GetPlayers();
		std::stable_sort(rows.begin(), rows.end(), [](const PlayerRow &a, const PlayerRow &b) {
			if (a.alive != b.alive)
				return a.alive;
			return a.alive && a.meters < b.meters;
		});
		return rows;
	}

	bool SelectedAlive() const
	{
		for (const PlayerRow &row : GetPlayers())
			if (row.index == m_iSelected)
				return row.alive;
		return false;
	}

	void Action(int which)
	{
		if (!SelectedAlive())
			return;
		char cmd[64];
		switch (which)
		{
		case 0: snprintf(cmd, sizeof(cmd), "cmd gm_goto %d", m_iSelected); break;
		case 1: snprintf(cmd, sizeof(cmd), "cmd gm_follow %d 0", m_iSelected); break;
		case 2: snprintf(cmd, sizeof(cmd), "cmd gm_follow %d 1", m_iSelected); break;
		default: snprintf(cmd, sizeof(cmd), "cmd gm_spawnnear %d", m_iSelected); break;
		}
		Close();
		sbo::ExecClientCmd(cmd);
	}

	// the overview of this map: one texture per layer, decoded from overviews/<map>.tga (the server browser's decoder)
	void LoadOverview()
	{
		sbo::GmOverview ov;
		const bool have = sbo::GetGmOverview(ov);
		const char *level = sbo::GetLevelName();
		std::string key = (level ? level : "") + std::string(have ? ov.images[0] : "");
		if (key == m_OverviewKey)
			return;
		m_OverviewKey = key;
		m_bHaveOverview = have;
		m_Overview = ov;
		m_Textures.assign(have ? ov.layers : 0, Tex());
		for (int i = 0; have && i < ov.layers; i++)
		{
			std::vector<uint8_t> data;
			const char *path = ov.images[i];
			FileHandle_t fh = g_pFullFileSystem->FileExists(path) ? g_pFullFileSystem->Open(path, "rb") : nullptr;
			if (!fh)
				continue;
			const unsigned size = g_pFullFileSystem->Size(fh);
			if (size > 0 && size <= sbo_image::MAX_FILE_BYTES)
			{
				data.resize(size);
				if (g_pFullFileSystem->Read(data.data(), (int)size, fh) != (int)size)
					data.clear();
			}
			g_pFullFileSystem->Close(fh);
			std::vector<uint8_t> rgba;
			int w = 0, h = 0;
			if (data.empty() || !sbo_image::DecodeTgaScaled(data.data(), data.size(), 1024, 1024, rgba, w, h, true))
				continue;
			Tex &t = m_Textures[i];
			t.id = vgui2::surface()->CreateNewTextureID(true);
			vgui2::surface()->DrawSetTextureRGBA(t.id, rgba.data(), w, h, true, true);
			t.w = w;
			t.h = h;
			// the part the level is drawn in (the rest is black): the window zooms to it
			int x0 = w, y0 = h, x1 = -1, y1 = -1;
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++)
				{
					const uint8_t *px = &rgba[(y * w + x) * 4];
					if (px[3] > 0 && (px[0] > 12 || px[1] > 12 || px[2] > 12))
					{
						x0 = std::min(x0, x);
						x1 = std::max(x1, x);
						y0 = std::min(y0, y);
						y1 = std::max(y1, y);
					}
				}
			if (x1 >= x0 && y1 >= y0)
			{
				const float pad = 0.02f;
				t.u0 = std::max(0.0f, (float)x0 / w - pad);
				t.v0 = std::max(0.0f, (float)y0 / h - pad);
				t.u1 = std::min(1.0f, (float)(x1 + 1) / w + pad);
				t.v1 = std::min(1.0f, (float)(y1 + 1) / h + pad);
			}
		}
	}

	// the layers from the lowest floor up: "Level 1" is the lowest
	std::vector<int> LevelOrder() const
	{
		std::vector<int> order;
		const int n = m_bHaveOverview ? m_Overview.layers : 0;
		for (int i = 0; i < n; i++)
			order.push_back(i);
		std::sort(order.begin(), order.end(), [this](int a, int b) { return m_Overview.heights[a] < m_Overview.heights[b]; });
		if (order.empty())
			order.push_back(0);
		return order;
	}

	int CurrentLayer() const
	{
		if (!m_bHaveOverview || m_Overview.layers <= 0)
			return 0;
		if (m_iLayer >= 0 && m_iLayer < m_Overview.layers)
			return m_iLayer;
		for (const PlayerRow &r : GetPlayers())
			if (r.index == m_iSelected)
				return LayerOfZ(r.pos[2]);
		float org[3], ang[3];
		sbo::GetLocalView(org, ang);
		return LayerOfZ(org[2]);
	}

	// a world position -> 0..1 on the overview image (hud_spectator.cpp DrawOverviewLayer's quads)
	bool MapUV(const float *pos, float &u, float &v) const
	{
		const float z = m_Overview.zoom, a = 4.0f / 3.0f;
		const float xs = m_Overview.origin[0], ys = m_Overview.origin[1];
		if (m_Overview.rotated)
		{
			u = (pos[0] - (xs - 4096.0f / z)) / (2 * 4096.0f / z);
			v = ((ys + 4096.0f / (z * a)) - pos[1]) / (2 * 4096.0f / (z * a));
		}
		else
		{
			v = ((xs + 4096.0f / (z * a)) - pos[0]) / (2 * 4096.0f / (z * a));
			u = ((ys + 4096.0f / z) - pos[1]) / (2 * 4096.0f / z);
		}
		return u >= 0 && u <= 1 && v >= 0 && v <= 1;
	}

	void Layout()
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		m_rWin = { (sw - S(1000)) / 2, (sh - S(620)) / 2, S(1000), S(620) };
		const int px = m_rWin.x + S(24), py = m_rWin.y + S(20);
		m_rClose = { m_rWin.x + m_rWin.w - S(24) - S(32), py + (S(31) - S(32)) / 2, S(32), S(32) };
		const int bodyY = py + S(31) + S(14);
		m_rBody = { px, bodyY, m_rWin.w - S(48), m_rWin.y + m_rWin.h - S(20) - bodyY };
		m_rTabMap = { px, bodyY, S(80), S(28) };
		m_rTabView = { px + S(86), bodyY, S(80), S(28) };
		m_rMap = { px, bodyY + S(34), S(360), S(360) };
		m_rList = { px + S(360) + S(16), bodyY, m_rBody.w - S(376), m_rBody.h };
		const int bw = (m_rList.w - 3 * S(8)) / 4;
		for (int i = 0; i < 4; i++)
			m_rButtons[i] = { m_rList.x + i * (bw + S(8)), m_rList.y + m_rList.h - S(44), bw, S(44) };
	}

	void Button(const Rect &r, const wchar_t *label, bool primary, float px, bool enabled = true)
	{
		Fill(r.x, r.y, r.w, r.h, primary ? BUTTON_BG : Color(0, 0, 0, 0));
		Outline(r.x, r.y, r.w, r.h, primary ? SEL_BORDER : BORDER);
		vgui2::HFont f = Font(px);
		Color c = primary ? BRIGHT : GREEN;
		if (!enabled)
			c[3] = 110;
		Text(f, r.x + (r.w - TextWide(f, label)) / 2, r.y + (r.h - FontTall(f)) / 2, c, label);
	}

	void PaintWindow()
	{
		// the View's picture is the 3D view showing through: the window's background goes around it
		const bool pip = m_bView && SelectedAlive();
		if (pip)
		{
			const Rect &m = m_rMap;
			Fill(m_rWin.x, m_rWin.y, m_rWin.w, m.y - m_rWin.y, BG);
			Fill(m_rWin.x, m.y + m.h, m_rWin.w, m_rWin.y + m_rWin.h - (m.y + m.h), BG);
			Fill(m_rWin.x, m.y, m.x - m_rWin.x, m.h, BG);
			Fill(m.x + m.w, m.y, m_rWin.x + m_rWin.w - (m.x + m.w), m.h, BG);
		}
		else
			Fill(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BG);
		s_iPipTarget = pip ? m_iSelected : 0;
		if (pip)
		{
			int sx = m_rMap.x + 1, sy = m_rMap.y + 1;
			LocalToScreen(sx, sy);
			s_PipRect[0] = sx;
			s_PipRect[1] = sy;
			s_PipRect[2] = m_rMap.w - 2;
			s_PipRect[3] = m_rMap.h - 2;
		}
		if (s_iPipTarget != m_iPipSent)
		{
			char cmd[32];
			snprintf(cmd, sizeof(cmd), "cmd gm_pip %d", s_iPipTarget);
			sbo::ExecClientCmd(cmd);
			m_iPipSent = s_iPipTarget;
		}
		Outline(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BORDER);

		// header: "Players" 26px, "[n] alive - [n] dead", X
		vgui2::HFont title = Font(26);
		const int px = m_rWin.x + S(24), py = m_rWin.y + S(20);
		Text(title, px, py + (S(31) - FontTall(title)) / 2, GREEN, L"Players");
		const std::vector<PlayerRow> rows = SortedRows();
		int alive = 0, dead = 0;
		for (const PlayerRow &r : rows)
			(r.alive ? alive : dead)++;
		wchar_t buf[128];
		swprintf(buf, 128, L"%d alive \u00b7 %d dead", alive, dead);
		vgui2::HFont f13 = Font(13);
		Text(f13, m_rClose.x - S(12) - TextWide(f13, buf), m_rClose.y + (m_rClose.h - FontTall(f13)) / 2, DIM, buf);
		CloseButton(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h);

		PaintMap(rows);
		PaintList(rows);

		const bool on = SelectedAlive();
		Button(m_rButtons[0], L"Go to", true, 14, on);
		Button(m_rButtons[1], L"Follow", false, 14, on);
		Button(m_rButtons[2], L"First Person View", false, 14, on);
		Button(m_rButtons[3], L"Spawn near", false, 14, on);
	}

	void PaintMap(const std::vector<PlayerRow> &rows)
	{
		const Rect &m = m_rMap;
		Button(m_rTabMap, L"Map", !m_bView, 12);
		Button(m_rTabView, L"View", m_bView, 12);
		if (m_bView)
		{
			// the selected player in chase locked (view.cpp draws it here); without a living one, a line
			m_Dots.clear();
			m_Markers.clear();
			m_LevelRects.clear();
			Outline(m.x, m.y, m.w, m.h, BORDER_DIM);
			vgui2::HFont f11 = Font(11);
			if (!SelectedAlive())
			{
				Fill(m.x + 1, m.y + 1, m.w - 2, m.h - 2, MAP_BG);
				const wchar_t *t = L"Select a living player";
				Text(f11, m.x + (m.w - TextWide(f11, t)) / 2, m.y + (m.h - FontTall(f11)) / 2, FAINT, t);
			}
			else
			{
				const std::wstring who = PlayerName(m_iSelected);
				Fill(m.x + 1, m.y + 1, TextWide(f11, who.c_str()) + S(12), FontTall(f11) + S(6), BG);
				Text(f11, m.x + 1 + S(6), m.y + 1 + S(3), BRIGHT, who.c_str());
			}
			return;
		}
		Fill(m.x, m.y, m.w, m.h, MAP_BG);
		m_Dots.clear();
		const int layer = CurrentLayer();
		Rect img = m;
		if (m_bHaveOverview && layer < (int)m_Textures.size() && m_Textures[layer].id >= 0)
		{
			const Tex &t = m_Textures[layer];
			// the drawn part of the level fitted into the box, keeping its shape (img: where the whole image lands, for
			// the positions)
			const float bw = (t.u1 - t.u0) * t.w, bh = (t.v1 - t.v0) * t.h;
			const float scale = std::min((float)(m.w - 2) / std::max(1.0f, bw), (float)(m.h - 2) / std::max(1.0f, bh));
			img.w = (int)(t.w * scale);
			img.h = (int)(t.h * scale);
			img.x = m.x + (int)((m.w - bw * scale) / 2 - t.u0 * img.w);
			img.y = m.y + (int)((m.h - bh * scale) / 2 - t.v0 * img.h);
			const float x0 = img.x + t.u0 * img.w, y0 = img.y + t.v0 * img.h, x1 = img.x + t.u1 * img.w, y1 = img.y + t.v1 * img.h;
			vgui2::VGuiVertex v[4] = { vgui2::VGuiVertex(x0, y0, t.u0, t.v0), vgui2::VGuiVertex(x1, y0, t.u1, t.v0),
				vgui2::VGuiVertex(x1, y1, t.u1, t.v1), vgui2::VGuiVertex(x0, y1, t.u0, t.v1) };
			vgui2::surface()->DrawSetColor(Color(255, 255, 255, 255));
			vgui2::surface()->DrawSetTexture(t.id);
			vgui2::surface()->DrawTexturedPolygon(v, 4);
		}
		m_rImg = img;
		Outline(m.x, m.y, m.w, m.h, BORDER_DIM);
		{
			// "<map> · Level n" (the map alone with one level)
			// the map's name alone ("maps/co_x.bsp" -> "co_x")
			std::string map = sbo::GetLevelName() ? sbo::GetLevelName() : "";
			const size_t slash = map.find_last_of("/\\");
			if (slash != std::string::npos)
				map = map.substr(slash + 1);
			if (map.size() > 4 && map.compare(map.size() - 4, 4, ".bsp") == 0)
				map.resize(map.size() - 4);
			std::wstring label = Widen(map.c_str());
			if (m_bHaveOverview && m_Overview.layers > 1)
				label += L" \u00b7 Level " + std::to_wstring(LevelNumber(layer));
			Text(Font(11), m.x + S(12), m.y + S(10), FAINT, label.c_str());
		}

		if (m_bHaveOverview)
		{
			// each player as his picture in a bubble; the dead grey with a red ring where they died; on another
			// level dim with a small arrow up or down; the name over the one under the cursor
			const int br = BubbleR();
			const bool levels = m_Overview.layers > 1;
			int mx, my;
			vgui2::input()->GetCursorPos(mx, my);
			ScreenToLocal(mx, my);

			// this Game Master's monsters: amber diamonds (the selected light green with a ring), dim on another
			// level; the name and health over the one under the cursor
			m_Markers.clear();
			const MonsterRow *hoverMonster = nullptr;
			int hmx = 0, hmy = 0;
			for (const MonsterRow &mo : GetMonsters())
			{
				float u, v;
				if (!MapUV(mo.pos, u, v))
					continue;
				const int cx = img.x + (int)(u * img.w), cy = img.y + (int)(v * img.h);
				if (cx < m.x || cx >= m.x + m.w || cy < m.y || cy >= m.y + m.h)
					continue; // outside the zoomed part
				m_Markers.push_back({ cx, cy, mo.entity });
				const bool sel = (mo.flags & GM_MONSTER_SELECTED) != 0;
				Color c = sel ? GREEN_LIGHT : ACCENT;
				if (levels && LayerOfZ(mo.pos[2]) != layer)
					c[3] = 90;
				if (sel)
					Diamond(cx, cy, MarkerR() + S(2), BRIGHT);
				Diamond(cx, cy, MarkerR(), c);
				if (abs(mx - cx) <= MarkerR() + S(2) && abs(my - cy) <= MarkerR() + S(2))
				{
					hoverMonster = &mo;
					hmx = cx;
					hmy = cy;
				}
			}
			if (m_bBoxing)
			{
				const int x0 = std::min(mx, m_iBoxX), y0 = std::min(my, m_iBoxY);
				Fill(x0, y0, abs(mx - m_iBoxX), abs(my - m_iBoxY), Color(0xa6, 0xec, 0x8a, 15));
				Dashed(x0, y0, abs(mx - m_iBoxX) + 1, abs(my - m_iBoxY) + 1, GREEN_LIGHT, S(4), S(3));
			}
			if (sbo::GetHudTime() - m_flMoveMarkTime < 1.0f)
				Ring(m_iMoveMarkX, m_iMoveMarkY, S(8), GREEN_LIGHT); // where the move order went
			const PlayerRow *hover = nullptr;
			int hx = 0, hy = 0;
			for (const PlayerRow &r : rows)
			{
				float u, v;
				if (!MapUV(r.pos, u, v))
					continue;
				const int cx = img.x + (int)(u * img.w), cy = img.y + (int)(v * img.h);
				if (cx < m.x || cx >= m.x + m.w || cy < m.y || cy >= m.y + m.h)
					continue;
				m_Dots.push_back({ cx, cy, r.index });
				const int pl = LayerOfZ(r.pos[2]);
				const bool other = levels && pl != layer;
				Color tint = r.alive ? Color(255, 255, 255, 255) : Color(120, 120, 120, 255);
				const int tex = AvatarTexture(r.index);
				if (tex < 0)
					tint = r.alive ? GREEN_LIGHT : FAINT;
				Color border = r.alive ? GREEN_LIGHT : BAR_RED;
				if (other)
				{
					tint[3] = 90;
					border[3] = 110;
				}
				Bubble(cx, cy, br, tex, tint, border);
				if (!r.alive)
					Ring(cx, cy, br + S(2), other ? Color(0xd6, 0x5f, 0x5f, 110) : BAR_RED);
				if (other)
				{
					// up or down from the level shown
					const bool above = m_Overview.heights[pl] > m_Overview.heights[layer];
					ArrowMark(cx + br + S(5), cy, S(8), S(7), above, DIM);
				}
				if (r.index == m_iSelected)
					Ring(cx, cy, br + S(4), BRIGHT);
				if (abs(mx - cx) <= br && abs(my - cy) <= br)
				{
					hover = &r;
					hx = cx;
					hy = cy;
				}
			}
			if (!hover && hoverMonster)
			{
				vgui2::HFont f11 = Font(11);
				wchar_t label[96];
				swprintf(label, 96, L"%ls \u00b7 %d%%", hoverMonster->entry < GM_CATALOG_COUNT ? Widen(g_GmCatalog[hoverMonster->entry].pszName).c_str() : L"Monster",
				    hoverMonster->health);
				const int tw = TextWide(f11, label) + S(10), th = FontTall(f11) + S(4);
				const int lx = std::min(std::max(hmx - tw / 2, m.x + 1), m.x + m.w - 1 - tw), ly = hmy - MarkerR() - S(6) - th;
				Fill(lx, ly, tw, th, BG);
				Outline(lx, ly, tw, th, BORDER_DIM);
				Text(f11, lx + S(5), ly + S(2), BRIGHT, label);
			}
			if (hover)
			{
				vgui2::HFont f11 = Font(11);
				const std::wstring name = PlayerName(hover->index);
				const int tw = TextWide(f11, name.c_str()) + S(10), th = FontTall(f11) + S(4);
				const int lx = std::min(std::max(hx - tw / 2, m.x + 1), m.x + m.w - 1 - tw), ly = hy - br - S(6) - th;
				Fill(lx, ly, tw, th, BG);
				Outline(lx, ly, tw, th, BORDER_DIM);
				Text(f11, lx + S(5), ly + S(2), BRIGHT, name.c_str());
			}
			float org[3], ang[3], u, v;
			sbo::GetLocalView(org, ang);
			if (MapUV(org, u, v))
				Triangle(img.x + (int)(u * img.w), img.y + (int)(v * img.h), S(16), S(14), ACCENT);
		}

		vgui2::HFont f11 = Font(11);
		const int levels = (int)LevelOrder().size();
		const int legendY = m.y + m.h + S(8);
		Text(f11, m.x, legendY, DIM, L"\u25c6 your monsters \u00b7 \u25b2 you   Click/drag: select \u00b7 Right click: send");

		m_LevelRects.clear();
		const std::vector<int> order = LevelOrder();
		const int by = legendY + FontTall(f11) + S(8);
		const int bw = (m.w - (levels - 1) * S(6)) / levels;
		for (int i = 0; i < levels; i++)
		{
			Rect r = { m.x + i * (bw + S(6)), by, bw, S(32) };
			m_LevelRects.push_back(r);
			wchar_t label[32];
			swprintf(label, 32, L"Level %d", i + 1);
			Button(r, label, order[i] == layer, 12);
		}
	}

	void PaintList(const std::vector<PlayerRow> &rows)
	{
		const Rect &l = m_rList;
		vgui2::HFont f11 = Font(11), f13 = Font(13);
		// columns 2fr 1fr 1fr 1.4fr, gap 8, padding 0 10
		const int inner = l.w - S(20) - 3 * S(8);
		const int c0 = l.x + S(10), c1 = c0 + (int)(inner * 2 / 5.4f) + S(8), c2 = c1 + (int)(inner / 5.4f) + S(8),
		          c3 = c2 + (int)(inner / 5.4f) + S(8);
		Text(f11, c0, l.y, DIM, L"Player");
		Text(f11, c1, l.y, DIM, L"Health");
		Text(f11, c2, l.y, DIM, L"Distance");
		Text(f11, c3, l.y, DIM, L"Weapon");

		m_RowRects.clear();
		int y = l.y + FontTall(f11) + S(6);
		const int bottom = m_rButtons[0].y - S(6);
		int skip = m_iScroll;
		for (const PlayerRow &r : rows)
		{
			const bool note = !r.alive && r.killedBy != 255;
			const int rowTall = S(9) * 2 + FontTall(f13) + (note ? FontTall(f11) + S(2) : 0) + 2;
			if (skip > 0)
			{
				skip--;
				continue;
			}
			if (y + rowTall > bottom)
				break;
			const bool sel = r.index == m_iSelected;
			Rect rr = { l.x, y, l.w, rowTall };
			m_RowRects.push_back({ rr, r.index });
			if (sel)
				Fill(rr.x, rr.y, rr.w, rr.h, SEL_BG);
			Outline(rr.x, rr.y, rr.w, rr.h, sel ? SEL_BORDER : BORDER_DIM);
			const int ty = y + 1 + S(9);
			// the Steam picture (the scoreboard's), then the name and, on a map with levels, "Level n"
			const int av = S(20), tex = AvatarTexture(r.index);
			const int ay = ty + (FontTall(f13) - av) / 2;
			if (tex >= 0)
			{
				vgui2::surface()->DrawSetColor(r.alive ? Color(255, 255, 255, 255) : Color(120, 120, 120, 255));
				vgui2::surface()->DrawSetTexture(tex);
				vgui2::surface()->DrawTexturedRect(c0, ay, c0 + av, ay + av);
			}
			else
				Disc(c0 + av / 2, ay + av / 2, av / 2, r.alive ? GREEN_LIGHT : FAINT); // no picture: the map's disc
			const int nx = c0 + av + S(6);
			const std::wstring name = PlayerName(r.index);
			Text(f13, nx, ty, r.alive ? BRIGHT : DIM, name.c_str());
			if (m_bHaveOverview && m_Overview.layers > 1)
			{
				wchar_t lv[24];
				swprintf(lv, 24, L"Level %d", LevelNumber(LayerOfZ(r.pos[2])));
				Text(f11, nx + TextWide(f13, name.c_str()) + S(8), ty + FontTall(f13) - FontTall(f11), FAINT, lv);
			}
			if (note)
			{
				std::wstring n = L"Killed by your " + Widen(g_GmCatalog[r.killedBy].pszName);
				Text(f11, nx, ty + FontTall(f13) + S(2), ACCENT, n.c_str());
			}
			wchar_t buf[64];
			if (r.alive)
				swprintf(buf, 64, L"%d", r.health);
			else
				wcscpy(buf, L"dead");
			Text(f13, c1, ty, !r.alive ? FAINT : (r.health < 25 ? ACCENT : GREEN_LIGHT), buf);
			if (r.alive)
				swprintf(buf, 64, L"%d m", r.meters);
			else
				wcscpy(buf, L"-");
			Text(f13, c2, ty, VALUE, buf);
			std::wstring weapon = r.alive && r.weapon != 255 ? Widen(g_GmCatalog[r.weapon].pszName) : L"-";
			Text(f13, c3, ty, VALUE, weapon.c_str());
			y += rowTall + S(6);
		}
	}

	static void Disc(int cx, int cy, int r, const Color &c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int dy = -r; dy <= r; dy++)
		{
			int dx = (int)sqrtf((float)(r * r - dy * dy));
			vgui2::surface()->DrawFilledRect(cx - dx, cy + dy, cx + dx + 1, cy + dy + 1);
		}
	}

	static void Ring(int cx, int cy, int r, const Color &c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int dy = -r; dy <= r; dy++)
		{
			int outer = (int)sqrtf((float)(r * r - dy * dy));
			int innerR = r - S(2);
			int inner = abs(dy) < innerR ? (int)sqrtf((float)(innerR * innerR - dy * dy)) : -1;
			if (inner < 0)
				vgui2::surface()->DrawFilledRect(cx - outer, cy + dy, cx + outer + 1, cy + dy + 1);
			else
			{
				vgui2::surface()->DrawFilledRect(cx - outer, cy + dy, cx - inner, cy + dy + 1);
				vgui2::surface()->DrawFilledRect(cx + inner + 1, cy + dy, cx + outer + 1, cy + dy + 1);
			}
		}
	}

	// a small arrow up or down (a player on another level), w x h, centered
	static void ArrowMark(int cx, int cy, int w, int h, bool up, const Color &c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int i = 0; i < h; i++)
		{
			const int row = up ? i : h - 1 - i;
			int half = (int)((w / 2.0f) * (row + 1) / h);
			vgui2::surface()->DrawFilledRect(cx - half, cy - h / 2 + i, cx + half + 1, cy - h / 2 + i + 1);
		}
	}

	// the design's "you": a triangle pointing up, w x h, centered
	static void Triangle(int cx, int cy, int w, int h, const Color &c)
	{
		vgui2::surface()->DrawSetColor(c);
		for (int i = 0; i < h; i++)
		{
			int half = (int)((w / 2.0f) * (i + 1) / h);
			vgui2::surface()->DrawFilledRect(cx - half, cy - h / 2 + i, cx + half + 1, cy - h / 2 + i + 1);
		}
	}

	struct Tex
	{
		int id = -1, w = 0, h = 0;
		float u0 = 0, v0 = 0, u1 = 1, v1 = 1; // the drawn part of the image
	};

	bool m_bOpen = false;
	int m_iSelected = 0;
	int m_iScroll = 0;
	int m_iLayer = -1;
	Rect m_rWin, m_rClose, m_rBody, m_rMap, m_rList, m_rButtons[4];
	std::vector<RowRect> m_RowRects;
	std::vector<Rect> m_LevelRects;
	std::vector<Dot> m_Dots;
	std::string m_OverviewKey;
	bool m_bHaveOverview = false;
	sbo::GmOverview m_Overview = {};
	std::vector<Tex> m_Textures;
	std::map<unsigned long long, CAvatarImage *> m_Avatars;
	struct Marker
	{
		int x, y, entity;
	};
	std::vector<Marker> m_Markers;
	Rect m_rTabMap, m_rTabView, m_rImg;
	bool m_bView = false;   // the left area shows the View (the selected player) instead of the map
	int m_iPipSent = 0;     // the player last sent with gm_pip
	bool m_bBoxing = false; // a box being dragged on the map
	int m_iBoxX = 0, m_iBoxY = 0;
	float m_flMoveMarkTime = -100;
	int m_iMoveMarkX = 0, m_iMoveMarkY = 0;
};

static CSboGmPlayersWindow *s_pPlayers = nullptr;

//=============================================================================
// API
//=============================================================================
void GmUi_OpenPlayers(int highlight)
{
	if (s_pPlayers)
		s_pPlayers->Open(highlight);
}

void GmUi_ClosePlayers()
{
	if (s_pPlayers)
		s_pPlayers->Close();
}

bool GmUi_PlayersOpen()
{
	return s_pPlayers && s_pPlayers->IsOpen();
}

void GmUi_Init()
{
	if (s_pHud || !g_pViewport)
		return;
	s_pHud = new CSboGmHud(g_pViewport);
	s_pBoxSelect = new CSboGmBoxSelect(g_pViewport);
	s_pPlayers = new CSboGmPlayersWindow(g_pViewport);
	s_pNotice = new CSboGmNotice(g_pViewport);
	sbo::HookUserMsg("GmStatus", &gmui::MsgFunc_GmStatus);
	sbo::HookUserMsg("GmPlayers", &gmui::MsgFunc_GmPlayers);
	sbo::HookUserMsg("GmNotice", &gmui::MsgFunc_GmNotice);
	sbo::HookUserMsg("GmMonsters", &gmui::MsgFunc_GmMonsters);
}

void GmUi_Reset()
{
	gmui::ResetData();
	if (s_pPlayers)
		s_pPlayers->Close();
	if (s_pBoxSelect)
		s_pBoxSelect->Finish(false);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_GmAttack()
{
	// a flying Game Master without a placement or window: Fire draws the selection box (the server never sees it)
	if (!s_pBoxSelect || GetState() != GM_STATE_ACTIVE || IsPlacing() || AnyWindowOpen()
	    || (GetStatus().flags & GM_STATUS_FOLLOWING))
		return false;
	s_pBoxSelect->Start();
	return true;
}

bool VGui2_GmBoxSelecting()
{
	return s_pBoxSelect && s_pBoxSelect->IsActive();
}

bool VGui2_GmHudOwnsScreen()
{
	// the flying Game Master's HUD replaces the spectator bar ("Ghost Mode 3 : You are a Free Ghost")
	return GetState() != GM_STATE_OFF && !(GetStatus().flags & GM_STATUS_FOLLOWING);
}

void GmUi_ShowNotice()
{
	if (s_pNotice && !gmui::s_Notice.empty())
		s_pNotice->Show();
}

bool VGui2_GmPip(int &target, int &x, int &y, int &w, int &h)
{
	if (gmui::s_iPipTarget <= 0 || !GmUi_PlayersOpen())
		return false;
	target = gmui::s_iPipTarget;
	x = gmui::s_PipRect[0];
	y = gmui::s_PipRect[1];
	w = gmui::s_PipRect[2];
	h = gmui::s_PipRect[3];
	return w > 0 && h > 0;
}

int VGui2_GmStripBottom()
{
	// PaintStrip: 16 px from the top, 36 tall; 6 px gap under it
	if (GetState() != GM_STATE_ACTIVE || AnyWindowOpen() || (GetStatus().flags & GM_STATUS_FOLLOWING))
		return 0;
	return S(16) + S(36) + S(6);
}

int VGui2_GmKillFeedTop()
{
	// the kill feed goes under the nearest players panel while it is up (PaintNearest's height)
	if (GetState() != GM_STATE_ACTIVE || AnyWindowOpen() || (GetStatus().flags & GM_STATUS_FOLLOWING))
		return 0;
	const int rows = std::min<int>(3, (int)GetPlayers().size());
	return S(16) + NearestPanelTall(rows) + S(8);
}

//-----------------------------------------------------------------------------
// pm_shared: the roaming Game Master's speed for the client's prediction (the server's, from the last GmStatus)
//-----------------------------------------------------------------------------
extern "C" float PM_GmRoamSpeed(void)
{
	return gmui::GetStatus().received ? (float)gmui::GetStatus().roamSpeed : (float)GM_ROAM_SPEED;
}
