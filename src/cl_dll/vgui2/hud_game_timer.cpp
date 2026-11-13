//========= SBO VGUI2: game_timer HUD timer =========//
//
// Purpose: see hud_game_timer.h. The functions follow SBO Source's hud_generic_timer.cpp (named in the comments).
//
// Source's HudAnimations.txt events for this element, played here as the script writes them (docs/GAME_TIMER.md
// "Animations"):
//   GenericGameTimerShow       Alpha -> 220, FgColor / BgColor -> the scheme's, linear over 0.15 s (other
//                              animations of the panel stopped)
//   GenericGameTimerShowFlash  Show + Pulse (a timer that runs when it appears)
//   GenericGameTimerClose      Alpha -> 0, linear over 0.25 s; the element goes away after one second
//   GenericGameTimerPulse      Blur -> 6 at once, then -> 0 over 0.5 s ("Deaccel": the square root of the progress),
//                              starting 0.01 s later. In Source, Blur = the digits drawn that many more times with
//                              the additive font. SBO (unlike the script): the pulse flashes the
//                              WHOLE element -- bar, caption and digits jump towards white, the state's colour is
//                              laid over the box behind them -- and settles on the same curve.
//                              The extra passes were too weak to see on this engine.
//   GenericGameTimerPulseOnce  Pulse
//   GenericGameTimerWarn       FgColor -> 255 70 70, linear over 0.1 s, + Pulse
//   GenericGameTimerUnwarn     FgColor -> the scheme's, linear over 0.1 s
// Source's OnThink asks for Warn on every frame of the warning state and for PulseOnce on every frame of a pause.
// Its animation controller restarts a sequence that is asked for again, so there neither pulse ever gets going
// (SBO's first port copied that: no flash). Here a sequence plays to its end and a state that keeps asking for it
// gets it again when it has ended: the digits flash about twice a second for as long as the timer warns or is
// paused (gametimer.res "LoopWarnPulse" / "LoopPausePulse" 0: one flash when the state begins).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "hud_game_timer.h"
#include "sbo_bridge.h"
#include "sbo_menus.h"
#include "sbo_spectator_panel.h"
#include "vgui2_api.h"

#define GAMETIMER_RES VGUI2_ROOT_DIR "resource/gametimer.res"

namespace sbo
{
// sbo_game_timer_bridge.cpp (SBO world): the HUD's weapon selection is open (CHudAmmo, gpActiveSel)
bool IsWeaponSelectionOpen();
} // namespace sbo

namespace
{
// Source's HudAnimations.txt (see the top of the file)
constexpr float SHOWN_ALPHA = 220.0f;
constexpr float SHOW_TIME = 0.15f;
constexpr float CLOSE_TIME = 0.25f;
constexpr float SHUTOFF_TIME = 1.0f; // hud_generic_timer.cpp: the element stays this long after its timer went
constexpr float PULSE_BLUR = 6.0f;
constexpr float PULSE_DELAY = 0.01f;
constexpr float WARN_TIME = 0.1f;

// hud_generic_timer.cpp Paint: the default place, as a part of the free screen (override: the entity's x / y)
constexpr float DEFAULT_X = 0.5f;
constexpr float DEFAULT_Y = 0.05f;

// caption_width is in the pixels of a screen this high
constexpr float CAPTION_WIDTH_BASE_TALL = 1080.0f;

struct MsgReader
{
	const unsigned char *data;
	int size;
	int pos;

	bool HasBytes(int n) const { return pos + n <= size; }

	int ReadByte()
	{
		if (!HasBytes(1))
		{
			pos = size + 1;
			return 0;
		}
		return data[pos++];
	}

	int ReadShort()
	{
		if (!HasBytes(2))
		{
			pos = size + 1;
			return 0;
		}
		short value = (short)(data[pos] | (data[pos + 1] << 8));
		pos += 2;
		return value;
	}

	float ReadFloat()
	{
		if (!HasBytes(4))
		{
			pos = size + 1;
			return 0.0f;
		}
		float value;
		memcpy(&value, data + pos, sizeof(value));
		pos += 4;
		return value;
	}

	void ReadString(char *buf, int bufSize)
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
	}
};

float ScreenScalar(int value)
{
	// server: -32768 = Source's -1 (the default place), else the part of the screen * 10000
	return value == -32768 ? -1.0f : value / 10000.0f;
}

int MsgFunc_GameTimer(const char *pszName, int iSize, void *pbuf)
{
	if (CHudGameTimer::Get())
		CHudGameTimer::Get()->OnMessage(static_cast<const unsigned char *>(pbuf), iSize);
	return 1;
}
} // namespace

CHudGameTimer *CHudGameTimer::s_pInstance = nullptr;

CHudGameTimer *CHudGameTimer::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CHudGameTimer(pParent);
	return s_pInstance;
}

CHudGameTimer::CHudGameTimer(vgui2::Panel *pParent)
    : BaseClass(pParent, "HudGenericGameTimer")
{
	s_pInstance = this;

	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetProportional(false); // fonts are chosen in ApplySchemeSettings (hud_scale), like the HUD text menu

	if (sboui::GetScheme())
		SetScheme(sboui::GetScheme());

	LoadLayout();

	SetBounds(0, 0, 640, 480); // OnThink: the parent's (screen) size
	SetVisible(true);          // always: VGUI2 doesn't think invisible panels; nothing is painted without a timer
}

