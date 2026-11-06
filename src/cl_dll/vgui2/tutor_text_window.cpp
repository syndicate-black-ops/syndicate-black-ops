//========= CS:CZ tutor: VGUI2 tutor window =========//
//
// Purpose: see tutor_text_window.h.
//=================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <tier2/tier2.h>
#include <IGameUIFuncs.h>
#include <tier1/strtools.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_menus.h"
#include "tga_image.h"
#include "tutor_text_window.h"
#include "vgui2_api.h"

#define TUTOR_SCHEME_FILE VGUI2_ROOT_DIR "resource/tutorscheme.res"
#define TUTOR_LAYOUT_FILE VGUI2_ROOT_DIR "resource/ui/tutortextwindow.res"

// CZ's font (tutorscheme.res "Default" at 640x480 .. 1024x768): the .res sizes are pixels for this font height
static const float TUTOR_BASE_FONT_TALL = 14.0f;

// tutortextwindow.res controls (CZ's names); the defaults are CZ's file
enum
{
	TUTOR_CTRL_WINDOW = 0,
	TUTOR_CTRL_TOPLEFT,
	TUTOR_CTRL_TOPRIGHT,
	TUTOR_CTRL_BOTTOMLEFT,
	TUTOR_CTRL_BOTTOMRIGHT,
	TUTOR_CTRL_ICON,
	TUTOR_CTRL_GAP,
	TUTOR_CTRL_LABEL,
	TUTOR_CTRL_COUNT
};

struct TutorRect
{
	const char *name;
	int x, y, wide, tall;
};

static TutorRect s_Layout[TUTOR_CTRL_COUNT] = {
	{ "TutorTextWindow", 0, 0, 200, 200 },
	{ "TopLeftPanel", 0, 0, 10, 10 },
	{ "TopRightPanel", 190, 0, 10, 10 },
	{ "BottomLeftPanel", 0, 190, 10, 10 },
	{ "BottomRightPanel", 190, 190, 10, 10 },
	{ "IconPanel", 10, 10, 25, 25 },
	{ "GapPanel", 40, 0, 2, 200 },
	{ "TutorMessageLabel", 50, 10, 135, 180 },
};

static vgui2::HScheme s_hTutorScheme = 0;
static bool s_bInitialized = false;

//-----------------------------------------------------------------------------
// Images (loaded once; CZ: CBitmapImagePanel)
//-----------------------------------------------------------------------------
static const int TUTOR_MAX_IMAGES = 32;
static char s_ImageNames[TUTOR_MAX_IMAGES][128];
static CTGAImage *s_pImages[TUTOR_MAX_IMAGES];

static CTGAImage *GetTutorImage(const char *name)
{
	if (!name || !name[0])
		return nullptr;

	for (int i = 0; i < TUTOR_MAX_IMAGES; i++)
	{
		if (s_pImages[i] && !strcmp(s_ImageNames[i], name))
			return s_pImages[i];
	}

	// a free slot, else the last one is reused (more icons than slots: maps' custom icons)
	int slot = TUTOR_MAX_IMAGES - 1;
	for (int i = 0; i < TUTOR_MAX_IMAGES; i++)
	{
		if (!s_pImages[i])
		{
			slot = i;
			break;
		}
	}

	delete s_pImages[slot];
	strncpy(s_ImageNames[slot], name, sizeof(s_ImageNames[slot]) - 1);
	s_ImageNames[slot][sizeof(s_ImageNames[slot]) - 1] = '\0';
	s_pImages[slot] = new CTGAImage(name);
	return s_pImages[slot];
}

static void DrawTutorImage(const char *name, int x, int y, int wide, int tall, Color color)
{
	CTGAImage *pImage = GetTutorImage(name);
	if (!pImage)
		return;

	pImage->SetPos(x, y);
	pImage->SetSize(wide, tall);
	pImage->SetColor(color);
	pImage->Paint();
}

static Color ToColor(const unsigned char c[4])
{
	return Color(c[0], c[1], c[2], c[3]);
}

//-----------------------------------------------------------------------------
// CTutorMessageBox (CZ: CTutorTextWindow + CTutorMessageBox)
//-----------------------------------------------------------------------------
class CTutorMessageBox : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CTutorMessageBox, vgui2::Panel);

