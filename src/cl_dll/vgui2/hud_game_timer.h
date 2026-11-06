//========= SBO VGUI2: game_timer HUD timer =========//
//
// Purpose: The HUD countdown timer of the map entity game_timer (server: src/dlls/game_timer.{h,cpp}, the
//          GameTimer message; docs/GAME_TIMER.md). Port of SBO Source's CHudGenericGameTimer
//          (syndicate-blackops-source src/game/client/hud_generic_timer.cpp, Mapbase's generic timer HUD element by
//          Blixibon), with its HudLayout.res / HudAnimations.txt entries:
//            * a rounded, translucent box at the top centre (or where the entity's x / y put it) holding, from the
//              top: the progress bar's segments, the caption (wrapped, up to caption_lines lines), the time m:ss
//            * ONE timer at a time: the first enabled timer in the map's entity order; the next one shows after it
//              is disabled and has faded out (one second)
//            * fades in (0.15 s, with a flash when it runs), fades out (0.25 s); flashes when it resumes, and about
//              twice a second while it is paused or in the warning state, where it is red. SBO:
//              the whole element flashes, box included; Source's script only brightens the digits.
//          The values the animations of Source work on (alpha, foreground colour, blur) are animated here by hand:
//          the client has no HudAnimations.txt.
//
//          Layout: ui/resource/gametimer.res. Colours and fonts: ui/resource/sboscheme.res (GameTimer.*).
//
// Sizes are Source's 640x480 units times screen height / 480, as Source's proportional HUD, whatever hud_scale
// says (that cvar scales the sprite HUD); the fonts are created at that size (ApplySchemeSettings).
//
// SBO: below the spectator's top bar while that is shown; hidden while the weapon selection is open (Source: the same, for its own weapon selection); the countdown
// runs on the client from the server time the timer ends at.
//
// SDK world (hl1_source_sdk headers); SBO data through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_HUD_GAME_TIMER_H
#define SBO_VGUI2_HUD_GAME_TIMER_H

#include <wchar.h>
#include <Color.h>
#include <vgui_controls/Panel.h>

// Source: MAX_GAME_TIMER_CAPTION, MAX_GAME_TIMER_CAPTION_LINES (game_timer.h)
constexpr int GAME_TIMER_MAX_CAPTION = 128;
constexpr int GAME_TIMER_MAX_CAPTION_LINES = 4;

// The GameTimer message's flags byte (server: GAME_TIMER_FLAG_*)
enum
{
	GAME_TIMER_FLAG_ENABLED = (1 << 0),
	GAME_TIMER_FLAG_PAUSED = (1 << 1),
	GAME_TIMER_FLAG_SHOW_REMAINING = (1 << 2),
	GAME_TIMER_FLAG_FULL = (1 << 3),
};

//! A timer the server told us about (enabled ones only; Source: C_GameTimer).
struct GameTimerState
{
	int id = 0; //!< the entity's index; 0 = free slot
	bool paused = false;
	bool showTimeRemaining = true;
	float time = 0.0f;      //!< paused: seconds remaining; else the server time it ends at
	float maxLength = 0.0f; //!< Source: GetTimerMaxLength
	float warnTime = 0.0f;  //!< Source: GetWarnTime
	int progressBarMax = -1;
	int progressBarOverride = -1;
	float x = -1.0f, y = -1.0f;
	int captionWidth = 260;
	int captionMaxLines = 2;
	char caption[GAME_TIMER_MAX_CAPTION] = "";
};

class CHudGameTimer : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CHudGameTimer, vgui2::Panel);