void CHudGameTimer::LoadLayout()
{
	if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(GAMETIMER_RES))
	{
		sbo::DevPrint("Game timer: " GAMETIMER_RES " not found, using defaults\n");
		return;
	}

	KeyValues *pFile = new KeyValues("gametimer.res");
	if (pFile->LoadFromFile(g_pFullFileSystem, GAMETIMER_RES))
	{
		KeyValues *pSettings = pFile->FindKey(GetName());
		if (pSettings)
		{
			m_flMinWidth = pSettings->GetFloat("MinWidth", m_flMinWidth);
			m_flBorder = pSettings->GetFloat("Border", m_flBorder);
			m_flLabelTimerSpacing = pSettings->GetFloat("LabelTimerSpacing", m_flLabelTimerSpacing);
			m_flBarHeight = pSettings->GetFloat("BarHeight", m_flBarHeight);
			m_flBarChunkGap = pSettings->GetFloat("BarChunkGap", m_flBarChunkGap);
			m_flBarVerticalGap = pSettings->GetFloat("BarVerticalGap", m_flBarVerticalGap);
			m_iBarDisabledAlpha = pSettings->GetInt("BarDisabledAlpha", m_iBarDisabledAlpha);
			m_iMaxBars = pSettings->GetInt("MaxBars", m_iMaxBars);
			m_flSpectatorTop = pSettings->GetFloat("SpectatorTop", m_flSpectatorTop);
			Q_strncpy(m_szNumberFontName, pSettings->GetString("NumberFontName", m_szNumberFontName),
			    sizeof(m_szNumberFontName));
			m_flNumberFontTall = pSettings->GetFloat("NumberFontTall", m_flNumberFontTall);
			m_iNumberFontWeight = pSettings->GetInt("NumberFontWeight", m_iNumberFontWeight);
			Q_strncpy(m_szTextFontName, pSettings->GetString("TextFontName", m_szTextFontName),
			    sizeof(m_szTextFontName));
			m_flTextFontTall = pSettings->GetFloat("TextFontTall", m_flTextFontTall);
			m_iTextFontWeight = pSettings->GetInt("TextFontWeight", m_iTextFontWeight);
			m_bFontAdditive = pSettings->GetInt("FontAdditive", m_bFontAdditive ? 1 : 0) != 0;
			m_bLoopWarnPulse = pSettings->GetInt("LoopWarnPulse", m_bLoopWarnPulse ? 1 : 0) != 0;
			m_bLoopPausePulse = pSettings->GetInt("LoopPausePulse", m_bLoopPausePulse ? 1 : 0) != 0;
			m_flPulseTime = pSettings->GetFloat("PulseTime", m_flPulseTime);
			m_flPulseFgStrength = pSettings->GetFloat("PulseFgStrength", m_flPulseFgStrength);
			m_iPulseBgAlpha = pSettings->GetInt("PulseBgAlpha", m_iPulseBgAlpha);
			if (m_flPulseTime < 0.05f)
				m_flPulseTime = 0.05f;
			if (m_flPulseFgStrength < 0.0f)
				m_flPulseFgStrength = 0.0f;
			if (m_flPulseFgStrength > 1.0f)
				m_flPulseFgStrength = 1.0f;
			if (m_iPulseBgAlpha < 0)
				m_iPulseBgAlpha = 0;
			if (m_iPulseBgAlpha > 255)
				m_iPulseBgAlpha = 255;
		}
	}
	pFile->deleteThis();
}

// 640x480 units to pixels, Source's proportional scaling: times screen height / 480, cut to whole pixels
int CHudGameTimer::Scaled(float value) const
{
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);
	return (int)(value * screenTall / 480.0f);
}

// The fonts at the screen's size. A scheme font with a fixed "tall" doesn't grow with the screen (the scheme's
// "Default" has one size per resolution range), so they are built here, as sboui's text scheme fonts are
// (sbo_menus.cpp).
void CHudGameTimer::CreateFonts()
{
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);
	if (screenTall == m_iFontScreenTall && m_hNumberFont && m_hTextFont)
		return;

	m_iFontScreenTall = screenTall;

	int flags = vgui2::ISurface::FONTFLAG_ANTIALIAS;
	if (m_bFontAdditive)
		flags |= vgui2::ISurface::FONTFLAG_ADDITIVE;

	int numberTall = Scaled(m_flNumberFontTall);
	int textTall = Scaled(m_flTextFontTall);
	if (numberTall < 8)
		numberTall = 8;
	if (textTall < 8)
		textTall = 8;

	m_hNumberFont = vgui2::surface()->CreateFont();
	vgui2::surface()->AddGlyphSetToFont(m_hNumberFont, m_szNumberFontName, numberTall, m_iNumberFontWeight, 0, 0,
	    flags, 0, 0);

	m_hTextFont = vgui2::surface()->CreateFont();
	vgui2::surface()->AddGlyphSetToFont(m_hTextFont, m_szTextFontName, textTall, m_iTextFontWeight, 0, 0, flags, 0,
	    0);
}

