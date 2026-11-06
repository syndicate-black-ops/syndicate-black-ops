// Spectator target info: the box above the spectator panel's bottom bar with the watched player's health, armor,
// weapon, legs and level / XP, points or BR rounds won (two lines; the name is in the bottom bar). Turned off with
// spec_drawinfo 0 (spectator menu: Settings > Target Info). The box is as wide as its wider line and centered.
// Sizes in 640x480 units, scaled with the screen. Colors: a color name from ClientScheme.res / ClientSchemeBase.res,
// or "r g b a".
"ui/resource/spectatorinfo.res"
{
	"SpectatorInfoPanel"
	{
		"ControlName"		"CSboSpectatorInfoPanel"
		"fieldName"			"SpectatorInfoPanel"

		"bottom_margin"		"4"			// Gap between the box and the spectator panel's bottom bar
		"hor_padding"		"8"			// Left and right padding inside the box
		"vert_padding"		"3"			// Top and bottom padding inside the box
		"row_height"		"12"		// Height of a line (at least the font's height)
		"item_spacing"		"12"		// Distance between two items of a line ("HP 100" and "Armor 50")
		"label_spacing"		"4"			// Distance between an item's label and its value
		"text_font"			"Default"

		"color_bg"			"SpecInfo_Background"
		"color_border"		"SpecInfo_Border"
		"color_label"		"SpecInfo_Label"	// "HP", "Armor", "Weapon", ...
		"color_value"		"SpecInfo_Value"	// the values
		"color_warning"		"SpecInfo_Warning"	// a value that is bad news (broken legs)
	}
}
