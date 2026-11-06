///////////////////////////////////////////////////////////
// World screen scheme (vgui_screen; see the SBO docs: World screens)
//
// World screen panels (ui/scripts/screens/*.res) are drawn into a texture of a fixed pixel size, so their fonts
// have one size at every screen resolution. Everything else comes from the client scheme. Buttons are flat green,
// bright green when aimed at.
///////////////////////////////////////////////////////////
#base "clientschemebase.res"

Scheme
{
	Colors
	{
		"WorldScreen.Green"			"30 110 55 255"
		"WorldScreen.GreenArmed"	"56 255 110 255"
		"WorldScreen.GreenPressed"	"125 255 160 255"
		"WorldScreen.Text"			"230 255 235 255"
		"WorldScreen.TextDark"		"10 30 15 255"
	}

	BaseSettings
	{
		Button.TextColor				"WorldScreen.Text"
		Button.BgColor					"WorldScreen.Green"
		Button.ArmedTextColor			"WorldScreen.TextDark"
		Button.ArmedBgColor				"WorldScreen.GreenArmed"
		Button.DepressedTextColor		"WorldScreen.TextDark"
		Button.DepressedBgColor			"WorldScreen.GreenPressed"
		Label.TextColor					"WorldScreen.Text"
	}

	Fonts
	{
		// a layout names these in a control's "font" key
		"WorldScreenTitle"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"34"
				"weight"	"700"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
		"WorldScreenText"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"22"
				"weight"	"500"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
		// larger and smaller sizes (the security station showcase, security_main.res / security_mirror.res)
		"WorldScreenHeader"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"42"
				"weight"	"800"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
		"WorldScreenLarge"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"30"
				"weight"	"700"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
		"WorldScreenSmall"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"18"
				"weight"	"600"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
		"WorldScreenButton"
		{
			"1"
			{
				"name"		"Tahoma" [$WINDOWS]
				"name"		"Liberation Sans" [!$WINDOWS]
				"tall"		"24"
				"weight"	"700"
				"antialias"	"1"
				"yres"		"1 10000"
			}
		}
	}
}