public:
	CTutorMessageBox(vgui2::Panel *pParent, const wchar_t *text, const TutorWindowParams &params);
	virtual ~CTutorMessageBox();

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PerformLayout() override;
	virtual void OnThink() override;
	virtual void PaintBackground() override;
	virtual void Paint() override;

private:
	int Scaled(int value) const { return (int)(value * m_flScale + 0.5f); }
	bool HasIcon() const { return m_Params.icon[0] != '\0'; }
	int LabelX() const;
	void UpdatePosition();
	void WrapText(int maxWide);

	wchar_t *m_pText = nullptr;
	TutorWindowParams m_Params;

	vgui2::HFont m_hFont = vgui2::INVALID_FONT;
	Color m_TextColor;
	Color m_IconColor;
	Color m_BgColor;
	Color m_BorderColor;
	float m_flScale = 1.0f;

	// wrapped lines: offset into m_pText and length
	static const int MAX_LINES = 32;
	int m_iLineStart[MAX_LINES] = {};
	int m_iLineLen[MAX_LINES] = {};
	int m_iLineCount = 0;
	int m_iTextWide = 0;
	int m_iTextTall = 0;
};

CTutorMessageBox::CTutorMessageBox(vgui2::Panel *pParent, const wchar_t *text, const TutorWindowParams &params)
    : BaseClass(pParent, "TutorTextWindow")
{
	size_t len = wcslen(text);
	m_pText = new wchar_t[len + 1];
	memcpy(m_pText, text, (len + 1) * sizeof(wchar_t));

	m_Params = params;

	// CZ: a frame without title bar, not moveable/sizeable, no mouse or keyboard input
	SetMouseInputEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetScheme(s_hTutorScheme);
	SetVisible(true);
	MoveToFront();
	InvalidateLayout(false, true);
}

CTutorMessageBox::~CTutorMessageBox()
{
	delete[] m_pText;
}

void CTutorMessageBox::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// CZ: label colour "TutorTextClass<type>", fallback "TutorTextClassDefault"; the icon is drawn in it too.
	// env_tutor's colours override them.
	char className[64];
	snprintf(className, sizeof(className), "TutorTextClass%d", m_Params.type);
	Color defaultColor = pScheme->GetColor("TutorTextClassDefault", Color(28, 207, 61, 255));
	m_TextColor = m_Params.hasTextColor ? ToColor(m_Params.textColor) : pScheme->GetColor(className, defaultColor);

	if (m_Params.hasIconColor)
		m_IconColor = ToColor(m_Params.iconColor);
	else
		m_IconColor = m_Params.tintIcon ? m_TextColor : Color(255, 255, 255, 255);

	m_BgColor = m_Params.hasBgColor ? ToColor(m_Params.bgColor) : pScheme->GetColor("WindowBG", Color(0, 0, 0, 200));
	m_BorderColor = m_Params.hasBorderColor ? ToColor(m_Params.borderColor)
	                                        : pScheme->GetColor("BorderBright", Color(28, 207, 61, 128));

	m_hFont = pScheme->GetFont("Default");
	int fontTall = m_hFont != vgui2::INVALID_FONT ? vgui2::surface()->GetFontTall(m_hFont) : 0;
	m_flScale = fontTall > 0 ? fontTall / TUTOR_BASE_FONT_TALL : 1.0f;
	if (m_flScale < 1.0f)
		m_flScale = 1.0f;

	SetBgColor(m_BgColor);
	InvalidateLayout();
}

// Without an icon the text takes the icon's place
int CTutorMessageBox::LabelX() const
{
	return HasIcon() ? Scaled(s_Layout[TUTOR_CTRL_LABEL].x) : Scaled(s_Layout[TUTOR_CTRL_ICON].x);
}

