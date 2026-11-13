//========= SBO VGUI2: trigger_objective HUD list =========//
//
// Purpose: see hud_objectives.h.
//=================================================================//

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <vector>

#include <tier1/strtools.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "hud_deathnotice.h"
#include "hud_objectives.h"
#include "sbo_bridge.h"
#include "sbo_cvar.h"
#include "sbo_menus.h"
#include "sbo_spectator_panel.h"
#include "sbo_team_menu.h"
#include "sbo_text_window.h"
#include "sbo_ui_style.h"
#include "vgui2_api.h"

#include "../objective_core.h"

namespace sbo
{
// sbo_objectives_bridge.cpp (SBO world): titles.txt's message NAME
bool ObjectiveTitlesText(const char *name, char *out, int outSize);
} // namespace sbo

namespace
{
// the panel: 0 hidden (the notices still show), 1 compact, 2 full; the command "objectives" (default key H) cycles
// 1 -> 2 -> 0 -> 1; Options > Advanced "Enable on-screen objectives" (0 / 1)
CSboCvar hud_objectives("hud_objectives", "1");

objective::List s_List;
int s_iResets = 0; // Objectives_Revision changes at a reset too

const char NOTICE_SOUND[] = "events/tutor_msg.wav"; // env_tutor's message sound

// Colors
const Color COL_BOX(10, 14, 18, 184);         // rgba(10, 14, 18, 0.72)
const Color COL_BOX_BORDER(127, 224, 210, 89); // the teal at 0.35
const Color COL_TEAL(127, 224, 210, 255);      // heading, markers, counters
const Color COL_DIM(167, 177, 186, 255);       // "2 active"
const Color COL_TEXT(242, 245, 247, 255);      // an active objective
const Color COL_DONE(139, 148, 156, 255);      // a completed one: text, tick, line through it
const Color COL_NOTICE_BOX(10, 14, 18, 209);   // rgba(10, 14, 18, 0.82)
const Color COL_AMBER(224, 169, 90, 255);      // the notice's border and heading
const Color COL_NOTICE_TEXT(255, 246, 232, 255);
const Color COL_FULL_BOX(10, 14, 18, 209);
const Color COL_FULL_BORDER(58, 67, 75, 255);  // #3a434b

// 640x480 units (pixels at 720p / 1.5)
constexpr float U_WIDE = 220.0f;
constexpr float U_PAD_X = 9.3f;
constexpr float U_PAD_Y = 8.0f;
constexpr float U_GAP = 5.3f;          // between the heading and rows, and between rows
constexpr float U_ICON = 9.3f;
constexpr float U_ICON_GAP = 6.7f;
constexpr float U_HEAD_FONT = 8.0f;
constexpr float U_SMALL_FONT = 7.3f;
constexpr float U_ROW_FONT = 10.0f;
constexpr float U_NOTICE_FONT = 10.7f;
constexpr float U_BELOW_KILLFEED = 4.0f;
constexpr float U_NOTICE_GAP = 6.7f;
constexpr float U_NOTICE_PAD_Y = 6.7f;
constexpr float U_LEFT = 12.0f;        // the panel's left edge (the kill feed's margin on the other side)
constexpr float U_INDENT = 11.0f;      // a part under its group
constexpr float U_SPECTATOR_TOP = 36.0f; // below the spectator's top bar (as game_timer)
constexpr float FULL_MAX_TALL = 0.55f; // the full panel: at most this part of the screen's height, then "+N more"
constexpr float U_TOP = 24.0f;         // the kill feed's top
constexpr int HUD_TEXT_LINES = 2;      // a long objective on the HUD: two lines, then "..."

int ScreenTall()
{
	int w = 0, h = 0;
	vgui2::surface()->GetScreenSize(w, h);
	return h > 0 ? h : 480;
}

int U(float v)
{
	return (int)lroundf(v * ScreenTall() / 480.0f);
}

vgui2::HFont Font(float units, int weight)
{
	return sbolook::MakeFont(std::max(10, U(units)), weight);
}

Color Fade(Color c, float alpha)
{
	c[3] = (unsigned char)(c[3] * (alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha)));
	return c;
}

