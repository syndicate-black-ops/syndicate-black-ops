//=========================================================
// SBO 2.0 env_overlay, client side: the "Overlay" message, the fades and the placement, without engine headers
// (tests/overlay/run_test.sh runs them with ASan/UBSan on good, cut and hostile messages). Format:
// game_shared/overlay_shared.h; docs/ENV_OVERLAY.md.
//=========================================================

#if !defined( OVERLAY_CORE_H )
#define OVERLAY_CORE_H

#include "overlay_shared.h"

namespace overlay
{
struct Params
{
	int		channel;			// 1 .. OVERLAY_CHANNELS; 0 = every channel (off only)
	int		flags;				// OVERLAY_FLAG_*
	char	image[ OVERLAY_MAX_PATH ];	// "" = a plain color
	float	x, y;				// game_text's x / y (-1 centered)
	float	scale;				// > 0
	int		r, g, b, a;			// 0 .. 255
	float	framerate;			// >= 0
	float	fadein, hold, fadeout;	// >= 0
	float	elapsed;			// >= 0: how long it has been shown already
};

// Reads one message. False: cut short, a channel out of range, or "on" for every channel; nothing usable.
bool Parse( const unsigned char *data, int size, Params &out );

// Opacity (0 .. 1, times a / 255 left to the caller) t seconds after the start. Below 0: over (a timed one ended).
float Alpha( const Params &p, float t );

// Opacity of an overlay turned off with opacity a0, dt seconds ago. Below 0: gone.
float AlphaOff( const Params &p, float a0, float dt );

// game_text's placement (cl_dll/message.cpp XPosition / YPosition) of something size pixels long on a screen of
// screen pixels: pos -1 centered, 0 .. 1 from the start, other negative values from the end; kept on the screen when it
// fits.
int Place( float pos, int size, int screen );

// The frame of an animated sprite t seconds after the start (frames > 0).
int Frame( const Params &p, float t, int frames );
} // namespace overlay

#endif // OVERLAY_CORE_H
