//
// Tutor window scheme (env_tutor messages; see the SBO docs: Tutor window)
//
// The colors of Counter-Strike: Condition Zero's tutor, with larger "Default" font sizes for higher
// resolutions. The window's layout (resource/ui/tutortextwindow.res) scales with this font's height.
//
// Text and icon color of a message: "TutorTextClass<type>", <type> = the TutorText message's type
// (1 default, 2 friend death, 4 enemy death, 8 scenario, 16 buy, 32 career, 64 hint, 128 in-game hint,
// 256 end game; env_tutor's "type"); missing ones use "TutorTextClassDefault". An env_tutor's "color",
// "iconcolor", "bgcolor" and "bordercolor" override the type's color, "WindowBG" and "BorderBright".
//
#base "clientschemebase.res"

Scheme
{
	//////////////////////// COLORS ///////////////////////////
	Colors
	{
		// background colors
		"WindowBG"			"0 0 0 200"		// background of the window (the rounded corners use its alpha)
		"ControlBG"			"0 0 0 0"		// background color of controls

		"BorderBright"			"28 207 61 128"		// outline of the (square) window

		"TutorTextClassDefault"	"28 207 61 255"
		"TutorTextClass2"	"255 38 38 255"			// friend death
		"TutorTextClass4"	"38 149 253 255"		// enemy death
		"TutorTextClass8"	"255 227 38 255"		// scenario
		"TutorTextClass256"	"255 227 38 255"		// end game
	}

	//////////////////////// FONTS /////////////////////////////
	Fonts
	{
		"Default"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"14"
				"weight"	"500"
				"yres"	"1 1023"
			}
			"2"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"18"
				"weight"	"500"
				"yres"	"1024 1199"
				"antialias"	"1"
			}
			"3"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"20"
				"weight"	"500"
				"yres"	"1200 1439"
				"antialias"	"1"
			}
			"4"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"26"
				"weight"	"500"
				"yres"	"1440 2159"
				"antialias"	"1"
			}
			"5"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"38"
				"weight"	"500"
				"yres"	"2160 10000"
				"antialias"	"1"
			}
		}
	}
}
