//=========================================================
// SBO 2.0: game_playvideo, the video's sound through SDL2 (video_audio.h, docs/VIDEO_PLAYER.md).
//
// Aura-CL plays the sound with its own OpenAL-soft library. SBO uses the SDL2 the engine already has loaded: a device
// of its own (SDL_OpenAudioDevice: the engine's own sound keeps its device), fed from the main thread with
// SDL_QueueAudio (SDL 2.0.4 and later; HL25 ships a newer SDL2). The functions are looked up at run time in the
// engine's SDL2 (Windows: SDL2.dll, GetModuleHandle; Linux: dlsym in the libraries already loaded), so no SDL header
// or import library is needed and nothing is shipped. Without them the video plays without sound.
//
// Compiled with SBO's client flags (sbo_compat.h); no SBO headers.
//=========================================================

#include "video_audio.h"

#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define VSDL_CALL __cdecl
#else
#include <dlfcn.h>
#define VSDL_CALL
#endif

namespace
{
typedef unsigned char VUint8;
typedef unsigned short VUint16;
typedef unsigned int VUint32;
typedef VUint32 VAudioDeviceID;

// SDL_AudioSpec (SDL_audio.h, SDL 2.0)
struct VAudioSpec
{
	int freq;
	VUint16 format;
	VUint8 channels;
	VUint8 silence;
	VUint16 samples;
	VUint16 padding;
	VUint32 size;
	void ( VSDL_CALL *callback )( void *userdata, VUint8 *stream, int len );
	void *userdata;
};

const VUint32 VSDL_INIT_AUDIO = 0x00000010;
const VUint16 VAUDIO_S16SYS = 0x8010;	// little endian: Windows and Linux x86

struct
{
	VUint32 ( VSDL_CALL *WasInit )( VUint32 flags );
	int ( VSDL_CALL *InitSubSystem )( VUint32 flags );
	VAudioDeviceID ( VSDL_CALL *OpenAudioDevice )( const char *device, int iscapture, const VAudioSpec *desired,
		VAudioSpec *obtained, int allowed_changes );
	void ( VSDL_CALL *PauseAudioDevice )( VAudioDeviceID dev, int pause_on );
	int ( VSDL_CALL *QueueAudio )( VAudioDeviceID dev, const void *data, VUint32 len );
	VUint32 ( VSDL_CALL *GetQueuedAudioSize )( VAudioDeviceID dev );
	void ( VSDL_CALL *CloseAudioDevice )( VAudioDeviceID dev );
	const char *( VSDL_CALL *GetError )( void );
} sdl;

bool s_bResolved = false;
bool s_bAvailable = false;
VAudioDeviceID s_iDevice = 0;
const char *s_pszError = "";

void *GetProc( const char *name )
{
#ifdef _WIN32
	HMODULE lib = GetModuleHandleA( "SDL2.dll" );
	return lib ? (void *)GetProcAddress( lib, name ) : NULL;
#else
	// the engine's SDL2 (client.so links it too): its handle without loading anything new. RTLD_DEFAULT does not
	// find it: hl_linux loads it into a local scope (seen on the HL25 Linux client).
	static void *s_pLib = NULL;
	if ( !s_pLib )
		s_pLib = dlopen( "libSDL2-2.0.so.0", RTLD_NOW | RTLD_NOLOAD );
	void *p = s_pLib ? dlsym( s_pLib, name ) : NULL;
	return p ? p : dlsym( RTLD_DEFAULT, name );
#endif
}

template <typename T> bool Resolve( T &fn, const char *name )
{
	fn = (T)GetProc( name );
	return fn != NULL;
}

bool Available( void )
{
	if ( !s_bResolved )
	{
		s_bResolved = true;
		s_bAvailable =
			Resolve( sdl.WasInit, "SDL_WasInit" ) &&
			Resolve( sdl.InitSubSystem, "SDL_InitSubSystem" ) &&
			Resolve( sdl.OpenAudioDevice, "SDL_OpenAudioDevice" ) &&
			Resolve( sdl.PauseAudioDevice, "SDL_PauseAudioDevice" ) &&
			Resolve( sdl.QueueAudio, "SDL_QueueAudio" ) &&
			Resolve( sdl.GetQueuedAudioSize, "SDL_GetQueuedAudioSize" ) &&
			Resolve( sdl.CloseAudioDevice, "SDL_CloseAudioDevice" );
		Resolve( sdl.GetError, "SDL_GetError" );
	}
	return s_bAvailable;
}
}

// Opens an SDL2 device for 16-bit stereo at rate; 0 (and s_pszError) when it fails
static VAudioDeviceID OpenDevice( int rate, int samples )
{
	s_pszError = "";
	if ( rate <= 0 )
		return 0;
	if ( !Available() )
	{
		s_pszError = "SDL2 audio functions not found";
		return 0;
	}

	// the engine's sound may not use SDL (Windows): start SDL's audio part once; it stays for the engine's lifetime
	if ( !( sdl.WasInit( VSDL_INIT_AUDIO ) & VSDL_INIT_AUDIO ) && sdl.InitSubSystem( VSDL_INIT_AUDIO ) != 0 )
	{
		s_pszError = sdl.GetError ? sdl.GetError() : "SDL_InitSubSystem failed";
		return 0;
	}

	VAudioSpec want;
	memset( &want, 0, sizeof( want ) );
	want.freq = rate;
	want.format = VAUDIO_S16SYS;
	want.channels = 2;
	want.samples = (VUint16)samples;
	want.callback = NULL;	// queued from the main thread

	// allowed_changes 0: SDL converts to what the sound card takes
	VAudioDeviceID dev = sdl.OpenAudioDevice( NULL, 0, &want, NULL, 0 );
	if ( !dev )
	{
		s_pszError = sdl.GetError ? sdl.GetError() : "SDL_OpenAudioDevice failed";
		return 0;
	}

	sdl.PauseAudioDevice( dev, 0 );
	return dev;
}

bool VideoAudio_Open( int rate )
{
	VideoAudio_Close();
	s_iDevice = OpenDevice( rate, 2048 );
	return s_iDevice != 0;
}

unsigned int VideoAudio_OpenDevice( int rate )
{
	return OpenDevice( rate, 1024 );
}

void VideoAudio_CloseDevice( unsigned int device )
{
	if ( device && Available() )
		sdl.CloseAudioDevice( device );
}

int VideoAudio_DeviceQueued( unsigned int device )
{
	return device ? (int)( sdl.GetQueuedAudioSize( device ) / 4 ) : 0;
}

void VideoAudio_DeviceQueue( unsigned int device, const short *pcm, int frames )
{
	if ( device && pcm && frames > 0 )
		sdl.QueueAudio( device, pcm, (VUint32)frames * 4 );
}

void VideoAudio_Close( void )
{
	if ( s_iDevice )
	{
		sdl.CloseAudioDevice( s_iDevice );
		s_iDevice = 0;
	}
}

const char *VideoAudio_Error( void )
{
	return s_pszError ? s_pszError : "";
}

bool VideoAudio_IsOpen( void )
{
	return s_iDevice != 0;
}

int VideoAudio_Queued( void )
{
	return s_iDevice ? (int)( sdl.GetQueuedAudioSize( s_iDevice ) / 4 ) : 0;
}

void VideoAudio_Queue( const short *pcm, int frames )
{
	if ( s_iDevice && pcm && frames > 0 )
		sdl.QueueAudio( s_iDevice, pcm, (VUint32)frames * 4 );
}
