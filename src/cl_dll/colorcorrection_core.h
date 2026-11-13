//=========================================================
// SBO 2.0 color_correction / color_correction_volume, client side: the "ColorCorr" message, the lookup table readers
// (.raw, .cube), the simple keys, the weights and the blend, without engine headers (tests/colorcorrection/run_test.sh
// runs them with ASan/UBSan on good, cut and hostile input). Format: game_shared/colorcorrection_shared.h;
// docs/COLOR_CORRECTION.md.
//=========================================================

#if !defined( COLORCORRECTION_CORE_H )
#define COLORCORRECTION_CORE_H

#include <vector>

#include "colorcorrection_shared.h"

namespace colorcorr
{
struct Params
{
	int		index;				// entity index, 1 .. 2047
	int		flags;				// COLORCORR_FLAG_*
	float	maxWeight;			// 0 .. 1
	float	weightNow;			// point: the weight when the message was sent
	float	fadeIn, fadeOut;	// >= 0 (a volume: both its fade time)
	float	origin[3];
	float	minFalloff, maxFalloff;	// point: -1 = no falloff
	int		model;				// volume: brush model index
	float	saturation, contrast, brightness, gamma;
	int		tint[3];			// 0 .. 255
	float	tintAmount;			// 0 .. 1
	char	file[ COLORCORR_MAX_PATH ];	// "" = none
};

// Reads one message. False: cut short or an index out of range; nothing usable.
bool Parse( const unsigned char *data, int size, Params &out );

// A 3D lookup table: size^3 RGB bytes, red fastest, then green, then blue (Source's .raw order).
struct Lut
{
	int							size = 0;
	std::vector<unsigned char>	rgb;
};

constexpr int RAW_SIZE = 32;							// Source .raw: 32^3
constexpr int RAW_BYTES = RAW_SIZE * RAW_SIZE * RAW_SIZE * 3;	// 98,304
constexpr int CUBE_MIN_SIZE = 2;
constexpr int CUBE_MAX_SIZE = 64;

// Source's .raw (exactly RAW_BYTES). False with a reason in err.
bool ReadRaw( const unsigned char *data, int len, Lut &out, char *err, int errLen );
// Adobe / Resolve .cube: LUT_3D_SIZE 2 .. 64, domain 0 .. 1, size^3 lines of three values 0 .. 1 (red fastest).
// False with a reason (and the line) in err.
bool ReadCube( const unsigned char *data, int len, Lut &out, char *err, int errLen );
// By the path's extension. False with a reason in err.
bool ReadTable( const char *path, const unsigned char *data, int len, Lut &out, char *err, int errLen );

// Any simple key away from its neutral value (saturation 1, contrast 1, brightness 0, gamma 1, tint amount 0)
bool HasSimpleKeys( const Params &p );
// The simple keys on one color (0 .. 1 each), in this order: saturation, contrast, brightness, gamma, tint.
void ApplySimpleKeys( const Params &p, float rgb[3] );
// The entity's table: the file's (if any) with the simple keys over it, else a 32^3 table of the simple keys
void BuildLut( const Params &p, const Lut *file, Lut &out );
// A table that changes nothing
void IdentityLut( int size, Lut &out );

// Point entity: the weight w scaled by the distance from the view (Source C_ColorCorrection: full weight up to the
// min falloff, none from the max falloff, linear between; -1 or min == max: no falloff)
float FalloffWeight( const Params &p, float w, const float view[3] );

// A weight moving from w0 toward target at rate per second (rate <= 0: there at once), dt seconds later
float Approach( float w0, float target, float rate, float dt );
// The rate of a fade of the given time to or from the maximum weight (time <= 0: at once, returns 0)
float FadeRate( float maxWeight, float time );

// The blend of n weighted tables: an exclusive one with weight E leaves the others (1 - E) of theirs, the
// COLORCORR_BLEND_MAX heaviest are used, a sum above 1 is scaled to 1, the rest is the picture unchanged.
// Writes the chosen entries' positions (into the input arrays) and weights, returns how many; identity gets 1 - sum.
constexpr int BLEND_MAX = 4;
int Blend( const float *weights, const bool *exclusive, int n, int outIndex[ BLEND_MAX ], float outWeight[ BLEND_MAX ],
	float &identity );
} // namespace colorcorr

#endif // COLORCORRECTION_CORE_H