void CTutorMessageBox::WrapText(int maxWide)
{
	m_iLineCount = 0;
	m_iTextWide = 0;
	m_iTextTall = 0;

	if (m_hFont == vgui2::INVALID_FONT)
		return;

	const wchar_t *text = m_pText;
	int pos = 0;

	while (m_iLineCount < MAX_LINES)
	{
		if (!text[pos] && m_iLineCount > 0)
			break;

		// one line: up to the next "\n", or the last space before maxWide
		const int start = pos;
		int wide = 0;
		int lastSpace = -1;
		int lastSpaceWide = 0;

		while (text[pos] && text[pos] != L'\n')
		{
			const int charWide = vgui2::surface()->GetCharacterWidth(m_hFont, text[pos]);
			if (wide + charWide > maxWide && pos > start)
				break;

			if (text[pos] == L' ')
			{
				lastSpace = pos;
				lastSpaceWide = wide;
			}

			wide += charWide;
			pos++;
		}

		int end, lineWide, next;
		if (text[pos] && text[pos] != L'\n')
		{
			// too wide: break at the last space (dropped), or inside a word that fills the line alone
			if (lastSpace > start)
			{
				end = lastSpace;
				lineWide = lastSpaceWide;
				next = lastSpace + 1;
			}
			else
			{
				end = pos;
				lineWide = wide;
				next = pos;
			}
		}
		else
		{
			end = pos;
			lineWide = wide;
			next = text[pos] ? pos + 1 : pos;
		}

		m_iLineStart[m_iLineCount] = start;
		m_iLineLen[m_iLineCount] = end - start;
		m_iLineCount++;
		if (lineWide > m_iTextWide)
			m_iTextWide = lineWide;

		if (!text[next] && next == end)
			break;

		pos = next;
	}

	m_iTextTall = m_iLineCount * vgui2::surface()->GetFontTall(m_hFont);
}

// CZ CTutorMessageBox::PerformLayout: the label is sized to its text (at least the icon's height) and the frame,
// the right corners, the bottom corners and the gap panel move/grow by the label's size change
void CTutorMessageBox::PerformLayout()
{
	BaseClass::PerformLayout();

	const TutorRect &window = s_Layout[TUTOR_CTRL_WINDOW];
	const TutorRect &label = s_Layout[TUTOR_CTRL_LABEL];
	const TutorRect &icon = s_Layout[TUTOR_CTRL_ICON];

	WrapText(Scaled(label.wide * 2));

	int labelWide = m_iTextWide;
	int labelTall = m_iTextTall;
	if (HasIcon() && labelTall < Scaled(icon.tall))
		labelTall = Scaled(icon.tall);

	// the right margin stays the .res file's; the label starts at LabelX()
	int wide = LabelX() + labelWide + (Scaled(window.wide) - Scaled(label.x) - Scaled(label.wide));
	int tall = Scaled(window.tall) + labelTall - Scaled(label.tall);
	SetSize(wide, tall);

	UpdatePosition();
}

// CZ CTutorTextWindow::Activate / CTutorMessageBox::OnThink: right edge 10 pixels from the screen's, top 10 pixels
// down (a dead player's window 1/8 of the screen down, below the spectator bar). SBO: env_tutor's alignment
// (which edge or centre of the window is placed) and its x / y (where, as a fraction of the screen; -1 = 10
// pixels from the screen's edge, or centred). A dead player's window keeps 1/8 of the screen clear at the top
// and the bottom (the spectator bars) when y is -1.
void CTutorMessageBox::UpdatePosition()
{
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);

	int align = m_Params.align;
	if (align < 0 || align >= TUTOR_ALIGN_COUNT)
		align = TUTOR_ALIGN_TOP_RIGHT;

	// 0 = left / top, 1 = centre, 2 = right / bottom
	static const int s_Column[TUTOR_ALIGN_COUNT] = { 2, 0, 1, 0, 1, 2, 0, 1, 2 };
	static const int s_Row[TUTOR_ALIGN_COUNT] = { 0, 0, 0, 1, 1, 1, 2, 2, 2 };
	const int column = s_Column[align];
	const int row = s_Row[align];

	const int wide = GetWide();
	const int tall = GetTall();
	const int margin = Scaled(10);

	int x, y;
	if (m_Params.x >= 0.0f)
	{
		int anchor = (int)(m_Params.x * screenWide);
		x = column == 0 ? anchor : (column == 1 ? anchor - wide / 2 : anchor - wide);
	}
	else
		x = column == 0 ? margin : (column == 1 ? (screenWide - wide) / 2 : screenWide - wide - margin);

	if (m_Params.y >= 0.0f)
	{
		int anchor = (int)(m_Params.y * screenTall);
		y = row == 0 ? anchor : (row == 1 ? anchor - tall / 2 : anchor - tall);
	}
	else if (row == 0)
		y = m_Params.isDead ? screenTall / 8 : margin;
	else if (row == 1)
		y = (screenTall - tall) / 2;
	else
		y = m_Params.isDead ? screenTall - screenTall / 8 - tall : screenTall - tall - margin;

	// on the screen
	if (x > screenWide - wide)
		x = screenWide - wide;
	if (x < 0)
		x = 0;
	if (y > screenTall - tall)
		y = screenTall - tall;
	if (y < 0)
		y = 0;

	int curX, curY;
	GetPos(curX, curY);
	if (curX != x || curY != y)
		SetPos(x, y);
}

