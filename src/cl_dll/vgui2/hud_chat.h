//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VGUI2 chat for SBO.
//
// Ported from BugfixedHL-Rebased src/game/client/hud/chat.h (itself derived from
// Source SDK 2013 hud_basechat.h). SBO changes:
//   - no CHudElemBase/ConVar/CPlayerInfo/AG location/results log: SBO data comes
//     through sbo_bridge.h, cvars are plain engine cvars (archived);
//   - the \x01..\x08 color markup is fully parsed (BHL only declared the codes).
//
// $NoKeywords: $
//=============================================================================//

#ifndef SBO_VGUI2_HUD_CHAT_H
#define SBO_VGUI2_HUD_CHAT_H

#include <vgui/KeyCode.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Label.h>
#include <tier1/utlvector.h>
#include <tier1/strtools.h>
#include <KeyValues.h>
#include <Color.h>

#include "sbo_bridge.h"

class CHudChatInputLine;
class CHudChatEntry;
class CHudChatColorButton;
class CHudChatColorPicker;
class CHudChatColorTooltip;

namespace vgui2
{
class IScheme;
};

#define CHATLINE_NUM_FLASHES 8.0f
#define CHATLINE_FLASH_TIME  5.0f
#define CHATLINE_FADE_TIME   1.0f

#define CHAT_HISTORY_FADE_TIME      0.25f
#define CHAT_HISTORY_IDLE_TIME      15.0f
#define CHAT_HISTORY_IDLE_FADE_TIME 2.5f
#define CHAT_HISTORY_ALPHA          64

/**
 * Maximum number of characters kept in the chat history (RichText) and in CHudChat's entry list.
 */
constexpr int CHAT_HISTORY_MAX_CHARS = 127 * 100;

/**
 * Maximum length of a chat input string in bytes that can be sent in one "say_team" command
 */
constexpr int MAX_CHAT_STRING_LEN = 101;

/**
 * Maximum number of wide chars that can be entered in the chat input.
 */
constexpr int MAX_CHAT_INPUT_STRING_LEN = 256;

/**
 * Maximum player name length (engine limit incl. terminator).
 */
constexpr int MAX_CHAT_PLAYER_NAME = 32;

/**
 * SBO: number of sent lines kept for Up/Down recall in the input line.
 */
constexpr int CHAT_INPUT_HISTORY_SIZE = 32;

/**
 * SBO: chat color picker (BugfixedHL-Rebased issue #98; hud_chat_color_picker.h). Collapsed to a circle in the
 * current color left of the prompt (CHudChatColorButton); a click opens the popup (CHudChatColorPicker) above the
 * input line: a color wheel with a brightness bar (any color) and a row of quick picks, "none" (the default
 * text color) and the ^1..^8 color codes (^0 and ^9 only reset to the default color).
 */
constexpr int CHAT_COLOR_PRESETS = 9;

/**
 * SBO: maximum length (bytes, without terminator) of a clickable chat link, both the
 * displayed text and the opened URL. Longer URLs are left as plain text.
 * Must stay below 511: RichText::OnClickPanel copies the clicked run into a wchar_t[512].
 */
constexpr int MAX_CHAT_URL_LEN = 255;

/**
 * SBO: true if url is safe to open from a chat link: http:// or https:// only,
 * printable ASCII without quotes/brackets/backslashes/spaces, 1..MAX_CHAT_URL_LEN bytes,
 * a non-empty host made of [A-Za-z0-9.-_:[]] (no "user@" part).
 */
bool IsSafeChatURL(const char *url);
//! Opens a URL IsSafeChatURL accepts as a chat link does: the Steam overlay browser, else the system browser
void HudChat_OpenURL(const char *url);

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string (BHL chat.h)
 */
inline wchar_t *CloneWString(const wchar_t *str)
{
	const int nLen = V_wcslen(str) + 1;
	wchar_t *cloneStr = new wchar_t[nLen];
	const int nSize = nLen * sizeof(wchar_t);
	V_wcsncpy(cloneStr, str, nSize);
	return cloneStr;
}

// Message mode types
enum
{
	MM_NONE = 0,
	MM_SAY,
	MM_SAY_TEAM,
};

