//=========================================================
// SBO 2.0: game_playvideo, the video's sound (video_audio.cpp, docs/VIDEO_PLAYER.md). An SDL2 audio device of its
// own, fed from the main thread (SDL_QueueAudio). SDL2 is the engine's: HL25 loads it on Windows (SDL2.dll) and on
// Linux (libSDL2-2.0.so.0); its functions are looked up at run time, so nothing new is linked or shipped.
//=========================================================

#if !defined( VIDEO_AUDIO_H )
#define VIDEO_AUDIO_H

// Opens the device for 16-bit stereo at rate (SDL converts for the sound card). False without SDL audio: the video
// plays without sound.
bool VideoAudio_Open( int rate );
void VideoAudio_Close( void );
// Why the last VideoAudio_Open failed (SDL's message), "" when it did not
const char *VideoAudio_Error( void );
bool VideoAudio_IsOpen( void );

// Sample frames waiting in the device's queue
int VideoAudio_Queued( void );
// Interleaved 16-bit stereo frames to the end of the queue
void VideoAudio_Queue( const short *pcm, int frames );

// More devices (the world screens' mixer, video/video_mixer.cpp): an id, 0 when it cannot be opened
// (VideoAudio_Error says why). 1024-frame device buffer.
unsigned int VideoAudio_OpenDevice( int rate );
void VideoAudio_CloseDevice( unsigned int device );
int VideoAudio_DeviceQueued( unsigned int device );
void VideoAudio_DeviceQueue( unsigned int device, const short *pcm, int frames );

#endif // VIDEO_AUDIO_H