void CTutorMessageBox::OnThink()
{
	BaseClass::OnThink();
	UpdatePosition();
}

void CTutorMessageBox::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);

	vgui2::surface()->DrawSetColor(m_BgColor);

	if (!m_Params.roundedCorners)
	{
		// CZ: filled rectangle and a BorderBright outline
		vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
		vgui2::surface()->DrawSetColor(m_BorderColor);
		vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
		return;
	}

	// CZ: the corner images (black quarter circles, drawn white * WindowBG's alpha) and the rectangles between them
	const int corner = Scaled(s_Layout[TUTOR_CTRL_TOPLEFT].wide);
	vgui2::surface()->DrawFilledRect(corner, 0, wide - corner, tall);
	vgui2::surface()->DrawFilledRect(0, corner, corner, tall - corner);
	vgui2::surface()->DrawFilledRect(wide - corner, corner, wide, tall - corner);

	Color cornerColor(255, 255, 255, m_BgColor.a());
	DrawTutorImage(VGUI2_ROOT_DIR "gfx/career/round_corner_nw", 0, 0, corner, corner, cornerColor);
	DrawTutorImage(VGUI2_ROOT_DIR "gfx/career/round_corner_ne", wide - corner, 0, corner, corner, cornerColor);
	DrawTutorImage(VGUI2_ROOT_DIR "gfx/career/round_corner_sw", 0, tall - corner, corner, corner, cornerColor);
	DrawTutorImage(VGUI2_ROOT_DIR "gfx/career/round_corner_se", wide - corner, tall - corner, corner, corner,
	    cornerColor);
}

void CTutorMessageBox::Paint()
{
	const TutorRect &icon = s_Layout[TUTOR_CTRL_ICON];
	const TutorRect &label = s_Layout[TUTOR_CTRL_LABEL];

	if (HasIcon())
	{
		DrawTutorImage(m_Params.icon, Scaled(icon.x), Scaled(icon.y), Scaled(icon.wide), Scaled(icon.tall),
		    m_IconColor);
	}

	if (m_hFont == vgui2::INVALID_FONT)
		return;

	const int fontTall = vgui2::surface()->GetFontTall(m_hFont);
	vgui2::surface()->DrawSetTextFont(m_hFont);
	vgui2::surface()->DrawSetTextColor(m_TextColor);

	for (int i = 0; i < m_iLineCount; i++)
	{
		if (m_iLineLen[i] <= 0)
			continue;

		vgui2::surface()->DrawSetTextPos(LabelX(), Scaled(label.y) + i * fontTall);
		vgui2::surface()->DrawPrintText(m_pText + m_iLineStart[i], m_iLineLen[i]);
	}
}

//-----------------------------------------------------------------------------
// TutorWindowManager
//-----------------------------------------------------------------------------
static CTutorMessageBox *s_pWindow = nullptr;
static int s_iWindowID = -1;

bool TutorWindow_PresetIcon(int icon, TutorWindowParams &params)
{
	// CZ's tutor uses icon_i and icon_skulls (grey: drawn in the text colour); icon_! and the bigger, already
	// coloured icons are from CZ's other screens (drawn in their own colours)
	const char *name = nullptr;
	bool tint = true;
	switch (icon)
	{
	case TUTOR_ICON_I:
		name = "icon_i";
		break;
	case TUTOR_ICON_EXCLAMATION:
		name = "icon_!";
		break;
	case TUTOR_ICON_SKULLS:
		name = "icon_skulls";
		break;
	case TUTOR_ICON_I_BIGGER:
		name = "icon_i-bigger";
		tint = false;
		break;
	case TUTOR_ICON_EXCLAMATION_BIGGER:
		name = "icon_!-bigger";
		tint = false;
		break;
	default:
		params.icon[0] = '\0';
		return false;
	}

	snprintf(params.icon, sizeof(params.icon), VGUI2_ROOT_DIR "gfx/career/%s", name);
	params.tintIcon = tint;
	return true;
}

