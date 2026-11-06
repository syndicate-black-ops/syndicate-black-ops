"ui/resource/gametimer.res"
{
	// HUD timer of the map entity game_timer (SBO docs: game_timer). Sizes in 640x480 units, fonts included:
	// multiplied by the screen height / 480 (2.25 at 1080p), with or without hud_scale.
	// Colors: sboscheme.res (GameTimer.*).
	"HudGenericGameTimer"
	{
		"fieldName"			"HudGenericGameTimer"

		"MinWidth"			"100"		// Narrowest box
		"Border"			"24"		// Left + right padding of the box; half of it above the progress bar
		"LabelTimerSpacing"	"2"			// Gap between the caption and the time
		"BarHeight"			"5"			// Height of a progress bar segment
		"BarChunkGap"		"3"			// Gap between two segments
		"BarVerticalGap"	"8"			// From the top of the bar to the caption (or the time)
		"BarDisabledAlpha"	"70"		// Alpha of the used-up segments (0-255)
		"MaxBars"			"15"		// Most segments when the entity doesn't set progress_bar_max

		"SpectatorTop"		"36"		// Top of the box while the spectator bars are shown (640x480 screen units;
										// the top bar is 32 high), for timers without their own y

		// Fonts: the time, then the caption
		"NumberFontName"	"Arial" [$WINDOWS]
		"NumberFontName"	"Liberation Sans" [!$WINDOWS]
		"NumberFontTall"	"15"
		"NumberFontWeight"	"700"
		"TextFontName"		"Arial" [$WINDOWS]
		"TextFontName"		"Liberation Sans" [!$WINDOWS]
		"TextFontTall"		"13"
		"TextFontWeight"	"0"
		"FontAdditive"		"1"			// 0: plain (alpha blended) text

		// The flash of the whole timer. It repeats while the state lasts; 0: once, when the state begins
		"LoopWarnPulse"		"1"			// warning state
		"LoopPausePulse"	"1"			// paused
		"PulseTime"			"0.5"		// Seconds a flash takes to settle
		"PulseFgStrength"	"0.85"		// How far bar, caption and time jump towards white (0 = not at all, 1 = white)
		"PulseBgAlpha"		"100"		// Alpha of the state's color laid over the box at the start of a flash (0-255)
	}
}
