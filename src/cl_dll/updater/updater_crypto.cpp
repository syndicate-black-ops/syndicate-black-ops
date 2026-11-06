//========= SBO: updater, signature check and inflate =========//
//
// Purpose: see updater_crypto.h.
//=============================================================//

#include "updater_crypto.h"

#include <string.h>

extern "C" {
#include "monocypher-ed25519.h"
}

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG       // the smallest decoder set that keeps the zlib decoder
#define STBI_SUPPORT_ZLIB
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#include "stb_image.h"

namespace sbo_update
{
bool ParsePublicKey(const char *hex, uint8_t key[32])
{
	if (!hex || strlen(hex) != 64)
		return false;
	for (int i = 0; i < 32; i++)
	{
		int v = 0;
		for (int j = 0; j < 2; j++)
		{
			const char c = hex[i * 2 + j];
			const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			if (d < 0)
				return false;
			v = v * 16 + d;
		}
		key[i] = (uint8_t)v;
	}
	return true;
}

bool VerifySignature(const uint8_t key[32], const void *msg, size_t len, const void *sig, size_t sigLen)
{
	if (sigLen != 64)
		return false;
	return crypto_ed25519_check((const uint8_t *)sig, key, (const uint8_t *)msg, len) == 0;
}

bool InflateRaw(const void *in, size_t inLen, size_t outLen, std::string &out)
{
	if (inLen > 0x7FFFFFFF || outLen > 0x7FFFFFFE)
		return false;
	// one spare byte: output longer than the manifest says shows as a length mismatch, not a silent cut
	out.assign(outLen + 1, '\0');
	const int got = stbi_zlib_decode_noheader_buffer(&out[0], (int)outLen + 1, (const char *)in, (int)inLen);
	if (got < 0 || (size_t)got != outLen)
	{
		out.clear();
		return false;
	}
	out.resize(outLen);
	return true;
}
} // namespace sbo_update