// The objective's text: "#NAME" is titles.txt's message NAME, else a VGUI2 language token, else the text itself
std::wstring ObjectiveText(const objective::Entry &e)
{
	if (e.text[0] == '#')
	{
		char buf[512];
		if (sbo::ObjectiveTitlesText(e.text + 1, buf, sizeof(buf)))
			return sbolook::Widen(buf);
		const wchar_t *loc = g_pVGuiLocalize ? g_pVGuiLocalize->Find(e.text) : nullptr;
		if (loc)
			return loc;
		return sbolook::Widen(e.text + 1);
	}
	return sbolook::Widen(e.text);
}

// Words onto lines at most maxWide wide; past maxLines (0 = no limit) the last line ends in "..."
std::vector<std::wstring> Wrap(vgui2::HFont font, const std::wstring &text, int maxWide, int maxLines)
{
	std::vector<std::wstring> lines;
	std::wstring line;
	size_t i = 0;
	while (i < text.size())
	{
		while (i < text.size() && text[i] == L' ')
			i++;
		size_t j = i;
		while (j < text.size() && text[j] != L' ')
			j++;
		if (j == i)
			break;
		const std::wstring word = text.substr(i, j - i);
		const std::wstring trial = line.empty() ? word : line + L" " + word;
		if (line.empty() || sbolook::TextWide(font, trial) <= maxWide)
			line = trial;
		else
		{
			lines.push_back(line);
			line = word;
		}
		i = j;
	}
	if (!line.empty() || lines.empty())
		lines.push_back(line);
	// a word wider than the line: cut with "..."
	for (std::wstring &l : lines)
	{
		if (sbolook::TextWide(font, l) <= maxWide)
			continue;
		while (!l.empty() && sbolook::TextWide(font, l + L"...") > maxWide)
			l.pop_back();
		l += L"...";
	}
	if (maxLines > 0 && (int)lines.size() > maxLines)
	{
		lines.resize(maxLines);
		std::wstring &l = lines.back();
		while (!l.empty() && sbolook::TextWide(font, l + L"...") > maxWide)
			l.pop_back();
		l += L"...";
	}
	return lines;
}

// One objective's row laid out at a width
struct Row
{
	int slot = -1;
	bool part = false;
	bool done = false;
	float alpha = 1.0f;
	std::vector<std::wstring> lines;
	std::wstring counter;
	int counterWide = 0;
	int tall = 0;
};

Row LayoutRow(int slot, bool part, int wide, vgui2::HFont rowFont, vgui2::HFont counterFont, int maxLines, float alpha)
{
	const objective::Entry &e = s_List.Get(slot);
	Row r;
	r.slot = slot;
	r.part = part;
	if (part)
		wide -= U(U_INDENT);
	r.done = e.state == OBJECTIVE_COMPLETED;
	r.alpha = alpha;
	if (e.target > 0 && !r.done)
	{
		wchar_t buf[16];
		swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%d/%d", e.count, e.target);
		r.counter = buf;
		r.counterWide = sbolook::TextWide(counterFont, r.counter) + U(U_ICON_GAP);
	}
	const int textWide = std::max(U(40), wide - U(U_ICON) - U(U_ICON_GAP) - r.counterWide);
	r.lines = Wrap(rowFont, ObjectiveText(e), textWide, maxLines);
	r.tall = (int)r.lines.size() * sbolook::FontTall(rowFont);
	return r;
}

// a triangle pointing right, size x size, its top left at (x, y)
void DrawMarker(int x, int y, int size, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	const int w = std::max(2, size * 6 / 14), x0 = x + size * 4 / 14;
	const int top = y + size * 2 / 14, h = std::max(3, size * 10 / 14);
	for (int i = 0; i < w; i++)
	{
		const int half = (int)lroundf((h / 2.0f) * (1.0f - (float)i / w));
		const int cy = top + h / 2;
		vgui2::surface()->DrawFilledRect(x0 + i, cy - half, x0 + i + 1, cy + half + 1);
	}
}