//-----------------------------------------------------------------------------
enum TextColor
{
	COLOR_NORMAL = 1,
	COLOR_PLAYERNAME = 2,
	COLOR_PLAYERNAME_SOURCE = 3,
	COLOR_LOCATION = 4,
	COLOR_ACHIEVEMENT = 5,
	COLOR_CUSTOM = 6, // Will use the most recently SetCustomColor()
	COLOR_HEXCODE = 7, // Reads the color from the next six characters
	COLOR_HEXCODE_ALPHA = 8, // Reads the color and alpha from the next eight characters
	COLOR_MAX
};

//-----------------------------------------------------------------------------
// Color codes (BHL hud_colortext): ^0..^9, and SBO's ^xRGB and ^#RRGGBB
//-----------------------------------------------------------------------------
enum class ColorCodeAction
{
	Ignore = 0, //!< Color codes are not touched.
	Handle = 1, //!< Color codes change the color.
	Strip = 2, //!< Color codes don't change the color but are removed from the string.
};

/**
 * SBO: length of a "^xRGB" color code: '^', 'x' or 'X', then three hex digits, one per channel, each expanded like
 * CSS #RGB -> #RRGGBB (digit * 17): 4096 colors in 5 bytes. (BHL's "^N" codes are 2 bytes.)
 */
constexpr int COLOR_CODE_RGB_LEN = 5;

/**
 * SBO: length of a "^#RRGGBB" color code: '^', '#', then six hex digits, two per channel: any 24-bit color in 8
 * bytes. "^xRGB" is the compact form (it fits better in a name, which the engine limits to 31 bytes), "^#RRGGBB" the
 * exact one. COLOR_CODE_MAX_LEN: the longest code.
 */
constexpr int COLOR_CODE_RGB24_LEN = 8;
constexpr int COLOR_CODE_MAX_LEN = COLOR_CODE_RGB24_LEN;

/**
 * SBO: brightness floor of ^xRGB / ^#RRGGBB colors, as Rec. 601 luma (0.299 R + 0.587 G + 0.114 B) on the 0..255 scale. Darker
 * colors are unreadable on the chat / scoreboard / kill feed backgrounds: they are blended towards white until they
 * reach it (hue kept), only when drawn (ClampColorCodeBrightness) -- the text itself is never rewritten. The table
 * colors of ^1..^8 (chatscheme.res ColorCodeN) are drawn as configured.
 */
constexpr int COLOR_CODE_MIN_LUMA = 100;

/**
 * Value of a hex digit, -1 if c isn't one (the terminator included).
 */
