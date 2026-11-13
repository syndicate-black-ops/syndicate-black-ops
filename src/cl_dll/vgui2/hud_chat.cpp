//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VGUI2 chat for SBO.
//
// Ported from BugfixedHL-Rebased src/game/client/hud/chat.cpp (Source SDK 2013
// hud_basechat.cpp lineage). Behaviour kept from BHL: chat history/scrollback
// (CHudChatHistory RichText with fades), input line editing (TextEntry), long
// input split into several say commands (UTF-8 safe), flash/fade/expire,
// cl_mute_all_comms, hud_saytext/hud_saytext_time/hud_saytext_sound, team-colored
// player names, ^0..^9 color codes, ESC closes the input without opening GameUI,
// messagemode/messagemode2 hooks.
//
// SBO changes (marked "SBO:"):
//   - SBO data (players, team colors, voice mutes, engine) via sbo_bridge.h;
//   - everyone sees all chat (no dead/alive filter, no AllowedToPrintText
//     suppression); sender names are colored for public chat too (the old
//     "\x01<name>: " server form is upgraded to "\x02<name>: ");
//   - input line history (Up/Down), PgUp/PgDn + mouse wheel scrollback;
//   - chat also works with maxclients == 1;
//   - full \x01..\x08 markup parsing in InsertAndColorizeText (BHL declared the
//     TextColor codes but only parsed \x02 at the start of the message);
//   - no AG location substitution, real-name replacement or results log.
//   - "^xRGB" (three hex digits, 4096 colors) and "^#RRGGBB" (six, any color) color codes next to BHL's ^0..^9,
//     drawn no darker than COLOR_CODE_MIN_LUMA (hud_chat.h).
//   - clickable http(s)/www. links in the history, only while the input line is
//     open (validated, Steam overlay or system browser).
//
// SDK world: compiled with hl1_source_sdk include paths.
//
// $NoKeywords: $
//=============================================================================//

#include <cstdarg>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cwchar>

#include <tier2/tier2.h>
#include <FileSystem.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_options.h"
#include "sbo_server_browser.h"
#include "sbo_ui_style.h"
#include "sbo_options_window.h"
#include "sbo_create_server.h"
#include "client_steam_context.h"
#include "hud_chat.h"
#include "hud_chat_color_picker.h"
#include "sbo_widgets.h"
#include "score_panel.h"
#include "sbo_bridge.h"
#include "sbo_cvar.h"
#include "vgui2_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
//-----------------------------------------------------------------------------
// Cvars (BHL: ConVar with FCVAR_BHL_ARCHIVE; SBO: archived engine cvars)
//-----------------------------------------------------------------------------
// shared with the scoreboard (sbo_cvar.h)
using CChatCvar = CSboCvar;

CChatCvar hud_saytext("hud_saytext", "1");                     // Enable/disable display of new chat messages
CChatCvar hud_saytext_time("hud_saytext_time", "12");          // How long for new messages should stay on the screen
CChatCvar hud_saytext_sound("hud_saytext_sound", "1");         // Play sound on new chat message (volume)
CChatCvar cl_mute_all_comms("cl_mute_all_comms", "1");         // If 1, a voice-muted player's chat is blocked too
CChatCvar hud_colortext("hud_colortext", "1");                 // ^0..^9 / ^xRGB / ^#RRGGBB: 0 ignore, 1 handle, 2 strip
// SBO: the player's chat color (color picker selection): 0 none, 1..8 ^1..^8, xRRGGBB / xRGB. FCVAR_USERINFO like
// namecolor: the engine sends it to every client, which draw the player's messages in it (GetPlayerChatColor)
CChatCvar hud_chat_color("hud_chat_color", "0", true);
// SBO: chat text size in pixels, 14..34, 17 = the scheme's ChatFont (CHudChat::GetChatFontTall), set in Advanced > Chat
CChatCvar hud_chat_fontsize("hud_chat_fontsize", "17");

constexpr int CHAT_FONT_SCHEME_TALL = CHudChat::CHAT_FONT_DEFAULT; //!< chatschemebase.res ChatFont / ChatFontUnderline
#ifdef _WIN32
constexpr const char CHAT_FONT_FACE[] = "Tahoma"; // chatschemebase.res ChatFont [$WINDOWS]
#else
constexpr const char CHAT_FONT_FACE[] = "Liberation Sans"; // [!$WINDOWS]
#endif

// SBO: misc/talk.wav (SBO's legacy chat sound; valve/sound/misc/talk.wav).
// BHL falls back to its own misc/talk_bhl_fallback.wav when the file is missing.
constexpr const char CHAT_SOUND_FILE[] = "misc/talk.wav";

// BHL hud.cpp s_DefaultColorCodeColors
const Color s_DefaultColorCodeColors[10] = {
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^0 orange/reset
	Color(0xFF, 0x00, 0x00, 0xFF), // ^1 red
	Color(0x00, 0xFF, 0x00, 0xFF), // ^2 green
	Color(0xFF, 0xFF, 0x00, 0xFF), // ^3 yellow
	Color(0x00, 0x00, 0xFF, 0xFF), // ^4 blue
	Color(0x00, 0xFF, 0xFF, 0xFF), // ^5 cyan
	Color(0xFF, 0x00, 0xFF, 0xFF), // ^6 magenta
	Color(0x88, 0x88, 0x88, 0xFF), // ^7 grey
	Color(0xFF, 0xFF, 0xFF, 0xFF), // ^8 white
	Color(0xFF, 0xAA, 0x00, 0xFF), // ^9 orange/reset
};

// BHL hud.cpp NoTeamColor::Orange
const Color s_NoTeamColorOrange = Color(255, 178, 0, 255);

inline float GetCurTime()
{
	return (float)vgui2::system()->GetCurrentTime();
}