// a tick, size x size, its top left at (x, y): the path M2 7.5 l3 3 7 -7 in a 14 unit box
void DrawTick(int x, int y, int size, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	const float k = size / 14.0f;
	const int t = std::max(2, (int)lroundf(2.0f * k));
	auto seg = [&](float ax, float ay, float bx, float by) {
		const int steps = std::max(2, (int)(fabsf(bx - ax) * k * 2.0f));
		for (int i = 0; i <= steps; i++)
		{
			const float f = (float)i / steps;
			const int px = x + (int)lroundf((ax + (bx - ax) * f) * k) - t / 2;
			const int py = y + (int)lroundf((ay + (by - ay) * f) * k) - t / 2;
			vgui2::surface()->DrawFilledRect(px, py, px + t, py + t);
		}
	};
	seg(2.0f, 7.5f, 5.0f, 10.5f);
	seg(5.0f, 10.5f, 12.0f, 3.5f);
}

// Boxes, markers and lines through completed text (before the text: text drawing ends alpha blending)
void PaintRowsBack(const std::vector<Row> &rows, int x, int y, vgui2::HFont rowFont, float alpha)
{
	const int lineTall = sbolook::FontTall(rowFont);
	for (const Row &r : rows)
	{
		const float a = alpha * r.alpha;
		const int iconY = y + (lineTall - U(U_ICON)) / 2;
		const int rx = r.part ? x + U(U_INDENT) : x;
		if (r.done)
		{
			DrawTick(rx, iconY, U(U_ICON), Fade(COL_DONE, a));
			const int tx = rx + U(U_ICON) + U(U_ICON_GAP);
			const int thick = std::max(1, U(0.7f));
			for (size_t i = 0; i < r.lines.size(); i++)
			{
				const int ly = y + (int)i * lineTall + lineTall / 2 + thick / 2;
				sbolook::FillRect(tx, ly, sbolook::TextWide(rowFont, r.lines[i]), thick, Fade(COL_DONE, a));
			}
		}
		else
			DrawMarker(rx, iconY, r.part ? U(U_ICON * 0.8f) : U(U_ICON), Fade(COL_TEAL, a));
		y += r.tall + U(U_GAP);
	}
}

void PaintRowsText(const std::vector<Row> &rows, int x, int y, int wide, vgui2::HFont rowFont,
    vgui2::HFont counterFont, float alpha)
{
	const int lineTall = sbolook::FontTall(rowFont);
	for (const Row &r : rows)
	{
		const float a = alpha * r.alpha;
		const int tx = x + (r.part ? U(U_INDENT) : 0) + U(U_ICON) + U(U_ICON_GAP);
		for (size_t i = 0; i < r.lines.size(); i++)
			sbolook::DrawText(rowFont, Fade(r.done ? COL_DONE : COL_TEXT, a), tx, y + (int)i * lineTall, r.lines[i]);
		if (!r.counter.empty())
			sbolook::DrawTextMid(counterFont, Fade(COL_TEAL, a),
			    x + wide - sbolook::TextWide(counterFont, r.counter), y, lineTall, r.counter);
		y += r.tall + U(U_GAP);
	}
}

int RowsTall(const std::vector<Row> &rows)
{
	int tall = 0;
	for (const Row &r : rows)
		tall += r.tall + U(U_GAP);
	return rows.empty() ? 0 : tall - U(U_GAP);
}

std::wstring ActiveLabel(int active)
{
	wchar_t buf[32];
	swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%d active", active);
	return buf;
}

std::wstring MoreLabel(int more)
{
	wchar_t buf[32];
	swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"+%d more", more);
	return buf;
}

// The panel's frame: what is drawn where (computed in OnThink, painted in PaintBackground and Paint)
struct HudFrame
{
	bool list = false;
	int x = 0, y = 0, wide = 0, tall = 0; // the panel's box
	std::vector<Row> rows;
	int more = 0;                         // objectives left out: "+N more"
	bool notice = false;
	int noticeKind = objective::NOTICE_NEW;
	float noticeAlpha = 0.0f;
	int nx = 0, ny = 0, nwide = 0, ntall = 0;
	std::vector<std::wstring> noticeLines;
} s_Frame;