template <typename CharT>
inline int ColorCodeHexDigit(CharT c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/**
 * Length of the color code at s (a terminated string): 2 for ^0..^9, COLOR_CODE_RGB_LEN for ^xRGB,
 * COLOR_CODE_RGB24_LEN for ^#RRGGBB, 0 if there is none. An incomplete or invalid sequence ("^x4A", "^xZZZ",
 * "^#44AAF", "^a") is no code: it stays literal text.
 */
template <typename CharT>
inline int GetColorCodeLength(const CharT *s)
{
	if (s[0] != '^')
		return 0;
	if (s[1] >= '0' && s[1] <= '9')
		return 2;
	if ((s[1] == 'x' || s[1] == 'X') && ColorCodeHexDigit(s[2]) >= 0 && ColorCodeHexDigit(s[3]) >= 0 && ColorCodeHexDigit(s[4]) >= 0)
		return COLOR_CODE_RGB_LEN;
	if (s[1] == '#')
	{
		for (int i = 2; i < COLOR_CODE_RGB24_LEN; i++)
		{
			if (ColorCodeHexDigit(s[i]) < 0)
				return 0;
		}
		return COLOR_CODE_RGB24_LEN;
	}
	return 0;
}

/**
 * True if s starts with a color code (^0..^9, ^xRGB or ^#RRGGBB).
 */
inline bool IsColorCode(const char *s)
{
	return GetColorCodeLength(s) > 0;
}

inline bool IsColorCode(const wchar_t *s)
{
	return GetColorCodeLength(s) > 0;
}

/**
 * SBO: the color of the ^xRGB / ^#RRGGBB code at s (GetColorCodeLength(s) == COLOR_CODE_RGB_LEN or
 * COLOR_CODE_RGB24_LEN), as written (not clamped).
 */
template <typename CharT>
inline Color GetColorCodeRGB(const CharT *s)
{
	if (s[1] == '#')
	{
		return Color(ColorCodeHexDigit(s[2]) * 16 + ColorCodeHexDigit(s[3]), ColorCodeHexDigit(s[4]) * 16 + ColorCodeHexDigit(s[5]),
		    ColorCodeHexDigit(s[6]) * 16 + ColorCodeHexDigit(s[7]), 255);
	}
	return Color(ColorCodeHexDigit(s[2]) * 17, ColorCodeHexDigit(s[3]) * 17, ColorCodeHexDigit(s[4]) * 17, 255);
}

/**
 * SBO: color raised to COLOR_CODE_MIN_LUMA (alpha kept); brighter colors are returned as they are.
 */
Color ClampColorCodeBrightness(Color color);

/**
 * SBO: a color picker selection (cvars hud_chat_color, namecolor): none, one of the ^1..^8 codes, or any 24-bit
 * color (^#RRGGBB; ^xRGB when both hex digits of every channel are the same).
 */
struct ChatColor
{
	int code = 0; //!< 0 = none (default text color), 1..8 = ^1..^8, CUSTOM = rgb
	int rgb = 0;  //!< CUSTOM: 0xRRGGBB

	enum { CUSTOM = -1 };

	bool IsNone() const { return code == 0; }
	bool IsCustom() const { return code == CUSTOM; }
	bool operator==(const ChatColor &other) const { return code == other.code && (code != CUSTOM || rgb == other.rgb); }
	bool operator!=(const ChatColor &other) const { return !(*this == other); }

	//! 1..8, anything else = none
	static ChatColor FromCode(int code)
	{
		ChatColor c;
		c.code = (code >= 1 && code < CHAT_COLOR_PRESETS) ? code : 0;
		return c;
	}

	static ChatColor FromRGB(Color color)
	{
		ChatColor c;
		c.code = CUSTOM;
		c.rgb = (color.r() << 16) | (color.g() << 8) | color.b();
		return c;
	}

	//! CUSTOM: the color as written (not clamped)
	Color GetRGB() const { return Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255); }
	//! CUSTOM: "^xRGB" writes it exactly (both hex digits of every channel are the same)
	bool IsCompact() const { return ((rgb >> 4) & 0x0F0F0F) == (rgb & 0x0F0F0F); }
};

/**
 * SBO: the setting the color picker popup edits (its "Chat" / "Name" switch).
 */
enum class ChatColorTarget
{
	Chat, //!< cvar hud_chat_color: the color of the player's chat text
	Name, //!< cvar namecolor: the color of the player's name
};

/**
 * SBO: the value of a color setting (cvars hud_chat_color and namecolor; a player's userinfo keys of those names):
 * "1".."8" = ^1..^8; "xRRGGBB" (six hex digits, e.g. "x40A0FF") = that color; "xRGB" (three hex digits, e.g.
 * "x4AF") = the color "x44AAFF"; '#' instead of 'x' and a leading '^' ("#40A0FF", "^x4AF", "^#40A0FF") are read too;
 * "0" and anything else (^0 and ^9 only reset to the default color) = none.
 */
ChatColor ParseChatColor(const char *value);

/**
 * SBO: the color code of a selection ("^N", "^xRGB" when that is exact, else "^#RRGGBB"; terminated) into out
 * (COLOR_CODE_MAX_LEN + 1 chars); nothing for "none". Returns its length.
 */
template <typename CharT>
inline int WriteColorCode(const ChatColor &color, CharT *out)
{
	static const char s_szHex[] = "0123456789ABCDEF";
	int n = 0;
	if (color.IsCustom())
	{
		// one hex digit per channel ("^xRGB") when that is the same color, else two ("^#RRGGBB")
		const bool bCompact = color.IsCompact();
		out[n++] = '^';
		out[n++] = bCompact ? 'x' : '#';
		for (int shift = 20; shift >= 0; shift -= 4)
		{
			if (!bCompact || (shift & 4))
				out[n++] = s_szHex[(color.rgb >> shift) & 15];
		}
	}
	else if (color.code >= 1 && color.code < CHAT_COLOR_PRESETS)
	{
		out[n++] = '^';
		out[n++] = (CharT)('0' + color.code);
	}
	out[n] = 0;
	return n;
}

