//=========================================================
// SBO 2.0 map music (docs/MAP_MUSIC.md): ambient_music / an ambient_generic with an .mp3 (dlls/sound.cpp). The
// server sends the track state in the message "MapMusic"; the client library (cl_dll/map_music.cpp) runs the
// engine's "mp3" command for it locally. The path becomes part of that console command, so both sides check it
// with the same rule, and the client never trusts the server's.
//
// MapMusic: byte state (MAPMUSIC_*), byte serial (a new number for every start, so a repeat of the same state -- a
// late joiner's or a fullupdate's -- does not start the track over), string path ("sound/....mp3", empty for stop).
//=========================================================

#ifndef MAP_MUSIC_SHARED_H
#define MAP_MUSIC_SHARED_H

enum
{
	MAPMUSIC_STOP = 0,
	MAPMUSIC_PLAY,		// once
	MAPMUSIC_LOOP
};

// The longest path + 1, "sound/" included: the engine's resource names (download list) are under 64 characters
#define MAPMUSIC_PATH_MAX	64

// Under sound/, ending in .mp3, letters, digits and / _ - . only (no blank, quote, semicolon or line end), no "..",
// shorter than MAPMUSIC_PATH_MAX.
inline int MapMusic_IsSafePath( const char *path )
{
	int length = 0;

	if ( !path || path[0] != 's' || path[1] != 'o' || path[2] != 'u' || path[3] != 'n' || path[4] != 'd' || path[5] != '/' )
		return 0;

	for ( ; path[ length ]; length++ )
	{
		const char ch = path[ length ];

		if ( length >= MAPMUSIC_PATH_MAX - 1 )
			return 0;

		if ( !( ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ( ch >= '0' && ch <= '9' )
			|| ch == '/' || ch == '_' || ch == '-' || ch == '.' ) )
			return 0;

		// no way out of the game directory
		if ( ch == '.' && path[ length + 1 ] == '.' )
			return 0;
	}

	// "sound/x.mp3" at least
	if ( length < 11 )
		return 0;

	const char *ext = path + length - 4;
	return ext[0] == '.' && ( ext[1] | 0x20 ) == 'm' && ( ext[2] | 0x20 ) == 'p' && ext[3] == '3';
}

#endif // MAP_MUSIC_SHARED_H