void TutorWindow_Close()
{
	if (s_pWindow)
	{
		s_pWindow->SetVisible(false);
		s_pWindow->MarkForDeletion();
		s_pWindow = nullptr;
	}

	s_iWindowID = -1;
}

bool TutorWindow_IsOpen()
{
	return s_pWindow != nullptr;
}

void TutorWindow_Show(const wchar_t *text, const TutorWindowParams &params)
{
	TutorWindow_Close();

	if (!s_bInitialized || !g_pViewport)
		return;

	s_pWindow = new CTutorMessageBox(g_pViewport, text, params);
	s_iWindowID = params.id;
}

//-----------------------------------------------------------------------------
// Messages
//-----------------------------------------------------------------------------
namespace
{
// BEGIN_READ / READ_BYTE / READ_SHORT / READ_STRING
struct CTutorMsgReader
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

	void ReadColor(unsigned char *color, bool alpha)
	{
		for (int i = 0; i < (alpha ? 4 : 3); i++)
			color[i] = (unsigned char)ReadByte();
		if (!alpha)
			color[3] = 255;
	}
};

const int TUTOR_TEXT_CHARS = 2048;

// The text of a TutorText: TutorPart pieces first (raw bytes, decoded once complete)
char s_PendingText[TUTOR_TEXT_CHARS];

void AppendPending(const char *piece)
{
	const size_t len = strlen(s_PendingText);
	strncpy(s_PendingText + len, piece, sizeof(s_PendingText) - len - 1);
	s_PendingText[sizeof(s_PendingText) - 1] = '\0';
}

void AppendWide(wchar_t *out, size_t &o, const wchar_t *value)
{
	for (; *value && o < (size_t)TUTOR_TEXT_CHARS - 1; value++)
		out[o++] = *value;
}

bool IsCommandChar(wchar_t c)
{
	return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_';
}

// Does the binding run the command? 'command' has no '+'/'-'. A binding's leading '+' is skipped (the engine binds
// "+reload", the key's release runs "-reload"). wholeBinding: only "+reload" itself; otherwise any command of a
// multi-command binding ("+reload; say reloading") with any arguments ("impulse 201").
bool BindingRunsCommand(const char *binding, const char *command, bool wholeBinding)
{
	const size_t len = strlen(command);
	while (binding && *binding)
	{
		while (*binding == ' ' || *binding == '\t')
			binding++;
		if (*binding == '+')
			binding++;

		if (!Q_strnicmp(binding, command, (int)len))
		{
			const char next = binding[len];
			if (!next || (!wholeBinding && (next == ' ' || next == '\t' || next == ';')))
				return true;
		}

		if (wholeBinding)
			return false;
		binding = strchr(binding, ';');
		if (binding)
			binding++;
	}
	return false;
}

// The engine's name of the primary key bound to the command ("r", "MOUSE1", "SPACE"), nullptr if none: the key
// Options > Keyboard shows in its "Key" column, not the "Alternate" one. GameUI (COptionsSubKeyboard::
// FillInCurrentBindings) walks the keys 0..255 and AddBinding moves the item's key to the alternate slot for each
// further key, so the highest key number is the primary one (MOUSE1 241 over ENTER 13).
// Scans the engine's bindings with IGameUIFuncs::Key_BindingForKey/Key_NameForKey (the engine's GameUI interface,
// as BHL's MOTD uses GetVGUI2KeyCodeForBind): cl_enginefunc_t::Key_LookupBinding (the engine's Key_NameForBinding)
// skips the binding's '+' and compares only strlen(argument) characters, so "+reload" never matches and
// "attack" also matches a "+attack2" key. It's only the fallback if the interface is missing.
const char *FindKeyForCommand(const char *command)
{
	if (command[0] == '+' || command[0] == '-')
		command++;
	if (!command[0])
		return nullptr;

	if (!g_pGameUIFuncs)
		return sbo::LookupKeyBinding(command);

	const int NUM_ENGINE_KEYS = 256; // keys.c keybindings[256]
	for (int pass = 0; pass < 2; pass++)
	{
		for (int keyNum = NUM_ENGINE_KEYS - 1; keyNum >= 0; keyNum--)
		{
			const char *binding = g_pGameUIFuncs->Key_BindingForKey(keyNum);
			if (!binding || !binding[0] || !BindingRunsCommand(binding, command, pass == 0))
				continue;

			const char *key = g_pGameUIFuncs->Key_NameForKey(keyNum);
			if (key && key[0])
				return key;
		}
	}
	return nullptr;
}

