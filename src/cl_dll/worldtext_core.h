//=========================================================
// SBO 2.0: point_worldtext, client side: the parts that need no engine (cl_dll/worldtext.cpp draws). Messages
// (game_shared/worldtext_shared.h), font metrics files (sprites/worldtext/<font>.txt, scripts/gen_worldtext_fonts.py),
// UTF-8 / Windows-1252 decoding and the text layout. tests/worldtext/run_test.sh builds this file alone and feeds it
// hostile input. docs/POINT_WORLDTEXT.md.
//=========================================================

#if !defined( WORLDTEXT_CORE_H )
#define WORLDTEXT_CORE_H

#include <string>
#include <vector>

#include "worldtext_shared.h"

#define WT_FONT_MAX_GLYPHS	1024	// glyph lines read from a metrics file; more are ignored
#define WT_FONT_MAX_FRAMES	32		// frames per mask (the sprite has twice as many)

struct WTGlyph
{
	int		codepoint;
	int		frame;
	int		x, y, w, h;		// the glyph's box in its frame (pixels)
	int		left, top;		// pen to the box's left edge; baseline to the box's top edge (pixels, up)
	float	advance;
};

struct WTFontMetrics
{
	std::string		name;
	int				em;				// pixels per em = line height
	int				ascent, descent;
	int				spacingUnit;	// textspacingX/Y are in 1/spacingUnit of the text size
	int				frameW, frameH;
	int				frames;			// per mask
	std::vector<WTGlyph>	glyphs;		// sorted by codepoint
	int				fallback;		// index of '?', -1 = none
	int				latin[256];		// codepoint < 256 -> glyph index, -1 = none

	const WTGlyph *Find( int codepoint ) const;	// the glyph, '?' if the font has none, NULL without '?'
};

// Parses a metrics file (len bytes, need not end in 0). false: not a usable font (the reason in err).
bool WT_ParseFontMetrics( const char *data, int len, WTFontMetrics &out, std::string &err );

// UTF-8 to code points; a byte that doesn't start a valid UTF-8 sequence is one Windows-1252 character (maps from
// editors that save in the Windows code page). At most maxOut code points.
void WT_DecodeText( const unsigned char *s, int len, std::vector<int> &out, int maxOut );

// One glyph quad of a laid-out text, in em pixels relative to the origin (x right, y up)
struct WTQuad
{
	float	x0, y0, x1, y1;		// left, top, right, bottom (y0 > y1)
	float	s0, t0, s1, t1;		// texture coordinates in the frame
	short	frame;
	short	index;				// the character's position in the text (rainbow)
};

struct WTLayout
{
	std::vector<WTQuad>	quads;		// sorted by frame
	int		frameStart[WT_FONT_MAX_FRAMES + 1];	// quads of frame f: frameStart[f] .. frameStart[f + 1] - 1
	float	minX, maxX, minY, maxY;	// the block's box (em pixels), for culling
	int		lines;
};

// flags: WORLDTEXT_FLAG_CENTER centers each line; billboard: the whole block centered on the origin (Source's
// orientations 1 and 2). spacingX / spacingY: the keys' values.
void WT_Layout( const WTFontMetrics &font, const std::vector<int> &text, float spacingX, float spacingY, int flags,
	bool billboard, WTLayout &out );

// A text's state as the server sends it
struct WTState
{
	int		id;
	int		flags;
	float	origin[3];
	float	angles[3];
	float	size, spacingX, spacingY;
	unsigned char color[4];
	int		orientation;
	int		font;
	// WORLDTEXT_FLAG_PARENT: drawn on this parent (iuser4 form) at localOrigin / localAngles (worldtext.cpp)
	int		parent;
	float	localOrigin[3];
	float	localAngles[3];
	std::string text;
};

// Parses "WorldText" (into state; text = the message's own string) / "WorldTextP" (id and piece). false: malformed.
bool WT_ParseMain( const void *buf, int size, WTState &state );
bool WT_ParsePart( const void *buf, int size, int &id, std::string &piece );

// The texts the server sent (joins the pieces of a long text). The client keeps one per entity index.
class WTStore
{
public:
	WTStore() : m_iPendingId( -1 ) {}

	// a text changed: on (true, state filled) or forgotten (false, only state.id); returns false when malformed
	bool OnMain( const void *buf, int size, WTState &state, bool &on );
	bool OnPart( const void *buf, int size );
	void Clear( void ) { m_iPendingId = -1; m_Pending.clear(); }

private:
	int			m_iPendingId;
	std::string	m_Pending;
};

#endif // WORLDTEXT_CORE_H