void CHudGameTimer::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0, 0, 0, 0)); // the box is drawn in PaintBackground

	CreateFonts();

	// Source: the scheme's FgColor / BgColor (SBO's box: a dark green, 16 48 20 at alpha 110; Source's was 47 145 60 at
	// 15); the warning colour is HudAnimations.txt's
	m_SchemeFgColor = pScheme->GetColor("GameTimer.FgColor", Color(47, 145, 60, 255));
	m_SchemeBgColor = pScheme->GetColor("GameTimer.BgColor", Color(16, 48, 20, 110));
	m_WarnColor = pScheme->GetColor("GameTimer.WarnColor", Color(255, 70, 70, 255));

	// the first time: the colour at rest. Later (new fonts for another screen size, a reloaded scheme) the
	// animations go on from where they are.
	if (!m_bColorsSet)
	{
		for (int i = 0; i < 4; i++)
			m_FgColor[i] = m_SchemeFgColor[i];
		m_bColorsSet = true;
	}

	// Source: start with a 0:00 timer
	m_nTimerWidth = vgui2::surface()->GetCharacterWidth(m_hNumberFont, '0') * 3;
	m_nTimerWidth += vgui2::surface()->GetCharacterWidth(m_hNumberFont, ':');
	m_nTimerHeight = vgui2::surface()->GetFontTall(m_hNumberFont);

	// the font changed: wrap the caption again
	LayoutLabelText();
}

//-----------------------------------------------------------------------------
// The server's timers
//-----------------------------------------------------------------------------
GameTimerState *CHudGameTimer::FindState(int id)
{
	if (id <= 0)
		return nullptr;

	for (GameTimerState &timer : m_Timers)
	{
		if (timer.id == id)
			return &timer;
	}
	return nullptr;
}

GameTimerState *CHudGameTimer::AddState(int id)
{
	for (GameTimerState &timer : m_Timers)
	{
		if (!timer.id)
		{
			timer = GameTimerState();
			timer.id = id;
			return &timer;
		}
	}
	return nullptr;
}

// Source: C_GameTimer::GetTimeRemaining. The client's clock (the HUD time) follows the server's.
float CHudGameTimer::GetTimeRemaining(const GameTimerState &timer) const
{
	float remaining = timer.paused ? timer.time : timer.time - sbo::GetHudTime();
	return remaining < 0.0f ? 0.0f : remaining;
}

// GameTimer (server: src/dlls/game_timer.cpp SendState): short id, byte flags; not ENABLED: the end. float time;
// FULL: float length, float warn time, short progress_bar_max, short progress_bar_override, short x, short y,
// short caption_width, byte caption_lines, string caption.
void CHudGameTimer::OnMessage(const unsigned char *data, int size)
{
	MsgReader msg = { data, size, 0 };

	const int id = msg.ReadShort();
	const int flags = msg.ReadByte();
	if (msg.pos > msg.size || id <= 0)
		return;

	GameTimerState *pTimer = FindState(id);

	if (!(flags & GAME_TIMER_FLAG_ENABLED))
	{
		// disabled (or no longer shown in the HUD): Source's element sees IsDisabled and closes
		if (pTimer)
			pTimer->id = 0;
		return;
	}

	if (!pTimer)
	{
		// an update for a timer we never got in full: wait for its full state (sent with the HUD initialisation)
		if (!(flags & GAME_TIMER_FLAG_FULL))
			return;

		pTimer = AddState(id);
		if (!pTimer)
			return;
	}

	pTimer->paused = (flags & GAME_TIMER_FLAG_PAUSED) != 0;
	pTimer->showTimeRemaining = (flags & GAME_TIMER_FLAG_SHOW_REMAINING) != 0;
	pTimer->time = msg.ReadFloat();

	if (flags & GAME_TIMER_FLAG_FULL)
	{
		pTimer->maxLength = msg.ReadFloat();
		pTimer->warnTime = msg.ReadFloat();
		pTimer->progressBarMax = msg.ReadShort();
		pTimer->progressBarOverride = msg.ReadShort();
		pTimer->x = ScreenScalar(msg.ReadShort());
		pTimer->y = ScreenScalar(msg.ReadShort());
		pTimer->captionWidth = msg.ReadShort();
		pTimer->captionMaxLines = msg.ReadByte();
		msg.ReadString(pTimer->caption, sizeof(pTimer->caption));
	}
}

// Source: Reset (Init / VidInit)
void CHudGameTimer::Reset()
{
	for (GameTimerState &timer : m_Timers)
		timer.id = 0;

	m_iValue = 0;
	m_flAlpha = 0.0f;
	m_bAlphaAnim = false;
	m_bFgAnim = false;
	m_flBlur = 0.0f;
	m_bBlurAnim = false;
	for (int i = 0; i < 4; i++)
		m_FgColor[i] = m_SchemeFgColor[i];

	m_flShutoffTime = 0.0f;
	m_bTimerDisplayed = false;
	m_bTimerPaused = false;
	m_bTimerWarned = false;
	m_iTimerIndex = 0;
	m_szCurrentTimerCaption[0] = '\0';
	m_LabelText[0] = L'\0';
	m_nLabelLineCount = 0;
	m_nLabelWidth = 0;
	m_nLabelHeight = 0;
}

