///////////////////////////////////////////////////////////
// SBO scheme resource file: colors and fonts of SBO's menus, HUD menu, game timer and message box.
//
// Everything not listed here comes from ClientSchemeBase.res. Most fonts and colors of the briefing, team and
// spectator menus come from <res>_textscheme.txt instead.
///////////////////////////////////////////////////////////
#base "clientschemebase.res"

Scheme
{
	Colors
	{
		"SBO.Green"				"41 107 34 255"		// borders, window titles
		"SBO.ButtonText"		"47 145 60 255"		// button text
		"SBO.ButtonArmedBg"		"47 145 60 70"		// button background when aimed at

		// HUD text menu (radio, server and plugin menus)
		"HudMenu.TextColor"		"47 145 60 255"		// text lines (title), default color and \w
		"HudMenu.ItemColor"		"47 145 60 255"		// numbered item lines, default color and \w
		"HudMenu.BoxColor"		"8 24 10 230"		// rounded background box (a nearly opaque dark green, so the lines read over a bright scene)
		"HudMenu.SelectedColor"	"150 255 160 255"	// the chosen item while it flashes
		"HudMenu.SelectedBgColor"	"47 145 60 70"	// bar behind the chosen item
		// color codes sent by the server and plugins
		"HudMenu.YellowColor"	"255 210 64 255"	// \y
		"HudMenu.RedColor"		"210 24 0 255"		// \r
		"HudMenu.DimColor"		"100 100 100 255"	// \d

		// game_timer HUD timer (layout: gametimer.res)
		"GameTimer.FgColor"		"47 145 60 255"		// time, caption, progress bar
		"GameTimer.BgColor"		"16 48 20 110"		// rounded background box (a dark green, so time and caption read over a bright scene)
		"GameTimer.WarnColor"	"255 70 70 255"		// time, caption and bar from the warning time on

		// game_messagebox message box: text and frame in SBO's HUD green on a nearly opaque black box, so the text
		// stays readable over a bright scene. Its scroll bar is styled below (BaseSettings), the Close button's label
		// uses MessageBox.ButtonFont.
		"MessageBox.TextColor"		"47 145 60 255"		// text
		"MessageBox.BorderColor"	"47 145 60 255"		// 1-pixel frame, the line under the title bar
		"MessageBox.BgColor"		"0 0 0 240"			// box background
		"MessageBox.TitleColor"		"47 145 60 255"		// title ("title" key)
		"MessageBox.TitleBgColor"	"47 145 60 40"		// title bar
	}

	BaseSettings
	{
		// scroll bars of the menus: 16-pixel arrow buttons (white, half transparent until aimed at) and a green slider
		ScrollBar.Wide						16

		ScrollBarButton.FgColor				"255 255 255 127"
		ScrollBarButton.BgColor				"Blank"
		ScrollBarButton.ArmedFgColor		"White"
		ScrollBarButton.ArmedBgColor		"Blank"
		ScrollBarButton.DepressedFgColor	"White"
		ScrollBarButton.DepressedBgColor	"Blank"

		ScrollBarSlider.FgColor				"SBO.ButtonArmedBg"	// nob
		ScrollBarSlider.BgColor				"Blank"				// track
	}

	Borders
	{
		ScrollBarButtonBorder
		{
			"inset" "0 0 0 0"
		}

		ScrollBarButtonDepressedBorder
		{
			"inset" "0 0 0 0"
		}

		ScrollBarSliderBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "SBO.ButtonText"
					"offset" "0 0"
				}
			}

			Right
			{
				"1"
				{
					"color" "SBO.ButtonText"
					"offset" "0 0"
				}
			}

			Top
			{
				"1"
				{
					"color" "SBO.ButtonText"
					"offset" "0 0"
				}
			}

			Bottom
			{
				"1"
				{
					"color" "SBO.ButtonText"
					"offset" "0 0"
				}
			}
		}
	}

	Fonts
	{
		// HUD text menu fonts with hud_scale 1: sizes for a 640x480 screen, grown with the screen like the scaled HUD.
		// Without hud_scale the menu sizes its fonts by the screen height.
		// No "dropshadow": it does not work together with "antialias".
		"HudMenu.ScaledTextFont"
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"15"
				"weight"	"600"
				"antialias"	"1"
			}
		}
		"HudMenu.ScaledItemFont"
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"15"
				"weight"	"600"
				"antialias"	"1"
			}
		}
		"HudMenu.ScaledItemFontPulsing"
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"15"
				"weight"	"800"
				"antialias"	"1"
			}
		}
		// game_messagebox message box text and title. Always proportional: "tall" is for a 480-pixel-high screen
		// and grows with the screen height (with or without hud_scale), like the text of SBO's other windows.
		"MessageBox.TextFont"
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"14"
				"weight"	"500"
				"antialias"	"1"
			}
		}
		"MessageBox.ButtonFont"		// the Close button's label
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"14"
				"weight"	"700"
				"antialias"	"1"
			}
		}
		"MessageBox.TitleFont"
		{
			"1"
			{
				"name"		"Arial" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"16"
				"weight"	"700"
				"antialias"	"1"
			}
		}
	}
}
