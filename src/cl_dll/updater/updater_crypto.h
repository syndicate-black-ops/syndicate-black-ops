//========= SBO: updater, signature check and inflate =========//
//
// Purpose: the two checks a release goes through before anything is installed (docs/UPDATER.md):
//            * manifest.json's Ed25519 signature (manifest.json.sig, 64 bytes) against the public key built into the
//              client (CMake SBO_UPDATE_PUBKEY), with Monocypher 4.0.2 (external/monocypher, BSD-2 / CC0);
//            * a file cut out of the release zip: raw deflate (zip method 8) inflated with stb_image's zlib decoder
//              (src/cl_dll/vgui2/stb_image.h, public domain), the output size bounded by the manifest.
//          Plain C++, no SDK or SBO headers.
//=============================================================//

#ifndef SBO_UPDATER_CRYPTO_H
#define SBO_UPDATER_CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace sbo_update
{
//! 64 hex digits -> 32 bytes; false if text is not exactly that
bool ParsePublicKey(const char *hex, uint8_t key[32]);
//! sig: the 64 signature bytes; true if it signs msg with key
bool VerifySignature(const uint8_t key[32], const void *msg, size_t len, const void *sig, size_t sigLen);
//! Raw deflate data -> exactly outLen bytes; false on bad data or another length
bool InflateRaw(const void *in, size_t inLen, size_t outLen, std::string &out);
} // namespace sbo_update

#endif // SBO_UPDATER_CRYPTO_H
