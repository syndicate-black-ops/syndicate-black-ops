//=========================================================
// SBO 2.0: game_playvideo, the client's full-screen video player (video_player.h, docs/VIDEO_PLAYER.md).
//
// Port of Aura-CL's CVideoPlayer and its hooks (cl_dll/video/video_player.cpp; hud.cpp __MsgFunc_PlayVideo and
// Cmd_PlayVideo; hud_redraw.cpp; input.cpp HUD_Key_Event / CL_CreateMove): the server's "PlayVideo" message names a
// video in the mod's video/ folder (and optionally a separate sound file); the picture is drawn over the whole screen
// at the end of the HUD redraw until the video ends; while it plays the player stands still. Changes from Aura:
// - the files are read through the engine's file system (COM_LoadFile), so videos a server sent (FastDL, in
//   syndicate_downloads) are found; the names are checked (game_shared/video_shared.h: inside video/, no "..")
// - the sound is the video's own Vorbis track (or the separate file), played through the engine's SDL2
//   (video_audio.cpp) instead of OpenAL; its volume follows the engine's "volume"
// - Esc stops the video (Aura's skip, which is commented out there): HL25 opens GameUI on Esc before HUD_Key_Event,
//   so while a video plays GameUI is hidden again at once and that press stops it (vgui2/client_viewport.cpp, as
//   BHL's chat keeps Esc from opening GameUI)
// - the video stops when the player dies (alive when it started) and on a new map or server (VidInit)
// - the console command is test_cl_playvideo, in test builds only
//=========================================================

// before cl_util.h: its fabs macro breaks the C++ library headers (<vector>)
#include "video_decoder.h"

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "usercmd.h"
#include "keydefs.h"

#include "video_player.h"
#include "video_gl.h"
#include "video_audio.h"
#include "video_shared.h"	// game_shared
#include "vgui2/vgui2_api.h"	// Esc: VGui2_Video_SetEscapeGuard

#include <string.h>

int CL_IsDead( void );	// input.cpp

namespace
{
CVideoDecoder	s_Decoder;
byte			*s_pFile = NULL;		// the video file (COM_LoadFile); the decoder reads it in place
bool			s_bPlaying = false;
bool			s_bChanged = false;		// a new picture since the last draw
float			s_flStart = -1.0f;		// HUD time of the first frame; -1: set at the next redraw
bool			s_bWasAlive = false;	// alive when it started (or since): dying stops the video
char			s_szName[VIDEO_MAX_PATH];

// sound queued ahead in the device, seconds
const float AUDIO_AHEAD = 0.25f;

void FreeFile( void )
{
	if ( s_pFile )
	{
		gEngfuncs.COM_FreeFile( s_pFile );
		s_pFile = NULL;
	}
}

void Stop( const char *why )
{
	if ( !s_bPlaying && !s_pFile )
		return;

	if ( s_bPlaying && why )
		gEngfuncs.Con_Printf( "Video: %s stopped (%s, %d frames)\n", s_szName, why, s_Decoder.FramesDecoded() );

	s_bPlaying = false;
	VGui2_Video_SetEscapeGuard( false );
	VideoAudio_Close();
	s_Decoder.Close();
	FreeFile();
	VideoGL_Release();
	s_flStart = -1.0f;
	s_szName[0] = 0;
}

// A file of the game (engine file system: the mod folder, syndicate_downloads, ...); NULL when it is not there
byte *LoadFile( const char *path, int &length )
{
	length = 0;
	byte *data = gEngfuncs.COM_LoadFile( (char *)path, 5, &length );
	if ( data && ( length <= 0 || length > CVideoDecoder::MAX_FILE_BYTES ) )
	{
		gEngfuncs.COM_FreeFile( data );
		data = NULL;
	}
	return data;
}

bool Play( const char *video, const char *audio )
{
	Stop( "replaced" );

	char path[VIDEO_MAX_PATH];
	if ( !VideoShared_MakePath( video, true, path, sizeof( path ) ) )
	{
		gEngfuncs.Con_Printf( "Video: video name \"%s\" refused: %s\n", video ? video : "", VideoShared_NameError( video, true ) );
		return false;
	}

	int length = 0;
	s_pFile = LoadFile( path, length );
	if ( !s_pFile )
	{
		gEngfuncs.Con_Printf( "Video: %s not found\n", path );
		return false;
	}

	if ( !s_Decoder.Open( s_pFile, (size_t)length ) )
	{
		gEngfuncs.Con_Printf( "Video: %s: %s\n", path, s_Decoder.Error() ? s_Decoder.Error() : "cannot be read" );
		FreeFile();
		return false;
	}

	// a separate sound file replaces the video's own sound (Aura's "audio" key)
	if ( audio && audio[0] )
	{
		char audioPath[VIDEO_MAX_PATH];
		int audioLength = 0;
		byte *data = NULL;
		if ( !VideoShared_MakePath( audio, false, audioPath, sizeof( audioPath ) ) )
			gEngfuncs.Con_Printf( "Video: sound name \"%s\" refused: %s\n", audio, VideoShared_NameError( audio, false ) );
		else if ( ( data = LoadFile( audioPath, audioLength ) ) == NULL )
			gEngfuncs.Con_Printf( "Video: %s not found\n", audioPath );
		else
		{
			if ( !s_Decoder.OpenAudioFile( data, (size_t)audioLength ) )
				gEngfuncs.Con_Printf( "Video: %s is not Ogg Vorbis\n", audioPath );
			gEngfuncs.COM_FreeFile( data );
		}
	}

	const int rate = s_Decoder.AudioRate();
	const bool sound = rate > 0 && VideoAudio_Open( rate );

	strncpy( s_szName, path, sizeof( s_szName ) - 1 );
	s_szName[sizeof( s_szName ) - 1] = 0;
	s_bPlaying = true;
	s_bChanged = false;
	s_flStart = -1.0f;
	s_bWasAlive = !CL_IsDead();
	VGui2_Video_SetEscapeGuard( true );

	gEngfuncs.Con_Printf( "Video: playing %s (%dx%d, %.2f fps, %s%s%s; %d bytes)\n", path, s_Decoder.Width(),
		s_Decoder.Height(), s_Decoder.FrameRate(), sound ? "sound" : ( rate > 0 ? "no sound device" : "no sound" ),
		( !sound && rate > 0 ) ? ": " : "", ( !sound && rate > 0 ) ? VideoAudio_Error() : "", length );
	return true;
}

int MsgFunc_PlayVideo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	char video[VIDEO_MAX_PATH], audio[VIDEO_MAX_PATH];
	strncpy( video, READ_STRING(), sizeof( video ) - 1 );
	video[sizeof( video ) - 1] = 0;
	strncpy( audio, READ_STRING(), sizeof( audio ) - 1 );
	audio[sizeof( audio ) - 1] = 0;

