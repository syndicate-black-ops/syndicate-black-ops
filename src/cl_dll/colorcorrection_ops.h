//=========================================================
// SBO 2.0 color correction editor: the operations a look is made of (Source's Color Correction Tools: curves, levels,
// selected HSV, color balance, lookup) and the 32 x 32 x 32 lookup table they make, without engine or VGUI2 headers
// (tests/colorcorrection/run_test.sh runs them with ASan/UBSan). The editor (vgui2/sbo_cc_editor.cpp) shows the table
// live through ColorCorr_SetEditorLook and saves it as a Source .raw file, the format color_correction reads.
// docs/COLOR_CORRECTION.md "Editor".
//=========================================================

#if !defined( COLORCORRECTION_OPS_H )
#define COLORCORRECTION_OPS_H

#include <string>
#include <vector>

#include "colorcorrection_core.h"
#include "vgui2/controls/sbo_curve.h"

namespace colorcorr
{
namespace ops
{
enum Type
{
	OP_CURVES = 0,
	OP_LEVELS,
	OP_SELECTED_HSV,
	OP_BALANCE,
	OP_LOOKUP,
	OP_COUNT
};

//! "Curves", "Levels", "Selected HSV", "Color Balance", "Lookup"
const char *TypeName( int type );

// Channels of curves and levels: all three together, then each one
enum Channel
{
	CH_RGB = 0,
	CH_RED,
	CH_GREEN,
	CH_BLUE,
	CH_COUNT
};

struct Curve
{
	int					count = 2;
	sboctl::CurvePoint	points[ sboctl::CURVE_MAX_POINTS ] = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
};

struct Levels
{
	int		inBlack = 0, inWhite = 255;		// 0 .. 255, inBlack < inWhite
	float	gamma = 1.0f;					// 0.1 .. 9.99 (above 1 brighter mid tones)
	int		outBlack = 0, outWhite = 255;	// 0 .. 255
};

enum SelectMode
{
	SELECT_RGB = 0,		// distance to the picked color in RGB
	SELECT_HUE,			// distance to the picked color's hue
	SELECT_COUNT
};

// Color balance ranges
enum Range
{
	RANGE_SHADOWS = 0,
	RANGE_MIDTONES,
	RANGE_HIGHLIGHTS,
	RANGE_COUNT
};

struct Operation
{
	int			type = OP_CURVES;
	std::string	name;					// shown in the list
	bool		enabled = true;
	int			blend = 255;			// 0 .. 255: how much of the operation's result replaces its input

	// OP_CURVES / OP_LEVELS: one per Channel (the RGB one first, then each channel's own)
	Curve		curves[ CH_COUNT ];
	Levels		levels[ CH_COUNT ];

	// OP_SELECTED_HSV
	int			selectMode = SELECT_RGB;
	int			target[3] = { 128, 128, 128 };	// the picked color
	int			tolerance = 51;			// 0 .. 255: fully selected up to this distance
	int			fuzziness = 0;			// 0 .. 255: then fading out over this distance
	int			hue = 0;				// -360 .. 360 degrees (colorize: the hue itself)
	int			saturation = 0;			// -255 .. 255
	int			value = 0;				// -255 .. 255
	bool		colorize = false;		// the selection gets one hue and saturation
	bool		invert = false;			// everything but the selection

	// OP_BALANCE: cyan-red, magenta-green, yellow-blue, -100 .. 100, per Range
	int			balance[ RANGE_COUNT ][3] = {};
	bool		preserveLuminosity = true;

	// OP_LOOKUP: a table file in gfx/colorcorrection/ (read by the editor)
	std::string	file;
	Lut			lut;					// size 0: none loaded (the operation changes nothing)
};

//! A new operation of a type with its default settings and name
Operation Make( int type );

//! The selection weight of a color for a Selected HSV operation (0 .. 1, invert applied); rgb 0 .. 1
float SelectionWeight( const Operation &op, const float rgb[3] );

//! One operation on one color (0 .. 1 each), its blend included; a disabled operation changes nothing
void ApplyOne( const Operation &op, float rgb[3] );
//! Every operation in order
void ApplyAll( const std::vector<Operation> &ops, float rgb[3] );

//! The table of the operations: size^3 colors (RAW_SIZE for a .raw), red fastest, then green, then blue
void BuildLut( const std::vector<Operation> &ops, int size, Lut &out );

//! A table looked up at one color (trilinear, as the GL pass does); rgb 0 .. 1 in and out
void SampleLut( const Lut &lut, const float in[3], float out[3] );

//! Lookup view (Source's): the table's blue slices as tiles of size x size, cols across, red to the right and
//! green down in each tile, blue rising tile by tile. RGBA, (size * cols) x (size * rows), rows = ceil(size / cols).
void LutImage( const Lut &lut, int cols, std::vector<unsigned char> &rgba, int &wide, int &tall );

//! A look name usable as a file name: letters, digits, '_' and '-', 1 .. 39 characters (the path
//! gfx/colorcorrection/<name>.raw stays under COLORCORR_MAX_PATH). Upper case is turned to lower case.
bool CleanName( const char *in, char *out, int outSize );

//! The operation file the editor saves next to the table (gfx/colorcorrection/<name>.ccops): VDF text with the
//! editor's Blend and every operation's type, name, on / off and values (a Lookup operation keeps its file name; the
//! table is read again on load). Only the editor reads it; color_correction loads the .raw alone.
constexpr const char *OPS_FILE_EXT = ".ccops";
constexpr int OPS_MAX = 64;			// operations in one file
void WriteOpsText( const std::vector<Operation> &ops, int blend, std::string &out );
//! False (and a reason with its line in err) on text that is not such a file; values are clamped to their ranges,
//! unknown keys are skipped. ops gets the operations (Lookup ones without their table), blend the editor's Blend.
bool ReadOpsText( const char *text, int len, std::vector<Operation> &ops, int &blend, char *err, int errLen );

//! HSV (h 0 .. 360, s and v 0 .. 1) <-> RGB (0 .. 1)
void RgbToHsv( const float rgb[3], float &h, float &s, float &v );
void HsvToRgb( float h, float s, float v, float rgb[3] );
} // namespace ops
} // namespace colorcorr

#endif // COLORCORRECTION_OPS_H