/**
 * SBO: a player name with his name color (cvar namecolor) written as color codes, so every panel that draws the
 * codes of a name draws the name color too: the color's code, then the name with each ^0 / ^9 replaced by that code.
 * The name color is the color of the name's uncoded text; a code typed in the name still applies from where it is,
 * and a reset (^0 / ^9) in the name goes back to the name color. Nothing ends the color: what follows the name
 * starts with its own code (the scoreboard's "^0 (spectator)") or is drawn separately.
 * name: nameLen chars of a terminated string. Returns the length written to out (outCount chars, terminated);
 * what doesn't fit is left out. The server's UTIL_ChatPlayerName (dlls/util.cpp) does the same for the chat lines
 * it builds.
 */
template <typename CharT>
inline int ApplyNameColorCodes(const CharT *name, int nameLen, const ChatColor &color, CharT *out, int outCount)
{
	CharT code[COLOR_CODE_MAX_LEN + 1];
	const int codeLen = WriteColorCode(color, code);
	int n = 0;

	auto fnPut = [&](const CharT *s, int len) {
		if (n + len > outCount - 1)
			return false;
		for (int i = 0; i < len; i++)
			out[n++] = s[i];
		return true;
	};

	fnPut(code, codeLen);
	for (int i = 0; i < nameLen && name[i];)
	{
		int len = GetColorCodeLength(name + i);
		if (len > nameLen - i)
			len = 0;

		const bool bReset = len == 2 && (name[i + 1] == '0' || name[i + 1] == '9');
		if (len == 0)
			len = 1;
		if (!(bReset && codeLen > 0 && fnPut(code, codeLen)) && !fnPut(name + i, len))
			break;
		i += len;
	}

	out[n] = 0;
	return n;
}

/**
 * Removes ^0..^9, ^xRGB and ^#RRGGBB color codes. Returns a pointer to a static buffer.
 */
const char *RemoveColorCodes(const char *string);

/**
 * Removes \x01..\x08 color markup (incl. the hex digits of \x07/\x08) in place.
 */
char *RemoveColorMarkup(char *str);

//--------------------------------------------------------------------------------------------------------------
//! SBO: a hyperlink in a chat line (indices into the markup-free text, end exclusive)
struct ChatLink
{
	int start;
	int end;
	char url[MAX_CHAT_URL_LEN + 1]; //!< validated target (www. hosts get https://)
};

//--------------------------------------------------------------------------------------------------------------
struct TextRange
{
	TextRange() { preserveAlpha = false; }
	int start;
	int end;
	Color color;
	bool preserveAlpha;
};

//--------------------------------------------------------------------------------------------------------------
//! SBO: one message in the chat history, kept so the history can be rebuilt without the transient lines (the
//! server's hit messages: shown like chat, gone from the scrollback once they faded out). RichText can't remove
//! a range of its text, so CHudChat rebuilds it from these.
struct ChatHistorySegment
{
	int start; //!< indices into ChatHistoryEntry::text, end exclusive
	int end;
	Color color; //!< final color (alpha applied)
	int link; //!< index into ChatHistoryEntry::links, -1 = plain text
};

struct ChatHistoryEntry
{
	ChatHistoryEntry() { text = nullptr; flFadeEnd = flFadeLength = 0; bTransient = false; }
	~ChatHistoryEntry() { delete[] text; }

	wchar_t *text; //!< markup-free text
	CUtlVector<ChatHistorySegment> segments;
	CUtlVector<ChatLink> links;
	float flFadeEnd; //!< absolute time the line is fully faded out (RichText's fade "start" time)
	float flFadeLength;
	bool bTransient;

private:
	ChatHistoryEntry(const ChatHistoryEntry &); // not defined, not accessible
	ChatHistoryEntry &operator=(const ChatHistoryEntry &);
};

