//=========================================================
// SBO 2.0 env_overlay (docs/ENV_OVERLAY.md): the "Overlay" user message, server dlls/env_overlay.cpp, client
// cl_dll/overlay.cpp. One reliable message per channel change; server plugins may send it too (the doc has the
// format for them). The client draws the overlays under the HUD.
//
//   BYTE   channel: 1 .. OVERLAY_CHANNELS; 0 = every channel (only with OVERLAY_FLAG_ON clear: all off)
//   BYTE   OVERLAY_FLAG_* flags
//   without OVERLAY_FLAG_ON: nothing more. The channel's overlay fades out over its own fade-out time, or goes at
//   once with OVERLAY_FLAG_NOW.
//   with OVERLAY_FLAG_ON (replaces whatever the channel shows):
//   STRING image: a precached .spr (PRECACHE_MODEL) or a .tga the client has (PRECACHE_GENERIC); "" = no image, a
//          plain color (needs OVERLAY_FLAG_FULLSCREEN)
//   SHORT  x * OVERLAY_POS_UNIT: 0 .. 1 left edge from the left of the screen, -1 centered, other negative values:
//          right edge from the right (game_text's x)
//   SHORT  y * OVERLAY_POS_UNIT: as x, top / bottom
//   SHORT  scale * OVERLAY_SCALE_UNIT (1 = the image's own size in pixels at hud_scale 0)
//   BYTE   red, BYTE green, BYTE blue, BYTE amount (0 .. 255)
//   SHORT  frame rate * OVERLAY_TIME_UNIT (frames per second of an animated .spr; 0 = the first frame)
//   SHORT  fade-in time * OVERLAY_TIME_UNIT (unsigned)
//   SHORT  hold time * OVERLAY_TIME_UNIT (unsigned)
//   SHORT  fade-out time * OVERLAY_TIME_UNIT (unsigned)
//   SHORT  time already shown * OVERLAY_ELAPSED_UNIT (unsigned; a resend to a client that joined or got its HUD
//          again goes on where the others are)
//=========================================================

#ifndef OVERLAY_SHARED_H
#define OVERLAY_SHARED_H

#define OVERLAY_MSG_NAME		"Overlay"

#define OVERLAY_CHANNELS		8		// channels 1 .. 8

#define OVERLAY_FLAG_ON			1		// show (else: turn the channel off)
#define OVERLAY_FLAG_FULLSCREEN	2		// stretched over the whole screen (x, y and scale unused)
#define OVERLAY_FLAG_LOOP		4		// fade in, hold, fade out, again, until turned off
#define OVERLAY_FLAG_ADDITIVE	8		// added to the picture (light); else blended by its own transparency
#define OVERLAY_FLAG_STAY		16		// after the fade in it stays until turned off (the hold time is not used)
#define OVERLAY_FLAG_NOW		32		// off: gone at once, no fade out

#define OVERLAY_POS_UNIT		10000.0f	// x, y
#define OVERLAY_SCALE_UNIT		100.0f		// scale: 0.01 .. 327.67
#define OVERLAY_TIME_UNIT		100.0f		// frame rate, fade in, hold, fade out: 0 .. 655.35
#define OVERLAY_ELAPSED_UNIT	10.0f		// time already shown: 0 .. 6553.5 s

#define OVERLAY_MAX_PATH		64		// image path with its terminator

#endif // OVERLAY_SHARED_H