// "objectives" (default key H, Controls "Objectives panel"): compact -> full -> hidden -> compact
void Cmd_Objectives()
{
	const int mode = hud_objectives.GetInt();
	hud_objectives.SetValue(mode == 1 ? 2 : (mode == 2 ? 0 : 1));
}

int MsgFunc_Objective(const char *pszName, int iSize, void *pbuf)
{
	if (CHudObjectives::Get())
		CHudObjectives::Get()->OnMessage(static_cast<const unsigned char *>(pbuf), iSize);
	return 1;
}
} // namespace

CHudObjectives *CHudObjectives::s_pInstance = nullptr;

CHudObjectives *CHudObjectives::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CHudObjectives(pParent);
	return s_pInstance;
}

CHudObjectives::CHudObjectives(vgui2::Panel *pParent)
    : BaseClass(pParent, "HudObjectives")
{
	s_pInstance = this;
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetProportional(false);
	SetBounds(0, 0, 640, 480); // OnThink: the parent's (screen) size
	SetVisible(true);          // always: VGUI2 doesn't think invisible panels; nothing is painted without objectives
}

void CHudObjectives::Reset()
{
	s_List.Reset();
	s_iResets++;
	s_Frame = HudFrame();
	m_flListY = -1.0f;
}

void CHudObjectives::OnMessage(const unsigned char *data, int size)
{
	objective::Message msg;
	if (!objective::Parse(data, size, msg))
	{
		sbo::DevPrint("Objective: a message that does not fit its format was ignored\n");
		return;
	}
	s_List.Apply(msg);

	// developer 1: what arrived
	static sbo::CvarHandle s_hDeveloper = nullptr;
	if (!s_hDeveloper)
		s_hDeveloper = sbo::FindCvar("developer");
	if (s_hDeveloper && sbo::CvarGetFloat(s_hDeveloper) > 0.0f)
	{
		char line[256];
		if (msg.slot == OBJECTIVE_SLOT_CLEAR)
			Q_snprintf(line, sizeof(line), "Objective: list cleared\n");
		else
			Q_snprintf(line, sizeof(line), "Objective: slot %d state %d flags 0x%02x %d/%d \"%s\"\n", msg.slot, msg.state,
			    msg.flags, msg.count, msg.target, msg.text);
		sbo::ConsolePrint(line);
	}
}

