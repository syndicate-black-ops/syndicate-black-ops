//=========================================================
// SBO 2.0: game_playvideo, the client's full-screen video player (docs/VIDEO_PLAYER.md). Port of Aura-CL's
// CVideoPlayer (cl_dll/video/video_player.cpp, The Phoenix Project Software).
//=========================================================

#if !defined( VIDEO_PLAYER_H )
#define VIDEO_PLAYER_H

struct usercmd_s;

void Video_Init( void );		// HUD init: the "PlayVideo" message (and test_cl_playvideo in test builds)
void Video_Reset( void );		// VidInit (a new map or server): a video that plays stops
void Video_Shutdown( void );	// the client library unloads
void Video_Redraw( float time );	// end of the HUD redraw: next frame, sound, the picture over the screen

bool Video_IsPlaying( void );
// Esc while a video plays: stops it. True when the key was used (the engine and the menus do not get it).
bool Video_KeyDown( int keynum );
// While a video plays the player stands still: no movement, buttons, impulse or weapon switch (as in Aura)
void Video_FilterMove( struct usercmd_s *cmd );

#endif // VIDEO_PLAYER_H
