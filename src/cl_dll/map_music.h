//=========================================================
// SBO 2.0 map music, client side (map_music.cpp, docs/MAP_MUSIC.md): the server's "MapMusic" message runs the
// engine's "mp3" command here.
//=========================================================

#if !defined( MAP_MUSIC_CLIENT_H )
#define MAP_MUSIC_CLIENT_H

void MapMusic_Init( void );		// hooks the message
void MapMusic_Reset( void );	// VidInit / InitHUD: a new map or server; the next state is run whatever it is
void MapMusic_Redraw( void );	// CHud::Redraw: runs the command of the last message

#endif // MAP_MUSIC_CLIENT_H
