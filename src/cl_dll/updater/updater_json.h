//========= SBO: updater, JSON reader =========//
//
// Purpose: a small bounded JSON parser for the two documents the updater reads: GitHub's release list
//          (GET /repos/<repo>/releases) and the release's manifest.json (docs/UPDATER.md). RFC 8259 syntax; strings
//          decoded to UTF-8 (\uXXXX with surrogate pairs), nesting at most MAX_DEPTH, integers exact up to 2^53.
//          Plain C++, no SDK or SBO headers (tests/updater/run_test.sh fuzzes it with ASan/UBSan).
//=============================================//

#ifndef SBO_UPDATER_JSON_H
#define SBO_UPDATER_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace sbo_update
{
namespace json
{
constexpr int MAX_DEPTH = 32;

struct Value
{
	enum Type
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	} type = Null;
	bool b = false;
	double n = 0.0;
	int64_t i = 0;          //!< Number: the value when it is an integer (isInt)
	bool isInt = false;
	std::string s;
	std::vector<Value> a;
	std::vector<std::pair<std::string, Value>> o;

	//! Object member (the first of that name), else nullptr (also for non-objects)
	const Value *Get(const char *key) const;
	//! Member as a string / bool (Flag) / integer, def if missing or of another type
	std::string Str(const char *key, const char *def = "") const;
	bool Flag(const char *key, bool def = false) const;
	int64_t Int(const char *key, int64_t def = -1) const;
};

//! Parses text[0, len). False with err set on any syntax error, trailing data or a nesting deeper than MAX_DEPTH.
bool Parse(const char *text, size_t len, Value &out, std::string &err);

//! A JSON string literal for s (quotes included; control characters escaped)
std::string Quote(const std::string &s);
} // namespace json
} // namespace sbo_update

#endif // SBO_UPDATER_JSON_H