public:
	static CHudGameTimer *Create(vgui2::Panel *pParent);
	static CHudGameTimer *Get() { return s_pInstance; }

	//! InitHUD (new map / connection): no timers, nothing shown. Source: Reset.
	void Reset();
	//! The GameTimer message.
	void OnMessage(const unsigned char *data, int size);

	virtual void OnThink() override;
	virtual void PaintBackground() override;
	virtual void Paint() override;
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
	CHudGameTimer(vgui2::Panel *pParent);

	static constexpr int MAX_TIMERS = 64;

	GameTimerState *FindState(int id);
	GameTimerState *AddState(int id);
	//! Source: C_GameTimer::GetTimeRemaining
	float GetTimeRemaining(const GameTimerState &timer) const;

	//! Source: FindTimer (the first enabled timer becomes the shown one).
	bool FindTimer();
	//! Source: OnThink.
	void ThinkTimer(float now, float frameTime);
	//! The values of Source's animations.
	void UpdateAnimations(float now, float frameTime);
	void StartShow(float now, bool flash);
	void StartPulse(float now);

	//! Source: UpdateTimerLabel / SetLabelText / LayoutLabelText.
	void UpdateTimerLabel(const GameTimerState &timer);
	void LayoutLabelText();
	void AddLabelLine(const wchar_t *text, int nChars);

	void LoadLayout();
	void CreateFonts();
	int Scaled(float value) const;
	bool ShouldPaint() const;
	void GetBoxBounds(int &x, int &y, int &wide, int &tall, int &timerWide) const;
	Color WithAlpha(Color color) const;
	float PulseFraction() const;
	Color GetTimerFgColor() const;
	Color GetBoxColor() const;
	void PaintBars(int x, int y, int wide);
	void PaintNumbers(int x, int y);

	static CHudGameTimer *s_pInstance;

	GameTimerState m_Timers[MAX_TIMERS];

	// Source's members
	int m_iTimerIndex = 0;
	bool m_bTimerDisplayed = false;
	float m_flShutoffTime = 0.0f;
	bool m_bTimerPaused = false;
	bool m_bTimerWarned = false;

	int m_iValue = 0;
	int m_iTimerMaxLength = 0;
	bool m_bShowTimeRemaining = true;
	int m_nProgressBarMax = 0;
	int m_nProgressBarOverride = -1;
	float m_flOverrideX = -1.0f, m_flOverrideY = -1.0f;
	int m_nCaptionWidth = 0;
	int m_nCaptionMaxLines = 2;
	char m_szCurrentTimerCaption[GAME_TIMER_MAX_CAPTION] = "";
	wchar_t m_LabelText[GAME_TIMER_MAX_CAPTION] = L"";
	wchar_t m_LabelLines[GAME_TIMER_MAX_CAPTION_LINES][GAME_TIMER_MAX_CAPTION] = {};
	int m_nLabelLineWidths[GAME_TIMER_MAX_CAPTION_LINES] = {};
	int m_nLabelLineCount = 0;
	int m_nLabelWidth = 0, m_nLabelHeight = 0;
	int m_nTimerWidth = 0, m_nTimerHeight = 0;

	// gametimer.res (Source: the panel's animation variables), in 640x480 HUD units
	float m_flMinWidth = 100;
	float m_flBorder = 24;
	float m_flLabelTimerSpacing = 2;
	float m_flBarHeight = 5;
	float m_flBarChunkGap = 3;
	float m_flBarVerticalGap = 8;
	int m_iBarDisabledAlpha = 70;
	int m_iMaxBars = 15;          //!< Source: the cvar hud_timer_max_bars
	float m_flSpectatorTop = 36;  //!< SBO: the box's top below the spectator's top bar (640x480 screen units)
	char m_szNumberFontName[64] = "Arial"; //!< Source: HudHintTextLarge (GorDIN 15, bold, additive)
	float m_flNumberFontTall = 15;
	int m_iNumberFontWeight = 700;
	char m_szTextFontName[64] = "Arial"; //!< Source: HudHintTextSmall (GorDIN 13, additive)
	float m_flTextFontTall = 13;
	int m_iTextFontWeight = 0;
	bool m_bFontAdditive = true;
	bool m_bLoopWarnPulse = true;  //!< the flash repeats for as long as the timer warns (0: once)
	bool m_bLoopPausePulse = true; //!< the flash repeats for as long as the timer is paused (0: once)
	bool m_bColorsSet = false;
	float m_flPulseTime = 0.5f;        //!< seconds a flash takes to settle (Source's Pulse: 0.5)
	float m_flPulseFgStrength = 0.85f; //!< how far bar, caption and digits jump towards white (0..1)
	int m_iPulseBgAlpha = 100;         //!< alpha of the state's colour laid over the box at the start of a flash (0-255)

	// sboscheme.res
	vgui2::HFont m_hNumberFont = 0;
	vgui2::HFont m_hTextFont = 0;
	Color m_SchemeFgColor;
	Color m_SchemeBgColor;
	Color m_WarnColor;
	int m_iFontScreenTall = 0; //!< the screen height the fonts were created for

	// Animated values (Source: Alpha, FgColor, Blur)
	float m_flAlpha = 0.0f; //!< 0..255
	float m_flAlphaFrom = 0.0f, m_flAlphaTo = 0.0f, m_flAlphaStart = 0.0f, m_flAlphaDuration = 0.0f;
	bool m_bAlphaAnim = false;
	float m_FgColor[4] = { 255, 255, 255, 255 };
	float m_FgFrom[4] = {}, m_FgTo[4] = {};
	float m_flFgStart = 0.0f, m_flFgDuration = 0.0f;
	bool m_bFgAnim = false;
	float m_flBlur = 0.0f;
	float m_flBlurStart = 0.0f; //!< when the glow's fall begins
	bool m_bBlurAnim = false;

	double m_flLastThinkTime = 0;
	double m_flTimeBase = 0;
};

#endif // SBO_VGUI2_HUD_GAME_TIMER_H
