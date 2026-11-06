// Kill feed layout. Sizes in 640x480 units, scaled with the screen.
// Colors: a color name from ClientScheme.res / ClientSchemeBase.res, or "r g b a".
"ui/resource/killfeed.res"
{
	"HudDeathNoticePanel"
	{
		"ControlName"		"CHudDeathNoticePanel"
		"fieldName"			"HudDeathNoticePanel"
		"visible"			"1"
		"enabled"			"1"
		"xpos"				"r640"
		"ypos"				"24"
		"wide"				"628"
		"tall"				"468"

		"hor_padding"		"12"		// Left and right padding of a row
		"icon_padding"		"2"			// Left and right padding of the kill icon
		"vert_margin"		"4"			// Distance between rows
		"row_height"		"13"		// Height of a row
		"text_font"			"Default"

		// Background color
		"color_bg"			"KF_Background"
		"color_bg_kill"		"KF_Background_Kill"	// rows where the local player is the killer
		"color_bg_death"	"KF_Background_Death"	// rows where the local player died

		// Icon color (also the weapon name text when hud.txt has no d_<weapon> icon, and the "HS" marker)
		"color_icon"		"KF_Icon"
		"color_icon_kill"	"KF_Icon_Kill"
		"color_icon_death"	"KF_Icon_Death"
		"color_icon_teamkill"	"KF_Icon_Teamkiill"

		// names without a team color (monsters, objects); player names use their team color
		"npc_name_color"	"KF_Name_NPC"
		"plain_name_color"	"KF_Name_Plain"
	}
}