//-----------------------------------------------------------------------------
// Purpose: An output/display line of the chat interface
//-----------------------------------------------------------------------------
class CHudChatLine : public vgui2::RichText
{
	typedef vgui2::RichText BaseClass;

public:
	CHudChatLine(vgui2::Panel *parent, const char *panelName);
	~CHudChatLine();

	void SetExpireTime(void);

	bool IsReadyToExpire(void);

	void Expire(void);

	float GetStartTime(void);

	int GetCount(void);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	vgui2::HFont GetFont() { return m_hFont; }

	Color GetTextColor(void) { return m_clrText; }
	void SetNameLength(int iLength) { m_iNameLength = iLength; }
	void SetNameColor(Color cColor) { m_clrNameColor = cColor; }

	virtual void PerformFadeout(void);
	virtual void InsertAndColorizeText(wchar_t *buf, int clientIndex);
	virtual void Colorize(int alpha = 255); ///< Re-inserts the text in the appropriate colors at the given alpha

	//! SBO: finds http(s):// and www. links in m_text (fills m_links)
	void FindLinks();

	void SetNameStart(int iStart) { m_iNameStart = iStart; }
	//! SBO: the next line is a player's message from iStart on (index in the text given to InsertAndColorizeText;
	//! -1 = none), drawn in his chat color instead of the normal text color
	void SetMessageColor(int iStart, Color color)
	{
		m_iMessageStart = iStart;
		m_clrMessage = color;
	}
	//! SBO: the next line is transient (not kept in the history scrollback)
	void SetTransient(bool bTransient) { m_bTransient = bTransient; }

protected:
	int m_iNameLength;
	bool m_bTransient = false;
	vgui2::HFont m_hFont;

	Color m_clrText;
	Color m_clrNameColor;

	float m_flExpireTime;

	CUtlVector<TextRange> m_textRanges;
	CUtlVector<ChatLink> m_links; //!< SBO: links in m_text, sorted, non-overlapping
	wchar_t *m_text = nullptr;

	int m_iNameStart;
	int m_iMessageStart = -1; //!< SBO: SetMessageColor
	Color m_clrMessage;

private:
	float m_flStartTime;
	int m_nCount;

	vgui2::HFont m_hFontMarlett;

private:
	CHudChatLine(const CHudChatLine &); // not defined, not accessible
};

class CHudChatHistory : public vgui2::RichText
{
	DECLARE_CLASS_SIMPLE(CHudChatHistory, RichText);

public:
	CHudChatHistory(vgui2::Panel *pParent, const char *panelName);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override;
	virtual void OnThink() override;

	//! SBO: link click panels only take the mouse while the chat input is open
	void SyncLinkMouseInput();
	//! SBO: the chat font of hud_chat_fontsize (CHudChat::GetChatFont), links underlined
	void SetChatFonts(vgui2::IScheme *pScheme);

protected:
	//! SBO: replaces RichText's handler (which ShellExecutes whatever was clicked)
	virtual void OnTextClicked(const wchar_t *text) override;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CHudChat : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CHudChat, EditablePanel);