//-----------------------------------------------------------------------------
// Animated values
//-----------------------------------------------------------------------------
// Source: GenericGameTimerShow / GenericGameTimerShowFlash
void CHudGameTimer::StartShow(float now, bool flash)
{
	// StopPanelAnimations, then Alpha and FgColor from where they are
	m_bBlurAnim = false;

	m_flAlphaFrom = m_flAlpha;
	m_flAlphaTo = SHOWN_ALPHA;
	m_flAlphaStart = now;
	m_flAlphaDuration = SHOW_TIME;
	m_bAlphaAnim = true;

	for (int i = 0; i < 4; i++)
	{
		m_FgFrom[i] = m_FgColor[i];
		m_FgTo[i] = m_SchemeFgColor[i];
	}
	m_flFgStart = now;
	m_flFgDuration = SHOW_TIME;
	m_bFgAnim = true;

	if (flash)
		StartPulse(now);
}

// Source: GenericGameTimerPulse
void CHudGameTimer::StartPulse(float now)
{
	m_flBlur = PULSE_BLUR;
	m_flBlurStart = now + PULSE_DELAY;
	m_bBlurAnim = true;
}

void CHudGameTimer::UpdateAnimations(float now, float frameTime)
{
	if (m_bAlphaAnim)
	{
		float pos = m_flAlphaDuration > 0.0f ? (now - m_flAlphaStart) / m_flAlphaDuration : 1.0f;
		if (pos >= 1.0f)
		{
			pos = 1.0f;
			m_bAlphaAnim = false;
		}
		else if (pos < 0.0f)
			pos = 0.0f;
		m_flAlpha = m_flAlphaFrom + (m_flAlphaTo - m_flAlphaFrom) * pos;
	}

	if (m_bFgAnim)
	{
		float pos = m_flFgDuration > 0.0f ? (now - m_flFgStart) / m_flFgDuration : 1.0f;
		if (pos >= 1.0f)
		{
			pos = 1.0f;
			m_bFgAnim = false;
		}
		else if (pos < 0.0f)
			pos = 0.0f;
		for (int i = 0; i < 4; i++)
			m_FgColor[i] = m_FgFrom[i] + (m_FgTo[i] - m_FgFrom[i]) * pos;
	}

	if (m_bBlurAnim && now >= m_flBlurStart)
	{
		float pos = m_flPulseTime > 0.0f ? (now - m_flBlurStart) / m_flPulseTime : 1.0f;
		if (pos >= 1.0f)
		{
			m_flBlur = 0.0f;
			m_bBlurAnim = false;
		}
		else
			m_flBlur = PULSE_BLUR * (1.0f - sqrtf(pos)); // "Deaccel"
	}
}

//-----------------------------------------------------------------------------
// Source: ShouldDraw + OnThink
//-----------------------------------------------------------------------------
void CHudGameTimer::OnThink()
{
	BaseClass::OnThink();

	// cover the parent (the full-screen client viewport)
	int wide, tall, parentWide, parentTall;
	GetSize(wide, tall);
	g_pVGuiPanel->GetSize(GetVParent(), parentWide, parentTall);
	if (wide != parentWide || tall != parentTall)
		SetBounds(0, 0, parentWide, parentTall);

	// the screen's height changed: fonts for the new size
	if (parentTall != m_iFontScreenTall && parentTall > 0)
		InvalidateLayout(false, true);

	// the animations run in real time (the HUD time jumps when a map starts)
	const double realTime = vgui2::system()->GetCurrentTime();
	float frameTime = 0.0f;
	if (m_flLastThinkTime > 0 && realTime > m_flLastThinkTime)
		frameTime = (float)(realTime - m_flLastThinkTime);
	m_flLastThinkTime = realTime;
	// seconds since the panel's first frame: small numbers, so a float keeps its milliseconds
	if (m_flTimeBase <= 0)
		m_flTimeBase = realTime;
	const float now = (float)(realTime - m_flTimeBase);

	// Source: ShouldDraw
	if (!m_bTimerDisplayed)
	{
		// check if we should find a new timer
		if (!FindTimer())
			return;

		m_flShutoffTime = 0.0f;
		m_bTimerDisplayed = true;
		m_bTimerWarned = false;
		StartShow(now, !m_bTimerPaused);
	}
	else if (m_flShutoffTime > 0.0f && m_flShutoffTime <= now)
	{
		// time's up, shut off (the next frame looks for another timer)
		m_bTimerDisplayed = false;
		m_flAlpha = 0.0f;
		m_bAlphaAnim = false;
		return;
	}

	ThinkTimer(now, frameTime);
	UpdateAnimations(now, frameTime);
}

// Source: FindTimer. The first enabled timer in the map's entity order (Source: the first in the client's list of
// timers, which is their order of creation).
bool CHudGameTimer::FindTimer()
{
	const GameTimerState *pFound = nullptr;
	for (const GameTimerState &timer : m_Timers)
	{
		if (timer.id && (!pFound || timer.id < pFound->id))
			pFound = &timer;
	}

	if (!pFound)
		return false;

	m_iTimerIndex = pFound->id;

	// new timer selected, set the caption
	UpdateTimerLabel(*pFound);

	m_bTimerPaused = pFound->paused;
	m_iValue = (int)ceilf(GetTimeRemaining(*pFound));
	return true;
}