void CHudObjectives::OnThink()
{
	BaseClass::OnThink();

	int wide, tall, parentWide, parentTall;
	GetSize(wide, tall);
	g_pVGuiPanel->GetSize(GetVParent(), parentWide, parentTall);
	if (wide != parentWide || tall != parentTall)
		SetBounds(0, 0, parentWide, parentTall);

	// real time (the HUD time jumps when a map starts)
	const double now = vgui2::system()->GetCurrentTime();
	float dt = 0.0f;
	if (m_flLastThinkTime > 0 && now > m_flLastThinkTime)
		dt = (float)(now - m_flLastThinkTime);
	m_flLastThinkTime = now;

	// with the HUD; not under the scoreboard or a briefing / text window (they drew over each other)
	const bool windowOpen = VGui2_Score_IsVisible() || (CSboTextWindow::Get() && CSboTextWindow::Get()->IsVisible())
	    || (CSboTeamMenu::Get() && CSboTeamMenu::Get()->IsVisible());
	const bool visible = sbo::KillFeedShouldDraw() && !windowOpen;
	if (s_List.Tick(dt, visible))
		sbo::PlaySoundByName(NOTICE_SOUND, 1.0f);

	HudFrame f;
	const int boxWide = U(U_WIDE);
	const int inner = boxWide - 2 * U(U_PAD_X);

	// top left; below the spectator's top bar while it is up, and below a Game Master's status strip
	const int x = U(U_LEFT);
	int targetY = U(U_TOP);
	if (CSboSpectatorPanel::Get() && CSboSpectatorPanel::Get()->IsShown())
		targetY = std::max(targetY, U(U_SPECTATOR_TOP));
	if (VGui2_GmStripBottom() > 0)
		targetY = std::max(targetY, VGui2_GmStripBottom() + U(4.0f));
	if (m_flListY < 0.0f || dt <= 0.0f)
		m_flListY = (float)targetY;
	else
		m_flListY += ((float)targetY - m_flListY) * std::min(1.0f, dt * 12.0f);
	int y = (int)lroundf(m_flListY);

	const int mode = hud_objectives.GetInt();
	if (visible && (mode == 1 || mode == 2))
	{
		const vgui2::HFont rowFont = Font(U_ROW_FONT, 400), counterFont = Font(U_ROW_FONT * 0.87f, 700);
		objective::Line lines[OBJECTIVE_MAX];
		int more = 0, n;
		if (mode == 2)
		{
			// as many lines as fit the part of the screen it may take (one line each; a long text wraps once)
			const int lineTall = sbolook::FontTall(rowFont) + U(U_GAP);
			const int maxLines = std::max(3, (int)((parentTall * FULL_MAX_TALL - y) / lineTall) - 2);
			n = s_List.FullLines(lines, std::min(maxLines, OBJECTIVE_MAX), more);
		}
		else
			n = s_List.CompactLines(lines, objective::COMPACT_LINES, more);
		if (n > 0 || more > 0)
		{
			for (int i = 0; i < n; i++)
			{
				const float alpha = mode == 2 ? 1.0f : s_List.RowAlpha(lines[i].slot);
				f.rows.push_back(LayoutRow(lines[i].slot, lines[i].part, inner, rowFont, counterFont,
				    mode == 2 ? 1 : HUD_TEXT_LINES, alpha));
			}
			f.more = more;
			f.list = true;
			f.x = x;
			f.y = y;
			f.wide = boxWide;
			f.tall = 2 * U(U_PAD_Y) + sbolook::FontTall(Font(U_HEAD_FONT, 700)) + U(U_GAP) + RowsTall(f.rows);
			if (more > 0)
				f.tall += U(U_GAP) + sbolook::FontTall(Font(U_SMALL_FONT, 400));
			y += f.tall + U(U_NOTICE_GAP);
		}
	}

	// the notice: under the panel (where the panel is when it is hidden), sliding in from the left
	int noticeSlot = -1, noticeKind = objective::NOTICE_NEW;
	float noticeAlpha = 0.0f;
	if (visible && s_List.Notice(noticeSlot, noticeKind, noticeAlpha) && noticeAlpha > 0.0f)
	{
		const vgui2::HFont noticeFont = Font(U_NOTICE_FONT, 700);
		f.notice = true;
		f.noticeKind = noticeKind;
		f.noticeAlpha = noticeAlpha;
		f.noticeLines = Wrap(noticeFont, ObjectiveText(s_List.Get(noticeSlot)), inner, 3);
		f.nwide = boxWide;
		// the slide: from off the left edge to its place while it fades in (ease out)
		const float in = std::min(1.0f, noticeAlpha);
		const float slide = 1.0f - (1.0f - in) * (1.0f - in);
		f.nx = x - (int)((1.0f - slide) * (boxWide + x));
		f.ny = y;
		f.ntall = 2 * U(U_NOTICE_PAD_Y) + sbolook::FontTall(Font(U_SMALL_FONT, 700)) + U(2.7f)
		    + (int)f.noticeLines.size() * sbolook::FontTall(noticeFont);
	}
	s_Frame = f;
}

void CHudObjectives::PaintBackground()
{
	const HudFrame &f = s_Frame;
	if (f.list)
	{
		sbolook::FillRect(f.x, f.y, f.wide, f.tall, COL_BOX);
		sbolook::OutlineRect(f.x, f.y, f.wide, f.tall, COL_BOX_BORDER);
		const int rowsY = f.y + U(U_PAD_Y) + sbolook::FontTall(Font(U_HEAD_FONT, 700)) + U(U_GAP);
		PaintRowsBack(f.rows, f.x + U(U_PAD_X), rowsY, Font(U_ROW_FONT, 400), 1.0f);
	}
	if (f.notice)
	{
		sbolook::FillRect(f.nx, f.ny, f.nwide, f.ntall, Fade(COL_NOTICE_BOX, f.noticeAlpha));
		sbolook::OutlineRect(f.nx, f.ny, f.nwide, f.ntall,
		    Fade(f.noticeKind == objective::NOTICE_DONE ? COL_TEAL : COL_AMBER, f.noticeAlpha));
	}
}