public:
	enum
	{
		CHAT_INTERFACE_LINES = 6,
		MAX_CHARS_PER_LINE = 128
	};

	CHudChat();
	~CHudChat();

	static CHudChat *Get() { return m_sInstance; }

	virtual void CreateChatInputLine(void);
	virtual void CreateChatLines(void);

	virtual void Init(void);

	virtual void InitHUDData();
	void LevelShutdown(void);

	virtual void Printf(const char *fmt, ...);
	virtual void ChatPrintf(int iPlayerIndex, const char *fmt, ...);
	//! SBO: ChatPrintf's body; bTransient: shown, but not kept in the scrollback and not printed to the console
	void PrintChatLine(int iPlayerIndex, const char *text, bool bTransient);

	//! SBO: history entries (CHudChatLine::Colorize). Takes ownership; inserts the entry into the RichText.
	void AddHistoryEntry(ChatHistoryEntry *pEntry);

	virtual void StartMessageMode(int iMessageModeType);
	virtual void StopMessageMode(void);
	void Send(void);

	MESSAGE_FUNC(OnChatEntrySend, "ChatEntrySend");
	MESSAGE_FUNC(OnChatEntryStopMessageMode, "ChatEntryStopMessageMode");

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	virtual void Paint(void);
	virtual void OnTick(void);
	virtual void Reset();
	vgui2::Panel *GetInputPanel(void);

	//! SBO: chat text sizes in pixels, hud_chat_fontsize CHAT_FONT_MIN..CHAT_FONT_MAX
	enum
	{
		CHAT_FONT_MIN = 14,
		CHAT_FONT_MAX = 34,
		CHAT_FONT_DEFAULT = 17, //!< the scheme's ChatFont
	};
	//! SBO: the chat font's height in pixels (hud_chat_fontsize, 14..34), the same at every resolution
	static int GetChatFontTall();
	//! SBO: the chat font of that height (bUnderline: for links). 17 px is the scheme's ChatFont /
	//! ChatFontUnderline; other heights are created once with the same face, weight and drop shadow.
	static vgui2::HFont GetChatFont(vgui2::IScheme *pScheme, bool bUnderline = false);

	static int m_nLineCounter;

	virtual int GetChatInputOffset(void);

	CHudChatHistory *GetChatHistory();

	void FadeChatHistory();
	float m_flHistoryFadeTime;
	float m_flHistoryIdleTime;

	CHudChatInputLine *GetChatInput(void) { return m_pChatInput; }

	//-----------------------------------------------------------------------------
	virtual Color GetDefaultTextColor(void);
	virtual Color GetTextColorForClient(TextColor colorNum, int clientIndex);
	virtual Color GetClientColor(int clientIndex);
	//! SBO: hyperlink color (chatscheme.res "ChatLinkColor")
	Color GetLinkColor(void) { return m_LinkColor; }

	int GetMessageMode(void) { return m_nMessageMode; }

	void SetCustomColor(Color colNew) { m_ColorCustom = colNew; }
	void SetCustomColor(const char *pszColorName);

	//! ^0..^9 / ^xRGB / ^#RRGGBB handling (cvar hud_colortext, 0..2)
	ColorCodeAction GetColorCodeAction();
	Color GetColorCodeColor(int code);

	//! SBO: the color of the color code at s (GetColorCodeLength(s) > 0), for every panel that draws player text:
	//! ^1..^8 = the scheme's ColorCodeN, ^xRGB / ^#RRGGBB = that color, raised to COLOR_CODE_MIN_LUMA. false = ^0 / ^9, the
	//! reset to the caller's default color (color is not touched).
	template <typename CharT>
	bool GetColorCodeColor(const CharT *s, Color &color)
	{
		if (s[1] >= '0' && s[1] <= '9')
		{
			const int idx = s[1] - '0';
			if (idx == 0 || idx == 9)
				return false;
			color = m_ColorCodeColors[idx];
			return true;
		}

		color = ClampColorCodeBrightness(GetColorCodeRGB(s));
		return true;
	}

	//! SBO: name colors. A player's name color is the "namecolor" key of his userinfo (client cvar namecolor,
	//! FCVAR_USERINFO, values as hud_chat_color), which the engine sends to every client; read from the engine when
	//! a name is drawn and validated here (ParseChatColor: anything invalid = none). None for an empty slot.
	static ChatColor GetPlayerNameColor(int clientIndex);
	//! SBO: chat colors. A player's chat color is the "hud_chat_color" key of his userinfo (client cvar
	//! hud_chat_color, FCVAR_USERINFO), sent to every client like the name color; read when one of his messages is
	//! printed and validated (ParseChatColor: no key, "0" or anything invalid = none). The message text never
	//! contains it. The local player's own: the cvar (no wait for the userinfo to come back from the server).
	ChatColor GetPlayerChatColor(int clientIndex);
	//! SBO: the name as the chat, scoreboard, kill feed, voice labels and status bar show it: with the player's name color
	//! written as color codes (ApplyNameColorCodes) while hud_colortext 1 draws codes; else (no name color,
	//! hud_colortext 0 or 2, no chat) the name as given. Four rotating static buffers.
	static const char *GetColoredPlayerName(int clientIndex, const char *name);

	//! SBO: the local player's chat color setting (color picker selection), cvar hud_chat_color (ParseChatColor).
	//! Setting it changes the userinfo, as SetNameColor (GetPlayerChatColor)
	ChatColor GetChatColor();
	void SetChatColor(const ChatColor &color);
	//! SBO: the local player's name color setting, cvar namecolor (ParseChatColor). Setting it changes the
	//! userinfo: the engine sends it to the server, which sends it to every client (GetPlayerNameColor).
	ChatColor GetNameColor();
	void SetNameColor(const ChatColor &color);
	//! SBO: the chat color to show (collapsed circle, typed text): the color being picked while the wheel /
	//! brightness bar is dragged for the chat color, else GetChatColor
	ChatColor GetActiveChatColor();
	//! SBO: the color being picked (wheel / brightness bar drag) for the setting the popup edits
	void SetChatColorPreview(const ChatColor &color);
	void ClearChatColorPreview();
	//! SBO: the setting the popup edits: the chat color each time it opens, the name color after a click on "Name"
	ChatColorTarget GetColorPickerTarget() const { return m_ColorPickerTarget; }
	void SetColorPickerTarget(ChatColorTarget target);
	//! SBO: the popup's selection: the color being picked, else the edited setting's (GetChatColor / GetNameColor)
	ChatColor GetPickerColor();
	//! SBO: selects a color for the edited setting (SetChatColor / SetNameColor)
	void SetPickerColor(const ChatColor &color);
	//! SBO: the color a selection is drawn in (as a "^N" / "^xRGB" / "^#RRGGBB" code in a chat line is: a wheel color no darker than
	//! COLOR_CODE_MIN_LUMA); the default text color for "none"
	static Color GetChatColorDisplay(const ChatColor &color);
	//! SBO: the color the input line shows its text in while typing: the chat color the message will be drawn in
	//! (GetActiveChatColor) while hud_colortext 1 draws chat colors; false = the normal input text color (no
	//! color selected, hud_colortext 0 or 2). Codes typed in the text are not drawn in the input line.
	bool GetChatColorPreview(Color &color);
	//! SBO: tooltip text of a selection: "None (default color)", "<name> (^N)" with the name taken from the
	//! configured ColorCodeN color, or the "^#RRGGBB" / "^xRGB" code
	void GetChatColorName(const ChatColor &color, wchar_t *buf, int bufSizeBytes);

	//! SBO: color picker popup (only while the input line is open; never takes the keyboard)
	void OpenColorPicker();
	void CloseColorPicker();
	void ToggleColorPicker();
	bool IsColorPickerOpen();
	void ShowColorTooltip(vgui2::Panel *pOwner, const wchar_t *text);
	void HideColorTooltip(vgui2::Panel *pOwner);

	//! SBO: input line history (Up = dir +1 older, Down = dir -1 newer)
	void AddToInputHistory(const wchar_t *text);
	void BrowseInputHistory(int dir);
	//! SBO: page the history up (+1) / down (-1)
	void ScrollHistory(int dir);

	//! SayText user message (hooked in Init)
	int MsgFunc_SayText(const char *pszName, int iSize, void *pbuf);