// Source: OnThink
void CHudGameTimer::ThinkTimer(float now, float frameTime)
{
	// if our timer has been disabled, close the menu
	const GameTimerState *pTimer = FindState(m_iTimerIndex);
	if (!pTimer)
	{
		if (m_flShutoffTime == 0.0f)
		{
			m_flShutoffTime = now + SHUTOFF_TIME;

			// GenericGameTimerClose
			m_flAlphaFrom = m_flAlpha;
			m_flAlphaTo = 0.0f;
			m_flAlphaStart = now;
			m_flAlphaDuration = CLOSE_TIME;
			m_bAlphaAnim = true;
		}
		m_iTimerIndex = 0;
		return;
	}

	// check pause state
	if (m_bTimerPaused && !pTimer->paused)
	{
		m_bTimerPaused = false;
		StartPulse(now);
	}
	else if (pTimer->paused)
	{
		// GenericGameTimerPulseOnce: when the pause begins, and again whenever it has ended while the pause lasts
		if (!m_bTimerPaused || (m_bLoopPausePulse && !m_bBlurAnim))
			StartPulse(now);
		m_bTimerPaused = true;
	}

	const float flTimeRemaining = floorf(GetTimeRemaining(*pTimer));

	// check warn state
	const float flWarnTime = pTimer->warnTime;
	if (flWarnTime > 0.0f)
	{
		if (m_bTimerWarned && flTimeRemaining > flWarnTime)
		{
			// turn back to normal: GenericGameTimerUnwarn
			m_bTimerWarned = false;
			for (int i = 0; i < 4; i++)
			{
				m_FgFrom[i] = m_FgColor[i];
				m_FgTo[i] = m_SchemeFgColor[i];
			}
			m_flFgStart = now;
			m_flFgDuration = WARN_TIME;
			m_bFgAnim = true;
		}
		else if (flTimeRemaining <= flWarnTime)
		{
			// turn red: GenericGameTimerWarn. The colour once, when the state begins; its pulse then and again
			// whenever it has ended while the state lasts.
			if (!m_bTimerWarned)
			{
				for (int i = 0; i < 4; i++)
				{
					m_FgFrom[i] = m_FgColor[i];
					m_FgTo[i] = m_WarnColor[i];
				}
				m_flFgStart = now;
				m_flFgDuration = WARN_TIME;
				m_bFgAnim = true;
				StartPulse(now);
			}
			else if (m_bLoopWarnPulse && !m_bBlurAnim)
				StartPulse(now);

			m_bTimerWarned = true;
		}
	}

	m_iValue = (int)flTimeRemaining;

	m_iTimerMaxLength = (int)pTimer->maxLength;
	m_bShowTimeRemaining = pTimer->showTimeRemaining;

	m_nProgressBarMax = pTimer->progressBarMax;
	if (m_nProgressBarMax == -1)
	{
		// default to timer max length
		m_nProgressBarMax = m_iTimerMaxLength < m_iMaxBars ? m_iTimerMaxLength : m_iMaxBars;
	}

	m_nProgressBarOverride = pTimer->progressBarOverride;

	m_flOverrideX = pTimer->x;
	m_flOverrideY = pTimer->y;

	int captionWidth = pTimer->captionWidth < 0 ? 0 : pTimer->captionWidth;
	int captionMaxLines = pTimer->captionMaxLines;
	if (captionMaxLines < 1)
		captionMaxLines = 1;
	if (captionMaxLines > GAME_TIMER_MAX_CAPTION_LINES)
		captionMaxLines = GAME_TIMER_MAX_CAPTION_LINES;

	if (strcmp(m_szCurrentTimerCaption, pTimer->caption) || m_nCaptionWidth != captionWidth
	    || m_nCaptionMaxLines != captionMaxLines)
	{
		UpdateTimerLabel(*pTimer);
	}
}

//-----------------------------------------------------------------------------
// Caption. Source: UpdateTimerLabel, SetLabelText, AddLabelLine, LayoutLabelText
//-----------------------------------------------------------------------------
void CHudGameTimer::UpdateTimerLabel(const GameTimerState &timer)
{
	Q_strncpy(m_szCurrentTimerCaption, timer.caption, sizeof(m_szCurrentTimerCaption));
	m_nCaptionWidth = timer.captionWidth < 0 ? 0 : timer.captionWidth;
	m_nCaptionMaxLines = timer.captionMaxLines;
	if (m_nCaptionMaxLines < 1)
		m_nCaptionMaxLines = 1;
	if (m_nCaptionMaxLines > GAME_TIMER_MAX_CAPTION_LINES)
		m_nCaptionMaxLines = GAME_TIMER_MAX_CAPTION_LINES;

	// "#Token": localized (Source looks every caption up); else the text itself
	wchar_t wszLabelText[GAME_TIMER_MAX_CAPTION];
	const wchar_t *wLocalizedItem = nullptr;
	if (timer.caption[0] == '#')
	{
		wLocalizedItem = g_pVGuiLocalize->Find(timer.caption + 1);
		if (!wLocalizedItem)
			wLocalizedItem = g_pVGuiLocalize->Find(timer.caption);
	}

	if (wLocalizedItem)
	{
		wcsncpy(wszLabelText, wLocalizedItem, GAME_TIMER_MAX_CAPTION - 1);
		wszLabelText[GAME_TIMER_MAX_CAPTION - 1] = L'\0';
	}
	else
		sboui::TextToWide(timer.caption, wszLabelText, sizeof(wszLabelText));

	// SetLabelText: a literal backslash-n is a new line
	int nDst = 0;
	for (int nSrc = 0; wszLabelText[nSrc] != L'\0' && nDst < GAME_TIMER_MAX_CAPTION - 1; ++nSrc)
	{
		if (wszLabelText[nSrc] == L'\\' && (wszLabelText[nSrc + 1] == L'n' || wszLabelText[nSrc + 1] == L'N'))
		{
			m_LabelText[nDst++] = L'\n';
			++nSrc;
			continue;
		}

		if (wszLabelText[nSrc] == L'\r')
			continue;

		m_LabelText[nDst++] = wszLabelText[nSrc];
	}
	m_LabelText[nDst] = L'\0';

	LayoutLabelText();
}

