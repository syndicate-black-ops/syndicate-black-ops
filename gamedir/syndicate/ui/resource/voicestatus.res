// Voice labels layout: the players who are talking. Sizes in 640x480 units, scaled with the screen.
// Scheme: ClientScheme (font "Default", "Orange" for empty slots).
"ui/resource/voicestatus.res"
{
	// Talking players (you included), newest at the top; the list grows upwards from the bottom of the panel.
	// Row: Steam avatar (ui/gfx/default_avatar.tga without one) and name, on the player's team color (alpha 128).
	HudVoiceStatus
	{
		"fieldName" "HudVoiceStatus"
		"visible" "1"
		"enabled" "1"
		"xpos" "r145"
		"ypos" "0"
		"wide" "145"
		"tall" "400"

		"item_wide"	"135"
		"item_tall"	"16"

		"show_avatar"		"1"
		"show_friend"		"0"

		// avatar at the left edge, 1 unit inside the 16-unit row
		"avatar_xpos"		"1"
		"avatar_ypos"		"1"
		"avatar_wide"		"14"
		"avatar_tall"		"14"

		// name 7 units right of the avatar (1 + 14 + 7)
		"text_xpos"			"22"

		"fade_in_time"		"0"
		"fade_out_time"		"0.15"
	}
}