protected:
	CHudChatLine *FindUnusedChatLine(void);

	CHudChatInputLine *m_pChatInput = nullptr;
	CHudChatLine *m_ChatLine = nullptr;
	int m_iFontHeight;

	CHudChatHistory *m_pChatHistory = nullptr;

	Color m_ColorCustom;
	Color m_ColorCodeColors[10];

private:
	void Clear(void);

	int ComputeBreakChar(int width, const char *text, int textlen);

	// SBO: history entries (transient lines)
	void InsertHistoryEntry(const ChatHistoryEntry &entry);
	//! Removes the transient entries (all, or only those that faded out) and rebuilds the RichText if any went.
	void PurgeTransientHistory(bool bAll);
	void RebuildHistory();

	CUtlVector<ChatHistoryEntry *> m_HistoryEntries; //!< oldest first
	int m_nHistoryChars = 0;
	int m_nTransientEntries = 0;

	int m_nMessageMode;

	int m_nVisibleHeight;

	vgui2::HFont m_hChatFont;
	int m_iAppliedChatFontTall = -1; //!< SBO: GetChatFontTall the history and the input line use (OnTick)
	Color m_DefTextColor;
	Color m_ConsoleMsgColor;
	Color m_LinkColor;
	const char *m_pszChatSoundPath = nullptr;

	static CHudChat *m_sInstance;

	// SBO: input line history, [0] = most recent
	CUtlVector<wchar_t *> m_InputHistory;
	int m_iInputHistoryPos = -1; //!< -1 = not browsing (draft shown)
	wchar_t m_szInputDraft[MAX_CHAT_INPUT_STRING_LEN + 1];

	// SBO: color picker popup (children of the chat panel, above the history)
	void LayoutColorPicker();
	//! true if Send would add the picker color's prefix to this message (not blank, no chat command, no typed code)
	CHudChatColorPicker *m_pColorPicker = nullptr;
	CHudChatColorTooltip *m_pColorTooltip = nullptr;
	vgui2::Panel *m_pColorTooltipOwner = nullptr; //!< panel the tooltip is shown for
	bool m_bPickerMouseDown = false;              //!< mouse button state last tick (outside-click detection)
	bool m_bChatColorPreview = false;             //!< a color is being picked (wheel / brightness bar drag)
	ChatColor m_ChatColorPreview;
	ChatColorTarget m_ColorPickerTarget = ChatColorTarget::Chat; //!< the setting the popup edits
	sbo::CvarHandle m_hNameColorCvar = nullptr;   //!< "namecolor" (registered by the HUD, hud.cpp)