void CHudGameTimer::AddLabelLine(const wchar_t *text, int nChars)
{
	if (m_nLabelLineCount >= m_nCaptionMaxLines)
		return;

	int nLine = m_nLabelLineCount++;
	int nCopy = nChars < GAME_TIMER_MAX_CAPTION - 1 ? nChars : GAME_TIMER_MAX_CAPTION - 1;
	wcsncpy(m_LabelLines[nLine], text, nCopy);
	m_LabelLines[nLine][nCopy] = L'\0';

	int nWidth = 0;
	for (int i = 0; i < nCopy; ++i)
		nWidth += vgui2::surface()->GetCharacterWidth(m_hTextFont, m_LabelLines[nLine][i]);

	m_nLabelLineWidths[nLine] = nWidth;
	if (nWidth > m_nLabelWidth)
		m_nLabelWidth = nWidth;
}

void CHudGameTimer::LayoutLabelText()
{
	m_nLabelWidth = 0;
	m_nLabelHeight = 0;
	m_nLabelLineCount = 0;

	for (int i = 0; i < GAME_TIMER_MAX_CAPTION_LINES; ++i)
	{
		m_LabelLines[i][0] = L'\0';
		m_nLabelLineWidths[i] = 0;
	}

	if (m_LabelText[0] == L'\0' || !m_hTextFont)
		return;

	// SBO: caption_width is pixels on a 1080-pixel-high screen (Source compares the key with pixels at every
	// resolution, so a caption wraps at other words on every screen). Scaled with the fonts (screen height /
	// 1080), the caption wraps at the same words everywhere, as far as the fonts' rounded widths allow.
	// 0 = no wrapping.
	int nCaptionWidth = 0;
	if (m_nCaptionWidth > 0)
	{
		int screenWide, screenTall;
		vgui2::surface()->GetScreenSize(screenWide, screenTall);
		nCaptionWidth = (int)(m_nCaptionWidth * screenTall / CAPTION_WIDTH_BASE_TALL + 0.5f);
		if (nCaptionWidth < 1)
			nCaptionWidth = 1;
	}

	const wchar_t *pParagraph = m_LabelText;
	while (*pParagraph && m_nLabelLineCount < m_nCaptionMaxLines)
	{
		const wchar_t *pParagraphEnd = pParagraph;
		while (*pParagraphEnd && *pParagraphEnd != L'\n')
			++pParagraphEnd;

		const wchar_t *pLineStart = pParagraph;
		while (pLineStart < pParagraphEnd && m_nLabelLineCount < m_nCaptionMaxLines)
		{
			while (pLineStart < pParagraphEnd && (*pLineStart == L' ' || *pLineStart == L'\t'))
				++pLineStart;

			if (pLineStart >= pParagraphEnd)
				break;

			int nWidth = 0;
			int nChars = 0;
			int nLastSpace = -1;

			for (const wchar_t *pScan = pLineStart; pScan < pParagraphEnd; ++pScan)
			{
				int nCharWidth = vgui2::surface()->GetCharacterWidth(m_hTextFont, *pScan);
				if (nCaptionWidth > 0 && nChars > 0 && nWidth + nCharWidth > nCaptionWidth)
					break;

				nWidth += nCharWidth;
				if (*pScan == L' ' || *pScan == L'\t')
					nLastSpace = nChars;

				++nChars;
			}

			if (pLineStart + nChars < pParagraphEnd && nLastSpace > 0)
			{
				AddLabelLine(pLineStart, nLastSpace);
				pLineStart += nLastSpace + 1;
			}
			else
			{
				AddLabelLine(pLineStart, nChars > 1 ? nChars : 1);
				pLineStart += nChars > 1 ? nChars : 1;
			}
		}

		if (*pParagraphEnd == L'\n')
			pParagraph = pParagraphEnd + 1;
		else
			break;
	}

	if (m_nLabelLineCount > 0)
	{
		m_nLabelHeight = vgui2::surface()->GetFontTall(m_hTextFont) * m_nLabelLineCount;
		m_nLabelHeight += Scaled(m_flLabelTimerSpacing);
	}
}