// "%+command%" (or "%command%"): the key bound to the command, Source's convention (UTIL_ReplaceKeyBindings,
// the instructor's hint bindings), e.g. "%+reload%" -> "R". Unbound: "<unbound: +reload>".
void KeyForCommand(const char *command, wchar_t *out, int outChars)
{
	const char *key = FindKeyForCommand(command);

	char name[96];
	if (key)
	{
		// the engine's key names: "r", "MOUSE1", "SHIFT", ... -> upper case
		size_t i = 0;
		for (; key[i] && i < sizeof(name) - 1; i++)
			name[i] = (key[i] >= 'a' && key[i] <= 'z') ? (char)(key[i] - 'a' + 'A') : key[i];
		name[i] = '\0';
	}
	else
		snprintf(name, sizeof(name), "<unbound: %s>", command);

	sboui::TextToWide(name, out, outChars * (int)sizeof(wchar_t));
}

// "%+command%" / "%command%" -> the key, "%n" -> the local player's name (env_tutor's texts and tokens)
void ReplacePlaceholders(wchar_t *text)
{
	static wchar_t out[TUTOR_TEXT_CHARS];
	size_t o = 0;

	for (const wchar_t *p = text; *p && o < (size_t)TUTOR_TEXT_CHARS - 1;)
	{
		if (*p == L'%')
		{
			const wchar_t *q = p + 1;
			if (*q == L'+' || *q == L'-')
				q++;
			const wchar_t *nameStart = q;
			while (IsCommandChar(*q) && q - p < 64)
				q++;

			if (q > nameStart && *q == L'%')
			{
				char command[72];
				int n = 0;
				for (const wchar_t *c = p + 1; c < q; c++)
					command[n++] = (char)*c;
				command[n] = '\0';

				wchar_t key[128];
				KeyForCommand(command, key, sizeof(key) / sizeof(key[0]));
				AppendWide(out, o, key);
				p = q + 1;
				continue;
			}

			if (p[1] == L'n')
			{
				// colour codes (^0..^9, ^xRGB, ^#RRGGBB) follow hud_colortext as far as plain text can: removed, or as typed (0)
				const char *name = sbo::GetPlayerName(sbo::GetLocalPlayerIndex());
				wchar_t value[128];
				sboui::TextToWide(VGui2_Chat_RemoveColorCodes(name ? name : ""), value, sizeof(value));
				AppendWide(out, o, value);
				p += 2;
				continue;
			}
		}

		out[o++] = *p++;
	}

	out[o] = 0;
	memcpy(text, out, (o + 1) * sizeof(wchar_t));
}

// The localizer may leave a literal backslash-n (see sboui::Localize); map texts may contain one too
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
		else
			*out++ = *in;
	}
	*out = 0;
}

int MsgFunc_TutorClose(const char *pszName, int iSize, void *pbuf)
{
	s_PendingText[0] = '\0';
	TutorWindow_Close();
	return 1;
}
} // namespace

int TutorWindow_MsgFunc_TutorPart(const char *pszName, int iSize, void *pbuf)
{
	CTutorMsgReader msg = { static_cast<const unsigned char *>(pbuf), iSize, 0 };

	char piece[256];
	msg.ReadString(piece, sizeof(piece));
	AppendPending(piece);
	return 1;
}

