"ui/scripts/screens/train_where.res"
{
	// The screen on the train of the full Move With example map (vgui_screen "panelname" "train_where"). The button sends the
	// command "where", and the map's vgui_control_proxy entities write the train's position into CoordX, CoordY
	// and CoordZ, each rounded to whole units by a trigger_copyvalue.
	"WorldScreen"
	{
		"ControlName"		"EditablePanel"
		"fieldName"			"WorldScreen"
		"wide"				"512"
		"tall"				"256"
		"bgcolor_override"	"12 22 16 255"
	}
	"LetterX"
	{
		"ControlName"		"Label"
		"fieldName"			"LetterX"
		"xpos"				"24"
		"ypos"				"48"
		"wide"				"32"
		"tall"				"56"
		"labelText"			"X"
		"font"				"WorldScreenLarge"
		"fgcolor_override"	"255 200 40 255"
		"textAlignment"		"west"
	}
	"CoordX"
	{
		"ControlName"		"Label"
		"fieldName"			"CoordX"
		"xpos"				"56"
		"ypos"				"48"
		"wide"				"112"
		"tall"				"56"
		"labelText"			"-"
		"font"				"WorldScreenLarge"
		"textAlignment"		"west"
	}
	"LetterY"
	{
		"ControlName"		"Label"
		"fieldName"			"LetterY"
		"xpos"				"184"
		"ypos"				"48"
		"wide"				"32"
		"tall"				"56"
		"labelText"			"Y"
		"font"				"WorldScreenLarge"
		"fgcolor_override"	"255 200 40 255"
		"textAlignment"		"west"
	}
	"CoordY"
	{
		"ControlName"		"Label"
		"fieldName"			"CoordY"
		"xpos"				"216"
		"ypos"				"48"
		"wide"				"112"
		"tall"				"56"
		"labelText"			"-"
		"font"				"WorldScreenLarge"
		"textAlignment"		"west"
	}
	"LetterZ"
	{
		"ControlName"		"Label"
		"fieldName"			"LetterZ"
		"xpos"				"344"
		"ypos"				"48"
		"wide"				"32"
		"tall"				"56"
		"labelText"			"Z"
		"font"				"WorldScreenLarge"
		"fgcolor_override"	"255 200 40 255"
		"textAlignment"		"west"
	}
	"CoordZ"
	{
		"ControlName"		"Label"
		"fieldName"			"CoordZ"
		"xpos"				"376"
		"ypos"				"48"
		"wide"				"112"
		"tall"				"56"
		"labelText"			"-"
		"font"				"WorldScreenLarge"
		"textAlignment"		"west"
	}
	"WhereButton"
	{
		"ControlName"		"Button"
		"fieldName"			"WhereButton"
		"xpos"				"56"
		"ypos"				"152"
		"wide"				"400"
		"tall"				"64"
		"labelText"			"Where is the train?"
		"font"				"WorldScreenButton"
		"textAlignment"		"center"
		"command"			"where"
	}
}
