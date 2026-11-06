///////////////////////////////////////////////////////////
// Server browser scheme ("Find server")
//
// The browser draws its own window, tabs, buttons, filters, list and info panel. This file styles the standard
// controls it uses: the drop-down menus of the Mode / Max ping filters, the text entries and the list's scroll bar.
///////////////////////////////////////////////////////////
#base "clientschemebase.res"

Scheme
{
	Colors
	{
		"SBOBrowser.Text"		"127 214 95 255"	// #7fd65f
		"SBOBrowser.TextBright"	"166 236 138 255"	// #a6ec8a
		"SBOBrowser.Input"		"11 14 11 255"		// #0b0e0b
		"SBOBrowser.Selected"	"29 58 24 255"		// #1d3a18
		"SBOBrowser.Border"		"60 122 50 255"		// #3c7a32
		"SBOBrowser.Primary"	"46 95 38 255"		// #2e5f26
	}

	BaseSettings
	{
		Menu.TextColor					"SBOBrowser.Text"
		Menu.BgColor					"SBOBrowser.Input"
		Menu.ArmedTextColor				"SBOBrowser.TextBright"
		Menu.ArmedBgColor				"SBOBrowser.Selected"
		Menu.TextInset					"8"

		TextEntry.TextColor			"SBOBrowser.Text"
		TextEntry.BgColor			"SBOBrowser.Input"
		TextEntry.CursorColor		"SBOBrowser.TextBright"
		TextEntry.DisabledTextColor	"SBOBrowser.Border"
		TextEntry.DisabledBgColor	"SBOBrowser.Input"
		TextEntry.SelectedTextColor	"230 245 223 255"
		TextEntry.SelectedBgColor	"SBOBrowser.Primary"
		TextEntry.OutOfFocusSelectedBgColor	"SBOBrowser.Primary"
		TextEntry.FocusEdgeColor	"0 0 0 0"

		ScrollBarButton.FgColor				"SBOBrowser.Text"
		ScrollBarButton.BgColor				"Blank"
		ScrollBarButton.ArmedFgColor		"SBOBrowser.TextBright"
		ScrollBarButton.ArmedBgColor		"Blank"
		ScrollBarButton.DepressedFgColor	"SBOBrowser.TextBright"
		ScrollBarButton.DepressedBgColor	"Blank"

		ScrollBarSlider.FgColor				"SBOBrowser.Selected"	// nob
		ScrollBarSlider.BgColor				"Blank"					// track
	}

	Borders
	{
		// the drop-down menus: a 1-pixel frame in the browser's border green
		MenuBorder
		{
			"inset" "0 0 0 0"
			Left
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Right
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
		}

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
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Right
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Top
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
			Bottom
			{
				"1"
				{
					"color" "SBOBrowser.Border"
					"offset" "0 0"
				}
			}
		}
	}
}
