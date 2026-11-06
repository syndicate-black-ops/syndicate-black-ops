//=========================================================
// SBO 2.0 game_playvideo (docs/VIDEO_PLAYER.md, port of Aura-SE's dlls/maprules.cpp CGamePlayVideo). The server
// sends "PlayVideo" to the activator or to everybody; the client (cl_dll/video/video_player.cpp) plays the file full
// screen. Both sides check the names with the same rule, and the client never trusts the server's.
//
// PlayVideo (Aura's message): string video, string audio. Names inside the mod's video/ folder ("intro.ogv"); the
// audio string is empty for the video's own sound; an empty video string stops a video that plays.
//=========================================================

#ifndef VIDEO_SHARED_H
#define VIDEO_SHARED_H

#include <stddef.h>

// The longest path + 1, "video/" included: the engine's resource names (download list) are under 64 characters
#define VIDEO_MAX_PATH	64
#define VIDEO_FOLDER	"video/"

// A name inside video/: letters, digits and / _ - . only (no blank, quote, backslash or colon), no "..", no "/" at the
// start or end or twice in a row, exactly one "." (the engine downloads only files whose path has one dot followed by
// a 3-letter extension: "a-b.c.ogv" never reaches a player), ending in .ogv (video) or .ogg (sound), "video/" + name
// shorter than VIDEO_MAX_PATH. NULL for a valid name, else why it is refused (for the console error).
inline const char *VideoShared_NameError( const char *name, bool video )
{
	if ( !name || !name[0] )
		return "no file name";

	size_t length = 0;
	int dots = 0;
	for ( ; name[length]; length++ )
	{
		const char ch = name[length];

		if ( length + sizeof( VIDEO_FOLDER ) - 1 >= VIDEO_MAX_PATH - 1 )
			return "too long (at most 57 characters)";

		if ( !( ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ( ch >= '0' && ch <= '9' )
			|| ch == '/' || ch == '_' || ch == '-' || ch == '.' ) )
			return "only letters, digits and / _ - . are allowed";

		// no way out of video/
		if ( ch == '.' && name[length + 1] == '.' )
			return "\"..\" is not allowed";
		if ( ch == '/' && ( length == 0 || name[length + 1] == '/' || name[length + 1] == 0 ) )
			return "a \"/\" at the start, at the end or twice in a row";
		if ( ch == '.' )
			dots++;
	}

	const char *ext = name + length - 4;
	const char last = video ? 'v' : 'g';
	// "x.ogv" at least
	if ( length < 5 || !( ext[0] == '.' && ( ext[1] | 0x20 ) == 'o' && ( ext[2] | 0x20 ) == 'g'
		&& ( ext[3] | 0x20 ) == last ) || ext[-1] == '/' )
		return video ? "not a .ogv file name" : "not a .ogg file name";

	if ( dots != 1 )
		return "more than one \".\": the engine never sends such a file to players; use one dot, before the extension";

	return NULL;
}

inline bool VideoShared_IsValidName( const char *name, bool video )
{
	return VideoShared_NameError( name, video ) == NULL;
}

// "video/<name>" into path (at least VIDEO_MAX_PATH bytes); false and an empty path for a name that is not valid
inline bool VideoShared_MakePath( const char *name, bool video, char *path, size_t size )
{
	if ( !path || size == 0 )
		return false;
	path[0] = 0;
	if ( size < VIDEO_MAX_PATH || !VideoShared_IsValidName( name, video ) )
		return false;

	size_t n = 0;
	for ( const char *p = VIDEO_FOLDER; *p; p++ )
		path[n++] = *p;
	for ( const char *p = name; *p; p++ )
		path[n++] = *p;
	path[n] = 0;
	return true;
}

#endif // VIDEO_SHARED_H
