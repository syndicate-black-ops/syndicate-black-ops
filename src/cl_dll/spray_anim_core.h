//=========================================================
// SBO 2.0: animated sprays: the frames packed in a player's tempdecal.wad. docs/ANIMATED_SPRAYS.md.
//
// A spray (tempdecal.wad) has exactly one lump, a miptex with its palette (the engine checks that and draws its first
// mip). The engine accepts a lump larger than the image and palette need, so the frames after the first one go there,
// in a block after the palette. The engine uploads and forwards the whole file and draws the first frame; the client
// (spray_anim.cpp) shows the others. No SDK headers here: tests/spray_anim builds this file alone.
//=========================================================

#if !defined( SPRAY_ANIM_CORE_H )
#define SPRAY_ANIM_CORE_H

#define SPRAYANIM_MAX_FRAMES	64
#define SPRAYANIM_MAX_SIDE		256		// the engine's limit for a spray's width and height
#define SPRAYANIM_MIN_DELAY		20		// ms; shorter delays (and 0) play at SPRAYANIM_DEFAULT_DELAY, as browsers do
#define SPRAYANIM_DEFAULT_DELAY	100
#define SPRAYANIM_MAX_DELAY		10000

struct SprayAnimInfo
{
	int width, height;
	int frames;									// 2 .. SPRAYANIM_MAX_FRAMES
	int delay[SPRAYANIM_MAX_FRAMES];			// ms, clamped
	int totalTime;								// ms, sum of the delays
	const unsigned char *frame0;				// width * height palette indices (the first mip)
	const unsigned char *palette;				// 256 RGB entries, as in the file; index 255 is transparent
	const unsigned char *data[SPRAYANIM_MAX_FRAMES];	// frames 1 .. frames - 1: the coded changes from the frame before
	int length[SPRAYANIM_MAX_FRAMES];
};

// Reads a tempdecal.wad (size bytes). True: an animated spray, every frame checked; info points into file.
// False: not a WAD3 spray, no animation block, or a damaged one (shown as the still spray).
bool SprayAnim_Parse( const unsigned char *file, int size, SprayAnimInfo &info );

// Turns pixels (width * height indices of frame - 1) into frame (1 .. frames - 1). False on a damaged frame (Parse
// rejects those).
bool SprayAnim_Decode( const SprayAnimInfo &info, int frame, unsigned char *pixels );

// The frame shown time ms after the start, looping.
int SprayAnim_FrameAt( const SprayAnimInfo &info, unsigned int time );

#endif // SPRAY_ANIM_CORE_H