template <typename T>
inline T ClampValue(T v, T lo, T hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

//-----------------------------------------------------------------------------
// \x07RRGGBB / \x08RRGGBBAA helpers
//-----------------------------------------------------------------------------
template <typename CharT>
inline int HexDigitValue(CharT c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

//! Parses numDigits (6 or 8) hex digits. Returns false if any is missing/invalid.
template <typename CharT>
bool ParseHexColor(const CharT *s, int numDigits, Color &out)
{
	int v[8];
	for (int i = 0; i < numDigits; i++)
	{
		if (!s[i])
			return false;
		v[i] = HexDigitValue(s[i]);
		if (v[i] < 0)
			return false;
	}

	int r = v[0] * 16 + v[1];
	int g = v[2] * 16 + v[3];
	int b = v[4] * 16 + v[5];
	int a = numDigits == 8 ? v[6] * 16 + v[7] : 255;
	out = Color(r, g, b, a);
	return true;
}

//! Length of the markup at s (\x01..\x08 incl. hex payload), 0 if s isn't markup.
template <typename CharT>
int GetColorMarkupLength(const CharT *s)
{
	Color dummy;
	switch (s[0])
	{
	case COLOR_NORMAL:
	case COLOR_PLAYERNAME:
	case COLOR_PLAYERNAME_SOURCE:
	case COLOR_LOCATION:
	case COLOR_ACHIEVEMENT:
	case COLOR_CUSTOM:
		return 1;
	case COLOR_HEXCODE:
		return ParseHexColor(s + 1, 6, dummy) ? 7 : 1;
	case COLOR_HEXCODE_ALPHA:
		return ParseHexColor(s + 1, 8, dummy) ? 9 : 1;
	default:
		return 0;
	}
}

//-----------------------------------------------------------------------------
// SBO: chat hyperlinks
//-----------------------------------------------------------------------------
//! Characters allowed in a chat link: printable ASCII except space, quotes, angle/curly
//! brackets, backslash, caret (^0..^9 / ^xRGB / ^#RRGGBB color codes), backtick and pipe.
template <typename CharT>
inline bool IsChatURLChar(CharT c)
{
	if (c < 0x21 || c > 0x7E)
		return false;
	switch (c)
	{
	case '"':
	case '<':
	case '>':
	case '\\':
	case '^':
	case '`':
	case '{':
	case '}':
	case '|':
		return false;
	default:
		return true;
	}
}

template <typename CharT>
inline bool IsAsciiAlnum(CharT c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

//! Case-insensitive "text[0..textLen) starts with the ASCII lower-case prefix"
inline bool StartsWithNoCase(const wchar_t *text, int textLen, const char *prefix)
{
	int i = 0;
	for (; prefix[i]; i++)
	{
		if (i >= textLen)
			return false;
		wchar_t c = text[i];
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
		if (c != (wchar_t)prefix[i])
			return false;
	}
	return true;
}

//! Looks for a link starting at text[pos]. Returns 0 if there is no link prefix there,
//! otherwise the number of characters to skip (the whole URL-like run); link.end > link.start
//! only if the run is a valid link.
int ParseChatLink(const wchar_t *text, int textLen, int pos, ChatLink &link)
{
	link.start = link.end = pos;
	link.url[0] = '\0';

	// must start a word
	if (pos > 0)
	{
		wchar_t p = text[pos - 1];
		if (IsAsciiAlnum(p) || p == '.' || p == '/' || p == '@' || p == '-' || p == '_' || p == ':' || p == '%' || p == '=' || p == '&')
			return 0;
	}

	const wchar_t *s = text + pos;
	int avail = textLen - pos;
	bool bWWW = false;
	int prefixLen;
	if (StartsWithNoCase(s, avail, "https://"))
		prefixLen = 8;
	else if (StartsWithNoCase(s, avail, "http://"))
		prefixLen = 7;
	else if (StartsWithNoCase(s, avail, "www."))
	{
		prefixLen = 4;
		bWWW = true;
	}
	else
		return 0;

	int end = pos;
	while (end < textLen && IsChatURLChar(text[end]))
		end++;
	int consumed = end - pos;

	// trailing punctuation belongs to the sentence, unbalanced ) or ] to the surrounding text
	while (end > pos + prefixLen)
	{
		wchar_t c = text[end - 1];
		if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '\'' || c == '*')
		{
			end--;
			continue;
		}
		if (c == ')' || c == ']')
		{
			wchar_t open = c == ')' ? '(' : '[';
			int depth = 0;
			for (int i = pos; i < end; i++)
			{
				if (text[i] == open)
					depth++;
				else if (text[i] == c)
					depth--;
			}
			if (depth < 0)
			{
				end--;
				continue;
			}
		}
		break;
	}

	int len = end - pos;
	if (len <= prefixLen)
		return consumed;

	// build the target (ASCII only, guaranteed by IsChatURLChar)
	char url[MAX_CHAT_URL_LEN + 1];
	int n = 0;
	if (bWWW)
	{
		const char *https = "https://";
		for (; https[n]; n++)
			url[n] = https[n];
	}
	if (n + len > MAX_CHAT_URL_LEN)
		return consumed; // too long: plain text
	for (int i = 0; i < len; i++)
		url[n++] = (char)text[pos + i];
	url[n] = '\0';

	if (bWWW)
	{
		// "www.<label>.<label>": something after www. that has another dot inside the host
		const char *host = url + 8 + 4;
		size_t hostLen = strcspn(host, "/?#:");
		const char *dot = (const char *)memchr(host, '.', hostLen);
		if (!dot || dot == host || dot + 1 >= host + hostLen)
			return consumed;
	}

	if (!IsSafeChatURL(url))
		return consumed;

	link.end = end;
	memcpy(link.url, url, n + 1);
	return consumed;
}

//! Opens a validated URL: Steam overlay browser if available, else the system browser.
void OpenChatURL(const char *url)
{
	if (!IsSafeChatURL(url))
		return;

	char msg[MAX_CHAT_URL_LEN + 32];
	snprintf(msg, sizeof(msg), "Opening %s\n", url);
	sbo::DevPrint(msg);

	// BHL: HUD_Init activates it; the scoreboard does the same (idempotent)
	ClientSteamContext().Activate();
	if (ClientSteamContext().SteamFriends() && ClientSteamContext().SteamUtils() && ClientSteamContext().SteamUtils()->IsOverlayEnabled())
	{
		ClientSteamContext().SteamFriends()->ActivateGameOverlayToWebPage(url);
		return;
	}

	vgui2::system()->ShellExecute("open", url);
}
} // namespace

void HudChat_OpenURL(const char *url)
{
	OpenChatURL(url);
}

bool IsSafeChatURL(const char *url)
{
	if (!url)
		return false;

	size_t len = strlen(url);
	if (len == 0 || len > (size_t)MAX_CHAT_URL_LEN)
		return false;

	size_t schemeLen;
	if (!V_strnicmp(url, "https://", 8))
		schemeLen = 8;
	else if (!V_strnicmp(url, "http://", 7))
		schemeLen = 7;
	else
		return false;

	for (size_t i = 0; i < len; i++)
	{
		if (!IsChatURLChar((unsigned char)url[i]))
			return false;
	}

	// authority = host[:port], no userinfo ("http://trusted.com@evil.com")
	const char *host = url + schemeLen;
	size_t hostLen = strcspn(host, "/?#");
	if (hostLen == 0)
		return false;

	bool bHasAlnum = false;
	for (size_t i = 0; i < hostLen; i++)
	{
		char c = host[i];
		if (IsAsciiAlnum(c))
			bHasAlnum = true;
		else if (c != '.' && c != '-' && c != '_' && c != ':' && c != '[' && c != ']')
			return false;
	}

	return bHasAlnum;
}

//-------------------------------------------------------------------
// Color code utilities (BHL cl_util.cpp)
//-------------------------------------------------------------------
static void RemoveColorCodes(const char *string, char *buf, size_t bufSize)
{
	char *c1 = buf;
	const char *c2 = string;
	char *end = buf + bufSize - 1;
	while (*c2 && c1 < end)
	{
		// SBO: ^0..^9, ^xRGB and ^#RRGGBB
		const int codeLen = GetColorCodeLength(c2);
		if (codeLen > 0)
		{
			c2 += codeLen;
			continue;
		}
		*c1 = *c2;
		c1++;
		c2++;
	}
	*c1 = 0;
}

// SBO: ^xRGB / ^#RRGGBB readability (hud_chat.h COLOR_CODE_MIN_LUMA)
Color ClampColorCodeBrightness(Color color)
{
	const float luma = 0.299f * color.r() + 0.587f * color.g() + 0.114f * color.b();
	if (luma >= (float)COLOR_CODE_MIN_LUMA)
		return color;

	// towards white: c + (255 - c) * t has the luma luma + (255 - luma) * t
	const float t = ((float)COLOR_CODE_MIN_LUMA - luma) / (255.0f - luma);
	int rgb[3] = { color.r(), color.g(), color.b() };
	for (int i = 0; i < 3; i++)
	{
		rgb[i] = (int)(rgb[i] + (255 - rgb[i]) * t + 0.999f); // rounded up: never below the floor
		if (rgb[i] > 255)
			rgb[i] = 255;
	}
	return Color(rgb[0], rgb[1], rgb[2], color.a());
}

const char *RemoveColorCodes(const char *string)
{
	static char buffer[1024];
	RemoveColorCodes(string, buffer, sizeof(buffer));
	return buffer;
}

// vgui2_api.h (BHL cl_util.cpp ConsolePrint)
const char *VGui2_Chat_RemoveColorCodes(const char *text)
{
	CHudChat *pChat = CHudChat::Get();
	if (pChat && pChat->GetColorCodeAction() == ColorCodeAction::Ignore)
		return text;
	return RemoveColorCodes(text);
}

// vgui2_api.h: hud_colortext for the status bar (statusbar.cpp)
int VGui2_Chat_GetColorCodeAction()
{
	CHudChat *pChat = CHudChat::Get();
	return (int)(pChat ? pChat->GetColorCodeAction() : ColorCodeAction::Strip);
}

// vgui2_api.h: the status bar's player name color, from the player's userinfo, resolved like the code the chat writes
// for it (ApplyNameColorCodes: WriteColorCode, then GetColorCodeColor when it is drawn)
bool VGui2_Chat_GetPlayerNameColor(int client, int *rgb)
{
	CHudChat *pChat = CHudChat::Get();
	if (!pChat)
		return false;

	const ChatColor nameColor = CHudChat::GetPlayerNameColor(client);
	char code[COLOR_CODE_MAX_LEN + 1];
	if (nameColor.IsNone() || WriteColorCode(nameColor, code) <= 0)
		return false;

	Color color;
	if (!pChat->GetColorCodeColor(code, color))
		return false;

	rgb[0] = color.r();
	rgb[1] = color.g();
	rgb[2] = color.b();
	return true;
}

// vgui2_api.h: the colour of a code in the status bar's player names (CHudDeathNoticePanel::DrawKfText)
int VGui2_Chat_GetColorCode(const char *s, bool *pbReset, int *rgb)
{
	const int codeLen = GetColorCodeLength(s);
	if (codeLen <= 0)
		return 0;

	Color color;
	CHudChat *pChat = CHudChat::Get();
	const bool bColor = pChat && pChat->GetColorCodeColor(s, color);
	if (pbReset)
		*pbReset = !bColor;
	if (bColor && rgb)
	{
		rgb[0] = color.r();
		rgb[1] = color.g();
		rgb[2] = color.b();
	}
	return codeLen;
}

char *RemoveColorMarkup(char *str)
{
	char *out = str;
	const char *in = str;

	while (*in)
	{
		int markupLen = GetColorMarkupLength(in);
		if (markupLen > 0)
		{
			in += markupLen;
			continue;
		}

		*out++ = *in++;
	}

	*out = '\0';
	return str;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *parent -
//			*panelName -
//-----------------------------------------------------------------------------
CHudChatLine::CHudChatLine(vgui2::Panel *parent, const char *panelName)
    : vgui2::RichText(parent, panelName)
{
	m_hFont = m_hFontMarlett = 0;
	m_flExpireTime = 0.0f;
	m_flStartTime = 0.0f;
	m_iNameLength = 0;
	m_iNameStart = 0;
	m_nCount = 0;
	m_text = NULL;

	SetPaintBackgroundEnabled(true);

	SetVerticalScrollbar(false);
}

CHudChatLine::~CHudChatLine()
{
	if (m_text)
	{
		delete[] m_text;
		m_text = NULL;
	}
}

void CHudChatLine::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont("Default");

	SetBgColor(Color(0, 0, 0, 100));

	m_hFontMarlett = pScheme->GetFont("Marlett");

	m_clrText = pScheme->GetColor("FgColor", GetFgColor());
	SetFont(m_hFont);
}

void CHudChatLine::PerformFadeout(void)
{
	// Flash + Extra bright when new
	float curtime = GetCurTime();

	int lr = m_clrText[0];
	int lg = m_clrText[1];
	int lb = m_clrText[2];

	if (curtime >= m_flStartTime && curtime < m_flStartTime + CHATLINE_FLASH_TIME)
	{
		float frac1 = (curtime - m_flStartTime) / CHATLINE_FLASH_TIME;
		float frac = frac1;

		frac *= CHATLINE_NUM_FLASHES;
		frac *= 2 * M_PI;

		frac = cos(frac);

		frac = ClampValue(frac, 0.0f, 1.0f);

		frac *= (1.0f - frac1);

		int r = lr, g = lg, b = lb;

		r = r + (255 - r) * frac;
		g = g + (255 - g) * frac;
		b = b + (255 - b) * frac;

		// Draw a right facing triangle in red, faded out over time
		int alpha = 63 + 192 * (1.0f - frac1);
		alpha = ClampValue(alpha, 0, 255);

		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(r, g, b, 255));
		InsertString(wbuf);
	}
	else if (curtime <= m_flExpireTime && curtime > m_flExpireTime - CHATLINE_FADE_TIME)
	{
		float frac = (m_flExpireTime - curtime) / CHATLINE_FADE_TIME;

		int alpha = frac * 255;
		alpha = ClampValue(alpha, 0, 255);

		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(lr * frac, lg * frac, lb * frac, alpha));
		InsertString(wbuf);
	}
	else
	{
		wchar_t wbuf[4096];
		GetText(0, wbuf, sizeof(wbuf));

		SetText("");

		InsertColorChange(Color(lr, lg, lb, 255));
		InsertString(wbuf);
	}

	OnThink();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : time -
//-----------------------------------------------------------------------------
void CHudChatLine::SetExpireTime(void)
{
	m_flStartTime = GetCurTime();

	if (hud_saytext.GetBool())
	{
		m_flExpireTime = m_flStartTime + hud_saytext_time.GetFloat();
	}
	else
	{
		// Expire immediately
		m_flExpireTime = m_flStartTime;
	}
	m_nCount = CHudChat::m_nLineCounter++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CHudChatLine::GetCount(void)
{
	return m_nCount;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudChatLine::IsReadyToExpire(void)
{
	if (GetCurTime() >= m_flExpireTime)
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : float
//-----------------------------------------------------------------------------
float CHudChatLine::GetStartTime(void)
{
	return m_flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChatLine::Expire(void)
{
	SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
CHudChatInputLine::CHudChatInputLine(vgui2::Panel *parent, char const *panelName)
    : vgui2::Panel(parent, panelName)
{
	SetMouseInputEnabled(false);

	m_pPrompt = new vgui2::Label(this, "ChatInputPrompt", L"Enter text:");

	m_pInput = new CHudChatEntry(this, "ChatInput", parent);
	m_pInput->SetMaximumCharCount(MAX_CHAT_INPUT_STRING_LEN);

	// SBO: color picker (BugfixedHL-Rebased issue #98), collapsed to a circle in the current color. Takes the mouse
	// only while the chat input is open (StartMessageMode / StopMessageMode), like the input line itself.
	m_pColorButton = new CHudChatColorButton(this, "ChatColorButton");
}

void CHudChatInputLine::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// FIXME:  Outline
	SetChatFont(CHudChat::GetChatFont(pScheme)); // SBO: hud_chat_fontsize

	m_pInput->SetNormalTextColor(pScheme->GetColor("Chat.TypingText", pScheme->GetColor("Panel.FgColor", Color(255, 255, 255, 255))));

	SetPaintBackgroundEnabled(true);
	m_pPrompt->SetPaintBackgroundEnabled(true);
	m_pPrompt->SetContentAlignment(vgui2::Label::a_west);
	m_pPrompt->SetTextInset(2, 0);

	m_pInput->SetMouseInputEnabled(true);

	SetBgColor(Color(0, 0, 0, 0));
}

void CHudChatInputLine::SetChatFont(vgui2::HFont hFont)
{
	m_pPrompt->SetFont(hFont);
	m_pInput->SetFont(hFont);
	InvalidateLayout();
}

void CHudChatInputLine::SetPrompt(const wchar_t *prompt)
{
	Assert(m_pPrompt);
	m_pPrompt->SetText(prompt);
	InvalidateLayout();
}

void CHudChatInputLine::ClearEntry(void)
{
	Assert(m_pInput);
	SetEntry(L"");
}

void CHudChatInputLine::SetEntry(const wchar_t *entry)
{
	Assert(m_pInput);
	Assert(entry);

	m_pInput->SetText(entry);
}

void CHudChatInputLine::GetMessageText(wchar_t *buffer, int buffersizebytes)
{
	m_pInput->GetText(buffer, buffersizebytes);
}

void CHudChatInputLine::GotoEntryEnd()
{
	m_pInput->GotoTextEnd();
}

void CHudChatInputLine::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize(wide, tall);

	// SBO: the collapsed color picker first (left of the prompt): a circle one line tall plus its arrow
	const int swatch = tall > 4 ? tall - 2 : tall;
	const int buttonWide = CHudChatColorButton::GetWideForSwatch(swatch);
	m_pColorButton->SetBounds(0, (tall - swatch) / 2, buttonWide, swatch);
	int x = buttonWide + 3;

	int w, h;
	m_pPrompt->GetContentSize(w, h);
	m_pPrompt->SetBounds(x, 0, w, tall);

	m_pInput->SetBounds(x + w + 2, 0, wide - x - w - 2, tall);
}

vgui2::Panel *CHudChatInputLine::GetColorButton(void)
{
	return m_pColorButton;
}

vgui2::Panel *CHudChatInputLine::GetInputPanel(void)
{
	return m_pInput;
}

CHudChatHistory::CHudChatHistory(vgui2::Panel *pParent, const char *panelName)
    : BaseClass(pParent, "HudChatHistory")
{
	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/chatscheme.res", "ChatScheme");
	SetScheme(scheme);

	InsertFade(-1, -1);
	sboui::ApplySymbolSubstitutes(this); // Linux: the scroll bar's arrow buttons
}

void CHudChatHistory::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetChatFonts(pScheme);
	SetAlpha(255);
}

void CHudChatHistory::SetChatFonts(vgui2::IScheme *pScheme)
{
	// SBO: hud_chat_fontsize (CHudChat::GetChatFont)
	vgui2::HFont hFont = CHudChat::GetChatFont(pScheme);
	SetFont(hFont);

	// SBO: links. RichText takes "DefaultUnderline", which ChatScheme doesn't have (font 0
	// would draw nothing): use the underlined chat font, or the chat font (links are still colored).
	vgui2::HFont hUnderline = CHudChat::GetChatFont(pScheme, true);
	SetUnderlineFont(hUnderline != vgui2::INVALID_FONT ? hUnderline : hFont);
}

void CHudChatHistory::OnThink()
{
	BaseClass::OnThink();
	SyncLinkMouseInput();
}

void CHudChatHistory::SyncLinkMouseInput()
{
	// RichText creates a ClickPanel child per visible link (piece). A child copies the parent's
	// mouse state only when it is created, so links added while the chat was closed would stay
	// unclickable after opening it -- and the other way round. Keep them in sync with
	// "input line open" (the history itself only takes the mouse while it is open, too).
	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());
	bool bEnable = pChat && pChat->GetMessageMode() != MM_NONE && IsMouseInputEnabled();

	for (int i = 0; i < GetChildCount(); i++)
	{
		vgui2::Panel *pChild = GetChild(i);
		if (pChild && !V_strcmp(pChild->GetClassName(), "ClickPanel") && pChild->IsMouseInputEnabled() != bEnable)
			pChild->SetMouseInputEnabled(bEnable);
	}
}

void CHudChatHistory::OnTextClicked(const wchar_t *text)
{
	// Not BaseClass: RichText::OnTextClicked ShellExecutes whatever it gets.
	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());
	if (!pChat || pChat->GetMessageMode() == MM_NONE || !IsMouseInputEnabled())
		return; // links only work while the input line is open

	if (text && wcslen(text) <= (size_t)MAX_CHAT_URL_LEN)
	{
		// the click action is the URL set by CHudChatLine::Colorize (ASCII); validate again
		char url[MAX_CHAT_URL_LEN * 4 + 1];
		V_UnicodeToUTF8(text, url, sizeof(url));
		OpenChatURL(url);
	}

	// keep typing in the input line
	if (pChat->GetInputPanel())
		pChat->GetInputPanel()->RequestFocus();
}