// CZ TeamFortressViewport::MsgFunc_TutorText + TutorWindowManager::NewTutorWindow. The message (server:
// src/dlls/tutor.cpp SendTutorText): short id, short type, byte flags, byte align, short x, short y (-1 or
// 0..10000), byte icon [string path], string sound, the colours flagged, string text (after TutorPart pieces).
int TutorWindow_MsgFunc_TutorText(const char *pszName, int iSize, void *pbuf)
{
	CTutorMsgReader msg = { static_cast<const unsigned char *>(pbuf), iSize, 0 };

	TutorWindowParams params;
	params.id = msg.ReadShort();
	params.type = msg.ReadShort();
	if (params.type <= 0)
		params.type = TUTOR_TYPE_DEFAULT;

	const int flags = msg.ReadByte();
	params.isDead = (flags & TUTOR_FLAG_DEAD) != 0;
	params.align = msg.ReadByte();

	const int x = msg.ReadShort();
	const int y = msg.ReadShort();
	params.x = x < 0 ? -1.0f : x / 10000.0f;
	params.y = y < 0 ? -1.0f : y / 10000.0f;

	const int icon = msg.ReadByte();
	if (icon == TUTOR_ICON_CUSTOM)
	{
		// a map's .tga (precached by env_tutor, so it was downloaded), relative to the game directory
		msg.ReadString(params.icon, sizeof(params.icon));
		params.tintIcon = false;
	}
	else
		TutorWindow_PresetIcon(icon, params);

	char sound[128];
	msg.ReadString(sound, sizeof(sound));

	if (flags & TUTOR_FLAG_TEXT_COLOR)
	{
		msg.ReadColor(params.textColor, false);
		params.hasTextColor = true;
	}
	if (flags & TUTOR_FLAG_ICON_COLOR)
	{
		msg.ReadColor(params.iconColor, false);
		params.hasIconColor = true;
	}
	if (flags & TUTOR_FLAG_BG_COLOR)
	{
		msg.ReadColor(params.bgColor, true);
		params.hasBgColor = true;
	}
	if (flags & TUTOR_FLAG_BORDER_COLOR)
	{
		msg.ReadColor(params.borderColor, true);
		params.hasBorderColor = true;
	}

	char last[256];
	msg.ReadString(last, sizeof(last));
	AppendPending(last);

	static char raw[TUTOR_TEXT_CHARS];
	strncpy(raw, s_PendingText, sizeof(raw) - 1);
	raw[sizeof(raw) - 1] = '\0';
	s_PendingText[0] = '\0';

	static wchar_t text[TUTOR_TEXT_CHARS];
	text[0] = 0;

	// "#token": localized (raw if no language file has it), else the text itself
	const wchar_t *localized = nullptr;
	if (raw[0] == '#')
	{
		localized = g_pVGuiLocalize->Find(raw + 1);
		if (!localized)
			localized = g_pVGuiLocalize->Find(raw);
	}

	if (localized)
	{
		wcsncpy(text, localized, TUTOR_TEXT_CHARS - 1);
		text[TUTOR_TEXT_CHARS - 1] = 0;
	}
	else
		sboui::TextToWide(raw, text, sizeof(text));

	ConvertNewLines(text);
	ReplacePlaceholders(text);

	TutorWindow_Show(text, params);

	if (sound[0])
		sbo::PlaySoundByName(sound, 1.0f);
	return 1;
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
static void LoadTutorLayout()
{
	KeyValues *pKV = new KeyValues("TutorTextWindow");
	if (pKV->LoadFromFile(g_pFullFileSystem, TUTOR_LAYOUT_FILE))
	{
		for (int i = 0; i < TUTOR_CTRL_COUNT; i++)
		{
			KeyValues *pControl = pKV->FindKey(s_Layout[i].name);
			if (!pControl)
				continue;

			s_Layout[i].x = pControl->GetInt("xpos", s_Layout[i].x);
			s_Layout[i].y = pControl->GetInt("ypos", s_Layout[i].y);
			s_Layout[i].wide = pControl->GetInt("wide", s_Layout[i].wide);
			s_Layout[i].tall = pControl->GetInt("tall", s_Layout[i].tall);
		}
	}
	pKV->deleteThis();
}

bool VGui2_Tutor_Init()
{
	if (s_bInitialized)
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	s_bInitialized = true;

	s_hTutorScheme = vgui2::scheme()->LoadSchemeFromFile(TUTOR_SCHEME_FILE, "TutorScheme");
	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/tutor_%language%.txt");
	LoadTutorLayout();

	sbo::HookUserMsg("TutorText", &TutorWindow_MsgFunc_TutorText);
	sbo::HookUserMsg("TutorPart", &TutorWindow_MsgFunc_TutorPart);
	sbo::HookUserMsg("TutorClose", &MsgFunc_TutorClose);
	return true;
}

void VGui2_Tutor_Reset()
{
	s_PendingText[0] = '\0';
	TutorWindow_Close();
}
