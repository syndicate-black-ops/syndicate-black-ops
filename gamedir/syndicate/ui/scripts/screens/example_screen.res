"ui/scripts/screens/example_screen.res"
{
	// Example world screen layout (vgui_screen "panelname" "example_screen"; SBO docs: World screens).
	// The "WorldScreen" section is the screen itself: "wide" / "tall" are its pixel size (default 512 x 256, at
	// most 1024). Fonts: ui/resource/worldscreenscheme.res. A Button's "command" goes to the server, which fires
	// the vgui_screen's "target" and its "~<command>" key.
	"WorldScreen"
	{
		"ControlName"		"EditablePanel"
		"fieldName"			"WorldScreen"
		"wide"				"512"
		"tall"				"256"
		"bgcolor_override"	"12 22 16 255"
	}
	"Title"
	{
		"ControlName"		"Label"
		"fieldName"			"Title"
		"xpos"				"24"
		"ypos"				"16"
		"wide"				"464"
		"tall"				"44"
		"labelText"			"SBO WORLD SCREEN"
		"font"				"WorldScreenTitle"
		"fgcolor_override"	"255 200 40 255"
		"textAlignment"		"west"
	}
	"Status"
	{
		"ControlName"		"Label"
		"fieldName"			"Status"
		"xpos"				"24"
		"ypos"				"72"
		"wide"				"464"
		"tall"				"60"
		"labelText"			"Aim at a button and fire to press it."
		"font"				"WorldScreenText"
		"textAlignment"		"north-west"
		"wrap"				"1"
	}
	"HelloButton"
	{
		"ControlName"		"Button"
		"fieldName"			"HelloButton"
		"xpos"				"24"
		"ypos"				"168"
		"wide"				"220"
		"tall"				"56"
		"labelText"			"HELLO"
		"font"				"WorldScreenButton"
		"textAlignment"		"center"
		"command"			"hello"
	}
	"LightButton"
	{
		"ControlName"		"Button"
		"fieldName"			"LightButton"
		"xpos"				"268"
		"ypos"				"168"
		"wide"				"220"
		"tall"				"56"
		"labelText"			"LIGHT"
		"font"				"WorldScreenButton"
		"textAlignment"		"center"
		"command"			"light"
	}
}