void CHudChatHistory::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
	{
		// Hide the chatbox
		static_cast<CHudChat *>(GetParent())->StopMessageMode();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

int CHudChat::m_nLineCounter = 1;
CHudChat *CHudChat::m_sInstance = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Text chat input/output hud element
//-----------------------------------------------------------------------------
CHudChat::CHudChat()
    : BaseClass(NULL, "HudChat")
{
	Assert(!m_sInstance);
	m_sInstance = this;

	SetParent(g_pViewport);
	SetProportional(true);

	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/chatscheme.res", "ChatScheme");
	SetScheme(scheme);
	SetPaintBackgroundEnabled(true);

	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/chat_%language%.txt");

	m_nMessageMode = 0;

	// SBO: BHL's ConVar defaults; custom color starts white (BHL SetCustomColor fallback)
	m_ColorCustom = Color(255, 255, 255, 255);
	m_LinkColor = Color(80, 170, 255, 255);
	for (int i = 0; i < 10; i++)
		m_ColorCodeColors[i] = s_DefaultColorCodeColors[i];

	vgui2::ivgui()->AddTickSignal(GetVPanel());

	// (We don't actually want input until they bring up the chat line).
	MakePopup();
	SetZPos(-30);

	m_pChatHistory = new CHudChatHistory(this, "HudChatHistory");

	CreateChatLines();
	CreateChatInputLine();

	// SBO: color picker popup and its tooltip: above the history and the input line
	m_pColorPicker = new CHudChatColorPicker(this, "ChatColorPicker");
	m_pColorPicker->SetZPos(10);
	m_pColorTooltip = new CHudChatColorTooltip(this, "ChatColorTooltip");
	m_pColorTooltip->SetZPos(11);
}

void CHudChat::CreateChatInputLine(void)
{
	m_pChatInput = new CHudChatInputLine(this, "ChatInputLine");
	m_pChatInput->SetVisible(false);

	if (GetChatHistory())
	{
		GetChatHistory()->SetMaximumCharCount(CHAT_HISTORY_MAX_CHARS);
		GetChatHistory()->SetVisible(true);
	}
}

void CHudChat::CreateChatLines(void)
{
	m_ChatLine = new CHudChatLine(this, "ChatLine1");
	m_ChatLine->SetVisible(false);
}

void CHudChat::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	LoadControlSettings(VGUI2_ROOT_DIR "resource/chat.res");

	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(2);
	SetPaintBorderEnabled(true);
	SetPaintBackgroundEnabled(true);

	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	m_nVisibleHeight = 0;

	Color cColor = pScheme->GetColor("ChatBgColor", GetBgColor());
	SetBgColor(Color(cColor.r(), cColor.g(), cColor.b(), CHAT_HISTORY_ALPHA));

	GetChatHistory()->SetVerticalScrollbar(false);

	m_pChatInput->SetVisible(false);
	CloseColorPicker();
	m_DefTextColor = pScheme->GetColor("ChatTextColor", Color(17, 244, 136, 255)); // SBO: original con_color
	m_ConsoleMsgColor = pScheme->GetColor("ChatConsoleMsg", Color(30, 230, 50, 255));
	m_LinkColor = pScheme->GetColor("ChatLinkColor", Color(80, 170, 255, 255));

	// BHL reads these from ClientScheme.res (CHud::ApplyViewportSchemeSettings).
	// SBO has no VGUI2 ClientScheme, so they come from ChatScheme.res.
	char buf[64];
	for (int i = 0; i <= 9; i++)
	{
		snprintf(buf, sizeof(buf), "ColorCode%d", i);
		m_ColorCodeColors[i] = pScheme->GetColor(buf, s_DefaultColorCodeColors[i]);
	}

	// BHL CHud::ApplyViewportSchemeSettings: the colours of Label/SectionedListPanel colour-coded text (scoreboard names)
	vgui2::Label::SetDefaultColorCodeArray(m_ColorCodeColors);
}

void CHudChat::Reset(void)
{
}

void CHudChat::Paint(void)
{
	if (m_nVisibleHeight == 0)
		return;
}

CHudChatHistory *CHudChat::GetChatHistory(void)
{
	return m_pChatHistory;
}

static sbo::CmdFn s_pfnEngineMsgMode = nullptr;
static sbo::CmdFn s_pfnEngineMsgMode2 = nullptr;

static void MessageModeVGUI2()
{
	// SBO: BHL returns here when maxclients == 1; SBO keeps chat in single player too.
	if (sbo::Cmd_Argc() != 1)
	{
		s_pfnEngineMsgMode();
		return;
	}

	CHudChat::Get()->StartMessageMode(MM_SAY);
}

static void MessageMode2VGUI2()
{
	if (sbo::Cmd_Argc() != 1)
	{
		s_pfnEngineMsgMode2();
		return;
	}

	// SBO: say_team is deprecated (the server treats it like say): messagemode2 opens
	// the same public chat input as messagemode.
	CHudChat::Get()->StartMessageMode(MM_SAY);
}

static int MsgFunc_SayTextThunk(const char *pszName, int iSize, void *pbuf)
{
	return CHudChat::Get()->MsgFunc_SayText(pszName, iSize, pbuf);
}

void CHudChat::Init(void)
{
	hud_saytext.Register();
	hud_saytext_time.Register();
	hud_saytext_sound.Register();
	cl_mute_all_comms.Register();
	hud_colortext.Register();
	hud_chat_color.Register();
	hud_chat_fontsize.Register();

	sbo::HookUserMsg("SayText", &MsgFunc_SayTextThunk);

	// Hook messagemode and messagemode2 (once: a second hook would save our own
	// function as the engine's)
	static bool s_bCommandsHooked = false;
	if (!s_bCommandsHooked)
	{
		s_bCommandsHooked = true;

		if (!sbo::ReplaceEngineCommand("messagemode", &MessageModeVGUI2, &s_pfnEngineMsgMode)
		    || !sbo::ReplaceEngineCommand("messagemode2", &MessageMode2VGUI2, &s_pfnEngineMsgMode2))
		{
			sbo::DevPrint("Failed to hook messagemode and messagemode2\n");
		}
	}

	m_pszChatSoundPath = CHAT_SOUND_FILE;
}

//-----------------------------------------------------------------------------
// SBO: chat text size (hud_chat_fontsize): a pixel height, the same at every resolution. 17 is the chat's size
// since the VGUI2 chat (the default); values below 14 (0..3 from an earlier test build) are 17, above 34 are 34.
//-----------------------------------------------------------------------------
int CHudChat::GetChatFontTall()
{
	int tall = CHAT_FONT_DEFAULT;
	const char *pszSize = hud_chat_fontsize.GetString(); // "" before Init registers it
	if (pszSize && *pszSize)
		tall = atoi(pszSize);
	if (tall < CHAT_FONT_MIN)
		tall = CHAT_FONT_DEFAULT;
	else if (tall > CHAT_FONT_MAX)
		tall = CHAT_FONT_MAX;
	return tall;
}

vgui2::HFont CHudChat::GetChatFont(vgui2::IScheme *pScheme, bool bUnderline)
{
	const int tall = GetChatFontTall();
	const char *pszScheme = bUnderline ? "ChatFontUnderline" : "ChatFont";
	if (tall == CHAT_FONT_SCHEME_TALL && pScheme)
		return pScheme->GetFont(pszScheme);

	// other heights: created once per height (the engine has no call to free a font, so they are kept)
	struct CachedFont
	{
		int tall;
		bool underline;
		vgui2::HFont font;
	};
	static CachedFont s_Fonts[2 * (CHAT_FONT_MAX - CHAT_FONT_MIN + 1)];
	static int s_nFonts = 0;
	for (int i = 0; i < s_nFonts; i++)
	{
		if (s_Fonts[i].tall == tall && s_Fonts[i].underline == bUnderline)
			return s_Fonts[i].font;
	}

	vgui2::HFont hFont = vgui2::surface()->CreateFont();
	int flags = vgui2::ISurface::FONTFLAG_DROPSHADOW;
	if (bUnderline)
		flags |= vgui2::ISurface::FONTFLAG_UNDERLINE;
	if (hFont == vgui2::INVALID_FONT || !vgui2::surface()->AddGlyphSetToFont(hFont, CHAT_FONT_FACE, tall, 800, 0, 0, flags, 0, 0))
		return pScheme ? pScheme->GetFont(pszScheme) : vgui2::INVALID_FONT;

	if (s_nFonts < (int)(sizeof(s_Fonts) / sizeof(s_Fonts[0])))
		s_Fonts[s_nFonts++] = { tall, bUnderline, hFont };
	return hFont;
}

