//========= SBO VGUI2: image decoding for the server browser (map pictures, overviews) =========//
//
// Purpose: the server browser's right panel shows a picture of the server's map: gfx/mapimages/<map>.jpg (shipped,
//          or downloaded once and cached), else the spectator overview overviews/<map>.tga. The engine's VGUI2 surface
//          refuses TGA files above 256x256 ("LoadTGA: texture too large") and reads no JPEG, so the files are decoded
//          here and scaled down to fit a box, for ISurface::DrawSetTextureRGBA.
//            TGA: truecolor only (types 2 and 10, 24 or 32 bits), which is what SBO's overviews are.
//            JPEG: stb_image (nothings/stb 003501f, v2.30, public domain / MIT, stb_image.h), JPEG only, at most
//                  MAX_FILE_BYTES and MAX_SOURCE_SIDE pixels a side.
//          No SDK headers: tests/server_browser/run_test.sh runs both with ASan/UBSan on real and damaged files.
//=================================================================//

#ifndef SBO_VGUI2_SBO_IMAGE_DECODE_H
#define SBO_VGUI2_SBO_IMAGE_DECODE_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace sbo_image
{
constexpr int MAX_SOURCE_SIDE = 4096;
constexpr size_t MAX_FILE_BYTES = 8u * 1024 * 1024;

//! Decodes data (a whole .tga file) and scales it down (area average) to fit maxWide x maxTall, keeping its aspect;
//! never scales up. rgba gets wide * tall * 4 bytes, top row first. keyGreen: pure green (0 255 0), the empty
//! background of SBO's overview pictures, becomes transparent. False for a file that is not a valid truecolor TGA
//! (or is cut short).
bool DecodeTgaScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba, int &wide,
    int &tall, bool keyGreen = false);

//! Reads only a .tga file's 18-byte header: true with its size, bits per pixel (24 or 32) and image type (2
//! uncompressed or 10 run-length) for a truecolor TGA, false for anything else (a color-mapped or gray TGA, a header cut
//! short, a size of 0 or above MAX_SOURCE_SIDE)
bool ReadTgaHeader(const uint8_t *data, size_t size, int &wide, int &tall, int &bits, int &imageType);

//! The same for a JPEG file (baseline or progressive). False for anything that is not a JPEG within the limits.
bool DecodeJpegScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall);

//! A frame of a Half-Life sprite (.spr, "IDSP" version 2): the one with the most to see among the first frames (a
//! smoke animation often starts empty; a frame group: its first), with its palette and its texture format: normal and
//! additive opaque, index alpha (the last palette color, the index as alpha), alpha test (index 255 transparent).
//! Additive and index alpha sprites get their brightest pixel at full white, for the list's black cells (glows and
//! smoke read clearly). Scaled down like the others, or up (nearest pixel) to fill the box, aspect kept (the particle
//! editor's sprite list; no engine sprite slot is used).
bool DecodeSprScaled(const uint8_t *data, size_t size, int maxWide, int maxTall, std::vector<uint8_t> &rgba,
    int &wide, int &tall);
} // namespace sbo_image

#endif // SBO_VGUI2_SBO_IMAGE_DECODE_H
