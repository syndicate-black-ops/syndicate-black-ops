//========= SBO: updater, SHA-256 =========//
//
// Purpose: SHA-256 (FIPS 180-4) of the files the updater downloads, stages and installs (docs/UPDATER.md). Plain C++,
//          no SDK or SBO headers (tests/updater/run_test.sh builds it alone with ASan/UBSan).
//=========================================//

#ifndef SBO_UPDATER_SHA256_H
#define SBO_UPDATER_SHA256_H

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace sbo_update
{
class Sha256
{
public:
	Sha256();
	void Update(const void *data, size_t len);
	//! The digest as 64 lower-case hex digits (the object is spent afterwards)
	std::string FinalHex();

private:
	void Block(const uint8_t *p);
	uint32_t m_State[8];
	uint64_t m_Bits = 0;
	uint8_t m_Buf[64];
	size_t m_Used = 0;
};

//! 64 lower-case hex digits
std::string Sha256Hex(const void *data, size_t len);
} // namespace sbo_update

#endif // SBO_UPDATER_SHA256_H