//-----------------------------------------------------------------------------
// Painting. Source: ShouldDraw's last checks, Paint, PaintBars, PaintNumbers
//-----------------------------------------------------------------------------
bool CHudGameTimer::ShouldPaint() const
{
	if (!m_bTimerDisplayed || m_flAlpha <= 0.0f)
		return false;

	// hud_draw, HIDEHUD_ALL, intermission, not connected, a full-screen menu (Source: the HUD element rules)
	if (!sbo::KillFeedShouldDraw())
		return false;

	if (m_flOverrideX == -1.0f && m_flOverrideY == -1.0f)
	{
		// don't overlap with the weapon selection HUD
		if (sbo::IsWeaponSelectionOpen())
			return false;
	}

	return true;
}

// The pulse, 1 when it starts, 0 at rest
float CHudGameTimer::PulseFraction() const
{
	float frac = m_flBlur / PULSE_BLUR;
	return frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);
}

// Bar, caption and digits: the state's colour, flashed towards white by the pulse
Color CHudGameTimer::GetTimerFgColor() const
{
	const float flash = PulseFraction() * m_flPulseFgStrength;
	return Color((int)(m_FgColor[0] + (255.0f - m_FgColor[0]) * flash), (int)(m_FgColor[1] + (255.0f - m_FgColor[1]) * flash),
	    (int)(m_FgColor[2] + (255.0f - m_FgColor[2]) * flash), (int)m_FgColor[3]);
}

// The box: the scheme's GameTimer.BgColor at rest (a dark green at alpha 110, so the text reads over a bright
// scene); the pulse lays the state's colour (green, red while warning) over it at up to PulseBgAlpha
Color CHudGameTimer::GetBoxColor() const
{
	const float over = PulseFraction() * (m_iPulseBgAlpha / 255.0f);
	const float under = m_SchemeBgColor[3] / 255.0f;
	const float alpha = over + under * (1.0f - over);
	Color color = m_SchemeBgColor;
	if (alpha <= 0.0f)
		return color;
	for (int i = 0; i < 3; i++)
		color[i] = (unsigned char)((m_FgColor[i] * over + m_SchemeBgColor[i] * under * (1.0f - over)) / alpha + 0.5f);
	color[3] = (unsigned char)(alpha * 255.0f + 0.5f);
	return color;
}

Color CHudGameTimer::WithAlpha(Color color) const
{
	// Source: the panel's alpha (0..220) scales everything it draws
	color[3] = (unsigned char)(color[3] * (m_flAlpha / 255.0f));
	return color;
}

void CHudGameTimer::GetBoxBounds(int &x, int &y, int &wide, int &tall, int &timerWide) const
{
	const int border = Scaled(m_flBorder);

	// check for 00:00 timer
	timerWide = m_nTimerWidth;
	if (m_iValue > 600)
		timerWide += vgui2::surface()->GetCharacterWidth(m_hNumberFont, '0');

	wide = (timerWide > m_nLabelWidth ? timerWide : m_nLabelWidth) + border;
	if (Scaled(m_flMinWidth) > wide)
		wide = Scaled(m_flMinWidth);

	tall = m_nTimerHeight + m_nLabelHeight + border / 2;
	if (m_nProgressBarMax > 0)
		tall += Scaled(m_flBarHeight) + Scaled(m_flBarVerticalGap);
	else
		tall += border / 2;

	// the panel covers the screen (OnThink)
	int screenW, screenH;
	const_cast<CHudGameTimer *>(this)->GetSize(screenW, screenH);

	const float flScalarX = m_flOverrideX != -1.0f ? m_flOverrideX : DEFAULT_X;
	const float flScalarY = m_flOverrideY != -1.0f ? m_flOverrideY : DEFAULT_Y;

	x = (int)((screenW - wide) * flScalarX);
	y = (int)((screenH - tall) * flScalarY);

	// SBO: the default place is inside the spectator's top bar; below it while the bars are shown
	if (m_flOverrideY == -1.0f && CSboSpectatorPanel::Get() && CSboSpectatorPanel::Get()->IsShown())
	{
		const int top = sboui::YRes(m_flSpectatorTop);
		if (y < top)
			y = top;
	}
	// SBO 2.0 GM mode: under a Game Master's status strip
	if (y < VGui2_GmStripBottom() && y + tall > 0)
		y = std::max(y, VGui2_GmStripBottom());
}

void CHudGameTimer::PaintBackground()
{
	// box and bars here, text in Paint(): text rendering disables alpha blending afterwards (BHL kill feed)
	if (!ShouldPaint())
		return;

	int x, y, wide, tall, timerWide;
	GetBoxBounds(x, y, wide, tall, timerWide);

	// rounded (the panel's corner textures, ui/gfx/vgui/800corner*); a plain rectangle if they are missing, where
	// DrawBox draws nothing
	// The box fades with the panel (show / close) but is at its own alpha once shown, not scaled by the panel's 220
	// as the text is, so its alpha is GameTimer.BgColor's
	float boxFade = m_flAlpha / SHOWN_ALPHA;
	if (boxFade > 1.0f)
		boxFade = 1.0f;
	int cornerWide = 0, cornerTall = 0;
	GetCornerTextureSize(cornerWide, cornerTall);
	if (cornerWide > 0 && cornerTall > 0)
		DrawBox(x, y, wide, tall, GetBoxColor(), boxFade);
	else
	{
		Color box = GetBoxColor();
		box[3] = (unsigned char)(box[3] * boxFade);
		vgui2::surface()->DrawSetColor(box);
		vgui2::surface()->DrawFilledRect(x, y, x + wide, y + tall);
	}

	if (m_nProgressBarMax > 0)
	{
		const int border = Scaled(m_flBorder);
		PaintBars(x + border / 2, y + border / 2, wide - border);
	}
}

