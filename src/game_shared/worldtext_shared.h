// SBO 2.0: point_worldtext (Source's in-world text), the user messages shared by the server (dlls/point_worldtext.cpp)
// and the client (cl_dll/worldtext.cpp). docs/POINT_WORLDTEXT.md.
//
// "WorldText": one text's whole state
//   byte    flags (WORLDTEXT_FLAG_*)
//   short   id: the point_worldtext's entity index
//   -- without WORLDTEXT_FLAG_ON: nothing more (the text is off or removed: the client forgets it)
//   long x3 origin (float bits)
//   short x3 angles (pitch, yaw, roll; 65536 = 360 degrees)
//   long    textsize (float bits): the line height in world units
//   long    textspacingX, textspacingY (float bits): in 1/spacingunit of textsize (the font's metrics file)
//   byte x4 color r g b a
//   byte    orientation (WORLDTEXT_ORIENT_*)
//   byte    font (Source's font numbers, cl_dll/worldtext.cpp)
//   string  the text, or its end when "WorldTextP" messages came first. Escapes are already resolved ("\n" is a
//           byte 10); UTF-8, a byte that isn't UTF-8 is read as Windows-1252.
//
// "WorldTextP": the next piece of the start of a text too long for one "WorldText"
//   short   id
//   string  at most WORLDTEXT_PART_BYTES bytes
// A user message carries at most 192 bytes. The pieces of one text are written one after the other, then its
// "WorldText", to one client (MSG_ONE, reliable): the client joins the pieces of the id it is collecting and drops
// them when a message of another id comes in between.
//
// At most 39 bytes + the string's terminator before the text in "WorldText": WORLDTEXT_MAIN_TEXT_BYTES of text fit.
#pragma once

#define WORLDTEXT_FLAG_ON		1	// shown (else: off or removed)
#define WORLDTEXT_FLAG_RAINBOW	2	// Source's "rainbow"
#define WORLDTEXT_FLAG_CENTER	4	// SBO: each line centered on the origin

#define WORLDTEXT_ORIENT_FIXED		0	// the entity's angles
#define WORLDTEXT_ORIENT_FACE		1	// faces the view (billboard)
#define WORLDTEXT_ORIENT_UPRIGHT	2	// faces the view, stays upright (turns about the vertical only)

#define WORLDTEXT_MAX_TEXT			1024	// with the terminator: the engine's 1023 bytes for a keyvalue
#define WORLDTEXT_MSG_BYTES			188		// what a message may hold (MAX_USER_MSG_DATA 192, with room)
#define WORLDTEXT_HEADER_BYTES		39		// "WorldText" before its string
#define WORLDTEXT_MAIN_TEXT_BYTES	( WORLDTEXT_MSG_BYTES - WORLDTEXT_HEADER_BYTES - 1 )
#define WORLDTEXT_PART_BYTES		180		// text bytes in a "WorldTextP" (with its short and terminator: 183)

#define WORLDTEXT_FONT_COUNT		14		// Source's 0 .. 12 and SBO's 13; others are font 0