void CHudObjectives::Paint()
{
	const HudFrame &f = s_Frame;
	if (f.list)
	{
		const vgui2::HFont headFont = Font(U_HEAD_FONT, 700), smallFont = Font(U_SMALL_FONT, 400);
		const int hx = f.x + U(U_PAD_X), hy = f.y + U(U_PAD_Y), headTall = sbolook::FontTall(headFont);
		sbolook::DrawSpacedUpper(headFont, COL_TEAL, hx, hy, "Objectives");
		const std::wstring active = ActiveLabel(s_List.ActiveCount());
		sbolook::DrawTextMid(smallFont, COL_DIM, f.x + f.wide - U(U_PAD_X) - sbolook::TextWide(smallFont, active), hy,
		    headTall, active);
		PaintRowsText(f.rows, hx, hy + headTall + U(U_GAP), f.wide - 2 * U(U_PAD_X), Font(U_ROW_FONT, 400),
		    Font(U_ROW_FONT * 0.87f, 700), 1.0f);
		if (f.more > 0)
			sbolook::DrawText(smallFont, COL_DIM, hx, hy + headTall + U(U_GAP) + RowsTall(f.rows) + U(U_GAP),
			    MoreLabel(f.more));
	}
	if (f.notice)
	{
		const vgui2::HFont titleFont = Font(U_SMALL_FONT, 700), noticeFont = Font(U_NOTICE_FONT, 700);
		const int nx = f.nx + U(U_PAD_X);
		int ny = f.ny + U(U_NOTICE_PAD_Y);
		if (f.noticeKind == objective::NOTICE_DONE)
			sbolook::DrawSpacedUpper(titleFont, Fade(COL_TEAL, f.noticeAlpha), nx, ny, "Objective complete");
		else
			sbolook::DrawSpacedUpper(titleFont, Fade(COL_AMBER, f.noticeAlpha), nx, ny, "New objective");
		ny += sbolook::FontTall(titleFont) + U(2.7f);
		for (const std::wstring &line : f.noticeLines)
		{
			sbolook::DrawText(noticeFont, Fade(COL_NOTICE_TEXT, f.noticeAlpha), nx, ny, line);
			ny += sbolook::FontTall(noticeFont);
		}
	}
}

//-----------------------------------------------------------------------------
// The scoreboard's line: "OBJECTIVES  3/9  ·  Next: <the newest active one>  ·  +2 more"
//-----------------------------------------------------------------------------
namespace
{
std::wstring SummaryText()
{
	int done = 0;
	const int total = s_List.TopCount(done);
	wchar_t buf[64];
	swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%d/%d", done, total);
	std::wstring text = buf;
	const int newest = s_List.NewestActive();
	if (newest >= 0)
	{
		const objective::Entry &e = s_List.Get(newest);
		text += L"  \u00b7  Next: " + ObjectiveText(e);
		if (e.target > 0)
		{
			swprintf(buf, sizeof(buf) / sizeof(buf[0]), L" %d/%d", e.count, e.target);
			text += buf;
		}
		const int others = s_List.ActiveCount() - 1;
		if (others > 0)
			text += L"  \u00b7  " + MoreLabel(others);
	}
	return text;
}
} // namespace

int Objectives_SummaryTall()
{
	if (s_List.ShownCount() <= 0)
		return 0;
	return sbolook::FontTall(Font(U_ROW_FONT * 0.93f, 400)) + 2 * U(3.0f);
}