//-----------------------------------------------------------------------------
// Purpose: Allow inheriting classes to change this spacing behavior
//-----------------------------------------------------------------------------
int CHudChat::GetChatInputOffset(void)
{
	if (m_pChatInput->IsVisible())
	{
		return m_iFontHeight;
	}
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Do respositioning here to avoid latency due to repositioning of vgui
//  voice manager icon panel
//-----------------------------------------------------------------------------
void CHudChat::OnTick(void)
{
	m_nVisibleHeight = 0;

	// SBO: hud_chat_fontsize changed: the history and the input line take the new chat font
	const int chatFontTall = GetChatFontTall();
	if (chatFontTall != m_iAppliedChatFontTall)
	{
		m_iAppliedChatFontTall = chatFontTall;
		vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(GetScheme());
		if (pScheme)
		{
			GetChatHistory()->SetChatFonts(pScheme);
			m_pChatInput->SetChatFont(GetChatFont(pScheme));
		}
	}

	CHudChatLine *line = m_ChatLine;

	if (line)
	{
		vgui2::HFont font = line->GetFont();
		m_iFontHeight = vgui2::surface()->GetFontTall(font) + 2;
		// SBO: a chat font taller than the scheme's ChatFont (Large / Larger) gets a taller input
		// line; up to that the line height is the one it always had
		if (chatFontTall > CHAT_FONT_SCHEME_TALL && chatFontTall + 1 > m_iFontHeight)
			m_iFontHeight = chatFontTall + 1;

		// Put input area at bottom

		int iChatX, iChatY, iChatW, iChatH;
		int iInputX, iInputY, iInputW, iInputH;

		m_pChatInput->GetBounds(iInputX, iInputY, iInputW, iInputH);
		GetBounds(iChatX, iChatY, iChatW, iChatH);

		m_pChatInput->SetBounds(iInputX, iChatH - (m_iFontHeight * 1.75), iInputW, m_iFontHeight);

		//Resize the History Panel so it fits more lines depending on the screen resolution.
		int iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH;

		GetChatHistory()->GetBounds(iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH);

		iChatHistoryH = (iChatH - (m_iFontHeight * 2.25)) - iChatHistoryY;

		GetChatHistory()->SetBounds(iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH);
	}

	FadeChatHistory();

	// SBO: color picker popup: follow the input line; a click anywhere outside it (and outside the collapsed
	// circle, which toggles it itself) closes it. The click still goes where it went. (A wheel drag that leaves the
	// popup is no new click.)
	if (IsColorPickerOpen())
	{
		LayoutColorPicker();

		const bool bDown = vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT) || vgui2::input()->IsMouseDown(vgui2::MOUSE_RIGHT)
		    || vgui2::input()->IsMouseDown(vgui2::MOUSE_MIDDLE);
		if (bDown && !m_bPickerMouseDown)
		{
			vgui2::Panel *pButton = m_pChatInput->GetColorButton();
			if (!m_pColorPicker->IsCursorOver() && !(pButton && pButton->IsCursorOver()))
				CloseColorPicker();
		}
		m_bPickerMouseDown = bDown;
	}

	// SBO: transient lines leave the history once faded out (only while closed: the open scrollback keeps its
	// position; StopMessageMode purges then)
	if (m_nTransientEntries > 0 && m_nMessageMode == MM_NONE)
		PurgeTransientHistory(false);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : width -
//			*text -
//			textlen -
// Output : int
//-----------------------------------------------------------------------------
int CHudChat::ComputeBreakChar(int width, const char *text, int textlen)
{
	CHudChatLine *line = m_ChatLine;
	vgui2::HFont font = line->GetFont();

	int currentlen = 0;
	int lastbreak = textlen;
	for (int i = 0; i < textlen; i++)
	{
		char ch = text[i];

		if (ch <= 32)
		{
			lastbreak = i;
		}

		wchar_t wch[2];

		g_pVGuiLocalize->ConvertANSIToUnicode(&ch, wch, sizeof(wch));

		int a, b, c;

		vgui2::surface()->GetCharABCwide(font, wch[0], a, b, c);
		currentlen += a + b + c;

		if (currentlen >= width)
		{
			// If we haven't found a whitespace char to break on before getting
			//  to the end, but it's still too long, break on the character just before
			//  this one
			if (lastbreak == textlen)
			{
				lastbreak = (i - 1 > 0) ? i - 1 : 0;
			}
			break;
		}
	}

	if (currentlen >= width)
	{
		return lastbreak;
	}
	return textlen;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CHudChat::Printf(const char *fmt, ...)
{
	va_list marker;
	char msg[4096];

	va_start(marker, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, marker);
	va_end(marker);

	// SBO: the server's hit messages (HUD_PRINTTALK) are transient: shown, not kept, not in the console
	PrintChatLine(0, msg, SboChat_IsTransientMessage(msg));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::StartMessageMode(int iMessageModeType)
{
	// SBO: no team-only chat mode any more (say_team is deprecated server-side)
	(void)iMessageModeType;
	m_nMessageMode = MM_SAY;

	m_pChatInput->ClearEntry();
	m_iInputHistoryPos = -1;

	const wchar_t *pszPrompt = (m_nMessageMode == MM_SAY) ? g_pVGuiLocalize->Find("#chat_say") : g_pVGuiLocalize->Find("#chat_say_team");
	if (pszPrompt)
	{
		m_pChatInput->SetPrompt(pszPrompt);
	}
	else
	{
		if (m_nMessageMode == MM_SAY)
		{
			m_pChatInput->SetPrompt(L"Say :");
		}
		else
		{
			m_pChatInput->SetPrompt(L"Say (TEAM) :");
		}
	}

	// SBO: the open history (scrollback) never shows the transient lines
	PurgeTransientHistory(true);

	if (GetChatHistory())
	{
		GetChatHistory()->SetMouseInputEnabled(true);
		GetChatHistory()->SetKeyBoardInputEnabled(false);
		GetChatHistory()->SetVerticalScrollbar(true);
		GetChatHistory()->ResetAllFades(true);
		GetChatHistory()->SetPaintBorderEnabled(true);
		GetChatHistory()->SetVisible(true);
		GetChatHistory()->SyncLinkMouseInput(); // SBO: links clickable now
	}

	vgui2::SETUP_PANEL(this);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	CloseColorPicker(); // SBO: always opens collapsed
	CHudChatColorPicker::PrecacheTextures();
	if (m_pChatInput->GetColorButton())
		m_pChatInput->GetColorButton()->SetMouseInputEnabled(true);
	m_pChatInput->SetVisible(true);
	vgui2::surface()->CalculateMouseVisible();
	m_pChatInput->RequestFocus();
	m_pChatInput->SetPaintBorderEnabled(true);
	m_pChatInput->SetMouseInputEnabled(true);

	//Place the mouse cursor near the text so people notice it.
	int x, y, w, h, chatx, chaty;
	GetPos(chatx, chaty);
	GetChatHistory()->GetBounds(x, y, w, h);
	sbo::ReleaseMouseForCursor(); // SBO: HL25 raw input, the position is for the real cursor
	vgui2::input()->SetCursorPos(chatx + x + (w / 2), chaty + y + (h / 2));

	// Move chat to front so it isn't obscured by the scoreboard in intermission
	MoveToFront();

	m_flHistoryFadeTime = GetCurTime() + CHAT_HISTORY_FADE_TIME;

	if (g_pGameUIViewport)
		g_pGameUIViewport->PreventEscapeToShow(true);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::StopMessageMode(void)
{
	if (g_pGameUIViewport)
		g_pGameUIViewport->PreventEscapeToShow(false);

	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);

	// SBO: closing (or sending) the chat closes the color picker; with the chat closed clicks go to the game
	CloseColorPicker();
	if (m_pChatInput->GetColorButton())
		m_pChatInput->GetColorButton()->SetMouseInputEnabled(false);

	// SBO: transient lines that came in while it was open and already timed out go now
	PurgeTransientHistory(false);

	if (GetChatHistory())
	{
		GetChatHistory()->SetPaintBorderEnabled(false);
		GetChatHistory()->GotoTextEnd();
		GetChatHistory()->SetMouseInputEnabled(false);
		GetChatHistory()->SetVerticalScrollbar(false);
		GetChatHistory()->ResetAllFades(false, true, CHAT_HISTORY_FADE_TIME);
		GetChatHistory()->SelectNoText();

		// SBO: same for the entries (RichText::ResetAllFades: faded-out ones restart a short fade)
		const float flNow = GetCurTime();
		for (int i = 0; i < m_HistoryEntries.Count(); i++)
		{
			if (m_HistoryEntries[i]->flFadeEnd < flNow)
				m_HistoryEntries[i]->flFadeEnd = flNow + CHAT_HISTORY_FADE_TIME;
		}
	}

	//Clear the entry since we wont need it anymore.
	m_pChatInput->ClearEntry();
	m_pChatInput->SetVisible(false);
	m_pChatInput->SetKeyBoardInputEnabled(false);
	m_pChatInput->SetMouseInputEnabled(false);

	// Move the scoreboard back above the chat (BHL)
	if (CScorePanel::Get() && CScorePanel::Get()->IsVisible())
		CScorePanel::Get()->MoveToFront();

	m_flHistoryFadeTime = GetCurTime() + CHAT_HISTORY_FADE_TIME;

	m_nMessageMode = MM_NONE;

	// SBO: links stop taking clicks (after MM_NONE)
	if (GetChatHistory())
		GetChatHistory()->SyncLinkMouseInput();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::OnChatEntrySend(void)
{
	Send();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::OnChatEntryStopMessageMode(void)
{
	StopMessageMode();
}

void CHudChat::FadeChatHistory(void)
{
	float frac = (m_flHistoryFadeTime - GetCurTime()) / CHAT_HISTORY_FADE_TIME;

	int alpha = frac * CHAT_HISTORY_ALPHA;
	alpha = ClampValue(alpha, 0, CHAT_HISTORY_ALPHA);

	if (alpha >= 0)
	{
		if (GetChatHistory())
		{
			if (IsMouseInputEnabled())
			{
				SetAlpha(255);
				GetChatHistory()->SetBgColor(Color(0, 0, 0, CHAT_HISTORY_ALPHA - alpha));
				m_pChatInput->GetPrompt()->SetAlpha((CHAT_HISTORY_ALPHA * 2) - alpha);
				m_pChatInput->GetInputPanel()->SetAlpha((CHAT_HISTORY_ALPHA * 2) - alpha);
				SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), CHAT_HISTORY_ALPHA - alpha));
				SetPaintBackgroundEnabled(true);
				SetPaintBackgroundType(2);
			}
			else
			{
				GetChatHistory()->SetBgColor(Color(0, 0, 0, alpha));
				SetBgColor(Color(GetBgColor().r(), GetBgColor().g(), GetBgColor().b(), alpha));

				m_pChatInput->GetPrompt()->SetAlpha(alpha);
				m_pChatInput->GetInputPanel()->SetAlpha(alpha);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: BHL's mapping of the TextColor codes to colors.
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient(TextColor colorNum, int clientIndex)
{
	Color c;
	switch (colorNum)
	{
	case COLOR_CUSTOM:
		c = m_ColorCustom;
		break;

	case COLOR_PLAYERNAME:
		c = GetClientColor(clientIndex);
		break;

	case COLOR_ACHIEVEMENT:
	{
		vgui2::IScheme *pSourceScheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetScheme("SourceScheme"));
		if (pSourceScheme)
		{
			c = pSourceScheme->GetColor("SteamLightGreen", GetBgColor());
		}
		else
		{
			c = GetDefaultTextColor();
		}
	}
	break;

	default:
		// COLOR_NORMAL, and (as in BHL) COLOR_PLAYERNAME_SOURCE and COLOR_LOCATION
		c = GetDefaultTextColor();
	}

	return Color(c[0], c[1], c[2], 255);
}

//-----------------------------------------------------------------------------
void CHudChat::SetCustomColor(const char *pszColorName)
{
	// BHL looks the name up in "ClientScheme"; SBO's VGUI2 layer only has ChatScheme.
	vgui2::IScheme *pScheme = vgui2::scheme()->GetIScheme(GetScheme());
	if (pScheme)
		SetCustomColor(pScheme->GetColor(pszColorName, Color(255, 255, 255, 255)));
}

//-----------------------------------------------------------------------------
Color CHudChat::GetDefaultTextColor(void)
{
	return m_DefTextColor;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor(int clientIndex)
{
	// SBO: the team color table SBO's legacy chat used (death.cpp GetClientColor).
	int r, g, b;
	sbo::GetClientColor(clientIndex, r, g, b);
	return Color(r, g, b, 255);
}

//-----------------------------------------------------------------------------
ColorCodeAction CHudChat::GetColorCodeAction()
{
	// BHL hud_redraw.cpp: clamp hud_colortext to 0..2
	int colorText = ClampValue(hud_colortext.GetInt(), 0, 2);
	if (hud_colortext.GetInt() != colorText)
		hud_colortext.SetValue(colorText);
	return (ColorCodeAction)colorText;
}

ChatColor ParseChatColor(const char *value)
{
	// ^0 and ^9 only reset to the default color: like anything else outside 1..8, they mean "none"
	if (!value)
		return ChatColor();
	while (*value == ' ')
		value++;

	// "xRRGGBB" or "xRGB" (also with '#' for 'x' and after a '^'): a wheel color
	const char *hex = value;
	if (hex[0] == '^')
		hex++;
	if (hex[0] == 'x' || hex[0] == 'X' || hex[0] == '#')
	{
		hex++;
		int digits = 0;
		int rgb = 0;
		for (; digits <= 6 && ColorCodeHexDigit(hex[digits]) >= 0; digits++)
			rgb = (rgb << 4) | ColorCodeHexDigit(hex[digits]);

		if ((digits != 3 && digits != 6) || (hex[digits] != '\0' && hex[digits] != ' '))
			return ChatColor();

		// three digits: each one a channel, expanded like CSS #RGB -> #RRGGBB
		if (digits == 3)
			rgb = (((rgb >> 8) & 15) * 0x110000) | (((rgb >> 4) & 15) * 0x1100) | ((rgb & 15) * 0x11);

		ChatColor color;
		color.code = ChatColor::CUSTOM;
		color.rgb = rgb;
		return color;
	}

	// as a cvar's value (cvar_t::value)
	return ChatColor::FromCode((int)atof(value));
}

ChatColor CHudChat::GetChatColor()
{
	return ParseChatColor(hud_chat_color.GetString());
}

ChatColor CHudChat::GetPlayerChatColor(int clientIndex)
{
	// the local player's own lines: the cvar itself, which the userinfo the server sends back follows with a delay
	// (the engine sends a client's userinfo changes to the other clients about once a second)
	if (clientIndex > 0 && clientIndex == sbo::GetLocalPlayerIndex())
		return GetChatColor();

	return ParseChatColor(sbo::GetPlayerInfoValue(clientIndex, "hud_chat_color"));
}

ChatColor CHudChat::GetPlayerNameColor(int clientIndex)
{
	return ParseChatColor(sbo::GetPlayerInfoValue(clientIndex, "namecolor"));
}

const char *CHudChat::GetColoredPlayerName(int clientIndex, const char *name)
{
	static char s_szNames[4][128];
	static int s_iNext = 0;

	CHudChat *pChat = Get();
	if (!name || !pChat || pChat->GetColorCodeAction() != ColorCodeAction::Handle)
		return name;

	const ChatColor color = GetPlayerNameColor(clientIndex);
	if (color.IsNone())
		return name;

	char *buf = s_szNames[s_iNext];
	s_iNext = (s_iNext + 1) % 4;
	ApplyNameColorCodes(name, (int)strlen(name), color, buf, (int)sizeof(s_szNames[0]));
	return buf;
}

// The value of a color cvar (hud_chat_color, namecolor) for a selection: "0".."8" or "xRRGGBB"
static void FormatChatColorValue(const ChatColor &color, char *value, int valueSize)
{
	if (color.IsCustom())
		snprintf(value, valueSize, "x%06X", color.rgb & 0xFFFFFF);
	else
		snprintf(value, valueSize, "%d", ChatColor::FromCode(color.code).code);
}

void CHudChat::SetChatColor(const ChatColor &color)
{
	// FCVAR_USERINFO, as SetNameColor: every client gets the new value with the player's next userinfo update
	char value[12];
	FormatChatColorValue(color, value, sizeof(value));
	hud_chat_color.SetValue(value);
}

ChatColor CHudChat::GetNameColor()
{
	// registered with the HUD's userinfo cvars (hud.cpp, FCVAR_ARCHIVE | FCVAR_USERINFO)
	if (!m_hNameColorCvar)
		m_hNameColorCvar = sbo::FindCvar("namecolor");
	return ParseChatColor(sbo::CvarGetString(m_hNameColorCvar));
}

void CHudChat::SetNameColor(const ChatColor &color)
{
	// the engine's Cvar_Set puts a FCVAR_USERINFO cvar in the userinfo and, while connected, sends the server a
	// "setinfo": every client gets the new value with the player's next userinfo update
	char value[12];
	FormatChatColorValue(color, value, sizeof(value));
	sbo::CvarSetString("namecolor", value);
}

ChatColor CHudChat::GetActiveChatColor()
{
	return (m_bChatColorPreview && m_ColorPickerTarget == ChatColorTarget::Chat) ? m_ChatColorPreview : GetChatColor();
}

ChatColor CHudChat::GetPickerColor()
{
	if (m_bChatColorPreview)
		return m_ChatColorPreview;
	return m_ColorPickerTarget == ChatColorTarget::Name ? GetNameColor() : GetChatColor();
}

void CHudChat::SetPickerColor(const ChatColor &color)
{
	if (m_ColorPickerTarget == ChatColorTarget::Name)
		SetNameColor(color);
	else
		SetChatColor(color);
}

void CHudChat::SetColorPickerTarget(ChatColorTarget target)
{
	if (target == m_ColorPickerTarget)
		return;

	// a color being dragged for the other setting is not selected
	if (m_pColorPicker)
		m_pColorPicker->CancelDrag();
	ClearChatColorPreview();
	HideColorTooltip(nullptr);

	m_ColorPickerTarget = target;

	// the wheel moves to the edited setting's color
	if (m_pColorPicker)
		m_pColorPicker->SetColor(GetPickerColor());
}

void CHudChat::SetChatColorPreview(const ChatColor &color)
{
	m_bChatColorPreview = true;
	m_ChatColorPreview = color;
}

void CHudChat::ClearChatColorPreview()
{
	m_bChatColorPreview = false;
}

Color CHudChat::GetChatColorDisplay(const ChatColor &color)
{
	if (color.IsCustom())
		return ClampColorCodeBrightness(color.GetRGB());

	CHudChat *pChat = Get();
	if (color.code >= 1 && color.code < CHAT_COLOR_PRESETS)
		return pChat ? pChat->m_ColorCodeColors[color.code] : s_DefaultColorCodeColors[color.code];

	return pChat ? pChat->GetDefaultTextColor() : Color(255, 255, 255, 255);
}

bool CHudChat::GetChatColorPreview(Color &color)
{
	// hud_colortext 0 and 2 draw no chat colors: the message comes back in the normal color
	const ChatColor sel = GetActiveChatColor();
	if (sel.IsNone() || GetColorCodeAction() != ColorCodeAction::Handle)
		return false;

	// exactly what the chat line will show (a wheel color: after the brightness floor)
	color = GetChatColorDisplay(sel);
	return true;
}

void CHudChat::GetChatColorName(const ChatColor &color, wchar_t *buf, int bufSizeBytes)
{
	char name[64];
	if (color.IsCustom())
	{
		// the code, as it can be typed in a message or used in a name
		WriteColorCode(color, name);
	}
	else if (color.IsNone())
	{
		V_strncpy(name, "None (default color)", sizeof(name));
	}
	else
	{
		// the nearest named color to the configured ColorCodeN (chatscheme.res; defaults: BHL's table,
		// ^1 red, ^2 green, ^3 yellow, ^4 blue, ^5 cyan, ^6 magenta, ^7 gray, ^8 white)
		static const struct
		{
			const char *name;
			int r, g, b;
		} s_Names[] = {
			{ "Red", 255, 0, 0 },
			{ "Green", 0, 255, 0 },
			{ "Yellow", 255, 255, 0 },
			{ "Blue", 0, 0, 255 },
			{ "Cyan", 0, 255, 255 },
			{ "Magenta", 255, 0, 255 },
			{ "Gray", 136, 136, 136 },
			{ "White", 255, 255, 255 },
			{ "Orange", 255, 170, 0 },
			{ "Black", 0, 0, 0 },
			{ "Pink", 255, 150, 200 },
			{ "Purple", 128, 0, 192 },
			{ "Brown", 140, 70, 20 },
			{ "Dark green", 0, 128, 0 },
			{ "Dark blue", 0, 0, 128 },
			{ "Dark red", 128, 0, 0 },
			{ "Light blue", 100, 170, 255 },
		};

		const Color c = GetColorCodeColor(color.code);
		int best = 0;
		int bestDist = INT_MAX;
		for (int i = 0; i < (int)(sizeof(s_Names) / sizeof(s_Names[0])); i++)
		{
			const int dr = c.r() - s_Names[i].r, dg = c.g() - s_Names[i].g, db = c.b() - s_Names[i].b;
			const int dist = dr * dr + dg * dg + db * db;
			if (dist < bestDist)
			{
				bestDist = dist;
				best = i;
			}
		}
		snprintf(name, sizeof(name), "%s (^%d)", s_Names[best].name, color.code);
	}
	V_UTF8ToUnicode(name, buf, bufSizeBytes);
}

//-----------------------------------------------------------------------------
// SBO: color picker popup
//-----------------------------------------------------------------------------
bool CHudChat::IsColorPickerOpen()
{
	return m_pColorPicker && m_pColorPicker->IsVisible();
}

void CHudChat::OpenColorPicker()
{
	if (!m_pColorPicker || m_nMessageMode == MM_NONE || IsColorPickerOpen())
		return;

	// it opens for the chat color (the collapsed circle's); the wheel starts at the current color
	ClearChatColorPreview();
	m_ColorPickerTarget = ChatColorTarget::Chat;
	m_pColorPicker->SetColor(GetChatColor());

	LayoutColorPicker();
	m_pColorPicker->SetVisible(true);
	m_pColorPicker->SetMouseInputEnabled(true);
	m_pColorPicker->MoveToFront();
	// the click that opened it is still down: only a later press can close it
	m_bPickerMouseDown = true;
}

void CHudChat::CloseColorPicker()
{
	HideColorTooltip(nullptr);
	ClearChatColorPreview(); // a color being dragged is not selected
	if (!m_pColorPicker)
		return;
	m_pColorPicker->CancelDrag();
	m_pColorPicker->SetMouseInputEnabled(false);
	m_pColorPicker->SetVisible(false);
}

void CHudChat::ToggleColorPicker()
{
	if (IsColorPickerOpen())
		CloseColorPicker();
	else
		OpenColorPicker();
}

void CHudChat::LayoutColorPicker()
{
	if (!m_pColorPicker || !m_pChatInput || !m_pChatInput->GetColorButton())
		return;

	// sized by the input line, as large as fits between the top of the chat panel and the input line
	int inputX, inputY, inputW, inputH;
	m_pChatInput->GetBounds(inputX, inputY, inputW, inputH);
	m_pColorPicker->SetMetrics(inputH, inputY - 2);

	// anchored to the collapsed circle, directly above the input line, inside the chat panel
	int bx = 0, by = 0;
	m_pChatInput->GetColorButton()->LocalToScreen(bx, by);
	ScreenToLocal(bx, by);

	int chatW, chatH, popW, popH;
	GetSize(chatW, chatH);
	m_pColorPicker->GetSize(popW, popH);

	int x = bx - 4;
	int y = inputY - popH - 2;
	if (x > chatW - popW)
		x = chatW - popW;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	m_pColorPicker->SetPos(x, y);
}

void CHudChat::ShowColorTooltip(vgui2::Panel *pOwner, const wchar_t *text)
{
	if (!m_pColorTooltip || !pOwner || !IsColorPickerOpen())
		return;

	m_pColorTooltip->SetTooltipText(text);

	// centered above its owner, just above the popup; without room there, right of the popup at the owner's
	// height; inside the chat panel
	int ox = 0, oy = 0;
	pOwner->LocalToScreen(ox, oy);
	ScreenToLocal(ox, oy);

	int px, py, popW, popH, chatW, chatH, tipW, tipH;
	m_pColorPicker->GetBounds(px, py, popW, popH);
	GetSize(chatW, chatH);
	m_pColorTooltip->GetSize(tipW, tipH);

	int x = ox + pOwner->GetWide() / 2 - tipW / 2;
	int y = py - tipH - 1;
	if (y < 0)
	{
		x = px + popW + 2;
		y = oy;
	}
	if (x > chatW - tipW)
		x = chatW - tipW;
	if (x < 0)
		x = 0;
	if (y > chatH - tipH)
		y = chatH - tipH;
	if (y < 0)
		y = 0;
	m_pColorTooltip->SetPos(x, y);
	m_pColorTooltip->SetVisible(true);
	m_pColorTooltip->MoveToFront();
	m_pColorTooltipOwner = pOwner;
}

void CHudChat::HideColorTooltip(vgui2::Panel *pOwner)
{
	// nullptr: hide whatever it shows; otherwise only if it still shows that panel's (enter/exit can arrive in
	// either order when moving between neighbors)
	if (!m_pColorTooltip || (pOwner && pOwner != m_pColorTooltipOwner))
		return;
	m_pColorTooltip->SetVisible(false);
	m_pColorTooltipOwner = nullptr;
}

Color CHudChat::GetColorCodeColor(int code)
{
	Assert(code >= 0 && code <= 9);
	return m_ColorCodeColors[code];
}

//-----------------------------------------------------------------------------
// Purpose: Parses a line of text for color markup and inserts it via Colorize()
//-----------------------------------------------------------------------------
void CHudChatLine::InsertAndColorizeText(wchar_t *buf, int clientIndex)
{
	// buf will look something like this
	// \x02PlayerName: A chat message!\0
	//     ^~~~~~~~~~^~~~~~~~~~~~~~~~~
	// <player color>   <default color>
	// \x02 is COLOR_PLAYERNAME
	//
	// Markup (TextColor), anywhere in the message:
	//   \x01 normal           \x02 sender's team color   \x03 player name source
	//   \x04 location         \x05 achievement           \x06 custom (SetCustomColor)
	//   \x07RRGGBB            \x08RRGGBBAA (alpha kept, not faded)
	// plus BHL's ^0..^9 color codes and SBO's ^xRGB / ^#RRGGBB (hud_colortext).
	// Colors per CHudChat::GetTextColorForClient (BHL's mapping).

	// m_textRanges contains TextRanges.
	// Each TextRange describes color of a substring of the chat message.
	//
	// start - index of the first char of the substring
	// end - index AFTER the last char of the substring
	// color - color of the substring

	if (m_text)
	{
		delete[] m_text;
		m_text = NULL;
	}
	m_textRanges.RemoveAll();

	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());

	if (pChat == NULL)
		return;

	ColorCodeAction colorCodeAction = pChat->GetColorCodeAction();

	// Color the message
	{
		wchar_t *buf2 = buf;
		int len = wcslen(buf2);
		m_text = new wchar_t[len + 1];

		wchar_t *str = m_text;

		// Add initial color of the message
		{
			TextRange range;
			range.start = 0;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}

		int last_range_idx = 0;
		int is_player_msg = *buf2 == COLOR_PLAYERNAME ? 1 : 0;
		bool bNameColorReset = false;
		bool bNameColorPending = false; // SBO: "\x02*DEAD* <name>: ": the tag stays normal, the name gets the color
		bool bMessageColor = false;     // SBO: in the sender's message, drawn in his chat color (SetMessageColor)

		// Starts a new color range at the current output position
		auto fnAddRange = [&](int pos, Color color, bool preserveAlpha) {
			TextRange range;
			range.start = pos;
			range.color = color;
			range.preserveAlpha = preserveAlpha;
			range.end = len;

			m_textRanges[last_range_idx].end = pos;
			m_textRanges.AddToTail(range);
			last_range_idx = m_textRanges.Count() - 1;
		};

		// Keeps the player name position (output coordinates) up to date when
		// markup characters are removed at output position pos
		auto fnRemoved = [&](int pos, int count) {
			if (pos < m_iNameStart)
				m_iNameStart -= count;
			else if (pos < m_iNameStart + m_iNameLength)
				m_iNameLength -= count;
		};

		while (*buf2)
		{
			int pos = str - m_text;

			// SBO: player color starts at the name (after the server's category tag)
			if (bNameColorPending && pos == m_iNameStart)
			{
				fnAddRange(pos, pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex), false);
				bNameColorPending = false;
			}

			// Reset color after player name to default
			// SBO: only once (BHL re-added it for every markup char removed at that position,
			// which overrode markup placed right after the name)
			if (!bNameColorReset && pos == m_iNameStart + m_iNameLength && is_player_msg) // The only color is player name
			{
				fnAddRange(pos, pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex), false);
				bNameColorReset = true;
			}

			// SBO: the sender's chat color starts at his message (a code at its start is handled below and wins)
			if (!bMessageColor && m_iMessageStart >= 0 && buf2 - buf >= m_iMessageStart)
			{
				fnAddRange(pos, m_clrMessage, false);
				bMessageColor = true;
			}

			// SBO: ^0..^9 (2 chars), ^xRGB (5 chars) or ^#RRGGBB (8 chars)
			const int codeLen = colorCodeAction != ColorCodeAction::Ignore ? GetColorCodeLength(buf2) : 0;

			if (codeLen > 0) // Parse colorcodes
			{
				if (colorCodeAction == ColorCodeAction::Handle)
				{
					Color color;
					if (!pChat->GetColorCodeColor(buf2, color))
					{
						// ^0 / ^9: back to the default (SBO: in the sender's message, his chat color)
						if (bMessageColor)
							color = m_clrMessage;
						else if (pos <= m_iNameStart + m_iNameLength && is_player_msg)
							color = pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex);
						else
							color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
					}

					fnAddRange(pos, color, false);
				}

				if (pos >= m_iNameStart && pos < m_iNameStart + m_iNameLength)
					m_iNameLength -= codeLen;

				buf2 += codeLen;
			}
			// m_iNameLength > 0 fixes the miniag issue too, but with the drawback of not coloring
			// the player name according to the player team
			else if (*buf2 == COLOR_PLAYERNAME && pos == 0 && m_iNameLength > 0) // Color of the player name
			{
				// shift name start position since we are removing a character
				m_iNameStart--;
				buf2++;

				// SBO: a category tag before the name keeps the normal color
				if (m_iNameStart > 0)
					bNameColorPending = true;
				else
					fnAddRange(pos, pChat->GetTextColorForClient(COLOR_PLAYERNAME, clientIndex), false);
			}
			else if (*buf2 > 0 && *buf2 < COLOR_MAX) // SBO: \x01..\x08 markup anywhere
			{
				TextColor code = (TextColor)*buf2;
				int markupLen = 1;

				if (code == COLOR_HEXCODE || code == COLOR_HEXCODE_ALPHA)
				{
					int numDigits = code == COLOR_HEXCODE ? 6 : 8;
					Color color;
					if (ParseHexColor(buf2 + 1, numDigits, color))
					{
						fnAddRange(pos, color, code == COLOR_HEXCODE_ALPHA);
						markupLen += numDigits;
					}
					// else: malformed, drop the marker byte only
				}
				else
				{
					fnAddRange(pos, pChat->GetTextColorForClient(code, clientIndex), false);
				}

				fnRemoved(pos, markupLen);
				buf2 += markupLen;
			}
			else
			{
				*str = *buf2;
				str++;
				buf2++;
			}
		}
		*str = '\0';

		// Add final text range if need to
		if (m_textRanges[last_range_idx].end != len)
		{
			TextRange range;
			range.start = m_textRanges[last_range_idx].end;
			range.color = pChat->GetTextColorForClient(COLOR_NORMAL, clientIndex);
			range.end = len;
			m_textRanges.AddToTail(range);
		}
	}

	// SBO: hyperlinks (on the markup-free text, so link targets never contain markup)
	FindLinks();

	// Add text to the history as described in m_textRanges
	Colorize();
}

//-----------------------------------------------------------------------------
// Purpose: Inserts colored text into the RichText control at the given alpha
//-----------------------------------------------------------------------------
void CHudChatLine::Colorize(int alpha)
{
	// clear out text
	SetText("");

	CHudChat *pChat = dynamic_cast<CHudChat *>(GetParent());
	CHudChatHistory *pHistory = pChat ? pChat->GetChatHistory() : nullptr;

	// SBO: range ends are in input coordinates (before markup removal); clamp to the text
	const int textLen = m_text ? (int)wcslen(m_text) : 0;

	// SBO: the history gets this line as an entry (CHudChat::AddHistoryEntry inserts it), so it can be rebuilt
	// without the transient lines later
	ChatHistoryEntry *pEntry = nullptr;
	if (pHistory)
	{
		pEntry = new ChatHistoryEntry;
		pEntry->text = CloneWString(m_text ? m_text : L"");
		pEntry->links.AddVectorToTail(m_links);
		pEntry->bTransient = m_bTransient;
		if (hud_saytext.GetBool())
		{
			pEntry->flFadeEnd = GetCurTime() + hud_saytext_time.GetFloat();
			pEntry->flFadeLength = CHAT_HISTORY_IDLE_FADE_TIME;
		}
		else
		{
			pEntry->flFadeEnd = GetCurTime();
			pEntry->flFadeLength = 0;
		}
	}

	wchar_t wText[4096];
	Color color;
	for (int i = 0; i < m_textRanges.Count(); ++i)
	{
		int rangeStart = m_textRanges[i].start;
		int rangeEnd = m_textRanges[i].end < textLen ? m_textRanges[i].end : textLen;
		int len = rangeEnd - rangeStart;
		if (len > 0 && len < (int)ARRAYSIZE(wText))
		{
			wcsncpy(wText, m_text + rangeStart, len);
			wText[len] = 0;
			color = m_textRanges[i].color;
			if (!m_textRanges[i].preserveAlpha)
			{
				color[3] = alpha;
			}

			InsertColorChange(color);
			InsertString(wText);

			if (pEntry)
			{
				// SBO: split the range at link boundaries; links get the link color, the
				// underline font and a click action (the validated URL)
				int pos = rangeStart;
				while (pos < rangeEnd)
				{
					int iLink = -1;
					int segEnd = rangeEnd;
					for (int l = 0; l < m_links.Count(); l++)
					{
						const ChatLink &link = m_links[l];
						if (link.end <= pos)
							continue;
						if (link.start <= pos)
						{
							iLink = l;
							segEnd = link.end < rangeEnd ? link.end : rangeEnd;
						}
						else if (link.start < segEnd)
						{
							segEnd = link.start;
						}
						break; // sorted, non-overlapping
					}

					ChatHistorySegment seg;
					seg.start = pos;
					seg.end = segEnd;
					seg.link = iLink;
					seg.color = color;
					if (iLink >= 0)
					{
						seg.color = pChat->GetLinkColor();
						seg.color[3] = alpha; // always visible, even inside \x08 alpha markup
					}
					pEntry->segments.AddToTail(seg);

					pos = segEnd;
				}
			}
		}
	}

	if (pEntry)
		pChat->AddHistoryEntry(pEntry);

	InvalidateLayout(true);
}

//-----------------------------------------------------------------------------
// Purpose: SBO: chat history entries. The RichText gets exactly what BHL's Colorize inserted: "\n", the segments
// (color, link, fade) and the fade terminator. Fades are stored as absolute times so a rebuild keeps them.
//-----------------------------------------------------------------------------
void CHudChat::AddHistoryEntry(ChatHistoryEntry *pEntry)
{
	if (!pEntry)
		return;

	m_HistoryEntries.AddToTail(pEntry);
	m_nHistoryChars += (int)wcslen(pEntry->text) + 1;
	if (pEntry->bTransient)
		m_nTransientEntries++;

	// the RichText drops its older half when full; the list only needs what a rebuild can show
	while (m_HistoryEntries.Count() > 1 && m_nHistoryChars > CHAT_HISTORY_MAX_CHARS)
	{
		ChatHistoryEntry *pOld = m_HistoryEntries[0];
		m_nHistoryChars -= (int)wcslen(pOld->text) + 1;
		if (pOld->bTransient)
			m_nTransientEntries--;
		m_HistoryEntries.Remove(0);
		delete pOld;
	}

	InsertHistoryEntry(*pEntry);
}

void CHudChat::InsertHistoryEntry(const ChatHistoryEntry &entry)
{
	CHudChatHistory *pHistory = GetChatHistory();
	if (!pHistory)
		return;

	pHistory->InsertString("\n");

	const float flSustain = entry.flFadeEnd - GetCurTime();
	wchar_t wText[4096];

	for (int i = 0; i < entry.segments.Count(); i++)
	{
		const ChatHistorySegment &seg = entry.segments[i];
		int segLen = seg.end - seg.start;
		if (segLen <= 0 || segLen >= (int)ARRAYSIZE(wText))
			continue;

		wcsncpy(wText, entry.text + seg.start, segLen);
		wText[segLen] = 0;

		pHistory->InsertColorChange(seg.color);
		if (seg.link >= 0 && seg.link < entry.links.Count())
		{
			pHistory->InsertClickableTextStart(entry.links[seg.link].url);
			pHistory->InsertString(wText);
			pHistory->InsertFade(flSustain, entry.flFadeLength);
			pHistory->InsertClickableTextEnd();
		}
		else
		{
			pHistory->InsertString(wText);
			pHistory->InsertFade(flSustain, entry.flFadeLength);
		}
	}

	// end of this message's fade region (BHL: only after a non-empty last range)
	pHistory->InsertFade(-1, -1);
}

void CHudChat::PurgeTransientHistory(bool bAll)
{
	if (m_nTransientEntries <= 0)
		return;

	const float flNow = GetCurTime();
	bool bRemoved = false;

	for (int i = m_HistoryEntries.Count() - 1; i >= 0; i--)
	{
		ChatHistoryEntry *pEntry = m_HistoryEntries[i];
		if (!pEntry->bTransient || (!bAll && flNow < pEntry->flFadeEnd))
			continue;

		m_nHistoryChars -= (int)wcslen(pEntry->text) + 1;
		m_nTransientEntries--;
		m_HistoryEntries.Remove(i);
		delete pEntry;
		bRemoved = true;
	}

	if (bRemoved)
		RebuildHistory();
}

void CHudChat::RebuildHistory()
{
	CHudChatHistory *pHistory = GetChatHistory();
	if (!pHistory)
		return;

	// as after construction (CHudChatHistory: InsertFade(-1, -1))
	pHistory->SetText("");
	pHistory->InsertFade(-1, -1);

	for (int i = 0; i < m_HistoryEntries.Count(); i++)
		InsertHistoryEntry(*m_HistoryEntries[i]);

	pHistory->GotoTextEnd();
}

//-----------------------------------------------------------------------------
// Purpose: SBO: finds the links in m_text
//-----------------------------------------------------------------------------
void CHudChatLine::FindLinks()
{
	m_links.RemoveAll();
	if (!m_text)
		return;

	const int textLen = (int)wcslen(m_text);
	for (int pos = 0; pos < textLen;)
	{
		ChatLink link;
		int consumed = ParseChatLink(m_text, textLen, pos, link);
		if (consumed > 0)
		{
			if (link.end > link.start)
				m_links.AddToTail(link);
			pos += consumed;
		}
		else
		{
			pos++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : CHudChatLine
//-----------------------------------------------------------------------------
CHudChatLine *CHudChat::FindUnusedChatLine(void)
{
	return m_ChatLine;
}

void CHudChat::Send(void)
{
	wchar_t szTextbuf[MAX_CHAT_INPUT_STRING_LEN + 1];

	m_pChatInput->GetMessageText(szTextbuf, sizeof(szTextbuf));

	AddToInputHistory(szTextbuf);

	char ansi[2 * MAX_CHAT_INPUT_STRING_LEN + 1];
	g_pVGuiLocalize->ConvertUnicodeToANSI(szTextbuf, ansi, sizeof(ansi));

	int len = Q_strlen(ansi);

	// remove the \n
	if (len > 0 && ansi[len - 1] == '\n')
	{
		ansi[len - 1] = '\0';
		len--;
	}

	char *pstr = ansi;

	// SBO: the message goes out exactly as typed. The chat color (cvar hud_chat_color) is not part of the text: it
	// travels in the userinfo and every client colors the message when drawing it (PrintChatLine).
	const int iChunkLen = MAX_CHAT_STRING_LEN;

	while (len > iChunkLen)
	{
		char buf[MAX_CHAT_STRING_LEN + 1];

		// Split ansi - take only iChunkLen chars
		Q_strncpy(buf, pstr, iChunkLen + 1);
		pstr += iChunkLen;
		len -= iChunkLen;

		// Check if we split a multibyte char
		int mblen = 0;
		if ((buf[iChunkLen - 1] & 0b11000000) == 0b11000000)
		{
			// Beginning of multibyte char
			mblen = 1;
		}
		else if ((buf[iChunkLen - 1] & 0b11000000) == 0b10000000)
		{
			// Middle of multibyte char
			mblen = 1;
			for (; mblen <= 6 && (buf[iChunkLen - mblen] & 0b11000000) != 0b11000000; mblen++)
				;
		}

		// SBO: nor a color code (^xRGB, ^#RRGGBB): a code cut in two would show as text in both parts
		const int cut = iChunkLen - mblen;
		for (int back = 1; back < COLOR_CODE_MAX_LEN && back < cut; back++)
		{
			if (buf[cut - back] == '^')
			{
				if (GetColorCodeLength(pstr - iChunkLen + cut - back) > back)
					mblen += back;
				break;
			}
		}

		// Leave mblen chars for the next iteration
		if (mblen > 0)
		{
			len += mblen;
			pstr -= mblen;
			buf[iChunkLen - mblen] = '\0';
		}

		// Say the string
		char szbuf[144]; // more than 128
		Q_snprintf(szbuf, sizeof(szbuf), "%s \"%s\"", "say" /* SBO: never say_team */, buf);
		sbo::ExecClientCmd(szbuf);
	}

	if (len > 0)
	{
		char szbuf[144]; // more than 128
		Q_snprintf(szbuf, sizeof(szbuf), "%s \"%s\"", "say" /* SBO: never say_team */, pstr);

		sbo::ExecClientCmd(szbuf);
	}

	m_pChatInput->ClearEntry();
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : vgui2::Panel
//-----------------------------------------------------------------------------
vgui2::Panel *CHudChat::GetInputPanel(void)
{
	return m_pChatInput->GetInputPanel();
}

int CHudChat::MsgFunc_SayText(const char *pszName, int iSize, void *pbuf)
{
	// BEGIN_READ / READ_BYTE / READ_STRING (SBO's server also appends a short,
	// m_bOutOfPlay, which the client ignores like SBO's legacy chat did)
	const unsigned char *data = static_cast<const unsigned char *>(pbuf);
	if (iSize < 1)
		return 1;

	int client = data[0]; // the client who spoke the message

	char szString[2048];
	int n = 0;
	for (int i = 1; i < iSize && data[i] && n < (int)sizeof(szString) - 1; i++)
		szString[n++] = (char)data[i];
	szString[n] = '\0';

	// SBO: no dead/alive filtering -- everybody sees all chat.

	// print raw chat text
	ChatPrintf(client, "%s", szString);

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudChat::Clear(void)
{
	// Kill input prompt
	StopMessageMode();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *newmap -
//-----------------------------------------------------------------------------
void CHudChat::InitHUDData()
{
	Clear();
	m_flHistoryFadeTime = 0;
}

void CHudChat::LevelShutdown(void)
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CHudChat::ChatPrintf(int iPlayerIndex, const char *fmt, ...)
{
	va_list marker;
	char msg[4096];

	va_start(marker, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, marker);
	va_end(marker);

	PrintChatLine(iPlayerIndex, msg, false);
}

void CHudChat::PrintChatLine(int iPlayerIndex, const char *text, bool bTransient)
{
	char msg[4096];
	Q_strncpy(msg, text, sizeof(msg));

	// Strip any trailing '\n'
	if (strlen(msg) > 0 && msg[strlen(msg) - 1] == '\n')
	{
		msg[strlen(msg) - 1] = 0;
	}

	// SBO: no AG location substitution / real name replacement (BHL-specific)

	// Strip leading \n characters ( or notify/color signifiers ) for empty string check
	char *pmsg = msg;
	while (*pmsg)
	{
		// SBO: a whole color code (^N, ^xRGB or ^#RRGGBB; BHL skipped only the '^', so a line of nothing but codes got through)
		const int codeLen = GetColorCodeLength(pmsg);
		if (codeLen > 0)
			pmsg += codeLen;
		else if (*pmsg == '\n' || (*pmsg > 0 && *pmsg < COLOR_MAX))
			pmsg++;
		else
			break;
	}

	if (!*pmsg)
		return;

	// Now strip just newlines, since we want the color info for printing
	pmsg = msg;
	while (*pmsg && (*pmsg == '\n'))
	{
		pmsg++;
	}

	if (!*pmsg)
		return;

	// Console copy, timestamped like BHL (without the color markup). SBO: its color codes are drawn as colors when the
	// console can (hud_colortext 1, docs/CONSOLE_COLOR.md: consoleCodes), else removed (kept with hud_colortext 0).
	char consoleMsg[4096 + 32];
	char consoleCodes[4096 + 32];
	{
		time_t now;
		struct tm *current;
		char time_buf[32];
		time(&now);
		current = localtime(&now);
		snprintf(time_buf, sizeof(time_buf), "[%02i:%02i:%02i] ", current->tm_hour, current->tm_min, current->tm_sec);

		char plain[4096];
		Q_strncpy(plain, pmsg, sizeof(plain));
		RemoveColorMarkup(plain);

		if (GetColorCodeAction() == ColorCodeAction::Handle || GetColorCodeAction() == ColorCodeAction::Strip)
			snprintf(consoleMsg, sizeof(consoleMsg), "%s %s\n", time_buf, RemoveColorCodes(plain));
		else
			snprintf(consoleMsg, sizeof(consoleMsg), "%s %s\n", time_buf, plain);
		snprintf(consoleCodes, sizeof(consoleCodes), "%s %s\n", time_buf, plain);
	}

	// If a player is muted for voice, also mute them for text because jerks gonna jerk.
	int iLocalPlayer = sbo::GetLocalPlayerIndex();
	if (iLocalPlayer > 0 && cl_mute_all_comms.GetBool() && iPlayerIndex != 0 && iPlayerIndex != iLocalPlayer)
	{
		if (sbo::IsPlayerBlocked(iPlayerIndex))
			return;
	}


	CHudChatLine *line = FindUnusedChatLine();
	if (!line)
	{
		line = FindUnusedChatLine();
	}

	if (!line)
	{
		return;
	}

	line->SetText("");

	int iNameStart = 0;
	int iNameLength = 0;
	int iRealNameStart = 0; // SBO: the name itself (iNameStart / iNameLength take the category tag in)
	int iRealNameLength = 0;

	const char *playerName = "Console";
	const char *engineName = sbo::GetPlayerName(iPlayerIndex);
	if (engineName)
	{
		playerName = engineName;
	}

	int msglen = strlen(pmsg);
	int bufSize = (msglen + 1) * sizeof(wchar_t);
	wchar_t *wbuf = static_cast<wchar_t *>(_alloca(bufSize));
	if (wbuf)
	{
		Color clrNameColor = GetClientColor(iPlayerIndex);

		line->SetExpireTime();

		g_pVGuiLocalize->ConvertANSIToUnicode(pmsg, wbuf, bufSize);

		// find the player's name in the unicode string, in case there is no color markup
		const char *pName = playerName;

		if (pName)
		{
			// miniag issue: server-side is giving a name with colorcodes while say message doesn't have them
			// so we need to remove them by ourselves and try to find again
			if (engineName && !strstr(pmsg, playerName))
				pName = RemoveColorCodes(pName);

			wchar_t wideName[MAX_CHAT_PLAYER_NAME];
			g_pVGuiLocalize->ConvertANSIToUnicode(pName, wideName, sizeof(wideName));

			// SBO: after the server's category tag ("\x02*DEAD* <name>: "; ASCII, so byte = wchar offsets), else
			// anywhere (a name like "D" would match inside the tag)
			const wchar_t *nameInString = nullptr;
			int iTag = (pmsg[0] == COLOR_PLAYERNAME) ? SboChat_TagLength(pmsg + 1) : 0;
			if (iTag > 0 && iTag + 1 <= (int)wcslen(wbuf))
				nameInString = wcsstr(wbuf + 1 + iTag, wideName);
			if (!nameInString)
				nameInString = wcsstr(wbuf, wideName);

			if (nameInString)
			{
				iNameStart = (nameInString - wbuf);
				iNameLength = wcslen(wideName);
				iRealNameStart = iNameStart;
				iRealNameLength = iNameLength;

				// SBO: the category tag takes the name's colour too ("*DEAD* <name>" coloured together)
				if (iTag > 0 && nameInString >= wbuf + 1 + iTag)
				{
					iNameLength += iNameStart - 1;
					iNameStart = 1;
				}
			}
		}

		// SBO: public chat used to come as "\x01<name>: text" (name not colored). Treat it like
		// the "\x02<name>: text" form so every player message gets a team-colored name.
		if (wbuf[0] == COLOR_NORMAL && engineName && iNameLength > 0)
			wbuf[0] = COLOR_PLAYERNAME;

		// SBO: the sender's name color (cvar namecolor, from his userinfo), for the lines with a sender name
		// (say / say_team): written into the line as color codes around the name, which InsertAndColorizeText
		// draws like codes typed in the name -- the name's uncoded text in the name color instead of the team
		// color, the category tag before it still in the team color, the text after the name in the normal one.
		// Only the copy that is drawn changes: the console line was made above. hud_colortext 0 / 2: no name colors.
		// (Lines the server builds around a name -- radio, "has left the game" -- carry the code already:
		// UTIL_ChatPlayerName.)
		if (wbuf[0] == COLOR_PLAYERNAME && engineName && iRealNameLength > 0 && GetColorCodeAction() == ColorCodeAction::Handle)
		{
			const ChatColor nameColor = GetPlayerNameColor(iPlayerIndex);
			if (!nameColor.IsNone())
			{
				// the code once, and each ^0 / ^9 (2 chars) in the name becomes a code
				const int wlen = (int)wcslen(wbuf);
				const int maxLen = wlen + COLOR_CODE_MAX_LEN * (1 + iRealNameLength / 2);
				wchar_t *wbuf2 = static_cast<wchar_t *>(_alloca((maxLen + 1) * sizeof(wchar_t)));

				wcsncpy(wbuf2, wbuf, iRealNameStart);
				const int coloredLen = ApplyNameColorCodes(wbuf + iRealNameStart, iRealNameLength, nameColor,
				    wbuf2 + iRealNameStart, maxLen + 1 - iRealNameStart - (wlen - iRealNameStart - iRealNameLength));
				wcscpy(wbuf2 + iRealNameStart + coloredLen, wbuf + iRealNameStart + iRealNameLength);

				iNameLength += coloredLen - iRealNameLength;
				wbuf = wbuf2;
			}
		}

		// SBO: the sender's chat color (cvar hud_chat_color, from his userinfo) for what a player says: the text after
		// "<name>: " of a say / say_team line, which is exactly what he typed (the color is not in the text).
		// InsertAndColorizeText draws it in that color instead of the normal one; codes typed in the message apply
		// from where they are and ^0 / ^9 go back to the chat color. Lines without "<name>: " right after the
		// sender's name (radio, server and plugin texts, also with a player index) keep the normal color.
		// hud_colortext 0 / 2: no chat colors.
		int iMessageStart = -1;
		Color clrMessage;
		if (wbuf[0] == COLOR_PLAYERNAME && engineName && iRealNameLength > 0 && GetColorCodeAction() == ColorCodeAction::Handle)
		{
			// iNameStart + iNameLength: the end of the name as drawn (with its name color codes)
			const wchar_t *pAfterName = wbuf + iNameStart + iNameLength;
			if (pAfterName[0] == L':' && pAfterName[1] == L' ')
			{
				const ChatColor chatColor = GetPlayerChatColor(iPlayerIndex);
				if (!chatColor.IsNone())
				{
					iMessageStart = (int)(pAfterName + 2 - wbuf);
					clrMessage = GetChatColorDisplay(chatColor);
				}
			}
		}

		line->SetVisible(false);
		line->SetMessageColor(iMessageStart, clrMessage);
		line->SetNameStart(iNameStart);
		line->SetNameLength(iNameLength);
		line->SetNameColor(clrNameColor);
		line->SetTransient(bTransient);

		line->InsertAndColorizeText(wbuf, iPlayerIndex);
	}

	if (hud_saytext.GetBool() && hud_saytext_sound.GetFloat() > 0)
		sbo::PlaySoundByName(m_pszChatSoundPath, hud_saytext_sound.GetFloat());

	// Print to console (SBO: not the transient lines)
	if (!bTransient && !VGui2_Console_PrintColorCodes(consoleCodes))
		sbo::ConsolePrint(consoleMsg);
}

//-----------------------------------------------------------------------------
// Purpose: SBO: the typed text shows in the chat color the message will be drawn in (CHudChat::GetChatColorPreview),
// also the one being picked while the wheel is dragged.
// Every frame while the input line is open, so a picker or hud_chat_color change shows at once.
// Only the text color changes: the caret (TextEntry.CursorColor) and the selection (TextEntry.SelectedTextColor
// on TextEntry.SelectedBgColor) keep their scheme colours.
//-----------------------------------------------------------------------------
void CHudChatEntry::OnThink()
{
	BaseClass::OnThink();

	if (!IsVisible())
		return;

	Color color = m_NormalTextColor;
	CHudChat *pChat = CHudChat::Get();
	if (pChat)
	{
		Color preview;
		if (pChat->GetChatColorPreview(preview))
			color = Color(preview.r(), preview.g(), preview.b(), m_NormalTextColor.a());
	}

	if (GetFgColor() != color)
		SetFgColor(color);
}

void CHudChatEntry::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
	{
		// SBO: Escape with the color picker open closes only the picker
		CHudChat *pChat = dynamic_cast<CHudChat *>(m_pHudChat);
		if (pChat && pChat->IsColorPickerOpen())
		{
			pChat->CloseColorPicker();
			return;
		}
	}

	if (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER || code == vgui2::KEY_ESCAPE)
	{
		if (code != vgui2::KEY_ESCAPE)
		{
			if (m_pHudChat)
			{
				PostMessage(m_pHudChat, new KeyValues("ChatEntrySend"));
			}
		}

		// End message mode.
		if (m_pHudChat)
		{
			PostMessage(m_pHudChat, new KeyValues("ChatEntryStopMessageMode"));
		}
	}
	else if (code == vgui2::KEY_TAB)
	{
		// Ignore tab, otherwise vgui will screw up the focus.
		return;
	}
	else if (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN)
	{
		// SBO: input line history, like the console
		if (CHudChat *pChat = dynamic_cast<CHudChat *>(m_pHudChat))
			pChat->BrowseInputHistory(code == vgui2::KEY_UP ? 1 : -1);
	}
	else if (code == vgui2::KEY_PAGEUP || code == vgui2::KEY_PAGEDOWN)
	{
		// SBO: scroll the chat history from the keyboard
		if (CHudChat *pChat = dynamic_cast<CHudChat *>(m_pHudChat))
			pChat->ScrollHistory(code == vgui2::KEY_PAGEUP ? 1 : -1);
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

void CHudChatEntry::OnMouseWheeled(int delta)
{
	// SBO: wheel over the input line scrolls the history too
	if (CHudChat *pChat = dynamic_cast<CHudChat *>(m_pHudChat))
	{
		if (CHudChatHistory *pHistory = pChat->GetChatHistory())
			pHistory->PostMessage(pHistory, new KeyValues("MoveScrollBar", "delta", delta));
	}
}

//-----------------------------------------------------------------------------
// SBO: input line history
//-----------------------------------------------------------------------------
CHudChat::~CHudChat()
{
	for (int i = 0; i < m_InputHistory.Count(); i++)
		delete[] m_InputHistory[i];
	for (int i = 0; i < m_HistoryEntries.Count(); i++)
		delete m_HistoryEntries[i];
}

void CHudChat::AddToInputHistory(const wchar_t *text)
{
	m_iInputHistoryPos = -1;

	// skip empty / whitespace-only input
	const wchar_t *p = text;
	while (*p == L' ' || *p == L'\t' || *p == L'\n')
		p++;
	if (!*p)
		return;

	// don't store the same line twice in a row
	if (m_InputHistory.Count() > 0 && !wcscmp(m_InputHistory[0], text))
		return;

	m_InputHistory.InsertBefore(0, CloneWString(text));

	while (m_InputHistory.Count() > CHAT_INPUT_HISTORY_SIZE)
	{
		int last = m_InputHistory.Count() - 1;
		delete[] m_InputHistory[last];
		m_InputHistory.Remove(last);
	}
}

void CHudChat::BrowseInputHistory(int dir)
{
	if (m_InputHistory.Count() == 0)
		return;

	// remember what was being typed before browsing
	if (m_iInputHistoryPos == -1)
		m_pChatInput->GetMessageText(m_szInputDraft, sizeof(m_szInputDraft));

	int newPos = m_iInputHistoryPos + dir; // dir: +1 older, -1 newer
	if (newPos >= m_InputHistory.Count())
		newPos = m_InputHistory.Count() - 1;
	if (newPos < -1)
		newPos = -1;

	if (newPos == m_iInputHistoryPos)
		return;

	m_iInputHistoryPos = newPos;
	m_pChatInput->SetEntry(newPos == -1 ? m_szInputDraft : m_InputHistory[newPos]);
	m_pChatInput->GotoEntryEnd();
}

void CHudChat::ScrollHistory(int dir)
{
	CHudChatHistory *pHistory = GetChatHistory();
	if (!pHistory)
		return;

	// RichText::MoveScrollBarDirect: positive = up, in lines
	int lines = (m_iFontHeight > 0) ? pHistory->GetTall() / m_iFontHeight - 1 : 4;
	if (lines < 1)
		lines = 1;
	pHistory->PostMessage(pHistory, new KeyValues("MoveScrollBarDirect", "delta", dir * lines));
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_Chat_Init()
{
	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	if (!CHudChat::Get())
		new CHudChat();

	CHudChat::Get()->Init();
	VGui2_RegisterDebugCommands();
	VGui2_RegisterOptionsCommand();
	VGui2_RegisterServerBrowserCommand();
	VGui2_RegisterMenusCvar();
	VGui2_RegisterOptionsWindowCommand();
	VGui2_RegisterCreateServerCommand();
	return true;
}

void VGui2_Chat_InitHUDData()
{
	if (CHudChat::Get())
		CHudChat::Get()->InitHUDData();
}

void VGui2_Chat_Print(const char *msg)
{
	if (CHudChat::Get())
		CHudChat::Get()->Printf("%s", msg);
}

bool VGui2_Chat_IsInMessageMode()
{
	return CHudChat::Get() && CHudChat::Get()->GetMessageMode() != MM_NONE;
}
