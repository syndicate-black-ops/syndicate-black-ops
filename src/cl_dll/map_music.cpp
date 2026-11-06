//=========================================================
// SBO 2.0 map music, client side (docs/MAP_MUSIC.md; server: dlls/sound.cpp). SevenKewp sends the engine's console
// command "mp3 ..." to the clients in SVC_STUFFTEXT. HL25's client checks such server commands (ValidStuffText in
// the engine) and drops a whole line that contains one of its words anywhere ("exec", "exit", and with
// cl_filterstuffcmd 1 also "rate", "volume", ...), so a track named "pirate.mp3" would never play. So the server
// sends the state in a message, and this runs the command locally: pfnClientCmd puts it in this client's own
// command buffer, which the engine does not filter (as trigger_sequence's SeqSpeak, hud.cpp). The path comes from
// the server, so it is checked here whatever the server checked (game_shared/map_music_shared.h); anything else is
// dropped without a word.
//
// The command is run from the HUD's redraw, not from the message: when a client enters a map, HL25's GameUI stops
// its menu music with its own "mp3 stop" (CGameUI::ConnectToServer, which the engine calls from CL_CheckClientState
// when the connection becomes active, in the frame after the one that read the join messages). Run from the message,
// "mp3 loop" went into the command buffer before that stop and the track stopped at once (seen on the HL25 Linux
// client). The engine draws the HUD only once the client is active, so a command put in the buffer there runs after
// GameUI's stop.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "map_music.h"
#include "map_music_shared.h"	// game_shared

#include <string.h>
#include <stdio.h>

// what this client was last told; s_iState -1: nothing yet on this map / server
static int s_iState = -1;
static int s_iSerial = -1;
static char s_szPath[ MAPMUSIC_PATH_MAX ];

// the command to run at the next redraw (the last message's)
static char s_szCommand[ MAPMUSIC_PATH_MAX + 16 ];

static int MsgFunc_MapMusic( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int state = READ_BYTE();
	const int serial = READ_BYTE();
	const char *path = READ_STRING();

	if ( state == MAPMUSIC_STOP )
	{
		if ( s_iState != MAPMUSIC_STOP )
			strcpy( s_szCommand, "mp3 stop\n" );

		s_iState = MAPMUSIC_STOP;
		s_iSerial = serial;
		s_szPath[0] = '\0';
		return 1;
	}

	if ( ( state != MAPMUSIC_PLAY && state != MAPMUSIC_LOOP ) || !MapMusic_IsSafePath( path ) )
		return 1;

	// the same start again (a late joiner's state, a fullupdate): the track plays on
	if ( state == s_iState && serial == s_iSerial && !strcmp( path, s_szPath ) )
		return 1;

	snprintf( s_szCommand, sizeof( s_szCommand ), "mp3 %s %s\n", state == MAPMUSIC_LOOP ? "loop" : "play", path );
	s_szCommand[ sizeof( s_szCommand ) - 1 ] = '\0';

	s_iState = state;
	s_iSerial = serial;
	strncpy( s_szPath, path, sizeof( s_szPath ) - 1 );
	s_szPath[ sizeof( s_szPath ) - 1 ] = '\0';
	return 1;
}

void MapMusic_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "MapMusic", MsgFunc_MapMusic );
}

void MapMusic_Redraw( void )
{
	if ( !s_szCommand[0] )
		return;

	gEngfuncs.pfnClientCmd( s_szCommand );
	s_szCommand[0] = '\0';
}

void MapMusic_Reset( void )
{
	s_iState = -1;
	s_iSerial = -1;
	s_szPath[0] = '\0';
	s_szCommand[0] = '\0';	// nothing from the last map
}