void CHudGameTimer::PaintBars(int xpos, int ypos, int wide)
{
	const int gap = Scaled(m_flBarChunkGap);
	const int barHeight = Scaled(m_flBarHeight);

	// get bar chunks
	int barChunkWidth = (wide / m_nProgressBarMax) - gap;

	int enabledChunks = 0;
	if (m_nProgressBarOverride > -1)
	{
		enabledChunks = m_nProgressBarOverride;
	}
	else if (m_iTimerMaxLength > 0)
	{
		enabledChunks = (int)floorf(((float)m_iValue / (float)m_iTimerMaxLength) * (float)m_nProgressBarMax);
		if (!m_bShowTimeRemaining)
			enabledChunks = m_nProgressBarMax - enabledChunks;
	}
	if (enabledChunks < 0)
		enabledChunks = 0;
	if (enabledChunks > m_nProgressBarMax)
		enabledChunks = m_nProgressBarMax;

	Color fg = GetTimerFgColor();

	// draw the enabled chunks
	vgui2::surface()->DrawSetColor(WithAlpha(fg));
	for (int i = 0; i < enabledChunks; i++)
	{
		vgui2::surface()->DrawFilledRect(xpos, ypos, xpos + barChunkWidth, ypos + barHeight);
		xpos += barChunkWidth + gap;
	}

	// draw the exhausted portion of the bar
	Color clrExhausted = fg;
	clrExhausted[3] = (unsigned char)(((float)clrExhausted[3] / 255.0f) * m_iBarDisabledAlpha);
	vgui2::surface()->DrawSetColor(WithAlpha(clrExhausted));
	for (int i = enabledChunks; i < m_nProgressBarMax; i++)
	{
		vgui2::surface()->DrawFilledRect(xpos, ypos, xpos + barChunkWidth, ypos + barHeight);
		xpos += barChunkWidth + gap;
	}
}

void CHudGameTimer::Paint()
{
	if (!ShouldPaint())
		return;

	int x, y, wide, tall, timerWide;
	GetBoxBounds(x, y, wide, tall, timerWide);

	const int border = Scaled(m_flBorder);
	const Color fg = WithAlpha(GetTimerFgColor());

	y += border / 2;

	// the bars (PaintBackground)
	if (m_nProgressBarMax > 0)
		y += Scaled(m_flBarVerticalGap);

	if (m_nLabelHeight > 0)
	{
		vgui2::surface()->DrawSetTextFont(m_hTextFont);
		vgui2::surface()->DrawSetTextColor(fg);

		const int nLineHeight = vgui2::surface()->GetFontTall(m_hTextFont);
		for (int i = 0; i < m_nLabelLineCount; ++i)
		{
			vgui2::surface()->DrawSetTextPos(x + (int)((wide - m_nLabelLineWidths[i]) * 0.5f), y);
			vgui2::surface()->DrawPrintText(m_LabelLines[i], (int)wcslen(m_LabelLines[i]));
			y += nLineHeight;
		}
		y += Scaled(m_flLabelTimerSpacing);
	}

	// draw our numbers
	vgui2::surface()->DrawSetTextColor(fg);
	PaintNumbers(x + (int)((wide - timerWide) * 0.5f), y);
}

void CHudGameTimer::PaintNumbers(int xpos, int ypos)
{
	vgui2::surface()->DrawSetTextFont(m_hNumberFont);

	int nTimeToDisplay = m_iValue;

	if (!m_bShowTimeRemaining)
		nTimeToDisplay = m_iTimerMaxLength - nTimeToDisplay;

	int nMinutes = 0;
	int nSeconds = 0;
	if (nTimeToDisplay > 0)
	{
		nMinutes = nTimeToDisplay / 60;
		nSeconds = nTimeToDisplay % 60;
	}

	// Source: a buffer of six characters, i.e. "m:ss" or "mm:ss" (longer times are cut)
	wchar_t unicode[6];
	swprintf(unicode, sizeof(unicode) / sizeof(unicode[0]), L"%d:%02d", nMinutes, nSeconds);
	unicode[5] = L'\0';
	const int len = (int)wcslen(unicode);

	vgui2::surface()->DrawSetTextPos(xpos, ypos);
	vgui2::surface()->DrawPrintText(unicode, len);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_GameTimer_Init()
{
	if (CHudGameTimer::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	CHudGameTimer *pPanel = CHudGameTimer::Create(g_pViewport);
	pPanel->MakeReadyForUse();
	pPanel->Reset();

	sbo::HookUserMsg("GameTimer", &MsgFunc_GameTimer);
	return true;
}

void VGui2_GameTimer_Reset()
{
	if (CHudGameTimer::Get())
		CHudGameTimer::Get()->Reset();
}