void Objectives_PaintSummary(int x, int y, int wide, int hint)
{
	if (s_List.ShownCount() <= 0 || wide <= 0)
		return;
	const vgui2::HFont headFont = Font(U_HEAD_FONT, 700), font = Font(U_ROW_FONT * 0.93f, 400);
	const vgui2::HFont smallFont = Font(U_SMALL_FONT, 400);
	const int tall = Objectives_SummaryTall();
	sbolook::FillRect(x, y, wide, tall, COL_FULL_BOX);
	sbolook::OutlineRect(x, y, wide, tall, hint ? COL_BOX_BORDER : COL_FULL_BORDER);
	const int hx = x + U(U_PAD_X);
	sbolook::DrawSpacedUpper(headFont, COL_TEAL, hx, y + (tall - sbolook::FontTall(headFont)) / 2, "Objectives");
	const int tx = hx + sbolook::SpacedUpperWide(headFont, "Objectives") + U(U_ICON_GAP);
	int right = x + wide - U(U_PAD_X);
	// the mouse is on: what a click does, at the right end
	if (hint)
	{
		const std::wstring h = hint == 1 ? L"\u25b4 Click for all objectives" : L"\u25be Click to close";
		const int hw = sbolook::TextWide(smallFont, h);
		sbolook::DrawTextMid(smallFont, COL_TEAL, right - hw, y, tall, h);
		right -= hw + U(U_ICON_GAP);
	}
	sbolook::DrawTextMid(font, COL_TEXT, tx, y, tall, SummaryText(), right - tx);
}

int Objectives_PaintFullListUp(int x, int bottom, int wide, int maxTall)
{
	if (s_List.ShownCount() <= 0 || wide <= 0)
		return bottom;
	const vgui2::HFont headFont = Font(U_HEAD_FONT, 700), smallFont = Font(U_SMALL_FONT, 400);
	const vgui2::HFont rowFont = Font(U_ROW_FONT * 0.93f, 400), counterFont = Font(U_ROW_FONT * 0.87f, 700);
	const int inner = wide - 2 * U(U_PAD_X);
	const int headTall = sbolook::FontTall(headFont);
	const int fixed = 2 * U(U_PAD_Y) + headTall + U(U_GAP) + U(U_GAP) + sbolook::FontTall(smallFont);
	const int lineTall = sbolook::FontTall(rowFont) + U(U_GAP);
	const int maxLines = std::max(1, std::min(OBJECTIVE_MAX, (maxTall - fixed) / std::max(1, lineTall)));
	objective::Line lines[OBJECTIVE_MAX];
	int more = 0;
	const int n = s_List.FullLines(lines, maxLines, more);
	std::vector<Row> rows;
	for (int i = 0; i < n; i++)
		rows.push_back(LayoutRow(lines[i].slot, lines[i].part, inner, rowFont, counterFont, 1, 1.0f));
	int tall = 2 * U(U_PAD_Y) + headTall + U(U_GAP) + RowsTall(rows);
	if (more > 0)
		tall += U(U_GAP) + sbolook::FontTall(smallFont);
	const int y = bottom - tall;
	sbolook::FillRect(x, y, wide, tall, Color(10, 14, 18, 240));
	sbolook::OutlineRect(x, y, wide, tall, COL_BOX_BORDER);
	const int hx = x + U(U_PAD_X), hy = y + U(U_PAD_Y), rowsY = hy + headTall + U(U_GAP);
	PaintRowsBack(rows, hx, rowsY, rowFont, 1.0f);
	sbolook::DrawSpacedUpper(headFont, COL_TEAL, hx, hy, "Mission objectives");
	const std::wstring active = ActiveLabel(s_List.ActiveCount());
	sbolook::DrawTextMid(smallFont, COL_DIM, x + wide - U(U_PAD_X) - sbolook::TextWide(smallFont, active), hy, headTall,
	    active);
	PaintRowsText(rows, hx, rowsY, inner, rowFont, counterFont, 1.0f);
	if (more > 0)
		sbolook::DrawText(smallFont, COL_DIM, hx, rowsY + RowsTall(rows) + U(U_GAP), MoreLabel(more));
	return y;
}

int Objectives_Revision()
{
	return s_List.Revision() + s_iResets * 1000003;
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_Objectives_Init()
{
	if (CHudObjectives::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	hud_objectives.Register();
	sbo::AddCommand("objectives", Cmd_Objectives);

	CHudObjectives *pPanel = CHudObjectives::Create(g_pViewport);
	pPanel->MakeReadyForUse();
	pPanel->Reset();

	sbo::HookUserMsg(OBJECTIVE_MSG_NAME, &MsgFunc_Objective);
	return true;
}

void VGui2_Objectives_Reset()
{
	if (CHudObjectives::Get())
		CHudObjectives::Get()->Reset();
}
