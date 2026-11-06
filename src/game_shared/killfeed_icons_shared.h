//=========================================================
// SBO 2.0: killfeed icons set by server plugins (docs/KILLFEED_ICONS.md). The server keeps a table "weapon code ->
// picture" (dlls/killfeed_icons.cpp, server command "killfeed_icon"); each client adds every entry to its HUD sprite
// list as d_<code> (cl_dll/killfeed_icons.cpp), where both kill feeds look up the DeathMsg / SboDeathMsg weapon code.
//
// "KfIcon" message, one icon, at most KFICON_MSG_MAX bytes:
//   BYTE   kind     KFICON_REMOVE, KFICON_SHEET or KFICON_REUSE
//   STRING code     the DeathMsg weapon code (no "d_"), 1 .. KFICON_CODE_MAX characters
//   STRING source   KFICON_SHEET: sprite path (sprites/....spr, shorter than KFICON_PATH_LEN)
//                   KFICON_REUSE: another icon's code (d_<code> of hud.txt or another custom icon)
//                   KFICON_REMOVE: ""
//   KFICON_SHEET only: SHORT x, y, w, h (the icon's rectangle in the sprite)
//=========================================================

#ifndef KILLFEED_ICONS_SHARED_H
#define KILLFEED_ICONS_SHARED_H

#define KFICON_MAX			32		// icons in the table (and extra slots in the client's HUD sprite list)
#define KFICON_CODE_MAX		21		// characters: "d_" + 21 + NUL fills the HUD's 24-character sprite names
#define KFICON_PATH_LEN		64		// sprite path buffer, NUL included
#define KFICON_RECT_MAX		256		// largest width / height of an icon
#define KFICON_MSG_MAX		( 1 + ( KFICON_CODE_MAX + 1 ) + KFICON_PATH_LEN + 4 * 2 )

enum
{
	KFICON_REMOVE = 0,
	KFICON_SHEET = 1,
	KFICON_REUSE = 2,
};

// a weapon code: 1 .. KFICON_CODE_MAX of A-Z a-z 0-9 _ -
inline bool KfIcon_ValidCode( const char *code )
{
	if ( !code || !code[0] )
		return false;
	int n = 0;
	for ( const char *p = code; *p; p++, n++ )
	{
		const char c = *p;
		if ( n >= KFICON_CODE_MAX )
			return false;
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) )
			return false;
	}
	return true;
}

// a sheet path: "sprites/" ... ".spr", shorter than KFICON_PATH_LEN, printable, no "..", '\\', ':' or quotes
inline bool KfIcon_ValidPath( const char *path )
{
	if ( !path )
		return false;
	int n = 0;
	for ( const char *p = path; *p; p++, n++ )
	{
		const char c = *p;
		if ( n >= KFICON_PATH_LEN - 1 || c <= ' ' || c > '~' || c == '\\' || c == ':' || c == '"' )
			return false;
		if ( c == '.' && p[1] == '.' )
			return false;
	}
	static const char prefix[] = "sprites/";
	for ( int i = 0; prefix[i]; i++ )
		if ( path[i] != prefix[i] )
			return false;
	return n > 12 && path[n - 4] == '.' && ( path[n - 3] | 32 ) == 's' && ( path[n - 2] | 32 ) == 'p' && ( path[n - 1] | 32 ) == 'r';
}

#endif // KILLFEED_ICONS_SHARED_H
