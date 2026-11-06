//========= SBO world screens: shared by the server (vgui_screen.cpp) and the client (worldscreen.cpp) =========//
//
// The WScreen user message and the vgui_screen_command client command (docs/WORLD_SCREENS.md, "Network").
// Plain C, no includes.
//=============================================================================================================//

#ifndef VGUI_SCREEN_SHARED_H
#define VGUI_SCREEN_SHARED_H

// WScreen: the first byte is the operation
#define WSCREEN_OP_DEFINE		0	// short entindex, coord origin[3], short angles[3] (360/65536 deg), coord width,
									// coord height, short inputdistance, byte flags, string panelname;
									// with WSCREEN_FLAG_BOUND also: short carrier entindex, short carrier submodel
									// (the N of its model "*N");
									// with WSCREEN_FLAG_MOVIE also: string moviefilename (a name in video/, VIDEO_MAX_PATH),
									// long milliseconds since the movie started, byte volume (0..10), short radius
#define WSCREEN_OP_STATE		1	// short entindex, byte flags
#define WSCREEN_OP_CONTROL		2	// short entindex, byte control op, string control name, string value
									// (WSCREEN_CONTROL_ANIMATION: string state group, may be "", string sequence)

// flags
#define WSCREEN_FLAG_ACTIVE		(1 << 0)	// drawn and usable
#define WSCREEN_FLAG_NOCURSOR	(1 << 1)	// "showcursor" 0: no cursor while aimed at it
#define WSCREEN_FLAG_BOUND		(1 << 2)	// "surface": shown on the carrier brush entity's WSCREEN_TOOL_TEXTURE faces
											// (origin / angles / width / height unused)
#define WSCREEN_FLAG_NOINPUT	(1 << 3)	// spawnflag "Input Disabled": display only (no aim, cursor or clicks; fire
											// and secondary fire work as usual; the server refuses its commands)
#define WSCREEN_FLAG_MOVIE		(1 << 4)	// "moviefilename": an Ogg Theora video from video/ instead of a panel (no input)
#define WSCREEN_FLAG_LOOP		(1 << 5)	// "looping" 1: the movie starts over at its end
#define WSCREEN_FLAG_SOUND2D	(1 << 6)	// spawnflag "Movie sound everywhere": same volume everywhere (no falloff)
#define WSCREEN_FLAG_NOSOUND	(1 << 7)	// spawnflag "Movie without sound"

// The tool texture that marks a carrier's screen faces (gamedir/syndicate/sbo_tools.wad; all palette index 255, so
// the faces are invisible with the carrier's render mode Solid). Compared case-insensitively.
#define WSCREEN_TOOL_TEXTURE	"{screen"

// control ops (vgui_control_proxy "mode")
#define WSCREEN_CONTROL_TEXT	0	// value = the new text of a Label / Button
#define WSCREEN_CONTROL_ENABLED	1	// value "1" / "0"
#define WSCREEN_CONTROL_VISIBLE	2	// value "1" / "0"
#define WSCREEN_CONTROL_ANIMATION	3	// value = a sequence of the layout's animation script (<panelname>_anims.txt);
										// the "control" is the state group it is kept under for players who join later
										// ("" = not kept)
// A text sent to a control that has a value (ProgressBar, ContinuousProgressBar, AnalogBar, ContinuousAnalogBar,
// Slider) is that value in percent: a number, 0 .. 100 (clamped).

// A button "command" may list several commands separated by ','. "anim:<sequence>" starts a sequence of the
// layout's animation script on this player's screen only (no server round trip); the others go to the server.
#define WSCREEN_LOCAL_ANIM_PREFIX	"anim:"

// limits (bytes with the terminator); a DEFINE is at most 1 + 2 + 6 + 6 + 4 + 2 + 1 + 32 + 4 = 58 bytes, a CONTROL
// 1 + 2 + 1 + 32 + 128 = 164 (under the engine's 192)
#define WSCREEN_MAX_PANELNAME	32
#define WSCREEN_MAX_CONTROLNAME	32
#define WSCREEN_MAX_VALUE		128
#define WSCREEN_MAX_COMMAND		64

#define WSCREEN_DEFAULT_INPUT_DISTANCE	128
#define WSCREEN_MAX_INPUT_DISTANCE		4096

// A panel name or a button command may only have these characters (no quotes, ';', spaces at the ends...):
// letters, digits, '_', '-'; a command may also have '.', ':' (Source's "submit:<entry>") and single inner spaces.
// A panel name becomes a file name (ui/scripts/screens/<name>.res): no '.', so no "..".
static inline int WScreen_ValidName( const char *s, int maxLen, int isCommand )
{
	int n = 0;
	if ( !s || !s[0] )
		return 0;
	for ( ; s[n]; n++ )
	{
		const char c = s[n];
		if ( n >= maxLen - 1 )
			return 0;
		if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' )
			continue;
		if ( isCommand && ( c == '.' || c == ':' || ( c == ' ' && n > 0 && s[n + 1] && s[n + 1] != ' ' ) ) )
			continue;
		return 0;
	}
	return 1;
}

#endif // VGUI_SCREEN_SHARED_H