public:
	// memory handling, uses calloc so members are zero'd out on instantiation
	void *operator new(size_t stAllocateBlock)
	{
		Assert(stAllocateBlock != 0);
		void *pMem = ::operator new(stAllocateBlock);
		memset(pMem, 0, stAllocateBlock);
		return pMem;
	}

	void operator delete(void *pMem)
	{
		::operator delete(pMem);
	}
};

class CHudChatEntry : public vgui2::TextEntry
{
	typedef vgui2::TextEntry BaseClass;

public:
	CHudChatEntry(vgui2::Panel *parent, char const *panelName, vgui2::Panel *pChat)
	    : BaseClass(parent, panelName)
	{
		SetCatchEnterKey(true);
		SetAllowNonAsciiCharacters(true);
		SetDrawLanguageIDAtLeft(true);
		m_pHudChat = pChat;
	}

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme)
	{
		BaseClass::ApplySchemeSettings(pScheme);

		SetPaintBorderEnabled(false);
		m_NormalTextColor = GetFgColor(); // TextEntry.TextColor
	}

	//! SBO: the text colour without a picker colour (the scheme's; the picker preview overrides it)
	void SetNormalTextColor(Color color)
	{
		m_NormalTextColor = color;
		SetFgColor(color);
	}

	virtual void OnThink();
	virtual void OnKeyCodeTyped(vgui2::KeyCode code);
	virtual void OnMouseWheeled(int delta);

private:
	vgui2::Panel *m_pHudChat = nullptr;
	Color m_NormalTextColor;
};

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
class CHudChatInputLine : public vgui2::Panel
{
	typedef vgui2::Panel BaseClass;

public:
	CHudChatInputLine(vgui2::Panel *parent, char const *panelName);

	void SetPrompt(const wchar_t *prompt);
	void ClearEntry(void);
	void SetEntry(const wchar_t *entry);
	void GetMessageText(wchar_t *buffer, int buffersizebytes);
	void GotoEntryEnd();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	vgui2::Panel *GetInputPanel(void);
	virtual vgui2::VPANEL GetCurrentKeyFocus() { return m_pInput->GetVPanel(); }

	virtual void Paint()
	{
		BaseClass::Paint();
	}

	vgui2::Label *GetPrompt(void) { return m_pPrompt; }
	//! SBO: the prompt and the text entry in the chat font (CHudChat::GetChatFont)
	void SetChatFont(vgui2::HFont hFont);
	//! SBO: the collapsed color picker (a circle in the current color), left of the prompt
	vgui2::Panel *GetColorButton(void);

protected:
	vgui2::Label *m_pPrompt = nullptr;
	CHudChatEntry *m_pInput = nullptr;
	CHudChatColorButton *m_pColorButton = nullptr; //!< SBO: color picker, left of the prompt
};

#endif // SBO_VGUI2_HUD_CHAT_H