	if ( !video[0] )
		Stop( "stopped by the server" );
	else
		Play( video, audio );
	return 1;
}

#ifdef SBO_TEST_COMMANDS
// test_cl_playvideo <video.ogv> [sound.ogg] (test builds only: SBO_TEST_COMMANDS; Aura's "playvideo"); no name: stop
void Cmd_PlayVideo( void )
{
	if ( gEngfuncs.Cmd_Argc() < 2 )
	{
		Stop( "test_cl_playvideo" );
		gEngfuncs.Con_Printf( "Usage: test_cl_playvideo <video.ogv> [sound.ogg] (in video/)\n" );
		return;
	}
	Play( gEngfuncs.Cmd_Argv( 1 ), gEngfuncs.Cmd_Argc() > 2 ? gEngfuncs.Cmd_Argv( 2 ) : "" );
}
#endif
}

void Video_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "PlayVideo", MsgFunc_PlayVideo );
#ifdef SBO_TEST_COMMANDS
	gEngfuncs.pfnAddCommand( "test_cl_playvideo", Cmd_PlayVideo );
#endif
}

void Video_Reset( void )
{
	Stop( "new map" );
}

void Video_Shutdown( void )
{
	Stop( NULL );
}

bool Video_IsPlaying( void )
{
	return s_bPlaying;
}

bool Video_KeyDown( int keynum )
{
	if ( !s_bPlaying || keynum != K_ESCAPE )
		return false;
	Stop( "Esc" );
	return true;
}

void Video_FilterMove( struct usercmd_s *cmd )
{
	if ( !s_bPlaying || !cmd )
		return;
	cmd->forwardmove = 0.0f;
	cmd->sidemove = 0.0f;
	cmd->upmove = 0.0f;
	cmd->buttons = 0;
	cmd->impulse = 0;
	cmd->weaponselect = 0;
}

void Video_Redraw( float time )
{
	if ( !s_bPlaying )
		return;

	if ( VGui2_Video_TakeEscape() )
	{
		Stop( "Esc" );
		return;
	}

	const bool dead = CL_IsDead() != 0;
	if ( s_bWasAlive && dead )
	{
		Stop( "the player died" );
		return;
	}
	if ( !dead )
		s_bWasAlive = true;

	// the clock starts at the first redraw (the message may come while the client is still connecting)
	if ( s_flStart < 0 || time < s_flStart )
		s_flStart = time;

	if ( s_Decoder.DecodeToTime( (double)( time - s_flStart ) ) )
		s_bChanged = true;

	if ( VideoAudio_IsOpen() )
	{
		const int rate = s_Decoder.AudioRate();
		float volume = gEngfuncs.pfnGetCvarFloat( "volume" );
		if ( volume < 0 )
			volume = 0;
		else if ( volume > 1 )
			volume = 1;
		const int gain = (int)( volume * 256 );

		short pcm[1024 * 2];
		while ( VideoAudio_Queued() < (int)( rate * AUDIO_AHEAD ) )
		{
			const int n = s_Decoder.ReadAudio( pcm, 1024 );
			if ( n <= 0 )
				break;
			for ( int i = 0; i < n * 2; i++ )
				pcm[i] = (short)( ( pcm[i] * gain ) >> 8 );
			VideoAudio_Queue( pcm, n );
		}
	}

	if ( s_Decoder.IsFinished() )
	{
		Stop( "finished" );
		return;
	}

	VideoGL_Draw( s_Decoder.RGBA(), s_Decoder.Width(), s_Decoder.Height(), s_bChanged );
	s_bChanged = false;
}
